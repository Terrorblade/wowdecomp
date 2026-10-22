#include "HoT/Platform/Files.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <thread>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include "Engine/Source/Os/Linux/OsPath.h"

namespace HoT::Platform {

bool RenameReplacing(const std::filesystem::path& from, const std::filesystem::path& to) {
    return rename(from.c_str(), to.c_str()) == 0;
}

bool RemoveFile(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::remove(path, ec);
}

FILE* OpenStdioFile(const std::filesystem::path& path, const char* mode) {
    return fopen(path.c_str(), mode);
}

std::filesystem::path HostPath(const std::string& path) {
    std::string p = path;
    std::replace(p.begin(), p.end(), '\\', '/');
    return std::filesystem::path(p);
}

// The player's copy keeps whatever case it has, match the components like the rest of the client does.
std::filesystem::path ResolveHostPath(const std::filesystem::path& path) {
    char resolved[4096];
    return OsLinuxResolvePath(path.string().c_str(), resolved, sizeof(resolved), true) ? std::filesystem::path(resolved) : path;
}

// Game paths resolve against the working directory (the game folder) with on-disk case, like the POSIX OsFile does.
std::filesystem::path GameFilePath(const std::string& gamePath) {
    char resolved[4096];
    OsLinuxResolvePath(gamePath.c_str(), resolved, sizeof(resolved), true);

    std::error_code ec;
    return std::filesystem::current_path(ec) / resolved;
}

bool IsAbsoluteHostPath(const std::string& path) {
    return !path.empty() && path[0] == '/';
}

bool ReadOnlyFile::Open(const std::filesystem::path& path) {
    int file = open(path.c_str(), O_RDONLY | O_CLOEXEC);

    if (file < 0) {
        return false;
    }

    this->m_file = file;
    struct stat info;

    if (fstat(file, &info) != 0) {
        return false;
    }

    this->m_size = static_cast<int64_t>(info.st_size);
    return true;
}

void ReadOnlyFile::Close() {
    if (this->m_file >= 0) {
        close(static_cast<int>(this->m_file));
        this->m_file = -1;
    }

    this->m_size = 0;
}

int64_t ReadOnlyFile::ReadAt(int64_t offset, void* out, size_t size) {
    for (;;) {
        ssize_t got = pread(static_cast<int>(this->m_file), out, size, static_cast<off_t>(offset));

        if (got < 0 && errno == EINTR) {
            continue;
        }

        return got;
    }
}

// flock() on a separate open file description, which also excludes other handles in this process like LockFileEx.
bool FileLock::Acquire(const std::filesystem::path& lockFilePath, bool exclusive, uint32_t timeoutMs) {
    std::error_code ec;
    std::filesystem::create_directories(lockFilePath.parent_path(), ec);

    int file = open(lockFilePath.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);

    if (file < 0) {
        return false;
    }

    int operation = exclusive ? LOCK_EX : LOCK_SH;
    auto start = std::chrono::steady_clock::now();

    do {
        if (flock(file, operation | LOCK_NB) == 0) {
            this->m_file = file;
            this->m_locked = true;
            return true;
        }

        if (timeoutMs == 0 || errno != EWOULDBLOCK) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(timeoutMs));

    close(file);

    return false;
}

void FileLock::Release() noexcept {
    if (this->m_locked && this->m_file >= 0) {
        int file = static_cast<int>(this->m_file);
        flock(file, LOCK_UN);
        close(file);
        this->m_file = -1;
        this->m_locked = false;
    }
}

}
