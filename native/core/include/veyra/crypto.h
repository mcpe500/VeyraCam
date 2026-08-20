#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <string>
#include <vector>

namespace veyra {

constexpr size_t VEYRA_CRYPTO_KEY_SIZE = 32;   // 256-bit key
constexpr size_t VEYRA_CRYPTO_TAG_SIZE = 16;   // 128-bit AEAD tag
constexpr size_t VEYRA_CRYPTO_NONCE_SIZE = 12; // 96-bit ChaCha20-Poly1305 nonce
constexpr size_t VEYRA_PUBLIC_KEY_SIZE = 32;   // X25519 public key
constexpr size_t VEYRA_PRIVATE_KEY_SIZE = 32;  // X25519 secret key
constexpr size_t VEYRA_SALT_SIZE = 8;          // per-direction nonce salt

struct KeyPair {
    std::array<uint8_t, VEYRA_PUBLIC_KEY_SIZE> publicKey{};
    std::array<uint8_t, VEYRA_PRIVATE_KEY_SIZE> privateKey{};
};

// SessionCrypto wraps mbedTLS (X25519 key exchange + HKDF-SHA256 key
// derivation + ChaCha20-Poly1305 AEAD + CTR-DRBG CSPRNG).
// Never roll custom crypto: all primitives come from the audited mbedTLS.
class SessionCrypto {
public:
    SessionCrypto() = default;
    ~SessionCrypto() = default;

    SessionCrypto(const SessionCrypto&) = delete;
    SessionCrypto& operator=(const SessionCrypto&) = delete;

    // Generate an ephemeral X25519 keypair (CSPRNG-backed).
    static KeyPair GenerateKeyPair();

    // Derive mirrored rx/tx session keys from an X25519 key exchange.
    // serverRole selects the direction mapping (server.tx == client.rx).
    // Keys are bound to the transcript (both public keys) via HKDF info.
    bool DeriveSessionKeys(bool serverRole,
                           const uint8_t* peerPublicKey,
                           const uint8_t* myPublicKey,
                           const uint8_t* myPrivateKey);

    // AEAD encrypt: out must have room for plaintextSize + VEYRA_CRYPTO_TAG_SIZE.
    // Additional authenticated data (e.g. the packet header) is integrity
    // protected but not encrypted. Returns written length or 0 on failure.
    size_t Encrypt(const uint8_t* plaintext, size_t plaintextSize,
                   uint32_t sequence,
                   const uint8_t* aad, size_t aadSize,
                   uint8_t* outCiphertextAndTag) const;

    // AEAD decrypt + verify: inputLength includes the trailing tag.
    // Returns plaintext length or SIZE_MAX when authentication fails.
    size_t Decrypt(const uint8_t* ciphertextAndTag, size_t inputLength,
                   uint32_t sequence,
                   const uint8_t* aad, size_t aadSize,
                   uint8_t* outPlaintext) const;

    bool IsKeySet() const { return hasKeys_; }
    void Reset();

    // CSPRNG helpers (CTR-DRBG seeded from the platform entropy source).
    static void RandomBytes(uint8_t* out, size_t len);
    static uint32_t RandomId();

    // Encoding helpers (constant-time base64 from mbedTLS).
    static std::string Base64Encode(const uint8_t* data, size_t len);
    static std::vector<uint8_t> Base64Decode(const std::string& b64);

private:
    void BuildNonce(uint8_t outNonce[VEYRA_CRYPTO_NONCE_SIZE], uint32_t sequence, bool tx) const;

    std::array<uint8_t, VEYRA_CRYPTO_KEY_SIZE> rxKey_{};
    std::array<uint8_t, VEYRA_CRYPTO_KEY_SIZE> txKey_{};
    std::array<uint8_t, VEYRA_SALT_SIZE> rxSalt_{};
    std::array<uint8_t, VEYRA_SALT_SIZE> txSalt_{};
    bool hasKeys_{false};
};

} // namespace veyra
