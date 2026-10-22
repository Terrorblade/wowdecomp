#ifndef HOT_PLATFORM_FILES_H
#define HOT_PLATFORM_FILES_H

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

namespace HoT::Platform {

// Moves from over to, replacing an existing file.
bool RenameReplacing(const std::filesystem::path& from, const std::filesystem::path& to);

bool RemoveFile(const std::filesystem::path& path);

FILE* OpenStdioFile(const std::filesystem::path& path, const char* mode);

// A relative path written with either separator, as a host path.
std::filesystem::path HostPath(const std::string& path);

// The on-disk spelling of a path whose components may differ in case, unchanged where nothing matches.
std::filesystem::path ResolveHostPath(const std::filesystem::path& path);

// Where a loose file for a game path such as "World\\Maps\\x.adt" lives on disk.
std::filesystem::path GameFilePath(const std::string& gamePath);

bool IsAbsoluteHostPath(const std::string& path);

// Random access reads that leave the file open to writers and deletion.
class ReadOnlyFile {
    public:
    ReadOnlyFile() = default;
    ~ReadOnlyFile() { this->Close(); }

    ReadOnlyFile(const ReadOnlyFile&) = delete;
    ReadOnlyFile& operator=(const ReadOnlyFile&) = delete;

    bool Open(const std::filesystem::path& path);
    void Close();

    int64_t Size() const { return this->m_size; }

    // Up to size bytes at offset, 0 at the end of the file and -1 on error.
    int64_t ReadAt(int64_t offset, void* out, size_t size);

    private:
    intptr_t m_file = -1;
    int64_t m_size = 0;
};

// Advisory lock on a lock file, released by Release() or the destructor.
class FileLock {
    public:
    FileLock() = default;
    ~FileLock() { this->Release(); }

    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;

    bool Acquire(const std::filesystem::path& lockFilePath, bool exclusive, uint32_t timeoutMs);
    void Release() noexcept;

    private:
    intptr_t m_file = -1;
    bool m_locked = false;
};

}

#endif
