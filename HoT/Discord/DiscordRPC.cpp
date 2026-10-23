#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include "HoT/Discord/DiscordRPC.h"
#include "HoT/Discord/Host.h"
#include "HoT/Discord/sdk/discord.h"
#include "HoT/Platform/Process.h"

namespace HoT::Discord::RPC {

static const discord::ClientId CLIENT_ID = 1420551644671901780;
static const auto DETECT_INTERVAL = std::chrono::seconds(15);
static const auto IDLE_SLEEP = std::chrono::milliseconds(500);
static const auto CONNECTED_SLEEP = std::chrono::milliseconds(100);

static const char* OrEmpty(const char* value) {
    return value ? value : "";
}

class Client {
    public:
    ~Client() {
        Shutdown();
    }

    bool Init() {
        if (m_initialized || Platform::IsWine()) {
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

                    if (Host::IsDiscordRunning()) {
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
