#include "HoT/Streaming/MpqScanner.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <thread>
#include "HoT/Core/Chat.h"
#include "HoT/Game/Session.h"
#include "HoT/Lua/Script.h"
#include "HoT/Platform/Files.h"
#include "HoT/Platform/Process.h"
#include "HoT/Streaming/BackgroundDownloader.h"
#include "HoT/Streaming/LocalDigests.h"
#include "HoT/Streaming/Paths.h"
#include "HoT/Streaming/TextConv.h"

namespace fs = std::filesystem;

namespace HoT::Streaming::MpqScanner {

static std::mutex s_mutex;
static std::vector<MpqInfo> s_results;
static std::atomic<bool> s_done{ false };
static unsigned s_scannedGeneration = 0;

static std::mutex s_asyncMutex;
static std::vector<Callback> s_waiting;
static std::vector<MpqInfo> s_asyncResults;
static bool s_asyncReady = false;
static bool s_asyncRunning = false;

static std::string ToLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

static fs::path GetDataFolder() {
    return Platform::ResolveHostPath(Paths::GameDir() / "Data");
}

// An archive with a (hotmanifest) is identified by its contents, anything else by the sampled digest.
static std::string DigestOf(const std::wstring& path) {
    std::string digest;

    if (!LocalDigests::Get(path, LocalDigests::Kind::ContentId, digest)) {
        return "";
    }

    if (digest.empty()) {
        LocalDigests::Get(path, LocalDigests::Kind::Quick, digest);
    }

    return digest;
}

static void Rescan() {
    s_results.clear();

    fs::path dataFolder = GetDataFolder();
    std::error_code ec;

    if (!fs::exists(dataFolder, ec)) {
        return;
    }

    auto scanFolder = [&](const fs::path& folder) {
        std::error_code ec;

        for (const auto& entry : fs::directory_iterator(folder, ec)) {
            if (!entry.is_regular_file(ec)) {
                continue;
            }

            if (Text::LowerPath(entry.path().extension().wstring()) != L".mpq") {
                continue;
            }

            // A download parked next to the real file waiting for the swap is not installed yet, reporting it only draws a warning.
            if (IsStagedName(entry.path().filename().wstring())) {
                continue;
            }

            MpqInfo info;
            info.m_filenameLower = ToLower(Text::NarrowAcp(entry.path().filename().wstring()));
            // Still reported so it counts as present, but the copy on disk is the old one so hashing it only costs I/O.
            info.m_updating = IsUpdatePending(entry.path().wstring());

            if (!info.m_updating) {
                info.m_digest = DigestOf(entry.path().wstring());
            }

            s_results.push_back(info);
        }
    };

    scanFolder(dataFolder);
    scanFolder(Platform::ResolveHostPath(dataFolder / "enUS"));
    scanFolder(Platform::ResolveHostPath(dataFolder / "enGB"));
    LocalDigests::Save();
}

bool IsDone() {
    return s_done.load();
}

std::vector<MpqInfo> GetResults() {
    std::lock_guard<std::mutex> lock(s_mutex);
    const unsigned generation = UpdateGeneration();

    if (!s_done.load() || generation != s_scannedGeneration) {
        Rescan();
        s_scannedGeneration = generation;
        s_done = true;
    }

    return s_results;
}

void ScanAsync(Callback onDone) {
    std::lock_guard<std::mutex> lock(s_asyncMutex);

    if (onDone) {
        s_waiting.push_back(std::move(onDone));
    }

    if (s_asyncRunning) {
        return;
    }

    s_asyncRunning = true;
    s_asyncReady = false;

    std::thread([] {
        Platform::SetBackgroundThreadPriority();
        std::vector<MpqInfo> scanned;

        try {
            scanned = GetResults();
        } catch (...) {
        }

        std::lock_guard<std::mutex> lock(s_asyncMutex);
        s_asyncResults = std::move(scanned);
        s_asyncReady = true;
        s_asyncRunning = false;
    }).detach();
}

void Pump() {
    std::vector<Callback> callbacks;
    std::vector<MpqInfo> scanned;

    {
        std::lock_guard<std::mutex> lock(s_asyncMutex);

        if (!s_asyncReady || s_waiting.empty()) {
            return;
        }

        callbacks.swap(s_waiting);
        scanned = s_asyncResults;
    }

    for (Callback& cb : callbacks) {
        cb(scanned);
    }
}

static int32_t Script_GetMpqList(lua_State* L) {
    ScanAsync([](const std::vector<MpqInfo>& mpqs) {
        if (!Session::InWorld()) {
            return;
        }

        for (const MpqInfo& mpq : mpqs) {
            Chat::Print("%s %s", mpq.m_filenameLower.c_str(), mpq.m_updating ? "(updating)" : mpq.m_digest.c_str());
        }
    });

    return 0;
}

void RegisterScripts() {
    Lua::RegisterFunction("GetMpqList", Script_GetMpqList, Lua::State::Game);
}

void Start() {
    static std::atomic<bool> s_started{ false };

    if (s_started.exchange(true)) {
        return;
    }

    std::thread([] {
        while (IsBusy()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        ScanAsync(nullptr);
    }).detach();
}

}
