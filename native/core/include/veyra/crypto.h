#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <array>

namespace veyra {

constexpr size_t VEYRA_CRYPTO_KEY_SIZE = 32;   // 256-bit key
constexpr size_t VEYRA_CRYPTO_NONCE_SIZE = 12; // 96-bit nonce
constexpr size_t VEYRA_CRYPTO_TAG_SIZE = 16;   // 128-bit MAC tag
constexpr size_t VEYRA_PUBLIC_KEY_SIZE = 32;   // X25519 Public Key
constexpr size_t VEYRA_PRIVATE_KEY_SIZE = 32;  // X25519 Private Key

struct KeyPair {
    std::array<uint8_t, VEYRA_PUBLIC_KEY_SIZE> publicKey;
    std::array<uint8_t, VEYRA_PRIVATE_KEY_SIZE> privateKey;
};

class SessionCrypto {
public:
    SessionCrypto();
    ~SessionCrypto();

    // Generate ephemeral X25519 KeyPair for session pairing
    static KeyPair GenerateKeyPair();

    // Compute shared session key from our private key and peer's public key
    bool ComputeSharedKey(
        const uint8_t* peerPublicKey,
        const uint8_t* myPrivateKey,
        const std::string& salt = "VeyraLink_Key_Derivation_v1"
    );

    // Explicitly set symmetric 256-bit key (e.g. from established pairing token)
    void SetSessionKey(const uint8_t* key32Bytes);

    // Encrypt payload in-place or into outBuffer
    bool Encrypt(
        const uint8_t* plaintext,
        size_t plaintextSize,
        uint64_t sequenceNonce,
        uint8_t* outCiphertext,
        uint8_t* outTag16Bytes
    );

    // Decrypt and verify payload
    bool Decrypt(
        const uint8_t* ciphertext,
        size_t ciphertextSize,
        const uint8_t* tag16Bytes,
        uint64_t sequenceNonce,
        uint8_t* outPlaintext
    );

    bool IsKeySet() const { return hasKey_; }

private:
    std::array<uint8_t, VEYRA_CRYPTO_KEY_SIZE> sessionKey_;
    bool hasKey_{false};
};

} // namespace veyra
