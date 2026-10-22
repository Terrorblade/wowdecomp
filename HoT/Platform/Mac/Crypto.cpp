#include "HoT/Platform/Crypto.h"
#include <cstring>
#include <vector>
#include <CommonCrypto/CommonDigest.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include "HoT/Core/Log.h"

namespace HoT::Platform {

Sha256::~Sha256() {
    delete static_cast<CC_SHA256_CTX*>(this->m_state);
}

bool Sha256::Open() {
    CC_SHA256_CTX* ctx = new CC_SHA256_CTX();
    this->m_state = ctx;
    return CC_SHA256_Init(ctx) == 1;
}

void Sha256::Update(const void* data, size_t size) {
    const unsigned char* bytes = static_cast<const unsigned char*>(data);

    while (size > 0) {
        CC_LONG chunk = size > 0x40000000 ? 0x40000000 : static_cast<CC_LONG>(size);
        CC_SHA256_Update(static_cast<CC_SHA256_CTX*>(this->m_state), bytes, chunk);
        bytes += chunk;
        size -= chunk;
    }
}

bool Sha256::Finish(unsigned char out[32]) {
    return CC_SHA256_Final(out, static_cast<CC_SHA256_CTX*>(this->m_state)) == 1;
}

// One DER INTEGER, a leading zero keeps a value with the top bit set positive.
static void AppendDerInteger(std::vector<unsigned char>& out, const unsigned char* value, size_t size) {
    while (size > 1 && value[0] == 0 && !(value[1] & 0x80)) {
        value++;
        size--;
    }

    bool pad = (value[0] & 0x80) != 0;
    out.push_back(0x02);
    out.push_back(static_cast<unsigned char>(size + (pad ? 1 : 0)));

    if (pad) {
        out.push_back(0x00);
    }

    out.insert(out.end(), value, value + size);
}

// Security.framework takes the key as an X9.63 point (0x04, X, Y) and a DER signature, BCrypt uses raw r || s.
bool VerifyEcdsaP256(const unsigned char publicKey[64], const unsigned char digest[32], const unsigned char signature[64]) {
    unsigned char point[65];
    point[0] = 0x04;
    memcpy(point + 1, publicKey, 64);

    std::vector<unsigned char> body;
    AppendDerInteger(body, signature, 32);
    AppendDerInteger(body, signature + 32, 32);

    std::vector<unsigned char> der;
    der.push_back(0x30);
    der.push_back(static_cast<unsigned char>(body.size()));
    der.insert(der.end(), body.begin(), body.end());

    int32_t keyBits = 256;
    CFNumberRef keySize = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &keyBits);
    const void* keys[] = { kSecAttrKeyType, kSecAttrKeyClass, kSecAttrKeySizeInBits };
    const void* values[] = { kSecAttrKeyTypeECSECPrimeRandom, kSecAttrKeyClassPublic, keySize };
    CFDictionaryRef attributes = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDataRef keyData = CFDataCreate(kCFAllocatorDefault, point, sizeof(point));

    SecKeyRef key = SecKeyCreateWithData(keyData, attributes, nullptr);

    CFRelease(keyData);
    CFRelease(attributes);
    CFRelease(keySize);

    if (!key) {
        LOG_WARN << "ECDSA P-256 provider unavailable, cannot verify manifest signature";
        return false;
    }

    CFDataRef digestData = CFDataCreate(kCFAllocatorDefault, digest, 32);
    CFDataRef signatureData = CFDataCreate(kCFAllocatorDefault, der.data(), static_cast<CFIndex>(der.size()));

    // The Digest variant verifies the SHA-256 as is, like BCryptVerifySignature.
    bool ok = SecKeyVerifySignature(key, kSecKeyAlgorithmECDSASignatureDigestX962SHA256, digestData, signatureData, nullptr);

    CFRelease(signatureData);
    CFRelease(digestData);
    CFRelease(key);
    return ok;
}

}
