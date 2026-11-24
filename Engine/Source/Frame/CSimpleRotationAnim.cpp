#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleRotationAnim.h"
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/Frame/CSimpleAnim.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Frame/CScriptRegion.h"
#include "Engine/Source/Frame/CSimpleAnimGroup.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/Tempest/CMath.h"

// STATIC: 0x00AC1A20
int32_t CSimpleRotationAnim::s_metatable = LUA_NOREF;

// STATIC: 0x00B499F0
int32_t CSimpleRotationAnim::s_objectType;

// STATIC: 0x00AC1394
const char* CSimpleRotationAnim::s_objectTypeName = "Rotation";

// INLINED
int32_t CSimpleRotationAnim::GetObjectType() {
    if (!CSimpleRotationAnim::s_objectType) {
        CSimpleRotationAnim::s_objectType = ++FrameScript_Object::s_objectTypes;
    }

    return CSimpleRotationAnim::s_objectType;
}

// VERIFY: 1314989f auto
// OFFSET: 0x00498EC0
bool CSimpleRotationAnim::IsA(int32_t type) {
    return type == CSimpleRotationAnim::s_objectType || type == CSimpleAnim::s_objectType || type == CScriptObject::s_objectType;
}

// VERIFY: 2bc3e3cb auto
// OFFSET: 0x00498EF0
bool CSimpleRotationAnim::IsA(const char* typeName) {
    return !SStrCmpI(typeName, CSimpleRotationAnim::s_objectTypeName, STORM_MAX_STR) || !SStrCmpI(typeName, CSimpleAnim::s_objectTypeName, STORM_MAX_STR) || !SStrCmpI(typeName, CScriptObject::s_objectTypeName, STORM_MAX_STR);
}

// VERIFY: eb41dcd6 auto
// OFFSET: 0x00498F50
const char* CSimpleRotationAnim::GetObjectTypeName() {
    return CSimpleRotationAnim::s_objectTypeName;
}

// VERIFY: 3bdde5e1 auto
// OFFSET: 0x00498F60
int32_t CSimpleRotationAnim::GetScriptMetaTable() {
    return CSimpleRotationAnim::s_metatable;
}

// VERIFY: d7e5fa4a auto
// OFFSET: 0x00497700
void CSimpleRotationAnim::ApplyInverse(float progress) {
    this->Apply(-progress);
}

// VERIFY: 90201cc7 manual
// OFFSET: 0x00498090
void CSimpleRotationAnim::Apply(float progress) {
    CScriptRegion* region = this->GetRegionParent();

    if (region) {
        region->AddAnimRotation(this->m_origin, this->m_originOffset, this->m_radians * progress);
    }
}

// VERIFY: 89166521 manual
// OFFSET: 0x004997E0
CSimpleRotationAnim::~CSimpleRotationAnim() {
}

// VERIFY: a8dd2640 auto
// OFFSET: 0x00499C80
CSimpleRotationAnim::CSimpleRotationAnim(CSimpleAnimGroup* group) : CSimpleAnim(group) {
    this->m_origin = FRAMEPOINT_CENTER;
    this->m_radians = 0.0f;
}

// VERIFY: 502c3f17 manual
// OFFSET: 0x0049BC10
void CSimpleRotationAnim::LoadXML(XMLNode* node, CStatus* status) {
    CSimpleAnim::LoadXML(node, status);

    const char* degrees = node->GetAttributeByName("degrees");

    if (degrees && *degrees) {
        this->m_radians = SStrToFloat(degrees) * 0.017453292f;
    }

    const char* radians = node->GetAttributeByName("radians");

    if (radians && *radians) {
        this->m_radians = SStrToFloat(radians);
    }

    for (XMLNode* child = node->m_child; child; child = child->m_next) {
        if (!SStrCmpI(child->m_name.m_str, "Scripts", STORM_MAX_STR)) {
            this->LoadXML_Scripts(child, status);
        } else if (!SStrCmpI(child->m_name.m_str, "Origin", STORM_MAX_STR) && !LoadXML_AnimOrigin(child, this->m_origin, this->m_originOffset, status)) {
            const char* name = this->GetName();
            status->Add(STATUS_WARNING, "%s %s: Error loading Origin element", this->GetObjectTypeName(), name ? name : "<unnamed>");
        }
    }
}
