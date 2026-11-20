#include "HoT/Streaming/WorkerThreads.h"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include "HoT/Streaming/StreamLog.h"

namespace HoT::Streaming::WorkerThreads {

struct State {
    std::mutex m_mutex;
    std::condition_variable m_changed;
    unsigned m_running = 0;
    bool m_shutdown = false;
};

// Never destroyed, so a thread finishing during exit never touches a destroyed mutex.
static State& GetState() {
    static State* s_state = new State();
    return *s_state;
}

bool Start(std::function<void()> fn) {
    State& state = GetState();

    {
        std::lock_guard<std::mutex> lock(state.m_mutex);

        if (state.m_shutdown) {
            return false;
        }

        state.m_running++;
    }

    std::thread([fn = std::move(fn)]() {
        try {
            fn();
        } catch (...) {
        }

        State& state = GetState();
        std::lock_guard<std::mutex> lock(state.m_mutex);
        state.m_running--;
        state.m_changed.notify_all();
    }).detach();

    return true;
}

bool ShuttingDown() {
    State& state = GetState();
    std::lock_guard<std::mutex> lock(state.m_mutex);
    return state.m_shutdown;
}

bool SleepFor(unsigned long long ms) {
    State& state = GetState();
    std::unique_lock<std::mutex> lock(state.m_mutex);
    return !state.m_changed.wait_for(lock, std::chrono::milliseconds(ms), [&state] { return state.m_shutdown; });
}

void BeginShutdown() {
    State& state = GetState();
    std::lock_guard<std::mutex> lock(state.m_mutex);
    state.m_shutdown = true;
    state.m_changed.notify_all();
}

bool WaitAll(unsigned long long timeoutMs) {
    State& state = GetState();
    std::unique_lock<std::mutex> lock(state.m_mutex);
    state.m_shutdown = true;
    state.m_changed.notify_all();

    if (state.m_changed.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&state] { return state.m_running == 0; })) {
        return true;
    }

    StreamLog("stream: %u background threads still running at exit", state.m_running);
    return false;
}

}
