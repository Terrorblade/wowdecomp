// State objects of the Metal graphics device (Objective-C++ only): compiled shader functions,
// vertex layouts, and the caches of render pipelines, depth states and samplers the native state
// block (CGxDeviceMetal::m_hw) is turned into at draw time.

#ifndef ENGINE_SOURCE_GX_CGXDEVICEMETAL_METALPIPELINE_H
#define ENGINE_SOURCE_GX_CGXDEVICEMETAL_METALPIPELINE_H

#import <Metal/Metal.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
#include "Engine/Source/Gx/CGxDeviceMetal/MetalFixedFunction.h"
#include "Engine/Source/Gx/CGxDeviceMetal/MetalResources.h"
#include "Engine/Source/Gx/Shader/ShaderBytecode.h"
#include "Engine/Source/Gx/CGxDeviceMetal/Shader/ShaderTranslator.h"
#include "Engine/Source/Gx/Types.h"

namespace GxMetal {

// Seconds spent compiling shader functions and pipelines on the calling thread, for the frame statistics
extern double g_compileSeconds;
extern uint32_t g_compiles;


struct CompiledFunction {
    id<MTLFunction> m_function = nil;
    uint64_t m_id = 0;
};

// A CGxShader: the parsed bytecode of a .bls permutation and its translations. A vertex shader
// is translated once per vertex input signature, a pixel shader once per variant (sampler kinds,
// projected stages, point sprites).
class Shader : public RefCounted {
    public:
    // Null when the bytecode is malformed or of the other stage
    static Shader* Create(const uint32_t* tokens, uint32_t sizeInBytes, bool vertex);

    ShaderProgram m_program;
    ShaderInfo m_info;
    std::vector<std::pair<VertexInputSignature, CompiledFunction>> m_vertexFunctions;
    std::vector<std::pair<PixelShaderVariant, CompiledFunction>> m_pixelFunctions;
};

// How the vertex streams of a draw feed the attributes: built from the client's vertex
// attributes (CGxVertexAttrib), one Metal attribute per Gx attribute (EGxVertexAttrib order is
// the attribute index the translator uses)
struct VertexElement {
    uint32_t m_attribute;                 // EGxVertexAttrib
    uint32_t m_stream;
    uint32_t m_offset;
    uint32_t m_type;                      // CGxVertexAttrib::m_type
};

class VertexLayout {
    public:
    // Returns false when an element has no Metal equivalent
    bool Init(const VertexElement* elements, uint32_t count, uint64_t id);
    bool Matches(const VertexElement* elements, uint32_t count) const;

    struct Mapped {
        uint32_t m_attribute;
        uint32_t m_stream;
        uint32_t m_offset;
        MTLVertexFormat m_format;
    };

    std::vector<VertexElement> m_elements;
    std::vector<Mapped> m_mapped;
    VertexInputSignature m_signature;
    uint8_t m_texcoordComponents[8] = {};
    uint32_t m_streamMask = 0;
    uint64_t m_id = 0;
};

struct PipelineKey {
    uint64_t m_vertexFunction;
    uint64_t m_fragmentFunction;
    uint64_t m_layout;
    uint16_t m_strides[16];
    uint32_t m_colorFormat;
    uint32_t m_depthFormat;
    uint8_t m_samples;
    uint8_t m_blendEnable;
    uint8_t m_srcBlend;                   // MTLBlendFactor, the same for colour and alpha
    uint8_t m_dstBlend;
    uint8_t m_writeMask;                  // MTLColorWriteMask
    uint8_t m_padding[3];

    PipelineKey() { memset(this, 0, sizeof(*this)); }
    bool operator==(const PipelineKey& other) const { return memcmp(this, &other, sizeof(*this)) == 0; }
};

// A sampler as CGxTexFlags describe it
struct SamplerKey {
    uint8_t m_filter;                     // EGxTexFilter
    uint8_t m_wrapU;
    uint8_t m_wrapV;
    uint8_t m_maxAnisotropy;
    uint8_t m_compare;                    // shadow comparison for depth textures
    uint8_t m_padding[3];

    SamplerKey() { memset(this, 0, sizeof(*this)); }
    bool operator==(const SamplerKey& other) const { return memcmp(this, &other, sizeof(*this)) == 0; }
};

template <typename T>
struct BytewiseHash {
    size_t operator()(const T& value) const {
        // FNV-1a over the key's bytes; keys are zero-initialized including padding
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
        uint64_t hash = 0xCBF29CE484222325ull;
        for (size_t i = 0; i < sizeof(T); i++) {
            hash = (hash ^ bytes[i]) * 0x100000001B3ull;
        }
        return static_cast<size_t>(hash);
    }
};

class PipelineCache {
    public:
    void Init(id<MTLDevice> device);

    uint64_t NextId() { return ++this->m_nextId; }
    // Compiles MSL source, once per distinct source
    bool CompileFunction(const std::string& source, bool vertex, CompiledFunction& function);

    CompiledFunction VertexShaderFunction(Shader* shader, const VertexInputSignature& signature);
    CompiledFunction PixelShaderFunction(Shader* shader, const PixelShaderVariant& variant);
    CompiledFunction FixedVertexFunction(const FixedVertexKey& key);
    CompiledFunction FixedPixelFunction(const FixedPixelKey& key);

    id<MTLRenderPipelineState> Pipeline(const PipelineKey& key, const CompiledFunction& vertex, const CompiledFunction& fragment, const VertexLayout& layout);
    id<MTLDepthStencilState> DepthStencil(MTLCompareFunction compare, bool write, bool stencilTag = false);
    id<MTLSamplerState> Sampler(const SamplerKey& key);

    private:
    id<MTLDevice> m_device = nil;
    uint64_t m_nextId = 0;
    std::unordered_map<std::string, CompiledFunction> m_functions;
    // Shader sources compiled in earlier runs, compiled again on background threads at start so their first
    // draw does not wait for the compiler (each takes tens of milliseconds); shared with those threads
    struct Prewarm;
    std::shared_ptr<Prewarm> m_prewarm;
    std::unordered_map<FixedVertexKey, CompiledFunction, BytewiseHash<FixedVertexKey>> m_fixedVertex;
    std::unordered_map<FixedPixelKey, CompiledFunction, BytewiseHash<FixedPixelKey>> m_fixedPixel;
    std::unordered_map<PipelineKey, id<MTLRenderPipelineState>, BytewiseHash<PipelineKey>> m_pipelines;
    std::unordered_map<uint32_t, id<MTLDepthStencilState>> m_depthStates;
    std::unordered_map<SamplerKey, id<MTLSamplerState>, BytewiseHash<SamplerKey>> m_samplers;
};

}

#endif
