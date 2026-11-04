#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cwchar>

#pragma warning(disable : 4073)
#pragma init_seg(lib)

// Each unique error is reported once and the game keeps running. An llvm-symbolizer found on PATH hangs on modules
// without a PDB, so only dbghelp symbolizes.
static const wchar_t s_options[] = L"continue_on_error=2:external_symbolizer_path=";

// ASan reads its flags before any of the exe's code runs, so the exe sets ASAN_OPTIONS and starts itself again.
static void Relaunch(const wchar_t* current) {
    static wchar_t options[8192];
    swprintf_s(options, L"%s%s%s", s_options, *current ? L":" : L"", current);
    SetEnvironmentVariableW(L"ASAN_OPTIONS", options);

    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);

    static wchar_t commandLine[32768];
    wcscpy_s(commandLine, GetCommandLineW());

    STARTUPINFOW startup;
    GetStartupInfoW(&startup);
    PROCESS_INFORMATION process;

    if (!CreateProcessW(path, commandLine, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
        return;
    }

    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);

    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    ExitProcess(exitCode);
}

// A GUI process has no stderr, so ASan output goes to asan.log next to the exe.
static void RedirectStderr() {
    char path[MAX_PATH + 16];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* slash = strrchr(path, '\\');

    if (!slash) {
        return;
    }

    strcpy_s(slash + 1, sizeof(path) - (slash + 1 - path), "asan.log");
    FILE* file;

    if (!freopen_s(&file, path, "w", stderr)) {
        setvbuf(stderr, nullptr, _IONBF, 0);
    }
}

static struct AsanSetup {
    AsanSetup() {
        wchar_t current[4096] = {};
        GetEnvironmentVariableW(L"ASAN_OPTIONS", current, sizeof(current) / sizeof(current[0]));

        if (!wcsstr(current, L"external_symbolizer_path")) {
            Relaunch(current);
        }

        RedirectStderr();
    }
} s_asanSetup;
