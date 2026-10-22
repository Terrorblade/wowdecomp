#include "HoT/Platform/Crypto.h"
#include <cstring>
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include "HoT/Core/Log.h"

namespace HoT::Platform {

Sha256::~Sha256() {
    if (this->m_state) {
        EVP_MD_CTX_free(static_cast<EVP_MD_CTX*>(this->m_state));
    }
}

bool Sha256::Open() {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();

    if (!ctx) {
        return false;
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    this->m_state = ctx;
    return true;
}

void Sha256::Update(const void* data, size_t size) {
    EVP_DigestUpdate(static_cast<EVP_MD_CTX*>(this->m_state), data, size);
}

bool Sha256::Finish(unsigned char out[32]) {
    unsigned int len = 0;
    return EVP_DigestFinal_ex(static_cast<EVP_MD_CTX*>(this->m_state), out, &len) == 1 && len == 32;
}

// OpenSSL wants the key as an uncompressed SEC1 point (0x04, X, Y) and a DER signature, BCrypt uses raw r || s.
bool VerifyEcdsaP256(const unsigned char publicKey[64], const unsigned char digest[32], const unsigned char signature[64]) {
    unsigned char point[65];
    point[0] = 0x04;
    memcpy(point + 1, publicKey, 64);

    char group[] = "prime256v1";
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, group, 0),
        OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY, point, sizeof(point)),
        OSSL_PARAM_construct_end(),
    };

    EVP_PKEY_CTX* importCtx = EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr);
    EVP_PKEY* key = nullptr;

    if (!importCtx || EVP_PKEY_fromdata_init(importCtx) != 1 || EVP_PKEY_fromdata(importCtx, &key, EVP_PKEY_PUBLIC_KEY, params) != 1) {
        LOG_WARN << "ECDSA P-256 provider unavailable, cannot verify manifest signature";
        EVP_PKEY_CTX_free(importCtx);
        return false;
    }

    EVP_PKEY_CTX_free(importCtx);

    ECDSA_SIG* sig = ECDSA_SIG_new();
    BIGNUM* r = BN_bin2bn(signature, 32, nullptr);
    BIGNUM* s = BN_bin2bn(signature + 32, 32, nullptr);

    if (!sig || !r || !s || ECDSA_SIG_set0(sig, r, s) != 1) {
        BN_free(r);
        BN_free(s);
        ECDSA_SIG_free(sig);
        EVP_PKEY_free(key);
        return false;
    }

    unsigned char* der = nullptr;
    int derLen = i2d_ECDSA_SIG(sig, &der);
    ECDSA_SIG_free(sig);

    if (derLen <= 0) {
        EVP_PKEY_free(key);
        return false;
    }

    // No digest is set on the context, so it verifies the SHA-256 as is, like BCryptVerifySignature.
    EVP_PKEY_CTX* verifyCtx = EVP_PKEY_CTX_new(key, nullptr);
    bool ok = verifyCtx && EVP_PKEY_verify_init(verifyCtx) == 1 && EVP_PKEY_verify(verifyCtx, der, static_cast<size_t>(derLen), digest, 32) == 1;

    EVP_PKEY_CTX_free(verifyCtx);
    OPENSSL_free(der);
    EVP_PKEY_free(key);
    return ok;
}

}
