#include "HoT/Streaming/Sha256.h"
#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/evp.h>
#include <filesystem>
#endif
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <vector>

#if defined(_WIN32)
#pragma comment(lib, "bcrypt.lib")
#endif

namespace HoT::Streaming {

static const char s_quickPrefix[] = "q1:";
static const size_t s_quickPrefixLen = 3;

// Sampled digest v1 is sha256 over "hotquick1", the size as 8 bytes little endian, then the windows in offset order.
static const long long s_windowBytes = 1 << 20;
static const int32_t s_windows = 8;

static std::string ToHex(const unsigned char* d, size_t n) {
    static const char* hx = "0123456789abcdef";
    std::string s(n * 2, '0');

    for (size_t i = 0; i < n; i++) {
        s[i * 2] = hx[d[i] >> 4];
        s[i * 2 + 1] = hx[d[i] & 0xF];
    }

    return s;
}

#if defined(_WIN32)
static const std::wstring& FilePath(const std::wstring& path) {
    return path;
}

struct Sha256Hasher {
    BCRYPT_ALG_HANDLE m_alg = nullptr;
    BCRYPT_HASH_HANDLE m_hash = nullptr;
    std::vector<unsigned char> m_obj;

    bool Open() {
        if (BCryptOpenAlgorithmProvider(&this->m_alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
            this->m_alg = nullptr;
            return false;
        }

        DWORD objLen = 0;
        DWORD cb = 0;
        BCryptGetProperty(this->m_alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objLen), sizeof(objLen), &cb, 0);
        this->m_obj.resize(objLen);

        if (BCryptCreateHash(this->m_alg, &this->m_hash, this->m_obj.data(), objLen, nullptr, 0, 0) != 0) {
            this->m_hash = nullptr;
            return false;
        }

        return true;
    }

    void Update(const void* data, size_t n) {
        if (n > 0) {
            BCryptHashData(this->m_hash, reinterpret_cast<PUCHAR>(const_cast<void*>(data)), static_cast<ULONG>(n), 0);
        }
    }

    bool FinishRaw(unsigned char out[32]) {
        return BCryptFinishHash(this->m_hash, out, 32, 0) == 0;
    }

    std::string Finish() {
        unsigned char digest[32];

        if (!this->FinishRaw(digest)) {
            return "";
        }

        return ToHex(digest, sizeof(digest));
    }

    ~Sha256Hasher() {
        if (this->m_hash) {
            BCryptDestroyHash(this->m_hash);
        }

        if (this->m_alg) {
            BCryptCloseAlgorithmProvider(this->m_alg, 0);
        }
    }
};
#else
// libstdc++ converts a wide path to UTF-8, which matches TextConv on Linux.
static std::filesystem::path FilePath(const std::wstring& path) {
    return std::filesystem::path(path);
}

struct Sha256Hasher {
    EVP_MD_CTX* m_ctx = nullptr;

    bool Open() {
        this->m_ctx = EVP_MD_CTX_new();

        if (!this->m_ctx) {
            return false;
        }

        if (EVP_DigestInit_ex(this->m_ctx, EVP_sha256(), nullptr) != 1) {
            EVP_MD_CTX_free(this->m_ctx);
            this->m_ctx = nullptr;
            return false;
        }

        return true;
    }

    void Update(const void* data, size_t n) {
        if (n > 0) {
            EVP_DigestUpdate(this->m_ctx, data, n);
        }
    }

    bool FinishRaw(unsigned char out[32]) {
        unsigned int len = 0;
        return EVP_DigestFinal_ex(this->m_ctx, out, &len) == 1 && len == 32;
    }

    std::string Finish() {
        unsigned char digest[32];

        if (!this->FinishRaw(digest)) {
            return "";
        }

        return ToHex(digest, sizeof(digest));
    }

    ~Sha256Hasher() {
        if (this->m_ctx) {
            EVP_MD_CTX_free(this->m_ctx);
        }
    }
};
#endif

// Hashes count bytes from the current position, false on a short read.
static bool HashRange(std::ifstream& f, Sha256Hasher& h, std::vector<char>& buf, long long count) {
    while (count > 0) {
        std::streamsize want = static_cast<std::streamsize>(std::min<long long>(count, static_cast<long long>(buf.size())));
        f.read(buf.data(), want);
        std::streamsize got = f.gcount();

        if (got <= 0) {
            return false;
        }

        h.Update(buf.data(), static_cast<size_t>(got));
        count -= got;
    }

    return true;
}

std::string Sha256File(const std::wstring& path) {
    std::ifstream f(FilePath(path), std::ios::binary);

    if (!f) {
        return "";
    }

    Sha256Hasher h;

    if (!h.Open()) {
        return "";
    }

    std::vector<char> buf(1 << 20);

    while (f) {
        f.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        std::streamsize got = f.gcount();

        if (got > 0) {
            h.Update(buf.data(), static_cast<size_t>(got));
        }
    }

    return h.Finish();
}

std::string QuickDigestFile(const std::wstring& path) {
    std::ifstream f(FilePath(path), std::ios::binary | std::ios::ate);

    if (!f) {
        return "";
    }

    long long size = static_cast<long long>(f.tellg());

    if (size < 0) {
        return "";
    }

    Sha256Hasher h;

    if (!h.Open()) {
        return "";
    }

    h.Update("hotquick1", 9);

    unsigned char le[8];

    for (int32_t i = 0; i < 8; i++) {
        le[i] = static_cast<unsigned char>(static_cast<unsigned long long>(size) >> (i * 8));
    }

    h.Update(le, sizeof(le));

    const bool whole = size <= s_windowBytes * s_windows;
    const int32_t windows = whole ? 1 : s_windows;
    std::vector<char> buf(static_cast<size_t>(s_windowBytes));

    for (int32_t i = 0; i < windows; i++) {
        long long offset = whole ? 0 : (size - s_windowBytes) * i / (s_windows - 1);
        // The previous window may have ended on EOF, which blocks seekg.
        f.clear();
        f.seekg(offset, std::ios::beg);

        if (!f) {
            return "";
        }

        if (!HashRange(f, h, buf, whole ? size : s_windowBytes)) {
            return "";
        }
    }

    std::string hex = h.Finish();

    if (hex.empty()) {
        return "";
    }

    return s_quickPrefix + hex;
}

bool IsQuickDigest(const std::string& digest) {
    return digest.compare(0, s_quickPrefixLen, s_quickPrefix) == 0;
}

bool Sha256Raw(const void* data, size_t size, unsigned char out[32]) {
    Sha256Hasher h;

    if (!h.Open()) {
        return false;
    }

    h.Update(data, size);
    return h.FinishRaw(out);
}

std::string Sha256Hex(const void* data, size_t size) {
    Sha256Hasher h;

    if (!h.Open()) {
        return "";
    }

    h.Update(data, size);
    return h.Finish();
}

struct Sha256Stream::Impl {
    Sha256Hasher m_hasher;
    bool m_ok = false;
};

Sha256Stream::Sha256Stream() : m_impl(std::make_unique<Impl>()) {
    this->m_impl->m_ok = this->m_impl->m_hasher.Open();
}

Sha256Stream::~Sha256Stream() = default;

void Sha256Stream::Update(const void* data, size_t size) {
    if (this->m_impl->m_ok) {
        this->m_impl->m_hasher.Update(data, size);
    }
}

std::string Sha256Stream::Finish() {
    if (!this->m_impl->m_ok) {
        return "";
    }

    this->m_impl->m_ok = false;
    return this->m_impl->m_hasher.Finish();
}

}
