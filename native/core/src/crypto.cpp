#include "veyra/crypto.h"

#include <mbedtls/ecdh.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/chachapoly.h>
#include <mbedtls/md.h>
#include <mbedtls/hkdf.h>
#include <mbedtls/base64.h>

#include <cstring>
#include <mutex>
#include <vector>

// mbedTLS 3.6.2's constant_time.h lacks C++ extern "C" guards, so declare
// the symbol directly instead of including the header.
extern "C" int mbedtls_ct_memcmp(const void* a, const void* b, size_t n);

namespace veyra {

namespace {

// Global CSPRNG (CTR-DRBG over the platform entropy source).
std::mutex g_rngMutex;
mbedtls_entropy_context g_entropy;
mbedtls_ctr_drbg_context g_drbg;
bool g_rngReady = false;

void EnsureRng() {
    std::lock_guard<std::mutex> lock(g_rngMutex);
    if (!g_rngReady) {
        mbedtls_entropy_init(&g_entropy);
        mbedtls_ctr_drbg_init(&g_drbg);
        if (mbedtls_ctr_drbg_seed(&g_drbg, mbedtls_entropy_func, &g_entropy,
                                  reinterpret_cast<const unsigned char*>("VeyraSessionCryptoV1"),
                                  strlen("VeyraSessionCryptoV1")) != 0) {
            abort();
        }
        g_rngReady = true;
    }
}

int RngRead(void* /*ctx*/, unsigned char* out, size_t len) {
    std::lock_guard<std::mutex> lock(g_rngMutex);
    if (!g_rngReady) return MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;
    return mbedtls_ctr_drbg_random(&g_drbg, out, len);
}

constexpr unsigned char kHkdfSalt[] = "VeyraV1-session-kx";
constexpr size_t kHkdfSaltLen = sizeof(kHkdfSalt) - 1;

} // namespace

KeyPair SessionCrypto::GenerateKeyPair() {
    EnsureRng();

    KeyPair pair;
    mbedtls_ecp_group grp;
    mbedtls_mpi d;
    mbedtls_ecp_point Q;

    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_ecp_point_init(&Q);

    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    (void)md;

    do {
        if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_CURVE25519) != 0) break;
        if (mbedtls_ecdh_gen_public(&grp, &d, &Q, RngRead, nullptr) != 0) break;

        size_t olen = 0;
        // X25519 public keys are the 32-byte little-endian point encoding.
        if (mbedtls_ecp_point_write_binary(&grp, &Q, MBEDTLS_ECP_PF_COMPRESSED,
                                           &olen, pair.publicKey.data(),
                                           pair.publicKey.size()) != 0 || olen != 32) {
            break;
        }
        if (mbedtls_mpi_write_binary_le(&d, pair.privateKey.data(),
                                        pair.privateKey.size()) != 0) {
            break;
        }
    } while (false);

    mbedtls_ecp_group_free(&grp);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_point_free(&Q);
    return pair;
}

void SessionCrypto::RandomBytes(uint8_t* out, size_t len) {
    EnsureRng();
    std::lock_guard<std::mutex> lock(g_rngMutex);
    mbedtls_ctr_drbg_random(&g_drbg, out, len);
}

uint32_t SessionCrypto::RandomId() {
    uint8_t b[4];
    RandomBytes(b, sizeof(b));
    return (static_cast<uint32_t>(b[0]) << 24) | (static_cast<uint32_t>(b[1]) << 16) |
           (static_cast<uint32_t>(b[2]) << 8) | static_cast<uint32_t>(b[3]);
}

std::string SessionCrypto::Base64Encode(const uint8_t* data, size_t len) {
    if (len == 0) return {};
    size_t outLen = 0;
    if (mbedtls_base64_encode(nullptr, 0, &outLen, data, len) != 0 &&
        outLen == 0) {
        return {};
    }
    std::string out(outLen, '\0');
    size_t written = 0;
    if (mbedtls_base64_encode(reinterpret_cast<unsigned char*>(out.data()), out.size(),
                              &written, data, len) != 0) {
        return {};
    }
    out.resize(written);
    return out;
}

std::vector<uint8_t> SessionCrypto::Base64Decode(const std::string& b64) {
    if (b64.empty()) return {};
    size_t outLen = 0;
    if (mbedtls_base64_decode(nullptr, 0, &outLen,
                              reinterpret_cast<const unsigned char*>(b64.c_str()),
                              b64.size()) != 0 && outLen == 0) {
        return {};
    }
    std::vector<uint8_t> out(outLen);
    size_t written = 0;
    if (mbedtls_base64_decode(out.data(), out.size(), &written,
                              reinterpret_cast<const unsigned char*>(b64.c_str()),
                              b64.size()) != 0) {
        return {};
    }
    out.resize(written);
    return out;
}

bool SessionCrypto::DeriveSessionKeys(bool serverRole,
                                      const uint8_t* peerPublicKey,
                                      const uint8_t* myPublicKey,
                                      const uint8_t* myPrivateKey) {
    EnsureRng();
    if (!peerPublicKey || !myPublicKey || !myPrivateKey) {
        return false;
    }

    Reset();

    mbedtls_ecp_group grp;
    mbedtls_mpi d, z;
    mbedtls_ecp_point Qpeer;
    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_mpi_init(&z);
    mbedtls_ecp_point_init(&Qpeer);

    bool ok = false;
    do {
        if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_CURVE25519) != 0) break;
        if (mbedtls_mpi_read_binary_le(&d, myPrivateKey, VEYRA_PRIVATE_KEY_SIZE) != 0) break;
        if (mbedtls_ecp_point_read_binary(&grp, &Qpeer, peerPublicKey,
                                          VEYRA_PUBLIC_KEY_SIZE) != 0) break;
        if (mbedtls_ecdh_compute_shared(&grp, &z, &Qpeer, &d, RngRead, nullptr) != 0) break;

        // Shared secret z (fixed-size, both peers derive identical bytes).
        uint8_t shared[32] = {0};
        if (mbedtls_mpi_write_binary(&z, shared, sizeof(shared)) != 0) break;

        // HKDF-SHA256: PRK from the shared secret, keys bound to the
        // transcript (both public keys, order-canonicalized) to prevent UKS.
        uint8_t prk[32];
        const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
        if (mbedtls_hkdf_extract(md, kHkdfSalt, kHkdfSaltLen, shared, sizeof(shared),
                                 prk) != 0) break;

        const uint8_t* lo = std::memcmp(myPublicKey, peerPublicKey, 32) <= 0 ? myPublicKey : peerPublicKey;
        const uint8_t* hi = std::memcmp(myPublicKey, peerPublicKey, 32) <= 0 ? peerPublicKey : myPublicKey;

        // info = label || lo || hi ; identical bytes on both peers.
        auto buildInfo = [lo, hi](const char* lbl) {
            std::vector<unsigned char> info(lbl, lbl + std::strlen(lbl));
            info.insert(info.end(), lo, lo + 32);
            info.insert(info.end(), hi, hi + 32);
            return info;
        };

        uint8_t kS2C[32], kC2S[32], nS2C[8], nC2S[8];
        auto infoS2C = buildInfo("VeyraV1|s2c|");
        auto infoC2S = buildInfo("VeyraV1|c2s|");
        auto infoNs2c = buildInfo("VeyraV1|ns2c|");
        auto infoNc2s = buildInfo("VeyraV1|nc2s|");

        bool derived = true;
        derived = derived && mbedtls_hkdf_expand(md, prk, sizeof(prk),
                                                 infoS2C.data(), infoS2C.size(),
                                                 kS2C, sizeof(kS2C)) == 0;
        derived = derived && mbedtls_hkdf_expand(md, prk, sizeof(prk),
                                                 infoC2S.data(), infoC2S.size(),
                                                 kC2S, sizeof(kC2S)) == 0;
        derived = derived && mbedtls_hkdf_expand(md, prk, sizeof(prk),
                                                 infoNs2c.data(), infoNs2c.size(),
                                                 nS2C, sizeof(nS2C)) == 0;
        derived = derived && mbedtls_hkdf_expand(md, prk, sizeof(prk),
                                                 infoNc2s.data(), infoNc2s.size(),
                                                 nC2S, sizeof(nC2S)) == 0;
        if (!derived) break;

        if (serverRole) {
            std::memcpy(txKey_.data(), kS2C, 32);
            std::memcpy(rxKey_.data(), kC2S, 32);
            std::memcpy(txSalt_.data(), nS2C, 8);
            std::memcpy(rxSalt_.data(), nC2S, 8);
        } else {
            std::memcpy(rxKey_.data(), kS2C, 32);
            std::memcpy(txKey_.data(), kC2S, 32);
            std::memcpy(rxSalt_.data(), nS2C, 8);
            std::memcpy(txSalt_.data(), nC2S, 8);
        }
        ok = true;
    } while (false);

    mbedtls_ecp_group_free(&grp);
    mbedtls_mpi_free(&d);
    mbedtls_mpi_free(&z);
    mbedtls_ecp_point_free(&Qpeer);

    if (ok) {
        hasKeys_ = true;
    } else {
        Reset();
    }
    return ok;
}

void SessionCrypto::BuildNonce(uint8_t outNonce[VEYRA_CRYPTO_NONCE_SIZE],
                               uint32_t sequence, bool tx) const {
    std::memset(outNonce, 0, VEYRA_CRYPTO_NONCE_SIZE);
    const auto& salt = tx ? txSalt_ : rxSalt_;
    std::memcpy(outNonce, salt.data(), VEYRA_SALT_SIZE);
    // 4-byte big-endian sequence fills the remaining nonce space.
    outNonce[VEYRA_SALT_SIZE + 0] = static_cast<uint8_t>((sequence >> 24) & 0xFF);
    outNonce[VEYRA_SALT_SIZE + 1] = static_cast<uint8_t>((sequence >> 16) & 0xFF);
    outNonce[VEYRA_SALT_SIZE + 2] = static_cast<uint8_t>((sequence >> 8) & 0xFF);
    outNonce[VEYRA_SALT_SIZE + 3] = static_cast<uint8_t>(sequence & 0xFF);
}

size_t SessionCrypto::Encrypt(const uint8_t* plaintext, size_t plaintextSize,
                              uint32_t sequence,
                              const uint8_t* aad, size_t aadSize,
                              uint8_t* outCiphertextAndTag) const {
    if (!hasKeys_ || (!plaintext && plaintextSize > 0) || !outCiphertextAndTag) {
        return 0;
    }

    uint8_t nonce[VEYRA_CRYPTO_NONCE_SIZE];
    BuildNonce(nonce, sequence, /*tx=*/true);

    mbedtls_chachapoly_context ctx;
    mbedtls_chachapoly_init(&ctx);

    size_t written = 0;
    do {
        if (mbedtls_chachapoly_setkey(&ctx, txKey_.data()) != 0) break;
        if (mbedtls_chachapoly_starts(&ctx, nonce, MBEDTLS_CHACHAPOLY_ENCRYPT) != 0) break;
        if (aad && aadSize > 0 &&
            mbedtls_chachapoly_update_aad(&ctx, aad, aadSize) != 0) break;
        if (plaintextSize > 0 &&
            mbedtls_chachapoly_update(&ctx, plaintextSize, plaintext,
                                      outCiphertextAndTag) != 0) break;
        if (mbedtls_chachapoly_finish(&ctx,
                                      outCiphertextAndTag + plaintextSize) != 0) break;
        written = plaintextSize + VEYRA_CRYPTO_TAG_SIZE;
    } while (false);

    mbedtls_chachapoly_free(&ctx);
    return written;
}

size_t SessionCrypto::Decrypt(const uint8_t* ciphertextAndTag, size_t inputLength,
                              uint32_t sequence,
                              const uint8_t* aad, size_t aadSize,
                              uint8_t* outPlaintext) const {
    if (!hasKeys_ || !ciphertextAndTag || !outPlaintext ||
        inputLength < VEYRA_CRYPTO_TAG_SIZE) {
        return static_cast<size_t>(-1);
    }

    const size_t cipherLen = inputLength - VEYRA_CRYPTO_TAG_SIZE;
    const uint8_t* tag = ciphertextAndTag + cipherLen;

    uint8_t nonce[VEYRA_CRYPTO_NONCE_SIZE];
    BuildNonce(nonce, sequence, /*tx=*/false);

    mbedtls_chachapoly_context ctx;
    mbedtls_chachapoly_init(&ctx);

    size_t plainLen = static_cast<size_t>(-1);
    do {
        if (mbedtls_chachapoly_setkey(&ctx, rxKey_.data()) != 0) break;
        if (mbedtls_chachapoly_starts(&ctx, nonce, MBEDTLS_CHACHAPOLY_DECRYPT) != 0) break;
        if (aad && aadSize > 0 &&
            mbedtls_chachapoly_update_aad(&ctx, aad, aadSize) != 0) break;
        if (cipherLen > 0 &&
            mbedtls_chachapoly_update(&ctx, cipherLen, ciphertextAndTag,
                                      outPlaintext) != 0) break;
        uint8_t computedTag[VEYRA_CRYPTO_TAG_SIZE];
        if (mbedtls_chachapoly_finish(&ctx, computedTag) != 0) break;
        if (mbedtls_ct_memcmp(computedTag, tag, VEYRA_CRYPTO_TAG_SIZE) != 0) {
            break; // forgery -> drop packet
        }
        plainLen = cipherLen;
    } while (false);

    mbedtls_chachapoly_free(&ctx);
    return plainLen;
}

void SessionCrypto::Reset() {
    rxKey_.fill(0);
    txKey_.fill(0);
    rxSalt_.fill(0);
    txSalt_.fill(0);
    hasKeys_ = false;
}

} // namespace veyra
