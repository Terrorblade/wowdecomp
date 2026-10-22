#include "HoT/Profiler/Profiler.h"
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstring>
#include <filesystem>
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxDevice/CGxShader.h"
#include "Engine/Source/Gx/CGxTex.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Shared.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "HoT/Core/CVars.h"
#include "HoT/Core/Hotkeys.h"
#include "HoT/Core/Log.h"
#include "HoT/Core/Modules.h"
#include "HoT/Lua/Script.h"
#include "HoT/Platform/Process.h"
#include "HoT/Platform/Time.h"
#include "HoT/Profiler/GpuTiming.h"
#include "HoT/Profiler/ProfilerUI.h"
#include "HoT/Runtime/LuaRuntime.h"

namespace HoT::Profiler {

static const size_t ZONE_COUNT = static_cast<size_t>(Zone::Count);
static const size_t COUNTER_COUNT = static_cast<size_t>(Counter::Count);
static const size_t STACK_DEPTH = 64;
static const size_t HISTORY_SIZE = 240;
static const size_t TOP_COUNT = 15;
static const size_t HITCH_KEEP = 8;

enum ZoneFlags : uint8_t {
    ZONE_FLAT = 1,
    ZONE_DETAIL = 2,
    ZONE_GPU = 4,
};

struct ZoneInfo {
    const char* m_name;
    Zone m_parent;
    uint8_t m_flags;
};

// Flat zones run inside many other zones and are listed as hot spots, detail zones also take the draws they issue.
static const ZoneInfo s_zones[ZONE_COUNT] = {
    { "Frame", Zone::Frame, 0 },
    { "Sleep and frame limiter", Zone::Frame, 0 },
    { "Update events and timers", Zone::Frame, 0 },
    { "Input", Zone::Frame, 0 },
    { "Poll", Zone::Frame, 0 },
    { "Async file callbacks", Zone::Poll, 0 },
    { "Texture cache", Zone::Poll, 0 },
    { "Sound engine", Zone::Poll, 0 },
    { "Network", Zone::Poll, 0 },
    { "Character textures", Zone::Poll, 0 },
    { "Posted events", Zone::Frame, 0 },
    { "Idle", Zone::Frame, 0 },
    { "Unit movement", Zone::Idle, 0 },
    { "Game UI idle", Zone::Idle, 0 },
    { "Spell idle", Zone::Idle, 0 },
    { "HoT modules", Zone::Idle, 0 },
    { "Paint", Zone::Frame, 0 },
    { "UI update (OnUpdate, layout)", Zone::Paint, 0 },
    { "UI batch build", Zone::Paint, 0 },
    { "UI draw", Zone::Paint, ZONE_GPU },
    { "World update", Zone::UiDraw, 0 },
    { "Object effects", Zone::WorldUpdate, 0 },
    { "Unit display update", Zone::WorldUpdate, 0 },
    { "World scene update", Zone::WorldUpdate, 0 },
    { "Map prepare (query, load areas)", Zone::WorldSceneUpdate, 0 },
    { "World text and names", Zone::UiDraw, 0 },
    { "World render", Zone::UiDraw, ZONE_GPU },
    { "Scene cull", Zone::WorldRender, 0 },
    { "Object animate (units, gobs)", Zone::SceneCull, 0 },
    { "M2 animate", Zone::WorldRender, 0 },
    { "Shadow maps", Zone::WorldRender, ZONE_GPU },
    { "Terrain", Zone::WorldRender, ZONE_GPU },
    { "WMO groups", Zone::WorldRender, ZONE_GPU },
    { "Horizon and low detail", Zone::WorldRender, ZONE_GPU },
    { "Sky", Zone::WorldRender, ZONE_GPU },
    { "Entity prep", Zone::WorldRender, 0 },
    { "Liquids", Zone::WorldRender, ZONE_GPU },
    { "Nameplates", Zone::WorldRender, 0 },
    { "M2 opaque", Zone::WorldRender, ZONE_GPU },
    { "Ground clutter", Zone::WorldRender, ZONE_GPU },
    { "M2 transparent", Zone::WorldRender, ZONE_GPU },
    { "Weather and barriers", Zone::WorldRender, ZONE_GPU },
    { "Spell visuals", Zone::WorldRender, ZONE_GPU },
    { "Glares and full screen effects", Zone::WorldRender, ZONE_GPU },
    { "Lua GC", Zone::Paint, 0 },
    { "Font update", Zone::Paint, 0 },
    { "Present", Zone::Paint, 0 },
    { "Deferred draw flush", Zone::Present, ZONE_GPU },
    { "GPU sync (gxFixLag)", Zone::Present, 0 },
    { "FPS cap sleep", Zone::Present, 0 },
    { "Swap (waits on GPU)", Zone::Present, 0 },
    { "Lua script handlers", Zone::Frame, ZONE_FLAT },
    { "Gx state sync", Zone::Frame, ZONE_FLAT },
    { "Texture create", Zone::Frame, ZONE_FLAT },
    { "Texture upload", Zone::Frame, ZONE_FLAT },
    { "Buffer lock", Zone::Frame, ZONE_FLAT },
    { "Shader create", Zone::Frame, ZONE_FLAT },
    { "M2 create (sync open)", Zone::Frame, ZONE_FLAT },
    { "Blocking load wait", Zone::Frame, ZONE_FLAT },
    { "Particle update", Zone::Frame, ZONE_FLAT },
    { "Ribbon update", Zone::Frame, ZONE_FLAT },
    { "Particle draw", Zone::Frame, ZONE_DETAIL },
    { "Ribbon draw", Zone::Frame, ZONE_DETAIL },
};

struct CounterInfo {
    const char* m_group;
    const char* m_name;
};

static const CounterInfo s_counters[COUNTER_COUNT] = {
    { "Draw", "Draw calls" },
    { "Draw", "Indexed draw calls" },
    { "Draw", "Primitives" },
    { "Draw", "Vertices" },
    { "State", "State syncs" },
    { "State", "Render state changes" },
    { "State", "Texture binds" },
    { "State", "Vertex shader binds" },
    { "State", "Pixel shader binds" },
    { "State", "Shader constant flushes" },
    { "Buffers", "Buffer locks" },
    { "Buffers", "Buffer lock KB" },
    { "Buffers", "Stream buffer discards" },
    { "Buffers", "Pool resizes" },
    { "Textures", "Texture creates" },
    { "Textures", "Texture uploads" },
    { "Textures", "Texture upload KB" },
    { "Textures", "Texture destroys" },
    { "Targets", "Render target switches" },
    { "Targets", "Clears" },
    { "Shaders", "Shader creates" },
    { "Network", "Packets handled" },
    { "Network", "Packet KB" },
    { "Lua", "Script handler calls" },
    { "Scene", "Terrain chunks" },
    { "Scene", "WMO group renders" },
    { "Scene", "Doodads" },
    { "Scene", "M2 models animated" },
    { "Scene", "M2 models drawn" },
    { "Scene", "M2 draw elements" },
    { "Scene", "Particle emitters" },
    { "Scene", "Ribbon emitters" },
    { "Scene", "Liquid instances" },
    { "Scene", "Ground clutter batches" },
    { "Units", "Objects updated" },
    { "Units", "Players drawn" },
    { "Units", "Players hidden by limit" },
    { "Loading", "M2 creates" },
    { "Loading", "Blocking load waits" },
};

struct ZoneFrame {
    int64_t m_inclusive;
    int64_t m_child;
    uint32_t m_calls;
    uint32_t m_draws;
    uint32_t m_prims;
};

struct FrameData {
    ZoneFrame m_zones[ZONE_COUNT];
    uint64_t m_counters[COUNTER_COUNT];
};

struct ZoneWindow {
    double m_inclusive;
    double m_self;
    double m_peak;
    uint64_t m_calls;
    uint64_t m_draws;
    uint64_t m_prims;
    double m_gpu;
    double m_gpuPeak;
};

struct CounterWindow {
    double m_sum;
    double m_peak;
};

struct StackEntry {
    Zone m_zone;
    Zone m_drawZone;
    int64_t m_start;
    int64_t m_child;
    int32_t m_gpuMark;
};

struct TopEntry {
    std::string m_name;
    double m_ms = 0;
    uint64_t m_count = 0;
    uint64_t m_draws = 0;
    uint64_t m_prims = 0;
    uint64_t m_bytes = 0;
};

struct ModelStat {
    std::string m_name;
    int64_t m_ticks = 0;
    uint64_t m_elements = 0;
    uint64_t m_draws = 0;
    uint64_t m_prims = 0;
};

struct OpcodeStat {
    int64_t m_ticks = 0;
    uint64_t m_count = 0;
    uint64_t m_bytes = 0;
};

struct ShaderStat {
    std::string m_name;
    uint64_t m_draws = 0;
    uint64_t m_prims = 0;
};

struct Published {
    double m_seconds = 0;
    uint32_t m_frames = 0;
    double m_frameMs = 0;
    double m_frameMax = 0;
    double m_frameP99 = 0;
    double m_low1Fps = 0;
    double m_cpuMs = 0;
    double m_gpuMs = 0;
    uint32_t m_gpuFrames = 0;
    uint32_t m_gpuDropped = 0;
    uint32_t m_hitches = 0;
    ZoneWindow m_zones[ZONE_COUNT] = {};
    CounterWindow m_counters[COUNTER_COUNT] = {};
    std::vector<TopEntry> m_models;
    std::vector<TopEntry> m_opcodes;
    std::vector<TopEntry> m_shaders;
    uint32_t m_textureCount = 0;
    double m_textureMB = 0;
    uint32_t m_renderTargets = 0;
    double m_renderTargetMB = 0;
    double m_availableTexMB = -1;
    double m_workingSetMB = 0;
    double m_privateMB = 0;
    double m_texCacheMB = -1;
    double m_texCacheLimitMB = -1;
};

struct Hitch {
    std::string m_clock;
    double m_frameMs = 0;
    double m_zoneInclusive[ZONE_COUNT] = {};
    double m_zoneSelf[ZONE_COUNT] = {};
    uint32_t m_zoneDraws[ZONE_COUNT] = {};
    uint64_t m_counters[COUNTER_COUNT] = {};
};

bool g_enabled = false;

static bool s_open = false;
static bool s_gpuTiming = true;
static bool s_modelTop = true;
static bool s_shaderTop = true;
static float s_intervalSec = 0.5f;
static float s_hitchMs = 50.0f;
static bool s_luaStatsWasOn = false;
static uint64_t s_mainThread = 0;

static FrameData s_frame;
static StackEntry s_stack[STACK_DEPTH];
static uint32_t s_depth = 0;
static uint32_t s_active[ZONE_COUNT];
static Zone s_drawZone = Zone::Frame;
static int64_t s_frameStart = 0;
static bool s_presented = false;

static ZoneWindow s_window[ZONE_COUNT];
static CounterWindow s_counterWindow[COUNTER_COUNT];
static std::vector<double> s_windowFrames;
static double s_windowCpu = 0;
static double s_windowGpu = 0;
static uint32_t s_windowGpuFrames = 0;
static uint32_t s_windowGpuDropped = 0;
static uint32_t s_windowHitches = 0;
static int64_t s_windowStart = 0;

static std::unordered_map<const CM2Shared*, ModelStat> s_models;
static ModelStat* s_currentModel = nullptr;
static std::unordered_map<uint32_t, OpcodeStat> s_opcodes;
static std::unordered_map<const CGxShader*, ShaderStat> s_pixelShaders;

static Published s_published;
static std::vector<Hitch> s_hitches;

static float s_history[HISTORY_SIZE];
static float s_historyGpu[HISTORY_SIZE];
static uint32_t s_historyIndex = 0;
static uint32_t s_historyGpuIndex = 0;

static bool s_gpuUnsupported = false;

static int64_t Now() {
    return Platform::Ticks();
}

static double ToMs(int64_t ticks) {
    return Platform::TicksToMs(ticks);
}

static void FormatClock(char* buffer, size_t size) {
    Platform::LocalTime t = Platform::LocalNow();
    snprintf(buffer, size, "%02d:%02d:%02d.%03d", t.m_hour, t.m_minute, t.m_second, t.m_millisecond);
}

static size_t Index(Zone zone) {
    return static_cast<size_t>(zone);
}

static uint32_t Depth(Zone zone) {
    uint32_t depth = 0;

    while (zone != Zone::Frame && depth < 16) {
        zone = s_zones[Index(zone)].m_parent;
        depth++;
    }

    return depth;
}

// GPU timestamps, see GpuTiming.h.

static void PushGpuHistory(float ms) {
    s_historyGpu[s_historyGpuIndex] = ms;
    s_historyGpuIndex = (s_historyGpuIndex + 1) % HISTORY_SIZE;
}

void GpuTiming::OnFrameTimed(double frameMs, const double* zoneMs) {
    for (size_t i = 0; i < ZONE_COUNT; i++) {
        s_window[i].m_gpu += zoneMs[i];
        s_window[i].m_gpuPeak = (std::max)(s_window[i].m_gpuPeak, zoneMs[i]);
    }

    s_window[Index(Zone::Frame)].m_gpu += frameMs;
    s_window[Index(Zone::Frame)].m_gpuPeak = (std::max)(s_window[Index(Zone::Frame)].m_gpuPeak, frameMs);
    s_windowGpu += frameMs;
    s_windowGpuFrames++;
    PushGpuHistory(static_cast<float>(frameMs));
}

void GpuTiming::OnFrameDropped() {
    s_windowGpuDropped++;
}

static void BeginGpuFrame() {
    if (!GpuTiming::BeginFrame(s_gpuTiming && !s_gpuUnsupported)) {
        s_gpuUnsupported = true;
        LOG_INFO << "graphics profiler: the device has no timestamp queries, GPU timing is off";
    }
}

static void EndGpuFrame() {
    GpuTiming::EndFrame();
}

static int32_t BeginGpuMark(Zone zone) {
    return GpuTiming::BeginMark(zone);
}

static void EndGpuMark(int32_t mark) {
    GpuTiming::EndMark(mark);
}

static void ReleaseGpu() {
    GpuTiming::Release();
}

// CPU zones.

int64_t Enter(Zone zone) {
    if (Platform::CurrentThreadId() != s_mainThread) {
        return 0;
    }

    size_t index = Index(zone);

    if (s_active[index] || s_depth >= STACK_DEPTH) {
        return 0;
    }

    s_active[index]++;

    StackEntry& entry = s_stack[s_depth++];
    entry.m_zone = zone;
    entry.m_drawZone = (s_zones[index].m_flags & ZONE_FLAT) ? s_drawZone : zone;
    entry.m_child = 0;
    entry.m_gpuMark = (s_zones[index].m_flags & ZONE_GPU) ? BeginGpuMark(zone) : -1;
    s_drawZone = entry.m_drawZone;

    int64_t start = Now();
    entry.m_start = start;

    return start ? start : 1;
}

void Leave(Zone zone, int64_t start) {
    int64_t now = Now();

    // A longjmp out of Lua can skip destructors, unwind whatever was left above this zone.
    while (s_depth && s_stack[s_depth - 1].m_zone != zone) {
        s_active[Index(s_stack[s_depth - 1].m_zone)] = 0;
        s_depth--;
    }

    if (!s_depth) {
        return;
    }

    StackEntry& entry = s_stack[--s_depth];
    size_t index = Index(zone);
    int64_t elapsed = now - entry.m_start;

    if (entry.m_gpuMark >= 0) {
        EndGpuMark(entry.m_gpuMark);
    }

    ZoneFrame& data = s_frame.m_zones[index];
    data.m_inclusive += elapsed;
    data.m_child += entry.m_child;
    data.m_calls++;
    s_active[index] = 0;

    if (s_depth) {
        s_stack[s_depth - 1].m_child += elapsed;
        s_drawZone = s_stack[s_depth - 1].m_drawZone;
    } else {
        s_drawZone = Zone::Frame;
    }
}

void AddCount(Counter counter, uint32_t amount) {
    s_frame.m_counters[static_cast<size_t>(counter)] += amount;
}

void OnDraw(uint32_t prims, uint32_t verts, bool indexed, const CGxShader* vertexShader, const CGxShader* pixelShader) {
    if (!g_enabled) {
        return;
    }

    s_frame.m_counters[static_cast<size_t>(Counter::Draws)]++;
    s_frame.m_counters[static_cast<size_t>(Counter::Prims)] += prims;
    s_frame.m_counters[static_cast<size_t>(Counter::Verts)] += verts;

    if (indexed) {
        s_frame.m_counters[static_cast<size_t>(Counter::DrawsIndexed)]++;
    }

    ZoneFrame& zone = s_frame.m_zones[Index(s_drawZone)];
    zone.m_draws++;
    zone.m_prims += prims;

    if (s_currentModel) {
        s_currentModel->m_draws++;
        s_currentModel->m_prims += prims;
    }

    if (s_shaderTop && pixelShader) {
        ShaderStat& stat = s_pixelShaders[pixelShader];

        if (stat.m_name.empty()) {
            const char* name = pixelShader->m_key.m_str;
            stat.m_name = name && *name ? name : "?";

            if (vertexShader && vertexShader->m_key.m_str) {
                stat.m_name += "  (vs ";
                stat.m_name += vertexShader->m_key.m_str;
                stat.m_name += ")";
            }
        }

        stat.m_draws++;
        stat.m_prims += prims;
    }
}

int64_t EnterModel(const CM2Model* model) {
    if (!s_modelTop || !model || !model->m_shared || Platform::CurrentThreadId() != s_mainThread) {
        return 0;
    }

    ModelStat& stat = s_models[model->m_shared];

    if (stat.m_name.empty()) {
        const char* name = model->m_shared->m_fileName;
        stat.m_name = name[0] ? name : "?";
    }

    stat.m_elements++;
    s_currentModel = &stat;

    int64_t start = Now();

    return start ? start : 1;
}

void LeaveModel(int64_t start) {
    if (s_currentModel) {
        s_currentModel->m_ticks += Now() - start;
        s_currentModel = nullptr;
    }
}

int64_t EnterMessage() {
    if (Platform::CurrentThreadId() != s_mainThread) {
        return 0;
    }

    int64_t start = Now();

    return start ? start : 1;
}

void LeaveMessage(uint32_t msgId, uint32_t bytes, int64_t start) {
    OpcodeStat& stat = s_opcodes[msgId];
    stat.m_ticks += Now() - start;
    stat.m_count++;
    stat.m_bytes += bytes;
    s_frame.m_counters[static_cast<size_t>(Counter::NetPackets)]++;
    s_frame.m_counters[static_cast<size_t>(Counter::NetBytes)] += bytes;
}

// Frame roll up.

static void SampleMemory(Published& out) {
    out.m_textureCount = 0;
    out.m_textureMB = 0;
    out.m_renderTargets = 0;
    out.m_renderTargetMB = 0;
    out.m_availableTexMB = -1;
    out.m_texCacheMB = -1;
    out.m_texCacheLimitMB = -1;

    if (CGxDevice* device = g_theGxDevicePtr) {
        uint64_t bytes = 0;
        uint64_t targetBytes = 0;

        for (CGxTex* tex = device->m_textures.Head(); tex; tex = device->m_textures.Next(tex)) {
            uint32_t size = device->ITexBitsSize(tex, 0xFFFFFFFF, 0xFFFFFFFF);

            if (tex->m_flags.m_renderTarget) {
                out.m_renderTargets++;
                targetBytes += size;
            } else {
                out.m_textureCount++;
                bytes += size;
            }
        }

        out.m_textureMB = bytes / (1024.0 * 1024.0);
        out.m_renderTargetMB = targetBytes / (1024.0 * 1024.0);

        GpuTiming::DeviceMemory(out.m_availableTexMB, out.m_texCacheMB, out.m_texCacheLimitMB);
    }

    uint64_t workingSet = 0;
    uint64_t privateBytes = 0;

    if (Platform::ProcessMemory(workingSet, privateBytes)) {
        out.m_workingSetMB = workingSet / (1024.0 * 1024.0);
        out.m_privateMB = privateBytes / (1024.0 * 1024.0);
    }
}

template <class Map, class Fill>
static std::vector<TopEntry> TopOf(const Map& map, Fill fill, double (*key)(const TopEntry&)) {
    std::vector<TopEntry> entries;
    entries.reserve(map.size());

    for (const auto& kv : map) {
        TopEntry entry;
        fill(kv, entry);
        entries.push_back(std::move(entry));
    }

    size_t keep = (std::min)(entries.size(), TOP_COUNT);
    std::partial_sort(entries.begin(), entries.begin() + keep, entries.end(), [key](const TopEntry& a, const TopEntry& b) {
        return key(a) > key(b);
    });
    entries.resize(keep);

    return entries;
}

static void Publish(int64_t now) {
    Published& out = s_published;
    uint32_t frames = static_cast<uint32_t>(s_windowFrames.size());

    out.m_seconds = ToMs(now - s_windowStart) / 1000.0;
    out.m_frames = frames;
    out.m_hitches = s_windowHitches;
    out.m_gpuFrames = s_windowGpuFrames;
    out.m_gpuDropped = s_windowGpuDropped;

    double inv = frames ? 1.0 / frames : 0.0;
    double total = 0;

    for (double ms : s_windowFrames) {
        total += ms;
    }

    out.m_frameMs = total * inv;
    out.m_cpuMs = s_windowCpu * inv;
    out.m_gpuMs = s_windowGpuFrames ? s_windowGpu / s_windowGpuFrames : 0.0;

    std::vector<double> sorted = s_windowFrames;
    std::sort(sorted.begin(), sorted.end());
    out.m_frameMax = sorted.empty() ? 0.0 : sorted.back();
    out.m_frameP99 = sorted.empty() ? 0.0 : sorted[(std::min)(sorted.size() - 1, static_cast<size_t>(sorted.size() * 0.99))];

    // 1% low is the average fps over the slowest 1% of frames, at least one frame.
    size_t slowCount = (std::max)(static_cast<size_t>(1), sorted.size() / 100);
    double slowTotal = 0;

    for (size_t i = 0; i < slowCount && i < sorted.size(); i++) {
        slowTotal += sorted[sorted.size() - 1 - i];
    }

    out.m_low1Fps = slowTotal > 0 ? 1000.0 * slowCount / slowTotal : 0.0;

    for (size_t i = 0; i < ZONE_COUNT; i++) {
        ZoneWindow zone = s_window[i];
        zone.m_inclusive *= inv;
        zone.m_self *= inv;
        zone.m_gpu = s_windowGpuFrames ? zone.m_gpu / s_windowGpuFrames : 0.0;
        out.m_zones[i] = zone;
    }

    for (size_t i = 0; i < COUNTER_COUNT; i++) {
        out.m_counters[i].m_sum = s_counterWindow[i].m_sum * inv;
        out.m_counters[i].m_peak = s_counterWindow[i].m_peak;
    }

    double perFrame = inv;

    out.m_models = TopOf(s_models, [perFrame](const auto& kv, TopEntry& entry) {
        entry.m_name = kv.second.m_name;
        entry.m_ms = ToMs(kv.second.m_ticks) * perFrame;
        entry.m_count = kv.second.m_elements;
        entry.m_draws = kv.second.m_draws;
        entry.m_prims = kv.second.m_prims;
    }, [](const TopEntry& e) { return e.m_ms; });

    out.m_opcodes = TopOf(s_opcodes, [perFrame](const auto& kv, TopEntry& entry) {
        char name[32];
        snprintf(name, sizeof(name), "0x%03X (%u)", kv.first, kv.first);
        entry.m_name = name;
        entry.m_ms = ToMs(kv.second.m_ticks) * perFrame;
        entry.m_count = kv.second.m_count;
        entry.m_bytes = kv.second.m_bytes;
    }, [](const TopEntry& e) { return e.m_ms; });

    out.m_shaders = TopOf(s_pixelShaders, [](const auto& kv, TopEntry& entry) {
        entry.m_name = kv.second.m_name;
        entry.m_draws = kv.second.m_draws;
        entry.m_prims = kv.second.m_prims;
    }, [](const TopEntry& e) { return static_cast<double>(e.m_draws); });

    for (TopEntry& entry : out.m_models) {
        entry.m_count = static_cast<uint64_t>(entry.m_count * perFrame + 0.5);
        entry.m_draws = static_cast<uint64_t>(entry.m_draws * perFrame + 0.5);
        entry.m_prims = static_cast<uint64_t>(entry.m_prims * perFrame + 0.5);
    }

    for (TopEntry& entry : out.m_shaders) {
        entry.m_draws = static_cast<uint64_t>(entry.m_draws * perFrame + 0.5);
        entry.m_prims = static_cast<uint64_t>(entry.m_prims * perFrame + 0.5);
    }

    SampleMemory(out);

    memset(s_window, 0, sizeof(s_window));
    memset(s_counterWindow, 0, sizeof(s_counterWindow));
    s_windowFrames.clear();
    s_windowCpu = 0;
    s_windowGpu = 0;
    s_windowGpuFrames = 0;
    s_windowGpuDropped = 0;
    s_windowHitches = 0;
    s_windowStart = now;
    s_models.clear();
    s_opcodes.clear();
    s_pixelShaders.clear();
}

static void CaptureHitch(double frameMs) {
    Hitch hitch;
    char clock[32];
    FormatClock(clock, sizeof(clock));
    hitch.m_clock = clock;
    hitch.m_frameMs = frameMs;

    for (size_t i = 0; i < ZONE_COUNT; i++) {
        const ZoneFrame& zone = s_frame.m_zones[i];
        hitch.m_zoneInclusive[i] = ToMs(zone.m_inclusive);
        hitch.m_zoneSelf[i] = ToMs(zone.m_inclusive - zone.m_child);
        hitch.m_zoneDraws[i] = zone.m_draws;
    }

    memcpy(hitch.m_counters, s_frame.m_counters, sizeof(hitch.m_counters));

    if (s_hitches.size() >= HITCH_KEEP) {
        s_hitches.erase(s_hitches.begin());
    }

    s_hitches.push_back(std::move(hitch));
}

static void EndFrame(int64_t now) {
    double frameMs = ToMs(now - s_frameStart);
    ZoneFrame& root = s_frame.m_zones[Index(Zone::Frame)];
    root.m_inclusive = now - s_frameStart;
    root.m_calls = 1;

    // Whatever the top level zones do not cover shows up as the frame's own self time.
    int64_t topLevel = 0;

    for (size_t i = 1; i < ZONE_COUNT; i++) {
        if (s_zones[i].m_parent == Zone::Frame && !(s_zones[i].m_flags & (ZONE_FLAT | ZONE_DETAIL))) {
            topLevel += s_frame.m_zones[i].m_inclusive;
        }
    }

    root.m_child = topLevel;

    for (size_t i = 0; i < ZONE_COUNT; i++) {
        const ZoneFrame& zone = s_frame.m_zones[i];
        ZoneWindow& window = s_window[i];
        double inclusive = ToMs(zone.m_inclusive);
        window.m_inclusive += inclusive;
        window.m_self += ToMs(zone.m_inclusive - zone.m_child);
        window.m_peak = (std::max)(window.m_peak, inclusive);
        window.m_calls += zone.m_calls;
        window.m_draws += zone.m_draws;
        window.m_prims += zone.m_prims;
    }

    for (size_t i = 0; i < COUNTER_COUNT; i++) {
        double value = static_cast<double>(s_frame.m_counters[i]);
        s_counterWindow[i].m_sum += value;
        s_counterWindow[i].m_peak = (std::max)(s_counterWindow[i].m_peak, value);
    }

    double waitMs = ToMs(s_frame.m_zones[Index(Zone::Wait)].m_inclusive);
    double swapMs = ToMs(s_frame.m_zones[Index(Zone::Swap)].m_inclusive);
    double capMs = ToMs(s_frame.m_zones[Index(Zone::FpsCap)].m_inclusive);
    s_windowCpu += (std::max)(0.0, frameMs - waitMs - swapMs - capMs);
    s_windowFrames.push_back(frameMs);

    s_history[s_historyIndex] = static_cast<float>(frameMs);
    s_historyIndex = (s_historyIndex + 1) % HISTORY_SIZE;

    if (s_hitchMs > 0 && frameMs >= s_hitchMs && s_windowStart) {
        s_windowHitches++;
        CaptureHitch(frameMs);
    }

    memset(&s_frame, 0, sizeof(s_frame));

    if (!s_windowStart) {
        s_windowStart = now;
    } else if (ToMs(now - s_windowStart) >= s_intervalSec * 1000.0) {
        Publish(now);
    }
}

void OnSchedulerTick() {
    if (!g_enabled || Platform::CurrentThreadId() != s_mainThread) {
        return;
    }

    int64_t now = Now();

    if (!s_frameStart) {
        s_frameStart = now;
        return;
    }

    if (!s_presented) {
        return;
    }

    s_presented = false;
    EndFrame(now);
    s_frameStart = now;
}

void OnPresentBegin() {
    if (g_enabled) {
        EndGpuFrame();
    }
}

void OnPresentEnd() {
    if (!g_enabled) {
        return;
    }

    s_presented = true;
    BeginGpuFrame();
}

void OnDeviceLost() {
    ReleaseGpu();
}

static void ResetAll() {
    memset(&s_frame, 0, sizeof(s_frame));
    memset(s_window, 0, sizeof(s_window));
    memset(s_counterWindow, 0, sizeof(s_counterWindow));
    memset(s_active, 0, sizeof(s_active));
    memset(s_history, 0, sizeof(s_history));
    memset(s_historyGpu, 0, sizeof(s_historyGpu));
    s_depth = 0;
    s_drawZone = Zone::Frame;
    s_frameStart = 0;
    s_presented = false;
    s_windowFrames.clear();
    s_windowCpu = 0;
    s_windowGpu = 0;
    s_windowGpuFrames = 0;
    s_windowGpuDropped = 0;
    s_windowHitches = 0;
    s_windowStart = 0;
    s_models.clear();
    s_opcodes.clear();
    s_pixelShaders.clear();
    s_currentModel = nullptr;
    s_published = Published();
}

static void SetEnabled(bool enabled) {
    if (enabled == g_enabled) {
        return;
    }

    if (enabled) {
        ResetAll();
        s_mainThread = Platform::CurrentThreadId();
        s_luaStatsWasOn = LuaRuntime::g_statsEnabled;
        LuaRuntime::g_statsEnabled = true;
        g_enabled = true;
        return;
    }

    g_enabled = false;
    LuaRuntime::g_statsEnabled = s_luaStatsWasOn;
    EndGpuFrame();
    ReleaseGpu();
    ResetAll();
}

bool IsOpen() {
    return s_open;
}

void SetOpen(bool open) {
    if (open && !UI::Load()) {
        return;
    }

    s_open = open;
    SetEnabled(open);
    UI::SetShown(open);
}

static void Toggle() {
    SetOpen(!s_open);
}

// Report file.

static std::string ReportPath() {
    std::filesystem::path directory = Platform::GameDirectory() / "Logs";
    std::error_code ec;
    std::filesystem::create_directory(directory, ec);

    time_t now = time(nullptr);
    tm local = *localtime(&now);
    char name[64];
    strftime(name, sizeof(name), "gfxprofile_%Y-%m-%d_%H-%M-%S.txt", &local);

    return (directory / name).string();
}

static void WriteZoneTree(FILE* file, const Published& data) {
    fprintf(file, "\n%-48s %9s %9s %9s %9s %8s %8s %10s\n", "zone (ms per frame)", "total", "self", "peak", "gpu", "calls", "draws", "prims");

    for (size_t i = 0; i < ZONE_COUNT; i++) {
        const ZoneWindow& zone = data.m_zones[i];

        if (s_zones[i].m_flags & (ZONE_FLAT | ZONE_DETAIL)) {
            continue;
        }

        char name[64];
        snprintf(name, sizeof(name), "%*s%s", Depth(static_cast<Zone>(i)) * 2, "", s_zones[i].m_name);
        double calls = data.m_frames ? static_cast<double>(zone.m_calls) / data.m_frames : 0.0;
        double draws = data.m_frames ? static_cast<double>(zone.m_draws) / data.m_frames : 0.0;
        double prims = data.m_frames ? static_cast<double>(zone.m_prims) / data.m_frames : 0.0;
        fprintf(file, "%-48s %9.3f %9.3f %9.3f %9.3f %8.1f %8.1f %10.0f\n", name, zone.m_inclusive, zone.m_self, zone.m_peak, zone.m_gpu, calls, draws, prims);
    }

    fprintf(file, "\n%-48s %9s %9s %8s %8s\n", "hot spots (ms per frame)", "total", "peak", "calls", "draws");

    for (size_t i = 0; i < ZONE_COUNT; i++) {
        if (!(s_zones[i].m_flags & (ZONE_FLAT | ZONE_DETAIL))) {
            continue;
        }

        const ZoneWindow& zone = data.m_zones[i];
        double calls = data.m_frames ? static_cast<double>(zone.m_calls) / data.m_frames : 0.0;
        double draws = data.m_frames ? static_cast<double>(zone.m_draws) / data.m_frames : 0.0;
        fprintf(file, "%-48s %9.3f %9.3f %8.1f %8.1f\n", s_zones[i].m_name, zone.m_inclusive, zone.m_peak, calls, draws);
    }
}

static void WriteTop(FILE* file, const char* title, const std::vector<TopEntry>& entries, bool timed, bool bytes) {
    fprintf(file, "\n%s\n", title);

    for (const TopEntry& entry : entries) {
        if (bytes) {
            fprintf(file, "  %8.3f ms  %6llu msgs  %8.1f KB  %s\n", entry.m_ms, static_cast<unsigned long long>(entry.m_count), entry.m_bytes / 1024.0, entry.m_name.c_str());
        } else if (timed) {
            fprintf(file, "  %8.3f ms  %5llu elements  %6llu draws  %9llu prims  %s\n", entry.m_ms, static_cast<unsigned long long>(entry.m_count), static_cast<unsigned long long>(entry.m_draws), static_cast<unsigned long long>(entry.m_prims), entry.m_name.c_str());
        } else {
            fprintf(file, "  %6llu draws  %9llu prims  %s\n", static_cast<unsigned long long>(entry.m_draws), static_cast<unsigned long long>(entry.m_prims), entry.m_name.c_str());
        }
    }
}

static std::string WriteReport() {
    std::string path = ReportPath();
    FILE* file = fopen(path.c_str(), "w");

    if (!file) {
        return std::string();
    }

    const Published& data = s_published;
    char clock[32];
    FormatClock(clock, sizeof(clock));

    const char* api = "?";

    if (g_theGxDevicePtr) {
        switch (g_theGxDevicePtr->DeviceApi()) {
            case GxApi_D3d9:
                api = "D3D9";
                break;
            case GxApi_D3d9Ex:
                api = "D3D9Ex";
                break;
            case GxApi_OpenGl:
                api = "OpenGL";
                break;
            case GxApi_Metal:
                api = "Metal";
                break;
            default:
                break;
        }
    }

    fprintf(file, "HoT graphics profile, %s, %s, %.2f s window, %u frames\n", clock, api, data.m_seconds, data.m_frames);
    fprintf(file, "fps %.1f, frame %.2f ms, p99 %.2f ms, max %.2f ms, 1%% low %.1f fps, hitches %u\n", data.m_frameMs > 0 ? 1000.0 / data.m_frameMs : 0.0, data.m_frameMs, data.m_frameP99, data.m_frameMax, data.m_low1Fps, data.m_hitches);
    fprintf(file, "cpu busy %.2f ms, gpu %.2f ms (%u frames timed, %u dropped)\n", data.m_cpuMs, data.m_gpuMs, data.m_gpuFrames, data.m_gpuDropped);
    fprintf(file, "textures %u (%.1f MB), render targets %u (%.1f MB), available texture memory %.0f MB, working set %.0f MB, private %.0f MB\n", data.m_textureCount, data.m_textureMB, data.m_renderTargets, data.m_renderTargetMB, data.m_availableTexMB, data.m_workingSetMB, data.m_privateMB);

    WriteZoneTree(file, data);

    fprintf(file, "\n%-12s %-32s %12s %12s\n", "group", "counter (per frame)", "avg", "peak");

    for (size_t i = 0; i < COUNTER_COUNT; i++) {
        double divisor = (i == static_cast<size_t>(Counter::BufLockBytes) || i == static_cast<size_t>(Counter::TexUploadBytes) || i == static_cast<size_t>(Counter::NetBytes)) ? 1024.0 : 1.0;
        fprintf(file, "%-12s %-32s %12.1f %12.0f\n", s_counters[i].m_group, s_counters[i].m_name, data.m_counters[i].m_sum / divisor, data.m_counters[i].m_peak / divisor);
    }

    WriteTop(file, "M2 models by CPU draw time (per frame)", data.m_models, true, false);
    WriteTop(file, "Pixel shaders by draw calls (per frame)", data.m_shaders, false, false);
    WriteTop(file, "Network opcodes by handler time (per window)", data.m_opcodes, true, true);

    for (auto it = s_hitches.rbegin(); it != s_hitches.rend(); ++it) {
        const Hitch& hitch = *it;
        fprintf(file, "\nhitch at %s, %.2f ms\n", hitch.m_clock.c_str(), hitch.m_frameMs);

        for (size_t i = 0; i < ZONE_COUNT; i++) {
            if (hitch.m_zoneInclusive[i] < 0.05) {
                continue;
            }

            char name[64];
            bool flat = (s_zones[i].m_flags & (ZONE_FLAT | ZONE_DETAIL)) != 0;
            snprintf(name, sizeof(name), "%*s%s%s", flat ? 0 : Depth(static_cast<Zone>(i)) * 2, "", flat ? "* " : "", s_zones[i].m_name);
            fprintf(file, "  %-48s %9.3f total %9.3f self %6u draws\n", name, hitch.m_zoneInclusive[i], hitch.m_zoneSelf[i], hitch.m_zoneDraws[i]);
        }

        for (size_t i = 0; i < COUNTER_COUNT; i++) {
            if (hitch.m_counters[i]) {
                fprintf(file, "  %-32s %llu\n", s_counters[i].m_name, static_cast<unsigned long long>(hitch.m_counters[i]));
            }
        }
    }

    fprintf(file, "\nlast %u frame times, oldest first, cpu wall ms / gpu ms\n", static_cast<uint32_t>(HISTORY_SIZE));

    for (size_t i = 0; i < HISTORY_SIZE; i++) {
        size_t index = (s_historyIndex + i) % HISTORY_SIZE;
        size_t gpuIndex = (s_historyGpuIndex + i) % HISTORY_SIZE;
        fprintf(file, "%.2f/%.2f%s", s_history[index], s_historyGpu[gpuIndex], (i % 12 == 11) ? "\n" : "  ");
    }

    fclose(file);
    LOG_INFO << "graphics profile written to " << path;

    return path;
}

// Lua API, every getter refills the table it is given so the UI does not churn garbage.

static void SetNumber(lua_State* L, const char* key, double value) {
    lua_pushnumber(L, value);
    lua_setfield(L, -2, key);
}

static void SetString(lua_State* L, const char* key, const char* value) {
    lua_pushstring(L, value);
    lua_setfield(L, -2, key);
}

static void SetBool(lua_State* L, const char* key, bool value) {
    lua_pushboolean(L, value ? 1 : 0);
    lua_setfield(L, -2, key);
}

// Leaves row i of the table at index 1 on the stack, creating it when missing.
static void PushRow(lua_State* L, int32_t i) {
    lua_rawgeti(L, 1, i);

    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_rawseti(L, 1, i);
    }
}

static void ClearRowsFrom(lua_State* L, int32_t first) {
    for (int32_t i = first;; i++) {
        lua_rawgeti(L, 1, i);
        bool present = !lua_isnil(L, -1);
        lua_pop(L, 1);

        if (!present) {
            break;
        }

        lua_pushnil(L);
        lua_rawseti(L, 1, i);
    }
}

static bool CheckTable(lua_State* L) {
    if (!lua_istable(L, 1)) {
        luaL_error(L, "Usage: needs a table to fill");
        return false;
    }

    return true;
}

static int32_t Script_GetSummary(lua_State* L) {
    if (!CheckTable(L)) {
        return 0;
    }

    const Published& data = s_published;
    lua_pushvalue(L, 1);
    SetNumber(L, "seconds", data.m_seconds);
    SetNumber(L, "frames", data.m_frames);
    SetNumber(L, "fps", data.m_frameMs > 0 ? 1000.0 / data.m_frameMs : 0.0);
    SetNumber(L, "frameMs", data.m_frameMs);
    SetNumber(L, "frameMax", data.m_frameMax);
    SetNumber(L, "frameP99", data.m_frameP99);
    SetNumber(L, "low1Fps", data.m_low1Fps);
    SetNumber(L, "cpuMs", data.m_cpuMs);
    SetNumber(L, "gpuMs", data.m_gpuMs);
    SetNumber(L, "gpuFrames", data.m_gpuFrames);
    SetNumber(L, "gpuDropped", data.m_gpuDropped);
    SetBool(L, "gpuTiming", s_gpuTiming && !s_gpuUnsupported && GpuTiming::Available());
    SetNumber(L, "hitches", data.m_hitches);
    SetNumber(L, "hitchMs", s_hitchMs);
    SetNumber(L, "waitMs", data.m_zones[Index(Zone::Wait)].m_inclusive);
    SetNumber(L, "swapMs", data.m_zones[Index(Zone::Swap)].m_inclusive);
    SetNumber(L, "capMs", data.m_zones[Index(Zone::FpsCap)].m_inclusive);
    SetNumber(L, "textures", data.m_textureCount);
    SetNumber(L, "textureMB", data.m_textureMB);
    SetNumber(L, "renderTargets", data.m_renderTargets);
    SetNumber(L, "renderTargetMB", data.m_renderTargetMB);
    SetNumber(L, "availableTexMB", data.m_availableTexMB);
    SetNumber(L, "texCacheMB", data.m_texCacheMB);
    SetNumber(L, "texCacheLimitMB", data.m_texCacheLimitMB);
    SetNumber(L, "workingSetMB", data.m_workingSetMB);
    SetNumber(L, "privateMB", data.m_privateMB);
    SetNumber(L, "draws", data.m_counters[static_cast<size_t>(Counter::Draws)].m_sum);
    SetNumber(L, "prims", data.m_counters[static_cast<size_t>(Counter::Prims)].m_sum);

    const char* api = "?";

    if (g_theGxDevicePtr) {
        EGxApi deviceApi = g_theGxDevicePtr->DeviceApi();
        api = deviceApi == GxApi_D3d9 ? "D3D9" : deviceApi == GxApi_D3d9Ex ? "D3D9Ex" : deviceApi == GxApi_OpenGl ? "OpenGL" : deviceApi == GxApi_Metal ? "Metal" : "?";
    }

    SetString(L, "api", api);
    lua_pop(L, 1);

    return 0;
}

static int32_t Script_GetZones(lua_State* L) {
    if (!CheckTable(L)) {
        return 0;
    }

    const Published& data = s_published;
    double frames = data.m_frames ? static_cast<double>(data.m_frames) : 1.0;
    int32_t row = 0;

    for (size_t i = 0; i < ZONE_COUNT; i++) {
        const ZoneWindow& zone = data.m_zones[i];
        uint8_t flags = s_zones[i].m_flags;

        PushRow(L, ++row);
        SetString(L, "name", s_zones[i].m_name);
        SetNumber(L, "id", static_cast<double>(i));
        SetNumber(L, "parent", static_cast<double>(Index(s_zones[i].m_parent)));
        SetNumber(L, "depth", (flags & (ZONE_FLAT | ZONE_DETAIL)) ? 0 : Depth(static_cast<Zone>(i)));
        SetBool(L, "hotspot", (flags & (ZONE_FLAT | ZONE_DETAIL)) != 0);
        SetBool(L, "gpuZone", (flags & ZONE_GPU) != 0);
        SetNumber(L, "ms", zone.m_inclusive);
        SetNumber(L, "self", zone.m_self);
        SetNumber(L, "peak", zone.m_peak);
        SetNumber(L, "gpu", zone.m_gpu);
        SetNumber(L, "gpuPeak", zone.m_gpuPeak);
        SetNumber(L, "calls", zone.m_calls / frames);
        SetNumber(L, "draws", zone.m_draws / frames);
        SetNumber(L, "prims", zone.m_prims / frames);
        lua_pop(L, 1);
    }

    ClearRowsFrom(L, row + 1);
    lua_pushnumber(L, row);

    return 1;
}

static int32_t Script_GetCounters(lua_State* L) {
    if (!CheckTable(L)) {
        return 0;
    }

    const Published& data = s_published;
    int32_t row = 0;

    for (size_t i = 0; i < COUNTER_COUNT; i++) {
        bool kb = i == static_cast<size_t>(Counter::BufLockBytes) || i == static_cast<size_t>(Counter::TexUploadBytes) || i == static_cast<size_t>(Counter::NetBytes);
        double divisor = kb ? 1024.0 : 1.0;

        PushRow(L, ++row);
        SetString(L, "group", s_counters[i].m_group);
        SetString(L, "name", s_counters[i].m_name);
        SetNumber(L, "avg", data.m_counters[i].m_sum / divisor);
        SetNumber(L, "peak", data.m_counters[i].m_peak / divisor);
        lua_pop(L, 1);
    }

    ClearRowsFrom(L, row + 1);
    lua_pushnumber(L, row);

    return 1;
}

static int32_t Script_GetTop(lua_State* L) {
    if (!CheckTable(L)) {
        return 0;
    }

    const char* kind = luaL_checkstring(L, 2);
    const std::vector<TopEntry>* entries = &s_published.m_models;

    if (!strcmp(kind, "opcodes")) {
        entries = &s_published.m_opcodes;
    } else if (!strcmp(kind, "shaders")) {
        entries = &s_published.m_shaders;
    }

    int32_t row = 0;

    for (const TopEntry& entry : *entries) {
        PushRow(L, ++row);
        SetString(L, "name", entry.m_name.c_str());
        SetNumber(L, "ms", entry.m_ms);
        SetNumber(L, "count", static_cast<double>(entry.m_count));
        SetNumber(L, "draws", static_cast<double>(entry.m_draws));
        SetNumber(L, "prims", static_cast<double>(entry.m_prims));
        SetNumber(L, "kb", entry.m_bytes / 1024.0);
        lua_pop(L, 1);
    }

    ClearRowsFrom(L, row + 1);
    lua_pushnumber(L, row);

    return 1;
}

static int32_t Script_GetHistory(lua_State* L) {
    if (!CheckTable(L)) {
        return 0;
    }

    bool gpu = lua_toboolean(L, 2) != 0;
    const float* values = gpu ? s_historyGpu : s_history;
    uint32_t start = gpu ? s_historyGpuIndex : s_historyIndex;

    for (uint32_t i = 0; i < HISTORY_SIZE; i++) {
        lua_pushnumber(L, values[(start + i) % HISTORY_SIZE]);
        lua_rawseti(L, 1, static_cast<int32_t>(i + 1));
    }

    lua_pushnumber(L, HISTORY_SIZE);

    return 1;
}

static int32_t Script_GetHitch(lua_State* L) {
    if (!CheckTable(L)) {
        return 0;
    }

    int32_t back = static_cast<int32_t>(luaL_optnumber(L, 2, 1));

    if (back < 1 || back > static_cast<int32_t>(s_hitches.size())) {
        ClearRowsFrom(L, 1);
        lua_pushnumber(L, 0);
        return 1;
    }

    const Hitch& hitch = s_hitches[s_hitches.size() - back];
    std::vector<size_t> order;

    for (size_t i = 1; i < ZONE_COUNT; i++) {
        if (hitch.m_zoneSelf[i] >= 0.05) {
            order.push_back(i);
        }
    }

    std::sort(order.begin(), order.end(), [&hitch](size_t a, size_t b) {
        return hitch.m_zoneSelf[a] > hitch.m_zoneSelf[b];
    });

    int32_t row = 0;

    for (size_t i : order) {
        PushRow(L, ++row);
        SetString(L, "name", s_zones[i].m_name);
        SetNumber(L, "ms", hitch.m_zoneInclusive[i]);
        SetNumber(L, "self", hitch.m_zoneSelf[i]);
        SetNumber(L, "draws", hitch.m_zoneDraws[i]);
        SetBool(L, "hotspot", (s_zones[i].m_flags & (ZONE_FLAT | ZONE_DETAIL)) != 0);
        lua_pop(L, 1);
    }

    ClearRowsFrom(L, row + 1);
    lua_pushnumber(L, row);
    lua_pushnumber(L, hitch.m_frameMs);
    lua_pushstring(L, hitch.m_clock.c_str());
    lua_pushnumber(L, static_cast<double>(s_hitches.size()));
    lua_pushnumber(L, static_cast<double>(hitch.m_counters[static_cast<size_t>(Counter::TexUploads)]));
    lua_pushnumber(L, static_cast<double>(hitch.m_counters[static_cast<size_t>(Counter::ModelCreates)]));
    lua_pushnumber(L, static_cast<double>(hitch.m_counters[static_cast<size_t>(Counter::LoadWaits)]));

    return 7;
}

static int32_t Script_Dump(lua_State* L) {
    std::string path = WriteReport();

    if (path.empty()) {
        return 0;
    }

    lua_pushstring(L, path.c_str());

    return 1;
}

static int32_t Script_Toggle(lua_State* L) {
    Toggle();
    lua_pushboolean(L, s_open ? 1 : 0);

    return 1;
}

static int32_t Script_IsOpen(lua_State* L) {
    lua_pushboolean(L, s_open ? 1 : 0);

    return 1;
}

static int32_t Script_ClearHitches(lua_State* L) {
    s_hitches.clear();

    return 0;
}

static void OnGpuTimingChanged(const char* value) {
    s_gpuTiming = CVars::Bool(value);
    s_gpuUnsupported = false;

    if (!s_gpuTiming) {
        EndGpuFrame();
        ReleaseGpu();
    }
}

static void OnIntervalChanged(const char* value) {
    s_intervalSec = std::clamp(CVars::Float(value), 0.1f, 10.0f);
}

static void OnHitchChanged(const char* value) {
    s_hitchMs = CVars::Float(value);
}

static void OnModelTopChanged(const char* value) {
    s_modelTop = CVars::Bool(value);
}

static void OnShaderTopChanged(const char* value) {
    s_shaderTop = CVars::Bool(value);
}

class ProfilerModule : public Module {
    public:
    const char* Name() const override {
        return "Profiler";
    }

    void OnInitializeGlobal() override {
        s_mainThread = Platform::CurrentThreadId();

        Hotkeys::Register("profilerToggleKey", "Key that opens and closes the graphics profiler, NONE turns it off", "F11", Toggle);
        CVars::Register("profilerGpuTiming", "Time the main render passes on the GPU with D3D9 timestamp queries while the graphics profiler is open", "1", OnGpuTimingChanged);
        CVars::Register("profilerInterval", "Seconds of frames the graphics profiler averages before it refreshes", "0.5", OnIntervalChanged);
        CVars::Register("profilerHitchMs", "Frames at least this long are captured as hitches with a full zone breakdown, 0 turns capture off", "50", OnHitchChanged);
        CVars::Register("profilerModels", "Track CPU draw time per M2 file for the model top list", "1", OnModelTopChanged);
        CVars::Register("profilerShaders", "Track draw calls per pixel shader for the shader top list", "1", OnShaderTopChanged);

        Lua::RegisterFunction("GraphicsProfiler_GetSummary", Script_GetSummary, Lua::State::Any);
        Lua::RegisterFunction("GraphicsProfiler_GetZones", Script_GetZones, Lua::State::Any);
        Lua::RegisterFunction("GraphicsProfiler_GetCounters", Script_GetCounters, Lua::State::Any);
        Lua::RegisterFunction("GraphicsProfiler_GetTop", Script_GetTop, Lua::State::Any);
        Lua::RegisterFunction("GraphicsProfiler_GetHistory", Script_GetHistory, Lua::State::Any);
        Lua::RegisterFunction("GraphicsProfiler_GetHitch", Script_GetHitch, Lua::State::Any);
        Lua::RegisterFunction("GraphicsProfiler_ClearHitches", Script_ClearHitches, Lua::State::Any);
        Lua::RegisterFunction("GraphicsProfiler_Dump", Script_Dump, Lua::State::Any);
        Lua::RegisterFunction("GraphicsProfiler_Toggle", Script_Toggle, Lua::State::Any);
        Lua::RegisterFunction("GraphicsProfiler_IsOpen", Script_IsOpen, Lua::State::Any);
    }

    void OnScriptsLoaded(Lua::State state) override {
        UI::OnScriptsReloaded();

        if (s_open && !UI::Load()) {
            s_open = false;
            SetEnabled(false);
        } else if (s_open) {
            UI::SetShown(true);
        }
    }
};

Module* GetModule() {
    static ProfilerModule module;
    return &module;
}

}
