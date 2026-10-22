#include "HoT/Platform/Crypto.h"
#include <cstring>
#include <vector>
#include "HoT/Core/Log.h"
#include <windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

namespace HoT::Platform {

struct Sha256State {
    BCRYPT_ALG_HANDLE m_alg = nullptr;
    BCRYPT_HASH_HANDLE m_hash = nullptr;
    std::vector<unsigned char> m_obj;

    ~Sha256State() {
        if (this->m_hash) {
            BCryptDestroyHash(this->m_hash);
        }

        if (this->m_alg) {
            BCryptCloseAlgorithmProvider(this->m_alg, 0);
        }
    }
};

Sha256::~Sha256() {
    delete static_cast<Sha256State*>(this->m_state);
}

bool Sha256::Open() {
    Sha256State* state = new Sha256State();
    this->m_state = state;

    if (BCryptOpenAlgorithmProvider(&state->m_alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        state->m_alg = nullptr;
        return false;
    }

    DWORD objLen = 0;
    DWORD cb = 0;
    BCryptGetProperty(state->m_alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objLen), sizeof(objLen), &cb, 0);
    state->m_obj.resize(objLen);

    if (BCryptCreateHash(state->m_alg, &state->m_hash, state->m_obj.data(), objLen, nullptr, 0, 0) != 0) {
        state->m_hash = nullptr;
        return false;
    }

    return true;
}

void Sha256::Update(const void* data, size_t size) {
    BCryptHashData(static_cast<Sha256State*>(this->m_state)->m_hash, reinterpret_cast<PUCHAR>(const_cast<void*>(data)), static_cast<ULONG>(size), 0);
}

bool Sha256::Finish(unsigned char out[32]) {
    return BCryptFinishHash(static_cast<Sha256State*>(this->m_state)->m_hash, out, 32, 0) == 0;
}

bool VerifyEcdsaP256(const unsigned char publicKey[64], const unsigned char digest[32], const unsigned char signature[64]) {
    BCRYPT_ALG_HANDLE alg = nullptr;

    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) != 0) {
        LOG_WARN << "ECDSA P-256 provider unavailable, cannot verify manifest signature";
        return false;
    }

    // BCRYPT_ECCKEY_BLOB is the magic and key size, then X and Y back to back.
    std::vector<unsigned char> blob(sizeof(BCRYPT_ECCKEY_BLOB) + 64);
    BCRYPT_ECCKEY_BLOB* header = reinterpret_cast<BCRYPT_ECCKEY_BLOB*>(blob.data());
    header->dwMagic = BCRYPT_ECDSA_PUBLIC_P256_MAGIC;
    header->cbKey = 32;
    memcpy(blob.data() + sizeof(BCRYPT_ECCKEY_BLOB), publicKey, 64);

    BCRYPT_KEY_HANDLE key = nullptr;
    NTSTATUS st = BCryptImportKeyPair(alg, nullptr, BCRYPT_ECCPUBLIC_BLOB, &key, blob.data(), static_cast<ULONG>(blob.size()), 0);

    if (st != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }

    st = BCryptVerifySignature(key, nullptr, const_cast<PUCHAR>(digest), 32, const_cast<PUCHAR>(signature), 64, 0);

    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg, 0);
    return st == 0;
}

}
