#include "HoT/Discord/Host.h"
#include <string>
#include <windows.h>
#include <tlhelp32.h>

namespace HoT::Discord::Host {

void* LoadCreateFunction() {
    static HMODULE s_module = nullptr;

    if (!s_module) {
        s_module = LoadLibraryA("discord_game_sdk_64.dll");
    }

    return s_module ? reinterpret_cast<void*>(GetProcAddress(s_module, "DiscordCreate")) : nullptr;
}

bool IsDiscordRunning() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snap == INVALID_HANDLE_VALUE) {
        return false;
    }

    static const wchar_t* PROCESS_NAMES[] = { L"Discord", L"DiscordPTB", L"DiscordCanary", L"DiscordDevelopment" };

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;

    if (Process32FirstW(snap, &entry)) {
        do {
            std::wstring name = entry.szExeFile;

            if (name.size() > 4 && _wcsicmp(name.c_str() + name.size() - 4, L".exe") == 0) {
                name.resize(name.size() - 4);
            }

            for (const wchar_t* candidate : PROCESS_NAMES) {
                if (_wcsicmp(name.c_str(), candidate) == 0) {
                    found = true;
                    break;
                }
            }
        } while (!found && Process32NextW(snap, &entry));
    }

    CloseHandle(snap);

    return found;
}

}
