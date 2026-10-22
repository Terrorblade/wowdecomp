#ifndef HOT_PLATFORM_CRYPTO_H
#define HOT_PLATFORM_CRYPTO_H

#include <cstddef>

namespace HoT::Platform {

class Sha256 {
    public:
    Sha256() = default;
    ~Sha256();

    Sha256(const Sha256&) = delete;
    Sha256& operator=(const Sha256&) = delete;

    bool Open();
    void Update(const void* data, size_t size);
    bool Finish(unsigned char out[32]);

    private:
    void* m_state = nullptr;
};

// ECDSA P-256 over a SHA-256 digest, the key is X then Y and the signature r then s, 32 bytes each.
bool VerifyEcdsaP256(const unsigned char publicKey[64], const unsigned char digest[32], const unsigned char signature[64]);

}

#endif
