#include "HoT/Streaming/BackgroundDownloader.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "Engine/Source/Os/Linux/OsPath.h"
#endif
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwctype>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Model2/M2Cache.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "HoT/Core/CVars.h"
#include "HoT/Core/Modules.h"
#include "HoT/DB/Tables.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "HoT/Runtime/LauncherSettings.h"
#include "HoT/Streaming/ArchiveManifest.h"
#include "HoT/Streaming/ArchivePatcher.h"
#include "HoT/Streaming/ArchiveRegistry.h"
#include "HoT/Streaming/BlobContainer.h"
#include "HoT/Streaming/HttpClient.h"
#include "HoT/Streaming/LauncherManifest.h"
#include "HoT/Streaming/LocalDigests.h"
#include "HoT/Streaming/ManifestSignature.h"
#include "HoT/Streaming/MpqFile.h"
#include "HoT/Streaming/MpqScanner.h"
#include "HoT/Streaming/Paths.h"
#include "HoT/Streaming/Sha256.h"
#include "HoT/Streaming/StreamLog.h"
#include "HoT/Streaming/TextConv.h"
#include "WoW/Source/Glue/CGlueMgr.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/AchievementInfo.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/SpellBookFrame.h"
#include "WoW/Source/WorldClient/World.h"

namespace fs = std::filesystem;

namespace HoT::Streaming {

static std::atomic<bool> s_started{ false };
static std::atomic<bool> s_running{ false };
static std::atomic<bool> s_pollerStarted{ false };
static std::atomic<bool> s_kickPending{ false };
static std::wstring s_installDir;

static constexpr uint32_t s_pollIntervalMs = 5 * 60 * 1000;
static constexpr int32_t s_downloadWorkers = 4;

static std::atomic<bool> s_firstPassDone{ false };
static std::atomic<bool> s_active{ false };
static std::atomic<long long> s_doneBytes{ 0 };
static std::atomic<long long> s_totalBytes{ 0 };
static std::atomic<int32_t> s_filesDone{ 0 };
static std::atomic<int32_t> s_filesTotal{ 0 };
static std::atomic<bool> s_restartNeeded{ false };
static std::atomic<bool> s_uiRefreshNeeded{ false };
static std::mutex s_statusMutex;
static std::string s_currentFile;

static constexpr unsigned long long s_progressJoinMs = 30000;
static std::atomic<unsigned long long> s_lastProgressMs{ 0 };

static std::vector<StreamListener> s_artListeners;
static std::vector<StreamListener> s_batchListeners;

#if !defined(_WIN32)
static constexpr uint32_t MOVEFILE_REPLACE_EXISTING = 0x1;
static constexpr uint32_t MOVEFILE_COPY_ALLOWED = 0x2;
static constexpr uint32_t MOVEFILE_WRITE_THROUGH = 0x8;

static unsigned long long GetTickCount64() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<unsigned long long>(ts.tv_sec) * 1000ULL + static_cast<unsigned long long>(ts.tv_nsec / 1000000);
}

static void LowerThreadPriority() {
    // Linux nice values are per thread when given the thread id.
    setpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)), 19);
}

// /proc/self/exe stats the running image even after it was renamed over, so this holds only before a replace.
static bool IsRunningExe(const std::wstring& path) {
    std::error_code ec;
    return fs::equivalent(fs::path(path), fs::path("/proc/self/exe"), ec);
}

// Linux renames over files that are open or running, so nothing here fails for being in use. A replaced running
// binary only takes effect on the next launch, which is what a staged exe means on Windows.
static bool MoveFileExW(const wchar_t* src, const wchar_t* dst, uint32_t flags) {
    std::error_code ec;
    const fs::path from(src);
    const fs::path to(dst);

    if (!(flags & MOVEFILE_REPLACE_EXISTING) && fs::exists(to, ec)) {
        return false;
    }

    const bool runningExe = IsRunningExe(dst);
    fs::rename(from, to, ec);

    if (ec == std::errc::cross_device_link && (flags & MOVEFILE_COPY_ALLOWED)) {
        const fs::path tmp = to.wstring() + L".copy";
        ec.clear();
        fs::copy_file(from, tmp, fs::copy_options::overwrite_existing, ec);

        if (!ec) {
            fs::rename(tmp, to, ec);
        }

        if (ec) {
            std::error_code ignored;
            fs::remove(tmp, ignored);
            return false;
        }

        fs::remove(from, ec);
        ec.clear();
    }

    if (ec) {
        return false;
    }

    if (runningExe) {
        s_restartNeeded = true;
    }

    return true;
}
#endif

static void ResetProgress() {
    s_filesDone = 0;
    s_filesTotal = 0;
    s_doneBytes = 0;
    s_totalBytes = 0;
}

static DownloadThrottle s_throttle([] {
    return static_cast<long long>(LauncherSettings::MaxDownloadBytesPerSecond());
});

struct ByteCounter {
    long long m_size = 0;
    long long m_shown = 0;

    void Report(long long written) {
        long long now = std::min(written, this->m_size);
        s_doneBytes.fetch_add(now - this->m_shown);
        this->m_shown = now;
    }

    void Settle() {
        s_doneBytes.fetch_add(this->m_size - this->m_shown);
        this->m_shown = this->m_size;
    }
};

static void SetCurrentFile(const std::string& label) {
    std::lock_guard<std::mutex> lock(s_statusMutex);
    s_currentFile = label;
}

static constexpr int32_t s_mountPriorityBase = 1000000;
static constexpr uint32_t s_mountFlags = 0xC00;
static std::atomic<int32_t> s_nextMountPriority{ s_mountPriorityBase };

// Main thread only, closed at exit.
static std::vector<SArchive*> s_ownedMounts;

static std::wstring Widen(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

static std::string NarrowOem(const std::wstring& s, bool* lossy) {
    if (s.empty()) {
        return "";
    }

#if defined(_WIN32)
    const bool utf8 = GetOEMCP() == CP_UTF8;
    const DWORD flags = utf8 ? 0 : WC_NO_BEST_FIT_CHARS;
    BOOL usedDefault = FALSE;
    BOOL* used = utf8 ? nullptr : &usedDefault;
    int32_t n = WideCharToMultiByte(CP_OEMCP, flags, s.c_str(), static_cast<int32_t>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_OEMCP, flags, s.c_str(), static_cast<int32_t>(s.size()), out.data(), n, nullptr, used);

    if (lossy && usedDefault) {
        *lossy = true;
    }

    return out;
#else
    (void)lossy;
    return Text::NarrowUtf8(s);
#endif
}

#ifdef AUTO_UPDATER_IGNORES_EXE
static bool IsDiscordDll(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
#if defined(_WIN32)
    return Text::LowerAscii(name) == "discord_game_sdk_64.dll";
#else
    return Text::LowerAscii(name) == "discord_game_sdk_64.dll" || Text::LowerAscii(name) == "discord_game_sdk.so";
#endif
}
#endif

#if !defined(_WIN32)
// The manifest describes the Windows install; its PE executables and DLLs have no use next to bin/Linux/Wow64.
static bool IsWindowsBinary(const std::string& path) {
    size_t dot = path.find_last_of('.');
    size_t slash = path.find_last_of("/\\");

    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
        return false;
    }

    const std::string ext = Text::LowerAscii(path.substr(dot));
    return ext == ".exe" || ext == ".dll";
}

// Manifest paths may use either separator, and the files on disk keep whatever case the player's copy has.
static fs::path HostRelPath(const std::string& relPath) {
    std::string p = relPath;
    std::replace(p.begin(), p.end(), '\\', '/');
    return fs::path(p);
}
#endif

static bool IsMpq(const std::string& path) {
    size_t dot = path.find_last_of('.');

    if (dot == std::string::npos) {
        return false;
    }

    return Text::LowerAscii(path.substr(dot)) == ".mpq";
}

static bool PathEqual(const std::wstring& a, const std::wstring& b) {
    return a.size() == b.size() && Text::LowerPath(a) == Text::LowerPath(b);
}

static std::wstring StagedPath(const ManifestFile& mf, const std::wstring& local) {
    fs::path lp(local);
    std::wstring name = lp.stem().wstring() + L".new-" + Widen(mf.m_sha256.substr(0, 8)) + lp.extension().wstring();
#if defined(_WIN32)
    return (Paths::StagedDir() / fs::path(mf.m_path).parent_path() / name).wstring();
#else
    return (Paths::StagedDir() / HostRelPath(mf.m_path).parent_path() / name).wstring();
#endif
}

static std::wstring PartPath(const ManifestFile& mf) {
#if defined(_WIN32)
    return (Paths::DownloadsDir() / fs::path(mf.m_path)).wstring() + L".part";
#else
    return (Paths::DownloadsDir() / HostRelPath(mf.m_path)).wstring() + L".part";
#endif
}

// The other half of StagedPath, does the name end in the .new-<8 hex> tag it appends.
static bool HasStagedTag(const std::wstring& name) {
    const std::wstring marker = L".new-";

    if (name.size() < marker.size() + 8) {
        return false;
    }

    if (name.compare(name.size() - 8 - marker.size(), marker.size(), marker) != 0) {
        return false;
    }

    for (size_t i = name.size() - 8; i < name.size(); i++) {
        if (!iswxdigit(name[i])) {
            return false;
        }
    }

    return true;
}

static bool StartsWithNoCase(const std::wstring& s, const std::wstring& pre) {
    return s.size() >= pre.size() && Text::LowerPath(s.substr(0, pre.size())) == Text::LowerPath(pre);
}

static bool EndsWithNoCase(const std::wstring& s, const std::wstring& suf) {
    return s.size() >= suf.size() && Text::LowerPath(s.substr(s.size() - suf.size())) == Text::LowerPath(suf);
}

static std::wstring AppendQuery(const std::wstring& url, const std::wstring& key, const std::wstring& value) {
    wchar_t sep = (url.find(L'?') == std::wstring::npos) ? L'?' : L'&';
    return url + sep + key + L"=" + value;
}

static std::string ReadSmallFile(const std::wstring& p) {
    std::ifstream f(fs::path(p), std::ios::binary);

    if (!f) {
        return "";
    }

    std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return s;
}

static void WriteSmallFile(const std::wstring& p, const std::string& s) {
    std::ofstream f(fs::path(p), std::ios::trunc | std::ios::binary);

    if (f) {
        f.write(s.data(), static_cast<std::streamsize>(s.size()));
    }
}

static std::string UrlEncodePath(const std::string& p) {
    static const char* hex = "0123456789ABCDEF";
    std::string o;

    for (unsigned char c : p) {
        if (std::isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~') {
            o += static_cast<char>(c);
        } else {
            o += '%';
            o += hex[c >> 4];
            o += hex[c & 0xF];
        }
    }

    return o;
}

static long long FileSize(const std::wstring& p) {
    std::error_code ec;
    auto s = fs::file_size(p, ec);
    return ec ? -1 : static_cast<long long>(s);
}

static fs::path PendingFile() {
    return Paths::WorkDir() / "hotstream-pending.txt";
}

static void RetireLegacyCacheFiles() {
    static const char* const s_names[] = { "hotstream-pending.txt", "hotstream-swap.hold", "hotstream-swap.cmd", "hotstream-swap-now.cmd", "hotstream.log" };
    const fs::path cache = Paths::LegacyCacheDir();
    std::error_code ec;

    for (const char* name : s_names) {
        fs::remove(cache / name, ec);
    }

    fs::remove_all(cache / "blobs", ec);
}

// MPQs carry a sampled digest so checking them does not read gigabytes, everything else stays on full SHA-256.
static const std::string& ExpectedDigest(const ManifestFile& mf) {
    return mf.m_quick.empty() ? mf.m_sha256 : mf.m_quick;
}

static void RememberPlaced(const std::wstring& path, const ManifestFile& mf, const ManifestArchive* arc) {
    LocalDigests::Remember(path, LocalDigests::Kind::Sha256, mf.m_sha256);
    LocalDigests::Remember(path, LocalDigests::Kind::Quick, mf.m_quick);

    if (arc) {
        LocalDigests::Remember(path, LocalDigests::Kind::ContentId, arc->m_contentId);
    }
}

static bool LocalMatches(const std::wstring& path, const std::string& expected) {
    const LocalDigests::Kind kind = IsQuickDigest(expected) ? LocalDigests::Kind::Quick : LocalDigests::Kind::Sha256;
    std::string got;
    return LocalDigests::Get(path, kind, got) && _stricmp(got.c_str(), expected.c_str()) == 0;
}

static std::wstring ToBackslashes(std::wstring p) {
    for (wchar_t& c : p) {
        if (c == L'/') {
            c = L'\\';
        }
    }

    return p;
}

struct PendingMove {
    std::wstring m_src;
    std::wstring m_dst;
};

static std::mutex s_pendingMutex;
static std::vector<PendingMove> s_pendingMoves;
static std::vector<std::wstring> s_pendingDeletes;

static std::mutex s_plannedMutex;
// Lowered absolute paths this pass will replace.
static std::set<std::wstring> s_plannedUpdates;
static std::atomic<unsigned> s_updateGeneration{ 0 };

static std::wstring LocalPath(const std::string& relPath) {
#if defined(_WIN32)
    return (fs::path(s_installDir) / fs::path(relPath)).wstring();
#else
    const std::string joined = (fs::path(s_installDir) / HostRelPath(relPath)).string();
    char resolved[4096];

    if (!OsLinuxResolvePath(joined.c_str(), resolved, sizeof(resolved), true)) {
        return fs::path(joined).wstring();
    }

    return fs::path(resolved).wstring();
#endif
}

static std::string s_launcherExe = "HoTLauncher.exe";

static bool IsLauncherExe(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    return _stricmp(name.c_str(), s_launcherExe.c_str()) == 0;
}

// The launcher entry follows the exe the player actually runs, which settings.json records and may be a renamed copy.
static std::wstring TargetPath(const ManifestFile& mf) {
    if (!IsLauncherExe(mf.m_path)) {
        return LocalPath(mf.m_path);
    }

    std::wstring recorded = Text::WidenUtf8(LauncherSettings::LauncherPath());

    if (recorded.empty()) {
        return LocalPath(mf.m_path);
    }

    std::error_code ec;
    return fs::exists(recorded, ec) ? recorded : LocalPath(mf.m_path);
}

#if defined(AUTO_UPDATER_IGNORES_EXE) && defined(_WIN32)
static bool IsRunningExe(const std::wstring& path) {
    std::vector<wchar_t> buffer(MAX_PATH * 4);
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));

    if (length == 0 || length >= buffer.size()) {
        return false;
    }

    std::error_code ec;
    return fs::equivalent(fs::path(path), fs::path(std::wstring(buffer.data(), length)), ec);
}
#endif

static void WritePendingLocked() {
    std::error_code ec;
    fs::create_directories(PendingFile().parent_path(), ec);
    std::ofstream f(PendingFile(), std::ios::trunc | std::ios::binary);

    if (!f) {
        return;
    }

    bool lossy = false;

    for (const PendingMove& m : s_pendingMoves) {
        f << "M|" << NarrowOem(ToBackslashes(m.m_src), &lossy) << "|" << NarrowOem(ToBackslashes(m.m_dst), &lossy) << "\r\n";
    }

    for (const std::wstring& d : s_pendingDeletes) {
        f << "D|" << NarrowOem(ToBackslashes(d), &lossy) << "\r\n";
    }

#if defined(_WIN32)
    if (lossy) {
        StreamLog("stream: pending path not representable in OEM code page %u, exit swap may fail", GetOEMCP());
    }
#else
    (void)lossy;
#endif
}

#if defined(_WIN32)
static HANDLE s_swapHold = INVALID_HANDLE_VALUE;
static constexpr const char* s_swapHoldName = "hotstream-swap.hold";

static bool WriteSwapScript(const fs::path& cmdPath, const std::string& hold, const std::string& pend, bool spinFirst) {
    std::ofstream b(cmdPath, std::ios::trunc | std::ios::binary);

    if (!b) {
        return false;
    }

    b << "@echo off\r\n";
    b << "set tries=0\r\n";

    if (spinFirst) {
        b << "set spin=0\r\n";
        b << ":spin\r\n";
        b << "2>nul type nul >>\"" << hold << "\" && goto apply\r\n";
        b << "set /a spin+=1\r\n";
        b << "if %spin% lss 4000 goto spin\r\n";
    }

    b << ":wait\r\n";
    b << "2>nul type nul >>\"" << hold << "\" || (ping -n 2 127.0.0.1 >nul & goto wait)\r\n";
    b << ":apply\r\n";
    b << "if not exist \"" << pend << "\" goto done\r\n";
    b << "set failed=0\r\n";
    b << "for /f \"usebackq tokens=1,2,3 delims=|\" %%a in (\"" << pend << "\") do (\r\n";
    b << "  if \"%%a\"==\"M\" if exist \"%%b\" (move /y \"%%b\" \"%%c\" >nul 2>&1 || set failed=1)\r\n";
    b << "  if \"%%a\"==\"D\" if exist \"%%b\" (del /q \"%%b\" >nul 2>&1 || set failed=1)\r\n";
    b << ")\r\n";
    b << "if \"%failed%\"==\"1\" if %tries% lss 10 (set /a tries+=1 & ping -n 2 127.0.0.1 >nul & goto apply)\r\n";
    b << ":done\r\n";
    b << "del /q \"" << pend << "\" >nul 2>&1\r\n";
    b << "del /q \"" << hold << "\" >nul 2>&1\r\n";
    b << "del /q \"%~f0\" >nul 2>&1\r\n";
    return true;
}

static bool SpawnSwapScript(const fs::path& cmdPath) {
    std::wstring cmdline = L"cmd.exe /c \"" + cmdPath.wstring() + L"\"";
    std::vector<wchar_t> cl(cmdline.begin(), cmdline.end());
    cl.push_back(0);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};

    if (!CreateProcessW(nullptr, cl.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

static std::string HoldRef() {
    return std::string("%~dp0") + s_swapHoldName;
}

static std::string PendRef() {
    return std::string("%~dp0") + PendingFile().filename().string();
}

static void SpawnCloseSwapHelper() {
    static std::atomic<bool> s_spawned{ false };

    if (s_spawned.exchange(true)) {
        return;
    }

    const fs::path cacheDir = PendingFile().parent_path();
    fs::path cmdPath = cacheDir / "hotstream-swap.cmd";
    fs::path holdPath = cacheDir / s_swapHoldName;

    s_swapHold = CreateFileW(holdPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (s_swapHold == INVALID_HANDLE_VALUE) {
        StreamLog("stream: swap hold create FAILED (err %lu), relying on in-process swap at exit", GetLastError());
        return;
    }

    if (!WriteSwapScript(cmdPath, HoldRef(), PendRef(), false)) {
        return;
    }

    if (SpawnSwapScript(cmdPath)) {
        StreamLog("stream: spawned close-swap helper");
    } else {
        StreamLog("stream: close-swap helper spawn FAILED (err %lu)", GetLastError());
    }
}

static void SpawnExitSwapHelper() {
    static std::atomic<bool> s_spawned{ false };

    if (s_spawned.exchange(true)) {
        return;
    }

    if (s_swapHold == INVALID_HANDLE_VALUE) {
        return;
    }

    fs::path cmdPath = PendingFile().parent_path() / "hotstream-swap-now.cmd";

    if (!WriteSwapScript(cmdPath, HoldRef(), PendRef(), true)) {
        return;
    }

    if (SpawnSwapScript(cmdPath)) {
        StreamLog("stream: spawned exit swap helper");
    } else {
        StreamLog("stream: exit swap helper spawn FAILED (err %lu)", GetLastError());
    }
}
#else
// Nothing the client holds open blocks a rename on Linux, so whatever is still pending is retried in process by
// SwapOnClose and no helper has to outlive the client.
static void SpawnCloseSwapHelper() {
}

static void SpawnExitSwapHelper() {
}
#endif

static void RecordPending(const std::wstring& newPath, const std::wstring& target, bool needsRestart) {
    {
        std::lock_guard<std::mutex> lock(s_pendingMutex);

        for (auto it = s_pendingMoves.begin(); it != s_pendingMoves.end();) {
            if (!PathEqual(it->m_dst, target)) {
                it++;
                continue;
            }

            if (!PathEqual(it->m_src, newPath)) {
                StreamLog("stream: superseding staged %s", Text::NarrowAcp(it->m_src).c_str());
                s_pendingDeletes.push_back(it->m_src);
            }

            it = s_pendingMoves.erase(it);
        }

        s_pendingMoves.push_back(PendingMove{ newPath, target });
        WritePendingLocked();
    }

    if (needsRestart) {
        s_restartNeeded = true;
    }

    SpawnCloseSwapHelper();
}

static bool HasPendingMove(const std::wstring& src, const std::wstring& dst) {
    std::lock_guard<std::mutex> lock(s_pendingMutex);

    for (const PendingMove& m : s_pendingMoves) {
        if (PathEqual(m.m_src, src) && PathEqual(m.m_dst, dst)) {
            return true;
        }
    }

    return false;
}

static void CancelPendingMove(const std::wstring& src, const std::wstring& dst) {
    std::lock_guard<std::mutex> lock(s_pendingMutex);
    size_t before = s_pendingMoves.size();

    for (auto it = s_pendingMoves.begin(); it != s_pendingMoves.end();) {
        it = (PathEqual(it->m_src, src) && PathEqual(it->m_dst, dst)) ? s_pendingMoves.erase(it) : it + 1;
    }

    if (s_pendingMoves.size() != before) {
        WritePendingLocked();
    }
}

static void DropStaged(const std::wstring& path) {
    std::error_code ec;

    if (!fs::exists(path, ec)) {
        return;
    }

    bool queued = false;

    {
        std::lock_guard<std::mutex> lock(s_pendingMutex);

        for (auto it = s_pendingMoves.begin(); it != s_pendingMoves.end();) {
            it = PathEqual(it->m_src, path) ? s_pendingMoves.erase(it) : it + 1;
        }

        if (fs::remove(path, ec)) {
            StreamLog("stream: removed stale staged %s", Text::NarrowAcp(path).c_str());
        } else {
            bool known = false;

            for (const std::wstring& d : s_pendingDeletes) {
                known = known || PathEqual(d, path);
            }

            if (!known) {
                s_pendingDeletes.push_back(path);
            }

            queued = true;
        }

        WritePendingLocked();
    }

    if (queued) {
        StreamLog("stream: stale staged %s in use, deleting on exit", Text::NarrowAcp(path).c_str());
        SpawnCloseSwapHelper();
    }
}

static void SweepStagedSiblings(const ManifestFile& mf, const std::wstring& local, const std::wstring& keep) {
    fs::path lp(local);
    std::wstring prefix = lp.stem().wstring() + L".new-";
    std::wstring ext = lp.extension().wstring();
    const fs::path dirs[] = { fs::path(StagedPath(mf, local)).parent_path(), lp.parent_path() };

    std::error_code ec;
    std::vector<std::wstring> stale;

    for (const fs::path& dir : dirs) {
        for (const fs::directory_entry& e : fs::directory_iterator(dir, ec)) {
            if (!e.is_regular_file(ec)) {
                continue;
            }

            std::wstring name = e.path().filename().wstring();

            if (name.size() <= prefix.size() + ext.size()) {
                continue;
            }

            if (!StartsWithNoCase(name, prefix) || !EndsWithNoCase(name, ext)) {
                continue;
            }

            if (!PathEqual(e.path().wstring(), keep)) {
                stale.push_back(e.path().wstring());
            }
        }
    }

    for (const std::wstring& s : stale) {
        DropStaged(s);
    }
}

static void ApplyPending() {
    std::lock_guard<std::mutex> lock(s_pendingMutex);

    for (auto it = s_pendingMoves.begin(); it != s_pendingMoves.end();) {
        std::error_code ec;

        if (!fs::exists(it->m_src, ec) || MoveFileExW(it->m_src.c_str(), it->m_dst.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH | MOVEFILE_COPY_ALLOWED)) {
            it = s_pendingMoves.erase(it);
        } else {
            it++;
        }
    }

    for (auto it = s_pendingDeletes.begin(); it != s_pendingDeletes.end();) {
        std::error_code ec;

        if (!fs::exists(*it, ec) || fs::remove(*it, ec)) {
            it = s_pendingDeletes.erase(it);
        } else {
            it++;
        }
    }

    WritePendingLocked();
}

void SwapOnClose() {
    {
        std::lock_guard<std::mutex> lock(s_pendingMutex);

        if (s_pendingMoves.empty() && s_pendingDeletes.empty()) {
            return;
        }
    }

    ApplyPending();

    bool remaining;

    {
        std::lock_guard<std::mutex> lock(s_pendingMutex);
        remaining = !s_pendingMoves.empty() || !s_pendingDeletes.empty();
    }

    if (remaining) {
        SpawnExitSwapHelper();
    }
}

void OnCloseGame() {
    for (SArchive* archive : s_ownedMounts) {
        SFile::CloseArchive(archive);
    }

    s_ownedMounts.clear();
    SwapOnClose();
}

struct MountRequest {
    std::string m_path;
};

// Everything one pass produced for the main thread, applied only once the whole pass finished downloading.
struct PassBatch {
    std::vector<ArchivePatchJob> m_jobs;
    std::vector<MountRequest> m_mounts;
};

static std::mutex s_batchMutex;
static std::deque<PassBatch> s_batchQueue;
// Guarded by s_batchMutex.
static size_t s_frontJobsDone = 0;
// Background thread only, published at the end of a pass.
static PassBatch s_passBatch;
// Path plus hash, so a re-download of a path still remounts.
static std::set<std::wstring> s_mounted;

static void PublishBatch() {
    if (s_passBatch.m_jobs.empty() && s_passBatch.m_mounts.empty()) {
        return;
    }

    StreamLog("stream: publishing batch, %d in-place job(s), %d mount(s)", static_cast<int32_t>(s_passBatch.m_jobs.size()), static_cast<int32_t>(s_passBatch.m_mounts.size()));
    std::lock_guard<std::mutex> lock(s_batchMutex);
    s_batchQueue.push_back(std::move(s_passBatch));
    s_passBatch = PassBatch();
}

static std::mutex s_eventMutex;
static std::deque<std::function<void()>> s_eventQueue;

static void EnqueueEvent(std::function<void()> fn) {
    std::lock_guard<std::mutex> lock(s_eventMutex);
    s_eventQueue.push_back(std::move(fn));
}

static void EnqueueMount(const std::wstring& path, const std::string& sha) {
    std::string p = Text::NarrowAcp(path);

    if (!s_mounted.insert(Text::LowerPath(path) + L"|" + Widen(sha)).second) {
        StreamLog("stream: already mounted %s", p.c_str());
        return;
    }

    StreamLog("stream: queued mount %s", p.c_str());
    s_passBatch.m_mounts.push_back(MountRequest{ std::move(p) });
}

static int32_t RefreshUnitModel(WOWGUID guid, void* param) {
    CGObject_C* unit = ClntObjMgrObjectPtr(guid, static_cast<TypeMask>(TYPEMASK_UNIT | TYPEMASK_PLAYER), __FILE__, __LINE__);

    if (unit) {
        unit->UpdateDisplayInfo(1);
    }

    return 1;
}

// A mounted archive only changes what gets parsed from here on, so drop the cached models and rebuild what is in the world.
static void RefreshLoadedModels() {
    CM2Scene* scene = World::s_m2Scene;

    if (scene && scene->m_cache) {
        scene->m_cache->ClearSharedHash();
        scene->m_cache->GarbageCollect(1);
    }

    // Glue screens have no world objects, RefreshGlueModels handles them later.
    if (!ClntObjMgrGetActivePlayerObj()) {
        return;
    }

    ClntObjMgrEnumVisibleObjects(RefreshUnitModel, nullptr);
    StreamLog("stream: rebuilt world models after art mount");
}

// Only Lua knows which character a glue screen shows, so it re-creates it after the batch listeners ran.
static void RefreshGlueModels() {
    if (ClntObjMgrGetActivePlayerObj()) {
        return;
    }

    FrameScript_Execute("if StreamingRefreshGlueModels then StreamingRefreshGlueModels() end", "HoTArtRefresh", nullptr);
    StreamLog("stream: asked the glue UI to rebuild its model after art mount");
}

static bool DownloadWhole(const std::string& baseUrl, const ManifestFile& mf, const std::wstring& local, ByteCounter& counter) {
    std::error_code ec;
    fs::create_directories(fs::path(local).parent_path(), ec);
    std::wstring part = PartPath(mf);
    std::wstring meta = part + L".meta";
    fs::create_directories(fs::path(part).parent_path(), ec);

    if (FileSize(part) < 0) {
        MoveFileExW((local + L".part").c_str(), part.c_str(), 0);
        MoveFileExW((local + L".part.meta").c_str(), meta.c_str(), 0);
    }

    // A leftover .part resumes with a Range request only when the sidecar ties it to this same manifest entry.
    std::string expected = std::to_string(mf.m_size) + ":" + mf.m_sha256;
    long long resume = 0;
    long long ps = FileSize(part);

    if (ps > 0 && ps < mf.m_size && ReadSmallFile(meta) == expected) {
        resume = ps;
    } else if (ps >= 0) {
        fs::remove(part, ec);
    }

    WriteSmallFile(meta, expected);

    std::string url = baseUrl;

    if (!url.empty() && url.back() == '/') {
        url.pop_back();
    }

    url += "/" + UrlEncodePath(mf.m_path) + "?v=" + mf.m_sha256;

    std::string got;
    DownloadOptions opt;
    opt.m_resumeFrom = resume;
    opt.m_sha256Out = &got;
    opt.m_throttle = &s_throttle;
    opt.m_onBytes = [&counter](long long written) {
        counter.Report(written);
    };

    if (!HttpDownloadFile(Widen(url), part, opt)) {
        StreamLog("stream: download failed %s", mf.m_path.c_str());
        return false;
    }

    if (got.empty()) {
        got = Sha256File(part);
    }

    if (_stricmp(got.c_str(), mf.m_sha256.c_str()) != 0) {
        fs::remove(part, ec);
        fs::remove(meta, ec);
        StreamLog("stream: hash mismatch %s", mf.m_path.c_str());
        return false;
    }

    fs::remove(meta, ec);
    return true;
}

struct MountAfter {
    std::wstring m_path;
    std::string m_sha;
};

static std::optional<MountAfter> PlaceDownloaded(const ManifestFile& mf, const ManifestArchive* arc, const std::wstring& local) {
    std::error_code ec;
    std::wstring part = PartPath(mf);
    bool isMpq = IsMpq(mf.m_path);
    bool existed = fs::exists(local, ec);
    std::optional<MountAfter> mount;

    if (!existed) {
        fs::rename(part, local, ec);

        if (ec) {
            fs::copy_file(part, local, fs::copy_options::overwrite_existing, ec);
            fs::remove(part, ec);
        }

        // The SHA-256 already matched, so the expected digests can be recorded without reading the file back.
        RememberPlaced(local, mf, arc);

        if (isMpq) {
            mount = MountAfter{ local, mf.m_sha256 };
        }

        return mount;
    }

    // Swap now when unlocked, else stage a hash named copy, mount it and swap later.
    if (MoveFileExW(part.c_str(), local.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH | MOVEFILE_COPY_ALLOWED)) {
        RememberPlaced(local, mf, arc);

        if (isMpq) {
            mount = MountAfter{ local, mf.m_sha256 };
        }

        SweepStagedSiblings(mf, local, L"");
        return mount;
    }

    std::wstring np = StagedPath(mf, local);
    fs::create_directories(fs::path(np).parent_path(), ec);

    if (!MoveFileExW(part.c_str(), np.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        fs::remove(part, ec);
        StreamLog("stream: could not stage %s", mf.m_path.c_str());
        return mount;
    }

    if (isMpq) {
        mount = MountAfter{ np, mf.m_sha256 };
    }

    RecordPending(np, local, !isMpq);
    SweepStagedSiblings(mf, local, np);
    StreamLog("stream: staged %s (in use), will swap on next launch", mf.m_path.c_str());
    return mount;
}

static void SwapOrStage(const std::wstring& np, const std::wstring& local, const ManifestFile& mf, const ManifestArchive* arc) {
    const bool isMpq = IsMpq(mf.m_path);

    if (MoveFileExW(np.c_str(), local.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH | MOVEFILE_COPY_ALLOWED)) {
        RememberPlaced(local, mf, arc);

        if (isMpq) {
            EnqueueMount(local, mf.m_sha256);
        }

        SweepStagedSiblings(mf, local, L"");
    } else {
        if (isMpq) {
            EnqueueMount(np, mf.m_sha256);
        }

        RecordPending(np, local, !isMpq);
        SweepStagedSiblings(mf, local, np);
    }
}

// HD off parks the file as <name>.disabled like the launcher does, false means skip the file this pass.
static bool ReconcileHdFile(const std::wstring& local, bool hdPatch, bool& restored) {
    const std::wstring disabled = local + L".disabled";
    std::error_code ec;

    if (!hdPatch) {
        if (!fs::exists(local, ec)) {
            return false;
        }

        if (MoveFileExW(local.c_str(), disabled.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            StreamLog("stream: hd off, disabled %s", Text::NarrowAcp(local).c_str());
        } else if (!HasPendingMove(local, disabled)) {
            // Loaded by the client, park it on exit.
            RecordPending(local, disabled, true);
        }

        return false;
    }

    // Turned back on before the queued rename ran, so keep what is already on disk.
    CancelPendingMove(local, disabled);

    if (fs::exists(local, ec) || !fs::exists(disabled, ec)) {
        return true;
    }

    // When it cannot be brought back the caller falls through and downloads it again.
    if (!MoveFileExW(disabled.c_str(), local.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return true;
    }

    StreamLog("stream: hd on, restored %s", Text::NarrowAcp(local).c_str());
    restored = true;
    return true;
}

static void RetireSupersededArchives() {
    static const char* const s_retired[] = { "Data/patch-w.mpq", "Data/patch-x.mpq" };
    std::error_code ec;

    for (const char* rel : s_retired) {
        std::wstring local = LocalPath(rel);

        if (!fs::exists(local, ec)) {
            continue;
        }

        std::wstring disabled = local + L".disabled";

        if (MoveFileExW(local.c_str(), disabled.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            StreamLog("stream: retired %s, patch-5 replaces it", rel);
        } else if (!HasPendingMove(local, disabled)) {
            RecordPending(local, disabled, false);
            StreamLog("stream: %s is in use, retiring it on exit", rel);
        }
    }
}

static fs::path BlobCacheDir() {
    return Paths::BlobsDir();
}

static std::wstring BlobPath(const std::string& sha) {
    return (BlobCacheDir() / Widen(sha)).wstring();
}

static bool ReadBinaryFile(const std::wstring& path, std::string& out) {
    std::ifstream f(fs::path(path), std::ios::binary | std::ios::ate);

    if (!f) {
        return false;
    }

    std::streamsize size = f.tellg();

    if (size < 0) {
        return false;
    }

    out.resize(static_cast<size_t>(size));
    f.seekg(0, std::ios::beg);
    return size == 0 || static_cast<bool>(f.read(out.data(), size));
}

static bool WriteBinaryFileAtomic(const std::wstring& path, const std::string& data) {
    std::wstring tmp = path + L".tmp";

    {
        std::ofstream f(fs::path(tmp), std::ios::trunc | std::ios::binary);

        if (!f || !f.write(data.data(), static_cast<std::streamsize>(data.size()))) {
            return false;
        }
    }

    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

static void MarkPlaced(const std::wstring& absPath) {
    {
        std::lock_guard<std::mutex> lock(s_plannedMutex);
        s_plannedUpdates.erase(Text::LowerPath(absPath));
    }

    s_updateGeneration++;
}

static const ManifestArchive* ArchiveFor(const Manifest& man, const ManifestFile& mf) {
    for (const ManifestArchive& a : man.m_archives) {
        if (_stricmp(a.m_path.c_str(), mf.m_path.c_str()) == 0 && ArchiveManifest::IsContentId(a.m_contentId)) {
            return &a;
        }
    }

    return nullptr;
}

static std::string JoinUrl(const std::string& baseUrl, const std::string& rel) {
    std::string url = baseUrl;

    if (!url.empty() && url.back() == '/') {
        url.pop_back();
    }

    return url + "/" + rel;
}

static std::mutex s_failureMutex;
// Consecutive in-place failures per archive.
static std::map<std::wstring, int32_t> s_archiveFailures;
static std::map<std::wstring, std::string> s_inPlaceApplied;
static constexpr int32_t s_inPlaceGiveUp = 3;

static int32_t InPlaceFailures(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(s_failureMutex);
    auto it = s_archiveFailures.find(Text::LowerPath(path));
    return it == s_archiveFailures.end() ? 0 : it->second;
}

static void NoteInPlaceResult(const std::wstring& path, bool ok) {
    std::lock_guard<std::mutex> lock(s_failureMutex);

    if (ok) {
        s_archiveFailures.erase(Text::LowerPath(path));
    } else {
        s_archiveFailures[Text::LowerPath(path)]++;
    }
}

static void ExpectInPlace(const std::wstring& path, const std::string& contentId) {
    std::lock_guard<std::mutex> lock(s_failureMutex);
    s_inPlaceApplied[Text::LowerPath(path)] = contentId;
}

static void SettleInPlace(const std::wstring& path, const std::string& localId, const std::string& relPath) {
    std::string expected;

    {
        std::lock_guard<std::mutex> lock(s_failureMutex);
        auto it = s_inPlaceApplied.find(Text::LowerPath(path));

        if (it == s_inPlaceApplied.end()) {
            return;
        }

        expected = std::move(it->second);
        s_inPlaceApplied.erase(it);
    }

    const bool ok = (expected == localId);

    if (!ok) {
        StreamLog("stream: %s was patched in place but reads back as %.12s, not %.12s", relPath.c_str(), localId.c_str(), expected.c_str());
    }

    NoteInPlaceResult(path, ok);
}

static void ForgetInPlace(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(s_failureMutex);
    s_inPlaceApplied.erase(Text::LowerPath(path));
    s_archiveFailures.erase(Text::LowerPath(path));
}

enum class ArchivePlan {
    Current,
    InPlace,
    WholeFile,
    Skip,
};

// The whole file path is only taken for reasons that will not go away on their own.
static constexpr long long s_bloatFloor = 64LL << 20;

static ArchivePlan PlanArchive(const std::string& baseUrl, const ManifestFile& mf, const ManifestArchive& arc, const std::wstring& local, ArchivePatchJob& job) {
    if (!mf.m_quick.empty() && FileSize(local) == mf.m_size && LocalMatches(local, mf.m_quick)) {
        LocalDigests::Remember(local, LocalDigests::Kind::ContentId, arc.m_contentId);
        ForgetInPlace(local);
        return ArchivePlan::Current;
    }

    std::string localId;

    if (!LocalDigests::Get(local, LocalDigests::Kind::ContentId, localId)) {
        StreamLog("stream: %s unreadable, retrying next pass", mf.m_path.c_str());
        return ArchivePlan::Skip;
    }

    SettleInPlace(local, localId, mf.m_path);

    if (localId.empty()) {
        StreamLog("stream: %s has no (hotmanifest), whole-file update", mf.m_path.c_str());
        return ArchivePlan::WholeFile;
    }

    if (localId == arc.m_contentId) {
        return ArchivePlan::Current;
    }

    if (InPlaceFailures(local) >= s_inPlaceGiveUp) {
        StreamLog("stream: %s failed in place %d times, whole-file update", mf.m_path.c_str(), s_inPlaceGiveUp);
        return ArchivePlan::WholeFile;
    }

    std::string localBytes;
    std::string err;
    bool unreadable = false;

    if (!MpqFile::ReadFile(local, ArchiveManifest::s_fileName, localBytes, &err, &unreadable)) {
        if (unreadable) {
            StreamLog("stream: %s unreadable (%s), retrying next pass", mf.m_path.c_str(), err.c_str());
            return ArchivePlan::Skip;
        }

        StreamLog("stream: %s manifest unreadable (%s), whole-file update", mf.m_path.c_str(), err.c_str());
        return ArchivePlan::WholeFile;
    }

    if (ArchiveManifest::ContentIdOf(localBytes) == arc.m_contentId) {
        LocalDigests::Remember(local, LocalDigests::Kind::ContentId, arc.m_contentId);
        return ArchivePlan::Current;
    }

    std::vector<ArchiveEntry> localEntries;

    if (!ArchiveManifest::Parse(localBytes, localEntries, &err)) {
        StreamLog("stream: %s manifest corrupt (%s), whole-file update", mf.m_path.c_str(), err.c_str());
        return ArchivePlan::WholeFile;
    }

    std::string targetBytes;
    std::wstring indexUrl = Widen(JoinUrl(baseUrl, BlobContainer::RelativeIndexPath(arc.m_contentId)));

    if (!HttpGetString(indexUrl, targetBytes)) {
        StreamLog("stream: %s index fetch failed, retrying next pass", mf.m_path.c_str());
        return ArchivePlan::Skip;
    }

    if (ArchiveManifest::ContentIdOf(targetBytes) != arc.m_contentId) {
        StreamLog("stream: %s index does not hash to its contentId, retrying next pass", mf.m_path.c_str());
        return ArchivePlan::Skip;
    }

    std::vector<ArchiveEntry> targetEntries;

    if (!ArchiveManifest::Parse(targetBytes, targetEntries, &err)) {
        StreamLog("stream: %s index corrupt (%s), retrying next pass", mf.m_path.c_str(), err.c_str());
        return ArchivePlan::Skip;
    }

    ArchiveDiff diff = ArchiveManifest::Diff(localEntries, targetEntries);

    if (!diff.m_removed.empty()) {
        StreamLog("stream: %s drops %d file(s), whole-file update", mf.m_path.c_str(), static_cast<int32_t>(diff.m_removed.size()));
        return ArchivePlan::WholeFile;
    }

    long long targetRaw = 0;

    for (const ArchiveEntry& e : targetEntries) {
        targetRaw += e.m_size;
    }

    const long long onDisk = FileSize(local);
    const long long growth = targetRaw > 0 ? static_cast<long long>(static_cast<double>(diff.m_changedBytes) * static_cast<double>(mf.m_size) / static_cast<double>(targetRaw)) : diff.m_changedBytes;
    const long long allowed = mf.m_size + std::max(s_bloatFloor, mf.m_size / 4);

    if (onDisk + growth > allowed) {
        StreamLog("stream: %s would grow to %lld MB against %lld MB published, whole-file update to compact", mf.m_path.c_str(), (onDisk + growth) >> 20, mf.m_size >> 20);
        return ArchivePlan::WholeFile;
    }

    job = ArchivePatchJob();
    job.m_archivePath = local;
    job.m_relPath = mf.m_path;
    job.m_targetManifest = std::move(targetBytes);
    job.m_contentId = arc.m_contentId;

    for (ArchiveEntry& e : diff.m_changed) {
        ArchiveWrite w;
        w.m_name = std::move(e.m_name);
        w.m_size = e.m_size;
        w.m_sha256 = e.m_sha256;
        w.m_blobPath = BlobPath(w.m_sha256);
        job.m_writes.push_back(std::move(w));
    }

    StreamLog("stream: %s -> %.12s in place, %d file(s), %lld bytes", mf.m_path.c_str(), arc.m_contentId.c_str(), static_cast<int32_t>(job.m_writes.size()), diff.m_changedBytes);
    return ArchivePlan::InPlace;
}

// Leaves the verified raw bytes of one file at blobs/<sha>, downloading unless a valid copy is already there.
static bool FetchBlob(const std::string& baseUrl, const ArchiveWrite& w, ByteCounter& counter) {
    std::string existing;

    if (FileSize(w.m_blobPath) == w.m_size && ReadBinaryFile(w.m_blobPath, existing) && Sha256Hex(existing.data(), existing.size()) == w.m_sha256) {
        return true;
    }

    std::error_code ec;
    fs::create_directories(BlobCacheDir(), ec);
    std::wstring part = w.m_blobPath + L".part";
    fs::remove(part, ec);

    DownloadOptions opt;
    opt.m_throttle = &s_throttle;
    opt.m_onBytes = [&counter](long long written) {
        counter.Report(written);
    };

    std::wstring url = Widen(JoinUrl(baseUrl, BlobContainer::RelativeBlobPath(w.m_sha256)));

    if (!HttpDownloadFile(url, part, opt)) {
        fs::remove(part, ec);
        StreamLog("stream: blob download failed %s (%.12s)", w.m_name.c_str(), w.m_sha256.c_str());
        return false;
    }

    std::string container;
    std::string raw;
    std::string err;
    bool ok = ReadBinaryFile(part, container) && BlobContainer::Unpack(container, raw, &err);
    fs::remove(part, ec);

    if (!ok) {
        StreamLog("stream: blob unpack failed %s: %s", w.m_name.c_str(), err.c_str());
        return false;
    }

    if (static_cast<long long>(raw.size()) != w.m_size || Sha256Hex(raw.data(), raw.size()) != w.m_sha256) {
        StreamLog("stream: blob hash mismatch %s", w.m_name.c_str());
        return false;
    }

    if (!WriteBinaryFileAtomic(w.m_blobPath, raw)) {
        StreamLog("stream: cannot write blob cache %s", w.m_name.c_str());
        return false;
    }

    return true;
}

// Blobs left behind by a pass that never got applied, whatever the current plan does not want is dead weight.
static void SweepBlobCache(const std::set<std::string>& wanted) {
    std::error_code ec;

    for (const fs::directory_entry& e : fs::directory_iterator(BlobCacheDir(), ec)) {
        if (!e.is_regular_file(ec)) {
            continue;
        }

        std::string name = Text::NarrowAcp(e.path().filename().wstring());

        if (wanted.find(name) == wanted.end()) {
            fs::remove(e.path(), ec);
        }
    }
}

struct RefreshResult {
    bool m_spellDataChanged = false;
    bool m_achievementDataChanged = false;
    bool m_interfaceFiles = false;
    bool m_artFiles = false;
};

static bool EqualNoCase(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }

    return true;
}

static bool ContainsNoCase(const char* hay, size_t hayLen, const char* needle, size_t needleLen) {
    if (needleLen == 0 || hayLen < needleLen) {
        return false;
    }

    for (size_t i = 0; i + needleLen <= hayLen; i++) {
        if (EqualNoCase(hay + i, needle, needleLen)) {
            return true;
        }
    }

    return false;
}

// Flags interface and world art names, and reloads a DBFilesClient table so whichever archive now wins lands in memory.
static void RefreshName(const std::string& name, RefreshResult& result) {
    static const char s_prefix[] = "dbfilesclient\\";
    static const char s_ext[] = ".dbc";
    static const char s_interface[] = "interface";
    static const char* const s_artExts[] = { ".m2", ".mdx", ".skin", ".blp", ".anim" };
    const size_t prefLen = sizeof(s_prefix) - 1;
    const size_t extLen = sizeof(s_ext) - 1;
    const size_t ifaceLen = sizeof(s_interface) - 1;
    const char* p = name.data();
    const size_t len = name.size();

    if (!result.m_interfaceFiles && ContainsNoCase(p, len, s_interface, ifaceLen)) {
        result.m_interfaceFiles = true;
    }

    // UI art rides on the reload prompt, so only world art counts here.
    if (!result.m_artFiles && !(len >= ifaceLen && EqualNoCase(p, s_interface, ifaceLen))) {
        for (const char* ext : s_artExts) {
            size_t el = std::strlen(ext);

            if (len > el && EqualNoCase(p + len - el, ext, el)) {
                result.m_artFiles = true;
                break;
            }
        }
    }

    if (len <= prefLen + extLen || !EqualNoCase(p, s_prefix, prefLen) || !EqualNoCase(p + len - extLen, s_ext, extLen)) {
        return;
    }

    size_t stemEnd = len - extLen;
    size_t stemStart = stemEnd;

    while (stemStart > 0 && p[stemStart - 1] != '\\' && p[stemStart - 1] != '/') {
        stemStart--;
    }

    const char* stem = p + stemStart;
    size_t stemLen = stemEnd - stemStart;

    if (stemLen >= 5 && EqualNoCase(stem, "spell", 5)) {
        result.m_spellDataChanged = true;
    }

    // Achievement, Achievement_Category and Achievement_Criteria all feed the index.
    if (stemLen >= 11 && EqualNoCase(stem, "achievement", 11)) {
        result.m_achievementDataChanged = true;
    }

    if (DB::ReloadTable(name.c_str())) {
        StreamLog("dbc: reloaded %s from streamed mpq", name.c_str());
    } else {
        StreamLog("dbc: %s not reloaded", name.c_str());
    }
}

static bool ReadArchiveFile(SArchive* archive, const char* name, std::string& out) {
    void* data = nullptr;
    uint32_t size = 0;

    if (!SFile::ReadFile(archive, name, &data, &size, 0, 0, nullptr) || !data) {
        return false;
    }

    bool ok = size != 0 && size != 0xFFFFFFFFu;

    if (ok) {
        out.assign(static_cast<const char*>(data), size);
    }

    SFile::Unload(data);
    return ok;
}

static void SplitLines(const std::string& text, std::vector<std::string>& out) {
    const char* p = text.data();
    const char* end = p + text.size();

    while (p < end) {
        const char* nl = p;

        while (nl < end && *nl != '\r' && *nl != '\n') {
            nl++;
        }

        if (nl > p) {
            out.emplace_back(p, nl);
        }

        p = nl;

        while (p < end && (*p == '\r' || *p == '\n')) {
            p++;
        }
    }
}

// Every name a mounted archive holds, from its (hotmanifest) or failing that its (listfile).
static void CollectArchiveNames(SArchive* archive, std::vector<std::string>& out) {
    std::string bytes;

    if (ReadArchiveFile(archive, ArchiveManifest::s_fileName, bytes)) {
        std::vector<ArchiveEntry> entries;

        if (ArchiveManifest::Parse(bytes, entries)) {
            for (ArchiveEntry& e : entries) {
                out.push_back(std::move(e.m_name));
            }

            return;
        }
    }

    if (ReadArchiveFile(archive, "(listfile)", bytes)) {
        SplitLines(bytes, out);
    }
}

struct BatchProgress {
    size_t m_nextJob = 0;
    size_t m_nextMount = 0;
    std::set<std::string> m_seen;
    std::vector<std::string> m_names;
    bool m_anyApplied = false;
};

// Main thread only.
static BatchProgress s_batchProgress;
static bool s_achievementRebuildPending = false;

static void AddRefreshName(BatchProgress& bp, const std::string& name) {
    if (bp.m_seen.insert(Text::LowerAscii(name)).second) {
        bp.m_names.push_back(name);
    }
}

// The one refresh a batch gets, every changed name is re-read through the client's normal lookup.
static void FinishBatch(BatchProgress& bp) {
    if (!bp.m_anyApplied) {
        return;
    }

    SFile::RebuildHash();

    RefreshResult r;

    for (const std::string& name : bp.m_names) {
        RefreshName(name, r);
    }

    if (r.m_spellDataChanged && ClntObjMgrGetActivePlayerObj()) {
        EnqueueEvent([] {
            CGSpellBook::UpdateSpells(0, 0, 0);
        });
    }

    // Streamed achievement rows land in the tables but not in CGAchievementInfo's index.
    if (r.m_achievementDataChanged) {
        s_achievementRebuildPending = true;
    }

    if (r.m_interfaceFiles) {
        s_uiRefreshNeeded = true;
    }

    if (r.m_artFiles) {
        // Cached "no HD version" answers taken while the art was still downloading are wrong now.
        for (StreamListener listener : s_artListeners) {
            listener();
        }

        RefreshLoadedModels();
    }

    for (StreamListener listener : s_batchListeners) {
        listener();
    }

    if (r.m_artFiles) {
        RefreshGlueModels();
    }

    StreamLog("stream: batch applied, refreshed %d name(s)", static_cast<int32_t>(bp.m_names.size()));
}

// One item of the front batch per frame, then a single refresh once it is drained.
static void ProcessBatch() {
    PassBatch* batch = nullptr;

    {
        std::lock_guard<std::mutex> lock(s_batchMutex);

        if (s_batchQueue.empty()) {
            return;
        }

        batch = &s_batchQueue.front();
    }

    BatchProgress& bp = s_batchProgress;

    if (bp.m_nextJob < batch->m_jobs.size()) {
        const ArchivePatchJob& job = batch->m_jobs[bp.m_nextJob++];
        unsigned long long t0 = GetTickCount64();
        ArchivePatchResult r = ArchivePatcher::Apply(job);
        StreamLog("stream: in-place %s %s: %d/%d file(s) written in %llu ms%s%s", job.m_relPath.c_str(), r.m_ok ? "OK" : "FAILED", static_cast<int32_t>(r.m_writtenNames.size()), static_cast<int32_t>(job.m_writes.size()), GetTickCount64() - t0, r.m_error.empty() ? "" : ", ", r.m_error.c_str());

        for (const std::string& n : r.m_writtenNames) {
            AddRefreshName(bp, n);
        }

        if (!r.m_writtenNames.empty()) {
            bp.m_anyApplied = true;
        }

        if (r.m_ok) {
            ExpectInPlace(job.m_archivePath, job.m_contentId);
        } else {
            NoteInPlaceResult(job.m_archivePath, false);
        }

        MarkPlaced(job.m_archivePath);
        std::lock_guard<std::mutex> lock(s_batchMutex);
        s_frontJobsDone = bp.m_nextJob;
        return;
    }

    if (bp.m_nextMount < batch->m_mounts.size()) {
        const MountRequest& req = batch->m_mounts[bp.m_nextMount++];
        int32_t priority = s_nextMountPriority.fetch_add(1);
        SArchive* archive = nullptr;
        bool ok = SFile::OpenArchive(req.m_path.c_str(), priority, s_mountFlags, &archive) != 0 && archive;
        StreamLog("stream: mount %s (prio %d) -> %s", ok ? "OK" : "FAILED", priority, req.m_path.c_str());

        if (ok) {
            s_ownedMounts.push_back(archive);
            std::vector<std::string> names;
            CollectArchiveNames(archive, names);

            for (const std::string& n : names) {
                AddRefreshName(bp, n);
            }

            bp.m_anyApplied = true;
        }

        return;
    }

    FinishBatch(bp);
    bp = BatchProgress();
    std::lock_guard<std::mutex> lock(s_batchMutex);
    s_frontJobsDone = 0;
    s_batchQueue.pop_front();
}

struct ActiveScope {
    ~ActiveScope() {
        s_active = false;
        std::lock_guard<std::mutex> lock(s_statusMutex);
        s_currentFile.clear();
    }
};

#if defined(_WIN32)
static HANDLE s_patchLock = nullptr;

static bool AcquirePatchLock() {
    if (s_patchLock) {
        return true;
    }

    // FNV-1a over the lowercased install path.
    uint64_t h = 1469598103934665603ULL;

    for (wchar_t c : s_installDir) {
        if (c >= L'A' && c <= L'Z') {
            c = static_cast<wchar_t>(c + 32);
        }

        h = (h ^ static_cast<uint32_t>(c)) * 1099511628211ULL;
    }

    wchar_t name[64];
    swprintf(name, 64, L"Local\\HoTStreamPatcher_%016llx", static_cast<unsigned long long>(h));

    HANDLE handle = CreateMutexW(nullptr, TRUE, name);

    if (!handle) {
        return true;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(handle);
        return false;
    }

    s_patchLock = handle;
    return true;
}
#else
static int32_t s_patchLock = -1;

// An flock on a file inside the install stands in for the install-path named mutex, held for the process lifetime.
static bool AcquirePatchLock() {
    if (s_patchLock >= 0) {
        return true;
    }

    std::error_code ec;
    fs::create_directories(Paths::WorkDir(), ec);
    const std::string path = (Paths::WorkDir() / "hotstream-patcher.lock").string();
    int32_t fd = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);

    if (fd < 0) {
        return true;
    }

    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        const bool held = errno == EWOULDBLOCK;
        close(fd);
        return !held;
    }

    s_patchLock = fd;
    return true;
}
#endif

struct FirstPassScope {
    ~FirstPassScope() {
        s_firstPassDone = true;
    }
};

struct BatchScope {
    ~BatchScope() {
        PublishBatch();
    }
};

struct PlannedItem {
    const ManifestFile* m_file = nullptr;
    bool m_inPlace = false;
    ArchivePatchJob m_job;
    std::wstring m_local;
    size_t m_remaining = 0;
    bool m_failed = false;
};

struct DownloadWork {
    bool m_blob = false;
    size_t m_item = 0;
    ArchiveWrite m_write;
    std::vector<size_t> m_refs;
    long long m_size = 0;
    std::string m_label;
};

struct WorkPool {
    std::atomic<size_t> m_next{ 0 };
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<std::pair<size_t, bool>> m_done;
};

static void Run() {
    // Login stays gated until it is known what needs patching.
    FirstPassScope firstPassScope;
    s_installDir = Paths::ExeDir().wstring();

    if (!AcquirePatchLock()) {
        StreamLog("stream: another instance is patching this install, skipping pass");
        return;
    }

    StreamLog("stream: pass start");

    LauncherConfig cfg;
    LoadLauncherConfig(s_installDir, cfg);
    std::wstring manifestUrl = cfg.m_manifestUrl;
    std::string baseUrl = cfg.m_patchBaseUrl;
    s_launcherExe = cfg.m_launcherExe;
    const bool hdPatch = LauncherSettings::HdPatch();

    // Busts the CDN cache so the newest manifest is read, the signature covers the body and not the URL.
    std::wstring stamp = std::to_wstring(static_cast<long long>(std::time(nullptr)));
    std::wstring manifestReq = AppendQuery(manifestUrl, L"t", stamp);

    std::string json;

    if (!HttpGetString(manifestReq, json)) {
        StreamLog("stream: manifest fetch failed");
        return;
    }

    // Checked before parsing, so a deploy landing between the two fetches fails closed until the next pass.
    std::wstring sigReq = AppendQuery(manifestUrl + Widen(s_signatureSuffix), L"t", stamp);

    std::string signature;

    if (!HttpGetString(sigReq, signature)) {
        StreamLog("stream: manifest signature fetch failed");
        return;
    }

    if (!VerifyManifestSignature(json, signature)) {
        StreamLog("stream: manifest signature REJECTED, ignoring this manifest");
        return;
    }

    Manifest man;

    if (!ParseManifest(json, man)) {
        StreamLog("stream: manifest parse failed");
        return;
    }

    if (!man.m_baseUrl.empty()) {
        baseUrl = man.m_baseUrl;
    }

    static std::atomic<bool> s_pendingReset{ false };

    if (!s_pendingReset.exchange(true)) {
        std::error_code ec;
        fs::create_directories(PendingFile().parent_path(), ec);
        std::ofstream(PendingFile(), std::ios::trunc | std::ios::binary);
        RetireLegacyCacheFiles();
    }

    RetireSupersededArchives();

    // Whatever this pass queues for the main thread goes out as one batch when the pass ends, however it ends.
    BatchScope batchScope;

    std::vector<PlannedItem> plan;
    std::set<std::string> wantedBlobs;

    for (const ManifestFile& mf : man.m_files) {
        std::wstring local = TargetPath(mf);

#ifdef AUTO_UPDATER_IGNORES_EXE
        if (IsRunningExe(local) || IsDiscordDll(mf.m_path)) {
            StreamLog("stream: AUTO_UPDATER_IGNORES_EXE set, ignoring %s", mf.m_path.c_str());
            continue;
        }
#endif

#if !defined(_WIN32)
        if (IsWindowsBinary(mf.m_path)) {
            continue;
        }
#endif

        bool restored = false;

        if (mf.m_hd && !ReconcileHdFile(local, hdPatch, restored)) {
            continue;
        }

        std::wstring np = StagedPath(mf, local);

        // An archive that describes itself is judged by its contents and brought up to date file by file where possible.
        const ManifestArchive* arc = IsMpq(mf.m_path) ? ArchiveFor(man, mf) : nullptr;

        if (arc && FileSize(local) >= 0 && FileSize(np) != mf.m_size) {
            PlannedItem item;
            item.m_file = &mf;
            item.m_inPlace = true;
            item.m_local = local;
            ArchivePlan how = PlanArchive(baseUrl, mf, *arc, local, item.m_job);

            if (how == ArchivePlan::Current) {
                // Restored from .disabled, so the client never loaded it at startup.
                if (restored) {
                    EnqueueMount(local, arc->m_contentId);
                }

                SweepStagedSiblings(mf, local, L"");
                continue;
            }

            if (how == ArchivePlan::Skip) {
                continue;
            }

            if (how == ArchivePlan::InPlace) {
                for (const ArchiveWrite& w : item.m_job.m_writes) {
                    wantedBlobs.insert(w.m_sha256);
                }

                plan.push_back(std::move(item));
                continue;
            }
        }

        if (FileSize(local) == mf.m_size && LocalMatches(local, ExpectedDigest(mf))) {
            if (restored && IsMpq(mf.m_path)) {
                EnqueueMount(local, mf.m_sha256);
            }

            // Nothing left to swap in.
            SweepStagedSiblings(mf, local, L"");
            continue;
        }

        if (FileSize(np) == mf.m_size) {
            SwapOrStage(np, local, mf, arc);
            continue;
        }

        PlannedItem item;
        item.m_file = &mf;
        item.m_local = local;
        plan.push_back(std::move(item));
    }

    LocalDigests::Save();
    SweepBlobCache(wantedBlobs);

    // Publishes what this pass will replace, plus earlier in-place jobs that have not landed, so the sanity check skips them.
    {
        std::set<std::wstring> planned;

        for (const PlannedItem& item : plan) {
            planned.insert(Text::LowerPath(item.m_local));
        }

        {
            std::lock_guard<std::mutex> lock(s_batchMutex);

            for (size_t b = 0; b < s_batchQueue.size(); b++) {
                const PassBatch& batch = s_batchQueue[b];

                for (size_t j = (b == 0) ? s_frontJobsDone : 0; j < batch.m_jobs.size(); j++) {
                    planned.insert(Text::LowerPath(batch.m_jobs[j].m_archivePath));
                }
            }
        }

        std::lock_guard<std::mutex> lock(s_plannedMutex);

        if (planned != s_plannedUpdates) {
            s_plannedUpdates.swap(planned);
            s_updateGeneration++;
        }
    }

    if (plan.empty()) {
        ResetProgress();
        StreamLog("stream: nothing to download");
        return;
    }

    std::vector<DownloadWork> tasks;
    std::map<std::string, size_t> blobTask;
    std::vector<size_t> wholeItems;

    for (size_t i = 0; i < plan.size(); i++) {
        PlannedItem& item = plan[i];

        if (!item.m_inPlace) {
            wholeItems.push_back(i);
            continue;
        }

        for (const ArchiveWrite& w : item.m_job.m_writes) {
            auto it = blobTask.find(w.m_sha256);

            if (it == blobTask.end()) {
                DownloadWork t;
                t.m_blob = true;
                t.m_write = w;
                t.m_size = w.m_size;
                t.m_label = item.m_file->m_path + " " + w.m_name;
                it = blobTask.emplace(w.m_sha256, tasks.size()).first;
                tasks.push_back(std::move(t));
            }

            tasks[it->second].m_refs.push_back(i);
            item.m_remaining++;
        }
    }

    std::stable_sort(wholeItems.begin(), wholeItems.end(), [&](size_t a, size_t b) {
        return plan[a].m_file->m_size < plan[b].m_file->m_size;
    });

    for (size_t i : wholeItems) {
        DownloadWork t;
        t.m_item = i;
        t.m_size = plan[i].m_file->m_size;
        t.m_label = plan[i].m_file->m_path;
        tasks.push_back(std::move(t));
    }

    long long total = 0;

    for (const DownloadWork& t : tasks) {
        total += t.m_size;
    }

    unsigned long long nowMs = GetTickCount64();
    unsigned long long lastMs = s_lastProgressMs.load();

    if (lastMs == 0 || nowMs - lastMs > s_progressJoinMs) {
        ResetProgress();
    }

    s_filesTotal = s_filesTotal.load() + static_cast<int32_t>(plan.size());
    s_totalBytes = s_totalBytes.load() + total;

    StreamLog("stream: downloading %d file(s) as %d request(s), %lld bytes", static_cast<int32_t>(plan.size()), static_cast<int32_t>(tasks.size()), total);
    s_active = true;
    ActiveScope activeScope;

    auto finishItem = [](const ManifestFile& mf) {
        s_filesDone = s_filesDone.load() + 1;
        s_lastProgressMs = GetTickCount64();
        StreamLog("stream: finished %d/%d %s", s_filesDone.load(), s_filesTotal.load(), mf.m_path.c_str());
    };

    for (PlannedItem& item : plan) {
        if (item.m_inPlace && item.m_remaining == 0) {
            finishItem(*item.m_file);
        }
    }

    WorkPool pool;

    auto execute = [&](const DownloadWork& t) {
        SetCurrentFile(t.m_label);
        ByteCounter counter;
        counter.m_size = t.m_size;
        bool ok = t.m_blob ? FetchBlob(baseUrl, t.m_write, counter) : DownloadWhole(baseUrl, *plan[t.m_item].m_file, plan[t.m_item].m_local, counter);
        counter.Settle();
        return ok;
    };

    std::vector<std::thread> workers;
    const size_t workerCount = std::min<size_t>(s_downloadWorkers, tasks.size());

    for (size_t w = 0; w < workerCount; w++) {
        workers.emplace_back([&] {
#if defined(_WIN32)
            SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
#else
            LowerThreadPriority();
#endif

            for (;;) {
                size_t t = pool.m_next.fetch_add(1);

                if (t >= tasks.size()) {
                    break;
                }

                bool ok = execute(tasks[t]);

                {
                    std::lock_guard<std::mutex> lock(pool.m_mutex);
                    pool.m_done.push_back({ t, ok });
                }

                pool.m_cv.notify_one();
            }
        });
    }

    std::vector<std::optional<MountAfter>> mounts(plan.size());

    for (size_t completed = 0; completed < tasks.size(); completed++) {
        std::pair<size_t, bool> r;

        {
            std::unique_lock<std::mutex> lock(pool.m_mutex);
            pool.m_cv.wait(lock, [&] {
                return !pool.m_done.empty();
            });
            r = pool.m_done.front();
            pool.m_done.pop_front();
        }

        const DownloadWork& t = tasks[r.first];

        if (t.m_blob) {
            for (size_t i : t.m_refs) {
                PlannedItem& item = plan[i];
                item.m_failed = item.m_failed || !r.second;

                if (--item.m_remaining == 0) {
                    finishItem(*item.m_file);
                }
            }

            continue;
        }

        PlannedItem& item = plan[t.m_item];

        if (r.second) {
            mounts[t.m_item] = PlaceDownloaded(*item.m_file, ArchiveFor(man, *item.m_file), item.m_local);
        }

        MarkPlaced(item.m_local);
        finishItem(*item.m_file);
    }

    for (std::thread& w : workers) {
        w.join();
    }

    for (size_t i = 0; i < plan.size(); i++) {
        PlannedItem& item = plan[i];

        if (item.m_inPlace) {
            if (!item.m_failed) {
                s_passBatch.m_jobs.push_back(std::move(item.m_job));
            } else {
                StreamLog("stream: %s in-place download incomplete, retrying next pass", item.m_file->m_path.c_str());
                MarkPlaced(item.m_local);
            }
        } else if (mounts[i]) {
            EnqueueMount(mounts[i]->m_path, mounts[i]->m_sha);
        }
    }

    LocalDigests::Save();
    StreamLog("stream: background download pass complete");
}

bool IsUpdatePending(const std::wstring& absPath) {
    const std::wstring key = Text::LowerPath(absPath);

    {
        std::lock_guard<std::mutex> lock(s_plannedMutex);

        if (s_plannedUpdates.find(key) != s_plannedUpdates.end()) {
            return true;
        }
    }

    std::lock_guard<std::mutex> lock(s_pendingMutex);

    for (const PendingMove& m : s_pendingMoves) {
        if (Text::LowerPath(m.m_dst) == key) {
            return true;
        }
    }

    return false;
}

unsigned UpdateGeneration() {
    return s_updateGeneration.load();
}

bool IsStagedName(const std::wstring& fileName) {
    // StagedPath puts the tag before the last extension, or at the end when there is none.
    const std::wstring stem = fs::path(fileName).stem().wstring();
    return HasStagedTag(stem) || HasStagedTag(fileName);
}

void OnArtStreamed(StreamListener listener) {
    s_artListeners.push_back(listener);
}

void OnBatchApplied(StreamListener listener) {
    s_batchListeners.push_back(listener);
}

void Start() {
    if (s_started.exchange(true)) {
        return;
    }

    // Kicked from the next idle, so every module has initialized before the first pass reads its settings.
    s_kickPending = true;
}

void StartPolling() {
    if (s_pollerStarted.exchange(true)) {
        return;
    }

    std::thread([] {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::milliseconds(s_pollIntervalMs));
            StreamLog("stream: periodic update check");
            Trigger();
        }
    }).detach();
}

void Trigger() {
    // The downloader is turned off, there is nothing to check against.
    if (!s_started.load()) {
        return;
    }

    bool expected = false;

    if (!s_running.compare_exchange_strong(expected, true)) {
        return;
    }

    std::thread([] {
#if defined(_WIN32)
        SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
#else
        LowerThreadPriority();
#endif
        Run();
        s_running = false;
    }).detach();
}

bool IsActive() {
    return s_active.load();
}

bool IsBusy() {
    if (s_active.load()) {
        return true;
    }

    return s_started.load() && !s_firstPassDone.load();
}

bool IsRestartPending() {
    return s_restartNeeded.load();
}

void PumpMainThread() {
    // The active state change is queued instead of signalled inline.
    static bool s_prevActive = false;
    bool active = s_active.load();

    if (active != s_prevActive) {
        s_prevActive = active;
        const char* eventName = active ? "HOT_STREAMING_STARTED" : "HOT_STREAMING_STOPPED";

        EnqueueEvent([eventName] {
            Lua::SignalEvent(eventName, nullptr);
        });
    }

    ProcessBatch();

    bool rebuild = s_achievementRebuildPending;
    s_achievementRebuildPending = false;

    if (rebuild && ClntObjMgrGetActivePlayerObj()) {
        CGAchievementInfo::ShutdownGame();
        CGAchievementInfo::InitializeGame();
        Net::Packet(Net::CMSG_ACHIEVEMENT_DATA_REQUEST).Send();
        StreamLog("stream: rebuilt achievement index, requested achievement data");
    }

    // Exactly one queued event per update.
    std::function<void()> event;

    {
        std::lock_guard<std::mutex> lock(s_eventMutex);

        if (s_eventQueue.empty()) {
            return;
        }

        event = std::move(s_eventQueue.front());
        s_eventQueue.pop_front();
    }

    event();
}

static int32_t Script_IsStreaming(lua_State* L) {
    lua_pushboolean(L, s_active.load());
    return 1;
}

static int32_t Script_IsStreamingBusy(lua_State* L) {
    lua_pushboolean(L, IsBusy());
    return 1;
}

static int32_t Script_GetStreamingProgress(lua_State* L) {
    std::string current;

    {
        std::lock_guard<std::mutex> lock(s_statusMutex);
        current = s_currentFile;
    }

    lua_pushnumber(L, static_cast<double>(s_filesDone.load()));
    lua_pushnumber(L, static_cast<double>(s_filesTotal.load()));
    lua_pushnumber(L, static_cast<double>(s_doneBytes.load()));
    lua_pushnumber(L, static_cast<double>(s_totalBytes.load()));
    lua_pushstring(L, current.c_str());
    return 5;
}

static int32_t Script_IsStreamingRestartPending(lua_State* L) {
    lua_pushboolean(L, IsRestartPending());
    return 1;
}

static int32_t Script_IsStreamingUIRefreshPending(lua_State* L) {
    lua_pushboolean(L, s_uiRefreshNeeded.load());
    return 1;
}

static int32_t Script_StreamingRefreshUI(lua_State* L) {
    s_uiRefreshNeeded = false;

    if (ClntObjMgrGetActivePlayerObj()) {
        CGGameUI::m_reloadUIRequested = 1;
    } else {
        CGlueMgr::m_reload = 1;
    }

    return 0;
}

static int32_t Packet_SMSG_BACKGROUND_DOWNLOAD(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Trigger();
    return 1;
}

static bool StartupDownloaderEnabled() {
    static CVar* s_cvar = nullptr;

    if (!s_cvar) {
        s_cvar = CVar::Lookup("enableStartupDownloader");
    }

    // The DLL default is on, so a missing CVar counts as on.
    if (!s_cvar || !s_cvar->m_stringValue.m_str) {
        return true;
    }

    return CVars::Bool(s_cvar->m_stringValue.m_str);
}

class StreamingModule : public Module {
    public:
    const char* Name() const override {
        return "Streaming";
    }

    void OnInitializeGlobal() override {
        Lua::RegisterFunction("IsStreaming", Script_IsStreaming, Lua::State::Any);
        Lua::RegisterFunction("IsStreamingBusy", Script_IsStreamingBusy, Lua::State::Any);
        Lua::RegisterFunction("GetStreamingProgress", Script_GetStreamingProgress, Lua::State::Any);
        Lua::RegisterFunction("IsStreamingRestartPending", Script_IsStreamingRestartPending, Lua::State::Any);
        Lua::RegisterFunction("IsStreamingUIRefreshPending", Script_IsStreamingUIRefreshPending, Lua::State::Any);
        Lua::RegisterFunction("StreamingRefreshUI", Script_StreamingRefreshUI, Lua::State::Any);

        Net::RegisterWorldHandler(Net::SMSG_BACKGROUND_DOWNLOAD, Packet_SMSG_BACKGROUND_DOWNLOAD);

        MpqScanner::RegisterScripts();
    }

    void OnInitializeClient() override {
        if (StartupDownloaderEnabled()) {
            Start();
        }

        MpqScanner::Start();
    }

    void OnIdle() override {
        if (!s_started.load() && StartupDownloaderEnabled()) {
            Start();
        }

        if (s_kickPending.exchange(false)) {
            Trigger();
            StartPolling();
        }

        PumpMainThread();
        MpqScanner::Pump();
    }
};

Module* GetModule() {
    static StreamingModule module;
    return &module;
}

}
