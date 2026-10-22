#ifndef HOT_STREAMING_PATHS_H
#define HOT_STREAMING_PATHS_H

#include <filesystem>

namespace HoT::Streaming::Paths {

std::filesystem::path GameDir();
std::filesystem::path LogsDir();
std::filesystem::path WorkDir();
std::filesystem::path DownloadsDir();
std::filesystem::path StagedDir();
std::filesystem::path BlobsDir();
std::filesystem::path LegacyCacheDir();

}

#endif
