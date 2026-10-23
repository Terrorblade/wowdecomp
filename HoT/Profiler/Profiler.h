#ifndef HOT_PROFILER_PROFILER_H
#define HOT_PROFILER_PROFILER_H

#include <cstdint>

class CGxShader;
class CM2Model;

namespace HoT {
class Module;
}

// Graphics profiler behind F11: nested CPU zones, GPU timestamps, Gx and scene counters, hitch captures.
namespace HoT::Profiler {

// Declaration order is the display order, the parent of each zone lives in the table in Profiler.cpp.
enum class Zone : uint16_t {
    Frame,
    Wait,
    Update,
    Input,
    Poll,
    AsyncFile,
    TexturePoll,
    Sound,
    Net,
    CharComponents,
    Queue,
    Idle,
    Movement,
    GameUiIdle,
    SpellIdle,
    HoTModules,
    Paint,
    UiUpdate,
    UiBuild,
    UiDraw,
    WorldUpdate,
    ObjectEffects,
    UnitUpdate,
    WorldSceneUpdate,
    MapPrepare,
    WorldText,
    WorldRender,
    SceneCull,
    ObjectAnimate,
    M2Animate,
    Shadows,
    Terrain,
    MapObjects,
    Horizon,
    Sky,
    Entities,
    Liquids,
    Nameplates,
    M2Opaque,
    DetailDoodads,
    M2Transparent,
    Weather,
    SpellVisuals,
    PostEffects,
    LuaGc,
    FontUpdate,
    Present,
    PresentFlush,
    GpuSync,
    FpsCap,
    Swap,
    LuaHandlers,
    StateSync,
    TexCreate,
    TexUpload,
    BufLock,
    ShaderCreate,
    ModelCreate,
    LoadWait,
    ParticleUpdate,
    RibbonUpdate,
    Particles,
    Ribbons,
    MoveApply,
    MoveWorldUpdate,
    M2AnimateThreads,
    M2AnimateMain,
    M2ModelLighting,
    CollideWmo,
    CollideTerrain,
    CollideTrace,
    ParticlePrebuild,
    CollideWmoBsp,
    CollideWmoTriFacets,
    CollideDoodads,
    GxDrawSubmit,
    M2AdvanceTime,
    M2BuildElements,
    M2SortElements,
    Count,
};

enum class Counter : uint16_t {
    Draws,
    DrawsIndexed,
    Prims,
    Verts,
    StateSyncs,
    RenderStates,
    TexBinds,
    VertexShaderBinds,
    PixelShaderBinds,
    ConstantFlushes,
    BufLocks,
    BufLockBytes,
    BufDiscards,
    PoolResizes,
    TexCreates,
    TexUploads,
    TexUploadBytes,
    TexDestroys,
    RenderTargets,
    Clears,
    ShaderCreates,
    NetPackets,
    NetBytes,
    LuaCalls,
    MapChunks,
    MapObjGroups,
    Doodads,
    M2Animated,
    M2Drawn,
    M2Elements,
    ParticleEmitters,
    Ribbons,
    LiquidInstances,
    DetailDoodads,
    UnitsUpdated,
    PlayersVisible,
    PlayersHidden,
    ModelCreates,
    LoadWaits,
    UnitsMoved,
    CollisionQueries,
    BspDigestBuilds,
    DoodadMeshBuilds,
    Count,
};

extern bool g_enabled;

int64_t Enter(Zone zone);
void Leave(Zone zone, int64_t start);
void AddCount(Counter counter, uint32_t amount);

inline void Count(Counter counter, uint32_t amount = 1) {
    if (g_enabled) {
        AddCount(counter, amount);
    }
}

// Times a scope into a zone while the profiler is open, a zone nested in itself counts once.
class Scope {
    public:
    explicit Scope(Zone zone) : m_zone(zone), m_start(g_enabled ? Enter(zone) : 0) {}

    ~Scope() {
        if (this->m_start) {
            Leave(this->m_zone, this->m_start);
        }
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

    // Ends the current zone and starts timing the next one.
    void Next(Zone zone) {
        if (this->m_start) {
            Leave(this->m_zone, this->m_start);
        }

        this->m_zone = zone;
        this->m_start = g_enabled ? Enter(zone) : 0;
    }

    private:
    Zone m_zone;
    int64_t m_start;
};

// Gx device hooks.
void OnDraw(uint32_t prims, uint32_t verts, bool indexed, const CGxShader* vertexShader, const CGxShader* pixelShader);
void OnPresentBegin();
void OnPresentEnd();
void OnDeviceLost();

// Top of every scheduler pass, closes the frame once a present happened.
void OnSchedulerTick();

// Per opcode handler timing for the network top list.
int64_t EnterMessage();
void LeaveMessage(uint32_t msgId, uint32_t bytes, int64_t start);

int64_t EnterModel(const CM2Model* model);
void LeaveModel(int64_t start);

// Attributes the draw calls and CPU time of one model element to its file in the model top list.
class ModelScope {
    public:
    explicit ModelScope(const CM2Model* model) : m_start(g_enabled ? EnterModel(model) : 0) {}

    ~ModelScope() {
        if (this->m_start) {
            LeaveModel(this->m_start);
        }
    }

    ModelScope(const ModelScope&) = delete;
    ModelScope& operator=(const ModelScope&) = delete;

    private:
    int64_t m_start;
};

bool IsOpen();
void SetOpen(bool open);

Module* GetModule();

}

#endif
