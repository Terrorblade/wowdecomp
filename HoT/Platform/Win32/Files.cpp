#include "HoT/Platform/Files.h"
#include <cstring>
#include "HoT/Platform/Process.h"
#include <windows.h>

namespace HoT::Platform {

static std::wstring WidenUtf8(const std::string& text) {
    if (text.empty()) {
        return std::wstring();
    }

    int32_t length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int32_t>(text.size()), nullptr, 0);

    if (length <= 0) {
        return std::wstring();
    }

    std::wstring result(length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int32_t>(text.size()), &result[0], length);
    return result;
}

bool RenameReplacing(const std::filesystem::path& from, const std::filesystem::path& to) {
    return MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

bool RemoveFile(const std::filesystem::path& path) {
    return DeleteFileW(path.c_str()) != 0;
}

FILE* OpenStdioFile(const std::filesystem::path& path, const char* mode) {
    std::wstring wideMode(mode, mode + strlen(mode));
    return _wfopen(path.c_str(), wideMode.c_str());
}

std::filesystem::path HostPath(const std::string& path) {
    return std::filesystem::path(path);
}

std::filesystem::path ResolveHostPath(const std::filesystem::path& path) {
    return path;
}

std::filesystem::path GameFilePath(const std::string& gamePath) {
    return std::filesystem::path(GameDirectory().wstring() + L"\\" + WidenUtf8(gamePath));
}

bool IsAbsoluteHostPath(const std::string& path) {
    return path.find(':') != std::string::npos || path.rfind("\\\\", 0) == 0;
}

bool ReadOnlyFile::Open(const std::filesystem::path& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    this->m_file = reinterpret_cast<intptr_t>(file);
    LARGE_INTEGER size{};

    if (!GetFileSizeEx(file, &size)) {
        return false;
    }

    this->m_size = size.QuadPart;
    return true;
}

void ReadOnlyFile::Close() {
    if (this->m_file != -1) {
        CloseHandle(reinterpret_cast<HANDLE>(this->m_file));
        this->m_file = -1;
    }

    this->m_size = 0;
}

int64_t ReadOnlyFile::ReadAt(int64_t offset, void* out, size_t size) {
    OVERLAPPED ov{};
    ov.Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
    ov.OffsetHigh = static_cast<DWORD>(offset >> 32);
    DWORD got = 0;

    if (!::ReadFile(reinterpret_cast<HANDLE>(this->m_file), out, static_cast<DWORD>(size), &got, &ov)) {
        return -1;
    }

    return got;
}

bool FileLock::Acquire(const std::filesystem::path& lockFilePath, bool exclusive, uint32_t timeoutMs) {
    std::error_code ec;
    std::filesystem::create_directories(lockFilePath.parent_path(), ec);

    HANDLE file = CreateFileW(lockFilePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD flags = exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0;
    ULONGLONG start = GetTickCount64();

    do {
        OVERLAPPED overlapped = {};

        if (LockFileEx(file, flags | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &overlapped)) {
            this->m_file = reinterpret_cast<intptr_t>(file);
            this->m_locked = true;
            return true;
        }

        if (timeoutMs == 0 || GetLastError() != ERROR_LOCK_VIOLATION) {
            break;
        }

        Sleep(10);
    } while ((GetTickCount64() - start) < timeoutMs);

    CloseHandle(file);

    return false;
}

void FileLock::Release() noexcept {
    if (this->m_locked && this->m_file != -1) {
        HANDLE file = reinterpret_cast<HANDLE>(this->m_file);
        OVERLAPPED overlapped = {};
        UnlockFileEx(file, 0, 1, 0, &overlapped);
        CloseHandle(file);
        this->m_file = -1;
        this->m_locked = false;
    }
}

}
