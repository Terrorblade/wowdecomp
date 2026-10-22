#include "HoT/Streaming/ManifestSignature.h"
#include <cstdint>
#include <vector>
#include "HoT/Platform/Crypto.h"
#include "HoT/Streaming/Sha256.h"

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

    return Platform::VerifyEcdsaP256(s_publicKey, digest, signature.data());
}

}
