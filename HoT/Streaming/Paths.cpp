#include "HoT/Streaming/Paths.h"
#if defined(_WIN32)
#include <windows.h>
#endif
#include <string>
#include <vector>

namespace HoT::Streaming::Paths {

std::filesystem::path ExeDir() {
    static const std::filesystem::path s_dir = [] {
#if defined(_WIN32)
        std::vector<wchar_t> buffer(MAX_PATH);

        for (;;) {
            DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));

            if (length == 0) {
                std::error_code ec;
                return std::filesystem::current_path(ec);
            }

            if (length < buffer.size()) {
                return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
            }

            buffer.resize(buffer.size() * 2);
        }
#else
        std::error_code ec;
        std::filesystem::path exe = std::filesystem::read_symlink("/proc/self/exe", ec);

        if (ec) {
            return std::filesystem::current_path(ec);
        }

        return exe.parent_path();
#endif
    }();

    return s_dir;
}

std::filesystem::path LogsDir() {
    return ExeDir() / "Logs";
}

std::filesystem::path WorkDir() {
    return ExeDir() / ".hotlauncher";
}

std::filesystem::path DownloadsDir() {
    return WorkDir() / "downloads";
}

std::filesystem::path StagedDir() {
    return WorkDir() / "staged";
}

std::filesystem::path BlobsDir() {
    return WorkDir() / "blobs";
}

std::filesystem::path LegacyCacheDir() {
    return ExeDir() / "Cache";
}

}
