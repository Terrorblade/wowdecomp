#ifndef HOT_STREAMING_BACKGROUNDDOWNLOADER_H
#define HOT_STREAMING_BACKGROUNDDOWNLOADER_H

#include <cstdint>
#include <string>
#include <vector>

struct SArchive;

namespace HoT {
class Module;
}

// Keeps the install in line with the signed launcher manifest while the game runs.
namespace HoT::Streaming {

// Turns the downloader on once, the first pass and the 5 minute poller start on the next frame.
void Start();

// Runs a pass now unless one is already running or the downloader is off.
void Trigger();

void StartPolling();

// Main thread work, one batch item per frame from the module's OnIdle.
void PumpMainThread();

bool IsActive();

bool IsEnabled();

// An active download, or the first update check has not finished yet.
bool IsBusy();

// A downloaded file is staged and only swaps in on the next launch.
bool IsRestartPending();

// True when the manifest says this absolute path is queued, downloading or staged for the swap.
bool IsUpdatePending(const std::wstring& absPath);

// Bumps whenever a pass changes what needs replacing or lands a new file.
unsigned UpdateGeneration();

// True for a download parked next to the real file as <stem>.new-<8 hex>.<ext>. Takes a bare file name.
bool IsStagedName(const std::wstring& fileName);

using StreamListener = void (*)();

// Runs when a batch landed world art, before the loaded models are rebuilt.
void OnArtStreamed(StreamListener listener);

// Runs after every applied batch, after the world model rebuild and before the glue model rebuild.
void OnBatchApplied(StreamListener listener);

// Swaps in staged files that are free to move, from EventPostCloseEx.
void SwapOnClose();

// Closes the archives HoT mounted and swaps staged files in, from BeginCloseGame after CloseAllArchives.
void OnCloseGame();

void RefreshArchiveNames(const std::vector<std::string>& names);
void CollectMountedNames(SArchive* archive, std::vector<std::string>& out);
int32_t NextMountPriority();
void NoteMountsChanged();

// Lua functions, SMSG_BACKGROUND_DOWNLOAD, the enableStartupDownloader check, PumpMainThread and the MPQ scanner pump.
Module* GetModule();

}

#endif
