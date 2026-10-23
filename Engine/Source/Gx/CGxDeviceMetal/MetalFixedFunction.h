// Generates Metal Shading Language for the client's fixed-function pipeline (Shader/FixedFunction.h
// has the keys). Each distinct state combination becomes its own small function, which the device
// caches by key. GxFixedVertexState in the generated source is FixedVertexStateBlock, which Metal
// reads as transposed matrices, so the generated code multiplies matrix * vector.

#ifndef ENGINE_SOURCE_GX_CGXDEVICEMETAL_METALFIXEDFUNCTION_H
#define ENGINE_SOURCE_GX_CGXDEVICEMETAL_METALFIXEDFUNCTION_H

#include <cstdint>
#include <string>
#include "Engine/Source/Gx/CGxDeviceMetal/Shader/ShaderTranslator.h"
#include "Engine/Source/Gx/Shader/FixedFunction.h"

namespace GxMetal {

bool GenerateFixedVertexShader(const FixedVertexKey& key, std::string& source, std::string& error);
bool GenerateFixedPixelShader(const FixedPixelKey& key, std::string& source, std::string& error);

}

#endif
