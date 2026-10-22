// GPU resources of the Metal graphics device (Objective-C++ only): buffers for CGxPool, textures
// for CGxTex and the window's targets, and queries for CGxQuery.
//
// GPU work is ordered by serials: every command buffer the context commits has one, and a resource
// records the serial of the last command buffer that used it. Comparing that with the serial of
// the last completed command buffer tells whether the CPU may write the resource's memory
// directly or must stage the write and copy it on the GPU in order.
//
// The objects are reference counted because the device keeps the ones bound to its state (vertex
// streams, textures, render targets) alive while the client destroys and recreates its own handles.

#ifndef ENGINE_SOURCE_GX_CGXDEVICEMETAL_METALRESOURCES_H
#define ENGINE_SOURCE_GX_CGXDEVICEMETAL_METALRESOURCES_H

#import <Metal/Metal.h>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>
#include "Engine/Source/Gx/CGxDeviceMetal/MetalFormats.h"
#include "Engine/Source/Gx/CGxDeviceMetal/Shader/ShaderTranslator.h"

namespace GxMetal {

class Context;

class RefCounted {
    public:
    void Retain() { this->m_refCount++; }
    void Release() {
        if (--this->m_refCount == 0) {
            delete this;
        }
    }

    protected:
    RefCounted() = default;
    virtual ~RefCounted() = default;

    private:
    uint32_t m_refCount = 1;

    RefCounted(const RefCounted&) = delete;
    RefCounted& operator=(const RefCounted&) = delete;
};

// Replaces a retained pointer
template <typename T>
void Assign(T*& slot, T* value) {
    if (slot == value) {
        return;
    }
    if (value) {
        value->Retain();
    }
    if (slot) {
        slot->Release();
    }
    slot = value;
}

template <typename T>
void Clear(T*& slot) {
    if (slot) {
        slot->Release();
        slot = nullptr;
    }
}

// Transient GPU memory: constants, per-draw state and staged uploads. Allocations stay valid
// until the command buffer of the serial they were made for completes.
struct UploadAllocation {
    id<MTLBuffer> m_buffer = nil;
    uint32_t m_offset = 0;
    uint8_t* m_data = nullptr;
};

class UploadHeap {
    public:
    void Init(id<MTLDevice> device);
    UploadAllocation Allocate(uint32_t size, uint32_t alignment, uint64_t serial, uint64_t completed);

    private:
    struct Chunk {
        id<MTLBuffer> m_buffer;
        uint32_t m_size;
        uint32_t m_used;
        uint64_t m_lastSerial;
    };

    id<MTLDevice> m_device = nil;
    std::vector<Chunk> m_chunks;
    size_t m_current = SIZE_MAX;
};

struct GpuUse {
    // Last command buffer that read or wrote the resource on the GPU
    uint64_t m_lastUse = 0;
    // Last render command buffer that read it in a draw
    uint64_t m_lastRenderUse = 0;
};

// How the client locks a pool (CGxDeviceD3d::IBufLock): stream pools append without
// overwriting what the GPU may still read, and discard when full; dynamic pools never overwrite
// data in flight; static pools are written whenever the GPU is done with them
enum BufferLock {
    BufferLock_Normal,
    BufferLock_NoOverwrite,
    BufferLock_Discard,
};

// The memory of a CGxPool: a vertex or index buffer
class Buffer : public RefCounted {
    public:
    Buffer(Context* context, uint32_t length, bool dynamic, bool keepShadow);

    bool Valid() const { return this->m_buffer != nil; }
    char* Lock(uint32_t offset, uint32_t size, BufferLock lock);
    void Unlock();

    Context* m_context;
    uint32_t m_length;
    bool m_dynamic;
    id<MTLBuffer> m_buffer = nil;
    GpuUse m_use;
    // Copy of the contents for CPU-side primitive conversion (triangle fans of index buffers)
    std::vector<uint8_t> m_shadow;

    private:
    struct LockRecord {
        uint32_t m_offset;
        uint32_t m_size;
        uint8_t* m_data;
        bool m_staged;
        UploadAllocation m_staging;
    };

    // Buffers renamed away by a discarding lock, reusable once their last use completes
    std::vector<std::pair<id<MTLBuffer>, uint64_t>> m_spares;
    std::vector<LockRecord> m_locks;
};

// A texture of a CGxTex, or one of the window's targets (back buffer, depth buffer). A
// multisampled target renders into m_texture and is read through its single-sample resolve.
class Texture : public RefCounted {
    public:
    // A sampled texture (2D or cube), optionally a render target and with generated mipmaps
    static Texture* Create(Context* context, MTLTextureType type, uint32_t width, uint32_t height, uint32_t levels, const FormatInfo& format, bool renderTarget, bool autoMips);
    // A window target: render target (and shader readable), possibly multisampled
    static Texture* CreateTarget(Context* context, const FormatInfo& format, uint32_t width, uint32_t height, uint32_t samples);

    // The texture to sample: a swizzled view where the format needs one
    id<MTLTexture> SampleTexture() const { return this->m_view ? this->m_view : this->m_texture; }
    // The single-sample texture holding the contents, resolving multisampled colour first
    id<MTLTexture> ResolvedTexture();
    SamplerKind Kind() const;
    uint32_t LevelWidth(uint32_t level) const { return std::max(1u, this->m_width >> level); }
    uint32_t LevelHeight(uint32_t level) const { return std::max(1u, this->m_height >> level); }
    // Rendering or a write into a level makes the resolve and the generated mipmaps stale
    void MarkWritten(uint32_t level);

    Context* m_context;
    FormatInfo m_format;
    MTLTextureType m_type = MTLTextureType2D;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_levels = 1;
    uint32_t m_samples = 1;
    bool m_renderTarget = false;
    bool m_autoMips = false;
    id<MTLTexture> m_texture = nil;
    id<MTLTexture> m_view = nil;
    // Single-sample resolve of a multisampled target (colour, or depth for readable depth)
    id<MTLTexture> m_resolve = nil;
    bool m_resolveDirty = false;
    bool m_mipsDirty = false;
    GpuUse m_use;

    private:
    explicit Texture(Context* context) : m_context(context) {}
};

// A level and face of a texture, as a render target, copy source or destination
struct Target {
    Texture* m_texture = nullptr;
    uint32_t m_face = 0;
    uint32_t m_level = 0;

    bool operator==(const Target& other) const { return this->m_texture == other.m_texture && this->m_face == other.m_face && this->m_level == other.m_level; }
    bool operator!=(const Target& other) const { return !(*this == other); }
    uint32_t Width() const { return this->m_texture->LevelWidth(this->m_level); }
    uint32_t Height() const { return this->m_texture->LevelHeight(this->m_level); }
};

// An occlusion query counts the samples that pass in the render passes between its begin and
// end; an event query completes with the command buffer that was current when it ended
class Query : public RefCounted {
    public:
    enum Type {
        Type_Occlusion,
        Type_Event,
    };

    enum State {
        State_Idle,
        State_Building,
        State_Issued,
    };

    struct Slot {
        id<MTLBuffer> m_buffer;
        uint32_t m_offset;
    };

    Query(Context* context, Type type) : m_context(context), m_type(type) {}
    ~Query() override;

    void Begin();
    void End();
    // False while the result is not available yet; flush submits the command buffer it waits for
    bool Result(uint32_t* samples, bool flush);

    Context* m_context;
    Type m_type;
    State m_state = State_Idle;
    uint64_t m_serial = 0;
    std::vector<Slot> m_slots;
};

}

#endif
