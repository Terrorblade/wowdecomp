#include "WoW/Source/Ui/UILCD/Windows/LCDCollection.h"
#include "WoW/Source/Ui/UILCD/Windows/LCDGfx.h"

// VERIFY: 33719c71 manual
// OFFSET: 0x0095B820
CLCDCollection::CLCDCollection() {
}

// VERIFY: 246f193d manual
// OFFSET: [0x0095B950, 0x0095BC40]
CLCDCollection::~CLCDCollection() {
    for (CLCDBASE_NODE* node = this->m_Objects.Head(); node; node = this->m_Objects.Next(node)) {
        delete node->m_pObject;
        node->m_pObject = nullptr;
    }

    this->m_Objects.Clear();
}

// VERIFY: 65a7d5fb manual
// OFFSET: 0x0095B850
bool CLCDCollection::AddObject(CLCDBase* object) {
    CLCDBASE_NODE* node = this->m_Objects.NewNode(TSLIST_LINK_BEFORE, 0, 0);
    node->m_pObject = object;
    return true;
}

// VERIFY: 3680e3d0 manual
// OFFSET: 0x0095B9F0
void CLCDCollection::OnDraw(CLCDGfxBase& gfx) {
    if (!this->IsVisible()) {
        return;
    }

    for (CLCDBASE_NODE* node = this->m_Objects.Head(); node; node = this->m_Objects.Next(node)) {
        CLCDBase* object = node->m_pObject;

        if (!object->IsVisible()) {
            continue;
        }

        object->OnPrepareDraw(gfx);
        HRGN region = CreateRectRgn(object->GetOrigin().x, object->GetOrigin().y, object->GetOrigin().x + object->GetWidth(), object->GetOrigin().y + object->GetHeight());
        SelectClipRgn(gfx.GetHDC(), region);
        DeleteObject(region);

        POINT previousOrigin = { 0, 0 };
        SetViewportOrgEx(gfx.GetHDC(), object->GetOrigin().x, object->GetOrigin().y, &previousOrigin);
        OffsetViewportOrgEx(gfx.GetHDC(), object->GetLogicalOrigin().x, object->GetLogicalOrigin().y, nullptr);
        object->OnDraw(gfx);
        SelectClipRgn(gfx.GetHDC(), nullptr);
        SetViewportOrgEx(gfx.GetHDC(), previousOrigin.x, previousOrigin.y, nullptr);
        OffsetViewportOrgEx(gfx.GetHDC(), 0, 0, nullptr);
    }
}

// VERIFY: c17c2a81 manual
// OFFSET: 0x0095BB90
void CLCDCollection::OnUpdate(uint32_t timestamp) {
    for (CLCDBASE_NODE* node = this->m_Objects.Head(); node; node = this->m_Objects.Next(node)) {
        node->m_pObject->OnUpdate(timestamp);
    }

    CLCDBase::OnUpdate(timestamp);
}

// VERIFY: 08a60d21 manual
// OFFSET: 0x0095BBE0
void CLCDCollection::ShowSubscreen(int32_t subscreen) {
    for (CLCDBASE_NODE* node = this->m_Objects.Head(); node; node = this->m_Objects.Next(node)) {
        CLCDBase* object = node->m_pObject;

        if (object->GetUserData() != -1) {
            object->Show(object->GetUserData() == subscreen);
        }
    }
}
