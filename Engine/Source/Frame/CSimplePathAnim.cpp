#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimplePathAnim.h"
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/Frame/CSimpleAnim.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include <cmath>
#include <new>
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Frame/CScriptRegion.h"
#include "Engine/Source/Frame/CSimpleAnimGroup.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/Tempest/C2Vector.h"

// STATIC: 0x00AC1A9C
int32_t CSimplePathAnim::s_metatable = LUA_NOREF;

// STATIC: 0x00B499E4
int32_t CSimplePathAnim::s_objectType;

// STATIC: 0x00AC13A0
const char* CSimplePathAnim::s_objectTypeName = "Path";

// VERIFY: e2f2ece5 auto
// OFFSET: 0x004A4F00
int32_t CSimplePathAnim::GetObjectType() {
    if (!CSimplePathAnim::s_objectType) {
        CSimplePathAnim::s_objectType = ++FrameScript_Object::s_objectTypes;
    }

    return CSimplePathAnim::s_objectType;
}

// VERIFY: efbd8198 auto
// OFFSET: 0x00499940
bool CSimplePathAnim::IsA(int32_t type) {
    return type == CSimplePathAnim::s_objectType || type == CSimpleAnim::s_objectType || type == CScriptObject::s_objectType;
}

// VERIFY: 7c5c147c auto
// OFFSET: 0x00499970
bool CSimplePathAnim::IsA(const char* typeName) {
    return !SStrCmpI(typeName, CSimplePathAnim::s_objectTypeName, STORM_MAX_STR) || !SStrCmpI(typeName, CSimpleAnim::s_objectTypeName, STORM_MAX_STR) || !SStrCmpI(typeName, CScriptObject::s_objectTypeName, STORM_MAX_STR);
}

// VERIFY: 932c67c7 auto
// OFFSET: 0x004999D0
const char* CSimplePathAnim::GetObjectTypeName() {
    return CSimplePathAnim::s_objectTypeName;
}

// VERIFY: 62e4eeb6 auto
// OFFSET: 0x004999E0
int32_t CSimplePathAnim::GetScriptMetaTable() {
    return CSimplePathAnim::s_metatable;
}

// VERIFY: 89ec7e89 auto
// OFFSET: 0x00497700
void CSimplePathAnim::ApplyInverse(float progress) {
    this->Apply(-progress);
}

// VERIFY: 1089655c auto
// OFFSET: 0x00498230
CSimpleControlPoint* CSimplePathAnim::GetControlPoint(uint32_t index) {
    return index < this->m_sortedPoints.Count() ? this->m_sortedPoints[index] : nullptr;
}

// VERIFY: e18738d7 auto
// OFFSET: 0x00498260
int32_t CSimplePathAnim::GetMaxControlPointOrder() {
    if (!this->m_sortedPoints.Count()) {
        return -1;
    }

    return this->m_sortedPoints[this->m_sortedPoints.Count() - 1]->m_order;
}

// VERIFY: 1feb6d37 auto
// OFFSET: 0x00498280
int32_t CSimplePathAnim::FindControlPoint(int32_t order) {
    if (order == -1) {
        return -1;
    }

    int32_t low = 0;
    int32_t high = this->m_sortedPoints.Count() - 1;

    while (low <= high) {
        uint32_t middle = static_cast<uint32_t>(high + low) >> 1;
        CSimpleControlPoint* point = this->m_sortedPoints[middle];

        if (point) {
            if (point->m_order < order) {
                low = middle + 1;
            } else if (point->m_order > order) {
                high = middle - 1;
            } else {
                return middle;
            }
        }
    }

    return -1;
}

// VERIFY: 956681d1 manual
// OFFSET: 0x004989B0
void CSimplePathAnim::UpdateControlPointPositions() {
    uint32_t count = this->m_sortedPoints.Count();

    if (!count) {
        return;
    }

    float step = 1.0f / count;

    for (uint32_t i = 0; i < this->m_sortedPoints.Count(); i++) {
        float position = (i + 1) * step;
        CSimpleControlPoint* point = this->m_sortedPoints[i];

        if (position < 0.0f) {
            position = 0.0f;
        } else if (position >= 1.0f) {
            position = 1.0f;
        }

        point->m_position = position;
    }
}

// VERIFY: c544ae9e manual
// OFFSET: 0x00499090
void CSimplePathAnim::NotifyOrderChanged(CSimpleControlPoint* point) {
    if (!point) {
        return;
    }

    int32_t index = this->FindControlPoint(point->m_order);

    if (index >= 0) {
        this->m_controlPoints.LinkNode(point, 2, this->m_sortedPoints[index]);
    }

    this->BuildSpline();
    this->UpdateControlPointPositions();
}

// VERIFY: 02e2d2dd manual
// OFFSET: 0x00499140
void CSimplePathAnim::Apply(float progress) {
    uint32_t count = this->m_sortedPoints.Count();

    if (!count) {
        return;
    }

    CScriptRegion* region = this->GetRegionParent();

    if (!region) {
        return;
    }

    float t = fabsf(progress);
    float scaled = count * t;
    float biased = fabsf(scaled) + 65536.0f;
    uint32_t sign = *reinterpret_cast<uint32_t*>(&scaled) >> 31;
    int16_t index = static_cast<int16_t>((((*reinterpret_cast<int32_t*>(&biased) >> 7) & 0x7FFF) ^ sign) - sign);

    C2Vector position;

    if (static_cast<uint32_t>(index) >= count) {
        position = this->m_sortedPoints[count - 1]->m_offset;
    } else if (this->m_curve && count > 1) {
        C2Vector p0;
        C2Vector p1;
        C2Vector p2;
        C2Vector p3;
        float t0;
        float t1;

        if (index) {
            CSimpleControlPoint* prev = this->m_sortedPoints[index - 1];
            CSimpleControlPoint* next = this->m_sortedPoints[index];
            p1 = prev->m_offset;
            t0 = prev->m_position;
            p2 = next->m_offset;
            t1 = next->m_position;

            if (index != 1) {
                p0 = this->m_sortedPoints[index - 2]->m_offset;
            }

            if (index == count - 1) {
                p3.x = p2.x * 2.0f - p1.x;
                p3.y = p2.y * 2.0f - p1.y;
            } else {
                p3 = this->m_sortedPoints[index + 1]->m_offset;
            }
        } else {
            CSimpleControlPoint* first = this->m_sortedPoints[0];
            t0 = 0.0f;
            p2 = first->m_offset;
            t1 = first->m_position;
            p0.x = -p2.x;
            p0.y = -p2.y;
            p3 = this->m_sortedPoints[1]->m_offset;
        }

        float f = (t - t0) / (t1 - t0);
        float f2 = f * f;
        float f3 = f2 * f;

        C2Vector a;
        a.x = -p0.x + p1.x * 3.0f - p2.x * 3.0f + p3.x;
        a.y = -p0.y + p1.y * 3.0f - p2.y * 3.0f + p3.y;

        C2Vector b;
        b.x = p0.x * 2.0f - p1.x * 5.0f + p2.x * 4.0f - p3.x;
        b.y = p0.y * 2.0f - p1.y * 5.0f + p2.y * 4.0f - p3.y;

        C2Vector c;
        c.x = -p0.x + p2.x;
        c.y = -p0.y + p2.y;

        position.x = (c.x * f + p1.x * 2.0f + b.x * f2 + a.x * f3) * 0.5f;
        position.y = (c.y * f + p1.y * 2.0f + b.y * f2 + a.y * f3) * 0.5f;
    } else {
        C2Vector a;
        C2Vector b;
        float t0;
        float t1;

        if (index) {
            CSimpleControlPoint* prev = this->m_sortedPoints[index - 1];
            CSimpleControlPoint* next = this->m_sortedPoints[index];
            a = prev->m_offset;
            t0 = prev->m_position;
            b = next->m_offset;
            t1 = next->m_position;
        } else {
            CSimpleControlPoint* first = this->m_sortedPoints[0];
            t0 = 0.0f;
            b = first->m_offset;
            t1 = first->m_position;
        }

        float f = (t - t0) / (t1 - t0);
        position.x = a.x + (b.x - a.x) * f;
        position.y = f * (b.y - a.y) + a.y;
    }

    if (progress >= 0.0f) {
        region->AddAnimTranslation(position);
    } else {
        C2Vector negative;
        negative.x = -position.x;
        negative.y = -position.y;
        region->AddAnimTranslation(negative);
    }
}

// VERIFY: 02d2723f manual
// OFFSET: 0x00499550
int32_t CSimplePathAnim::RegisterSplineControlPoint(CSimpleControlPoint* point) {
    int32_t count = this->m_sortedPoints.Count();

    if (!count) {
        return this->m_sortedPoints.New(point) ? 0 : -1;
    }

    int32_t index = count;
    int32_t order = point->m_order;

    for (int32_t i = 0; i < count; i++) {
        CSimpleControlPoint* existing = this->m_sortedPoints[i];

        if (!existing) {
            this->m_sortedPoints[i] = point;
            return i;
        }

        if (order == existing->m_order) {
            return i;
        }

        if (order < existing->m_order) {
            index = i;
            break;
        }
    }

    if (!this->m_sortedPoints.New()) {
        return -1;
    }

    for (int32_t i = this->m_sortedPoints.Count() - 1; i > index; i--) {
        this->m_sortedPoints[i] = this->m_sortedPoints[i - 1];
    }

    this->m_sortedPoints[index] = point;
    return index;
}

// VERIFY: c9fbb0d2 manual
// OFFSET: [0x004998A0, 0x00499DB0]
CSimplePathAnim::~CSimplePathAnim() {
    this->m_sortedPoints.Clear();
    this->m_controlPoints.Clear();
}

// VERIFY: e8ce9c43 auto
// OFFSET: 0x00499D50
CSimplePathAnim::CSimplePathAnim(CSimpleAnimGroup* group) : CSimpleAnim(group) {
    this->m_curve = ANIM_CURVE_NONE;
}

// VERIFY: 82fe553b manual
// OFFSET: 0x00499DE0
void CSimplePathAnim::LoadXML_ControlPoint(XMLNode* node, CStatus* status) {
    void* mem = SMemAlloc(sizeof(CSimpleControlPoint), __FILE__, __LINE__, 0);

    if (!mem) {
        return;
    }

    CSimpleControlPoint* point = new (mem) CSimpleControlPoint(this);

    const char* name = node->GetAttributeByName("name");

    if (name && *name) {
        point->SetName(name);
    }

    point->LoadXML(node, status);
}

// VERIFY: 5973c9b8 manual
// OFFSET: 0x0049AF20
void CSimplePathAnim::BuildSpline() {
    this->Stop(false);

    if (!this->m_controlPoints.Head()) {
        this->m_sortedPoints.Clear();
        return;
    }

    for (uint32_t i = 0; i < this->m_sortedPoints.Count(); i++) {
        this->m_sortedPoints[i] = nullptr;
    }

    int32_t nextOrder = 0;

    for (CSimpleControlPoint* point = this->m_controlPoints.Head(); point; point = this->m_controlPoints.Next(point)) {
        int32_t order = point->m_order;

        if (order == -1 || order < nextOrder) {
            order = nextOrder++;
            point->m_order = order < 0 ? 0 : order;
        } else {
            nextOrder = order + 1;
        }

        if (nextOrder > 99) {
            nextOrder = 0;
        }

        this->RegisterSplineControlPoint(point);
    }

    int32_t count = this->m_sortedPoints.Count();
    int32_t last = count - 1;

    while (last >= 0 && !this->m_sortedPoints[last]) {
        last--;
    }

    if (last < 0) {
        return;
    }

    uint32_t newCount = last + 1;

    if (static_cast<int32_t>(newCount) < count) {
        this->m_sortedPoints.SetCount(newCount);

        if (newCount) {
            this->m_sortedPoints.ReallocData(newCount);
        } else {
            this->m_sortedPoints.Clear();
        }
    }
}

// VERIFY: 22838652 manual
// OFFSET: 0x0049BF00
void CSimplePathAnim::LoadXML(XMLNode* node, CStatus* status) {
    CSimpleAnim::LoadXML(node, status);

    const char* curve = node->GetAttributeByName("curve");

    if (curve && *curve) {
        SimpleAnimCurveType curveType;

        if (StringToCurveType(curve, curveType)) {
            this->m_curve = curveType;
        } else {
            const char* name = this->GetName();
            status->Add(STATUS_WARNING, "%s %s: Invalid curve value: %s", this->GetObjectTypeName(), name ? name : "<unnamed>", curve);
        }
    }

    XMLNode* controlPoints = node->GetChildByName("ControlPoints");

    if (!controlPoints) {
        return;
    }

    for (XMLNode* child = controlPoints->m_child; child; child = child->m_next) {
        if (SStrCmpI(child->m_name.m_str, "ControlPoint", STORM_MAX_STR)) {
            const char* name = this->GetName();
            status->Add(STATUS_WARNING, "%s %s: Unknown child node in %s element: %s", this->GetObjectTypeName(), name ? name : "<unnamed>", controlPoints->m_name.m_str, child->m_name.m_str);
        } else {
            this->LoadXML_ControlPoint(child, status);
        }
    }
}

// VERIFY: f10c3adc auto
// OFFSET: 0x0049C040
void CSimplePathAnim::PostLoadXML(XMLNode* node, CStatus* status) {
    this->m_flags ^= 1;
    this->BuildSpline();
    this->UpdateControlPointPositions();

    if (this->m_onLoad.m_luaRef) {
        this->RunScript(this->m_onLoad, 0, nullptr);
    }
}

// VERIFY: 6a79e6ce manual
// OFFSET: 0x0049C070
void CSimplePathAnim::NotifyControlPointAdded(CSimpleControlPoint* point) {
    if (!point) {
        return;
    }

    int32_t index = this->FindControlPoint(point->m_order);

    if (index < 0) {
        if (this->RegisterSplineControlPoint(point) >= 0) {
            this->UpdateControlPointPositions();
        }
    } else {
        this->m_controlPoints.LinkNode(point, 2, this->m_sortedPoints[index]);
        this->BuildSpline();
        this->UpdateControlPointPositions();
    }
}

// VERIFY: 72b6007e auto
// OFFSET: 0x0049C140
void CSimplePathAnim::NotifyControlPointRemoved(CSimpleControlPoint* point) {
    // SetParent can change the order first, so look the point up by pointer to drop every stale entry.
    bool found = false;

    for (uint32_t i = 0; point && i < this->m_sortedPoints.Count(); i++) {
        if (this->m_sortedPoints[i] == point) {
            found = true;
        }
    }

    if (found) {
        this->BuildSpline();
        this->UpdateControlPointPositions();
    }
}
