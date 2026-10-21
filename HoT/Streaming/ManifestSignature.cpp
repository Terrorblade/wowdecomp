#include "HoT/Streaming/ManifestSignature.h"
#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#endif
#include <cstdint>
#include <cstring>
#include <vector>
#include "HoT/Core/Log.h"
#include "HoT/Streaming/Sha256.h"

#if defined(_WIN32)
#pragma comment(lib, "bcrypt.lib")
#endif

namespace HoT::Streaming {

const char* const s_signatureSuffix = ".sig";

static const size_t s_keyBytes = 32;
static const size_t s_signatureBytes = 64;

// Public half of the manifest signing key, X then Y.
static const unsigned char s_publicKey[s_keyBytes * 2] = {
    0xd1, 0x42, 0x82, 0x2d, 0x08, 0x60, 0xb7, 0xb6, 0xc8, 0xab, 0xe3, 0xea, 0xe8, 0x32, 0x0a, 0x93,
    0x90, 0x05, 0xac, 0x11, 0xd6, 0x22, 0x68, 0x05, 0xf8, 0x4e, 0x59, 0x6d, 0xb0, 0x60, 0xde, 0xdc,
    0xe7, 0x5c, 0x04, 0x83, 0x82, 0x0a, 0x00, 0xad, 0x67, 0x24, 0xe9, 0x43, 0xa3, 0x3a, 0xd3, 0x71,
    0x25, 0x6c, 0xdd, 0xe0, 0x29, 0x66, 0xc9, 0xaf, 0xde, 0x6a, 0x9d, 0xbd, 0x61, 0x6c, 0x7f, 0x64,
};

static int32_t Base64Value(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }

    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }

    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }

    if (c == '+') {
        return 62;
    }

    if (c == '/') {
        return 63;
    }

    return -1;
}

static bool Base64Decode(const std::string& text, std::vector<unsigned char>& out) {
    out.clear();
    uint32_t acc = 0;
    int32_t bits = 0;

    for (char c : text) {
        if (c == '=') {
            break;
        }

        if (c == '\r' || c == '\n' || c == ' ' || c == '\t') {
            continue;
        }

        int32_t v = Base64Value(c);

        if (v < 0) {
            return false;
        }

        acc = (acc << 6) | static_cast<uint32_t>(v);
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<unsigned char>((acc >> bits) & 0xFF));
        }
    }

    return true;
}

bool VerifyManifestSignature(const std::string& manifestBytes, const std::string& signatureText) {
    std::vector<unsigned char> signature;

    if (!Base64Decode(signatureText, signature) || signature.size() != s_signatureBytes) {
        return false;
    }

    unsigned char digest[32];

    if (!Sha256Raw(manifestBytes.data(), manifestBytes.size(), digest)) {
        return false;
    }

#if defined(_WIN32)
    BCRYPT_ALG_HANDLE alg = nullptr;

    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) != 0) {
        LOG_WARN << "ECDSA P-256 provider unavailable, cannot verify manifest signature";
        return false;
    }

    // BCRYPT_ECCKEY_BLOB is the magic and key size, then X and Y back to back.
    std::vector<unsigned char> blob(sizeof(BCRYPT_ECCKEY_BLOB) + sizeof(s_publicKey));
    BCRYPT_ECCKEY_BLOB* header = reinterpret_cast<BCRYPT_ECCKEY_BLOB*>(blob.data());
    header->dwMagic = BCRYPT_ECDSA_PUBLIC_P256_MAGIC;
    header->cbKey = s_keyBytes;
    memcpy(blob.data() + sizeof(BCRYPT_ECCKEY_BLOB), s_publicKey, sizeof(s_publicKey));

    BCRYPT_KEY_HANDLE key = nullptr;
    NTSTATUS st = BCryptImportKeyPair(alg, nullptr, BCRYPT_ECCPUBLIC_BLOB, &key, blob.data(), static_cast<ULONG>(blob.size()), 0);

    if (st != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }

    st = BCryptVerifySignature(key, nullptr, digest, sizeof(digest), signature.data(), static_cast<ULONG>(signature.size()), 0);

    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg, 0);
    return st == 0;
#else
    // OpenSSL wants the key as an uncompressed SEC1 point (0x04, X, Y) and a DER signature, BCrypt uses raw r || s.
    unsigned char point[1 + sizeof(s_publicKey)];
    point[0] = 0x04;
    memcpy(point + 1, s_publicKey, sizeof(s_publicKey));

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
    BIGNUM* r = BN_bin2bn(signature.data(), static_cast<int>(s_keyBytes), nullptr);
    BIGNUM* s = BN_bin2bn(signature.data() + s_keyBytes, static_cast<int>(s_keyBytes), nullptr);

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

    // No digest is set on the context, so it verifies the SHA-256 computed above as is, like BCryptVerifySignature.
    EVP_PKEY_CTX* verifyCtx = EVP_PKEY_CTX_new(key, nullptr);
    bool ok = verifyCtx && EVP_PKEY_verify_init(verifyCtx) == 1 && EVP_PKEY_verify(verifyCtx, der, static_cast<size_t>(derLen), digest, sizeof(digest)) == 1;

    EVP_PKEY_CTX_free(verifyCtx);
    OPENSSL_free(der);
    EVP_PKEY_free(key);
    return ok;
#endif
}

}
