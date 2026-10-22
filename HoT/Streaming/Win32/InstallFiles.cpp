#include "HoT/Streaming/InstallFiles.h"
#include <atomic>
#include <cstdio>
#include <fstream>
#include <vector>
#include "HoT/Streaming/StreamLog.h"
#include <windows.h>

namespace fs = std::filesystem;

namespace HoT::Streaming::InstallFiles {

bool Move(const std::wstring& src, const std::wstring& dst, uint32_t flags, bool& replacedRunningExe) {
    replacedRunningExe = false;
    return MoveFileExW(src.c_str(), dst.c_str(), flags) != 0;
}

std::string NarrowPendingPath(const std::wstring& s, bool* lossy) {
    if (s.empty()) {
        return "";
    }

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
}

uint32_t PendingCodePage() {
    return GetOEMCP();
}

static HANDLE s_patchLock = nullptr;

bool AcquirePatchLock(const std::wstring& installDir) {
    if (s_patchLock) {
        return true;
    }

    // FNV-1a over the lowercased install path.
    uint64_t h = 1469598103934665603ULL;

    for (wchar_t c : installDir) {
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

static std::string PendRef(const fs::path& pendingFile) {
    return std::string("%~dp0") + pendingFile.filename().string();
}

void SpawnCloseSwapHelper(const fs::path& pendingFile) {
    static std::atomic<bool> s_spawned{ false };

    if (s_spawned.exchange(true)) {
        return;
    }

    const fs::path cacheDir = pendingFile.parent_path();
    fs::path cmdPath = cacheDir / "hotstream-swap.cmd";
    fs::path holdPath = cacheDir / s_swapHoldName;

    s_swapHold = CreateFileW(holdPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (s_swapHold == INVALID_HANDLE_VALUE) {
        StreamLog("stream: swap hold create FAILED (err %lu), relying on in-process swap at exit", GetLastError());
        return;
    }

    if (!WriteSwapScript(cmdPath, HoldRef(), PendRef(pendingFile), false)) {
        return;
    }

    if (SpawnSwapScript(cmdPath)) {
        StreamLog("stream: spawned close-swap helper");
    } else {
        StreamLog("stream: close-swap helper spawn FAILED (err %lu)", GetLastError());
    }
}

void SpawnExitSwapHelper(const fs::path& pendingFile) {
    static std::atomic<bool> s_spawned{ false };

    if (s_spawned.exchange(true)) {
        return;
    }

    if (s_swapHold == INVALID_HANDLE_VALUE) {
        return;
    }

    fs::path cmdPath = pendingFile.parent_path() / "hotstream-swap-now.cmd";

    if (!WriteSwapScript(cmdPath, HoldRef(), PendRef(pendingFile), true)) {
        return;
    }

    if (SpawnSwapScript(cmdPath)) {
        StreamLog("stream: spawned exit swap helper");
    } else {
        StreamLog("stream: exit swap helper spawn FAILED (err %lu)", GetLastError());
    }
}

}
