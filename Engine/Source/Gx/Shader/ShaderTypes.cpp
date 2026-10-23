#include "Engine/Source/Gx/Shader/ShaderTypes.h"

#include <cstring>
#include "Engine/Source/Gx/Shader/ShaderBytecode.h"

namespace GxShader {

int32_t AttributeIndex(uint32_t usage, uint32_t usageIndex) {
    switch (usage) {
        case USAGE_POSITION:
            return usageIndex == 0 ? ATTRIB_POSITION : -1;
        case USAGE_BLENDWEIGHT:
            return usageIndex == 0 ? ATTRIB_BLENDWEIGHT : -1;
        case USAGE_BLENDINDICES:
            return usageIndex == 0 ? ATTRIB_BLENDINDICES : -1;
        case USAGE_NORMAL:
            return usageIndex == 0 ? ATTRIB_NORMAL : -1;
        case USAGE_COLOR:
            return usageIndex < 2 ? ATTRIB_COLOR0 + static_cast<int32_t>(usageIndex) : -1;
        case USAGE_TEXCOORD:
            return usageIndex < 8 ? ATTRIB_TEXCOORD0 + static_cast<int32_t>(usageIndex) : -1;
        default:
            return -1;
    }
}

bool VertexInputSignature::operator==(const VertexInputSignature& other) const {
    return memcmp(this->m_class, other.m_class, sizeof(this->m_class)) == 0;
}

}
