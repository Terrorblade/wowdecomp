#ifndef HOT_MSDF_UTILS_H
#define HOT_MSDF_UTILS_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <vector>
#include "HoT/MSDF/Os.h"
#include "HoT/MSDF/Types.h"
#include "HoT/Platform/Files.h"

namespace HoT::MSDF {

class ScopedFileLock {
    public:
    bool AcquireExclusive(const std::filesystem::path& lockFilePath, uint32_t timeoutMs = 2000) {
        return this->m_lock.Acquire(lockFilePath, true, timeoutMs);
    }

    bool AcquireShared(const std::filesystem::path& lockFilePath, uint32_t timeoutMs = 2000) {
        return this->m_lock.Acquire(lockFilePath, false, timeoutMs);
    }

    void Release() noexcept {
        this->m_lock.Release();
    }

    private:
    Platform::FileLock m_lock;
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

struct FileGuard {
    Os::Handle m_handle = Os::INVALID_FILE;
    std::filesystem::path m_path;
    bool m_deleteOnFailure = false;
    bool m_successful = false;

    FileGuard(Os::Handle handle = Os::INVALID_FILE) : m_handle(handle) {}
    ~FileGuard() { this->Close(); }

    FileGuard(const FileGuard&) = delete;
    FileGuard& operator=(const FileGuard&) = delete;

    void Close() {
        if (this->m_handle != Os::INVALID_FILE) {
            Os::CloseFile(this->m_handle);
            this->m_handle = Os::INVALID_FILE;
        }

        if (this->m_deleteOnFailure && !this->m_successful && !this->m_path.empty()) {
            std::error_code ec;
            std::filesystem::remove(this->m_path, ec);
        }
    }

    bool IsValid() const { return this->m_handle != Os::INVALID_FILE; }
};

struct MappingGuard {
    Os::Handle m_handle = 0;

    MappingGuard(Os::Handle handle = 0) : m_handle(handle) {}
    ~MappingGuard() { this->Close(); }

    MappingGuard(const MappingGuard&) = delete;
    MappingGuard& operator=(const MappingGuard&) = delete;

    void Close() {
        if (this->m_handle) {
            Os::CloseMapping(this->m_handle);
            this->m_handle = 0;
        }
    }
};

// A placeholder view sits in a Manager arena slot, closing it keeps the slot's address range reserved.
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
            Os::UnmapFromSlot(this->m_ptr, this->m_size);
        } else {
            Os::Unmap(this->m_ptr, this->m_size);
        }

        this->m_ptr = nullptr;
        this->m_size = 0;
    }
};

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
