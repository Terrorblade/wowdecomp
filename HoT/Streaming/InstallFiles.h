#ifndef HOT_STREAMING_INSTALLFILES_H
#define HOT_STREAMING_INSTALLFILES_H

#include <cstdint>
#include <filesystem>
#include <string>

// The per OS parts of how the background downloader places files in the install.
namespace HoT::Streaming::InstallFiles {

// The MoveFileExW flags of the same names.
enum : uint32_t {
    MOVE_REPLACE_EXISTING = 0x1,
    MOVE_COPY_ALLOWED = 0x2,
    MOVE_WRITE_THROUGH = 0x8,
};

// MoveFileExW semantics, replacedRunningExe is set when the running binary was renamed over.
bool Move(const std::wstring& src, const std::wstring& dst, uint32_t flags, bool& replacedRunningExe);

// Paths in the pending file are written in the code page the swap helper reads.
std::string NarrowPendingPath(const std::wstring& path, bool* lossy);
uint32_t PendingCodePage();

// One patcher per install across processes, held for the process lifetime.
bool AcquirePatchLock(const std::wstring& installDir);

// Helpers that apply the pending file once the client has closed, where something it holds open blocks a move.
void SpawnCloseSwapHelper(const std::filesystem::path& pendingFile);
void SpawnExitSwapHelper(const std::filesystem::path& pendingFile);

bool IsDiscordLibrary(const std::string& path);

// Manifest binaries built for another OS, which are never downloaded here.
bool IsForeignBinary(const std::string& path);

// The manifest platform tag of this OS, files tagged for any other OS are skipped.
const char* ManifestPlatform();

}

#endif
