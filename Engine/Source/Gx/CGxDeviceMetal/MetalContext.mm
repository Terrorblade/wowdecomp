#include "Engine/Source/Gx/CGxDeviceMetal/MetalContext.h"
#include "Engine/Source/Gx/CGxDeviceMetal/MetalPipeline.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <mach/mach_time.h>
#include <os/log.h>
#include <unistd.h>

namespace GxMetal {

void Log(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    os_log(OS_LOG_DEFAULT, "GxMetal: %{public}s", buffer);
    fprintf(stderr, "GxMetal: %s\n", buffer);
}

namespace {

// Utility pipeline kinds, the high bits of their cache keys
const uint64_t kUtilityClear = 1ull << 48;
const uint64_t kUtilityPresent = 2ull << 48;
const uint64_t kUtilityStretch = 3ull << 48;

const char* const kUtilityShaders = R"MSL(#include <metal_stdlib>
using namespace metal;

struct UtilityVaryings {
    float4 position [[position]];
    float2 uv;
};

vertex UtilityVaryings utility_vertex(uint vid [[vertex_id]], constant float4& rect [[buffer(0)]], constant float& depth [[buffer(1)]]) {
    // rect: destination x0 y0 x1 y1 in clip space; the uv covers 0..1 over the destination
    float2 corner = float2((vid & 1) ? 1.0f : 0.0f, (vid & 2) ? 1.0f : 0.0f);
    UtilityVaryings out;
    out.position = float4(mix(rect.xy, rect.zw, corner), depth, 1.0f);
    out.uv = corner;
    return out;
}

fragment float4 utility_clear(constant float4& color [[buffer(0)]]) {
    return color;
}

struct BlitParameters {
    float4 sourceRect;   // u0 v0 u1 v1
    uint gamma;
};

fragment float4 utility_blit(UtilityVaryings in [[stage_in]], texture2d<float> source [[texture(0)]], sampler smp [[sampler(0)]], constant BlitParameters& parameters [[buffer(0)]], texture2d<float> gammaRamp [[texture(1)]]) {
    float2 uv = mix(parameters.sourceRect.xy, parameters.sourceRect.zw, in.uv);
    float4 color = source.sample(smp, uv);

    if (parameters.gamma != 0u) {
        uint3 index = uint3(saturate(color.rgb) * 255.0f + 0.5f);
        color.r = gammaRamp.read(uint2(index.r, 0)).r;
        color.g = gammaRamp.read(uint2(index.g, 0)).g;
        color.b = gammaRamp.read(uint2(index.b, 0)).b;
    }

    return color;
}
)MSL";

struct BlitParameters {
    float m_sourceRect[4];
    uint32_t m_gamma;
    uint32_t m_padding[3];
};

double NowSeconds() {
    static mach_timebase_info_data_t s_timebase = {};
    if (!s_timebase.denom) {
        mach_timebase_info(&s_timebase);
    }
    return static_cast<double>(mach_absolute_time()) * s_timebase.numer / s_timebase.denom * 1e-9;
}

}

// Frame statistics (WRATHMODERN_FRAMESTATS=1): every five seconds the average wall time between
// presents, the GPU time of the frame's command buffers and the time the CPU spent blocked on
// drawables and on the in-flight limit, written to stderr
struct Context::FrameStats {
    double m_windowStart = 0.0;
    double m_lastPresent = 0.0;
    uint32_t m_frames = 0;
    double m_wall = 0.0;
    double m_blocked = 0.0;
    double m_frameCompileStart = 0.0;
    double m_frameBlockedStart = 0.0;
    double m_drawableWait = 0.0;
    uint32_t m_frameCompilesStart = 0;
    std::atomic<uint64_t> m_gpuNanoseconds { 0 };
    std::atomic<uint32_t> m_gpuBuffers { 0 };
    // What reached the display: drawables shown, refreshes that showed no new frame, drawables never shown
    std::atomic<double> m_lastPresented { 0.0 };
    std::atomic<uint32_t> m_presented { 0 };
    std::atomic<uint32_t> m_missedRefreshes { 0 };
    std::atomic<uint32_t> m_dropped { 0 };
};

// Lifetime

Context::Context() {
    if (getenv("WRATHMODERN_FRAMESTATS")) {
        this->m_stats = new FrameStats();
    }
}

Context::~Context() {
    if (this->m_queue) {
        this->Flush(true);
    }

    GxMetal::Clear(this->m_color.m_texture);
    GxMetal::Clear(this->m_depth.m_texture);
    // Completion handlers of committed buffers may still count into the statistics
    (void)this->m_stats;
}

bool Context::Init(id<MTLDevice> device) {
    this->m_device = device;
    this->m_queue = [device newCommandQueueWithMaxCommandBufferCount:64];
    if (!this->m_queue) {
        return false;
    }

    this->m_uploads.Init(device);
    this->m_inFlight = dispatch_semaphore_create(3);

    NSError* error = nil;
    this->m_utilityLibrary = [device newLibraryWithSource:@(kUtilityShaders) options:nil error:&error];
    if (!this->m_utilityLibrary) {
        Log("utility shaders failed to compile: %s", error.localizedDescription.UTF8String);
        return false;
    }

    MTLSamplerDescriptor* sampler = [MTLSamplerDescriptor new];
    sampler.minFilter = MTLSamplerMinMagFilterNearest;
    sampler.magFilter = MTLSamplerMinMagFilterNearest;
    sampler.sAddressMode = MTLSamplerAddressModeClampToEdge;
    sampler.tAddressMode = MTLSamplerAddressModeClampToEdge;
    this->m_pointSampler = [device newSamplerStateWithDescriptor:sampler];
    sampler.minFilter = MTLSamplerMinMagFilterLinear;
    sampler.magFilter = MTLSamplerMinMagFilterLinear;
    this->m_linearSampler = [device newSamplerStateWithDescriptor:sampler];

    MTLDepthStencilDescriptor* depth = [MTLDepthStencilDescriptor new];
    depth.depthCompareFunction = MTLCompareFunctionAlways;
    depth.depthWriteEnabled = YES;
    this->m_clearDepthWrite = [device newDepthStencilStateWithDescriptor:depth];
    depth.depthWriteEnabled = NO;
    this->m_clearDepthKeep = [device newDepthStencilStateWithDescriptor:depth];

    // Unbound samplers read opaque black
    const uint32_t black = 0xFF000000;
    MTLTextureDescriptor* dummy = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:1 height:1 mipmapped:NO];
    dummy.storageMode = MTLStorageModeShared;
    this->m_dummy2d = [device newTextureWithDescriptor:dummy];
    [this->m_dummy2d replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&black bytesPerRow:4];

    MTLTextureDescriptor* cube = [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm size:1 mipmapped:NO];
    cube.storageMode = MTLStorageModeShared;
    this->m_dummyCube = [device newTextureWithDescriptor:cube];
    for (NSUInteger face = 0; face < 6; face++) {
        [this->m_dummyCube replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 slice:face withBytes:&black bytesPerRow:4 bytesPerImage:4];
    }

    MTLTextureDescriptor* depthDummy = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:1 height:1 mipmapped:NO];
    depthDummy.storageMode = MTLStorageModePrivate;
    depthDummy.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
    this->m_dummyDepth = [device newTextureWithDescriptor:depthDummy];

    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.depthAttachment.texture = this->m_dummyDepth;
    pass.depthAttachment.loadAction = MTLLoadActionClear;
    pass.depthAttachment.clearDepth = 1.0;
    pass.depthAttachment.storeAction = MTLStoreActionStore;
    id<MTLCommandBuffer> commands = [this->m_queue commandBuffer];
    [[commands renderCommandEncoderWithDescriptor:pass] endEncoding];
    [commands commit];

    return true;
}

void Context::SetLayer(CAMetalLayer* layer) {
    if (layer == this->m_layer) {
        return;
    }

    this->m_layer = layer;

    if (layer) {
        layer.device = this->m_device;
        layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        // The drawable is only written, by the present blit, and every pixel is opaque: macOS can then show
        // it without compositing (direct to display in full screen), which keeps the window server's load
        // from delaying the next drawable
        layer.framebufferOnly = YES;
        layer.opaque = YES;
        layer.maximumDrawableCount = 3;
        layer.displaySyncEnabled = this->m_vsync;
    }
}

void Context::SetVSync(bool vsync) {
    // Testing only (Tools/autotest.sh): WRATHMODERN_NO_VSYNC=1 lets frames run as fast as the GPU draws them,
    // so the frame time measures the GPU's work
    static const bool s_noVSync = getenv("WRATHMODERN_NO_VSYNC") != nullptr;
    this->m_vsync = vsync && !s_noVSync;
    vsync = this->m_vsync;

    if (this->m_layer) {
        this->m_layer.displaySyncEnabled = vsync;
    }
}

// Commands and submission

id<MTLCommandBuffer> Context::Commands() {
    if (!this->m_commands) {
        // Limit the command buffers in flight, which paces the CPU to the GPU
        double waitStart = this->m_stats ? NowSeconds() : 0.0;
        dispatch_semaphore_wait(this->m_inFlight, DISPATCH_TIME_FOREVER);
        if (this->m_stats) {
            this->m_stats->m_blocked += NowSeconds() - waitStart;
        }
        this->m_commands = [this->m_queue commandBuffer];
        this->m_commands.label = @"Gx frame";
    }

    return this->m_commands;
}

void Context::EndEncoders() {
    if (this->m_render) {
        // Occlusion counting stops with the encoder; open queries continue in the next pass
        [this->m_render endEncoding];
        this->m_render = nil;
    }

    if (this->m_blit) {
        [this->m_blit endEncoding];
        this->m_blit = nil;
    }
}

void Context::Flush(bool wait) {
    this->ExecutePendingClears();
    this->EndEncoders();

    if (!this->m_commands && !this->m_uploadCommands) {
        if (wait) {
            this->WaitForSerial(this->m_serial - 1);
        }
        return;
    }

    if (this->m_uploadEncoder) {
        [this->m_uploadEncoder endEncoding];
        this->m_uploadEncoder = nil;
    }

    if (this->m_uploadCommands) {
        [this->m_uploadCommands commit];
        this->m_uploadCommands = nil;
    }

    id<MTLCommandBuffer> commands = this->Commands();
    uint64_t serial = this->m_serial;
    dispatch_semaphore_t inFlight = this->m_inFlight;
    std::atomic<uint64_t>* completed = &this->m_completed;
    FrameStats* stats = this->m_stats;

    [commands addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
        if (stats && buffer.GPUEndTime > buffer.GPUStartTime) {
            stats->m_gpuNanoseconds += static_cast<uint64_t>((buffer.GPUEndTime - buffer.GPUStartTime) * 1e9);
            stats->m_gpuBuffers++;
        }

        if (buffer.status == MTLCommandBufferStatusError) {
            Log("command buffer %llu failed: %s", serial, buffer.error.localizedDescription.UTF8String);
        }

        uint64_t previous = completed->load(std::memory_order_relaxed);
        while (previous < serial && !completed->compare_exchange_weak(previous, serial, std::memory_order_release)) {
        }

        dispatch_semaphore_signal(inFlight);
    }];

    [commands commit];
    this->m_commands = nil;
    this->m_visibility = nil;
    this->m_visibilityUsed = 0;
    this->m_serial++;

    if (wait) {
        [commands waitUntilCompleted];
        this->WaitForSerial(serial);
    }
}

void Context::WaitForSerial(uint64_t serial) {
    while (this->CompletedSerial() < serial) {
        // Completion handlers run on a Metal thread shortly after the command buffer finishes
        usleep(50);
    }
}

// Uploads

UploadAllocation Context::AllocateUpload(uint32_t size, uint32_t alignment) {
    return this->m_uploads.Allocate(std::max(size, 4u), alignment, this->m_serial, this->CompletedSerial());
}

id<MTLBlitCommandEncoder> Context::InlineBlit() {
    if (!this->m_blit) {
        this->ExecutePendingClears();
        if (this->m_render) {
            [this->m_render endEncoding];
            this->m_render = nil;
        }
        this->m_blit = [this->Commands() blitCommandEncoder];
    }

    return this->m_blit;
}

void Context::CopyBuffer(const UploadAllocation& source, id<MTLBuffer> dest, uint32_t destOffset, uint32_t size, GpuUse& use) {
    id<MTLBlitCommandEncoder> blit;

    if (use.m_lastRenderUse == this->m_serial) {
        blit = this->InlineBlit();
    } else {
        if (!this->m_uploadCommands) {
            this->m_uploadCommands = [this->m_queue commandBuffer];
            this->m_uploadCommands.label = @"Gx uploads";
        }
        if (!this->m_uploadEncoder) {
            this->m_uploadEncoder = [this->m_uploadCommands blitCommandEncoder];
        }
        blit = this->m_uploadEncoder;
    }

    [blit copyFromBuffer:source.m_buffer sourceOffset:source.m_offset toBuffer:dest destinationOffset:destOffset size:size];
    use.m_lastUse = this->m_serial;
}

void Context::CopyToTexture(const UploadAllocation& source, uint32_t bytesPerRow, uint32_t bytesPerImage, id<MTLTexture> dest, uint32_t slice, uint32_t level, MTLOrigin origin, MTLSize size, GpuUse& use) {
    id<MTLBlitCommandEncoder> blit;

    if (use.m_lastRenderUse == this->m_serial) {
        blit = this->InlineBlit();
    } else {
        if (!this->m_uploadCommands) {
            this->m_uploadCommands = [this->m_queue commandBuffer];
            this->m_uploadCommands.label = @"Gx uploads";
        }
        if (!this->m_uploadEncoder) {
            this->m_uploadEncoder = [this->m_uploadCommands blitCommandEncoder];
        }
        blit = this->m_uploadEncoder;
    }

    [blit copyFromBuffer:source.m_buffer sourceOffset:source.m_offset sourceBytesPerRow:bytesPerRow sourceBytesPerImage:bytesPerImage sourceSize:size toTexture:dest destinationSlice:slice destinationLevel:level destinationOrigin:origin];
    use.m_lastUse = this->m_serial;
}

void Context::Resolve(Texture* texture) {
    if (texture->m_samples <= 1 || !texture->m_resolveDirty || !texture->m_resolve) {
        return;
    }

    this->ExecutePendingClears();
    this->EndEncoders();

    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = texture->m_texture;
    pass.colorAttachments[0].resolveTexture = texture->m_resolve;
    pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
    pass.colorAttachments[0].storeAction = MTLStoreActionStoreAndMultisampleResolve;
    [[this->Commands() renderCommandEncoderWithDescriptor:pass] endEncoding];

    texture->m_resolveDirty = false;
    texture->m_use.m_lastUse = this->m_serial;
}

void Context::GenerateMipmaps(Texture* texture) {
    if (!texture->m_mipsDirty || texture->m_levels <= 1) {
        return;
    }

    [this->InlineBlit() generateMipmapsForTexture:texture->m_texture];
    texture->m_mipsDirty = false;
    texture->m_use.m_lastUse = this->m_serial;
}

// Render passes

void Context::SetTargets(const Target& color, const Target& depth) {
    if (color == this->m_color && depth == this->m_depth) {
        return;
    }

    this->ExecutePendingClears();
    if (this->m_render) {
        [this->m_render endEncoding];
        this->m_render = nil;
    }

    Target previousColor = this->m_color;
    Target previousDepth = this->m_depth;
    this->m_color = color;
    this->m_depth = depth;

    if (color.m_texture) {
        color.m_texture->Retain();
    }
    if (depth.m_texture) {
        depth.m_texture->Retain();
    }
    if (previousColor.m_texture) {
        previousColor.m_texture->Release();
    }
    if (previousDepth.m_texture) {
        previousDepth.m_texture->Release();
    }
}

bool Context::BeginPass() {
    if (this->m_render) {
        return true;
    }

    Texture* color = this->m_color.m_texture;
    if (!color) {
        return false;
    }

    if (this->m_blit) {
        [this->m_blit endEncoding];
        this->m_blit = nil;
    }

    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];

    pass.colorAttachments[0].texture = color->m_texture;
    pass.colorAttachments[0].level = this->m_color.m_level;
    pass.colorAttachments[0].slice = this->m_color.m_face;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;

    if (this->m_pendingClearColor) {
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(this->m_clearColor[0], this->m_clearColor[1], this->m_clearColor[2], this->m_clearColor[3]);
    } else {
        pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
    }

    if (Texture* depth = this->m_depth.m_texture) {
        pass.depthAttachment.texture = depth->m_texture;
        pass.depthAttachment.level = this->m_depth.m_level;
        pass.depthAttachment.slice = this->m_depth.m_face;
        pass.depthAttachment.storeAction = MTLStoreActionStore;

        if (this->m_pendingClearDepth) {
            pass.depthAttachment.loadAction = MTLLoadActionClear;
            pass.depthAttachment.clearDepth = this->m_clearDepth;
        } else {
            pass.depthAttachment.loadAction = MTLLoadActionLoad;
        }

        // The window's depth carries a stencil plane for the tags of GxMetalSetStencilTag; it clears with the
        // depth, so tags never outlive the frame that drew them
        if (depth->m_format.m_pixelFormat == MTLPixelFormatDepth32Float_Stencil8) {
            pass.stencilAttachment.texture = depth->m_texture;
            pass.stencilAttachment.level = this->m_depth.m_level;
            pass.stencilAttachment.slice = this->m_depth.m_face;
            pass.stencilAttachment.storeAction = MTLStoreActionStore;
            pass.stencilAttachment.loadAction = this->m_pendingClearDepth ? MTLLoadActionClear : MTLLoadActionLoad;
            pass.stencilAttachment.clearStencil = 0;
        }

        depth->m_use.m_lastUse = this->m_serial;
        depth->MarkWritten(this->m_depth.m_level);
    }

    this->m_pendingClearColor = false;
    this->m_pendingClearDepth = false;

    if (!this->m_activeQueries.empty() || this->m_visibility) {
        if (!this->m_visibility) {
            this->m_visibility = [this->m_device newBufferWithLength:64 * 1024 options:MTLResourceStorageModeShared];
            this->m_visibilityUsed = 0;
        }
        pass.visibilityResultBuffer = this->m_visibility;
    }

    this->m_render = [this->Commands() renderCommandEncoderWithDescriptor:pass];
    if (!this->m_render) {
        return false;
    }

    this->m_passId++;
    color->m_use.m_lastUse = this->m_serial;
    color->MarkWritten(this->m_color.m_level);

    // The client's front faces are clockwise, as Direct3D's
    [this->m_render setFrontFacingWinding:MTLWindingClockwise];

    // The query still being built counts in this pass as well
    if (!this->m_activeQueries.empty()) {
        this->BeginOcclusionQuery(this->m_activeQueries.back());
    }

    return true;
}

id<MTLRenderCommandEncoder> Context::RenderEncoder() {
    return this->BeginPass() ? this->m_render : nil;
}

void Context::MarkTargetsRendered() {
    if (this->m_color.m_texture) {
        this->m_color.m_texture->MarkWritten(this->m_color.m_level);
    }
    if (this->m_depth.m_texture) {
        this->m_depth.m_texture->MarkWritten(this->m_depth.m_level);
    }
}

void Context::ExecutePendingClears() {
    if (!this->m_render && (this->m_pendingClearColor || this->m_pendingClearDepth)) {
        if (this->BeginPass()) {
            [this->m_render endEncoding];
            this->m_render = nil;
        }
    }
}

// Utility drawing

id<MTLRenderPipelineState> Context::UtilityPipeline(uint64_t key, NSString* fragment, MTLPixelFormat color, MTLPixelFormat depth, uint32_t samples, bool writeColor) {
    auto found = this->m_utilityPipelines.find(key);
    if (found != this->m_utilityPipelines.end()) {
        return found->second;
    }

    MTLRenderPipelineDescriptor* descriptor = [MTLRenderPipelineDescriptor new];
    descriptor.vertexFunction = [this->m_utilityLibrary newFunctionWithName:@"utility_vertex"];
    descriptor.fragmentFunction = [this->m_utilityLibrary newFunctionWithName:fragment];
    descriptor.colorAttachments[0].pixelFormat = color;
    descriptor.colorAttachments[0].writeMask = writeColor ? MTLColorWriteMaskAll : MTLColorWriteMaskNone;
    descriptor.depthAttachmentPixelFormat = depth;
    if (depth == MTLPixelFormatDepth32Float_Stencil8) {
        descriptor.stencilAttachmentPixelFormat = depth;
    }
    descriptor.rasterSampleCount = samples;

    NSError* error = nil;
    id<MTLRenderPipelineState> pipeline = [this->m_device newRenderPipelineStateWithDescriptor:descriptor error:&error];
    if (!pipeline) {
        Log("utility pipeline creation failed: %s", error.localizedDescription.UTF8String);
    }

    this->m_utilityPipelines[key] = pipeline;
    return pipeline;
}

void Context::ClearWithQuad(const Rect& rect, bool color, bool depth, const float* rgba, float z) {
    if (!this->BeginPass()) {
        return;
    }

    Texture* target = this->m_color.m_texture;
    Texture* depthTarget = this->m_depth.m_texture;
    MTLPixelFormat colorFormat = target->m_format.m_pixelFormat;
    MTLPixelFormat depthFormat = depthTarget ? depthTarget->m_format.m_pixelFormat : MTLPixelFormatInvalid;
    uint64_t key = kUtilityClear | static_cast<uint64_t>(colorFormat) | (static_cast<uint64_t>(depthFormat) << 16) | (static_cast<uint64_t>(target->m_samples) << 32) | (color ? (1ull << 40) : 0);

    id<MTLRenderPipelineState> pipeline = this->UtilityPipeline(key, @"utility_clear", colorFormat, depthFormat, target->m_samples, color);
    if (!pipeline) {
        return;
    }

    id<MTLRenderCommandEncoder> encoder = this->m_render;
    MTLViewport viewport = { 0.0, 0.0, static_cast<double>(this->m_color.Width()), static_cast<double>(this->m_color.Height()), 0.0, 1.0 };
    MTLScissorRect scissor = { static_cast<NSUInteger>(rect.left), static_cast<NSUInteger>(rect.top), static_cast<NSUInteger>(rect.right - rect.left), static_cast<NSUInteger>(rect.bottom - rect.top) };

    [encoder setRenderPipelineState:pipeline];
    if (depthTarget) {
        [encoder setDepthStencilState:depth ? this->m_clearDepthWrite : this->m_clearDepthKeep];
    }
    [encoder setCullMode:MTLCullModeNone];
    [encoder setTriangleFillMode:MTLTriangleFillModeFill];
    [encoder setDepthBias:0.0f slopeScale:0.0f clamp:0.0f];
    [encoder setViewport:viewport];
    [encoder setScissorRect:scissor];

    float clipRect[4] = { -1.0f, -1.0f, 1.0f, 1.0f };
    float color4[4] = { rgba[0], rgba[1], rgba[2], rgba[3] };
    [encoder setVertexBytes:clipRect length:sizeof(clipRect) atIndex:0];
    [encoder setVertexBytes:&z length:sizeof(z) atIndex:1];
    [encoder setFragmentBytes:color4 length:sizeof(color4) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];

    // The draw replaced the encoder state the next draw expects
    this->m_passId++;
}

void Context::Clear(const Rect& area, bool clearColor, bool clearDepth, const float* rgba, float z) {
    if (!this->m_color.m_texture) {
        return;
    }

    clearDepth = clearDepth && this->m_depth.m_texture;
    if (!clearColor && !clearDepth) {
        return;
    }

    int32_t width = static_cast<int32_t>(this->m_color.Width());
    int32_t height = static_cast<int32_t>(this->m_color.Height());
    bool fullColor = area.left <= 0 && area.top <= 0 && area.right >= width && area.bottom >= height;
    bool fullDepth = !clearDepth || (area.left <= 0 && area.top <= 0 && area.right >= static_cast<int32_t>(this->m_depth.Width()) && area.bottom >= static_cast<int32_t>(this->m_depth.Height()));
    z = std::clamp(z, 0.0f, 1.0f);

    if (!this->m_render && fullColor && fullDepth) {
        // Clear when the next pass loads its attachments
        if (clearColor) {
            this->m_pendingClearColor = true;
            memcpy(this->m_clearColor, rgba, sizeof(this->m_clearColor));
        }
        if (clearDepth) {
            this->m_pendingClearDepth = true;
            this->m_clearDepth = z;
        }
        return;
    }

    Rect r = area;
    r.left = std::clamp(r.left, 0, width);
    r.top = std::clamp(r.top, 0, height);
    r.right = std::clamp(r.right, r.left, width);
    r.bottom = std::clamp(r.bottom, r.top, height);
    if (r.right > r.left && r.bottom > r.top) {
        this->ClearWithQuad(r, clearColor, clearDepth, rgba, z);
    }
}

// Copies

bool Context::Stretch(const Target& source, const Rect* sourceRect, const Target& dest, const Rect* destRect, bool linear) {
    if (!source.m_texture || !dest.m_texture || source.m_texture->m_format.m_depth || dest.m_texture->m_format.m_depth) {
        return false;
    }

    int32_t sourceWidth = static_cast<int32_t>(source.Width());
    int32_t sourceHeight = static_cast<int32_t>(source.Height());
    int32_t destWidth = static_cast<int32_t>(dest.Width());
    int32_t destHeight = static_cast<int32_t>(dest.Height());
    Rect from = sourceRect ? *sourceRect : Rect { 0, 0, sourceWidth, sourceHeight };
    Rect to = destRect ? *destRect : Rect { 0, 0, destWidth, destHeight };

    if (from.left < 0 || from.top < 0 || from.right > sourceWidth || from.bottom > sourceHeight || from.left >= from.right || from.top >= from.bottom) {
        return false;
    }
    if (to.left < 0 || to.top < 0 || to.right > destWidth || to.bottom > destHeight || to.left >= to.right || to.top >= to.bottom) {
        return false;
    }

    this->ExecutePendingClears();

    Texture* sourceTexture = source.m_texture;
    Texture* destTexture = dest.m_texture;
    id<MTLTexture> sourceMetal = sourceTexture->ResolvedTexture();
    uint32_t width = static_cast<uint32_t>(to.right - to.left);
    uint32_t height = static_cast<uint32_t>(to.bottom - to.top);
    bool sameSize = width == static_cast<uint32_t>(from.right - from.left) && height == static_cast<uint32_t>(from.bottom - from.top);
    id<MTLTexture> destMetal = destTexture->m_samples > 1 ? destTexture->m_texture : destTexture->m_texture;

    if (sameSize && destTexture->m_samples == 1 && sourceTexture->m_format.m_pixelFormat == destTexture->m_format.m_pixelFormat) {
        id<MTLBlitCommandEncoder> blit = this->InlineBlit();
        [blit copyFromTexture:sourceMetal sourceSlice:source.m_face sourceLevel:source.m_level sourceOrigin:MTLOriginMake(from.left, from.top, 0) sourceSize:MTLSizeMake(width, height, 1) toTexture:destMetal destinationSlice:dest.m_face destinationLevel:dest.m_level destinationOrigin:MTLOriginMake(to.left, to.top, 0)];
    } else {
        // Scale (or convert) by drawing the source over the destination rectangle
        this->EndEncoders();

        MTLPixelFormat destFormat = destTexture->m_format.m_pixelFormat;
        uint64_t key = kUtilityStretch | static_cast<uint64_t>(destFormat) | (static_cast<uint64_t>(destTexture->m_samples) << 32);
        id<MTLRenderPipelineState> pipeline = this->UtilityPipeline(key, @"utility_blit", destFormat, MTLPixelFormatInvalid, destTexture->m_samples, true);
        if (!pipeline) {
            return false;
        }

        MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = destMetal;
        pass.colorAttachments[0].level = dest.m_level;
        pass.colorAttachments[0].slice = dest.m_face;
        pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;

        id<MTLTexture> view = sourceMetal;
        if (source.m_level != 0 || source.m_face != 0 || sourceMetal.textureType != MTLTextureType2D) {
            view = [sourceMetal newTextureViewWithPixelFormat:sourceMetal.pixelFormat textureType:MTLTextureType2D levels:NSMakeRange(source.m_level, 1) slices:NSMakeRange(source.m_face, 1)];
        }

        BlitParameters parameters = { { from.left / static_cast<float>(sourceWidth), from.top / static_cast<float>(sourceHeight), from.right / static_cast<float>(sourceWidth), from.bottom / static_cast<float>(sourceHeight) }, 0, {} };
        float clipRect[4] = { to.left / static_cast<float>(destWidth) * 2.0f - 1.0f, 1.0f - to.top / static_cast<float>(destHeight) * 2.0f, to.right / static_cast<float>(destWidth) * 2.0f - 1.0f, 1.0f - to.bottom / static_cast<float>(destHeight) * 2.0f };
        float depth = 0.0f;

        id<MTLRenderCommandEncoder> encoder = [this->Commands() renderCommandEncoderWithDescriptor:pass];
        [encoder setRenderPipelineState:pipeline];
        [encoder setVertexBytes:clipRect length:sizeof(clipRect) atIndex:0];
        [encoder setVertexBytes:&depth length:sizeof(depth) atIndex:1];
        [encoder setFragmentTexture:view atIndex:0];
        [encoder setFragmentTexture:this->m_dummy2d atIndex:1];
        [encoder setFragmentSamplerState:linear ? this->m_linearSampler : this->m_pointSampler atIndex:0];
        [encoder setFragmentBytes:&parameters length:sizeof(parameters) atIndex:0];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
        [encoder endEncoding];
    }

    sourceTexture->m_use.m_lastUse = this->m_serial;
    sourceTexture->m_use.m_lastRenderUse = this->m_serial;
    destTexture->m_use.m_lastUse = this->m_serial;
    destTexture->MarkWritten(dest.m_level);
    return true;
}

bool Context::Read(const Target& source, const Rect& rect, std::vector<uint8_t>& pixels, uint32_t& pitch) {
    Texture* texture = source.m_texture;
    if (!texture || texture->m_format.m_depth || texture->m_format.m_compressed || rect.left < 0 || rect.top < 0 || rect.right > static_cast<int32_t>(source.Width()) || rect.bottom > static_cast<int32_t>(source.Height()) || rect.left >= rect.right || rect.top >= rect.bottom) {
        return false;
    }

    uint32_t width = static_cast<uint32_t>(rect.right - rect.left);
    uint32_t height = static_cast<uint32_t>(rect.bottom - rect.top);
    pitch = texture->m_format.RowPitch(width);

    id<MTLBuffer> readback = [this->m_device newBufferWithLength:pitch * height options:MTLResourceStorageModeShared];
    if (!readback) {
        return false;
    }

    id<MTLTexture> resolved = texture->ResolvedTexture();
    [this->InlineBlit() copyFromTexture:resolved sourceSlice:source.m_face sourceLevel:source.m_level sourceOrigin:MTLOriginMake(rect.left, rect.top, 0) sourceSize:MTLSizeMake(width, height, 1) toBuffer:readback destinationOffset:0 destinationBytesPerRow:pitch destinationBytesPerImage:pitch * height];
    texture->m_use.m_lastUse = this->m_serial;
    this->Flush(true);

    const uint8_t* data = static_cast<const uint8_t*>(readback.contents);
    pixels.assign(data, data + pitch * height);
    return true;
}

// Queries

void Context::BeginOcclusionQuery(Query* query) {
    if (std::find(this->m_activeQueries.begin(), this->m_activeQueries.end(), query) == this->m_activeQueries.end()) {
        query->m_slots.clear();
        this->m_activeQueries.push_back(query);
    }

    // Counting needs a visibility buffer in the pass; start a pass that has one
    if (this->m_render && !this->m_visibility) {
        this->ExecutePendingClears();
        [this->m_render endEncoding];
        this->m_render = nil;
    }

    if (!this->m_render) {
        // The next pass begins counting for the query
        if (!this->m_visibility) {
            this->m_visibility = [this->m_device newBufferWithLength:64 * 1024 options:MTLResourceStorageModeShared];
        }
        return;
    }

    if (this->m_visibilityUsed + 8 > this->m_visibility.length) {
        // Out of slots in this command buffer: continue in a new one
        this->Flush(false);
        this->m_visibility = [this->m_device newBufferWithLength:64 * 1024 options:MTLResourceStorageModeShared];
        return;
    }

    uint32_t offset = this->m_visibilityUsed;
    this->m_visibilityUsed += 8;
    memset(static_cast<uint8_t*>(this->m_visibility.contents) + offset, 0, 8);
    query->m_slots.push_back({ this->m_visibility, offset });
    [this->m_render setVisibilityResultMode:MTLVisibilityResultModeCounting offset:offset];
}

void Context::EndOcclusionQuery(Query* query) {
    auto found = std::find(this->m_activeQueries.begin(), this->m_activeQueries.end(), query);
    if (found == this->m_activeQueries.end()) {
        return;
    }

    bool wasCounting = found + 1 == this->m_activeQueries.end();
    this->m_activeQueries.erase(found);

    if (this->m_render && wasCounting) {
        if (this->m_activeQueries.empty()) {
            [this->m_render setVisibilityResultMode:MTLVisibilityResultModeDisabled offset:0];
        } else {
            // An enclosing query resumes counting
            this->BeginOcclusionQuery(this->m_activeQueries.back());
        }
    }
}

// Presentation

void Context::SetGammaRamp(const uint16_t* red, const uint16_t* green, const uint16_t* blue) {
    bool identity = true;
    for (uint32_t i = 0; i < 256 && identity; i++) {
        uint16_t expected = static_cast<uint16_t>(i * 257);
        identity = red[i] == expected && green[i] == expected && blue[i] == expected;
    }
    this->m_gammaIdentity = identity;

    if (identity) {
        return;
    }

    // The ramp may be in use by a frame in flight; a new texture avoids changing it under the GPU
    if (!this->m_gammaTexture || this->m_commands || this->CompletedSerial() + 1 < this->m_serial) {
        MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Unorm width:256 height:1 mipmapped:NO];
        descriptor.storageMode = MTLStorageModeShared;
        this->m_gammaTexture = [this->m_device newTextureWithDescriptor:descriptor];
    }

    uint16_t texels[256 * 4];
    for (uint32_t i = 0; i < 256; i++) {
        texels[i * 4 + 0] = red[i];
        texels[i * 4 + 1] = green[i];
        texels[i * 4 + 2] = blue[i];
        texels[i * 4 + 3] = 0xFFFF;
    }

    [this->m_gammaTexture replaceRegion:MTLRegionMake2D(0, 0, 256, 1) mipmapLevel:0 withBytes:texels bytesPerRow:sizeof(texels)];
}

void Context::Present(Texture* backBuffer) {
    this->ExecutePendingClears();
    this->EndEncoders();

    if (this->m_layer && backBuffer) {
        id<MTLTexture> source = backBuffer->ResolvedTexture();

        double drawableStart = this->m_stats ? NowSeconds() : 0.0;
        id<CAMetalDrawable> drawable = [this->m_layer nextDrawable];
        if (this->m_stats) {
            this->m_stats->m_blocked += NowSeconds() - drawableStart;
            this->m_stats->m_drawableWait = NowSeconds() - drawableStart;
        }

        if (drawable) {
            id<MTLTexture> target = drawable.texture;
            id<MTLRenderPipelineState> pipeline = this->UtilityPipeline(kUtilityPresent | static_cast<uint64_t>(target.pixelFormat), @"utility_blit", target.pixelFormat, MTLPixelFormatInvalid, 1, true);

            if (pipeline) {
                MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
                pass.colorAttachments[0].texture = target;
                pass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
                pass.colorAttachments[0].storeAction = MTLStoreActionStore;

                id<MTLRenderCommandEncoder> encoder = [this->Commands() renderCommandEncoderWithDescriptor:pass];
                bool sameSize = target.width == source.width && target.height == source.height;
                BlitParameters parameters = { { 0.0f, 0.0f, 1.0f, 1.0f }, this->m_gammaIdentity ? 0u : 1u, {} };
                float clipRect[4] = { -1.0f, 1.0f, 1.0f, -1.0f };
                float depth = 0.0f;

                [encoder setRenderPipelineState:pipeline];
                [encoder setVertexBytes:clipRect length:sizeof(clipRect) atIndex:0];
                [encoder setVertexBytes:&depth length:sizeof(depth) atIndex:1];
                [encoder setFragmentTexture:source atIndex:0];
                [encoder setFragmentTexture:this->m_gammaTexture ?: this->m_dummy2d atIndex:1];
                [encoder setFragmentSamplerState:sameSize ? this->m_pointSampler : this->m_linearSampler atIndex:0];
                [encoder setFragmentBytes:&parameters length:sizeof(parameters) atIndex:0];
                [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
                [encoder endEncoding];
            }

            if (FrameStats* stats = this->m_stats) {
                // A refresh is 1/60 s with vsync (the window's display runs at 60 Hz); a gap of more than one
                // and a half refreshes between shown frames is a frame the player saw twice
                [drawable addPresentedHandler:^(id<MTLDrawable> shown) {
                    double time = shown.presentedTime;
                    if (time <= 0.0) {
                        stats->m_dropped++;
                        return;
                    }
                    double last = stats->m_lastPresented.exchange(time);
                    stats->m_presented++;
                    double gap = time - last;
                    if (last > 0.0 && gap > 1.5 / 60.0 && gap < 1.0) {
                        stats->m_missedRefreshes += static_cast<uint32_t>(gap * 60.0 + 0.5) - 1;
                        fprintf(stderr, "framestats shown late: %.1f ms since the last frame shown\n", gap * 1e3);
                    }
                }];
            }

            [this->Commands() presentDrawable:drawable];
        }
    }

    this->Flush(false);
    this->UpdateFrameStats();
}

void Context::UpdateFrameStats() {
    FrameStats* stats = this->m_stats;
    if (!stats) {
        return;
    }

    double now = NowSeconds();
    if (stats->m_lastPresent > 0.0) {
        stats->m_wall += now - stats->m_lastPresent;
        stats->m_frames++;
        // A long frame, with the shader compilation done during it
        double frame = now - stats->m_lastPresent;
        if (frame > 0.025) {
            fprintf(stderr, "framestats long frame %.1f ms, drawable %.1f ms, waiting %.1f ms, compiling %.1f ms (%u)\n", frame * 1e3, stats->m_drawableWait * 1e3,
                    (stats->m_blocked - stats->m_frameBlockedStart) * 1e3, (g_compileSeconds - stats->m_frameCompileStart) * 1e3,
                    g_compiles - stats->m_frameCompilesStart);
        }
    } else {
        stats->m_windowStart = now;
    }
    stats->m_lastPresent = now;
    stats->m_frameCompileStart = g_compileSeconds;
    stats->m_frameBlockedStart = stats->m_blocked;
    stats->m_frameCompilesStart = g_compiles;

    if (now - stats->m_windowStart >= 5.0 && stats->m_frames) {
        uint32_t buffers = stats->m_gpuBuffers.exchange(0);
        uint64_t gpu = stats->m_gpuNanoseconds.exchange(0);
        double frames = stats->m_frames;
        time_t clock = time(nullptr);
        char stamp[16] = {};
        strftime(stamp, sizeof(stamp), "%H:%M:%S", localtime(&clock));
        fprintf(stderr, "framestats %s: %u frames, wall %.2f ms, cpu %.2f ms, blocked %.2f ms, gpu %.2f ms (%u buffers), shown %u, missed refreshes %u, dropped %u\n", stamp, stats->m_frames, stats->m_wall / frames * 1e3, (stats->m_wall - stats->m_blocked) / frames * 1e3, stats->m_blocked / frames * 1e3, gpu * 1e-6 / frames, buffers, stats->m_presented.exchange(0), stats->m_missedRefreshes.exchange(0), stats->m_dropped.exchange(0));
        stats->m_frames = 0;
        stats->m_wall = 0.0;
        stats->m_blocked = 0.0;
        stats->m_frameBlockedStart = 0.0;
        stats->m_windowStart = now;
    }
}

id<MTLTexture> Context::DummyTexture(SamplerKind kind) const {
    switch (kind) {
        case SAMPLERKIND_CUBE: return this->m_dummyCube;
        case SAMPLERKIND_DEPTH: return this->m_dummyDepth;
        default: return this->m_dummy2d;
    }
}

}
