#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include "HoT/Character/HD/HDSheetPool.h"
#include "HoT/Platform/Process.h"

namespace HoT::HD::Pool {

static const int MAX_THREADS = 6;

struct Task {
    std::function<void()> m_fn;
    Group* m_group = nullptr;
};

struct State {
    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::deque<Task> m_queue;
    int m_threadCount = 0;
};

static State* s_state = nullptr;
static std::once_flag s_once;

static void Run(Task& task) {
    task.m_fn();

    if (task.m_group) {
        task.m_group->m_remaining.fetch_sub(1, std::memory_order_acq_rel);
    }
}

static void Worker() {
    Platform::LowerThreadPriority();

    for (;;) {
        Task task;

        {
            std::unique_lock<std::mutex> lock(s_state->m_mutex);
            s_state->m_wake.wait(lock, [] {
                return !s_state->m_queue.empty();
            });
            task = std::move(s_state->m_queue.front());
            s_state->m_queue.pop_front();
        }

        Run(task);
    }
}

static void Start() {
    std::call_once(s_once, [] {
        s_state = new State();
        unsigned cores = std::thread::hardware_concurrency();
        s_state->m_threadCount = std::clamp<int>(cores > 2 ? static_cast<int>(cores) - 2 : 1, 1, MAX_THREADS);

        for (int i = 0; i < s_state->m_threadCount; i++) {
            std::thread(Worker).detach();
        }
    });
}

static bool RunOne(Group* group) {
    Task task;

    {
        std::lock_guard<std::mutex> lock(s_state->m_mutex);

        if (s_state->m_queue.empty() || s_state->m_queue.front().m_group != group) {
            return false;
        }

        task = std::move(s_state->m_queue.front());
        s_state->m_queue.pop_front();
    }

    Run(task);
    return true;
}

void Submit(std::function<void()> task, Group* group, bool front) {
    Start();

    if (group) {
        group->m_remaining.fetch_add(1, std::memory_order_acq_rel);
    }

    {
        std::lock_guard<std::mutex> lock(s_state->m_mutex);

        if (front) {
            s_state->m_queue.push_front(Task { std::move(task), group });
        } else {
            s_state->m_queue.push_back(Task { std::move(task), group });
        }
    }

    s_state->m_wake.notify_one();
}

void Wait(Group& group) {
    Start();

    while (group.m_remaining.load(std::memory_order_acquire) > 0) {
        if (!RunOne(&group)) {
            std::this_thread::yield();
        }
    }
}

int ThreadCount() {
    Start();
    return s_state->m_threadCount;
}

}
