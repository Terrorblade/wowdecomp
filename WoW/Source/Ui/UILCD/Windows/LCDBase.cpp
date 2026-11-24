#include "WoW/Source/Ui/UILCD/Windows/LCDBase.h"

// VERIFY: e5a4ccc5 auto
// OFFSET: 0x0095BC70
CLCDBase::CLCDBase() {
    this->m_Size.cx = 0;
    this->m_Size.cy = 0;
    this->m_Origin.x = 0;
    this->m_Origin.y = 0;
    this->m_bVisible = 1;
    this->m_bInverted = 0;
    this->m_ptLogical.x = 0;
    this->m_ptLogical.y = 0;
    this->m_sizeLogical.cx = 0;
    this->m_sizeLogical.cy = 0;
    this->m_nBkMode = TRANSPARENT;
    this->m_nObjectType = LCD_OBJECT_TYPE_DEFAULT;
    this->m_crBackgroundColor = RGB(0, 0, 0);
    this->m_crForegroundColor = RGB(255, 255, 255);
}

// VERIFY: de2ec0bb auto
// OFFSET: [0x0095BCC0, 0x0095BE70]
CLCDBase::~CLCDBase() {
}

// VERIFY: 21134cdc auto
// OFFSET: 0x00959CD0
int32_t CLCDBase::GetWidth() {
    return this->GetSize().cx;
}

// VERIFY: ce3beab0 auto
// OFFSET: 0x00959CE0
int32_t CLCDBase::GetHeight() {
    return this->GetSize().cy;
}

// VERIFY: 638d71cb auto
// OFFSET: 0x0095BD70
SIZE& CLCDBase::GetSize() {
    return this->m_Size;
}

// VERIFY: c4c3b568 auto
// OFFSET: 0x0095BE30
void CLCDBase::SetBackgroundMode(int32_t mode) {
    this->m_nBkMode = mode;
}

// VERIFY: 31752403 auto
// OFFSET: 0x009C1FD0
POINT& CLCDBase::GetOrigin() {
    return this->m_Origin;
}

// VERIFY: 3a4f2369 auto
// OFFSET: 0x0095BCD0
void CLCDBase::SetOriginXY(int32_t x, int32_t y) {
    this->m_Origin.x = x;
    this->m_Origin.y = y;
}

// VERIFY: 4424a140 auto
// OFFSET: 0x0095BCF0
void CLCDBase::SetOrigin(POINT pt) {
    this->SetOriginXY(pt.x, pt.y);
}

// VERIFY: f615e99a auto
// OFFSET: 0x0095BD10
void CLCDBase::SetSize(SIZE& size) {
    this->m_Size = size;
    this->SetLogicalSize(this->m_Size);
}

// VERIFY: 22bef7d7 auto
// OFFSET: 0x0095BD40
void CLCDBase::SetSizeWH(int32_t width, int32_t height) {
    SIZE size;
    size.cx = width;
    size.cy = height;
    this->SetSize(size);
}

// VERIFY: 943e9380 auto
// OFFSET: 0x008E5250
int32_t CLCDBase::Initialize() {
    return 0;
}

// VERIFY: 0838d01f auto
// OFFSET: 0x005EEB70
void CLCDBase::Shutdown() {
}

// VERIFY: dbc03d2b auto
// OFFSET: 0x0095BE10
void CLCDBase::Show(int32_t show) {
    this->m_bVisible = show;
}

// VERIFY: 894332e8 auto
// OFFSET: 0x00431E70
int32_t CLCDBase::IsVisible() {
    return this->m_bVisible;
}

// VERIFY: 6e0ea336 auto
// OFFSET: 0x0095BE20
void CLCDBase::Invert(int32_t enable) {
    this->m_bInverted = enable;
}

// VERIFY: e32c4efe auto
// OFFSET: 0x005EEB70
void CLCDBase::ResetUpdate() {
}

// VERIFY: 84e40c65 auto
// OFFSET: 0x0095BDA0
void CLCDBase::SetLogicalOriginXY(int32_t x, int32_t y) {
    this->m_ptLogical.x = x;
    this->m_ptLogical.y = y;
}

// VERIFY: eea59399 auto
// OFFSET: 0x0095BD80
void CLCDBase::SetLogicalOrigin(POINT& pt) {
    this->m_ptLogical = pt;
}

// VERIFY: 8333b697 auto
// OFFSET: 0x0095BDC0
POINT& CLCDBase::GetLogicalOrigin() {
    return this->m_ptLogical;
}

// VERIFY: f1f8b83e auto
// OFFSET: 0x0095BDF0
void CLCDBase::SetLogicalSizeWH(int32_t width, int32_t height) {
    this->m_sizeLogical.cx = width;
    this->m_sizeLogical.cy = height;
}

// VERIFY: 1f675f73 auto
// OFFSET: 0x0095BDD0
void CLCDBase::SetLogicalSize(SIZE& size) {
    this->m_sizeLogical = size;
}

// VERIFY: 19ec8bda auto
// OFFSET: 0x009C2590
SIZE& CLCDBase::GetLogicalSize() {
    return this->m_sizeLogical;
}

// VERIFY: a108eea6 auto
// OFFSET: 0x0095A490
int32_t CLCDBase::GetBackgroundMode() {
    return this->m_nBkMode;
}

// VERIFY: 876e6a13 auto
// OFFSET: 0x00499FB0
int32_t CLCDBase::GetObjectType() {
    return this->m_nObjectType;
}

// VERIFY: 5757b5c6 auto
// OFFSET: 0x0095BE40
void CLCDBase::SetObjectType(int32_t type) {
    this->m_nObjectType = type;
}

// VERIFY: 5f641614 auto
// OFFSET: 0x0095BE50
void CLCDBase::SetForegroundColor(COLORREF color) {
    this->m_crForegroundColor = color;
}

// VERIFY: 49035bd0 auto
// OFFSET: 0x0095BE60
void CLCDBase::SetBackgroundColor(COLORREF color) {
    this->m_crBackgroundColor = color;
}

// VERIFY: 71834d28 auto
// OFFSET: 0x00632050
void CLCDBase::OnPrepareDraw(CLCDGfxBase& gfx) {
}

// VERIFY: f6a9d580 auto
// OFFSET: 0x00632050
void CLCDBase::OnDraw(CLCDGfxBase& gfx) {
}

// VERIFY: 6fe5f9a0 auto
// OFFSET: 0x00632050
void CLCDBase::OnUpdate(uint32_t timestamp) {
}

// VERIFY: 1995ae9f auto
// OFFSET: 0x00959CF0
void CLCDBase::SetUserData(int32_t data) {
    this->m_nUserData = data;
}

// VERIFY: 110f8533 auto
// OFFSET: 0x00959D00
int32_t CLCDBase::GetUserData() {
    return this->m_nUserData;
}
