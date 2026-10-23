// Generates shader model 4.0 DXBC for the client's fixed-function pipeline (Shader/FixedFunction.h has
// the keys). The shaders write and read the same varyings as translated shaders, so fixed-function and
// programmable stages link with each other as in Direct3D 9.

#ifndef ENGINE_SOURCE_GX_CGXDEVICED3D11_SHADER_FIXEDFUNCTION_H
#define ENGINE_SOURCE_GX_CGXDEVICED3D11_SHADER_FIXEDFUNCTION_H

#include <cstdint>
#include <string>
#include <vector>
#include "Engine/Source/Gx/CGxDeviceD3d11/Shader/ShaderTranslator.h"
#include "Engine/Source/Gx/Shader/FixedFunction.h"

namespace GxD3d11 {

// Registers of FixedVertexStateBlock in cb2
enum FixedVertexRegister {
    FFREG_WORLD = 0,
    FFREG_WORLD_VIEW = 4,
    FFREG_NORMAL = 8,
    FFREG_PROJECTION = 12,
    FFREG_TEXTURE = 16,
    FFREG_GLOBAL_AMBIENT = 48,
    FFREG_MATERIAL_DIFFUSE = 49,
    FFREG_MATERIAL_AMBIENT = 50,
    FFREG_MATERIAL_SPECULAR = 51,
    FFREG_MATERIAL_EMISSIVE = 52,
    FFREG_MATERIAL_POWER = 53,
    FFREG_LIGHTS = 54,
    FFREG_LIGHT_SIZE = 6,
    FFREG_FOG = 102,
    FFREG_POINT_SCALE = 103,
    FFREG_COUNT = 104,
};

static_assert(sizeof(FixedVertexStateBlock) == FFREG_COUNT * 16, "FixedVertexStateBlock layout");

bool GenerateFixedVertexShader(const FixedVertexKey& key, std::vector<uint8_t>& code, std::string& error);
bool GenerateFixedPixelShader(const FixedPixelKey& key, bool alphaTest, std::vector<uint8_t>& code, std::string& error);

}

#endif
