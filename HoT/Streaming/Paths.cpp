#include "HoT/Streaming/Paths.h"
#include "HoT/Platform/Process.h"

namespace HoT::Streaming::Paths {

std::filesystem::path GameDir() {
    static const std::filesystem::path s_dir = Platform::GameDirectory();
    return s_dir;
}

std::filesystem::path LogsDir() {
    return GameDir() / "Logs";
}

std::filesystem::path WorkDir() {
    return GameDir() / ".hotlauncher";
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
    return GameDir() / "Cache";
}

}
