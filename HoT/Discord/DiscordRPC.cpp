#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#if defined(_WIN32)
#include <windows.h>
#include <tlhelp32.h>
#else
#include <cctype>
#include <filesystem>
#include <fstream>
#include <strings.h>
#endif
#include "HoT/Discord/DiscordRPC.h"
#include "HoT/Discord/sdk/discord.h"

namespace HoT::Discord::RPC {

static const discord::ClientId CLIENT_ID = 1420551644671901780;
static const auto DETECT_INTERVAL = std::chrono::seconds(15);
static const auto IDLE_SLEEP = std::chrono::milliseconds(500);
static const auto CONNECTED_SLEEP = std::chrono::milliseconds(100);

static bool IsWine() {
    return true;
#if defined(_WIN32)
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");

    return ntdll && GetProcAddress(ntdll, "wine_get_version") != nullptr;
#endif
}

// Core::Create with default flags relaunches the game through Discord when it is not running, so look for it first.
static bool IsDiscordRunning() {
#if defined(_WIN32)
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
#else
    // /proc/<pid>/comm stands in for the process snapshot; the kernel cuts it to 15 characters, so compare that much.
    static const char* PROCESS_NAMES[] = { "Discord", "DiscordPTB", "DiscordCanary", "DiscordDevelopment" };
    static const size_t COMM_LENGTH = 15;

    std::error_code ec;

    for (std::filesystem::directory_iterator it("/proc", ec), end; !ec && it != end; it.increment(ec)) {
        const std::string pid = it->path().filename().string();

        if (pid.empty() || !isdigit(static_cast<unsigned char>(pid[0]))) {
            continue;
        }

        std::ifstream comm(it->path() / "comm");
        std::string name;

        if (!comm || !std::getline(comm, name)) {
            continue;
        }

        for (const char* candidate : PROCESS_NAMES) {
            if (strncasecmp(name.c_str(), candidate, COMM_LENGTH) == 0 && name.size() == std::min(strlen(candidate), COMM_LENGTH)) {
                return true;
            }
        }
    }

    return false;
#endif
}

static const char* OrEmpty(const char* value) {
    return value ? value : "";
}

class Client {
    public:
    ~Client() {
        Shutdown();
    }

    bool Init() {
        if (m_initialized || IsWine()) {
            return false;
        }

        m_running = true;
        m_thread = std::thread([this]() {
            ThreadFunc();
        });
        m_initialized = true;

        return true;
    }

    void Shutdown() {
        if (!m_initialized) {
            return;
        }

        m_running = false;

        if (m_thread.joinable()) {
            m_thread.join();
        }

        if (m_core) {
            if (m_activityManager) {
                m_activityManager->ClearActivity(nullptr);
            }

            m_core->RunCallbacks();
        }

        Disconnect();
        m_initialized = false;
    }

    bool IsInitialized() const {
        return m_initialized;
    }

    void UpdateActivity(const char* state, const char* details, const char* largeImage, const char* largeText, const char* smallImage, const char* smallText) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingActivity.SetState(OrEmpty(state));
        m_pendingActivity.SetDetails(OrEmpty(details));

        discord::ActivityAssets& assets = m_pendingActivity.GetAssets();
        assets.SetLargeImage(OrEmpty(largeImage));
        assets.SetLargeText(OrEmpty(largeText));
        assets.SetSmallImage(OrEmpty(smallImage));
        assets.SetSmallText(OrEmpty(smallText));

        m_hasActivity = true;
        m_hasPending = true;
    }

    private:
    bool TryConnect() {
        discord::Core* core = nullptr;
        discord::Result result = discord::Core::Create(CLIENT_ID, DiscordCreateFlags_Default, &core);

        if (!core || result != discord::Result::Ok) {
            return false;
        }

        m_core = core;
        m_activityManager = &core->ActivityManager();

        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_hasActivity) {
            m_hasPending = true;
        }

        return true;
    }

    void Disconnect() {
        delete m_core;
        m_core = nullptr;
        m_activityManager = nullptr;
    }

    void FlushPendingActivity() {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_hasPending) {
            return;
        }

        m_activityManager->UpdateActivity(m_pendingActivity, [](discord::Result) {});
        m_hasPending = false;
    }

    void ThreadFunc() {
        using Clock = std::chrono::steady_clock;
        Clock::time_point nextDetect{};

        while (m_running) {
            if (!m_core) {
                Clock::time_point now = Clock::now();

                if (now >= nextDetect) {
                    nextDetect = now + DETECT_INTERVAL;

                    if (IsDiscordRunning()) {
                        TryConnect();
                    }
                }

                if (!m_core) {
                    std::this_thread::sleep_for(IDLE_SLEEP);
                    continue;
                }
            }

            if (m_core->RunCallbacks() != discord::Result::Ok) {
                Disconnect();
                continue;
            }

            if (m_activityManager) {
                FlushPendingActivity();
            }

            std::this_thread::sleep_for(CONNECTED_SLEEP);
        }
    }

    discord::Core* m_core = nullptr;
    discord::ActivityManager* m_activityManager = nullptr;
    discord::Activity m_pendingActivity{};
    std::thread m_thread;
    std::mutex m_mutex;
    std::atomic<bool> m_running{ false };
    bool m_initialized = false;
    bool m_hasActivity = false;
    bool m_hasPending = false;
};

// Function-local so the worker is joined by the destructor before the thread object goes away.
static Client& Get() {
    static Client client;
    return client;
}

bool Init() {
    return Get().Init();
}

void Shutdown() {
    Get().Shutdown();
}

bool IsInitialized() {
    return Get().IsInitialized();
}

void UpdateActivity(const char* state, const char* details, const char* largeImage, const char* largeText, const char* smallImage, const char* smallText) {
    Get().UpdateActivity(state, details, largeImage, largeText, smallImage, smallText);
}

}
