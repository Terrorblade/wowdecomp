#ifndef HOT_MSDF_UTILS_H
#define HOT_MSDF_UTILS_H

#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#undef min
#undef max
#else
#include <cerrno>
#include <chrono>
#include <thread>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <vector>
#include "HoT/MSDF/Types.h"

namespace HoT::MSDF {

#if defined(_WIN32)
namespace PlaceholderVA {

using VirtualAlloc2Fn = PVOID (WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
using MapViewOfFile3Fn = PVOID (WINAPI*)(HANDLE, HANDLE, PVOID, ULONG64, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
using UnmapViewOfFile2Fn = BOOL (WINAPI*)(HANDLE, PVOID, ULONG);

inline FARPROC KernelBaseProc(const char* name) {
    HMODULE module = GetModuleHandleW(L"kernelbase.dll");

    return module ? GetProcAddress(module, name) : nullptr;
}

inline VirtualAlloc2Fn VirtualAlloc2P() {
    static VirtualAlloc2Fn fn = reinterpret_cast<VirtualAlloc2Fn>(KernelBaseProc("VirtualAlloc2"));
    return fn;
}

inline MapViewOfFile3Fn MapViewOfFile3P() {
    static MapViewOfFile3Fn fn = reinterpret_cast<MapViewOfFile3Fn>(KernelBaseProc("MapViewOfFile3"));
    return fn;
}

inline UnmapViewOfFile2Fn UnmapViewOfFile2P() {
    static UnmapViewOfFile2Fn fn = reinterpret_cast<UnmapViewOfFile2Fn>(KernelBaseProc("UnmapViewOfFile2"));
    return fn;
}

inline bool Available() {
    return VirtualAlloc2P() && MapViewOfFile3P() && UnmapViewOfFile2P();
}

}

inline bool IsWine() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");

    return ntdll && GetProcAddress(ntdll, "wine_get_version") != nullptr;
}
#else
inline bool IsWine() {
    return false;
}
#endif

class ScopedFileLock {
    public:
    ScopedFileLock() = default;
    ~ScopedFileLock() { this->Release(); }

    ScopedFileLock(const ScopedFileLock&) = delete;
    ScopedFileLock& operator=(const ScopedFileLock&) = delete;

#if defined(_WIN32)
    bool AcquireExclusive(const std::filesystem::path& lockFilePath, DWORD timeoutMs = 2000) {
        return this->AcquireInternal(lockFilePath, LOCKFILE_EXCLUSIVE_LOCK, timeoutMs);
    }

    bool AcquireShared(const std::filesystem::path& lockFilePath, DWORD timeoutMs = 2000) {
        return this->AcquireInternal(lockFilePath, 0, timeoutMs);
    }

    void Release() noexcept {
        if (this->m_locked && this->m_file != INVALID_HANDLE_VALUE) {
            UnlockFileEx(this->m_file, 0, 1, 0, &this->m_overlapped);
            CloseHandle(this->m_file);
            this->m_file = INVALID_HANDLE_VALUE;
            this->m_locked = false;
        }
    }

    private:
    bool AcquireInternal(const std::filesystem::path& lockFilePath, DWORD flags, DWORD timeoutMs) {
        std::error_code ec;
        std::filesystem::create_directories(lockFilePath.parent_path(), ec);

        this->m_file = CreateFileW(lockFilePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (this->m_file == INVALID_HANDLE_VALUE) {
            return false;
        }

        ULONGLONG start = GetTickCount64();

        do {
            memset(&this->m_overlapped, 0, sizeof(this->m_overlapped));

            if (LockFileEx(this->m_file, flags | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &this->m_overlapped)) {
                this->m_locked = true;
                return true;
            }

            if (timeoutMs == 0 || GetLastError() != ERROR_LOCK_VIOLATION) {
                break;
            }

            Sleep(10);
        } while ((GetTickCount64() - start) < timeoutMs);

        CloseHandle(this->m_file);
        this->m_file = INVALID_HANDLE_VALUE;

        return false;
    }

    HANDLE m_file = INVALID_HANDLE_VALUE;
    OVERLAPPED m_overlapped = {};
    bool m_locked = false;
#else
    // flock() on a separate open file description, which also excludes other handles in this process like LockFileEx.
    bool AcquireExclusive(const std::filesystem::path& lockFilePath, DWORD timeoutMs = 2000) {
        return this->AcquireInternal(lockFilePath, LOCK_EX, timeoutMs);
    }

    bool AcquireShared(const std::filesystem::path& lockFilePath, DWORD timeoutMs = 2000) {
        return this->AcquireInternal(lockFilePath, LOCK_SH, timeoutMs);
    }

    void Release() noexcept {
        if (this->m_locked && this->m_file >= 0) {
            flock(this->m_file, LOCK_UN);
            close(this->m_file);
            this->m_file = -1;
            this->m_locked = false;
        }
    }

    private:
    bool AcquireInternal(const std::filesystem::path& lockFilePath, int operation, DWORD timeoutMs) {
        std::error_code ec;
        std::filesystem::create_directories(lockFilePath.parent_path(), ec);

        this->m_file = open(lockFilePath.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);

        if (this->m_file < 0) {
            return false;
        }

        auto start = std::chrono::steady_clock::now();

        do {
            if (flock(this->m_file, operation | LOCK_NB) == 0) {
                this->m_locked = true;
                return true;
            }

            if (timeoutMs == 0 || errno != EWOULDBLOCK) {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(timeoutMs));

        close(this->m_file);
        this->m_file = -1;

        return false;
    }

    int m_file = -1;
    bool m_locked = false;
#endif
};

// Keeps released vectors in power of two buckets so glyph generation does not churn the heap.
template <typename T>
class VectorPool {
    public:
    std::vector<T> Acquire(size_t minimumCapacity) {
        int32_t index = BucketIndexFor(minimumCapacity);
        std::vector<std::vector<T>>& slot = this->m_buckets[index];

        if (!slot.empty()) {
            std::vector<T> v = std::move(slot.back());
            slot.pop_back();

            if (v.capacity() < minimumCapacity) {
                v.reserve(std::max(minimumCapacity, CapacityForIndex(index)));
            }

            return v;
        }

        std::vector<T> v;
        v.reserve(std::max(minimumCapacity, CapacityForIndex(index)));

        return v;
    }

    std::vector<T> AcquireSized(size_t size, T init = T()) {
        std::vector<T> v = this->Acquire(size);
        v.resize(size, init);

        return v;
    }

    void Release(std::vector<T>&& v) {
        size_t capacity = v.capacity();

        if (capacity < MIN_BUCKET_SIZE) {
            return;
        }

        int32_t index = BucketIndexFor(capacity);

        if (index >= BUCKETS || this->m_buckets[index].size() >= MAX_VECTORS_PER_BUCKET) {
            std::vector<T>().swap(v);
            return;
        }

        v.clear();
        this->m_buckets[index].push_back(std::move(v));
    }

    private:
    static const size_t MIN_BUCKET_SIZE = 64;
    static const int32_t BUCKETS = 12;
    static const size_t MAX_VECTORS_PER_BUCKET = 100;

    static int32_t BucketIndexFor(size_t capacity) {
        if (capacity <= MIN_BUCKET_SIZE) {
            return 0;
        }

        int32_t bits = 0;

        for (size_t value = capacity - 1; value; value >>= 1) {
            bits++;
        }

        int32_t index = bits - 6;

        return std::clamp(index, 0, BUCKETS - 1);
    }

    static size_t CapacityForIndex(int32_t index) {
        return MIN_BUCKET_SIZE << index;
    }

    std::array<std::vector<std::vector<T>>, BUCKETS> m_buckets;
};

#if defined(_WIN32)
struct FileGuard {
    HANDLE m_handle = INVALID_HANDLE_VALUE;
    std::filesystem::path m_path;
    bool m_deleteOnFailure = false;
    bool m_successful = false;

    FileGuard(HANDLE handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}
    ~FileGuard() { this->Close(); }

    FileGuard(const FileGuard&) = delete;
    FileGuard& operator=(const FileGuard&) = delete;

    void Close() {
        if (this->m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(this->m_handle);
            this->m_handle = INVALID_HANDLE_VALUE;
        }

        if (this->m_deleteOnFailure && !this->m_successful && !this->m_path.empty()) {
            std::error_code ec;
            std::filesystem::remove(this->m_path, ec);
        }
    }

    bool IsValid() const { return this->m_handle != INVALID_HANDLE_VALUE; }
};

struct MappingGuard {
    HANDLE m_handle = nullptr;

    MappingGuard(HANDLE handle = nullptr) : m_handle(handle) {}
    ~MappingGuard() { this->Close(); }

    MappingGuard(const MappingGuard&) = delete;
    MappingGuard& operator=(const MappingGuard&) = delete;

    void Close() {
        if (this->m_handle) {
            CloseHandle(this->m_handle);
            this->m_handle = nullptr;
        }
    }

    bool IsValid() const { return this->m_handle != nullptr; }
};

struct ViewGuard {
    void* m_ptr = nullptr;
    bool m_placeholder = false;

    ViewGuard(void* ptr = nullptr) : m_ptr(ptr) {}
    ~ViewGuard() { this->Close(); }

    ViewGuard(const ViewGuard&) = delete;
    ViewGuard& operator=(const ViewGuard&) = delete;

    void Close() {
        if (!this->m_ptr) {
            return;
        }

        if (this->m_placeholder) {
            if (PlaceholderVA::UnmapViewOfFile2Fn unmap = PlaceholderVA::UnmapViewOfFile2P()) {
                unmap(GetCurrentProcess(), this->m_ptr, MEM_PRESERVE_PLACEHOLDER);
            }
        } else {
            UnmapViewOfFile(this->m_ptr);
        }

        this->m_ptr = nullptr;
    }
};
#else
// m_handle is a file descriptor here.
struct FileGuard {
    int m_handle = -1;
    std::filesystem::path m_path;
    bool m_deleteOnFailure = false;
    bool m_successful = false;

    FileGuard(int handle = -1) : m_handle(handle) {}
    ~FileGuard() { this->Close(); }

    FileGuard(const FileGuard&) = delete;
    FileGuard& operator=(const FileGuard&) = delete;

    void Close() {
        if (this->m_handle >= 0) {
            close(this->m_handle);
            this->m_handle = -1;
        }

        if (this->m_deleteOnFailure && !this->m_successful && !this->m_path.empty()) {
            std::error_code ec;
            std::filesystem::remove(this->m_path, ec);
        }
    }

    bool IsValid() const { return this->m_handle >= 0; }
};

// mmap() maps the descriptor directly, there is no section object to keep, so this only mirrors the Windows shape.
struct MappingGuard {
    void Close() {}
    bool IsValid() const { return true; }
};

// A placeholder view sits inside the Manager arena: closing it puts the PROT_NONE reservation back instead of
// unmapping, so the slot's address range stays reserved.
struct ViewGuard {
    void* m_ptr = nullptr;
    size_t m_size = 0;
    bool m_placeholder = false;

    ViewGuard(void* ptr = nullptr, size_t size = 0) : m_ptr(ptr), m_size(size) {}
    ~ViewGuard() { this->Close(); }

    ViewGuard(const ViewGuard&) = delete;
    ViewGuard& operator=(const ViewGuard&) = delete;

    void Close() {
        if (!this->m_ptr) {
            return;
        }

        if (this->m_placeholder) {
            mmap(this->m_ptr, this->m_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_NORESERVE, -1, 0);
        } else {
            munmap(this->m_ptr, this->m_size);
        }

        this->m_ptr = nullptr;
        this->m_size = 0;
    }
};

inline bool WriteAll(int fd, const void* data, size_t size) {
    const uint8_t* p = static_cast<const uint8_t*>(data);

    while (size > 0) {
        ssize_t n = write(fd, p, size);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            return false;
        }

        p += n;
        size -= static_cast<size_t>(n);
    }

    return true;
}
#endif

template <typename F>
struct FinalAction {
    F m_clean;

    FinalAction(F clean) : m_clean(clean) {}
    ~FinalAction() { this->m_clean(); }

    FinalAction(const FinalAction&) = delete;
    FinalAction& operator=(const FinalAction&) = delete;
};

using FontHash = uint64_t;

// FNV-1a over the whole font file.
inline FontHash HashFont(const uint8_t* data, size_t size) {
    uint64_t hash = 0xcbf29ce484222325ULL;

    for (size_t i = 0; i < size; i++) {
        hash ^= data[i];
        hash *= 0x100000001b3ULL;
    }

    return hash;
}

struct Pools {
    inline static thread_local VectorPool<uint8_t> s_byte;
    inline static thread_local VectorPool<float> s_float;
    inline static thread_local VectorPool<uint32_t> s_uint32;
    inline static thread_local VectorPool<GlyphEntry> s_glyph;
    inline static thread_local VectorPool<ManifestEntry> s_manifest;
};

}

#endif
