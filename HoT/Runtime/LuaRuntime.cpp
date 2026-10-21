#include "HoT/Runtime/LuaRuntime.h"
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "HoT/Core/CVars.h"
#include "HoT/Core/Log.h"
#include "HoT/Core/Modules.h"
#include "HoT/Lua/Script.h"
#include "HoT/Profiler/Profiler.h"
#include "HoT/Runtime/LuaPool.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/time.h>
#include "Engine/Source/Os/Path.h"
#endif
#include "Engine/Source/lua-5.1/src/lgc.h"
#include "Engine/Source/lua-5.1/src/lobject.h"
#include "Engine/Source/lua-5.1/src/lstate.h"

namespace HoT::LuaRuntime {

static const uint32_t STRATA_COUNT = 9;
static const int GC_PAUSE = 110;
static const int GC_STEPMUL = 200;
static const double GC_BUDGET_MS = 1.0;
static const uint64_t GC_SLACK_KB = 4096;
static const size_t PROFILE_TOP_N = 16;

enum ResizeCaller : uint32_t {
    RESIZE_PENDING = 0,
    RESIZE_FORCED,
    RESIZE_GET_SIZE,
    RESIZE_SHOW,
    RESIZE_FLATTEN,
    RESIZE_UNFLATTEN,
    RESIZE_DRAG,
    RESIZE_OTHER,
    RESIZE_CALLER_COUNT,
};

struct FrameStats {
    double m_handlerMs = 0;
    uint32_t m_handlerCalls = 0;
    double m_gcMs = 0;
    uint32_t m_gcSteps = 0;
    double m_allocKB = 0;
    double m_luaKB = 0;
    int m_gcState = 0;
    double m_frameMs = 0;
    double m_updateMs = 0;
    double m_layoutMs = 0;
    uint32_t m_layoutFrames = 0;
    double m_buildMs = 0;
    double m_drawMs = 0;
    uint32_t m_batches = 0;
    uint32_t m_textures = 0;
    uint32_t m_fontStrings = 0;
    uint32_t m_texUploads = 0;
    double m_texUploadMs = 0;
    double m_buildStrata[STRATA_COUNT] = {};
    double m_drawStrata[STRATA_COUNT] = {};
    uint32_t m_resizeFrom[RESIZE_CALLER_COUNT] = {};
    double m_raiseMs = 0;
    uint32_t m_raises = 0;
    double m_compressMs = 0;
    uint32_t m_compresses = 0;
    double m_occludeMs = 0;
    double m_addResizeMs = 0;
    uint32_t m_addResizes = 0;
    uint32_t m_layoutFailed = 0;
};

struct ProfileEntry {
    std::string m_name;
    uint32_t m_calls = 0;
    int64_t m_inclusive = 0;
    int64_t m_self = 0;
};

struct ProfileFrame {
    const void* m_key;
    int64_t m_start;
    int64_t m_child;
    int32_t m_ci;
};

bool g_statsEnabled = false;

static float s_hitchMs = 0.0f;
static int32_t s_profileMode = 0;
static LuaPool* s_activePool = nullptr;
static uint64_t s_lastAllocated = 0;
static FrameStats s_cur;
static FrameStats s_last;
static int32_t s_handlerDepth = 0;
static int32_t s_resizePendingDepth = 0;
static int64_t s_lastPaint = 0;
static uint32_t s_buildIndex = 0;
static uint32_t s_renderIndex = 0;
static std::ofstream s_perfLog;
static std::string s_perfLogPath;
static bool s_perfLogOpened = false;
static lua_State* s_profileState = nullptr;
static std::unordered_map<const void*, ProfileEntry> s_profile;
static std::vector<ProfileFrame> s_profileStack;

#if defined(_WIN32)
static int64_t Now() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

static double ToMs(int64_t ticks) {
    static const double ticksPerMs = [] {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        return frequency.QuadPart / 1000.0;
    }();

    return ticks / ticksPerMs;
}

static void FormatClock(char* buffer, size_t size) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(buffer, size, "%02u:%02u:%02u.%03u", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}
#else
// Ticks are CLOCK_MONOTONIC nanoseconds.
static int64_t Now() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000000000 + ts.tv_nsec;
}

static double ToMs(int64_t ticks) {
    return ticks / 1000000.0;
}

static void FormatClock(char* buffer, size_t size) {
    timeval tv;
    gettimeofday(&tv, nullptr);
    tm local;
    localtime_r(&tv.tv_sec, &local);
    snprintf(buffer, size, "%02d:%02d:%02d.%03d", local.tm_hour, local.tm_min, local.tm_sec, static_cast<int>(tv.tv_usec / 1000));
}
#endif

static uint32_t NextSlot(uint32_t& index) {
    return index < STRATA_COUNT ? index++ : STRATA_COUNT - 1;
}

static int FormatStrata(const FrameStats& stats, char* buffer, size_t size) {
    static const char* const names[STRATA_COUNT] = { "W", "BG", "LOW", "MED", "HIGH", "DLG", "FS", "FSD", "TIP" };
    int len = 0;
    buffer[0] = '\0';

    for (uint32_t i = 0; i < STRATA_COUNT && len < static_cast<int>(size); i++) {
        int written = snprintf(buffer + len, size - len, "%s%s %.1f/%.1f", i ? "  " : "", names[i], stats.m_buildStrata[i], stats.m_drawStrata[i]);

        if (written < 0) {
            break;
        }

        len += written;
    }

    return len;
}

static int FormatResizeFrom(const FrameStats& stats, char* buffer, size_t size) {
    const uint32_t* from = stats.m_resizeFrom;

    return snprintf(buffer, size, "p%u f%u g%u s%u fl%u uf%u d%u o%u", from[RESIZE_PENDING], from[RESIZE_FORCED], from[RESIZE_GET_SIZE], from[RESIZE_SHOW], from[RESIZE_FLATTEN], from[RESIZE_UNFLATTEN], from[RESIZE_DRAG], from[RESIZE_OTHER]);
}

static void OpenPerfLog() {
    if (s_perfLogOpened) {
        return;
    }

    s_perfLogOpened = true;

    if (s_perfLogPath.empty()) {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(s_perfLogPath).parent_path(), ec);
    s_perfLog.open(s_perfLogPath, std::ios::out | std::ios::trunc);

    if (!s_perfLog.is_open()) {
        return;
    }

    char clock[32];
    FormatClock(clock, sizeof(clock));
    char line[512];
    snprintf(line, sizeof(line), "# HoT UI perf log, one file per client start, opened at %s on the first hitch. One row per frame whose Lua plus GC time or update plus build plus draw time reached luaPerfHitchMs. Times are ms.", clock);
    s_perfLog << line << "\n";
    snprintf(line, sizeof(line), "# settings: luaPerfHitchMs %.1f, luaProfile %d", s_hitchMs, s_profileMode);
    s_perfLog << line << "\n";
    s_perfLog << "time\tframe\tlua\tcalls\tgc\tsteps\tupdate\tlayout\tlayoutFrames\tresizeFrom\tbuild\ttextures\tstrings\tdraw\tworld\tbatches\ttexUploads\ttexUploadMs\tallocKB\theapKB\traise/compress/occlude\tresizeList\tstrata build/draw\n";
    s_perfLog.flush();
    LOG_INFO << "UI perf log: " << s_perfLogPath;
}

static void PerfLog(const char* line) {
    OpenPerfLog();

    if (!s_perfLog.is_open()) {
        return;
    }

    s_perfLog << line << "\n";
    s_perfLog.flush();
}

static void NoteSetting(const char* name, double value) {
    if (!s_perfLog.is_open()) {
        return;
    }

    char clock[32];
    FormatClock(clock, sizeof(clock));
    char line[128];
    snprintf(line, sizeof(line), "# %s %s = %g", clock, name, value);
    PerfLog(line);
}

static void OnHook(lua_State* L, lua_Debug* ar) {
    if (L != s_profileState) {
        return;
    }

    int64_t now = Now();

    if (ar->event == LUA_HOOKCALL) {
        int32_t ci = ar->i_ci;

        while (!s_profileStack.empty() && s_profileStack.back().m_ci >= ci) {
            s_profileStack.pop_back();
        }

        Closure* closure = clvalue((L->base_ci + ci)->func);
        const void* key = closure->c.isC ? reinterpret_cast<const void*>(closure->c.f) : static_cast<const void*>(closure->l.p);

        if (s_profile.find(key) == s_profile.end()) {
            ProfileEntry& entry = s_profile[key];
            char name[192];

            if (closure->c.isC) {
                lua_getinfo(L, "n", ar);
                snprintf(name, sizeof(name), "%s  (C)", ar->name ? ar->name : "?");
            } else {
                lua_getinfo(L, "Sn", ar);
                const char* source = ar->source ? ar->source : ar->short_src;

                if (*source == '@' || *source == '=') {
                    source++;
                }

                snprintf(name, sizeof(name), "%s:%d  %s", source, ar->linedefined, ar->name ? ar->name : "");
            }

            entry.m_name = name;
        }

        s_profileStack.push_back({ key, now, 0, ci });
        return;
    }

    if (ar->event != LUA_HOOKRET && ar->event != LUA_HOOKTAILRET) {
        return;
    }

    if (s_profileStack.empty()) {
        return;
    }

    if (ar->event == LUA_HOOKRET) {
        int32_t ci = ar->i_ci;

        while (!s_profileStack.empty() && s_profileStack.back().m_ci > ci) {
            s_profileStack.pop_back();
        }

        if (s_profileStack.empty() || s_profileStack.back().m_ci != ci) {
            return;
        }
    }

    ProfileFrame frame = s_profileStack.back();
    s_profileStack.pop_back();
    int64_t elapsed = now - frame.m_start;
    auto it = s_profile.find(frame.m_key);

    if (it != s_profile.end()) {
        it->second.m_calls++;
        it->second.m_inclusive += elapsed;
        it->second.m_self += elapsed - frame.m_child;
    }

    if (!s_profileStack.empty()) {
        s_profileStack.back().m_child += elapsed;
    }
}

static void InstallHook(lua_State* L) {
    s_profileStack.clear();
    s_profile.clear();

    if (s_profileMode != 0) {
        lua_sethook(L, OnHook, LUA_MASKCALL | LUA_MASKRET, 0);
        s_profileState = L;
        return;
    }

    lua_sethook(L, nullptr, 0, 0);
    s_profileState = nullptr;
}

static void WriteProfile() {
    if (s_profile.empty()) {
        return;
    }

    std::vector<const ProfileEntry*> top;
    top.reserve(s_profile.size());
    int64_t totalSelf = 0;

    for (const auto& kv : s_profile) {
        top.push_back(&kv.second);
        totalSelf += kv.second.m_self;
    }

    std::sort(top.begin(), top.end(), [](const ProfileEntry* a, const ProfileEntry* b) {
        return a->m_self > b->m_self;
    });

    char line[448];
    snprintf(line, sizeof(line), "#  profile: %u functions, %.1f ms of self time, top entries as self ms / inclusive ms / calls", static_cast<uint32_t>(top.size()), ToMs(totalSelf));
    PerfLog(line);

    for (size_t i = 0; i < top.size() && i < PROFILE_TOP_N; i++) {
        const ProfileEntry* entry = top[i];
        snprintf(line, sizeof(line), "#   %8.2f %8.2f %6u  %s", ToMs(entry->m_self), ToMs(entry->m_inclusive), entry->m_calls, entry->m_name.c_str());
        PerfLog(line);
    }
}

// Spreads the collector over frames in about 1 ms slices, only while no Lua is running.
static void PaceGc(lua_State* L) {
    global_State* g = G(L);

    if (L->ci != L->base_ci) {
        return;
    }

    int64_t start = Now();
    uint64_t pauseThreshold = static_cast<uint64_t>(g->estimate) * GC_PAUSE / 100;
    bool idle = g->gcstate == GCSpause && static_cast<uint64_t>(g->totalbytes) < pauseThreshold;

    if (!idle) {
        double budget = GC_BUDGET_MS;

        if (pauseThreshold && g->totalbytes > pauseThreshold) {
            budget *= (std::min)(4.0, static_cast<double>(g->totalbytes) / static_cast<double>(pauseThreshold));
        }

        do {
            g->GCthreshold = g->totalbytes;
            luaC_step(L);
            s_cur.m_gcSteps++;

            if (g->gcstate == GCSpause) {
                break;
            }
        } while (ToMs(Now() - start) < budget);
    }

    uint64_t next = static_cast<uint64_t>(g->totalbytes) + GC_SLACK_KB * 1024;

    if (g->gcstate == GCSpause && pauseThreshold > next) {
        next = pauseThreshold;
    }

    g->GCthreshold = static_cast<lu_mem>((std::min)(next, static_cast<uint64_t>(MAX_LUMEM)));
    s_cur.m_gcMs += ToMs(Now() - start);
}

static void RollFrame(lua_State* L) {
    if (s_activePool) {
        uint64_t total = s_activePool->BytesAllocated();
        s_cur.m_allocKB = static_cast<double>(total - s_lastAllocated) / 1024.0;
        s_lastAllocated = total;
    } else {
        s_cur.m_allocKB = -1;
    }

    s_cur.m_luaKB = lua_gc(L, LUA_GCCOUNT, 0) + lua_gc(L, LUA_GCCOUNTB, 0) / 1024.0;
    s_cur.m_gcState = G(L)->gcstate;
    s_last = s_cur;
    s_cur = FrameStats();

    double paintMs = s_last.m_updateMs + s_last.m_buildMs + s_last.m_drawMs;

    if (s_hitchMs > 0 && (s_last.m_handlerMs + s_last.m_gcMs >= s_hitchMs || paintMs >= s_hitchMs)) {
        char clock[32];
        FormatClock(clock, sizeof(clock));
        char strata[256];
        FormatStrata(s_last, strata, sizeof(strata));
        char resizeFrom[96];
        FormatResizeFrom(s_last, resizeFrom, sizeof(resizeFrom));
        char raise[96];
        snprintf(raise, sizeof(raise), "r%.2f/%u c%.2f/%u o%.2f", s_last.m_raiseMs, s_last.m_raises, s_last.m_compressMs, s_last.m_compresses, s_last.m_occludeMs);
        char resizeList[64];
        snprintf(resizeList, sizeof(resizeList), "a%.2f/%u fail%u", s_last.m_addResizeMs, s_last.m_addResizes, s_last.m_layoutFailed);
        char line[1024];
        snprintf(line, sizeof(line), "%s\t%.1f\t%.2f\t%u\t%.2f\t%u\t%.2f\t%.2f\t%u\t%s\t%.2f\t%u\t%u\t%.2f\t%.2f\t%u\t%u\t%.2f\t%.0f\t%.0f\t%s\t%s\t%s", clock, s_last.m_frameMs, s_last.m_handlerMs, s_last.m_handlerCalls, s_last.m_gcMs, s_last.m_gcSteps, s_last.m_updateMs, s_last.m_layoutMs, s_last.m_layoutFrames, resizeFrom, s_last.m_buildMs, s_last.m_textures, s_last.m_fontStrings, s_last.m_drawMs, s_last.m_drawStrata[0], s_last.m_batches, s_last.m_texUploads, s_last.m_texUploadMs, s_last.m_allocKB, s_last.m_luaKB, raise, resizeList, strata);
        PerfLog(line);

        if (s_profileMode != 0) {
            WriteProfile();
        }
    }

    s_profile.clear();
    s_profileStack.clear();
}

void* CreatePool() {
    LuaPool* pool = new LuaPool();
    s_activePool = pool;
    s_lastAllocated = 0;

    return pool;
}

void* Realloc(void* ud, void* ptr, size_t osize, size_t nsize) {
    return static_cast<LuaPool*>(ud)->Reallocate(ptr, osize, nsize);
}

void DestroyPool(void* ud) {
    if (!ud) {
        return;
    }

    if (s_activePool == ud) {
        s_activePool = nullptr;
        s_lastAllocated = 0;
    }

    delete static_cast<LuaPool*>(ud);
}

void OnStateCreated(lua_State* L) {
    if (L) {
        lua_gc(L, LUA_GCSETPAUSE, GC_PAUSE);
        lua_gc(L, LUA_GCSETSTEPMUL, GC_STEPMUL);
        InstallHook(L);
    }

    s_cur = FrameStats();
    s_handlerDepth = 0;
    s_resizePendingDepth = 0;
}

void BeginPaint() {
    if (!g_statsEnabled) {
        return;
    }

    int64_t now = Now();

    if (s_lastPaint) {
        s_cur.m_frameMs = ToMs(now - s_lastPaint);
    }

    s_lastPaint = now;
    s_buildIndex = 0;
    s_renderIndex = 0;
}

void EndPaint() {
    lua_State* L = FrameScript_GetContext();

    if (!L) {
        return;
    }

    {
        Profiler::Scope profilerScope(Profiler::Zone::LuaGc);
        PaceGc(L);
    }

    if (g_statsEnabled) {
        RollFrame(L);
    }
}

int64_t BeginPhase(Phase phase) {
    if (phase == Phase::Layout) {
        s_resizePendingDepth++;
    }

    return Now();
}

void EndPhase(Phase phase, int64_t start) {
    double ms = ToMs(Now() - start);

    switch (phase) {
        case Phase::Update:
            s_cur.m_updateMs += ms;
            break;

        case Phase::Layout:
            s_cur.m_layoutMs += ms;
            s_resizePendingDepth--;
            break;

        case Phase::Build:
            s_cur.m_buildMs += ms;
            s_cur.m_buildStrata[NextSlot(s_buildIndex)] += ms;
            break;

        case Phase::Draw:
            s_cur.m_drawMs += ms;
            s_cur.m_drawStrata[NextSlot(s_renderIndex)] += ms;
            break;

        case Phase::TexUpload:
            s_cur.m_texUploadMs += ms;
            s_cur.m_texUploads++;
            break;

        case Phase::Raise:
            s_cur.m_raiseMs += ms;
            s_cur.m_raises++;
            break;

        case Phase::Compress:
            s_cur.m_compressMs += ms;
            s_cur.m_compresses++;
            break;

        case Phase::Occlude:
            s_cur.m_occludeMs += ms;
            break;

        case Phase::AddResize:
            s_cur.m_addResizeMs += ms;
            s_cur.m_addResizes++;
            break;
    }
}

void Increment(Counter counter) {
    switch (counter) {
        case Counter::Batches:
            s_cur.m_batches++;
            break;

        case Counter::Textures:
            s_cur.m_textures++;
            break;

        case Counter::FontStrings:
            s_cur.m_fontStrings++;
            break;

        case Counter::LayoutFrames:
            s_cur.m_layoutFrames++;
            s_cur.m_resizeFrom[s_resizePendingDepth > 0 ? RESIZE_PENDING : RESIZE_OTHER]++;
            break;

        case Counter::LayoutFailed:
            s_cur.m_layoutFailed++;
            break;
    }
}

int64_t EnterHandler(bool& outer) {
    outer = s_handlerDepth++ == 0;

    return Now();
}

void LeaveHandler(bool outer, int64_t start) {
    s_handlerDepth--;
    s_cur.m_handlerCalls++;

    if (outer) {
        s_cur.m_handlerMs += ToMs(Now() - start);
    }
}

static int32_t Script_GetLuaPerfStats(lua_State* L) {
    const FrameStats& stats = s_last;
    lua_pushnumber(L, stats.m_handlerMs);
    lua_pushnumber(L, stats.m_handlerCalls);
    lua_pushnumber(L, stats.m_gcMs);
    lua_pushnumber(L, stats.m_gcSteps);
    lua_pushnumber(L, stats.m_allocKB);
    lua_pushnumber(L, stats.m_luaKB);
    lua_pushnumber(L, stats.m_gcState);
    lua_pushnumber(L, 1);
    lua_pushboolean(L, s_activePool != nullptr);

    return 9;
}

static int32_t Script_GetUiPerfStats(lua_State* L) {
    const FrameStats& stats = s_last;
    lua_pushnumber(L, stats.m_frameMs);
    lua_pushnumber(L, stats.m_updateMs);
    lua_pushnumber(L, stats.m_layoutMs);
    lua_pushnumber(L, stats.m_layoutFrames);
    lua_pushnumber(L, stats.m_buildMs);
    lua_pushnumber(L, stats.m_drawMs);
    lua_pushnumber(L, stats.m_drawStrata[0]);
    lua_pushnumber(L, stats.m_batches);
    lua_pushnumber(L, stats.m_textures);
    lua_pushnumber(L, stats.m_fontStrings);
    lua_pushnumber(L, stats.m_texUploads);
    lua_pushnumber(L, stats.m_texUploadMs);

    char strata[256];
    FormatStrata(stats, strata, sizeof(strata));
    lua_pushstring(L, strata);
    lua_pushstring(L, s_perfLogPath.c_str());
    char resizeFrom[96];
    FormatResizeFrom(stats, resizeFrom, sizeof(resizeFrom));
    lua_pushstring(L, resizeFrom);

    return 15;
}

static void OnStatsChanged(const char* value) {
    g_statsEnabled = CVars::Bool(value);
    NoteSetting("luaPerfStats", g_statsEnabled ? 1 : 0);
}

static void OnHitchChanged(const char* value) {
    s_hitchMs = CVars::Float(value);
    NoteSetting("luaPerfHitchMs", s_hitchMs);
}

static void OnProfileChanged(const char* value) {
    s_profileMode = CVars::Int(value);

    if (lua_State* L = FrameScript_GetContext()) {
        InstallHook(L);
    }

    NoteSetting("luaProfile", s_profileMode);
}

static void SetPerfLogPath() {
    char module[MAX_PATH];
#if defined(_WIN32)
    GetModuleFileNameA(nullptr, module, MAX_PATH);
#else
    module[0] = '\0';
    GetModuleFileNameUTF8(nullptr, module, MAX_PATH);
#endif
    time_t now = time(nullptr);
    tm local = *localtime(&now);
    char name[64];
    strftime(name, sizeof(name), "uiperf_%Y-%m-%d_%H-%M-%S.log", &local);
    s_perfLogPath = (std::filesystem::path(module).parent_path() / "Logs" / name).string();
}

class LuaRuntimeModule : public Module {
    public:
    const char* Name() const override { return "LuaRuntime"; }

    void OnInitializeGlobal() override {
        SetPerfLogPath();

        Lua::RegisterFunction("GetLuaPerfStats", Script_GetLuaPerfStats, Lua::State::Any);
        Lua::RegisterFunction("GetUiPerfStats", Script_GetUiPerfStats, Lua::State::Any);

        CVars::Register("luaPerfStats", "Collect per-frame UI timings: Lua handler time and calls, GC time, allocation and heap size (GetLuaPerfStats), plus the layout, batch build, draw and texture upload phases of the UI frame (GetUiPerfStats).", "0", OnStatsChanged);
        CVars::Register("luaPerfHitchMs", "With luaPerfStats on, write any frame whose Lua handler plus GC time, or whose UI update plus build plus draw time, reaches this many ms as a row to Logs/uiperf_<client start time>.log, a new file per client start. 0 disables.", "0", OnHitchChanged);
        CVars::Register("luaProfile", "1 times every Lua and C function call through a Lua call/return hook and writes the top functions by self time under each hitch row of the uiperf log (needs luaPerfStats and luaPerfHitchMs). Costs a few percent of Lua time while on. 0 off.", "0", OnProfileChanged);
    }
};

Module* GetModule() {
    static LuaRuntimeModule module;
    return &module;
}

}
