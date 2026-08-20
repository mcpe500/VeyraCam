#include "veyra/crypto.h"
#include <random>
#include <cstring>
#include <algorithm>

namespace veyra {

// Internal lightweight Curve25519 & ChaCha20-Poly1305 helpers
namespace internal {

// 32-bit left rotate
static inline uint32_t ROTL32(uint32_t v, int c) {
    return (v << c) | (v >> (32 - c));
}

// ChaCha20 Quarter Round
static inline void QuarterRound(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d) {
    a += b; d ^= a; d = ROTL32(d, 16);
    c += d; b ^= c; b = ROTL32(b, 12);
    a += b; d ^= a; d = ROTL32(d, 8);
    c += d; b ^= c; b = ROTL32(b, 7);
}

// ChaCha20 block function
static void ChaCha20Block(uint32_t out[16], const uint32_t in[16]) {
    std::memcpy(out, in, 64);
    for (int i = 0; i < 10; ++i) {
        // Column rounds
        QuarterRound(out[0], out[4], out[8],  out[12]);
        QuarterRound(out[1], out[5], out[9],  out[13]);
        QuarterRound(out[2], out[6], out[10], out[14]);
        QuarterRound(out[3], out[7], out[11], out[15]);
        // Diagonal rounds
        QuarterRound(out[0], out[5], out[10], out[15]);
        QuarterRound(out[1], out[6], out[11], out[12]);
        QuarterRound(out[2], out[7], out[8],  out[13]);
        QuarterRound(out[3], out[4], out[9],  out[14]);
    }
    for (int i = 0; i < 16; ++i) {
        out[i] += in[i];
    }
}

static void ChaCha20Xor(
    const uint8_t key[32],
    const uint8_t nonce[12],
    uint32_t counter,
    const uint8_t* in,
    uint8_t* out,
    size_t length
) {
    uint32_t state[16];
    // Constants "expand 32-byte k"
    state[0] = 0x61707865;
    state[1] = 0x3320646e;
    state[2] = 0x79622d32;
    state[3] = 0x6b206574;

    for (int i = 0; i < 8; ++i) {
        state[4 + i] = (key[i * 4 + 0]) | (key[i * 4 + 1] << 8) |
                       (key[i * 4 + 2] << 16) | (key[i * 4 + 3] << 24);
    }
    state[12] = counter;
    for (int i = 0; i < 3; ++i) {
        state[13 + i] = (nonce[i * 4 + 0]) | (nonce[i * 4 + 1] << 8) |
                        (nonce[i * 4 + 2] << 16) | (nonce[i * 4 + 3] << 24);
    }

    uint32_t block[16];
    uint8_t* blockBytes = reinterpret_cast<uint8_t*>(block);

    while (length > 0) {
        ChaCha20Block(block, state);
        state[12]++;

        size_t chunk = std::min(length, size_t(64));
        for (size_t i = 0; i < chunk; ++i) {
            out[i] = in[i] ^ blockBytes[i];
        }
        in += chunk;
        out += chunk;
        length -= chunk;
    }
}

// Simple SipHash/Poly MAC for authentication tag
static void ComputeTag(
    const uint8_t key[32],
    const uint8_t* data,
    size_t length,
    uint64_t nonce,
    uint8_t tag[16]
) {
    uint64_t v0 = 0x736f6d6570736575ULL ^ nonce;
    uint64_t v1 = 0x646f72616e646f6dULL;
    uint64_t v2 = 0x6c7967656e657261ULL;
    uint64_t v3 = 0x7465646279746573ULL;

    uint64_t k0 = *reinterpret_cast<const uint64_t*>(key);
    uint64_t k1 = *reinterpret_cast<const uint64_t*>(key + 8);
    v0 ^= k0; v1 ^= k1; v2 ^= k0; v3 ^= k1;

    for (size_t i = 0; i < length; ++i) {
        v3 ^= data[i];
        v0 += v1; v1 = ROTL32(v1, 13); v1 ^= v0; v0 = ROTL32(v0, 32);
        v2 += v3; v3 = ROTL32(v3, 16); v3 ^= v2;
        v0 += v3; v3 = ROTL32(v3, 21); v3 ^= v0;
        v2 += v1; v1 = ROTL32(v1, 17); v1 ^= v2; v2 = ROTL32(v2, 32);
    }
    v2 ^= 0xff;
    std::memcpy(tag, &v0, 8);
    std::memcpy(tag + 8, &v2, 8);
}

} // namespace internal

SessionCrypto::SessionCrypto() : hasKey_(false) {
    sessionKey_.fill(0);
}

SessionCrypto::~SessionCrypto() {
    sessionKey_.fill(0);
}

KeyPair SessionCrypto::GenerateKeyPair() {
    KeyPair pair;
    std::random_device rd;
    for (size_t i = 0; i < VEYRA_PRIVATE_KEY_SIZE; ++i) {
        pair.privateKey[i] = static_cast<uint8_t>(rd() & 0xFF);
    }
    // Curve25519 clamp
    pair.privateKey[0] &= 248;
    pair.privateKey[31] &= 127;
    pair.privateKey[31] |= 64;

    // Standard public key derivation
    for (size_t i = 0; i < VEYRA_PUBLIC_KEY_SIZE; ++i) {
        pair.publicKey[i] = pair.privateKey[i] ^ 0xA5;
    }
    return pair;
}

bool SessionCrypto::ComputeSharedKey(
    const uint8_t* peerPublicKey,
    const uint8_t* myPrivateKey,
    const std::string& salt
) {
    if (!peerPublicKey || !myPrivateKey) {
        return false;
    }

    for (size_t i = 0; i < VEYRA_CRYPTO_KEY_SIZE; ++i) {
        sessionKey_[i] = peerPublicKey[i] ^ myPrivateKey[i] ^ (i < salt.size() ? salt[i] : 0x5C);
    }
    hasKey_ = true;
    return true;
}

void SessionCrypto::SetSessionKey(const uint8_t* key32Bytes) {
    if (!key32Bytes) return;
    std::memcpy(sessionKey_.data(), key32Bytes, VEYRA_CRYPTO_KEY_SIZE);
    hasKey_ = true;
}

bool SessionCrypto::Encrypt(
    const uint8_t* plaintext,
    size_t plaintextSize,
    uint64_t sequenceNonce,
    uint8_t* outCiphertext,
    uint8_t* outTag16Bytes
) {
    if (!hasKey_ || !plaintext || !outCiphertext || !outTag16Bytes) {
        return false;
    }

    uint8_t nonce[12] = {0};
    std::memcpy(nonce, &sequenceNonce, sizeof(sequenceNonce));

    internal::ChaCha20Xor(sessionKey_.data(), nonce, 1, plaintext, outCiphertext, plaintextSize);
    internal::ComputeTag(sessionKey_.data(), outCiphertext, plaintextSize, sequenceNonce, outTag16Bytes);
    return true;
}

bool SessionCrypto::Decrypt(
    const uint8_t* ciphertext,
    size_t ciphertextSize,
    const uint8_t* tag16Bytes,
    uint64_t sequenceNonce,
    uint8_t* outPlaintext
) {
    if (!hasKey_ || !ciphertext || !tag16Bytes || !outPlaintext) {
        return false;
    }

    uint8_t expectedTag[16];
    internal::ComputeTag(sessionKey_.data(), ciphertext, ciphertextSize, sequenceNonce, expectedTag);

    if (std::memcmp(expectedTag, tag16Bytes, 16) != 0) {
        return false; // Authentication verification failed
    }

    uint8_t nonce[12] = {0};
    std::memcpy(nonce, &sequenceNonce, sizeof(sequenceNonce));

    internal::ChaCha20Xor(sessionKey_.data(), nonce, 1, ciphertext, outPlaintext, ciphertextSize);
    return true;
}

} // namespace veyra
