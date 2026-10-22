// The Metal context of the graphics device (Objective-C++ only): the MTLDevice and its queue,
// serial-numbered command buffers, the upload heap, render passes over the current colour and
// depth targets with clears folded into their load actions, copies between targets, occlusion
// queries, readback, and presentation to the window's CAMetalLayer with the gamma ramp.
//
// CGxDeviceMetal owns one context and records its draws into RenderEncoder(); everything that
// needs another encoder (uploads, blits, resolves, presentation) goes through the context, which
// ends and reopens the render pass as needed.

#ifndef ENGINE_SOURCE_GX_CGXDEVICEMETAL_METALCONTEXT_H
#define ENGINE_SOURCE_GX_CGXDEVICEMETAL_METALCONTEXT_H

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "Engine/Source/Gx/CGxDeviceMetal/MetalResources.h"

namespace GxMetal {

void Log(const char* format, ...) __attribute__((format(printf, 1, 2)));

struct Rect {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
};

class Context {
    public:
    Context();
    ~Context();

    bool Init(id<MTLDevice> device);
    // The layer the frames are presented to; null presents nothing
    void SetLayer(CAMetalLayer* layer);
    // Presents on the display refresh (gxVSync) or as soon as a frame is ready
    void SetVSync(bool vsync);

    // Serials and command submission
    uint64_t CurrentSerial() const { return this->m_serial; }
    uint64_t CompletedSerial() const { return this->m_completed.load(std::memory_order_acquire); }
    id<MTLCommandBuffer> Commands();
    // Commits the work recorded so far; waits for it when wait is set
    void Flush(bool wait);
    void WaitForSerial(uint64_t serial);

    // Staged copies. A destination already read by the current command buffer is updated in
    // order within it; otherwise the copy runs ahead of it, in the upload command buffer.
    UploadAllocation AllocateUpload(uint32_t size, uint32_t alignment);
    void CopyBuffer(const UploadAllocation& source, id<MTLBuffer> dest, uint32_t destOffset, uint32_t size, GpuUse& use);
    void CopyToTexture(const UploadAllocation& source, uint32_t bytesPerRow, uint32_t bytesPerImage, id<MTLTexture> dest, uint32_t slice, uint32_t level, MTLOrigin origin, MTLSize size, GpuUse& use);
    id<MTLBlitCommandEncoder> InlineBlit();

    // Render targets. Changing either target ends the pass; the next draw or clear starts one.
    void SetTargets(const Target& color, const Target& depth);
    const Target& ColorTarget() const { return this->m_color; }
    const Target& DepthTarget() const { return this->m_depth; }
    // The render encoder of the current pass, beginning the pass if needed; nil without a colour
    // target. PassId() changes whenever a new encoder begins, which invalidates encoder state.
    id<MTLRenderCommandEncoder> RenderEncoder();
    uint64_t PassId() const { return this->m_passId; }
    void EndEncoders();
    void ExecutePendingClears();
    // Clears an area of the targets: whole-target clears become load actions of the next pass,
    // partial ones draw a quad
    void Clear(const Rect& area, bool color, bool depth, const float* rgba, float z);
    void MarkTargetsRendered();

    // Copies a rectangle of one target into another, scaling with a draw when the sizes or formats
    // differ (StretchRect). Null rectangles are the whole targets.
    bool Stretch(const Target& source, const Rect* sourceRect, const Target& dest, const Rect* destRect, bool linear);
    // Reads a rectangle of a colour target (resolved when multisampled) into memory, waiting for
    // the GPU; rows are pitch bytes apart
    bool Read(const Target& source, const Rect& rect, std::vector<uint8_t>& pixels, uint32_t& pitch);
    void Resolve(Texture* texture);
    void GenerateMipmaps(Texture* texture);


    // Occlusion queries count in the render passes between their begin and end
    void BeginOcclusionQuery(Query* query);
    void EndOcclusionQuery(Query* query);

    // Presentation: the back buffer is drawn into the layer's drawable through the gamma ramp
    void SetGammaRamp(const uint16_t* red, const uint16_t* green, const uint16_t* blue);
    void Present(Texture* backBuffer);

    // Unbound samplers read opaque black of the kind the function expects
    id<MTLTexture> DummyTexture(SamplerKind kind) const;

    id<MTLDevice> m_device = nil;
    id<MTLCommandQueue> m_queue = nil;

    private:
    struct FrameStats;

    id<MTLRenderPipelineState> UtilityPipeline(uint64_t key, NSString* fragment, MTLPixelFormat color, MTLPixelFormat depth, uint32_t samples, bool writeColor);
    bool BeginPass();
    void ClearWithQuad(const Rect& rect, bool color, bool depth, const float* rgba, float z);
    void UpdateFrameStats();

    uint64_t m_serial = 1;
    std::atomic<uint64_t> m_completed { 0 };
    uint64_t m_passId = 0;

    CAMetalLayer* m_layer = nil;
    bool m_vsync = true;
    UploadHeap m_uploads;
    dispatch_semaphore_t m_inFlight = nullptr;
    FrameStats* m_stats = nullptr;

    // Command buffers of the current serial, and the encoder open in the render command buffer
    id<MTLCommandBuffer> m_commands = nil;
    id<MTLCommandBuffer> m_uploadCommands = nil;
    id<MTLBlitCommandEncoder> m_uploadEncoder = nil;
    id<MTLRenderCommandEncoder> m_render = nil;
    id<MTLBlitCommandEncoder> m_blit = nil;

    // Targets and clears waiting for the next pass
    Target m_color;
    Target m_depth;
    bool m_pendingClearColor = false;
    bool m_pendingClearDepth = false;
    float m_clearColor[4] = {};
    float m_clearDepth = 1.0f;

    // Occlusion queries: the one being built counts; enclosing ones resume when it ends
    id<MTLBuffer> m_visibility = nil;
    uint32_t m_visibilityUsed = 0;
    std::vector<Query*> m_activeQueries;

    bool m_gammaIdentity = true;
    id<MTLTexture> m_gammaTexture = nil;

    std::unordered_map<uint64_t, id<MTLRenderPipelineState>> m_utilityPipelines;
    id<MTLLibrary> m_utilityLibrary = nil;
    id<MTLSamplerState> m_pointSampler = nil;
    id<MTLSamplerState> m_linearSampler = nil;
    id<MTLTexture> m_dummy2d = nil;
    id<MTLTexture> m_dummyCube = nil;
    id<MTLTexture> m_dummyDepth = nil;
    id<MTLDepthStencilState> m_clearDepthWrite = nil;
    id<MTLDepthStencilState> m_clearDepthKeep = nil;
};

}

#endif
