#include <mach/mach_time.h>
#include <functional>
#include <unistd.h>
#include <mutex>
#include "Engine/Source/Gx/CGxDeviceMetal/MetalPipeline.h"
#include "Engine/Source/Gx/CGxDeviceMetal/MetalContext.h"

#include <algorithm>

namespace GxMetal {

// Shaders

Shader* Shader::Create(const uint32_t* tokens, uint32_t sizeInBytes, bool vertex) {
    if (!tokens || sizeInBytes < 4) {
        return nullptr;
    }

    ShaderProgram program;
    ShaderInfo info;
    std::string error;

    if (!ParseShader(tokens, sizeInBytes / 4, program, error) || !AnalyzeShader(program, info, error)) {
        Log("shader rejected: %s", error.c_str());
        return nullptr;
    }

    if ((program.m_stage == STAGE_VERTEX) != vertex) {
        return nullptr;
    }

    Shader* shader = new Shader();
    shader->m_program = std::move(program);
    shader->m_info = info;
    return shader;
}

// Vertex layouts

namespace {

// CGxVertexAttrib::m_type, the formats of CGxDeviceD3d::s_gxAttribToD3dAttribType: packed colour
// (stored B, G, R, A), unsigned bytes, normalized unsigned bytes, two and three floats, two
// shorts and one float
bool AttributeFormat(uint32_t type, MTLVertexFormat& format, uint8_t& attributeClass, uint8_t& components) {
    attributeClass = ATTRIBCLASS_FLOAT;

    switch (type) {
        case 0: format = MTLVertexFormatUChar4Normalized_BGRA; components = 4; return true;
        case 1: format = MTLVertexFormatUChar4; components = 4; attributeClass = ATTRIBCLASS_UINT; return true;
        case 2: format = MTLVertexFormatUChar4Normalized; components = 4; return true;
        case 3: format = MTLVertexFormatFloat2; components = 2; return true;
        case 4: format = MTLVertexFormatFloat3; components = 3; return true;
        case 5: format = MTLVertexFormatShort2; components = 2; attributeClass = ATTRIBCLASS_SINT; return true;
        case 6: format = MTLVertexFormatFloat; components = 1; return true;
        default: return false;
    }
}

}

bool VertexLayout::Init(const VertexElement* elements, uint32_t count, uint64_t id) {
    this->m_elements.assign(elements, elements + count);
    this->m_id = id;

    for (uint32_t i = 0; i < count; i++) {
        const VertexElement& element = elements[i];
        MTLVertexFormat format;
        uint8_t attributeClass;
        uint8_t components;

        if (element.m_attribute >= ATTRIB_COUNT || element.m_stream >= 16 || !AttributeFormat(element.m_type, format, attributeClass, components)) {
            return false;
        }

        if (this->m_signature.m_class[element.m_attribute]) {
            continue;
        }

        this->m_mapped.push_back({ element.m_attribute, element.m_stream, element.m_offset, format });
        this->m_signature.m_class[element.m_attribute] = attributeClass;
        this->m_streamMask |= 1u << element.m_stream;

        if (element.m_attribute >= ATTRIB_TEXCOORD0) {
            this->m_texcoordComponents[element.m_attribute - ATTRIB_TEXCOORD0] = components;
        }
    }

    return true;
}

bool VertexLayout::Matches(const VertexElement* elements, uint32_t count) const {
    return count == this->m_elements.size() && memcmp(elements, this->m_elements.data(), count * sizeof(VertexElement)) == 0;
}

// Caches

double g_compileSeconds = 0.0;
uint32_t g_compiles = 0;

namespace {
double CompileClock() {
    static mach_timebase_info_data_t s_timebase = {};
    if (!s_timebase.denom) {
        mach_timebase_info(&s_timebase);
    }
    return static_cast<double>(mach_absolute_time()) * s_timebase.numer / s_timebase.denom * 1e-9;
}
}

namespace {
// Where the sources of compiled shaders are kept, relative to the game directory (the working directory)
const char* const kShaderCacheDirectory = "Cache/WrathModern/Shaders";

MTLCompileOptions* FunctionCompileOptions() {
    MTLCompileOptions* options = [MTLCompileOptions new];
    options.preserveInvariance = YES;
    options.mathMode = MTLMathModeFast;
    return options;
}

std::string ShaderCachePath(const std::string& source, bool vertex) {
    char name[64];
    snprintf(name, sizeof(name), "/%016llx.%s", static_cast<unsigned long long>(std::hash<std::string>()(source)), vertex ? "vsh" : "fsh");
    return std::string(kShaderCacheDirectory) + name;
}
}

struct PipelineCache::Prewarm {
    std::mutex m_lock;
    std::unordered_map<std::string, id<MTLFunction>> m_functions;
};

void PipelineCache::Init(id<MTLDevice> device) {
    this->m_device = device;
    this->m_prewarm = std::make_shared<Prewarm>();

    // Compile every source an earlier run compiled, in parallel, while the client starts and logs in
    std::shared_ptr<Prewarm> prewarm = this->m_prewarm;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSArray<NSString*>* names = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:@(kShaderCacheDirectory) error:nil];
        if (!names.count) {
            return;
        }
        double started = CompileClock();
        dispatch_apply(names.count, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^(size_t i) {
            @autoreleasepool {
                NSString* name = names[i];
                bool vertex = [name hasSuffix:@".vsh"];
                if (!vertex && ![name hasSuffix:@".fsh"]) {
                    return;
                }
                NSString* path = [@(kShaderCacheDirectory) stringByAppendingPathComponent:name];
                NSString* text = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:nil];
                if (!text) {
                    return;
                }
                id<MTLLibrary> library = [device newLibraryWithSource:text options:FunctionCompileOptions() error:nil];
                id<MTLFunction> function = [library newFunctionWithName:@(vertex ? kVertexFunctionName : kFragmentFunctionName)];
                if (function) {
                    std::lock_guard<std::mutex> guard(prewarm->m_lock);
                    prewarm->m_functions[std::string(text.UTF8String)] = function;
                }
            }
        });
        Log("shader cache: %lu sources compiled in the background in %.0f ms", static_cast<unsigned long>(names.count), (CompileClock() - started) * 1e3);
    });
}

bool PipelineCache::CompileFunction(const std::string& source, bool vertex, CompiledFunction& function) {
    auto cached = this->m_functions.find(source);
    if (cached != this->m_functions.end()) {
        function = cached->second;
        return function.m_function != nil;
    }

    function.m_function = nil;
    if (this->m_prewarm) {
        std::lock_guard<std::mutex> guard(this->m_prewarm->m_lock);
        auto prewarmed = this->m_prewarm->m_functions.find(source);
        if (prewarmed != this->m_prewarm->m_functions.end()) {
            function.m_function = prewarmed->second;
            this->m_prewarm->m_functions.erase(prewarmed);
        }
    }

    NSError* error = nil;
    if (!function.m_function) {
        double started = CompileClock();
        id<MTLLibrary> library = [this->m_device newLibraryWithSource:@(source.c_str()) options:FunctionCompileOptions() error:&error];
        function.m_function = [library newFunctionWithName:@(vertex ? kVertexFunctionName : kFragmentFunctionName)];
        g_compileSeconds += CompileClock() - started;
        g_compiles++;

        // Kept for the next run's background compilation
        if (function.m_function) {
            std::string path = ShaderCachePath(source, vertex);
            if (access(path.c_str(), F_OK) != 0) {
                [[NSFileManager defaultManager] createDirectoryAtPath:@(kShaderCacheDirectory) withIntermediateDirectories:YES attributes:nil error:nil];
                FILE* file = fopen(path.c_str(), "wb");
                if (file) {
                    fwrite(source.data(), 1, source.size(), file);
                    fclose(file);
                }
            }
        }
    }
    function.m_id = this->NextId();

    if (!function.m_function) {
        Log("shader compilation failed: %s\n%s", error.localizedDescription.UTF8String, source.c_str());
    }

    this->m_functions[source] = function;
    return function.m_function != nil;
}

CompiledFunction PipelineCache::VertexShaderFunction(Shader* shader, const VertexInputSignature& signature) {
    for (auto& entry : shader->m_vertexFunctions) {
        if (entry.first == signature) {
            return entry.second;
        }
    }

    CompiledFunction function;
    std::string source;
    std::string error;

    if (!TranslateVertexShader(shader->m_program, signature, source, error)) {
        Log("vertex shader translation failed: %s", error.c_str());
    } else {
        this->CompileFunction(source, true, function);
    }

    shader->m_vertexFunctions.emplace_back(signature, function);
    return function;
}

CompiledFunction PipelineCache::PixelShaderFunction(Shader* shader, const PixelShaderVariant& variant) {
    for (auto& entry : shader->m_pixelFunctions) {
        if (entry.first == variant) {
            return entry.second;
        }
    }

    CompiledFunction function;
    std::string source;
    std::string error;

    if (!TranslatePixelShader(shader->m_program, variant, source, error)) {
        Log("pixel shader translation failed: %s", error.c_str());
    } else {
        this->CompileFunction(source, false, function);
    }

    shader->m_pixelFunctions.emplace_back(variant, function);
    return function;
}

CompiledFunction PipelineCache::FixedVertexFunction(const FixedVertexKey& key) {
    auto found = this->m_fixedVertex.find(key);
    if (found != this->m_fixedVertex.end()) {
        return found->second;
    }

    CompiledFunction function;
    std::string source;
    std::string error;

    if (GenerateFixedVertexShader(key, source, error)) {
        this->CompileFunction(source, true, function);
    } else {
        Log("fixed-function vertex shader generation failed: %s", error.c_str());
    }

    this->m_fixedVertex[key] = function;
    return function;
}

CompiledFunction PipelineCache::FixedPixelFunction(const FixedPixelKey& key) {
    auto found = this->m_fixedPixel.find(key);
    if (found != this->m_fixedPixel.end()) {
        return found->second;
    }

    CompiledFunction function;
    std::string source;
    std::string error;

    if (GenerateFixedPixelShader(key, source, error)) {
        this->CompileFunction(source, false, function);
    } else {
        Log("fixed-function pixel shader generation failed: %s", error.c_str());
    }

    this->m_fixedPixel[key] = function;
    return function;
}

id<MTLRenderPipelineState> PipelineCache::Pipeline(const PipelineKey& key, const CompiledFunction& vertex, const CompiledFunction& fragment, const VertexLayout& layout) {
    auto found = this->m_pipelines.find(key);
    if (found != this->m_pipelines.end()) {
        return found->second;
    }

    MTLRenderPipelineDescriptor* descriptor = [MTLRenderPipelineDescriptor new];
    descriptor.vertexFunction = vertex.m_function;
    descriptor.fragmentFunction = fragment.m_function;
    descriptor.rasterSampleCount = key.m_samples;

    MTLVertexDescriptor* vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
    for (const VertexLayout::Mapped& element : layout.m_mapped) {
        vertexDescriptor.attributes[element.m_attribute].format = element.m_format;
        vertexDescriptor.attributes[element.m_attribute].offset = element.m_offset;
        vertexDescriptor.attributes[element.m_attribute].bufferIndex = element.m_stream;
    }
    for (uint32_t stream = 0; stream < 16; stream++) {
        if (!(layout.m_streamMask & (1u << stream))) {
            continue;
        }
        if (key.m_strides[stream] == 0) {
            // A stream without stride repeats its first vertex
            vertexDescriptor.layouts[stream].stepFunction = MTLVertexStepFunctionConstant;
            vertexDescriptor.layouts[stream].stepRate = 0;
            vertexDescriptor.layouts[stream].stride = 16;
        } else {
            vertexDescriptor.layouts[stream].stride = key.m_strides[stream];
        }
    }
    descriptor.vertexDescriptor = vertexDescriptor;

    MTLRenderPipelineColorAttachmentDescriptor* attachment = descriptor.colorAttachments[0];
    attachment.pixelFormat = static_cast<MTLPixelFormat>(key.m_colorFormat);
    attachment.writeMask = key.m_writeMask;

    if (key.m_blendEnable) {
        attachment.blendingEnabled = YES;
        attachment.sourceRGBBlendFactor = static_cast<MTLBlendFactor>(key.m_srcBlend);
        attachment.destinationRGBBlendFactor = static_cast<MTLBlendFactor>(key.m_dstBlend);
        attachment.rgbBlendOperation = MTLBlendOperationAdd;
        attachment.sourceAlphaBlendFactor = static_cast<MTLBlendFactor>(key.m_srcBlend);
        attachment.destinationAlphaBlendFactor = static_cast<MTLBlendFactor>(key.m_dstBlend);
        attachment.alphaBlendOperation = MTLBlendOperationAdd;
    }

    descriptor.depthAttachmentPixelFormat = static_cast<MTLPixelFormat>(key.m_depthFormat);
    if (key.m_depthFormat == MTLPixelFormatDepth32Float_Stencil8) {
        descriptor.stencilAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
    }

    NSError* error = nil;
    double started = CompileClock();
    id<MTLRenderPipelineState> pipeline = [this->m_device newRenderPipelineStateWithDescriptor:descriptor error:&error];
    g_compileSeconds += CompileClock() - started;
    g_compiles++;
    if (!pipeline) {
        Log("pipeline creation failed: %s", error.localizedDescription.UTF8String);
    }

    this->m_pipelines[key] = pipeline;
    return pipeline;
}

id<MTLDepthStencilState> PipelineCache::DepthStencil(MTLCompareFunction compare, bool write, bool stencilTag) {
    uint32_t key = static_cast<uint32_t>(compare) | (write ? 0x100u : 0u) | (stencilTag ? 0x200u : 0u);

    auto found = this->m_depthStates.find(key);
    if (found != this->m_depthStates.end()) {
        return found->second;
    }

    MTLDepthStencilDescriptor* descriptor = [MTLDepthStencilDescriptor new];
    descriptor.depthCompareFunction = compare;
    descriptor.depthWriteEnabled = write;

    if (stencilTag) {
        // Every fragment that passes the depth test writes the encoder's reference (GxMetalSetStencilTag)
        MTLStencilDescriptor* stencil = [MTLStencilDescriptor new];
        stencil.stencilCompareFunction = MTLCompareFunctionAlways;
        stencil.stencilFailureOperation = MTLStencilOperationKeep;
        stencil.depthFailureOperation = MTLStencilOperationKeep;
        stencil.depthStencilPassOperation = MTLStencilOperationReplace;
        stencil.readMask = 0xFF;
        stencil.writeMask = 0xFF;
        descriptor.frontFaceStencil = stencil;
        descriptor.backFaceStencil = stencil;
    }

    id<MTLDepthStencilState> state = [this->m_device newDepthStencilStateWithDescriptor:descriptor];
    this->m_depthStates[key] = state;
    return state;
}

id<MTLSamplerState> PipelineCache::Sampler(const SamplerKey& key) {
    auto found = this->m_samplers.find(key);
    if (found != this->m_samplers.end()) {
        return found->second;
    }

    // The filters of CGxDeviceD3d::s_filterModes: minification and magnification, then mipmaps
    MTLSamplerDescriptor* descriptor = [MTLSamplerDescriptor new];
    bool nearest = key.m_filter == GxTex_Nearest || key.m_filter == GxTex_NearestMipNearest;
    descriptor.minFilter = nearest ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
    descriptor.magFilter = descriptor.minFilter;

    switch (key.m_filter) {
        case GxTex_Nearest:
        case GxTex_Linear:
            descriptor.mipFilter = MTLSamplerMipFilterNotMipmapped;
            break;
        case GxTex_NearestMipNearest:
        case GxTex_LinearMipNearest:
            descriptor.mipFilter = MTLSamplerMipFilterNearest;
            break;
        default:
            descriptor.mipFilter = MTLSamplerMipFilterLinear;
            break;
    }

    // Anisotropy applies only to the anisotropic filter, as with Direct3D 9's
    descriptor.maxAnisotropy = key.m_filter == GxTex_Anisotropic ? std::clamp<uint32_t>(key.m_maxAnisotropy, 1, 16) : 1;

    descriptor.sAddressMode = key.m_wrapU ? MTLSamplerAddressModeRepeat : MTLSamplerAddressModeClampToEdge;
    descriptor.tAddressMode = key.m_wrapV ? MTLSamplerAddressModeRepeat : MTLSamplerAddressModeClampToEdge;
    descriptor.rAddressMode = MTLSamplerAddressModeClampToEdge;

    if (key.m_compare) {
        // Hardware shadow mapping: the texture coordinate's depth passes when not behind the stored one
        descriptor.compareFunction = MTLCompareFunctionLessEqual;
    }

    id<MTLSamplerState> state = [this->m_device newSamplerStateWithDescriptor:descriptor];
    this->m_samplers[key] = state;
    return state;
}

}
