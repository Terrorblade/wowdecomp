#include "HoT/Profiler/Sampler.h"
#include <windows.h>
#include <dbghelp.h>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace HoT::Profiler::Sampler {

static const uint32_t MAX_FRAMES = 64;

struct Counts {
    uint32_t m_self = 0;
    uint32_t m_inclusive = 0;
};

static std::thread s_thread;
static std::atomic<bool> s_running(false);
static HANDLE s_target = nullptr;
static std::mutex s_mutex;
static std::unordered_map<uint64_t, Counts> s_counts;
static uint32_t s_total = 0;
static ULONGLONG s_startTick = 0;
static bool s_symbolsReady = false;

// Functions are keyed by the start of their unwind entry so every address inside one lands on the same key.
static uint64_t FunctionKey(DWORD64 pc) {
    DWORD64 imageBase = 0;
    PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(pc, &imageBase, nullptr);
    return function ? imageBase + function->BeginAddress : pc;
}

// Walks the suspended thread's stack, the walk only reads memory so the target holding a heap lock is harmless.
static uint32_t WalkStack(CONTEXT& context, DWORD64* frames) {
    uint32_t count = 0;

    while (count < MAX_FRAMES && context.Rip) {
        frames[count++] = context.Rip;

        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(context.Rip, &imageBase, nullptr);

        if (function) {
            void* handlerData = nullptr;
            DWORD64 establisherFrame = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, context.Rip, function, &context, &handlerData, &establisherFrame, nullptr);
        } else {
            if (!context.Rsp) {
                break;
            }

            context.Rip = *reinterpret_cast<DWORD64*>(context.Rsp);
            context.Rsp += 8;
        }
    }

    return count;
}

static void Run() {
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    DWORD64 frames[MAX_FRAMES];
    uint64_t keys[MAX_FRAMES];

    while (s_running.load()) {
        if (timer) {
            LARGE_INTEGER due;
            due.QuadPart = -10000;
            SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
            WaitForSingleObject(timer, INFINITE);
        } else {
            Sleep(1);
        }

        if (SuspendThread(s_target) == static_cast<DWORD>(-1)) {
            continue;
        }

        CONTEXT context = {};
        context.ContextFlags = CONTEXT_FULL;
        uint32_t count = 0;

        if (GetThreadContext(s_target, &context)) {
            count = WalkStack(context, frames);
        }

        ResumeThread(s_target);

        if (!count) {
            continue;
        }

        uint32_t unique = 0;

        for (uint32_t i = 0; i < count; i++) {
            // Return addresses point past the call, step back so the lookup lands inside the caller.
            uint64_t key = FunctionKey(i ? frames[i] - 1 : frames[i]);

            if (std::find(keys, keys + unique, key) == keys + unique) {
                keys[unique++] = key;
            }
        }

        std::lock_guard<std::mutex> lock(s_mutex);
        s_total++;
        s_counts[keys[0]].m_self++;

        for (uint32_t i = 0; i < unique; i++) {
            s_counts[keys[i]].m_inclusive++;
        }
    }

    if (timer) {
        CloseHandle(timer);
    }
}

void Start() {
    Stop();

    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &s_target, THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0)) {
        s_target = nullptr;
        return;
    }

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_counts.clear();
        s_total = 0;
    }

    s_startTick = GetTickCount64();
    s_running.store(true);
    s_thread = std::thread(Run);
}

void Stop() {
    s_running.store(false);

    if (s_thread.joinable()) {
        s_thread.join();
    }

    if (s_target) {
        CloseHandle(s_target);
        s_target = nullptr;
    }
}

static std::string SymbolName(uint64_t address) {
    static HANDLE process = nullptr;

    // A duplicated process handle is its own DbgHelp session, so the crash handlers can still initialize theirs.
    if (!process && DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &process, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        s_symbolsReady = SymInitialize(process, nullptr, TRUE) != FALSE;
    }

    char module[MAX_PATH] = "?";
    HMODULE handle = nullptr;

    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(address), &handle)) {
        char path[MAX_PATH];

        if (GetModuleFileNameA(handle, path, sizeof(path))) {
            const char* slash = strrchr(path, '\\');
            snprintf(module, sizeof(module), "%s", slash ? slash + 1 : path);
        }
    }

    char buffer[sizeof(SYMBOL_INFO) + 512] = {};
    SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 511;
    DWORD64 displacement = 0;
    char name[700];

    if (s_symbolsReady && SymFromAddr(process, address, &displacement, symbol)) {
        snprintf(name, sizeof(name), "%s!%s", module, symbol->Name);
    } else {
        snprintf(name, sizeof(name), "%s+0x%llx", module, static_cast<unsigned long long>(handle ? address - reinterpret_cast<uint64_t>(handle) : address));
    }

    return name;
}

bool Report(std::vector<Entry>& entries, uint32_t& total, double& seconds) {
    std::unordered_map<uint64_t, Counts> counts;

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        counts = s_counts;
        total = s_total;
    }

    seconds = (GetTickCount64() - s_startTick) / 1000.0;

    std::unordered_map<std::string, Entry> merged;

    for (const auto& [address, count] : counts) {
        std::string name = SymbolName(address);
        Entry& entry = merged[name];
        entry.m_name = name;
        entry.m_self += count.m_self;
        entry.m_inclusive += count.m_inclusive;
    }

    entries.clear();

    for (auto& [name, entry] : merged) {
        entries.push_back(std::move(entry));
    }

    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.m_self > b.m_self; });
    return true;
}

}
