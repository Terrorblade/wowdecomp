#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleScrollFrame.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Frame/CRenderBatch.h"
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/Frame/CScriptRegion.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Services/Camera.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/CRect.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// STATIC: 0x00B2CED8
int32_t CSimpleScrollFrame::s_metatable = LUA_NOREF;

// STATIC: 0x00DCE4BC
int32_t CSimpleScrollFrame::s_objectType;

// STATIC: 0x00B2CCF0
const char* CSimpleScrollFrame::s_objectTypeName = "ScrollFrame";

// STATIC: 0x00DCE4C0
CDataAllocator CSimpleScrollFrame::s_allocator(sizeof(CSimpleScrollFrame), 5);

// INLINED
void CSimpleScrollFrame::operator delete(void* ptr) {
    if (ptr) {
        CSimpleScrollFrame::s_allocator.PutData(ptr, nullptr, 0);
    }
}

// INLINED
int32_t CSimpleScrollFrame::GetObjectType() {
    if (!CSimpleScrollFrame::s_objectType) {
        CSimpleScrollFrame::s_objectType = ++FrameScript_Object::s_objectTypes;
    }

    return CSimpleScrollFrame::s_objectType;
}

// VERIFY: c99507b4 auto
// OFFSET: 0x0096AF10
void CSimpleScrollFrame::RunOnHorizontalScrollScript() {
    if (this->m_onHorizontalScroll.m_luaRef) {
        lua_State* L = FrameScript_GetContext();
        lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * this->m_scrollOffset.x));
        this->RunScript(this->m_onHorizontalScroll, 1, nullptr);
    }
}

// VERIFY: 53321fee auto
// OFFSET: 0x0096AF70
void CSimpleScrollFrame::RunOnVerticalScrollScript() {
    if (this->m_onVerticalScroll.m_luaRef) {
        lua_State* L = FrameScript_GetContext();
        lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * this->m_scrollOffset.y));
        this->RunScript(this->m_onVerticalScroll, 1, nullptr);
    }
}

// VERIFY: 8c11b6c4 auto
// OFFSET: 0x0096AFD0
void CSimpleScrollFrame::RunOnScrollRangeChangedScript() {
    if (this->m_onScrollRangeChanged.m_luaRef) {
        lua_State* L = FrameScript_GetContext();
        lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * this->m_scrollRange.x));
        lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * this->m_scrollRange.y));
        this->RunScript(this->m_onScrollRangeChanged, 2, nullptr);
    }
}

// VERIFY: 19e0f389 auto
// OFFSET: 0x0096B060
void CSimpleScrollFrame::UnparentFrame(CSimpleFrame* child) {
    if (child == this->m_scrollChild) {
        this->m_scrollChild = nullptr;
    }

    CSimpleFrame::UnparentFrame(child);
}

// VERIFY: 43378d4d auto
// OFFSET: 0x0096B090
const char* CSimpleScrollFrame::GetObjectTypeName() {
    return CSimpleScrollFrame::s_objectTypeName;
}

// VERIFY: 8fad3c2b auto
// OFFSET: 0x0096B0A0
int32_t CSimpleScrollFrame::GetScriptMetaTable() {
    return CSimpleScrollFrame::s_metatable;
}

// VERIFY: a42c754e auto
// OFFSET: 0x0096B0B0
FrameScript_Object::ScriptIx* CSimpleScrollFrame::GetScriptByName(const char* name, FrameScript_Object::ScriptData& data) {
    FrameScript_Object::ScriptIx* script = CSimpleFrame::GetScriptByName(name, data);

    if (script) {
        return script;
    }

    if (!SStrCmpI(name, "OnHorizontalScroll", STORM_MAX_STR)) {
        data.m_wrapper = "return function(self,offset) %s end";
        return &this->m_onHorizontalScroll;
    }

    if (!SStrCmpI(name, "OnVerticalScroll", STORM_MAX_STR)) {
        data.m_wrapper = "return function(self,offset) %s end";
        return &this->m_onVerticalScroll;
    }

    if (!SStrCmpI(name, "OnScrollRangeChanged", STORM_MAX_STR)) {
        data.m_wrapper = "return function(self,xrange,yrange) %s end";
        return &this->m_onScrollRangeChanged;
    }

    return nullptr;
}

// VERIFY: 2ff8bc0f auto
// OFFSET: 0x0096B150
void CSimpleScrollFrame::GetScriptTime(lua_State* L, double* time, uint32_t* count, int32_t recurse) {
    FrameScript_Object::GetScriptTime(L, this->m_onHorizontalScroll, time, count);
    FrameScript_Object::GetScriptTime(L, this->m_onVerticalScroll, time, count);
    FrameScript_Object::GetScriptTime(L, this->m_onScrollRangeChanged, time, count);
    CSimpleFrame::GetScriptTime(L, time, count, recurse);
}

// VERIFY: 719b6b1e manual
// OFFSET: 0x0096B1C0
void CSimpleScrollFrame::UpdateScrollChild() {
    if (this->m_scrollChild) {
        this->m_scrollChild->MarkFramePointsUnused();
        this->m_scrollChild->SetPoint(FRAMEPOINT_TOPLEFT, this, FRAMEPOINT_TOPLEFT, -this->m_scrollOffset.x, this->m_scrollOffset.y, 1);
    }
}

// VERIFY: 33ffead9 auto
// OFFSET: 0x0096B210
void CSimpleScrollFrame::OnFrameSizeChanged(float width, float height) {
    CSimpleFrame::OnFrameSizeChanged(width, height);
    this->m_scrollChildUpdate = 1;
}

// VERIFY: aa726a49 auto
// OFFSET: [0x0096B240, 0x0096B340]
CSimpleScrollFrame::~CSimpleScrollFrame() {
}

// VERIFY: 9fdd996a auto
// OFFSET: 0x0096B250
bool CSimpleScrollFrame::IsA(int32_t type) {
    return type == CSimpleScrollFrame::s_objectType || type == CSimpleFrame::s_objectType || type == CScriptRegion::s_objectType || type == CScriptObject::s_objectType;
}

// VERIFY: 8a8d8651 auto
// OFFSET: 0x0096B290
bool CSimpleScrollFrame::IsA(const char* typeName) {
    return !SStrCmpI(typeName, CSimpleScrollFrame::s_objectTypeName, STORM_MAX_STR) || CSimpleFrame::IsA(typeName);
}

// VERIFY: 1b76a67e auto
// OFFSET: 0x0096B2D0
CSimpleScrollFrame::CSimpleScrollFrame(CSimpleFrame* parent) : CSimpleFrame(parent) {
    this->m_scrollChildUpdate = 0;
    this->m_scrollChild = nullptr;
    this->m_scrollRange.x = 0.0f;
    this->m_scrollRange.y = 0.0f;
    this->m_scrollOffset.x = 0.0f;
    this->m_scrollOffset.y = 0.0f;
}

// VERIFY: 5e5e99a4 auto
// OFFSET: 0x0096B3A0
void CSimpleScrollFrame::SetScrollChild(CSimpleFrame* child) {
    if (this->m_scrollChild) {
        this->m_scrollChild->SetBeingScrolled(0, 0);
        this->m_scrollChild->SetParent(nullptr);
    }

    this->m_scrollChild = child;

    if (child) {
        child->SetParent(this);
        this->m_scrollChild->SetBeingScrolled(1, 1);
    }

    this->UpdateScrollChild();
    this->m_scrollChildUpdate = 1;
}

// VERIFY: 502c3b17 auto
// OFFSET: 0x0096B440
void CSimpleScrollFrame::SetHorizontalScroll(float offset) {
    if (fabsf(offset - this->m_scrollOffset.x) >= 0.00000095367432f) {
        this->m_scrollOffset.x = offset;
        this->UpdateScrollChild();
        this->RunOnHorizontalScrollScript();
    }
}

// VERIFY: 698f3f61 auto
// OFFSET: 0x0096B480
void CSimpleScrollFrame::SetVerticalScroll(float offset) {
    if (fabsf(offset - this->m_scrollOffset.y) >= 0.00000095367432f) {
        this->m_scrollOffset.y = offset;
        this->UpdateScrollChild();
        this->RunOnVerticalScrollScript();
    }
}

// VERIFY: 3bba74b1 auto
// OFFSET: 0x0096B4C0
void CSimpleScrollFrame::UpdateScrollChildRect(float width, float height) {
    if (!this->m_scrollChild) {
        return;
    }

    C2Vector range = this->m_scrollRange;
    CRect rect = { FLT_MAX, FLT_MAX, 0.0f, 0.0f };
    this->m_scrollChild->GetBoundsRect(rect);

    float rangeX = rect.maxX - rect.minX - width;

    if (!(0.0f < rangeX)) {
        rangeX = 0.0f;
    }

    float scale = 1.0f / this->m_layoutScale;
    this->m_scrollRange.x = rangeX * scale;

    float rangeY = rect.maxY - rect.minY - height;
    this->m_scrollRange.y = (rangeY > 0.0f ? rangeY : 0.0f) * scale;

    if (!(fabsf(this->m_scrollRange.x - range.x) < 0.00000095367432f) || !(fabsf(this->m_scrollRange.y - range.y) < 0.00000095367432f)) {
        this->RunOnScrollRangeChangedScript();
    }
}

// VERIFY: a565c63e auto
// OFFSET: 0x0096B5A0
void CSimpleScrollFrame::OnLayerUpdate(float elapsedSec) {
    CSimpleFrame::OnLayerUpdate(elapsedSec);

    if (this->m_scrollChildUpdate) {
        CRect rect = { 0.0f, 0.0f, 0.0f, 0.0f };

        if (this->GetRect(&rect)) {
            this->UpdateScrollChildRect(rect.maxX - rect.minX, rect.maxY - rect.minY);
        }

        this->m_scrollChildUpdate = 0;
    }
}

// VERIFY: 75ecd328 manual
// OFFSET: 0x0096B610
void CSimpleScrollFrame::RenderScrollChild(void* param) {
    CSimpleScrollFrame* scrollFrame = static_cast<CSimpleScrollFrame*>(param);
    CRect rect = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (!scrollFrame->GetHitRect(rect) || !scrollFrame->m_scrollChild || !scrollFrame->m_scrollChild->m_isVisible) {
        return;
    }

    C44Matrix projection;
    C44Matrix view;
    CGxDevice* device = g_theGxDevicePtr;
    projection = device->m_projection;
    view = device->m_xforms[GxXform_View].m_mtx[device->m_xforms[GxXform_View].m_level];

    float minX;
    float maxX;
    float minY;
    float maxY;
    float minZ;
    float maxZ;
    GxXformViewport(minX, maxX, minY, maxY, minZ, maxZ);

    float left = 0.0f;
    float bottom = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    NDCToDDC(minX, minY, &left, &bottom);
    NDCToDDC(maxX, maxY, &right, &top);

    if (left > rect.minX) {
        rect.minX = left;
    }

    if (right < rect.maxX) {
        rect.maxX = right;
    }

    if (bottom > rect.minY) {
        rect.minY = bottom;
    }

    if (top < rect.maxY) {
        rect.maxY = top;
    }

    if (rect.minX == rect.maxX || rect.minY == rect.maxY) {
        return;
    }

    C2Vector offset;
    offset.x = 0.0f;
    offset.y = 0.0f;
    CameraSetupScreenProjection(rect, offset, 0.0f, 0);

    DDCToNDC(rect.minX, rect.minY, &rect.minX, &rect.minY);
    DDCToNDC(rect.maxX, rect.maxY, &rect.maxX, &rect.maxY);

    if (rect.minX < 0.0f) {
        rect.maxX = rect.maxX - rect.minX;
        rect.minX = 0.0f;
    }

    if (rect.maxX > 1.0f) {
        rect.minX = rect.minX - (rect.maxX - 1.0f);
        rect.maxX = 1.0f;
    }

    if (rect.minY < 0.0f) {
        rect.maxY = rect.maxY - rect.minY;
        rect.minY = 0.0f;
    }

    if (rect.maxY > 1.0f) {
        rect.minY = rect.minY - (rect.maxY - 1.0f);
        rect.maxY = 1.0f;
    }

    if (rect.minX < rect.maxX && rect.minY < rect.maxY) {
        GxXformSetViewport(rect.minX, rect.maxX, rect.minY, rect.maxY, 0.0f, 1.0f);
        scrollFrame->m_scrollChild->OnFrameRender();
    }

    GxXformSetProjection(projection);
    GxXformSetView(view);
    GxXformSetViewport(minX, maxX, minY, maxY, minZ, maxZ);
}

// VERIFY: be2d653f manual
// OFFSET: 0x0096B920
void CSimpleScrollFrame::LoadXML(XMLNode* node, CStatus* status) {
    CSimpleFrame::LoadXML(node, status);

    XMLNode* scrollChild = node->GetChildByName("ScrollChild");

    if (!scrollChild) {
        return;
    }

    if (scrollChild->m_child) {
        CSimpleFrame* child = FrameXML_CreateFrame(scrollChild->m_child, this, status);

        if (child) {
            this->SetScrollChild(child);
        }
    } else {
        const char* name = this->GetName();
        status->Add(STATUS_WARNING, "Frame %s: Scroll frame created without scroll child", name ? name : "<unnamed>");
    }
}

// VERIFY: b7b001f2 auto
// OFFSET: 0x0096B9A0
void CSimpleScrollFrame::OnFrameRender(CRenderBatch* batch, uint32_t layer) {
    CSimpleFrame::OnFrameRender(batch, layer);

    if (layer == DRAWLAYER_HIGHLIGHT) {
        batch->QueueCallback(&CSimpleScrollFrame::RenderScrollChild, this);
    }
}

// VERIFY: 35a569df manual
// OFFSET: 0x0096B9D0
bool CSimpleScrollFrame::GetBoundsRect(CRect& rect) {
    CRect frameRect = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (this->IsResizePending()) {
        this->Resize(1);
    }

    if (this->GetRect(&frameRect)) {
        rect.minX = frameRect.minX >= rect.minX ? rect.minX : frameRect.minX;
        rect.maxX = frameRect.maxX <= rect.maxX ? rect.maxX : frameRect.maxX;
        rect.minY = frameRect.minY >= rect.minY ? rect.minY : frameRect.minY;
        rect.maxY = frameRect.maxY <= rect.maxY ? rect.maxY : frameRect.maxY;
    }

    for (CSimpleRegion* region = this->m_regions.Head(); region; region = this->m_regions.Next(region)) {
        if (!region->m_shown) {
            continue;
        }

        if (region->IsResizePending()) {
            region->Resize(1);
        }

        if (region->GetRect(&frameRect)) {
            rect.minX = frameRect.minX >= rect.minX ? rect.minX : frameRect.minX;
            rect.maxX = frameRect.maxX <= rect.maxX ? rect.maxX : frameRect.maxX;
            rect.minY = frameRect.minY >= rect.minY ? rect.minY : frameRect.minY;
            rect.maxY = frameRect.maxY <= rect.maxY ? rect.maxY : frameRect.maxY;
        }
    }

    for (SIMPLEFRAMENODE* node = this->m_children.Head(); node; node = this->m_children.Next(node)) {
        if (node->m_frame != this->m_scrollChild && node->m_frame->m_isShown) {
            node->m_frame->GetBoundsRect(rect);
        }
    }

    return rect.maxY > rect.minY && rect.maxX > rect.minX;
}
