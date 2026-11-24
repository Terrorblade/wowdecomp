#include "WoW/Source/Ui/UILCD/Windows/LCDText.h"
#include <cstring>
#include <cwchar>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/Client.h"
#include "WoW/Source/Ui/UILCD/Windows/LCDGfx.h"

typedef HANDLE (WINAPI* ADDFONTMEMRESOURCEEXPROC)(PVOID fileView, DWORD size, PVOID reserved, DWORD* numFonts);

// VERIFY: de590517 auto
// OFFSET: 0x0095C060
CLCDText::CLCDText() {
    this->m_hFont = nullptr;
    this->m_nTextLength = 0;
    this->m_nTextFormat = DT_NOPREFIX;
    this->m_bRecalcExtent = 1;
    this->m_nTextAlignment = 0;
    this->m_dtp.iTabLength = 0;
    this->m_dtp.iLeftMargin = 0;
    this->m_dtp.iRightMargin = 0;
    this->m_dtp.uiLengthDrawn = 0;
    this->m_dtp.cbSize = sizeof(DRAWTEXTPARAMS);
    this->m_sizeVExtent.cx = 0;
    this->m_sizeVExtent.cy = 0;
    this->m_sizeHExtent.cx = 0;
    this->m_sizeHExtent.cy = 0;
    CLCDBase::SetBackgroundMode(TRANSPARENT);
    memset(this->m_szText, 0, sizeof(this->m_szText));
}

// VERIFY: dd97bf61 manual
// OFFSET: 0x0095C9B0
CLCDText::~CLCDText() {
    if (this->m_hFont) {
        DeleteObject(this->m_hFont);
        this->m_hFont = nullptr;
    }
}

// VERIFY: 1b343da2 auto
// OFFSET: 0x0095C100
int32_t CLCDText::Initialize() {
    if (this->m_hFont) {
        return S_OK;
    }

    this->SetForegroundColor(RGB(255, 255, 255));
    return this->m_hFont ? S_OK : E_OUTOFMEMORY;
}

// VERIFY: bec380cc auto
// OFFSET: 0x0095C140
void CLCDText::SetFont(LOGFONTA& font) {
    if (this->m_hFont) {
        DeleteObject(this->m_hFont);
        this->m_hFont = nullptr;
    }

    this->m_hFont = CreateFontIndirectA(&font);
    this->m_bRecalcExtent = 1;
}

// VERIFY: e431928d auto
// OFFSET: 0x0095C180
HFONT CLCDText::GetFont() {
    return this->m_hFont;
}

// VERIFY: 564584af auto
// OFFSET: 0x0095C190
void CLCDText::SetFontFaceName(const char* faceName) {
    if (!faceName) {
        return;
    }

    SStrPrintf(this->m_szFontFaceName, sizeof(this->m_szFontFaceName), faceName);
    LOGFONTA font;
    memset(&font, 0, sizeof(font));
    GetObjectA(this->m_hFont, sizeof(font), &font);
    SStrCopy(font.lfFaceName, faceName, sizeof(font.lfFaceName));
    font.lfCharSet = DEFAULT_CHARSET;
    this->SetFont(font);

    if (this->m_hFont) {
        return;
    }

    char fontFile[64];

    switch (s_locale) {
        case LOCALE_ko_KR:
            SStrCopy(fontFile, "Fonts\\2002.ttf", STORM_MAX_STR);
            SStrCopy(font.lfFaceName, "2002L", sizeof(font.lfFaceName));
            break;
        case LOCALE_zh_CN:
            SStrCopy(fontFile, "Fonts\\ZYHei.ttf", STORM_MAX_STR);
            SStrCopy(font.lfFaceName, "ZYHei_FRIZQT", sizeof(font.lfFaceName));
            break;
        case LOCALE_zh_TW:
            SStrCopy(fontFile, "Fonts\\bHEI00M.ttf", STORM_MAX_STR);
            SStrCopy(font.lfFaceName, "AR Heiti2 Medium B5", sizeof(font.lfFaceName));
            break;
        case LOCALE_ru_RU:
            SStrCopy(fontFile, "Fonts\\NIM_____.ttf", STORM_MAX_STR);
            SStrCopy(font.lfFaceName, "Nimrod MT", sizeof(font.lfFaceName));
            break;
        default:
            SStrCopy(fontFile, "Fonts\\ARIALN.TTF", STORM_MAX_STR);
            SStrCopy(font.lfFaceName, "Arial Narrow", sizeof(font.lfFaceName));
            break;
    }

    void* data;
    uint32_t size;

    if (SFile::LoadFile(fontFile, &data, &size, 0, nullptr)) {
        DWORD numFonts = 0;
        ADDFONTMEMRESOURCEEXPROC addFontMemResourceEx = reinterpret_cast<ADDFONTMEMRESOURCEEXPROC>(GetProcAddress(LoadLibraryA("gdi32.dll"), "AddFontMemResourceEx"));

        if (addFontMemResourceEx && addFontMemResourceEx(data, size, nullptr, &numFonts)) {
            this->SetFont(font);
        }

        SFile::Unload(data);
    }

    if (!this->m_hFont) {
        SStrCopy(font.lfFaceName, "", sizeof(font.lfFaceName));
        font.lfCharSet = DEFAULT_CHARSET;
        this->SetFont(font);
    }
}

// VERIFY: 678cab1d auto
// OFFSET: 0x0095C390
void CLCDText::SetFontPointSize(int32_t pointSize) {
    LOGFONTA font;
    memset(&font, 0, sizeof(font));
    GetObjectA(this->m_hFont, sizeof(font), &font);
    font.lfHeight = -MulDiv(pointSize, 96, 72);
    this->SetFont(font);
}

// VERIFY: 6565a42e auto
// OFFSET: 0x0095C3F0
void CLCDText::SetFontWeight(int32_t weight) {
    LOGFONTA font;
    memset(&font, 0, sizeof(font));
    GetObjectA(this->m_hFont, sizeof(font), &font);
    font.lfWeight = weight;
    this->SetFont(font);
}

// VERIFY: d5ce07f2 manual
// OFFSET: 0x0095C440
void CLCDText::SetFontColor(COLORREF color) {
    this->SetForegroundColor(color);
}

// VERIFY: 4106d234 manual
// OFFSET: 0x0095C450
void CLCDText::SetText(const WCHAR* text) {
    if (!text) {
        return;
    }

    size_t length = wcsnlen(text, sizeof(this->m_szText) / sizeof(this->m_szText[0]) - 1);

    if (!wcsncmp(this->m_szText, text, length) && !this->m_szText[length]) {
        return;
    }

    memcpy(this->m_szText, text, length * sizeof(WCHAR));
    this->m_szText[length] = 0;
    this->m_nTextLength = static_cast<int32_t>(length);
    this->m_dtp.iLeftMargin = 0;
    this->m_dtp.iRightMargin = 0;
    this->m_bRecalcExtent = 1;
}

// VERIFY: b7d6e216 auto
// OFFSET: 0x00714230
const WCHAR* CLCDText::GetText() {
    return this->m_szText;
}

// VERIFY: 8d8bb657 auto
// OFFSET: 0x0095C4E0
void CLCDText::SetWordWrap(int32_t enable) {
    this->m_bRecalcExtent = 1;

    if (enable) {
        this->m_nTextFormat |= DT_WORDBREAK;
    } else {
        this->m_nTextFormat &= ~DT_WORDBREAK;
    }
}

// VERIFY: f8e1a49f auto
// OFFSET: 0x0095C550
SIZE& CLCDText::GetVExtent() {
    return this->m_sizeVExtent;
}

// VERIFY: ba7e7174 auto
// OFFSET: 0x0095C560
SIZE& CLCDText::GetHExtent() {
    return this->m_sizeHExtent;
}

// VERIFY: bd4069df manual
// OFFSET: 0x0095C570
void CLCDText::CalculateExtent(int32_t singleLine) {
    HDC dc = CreateCompatibleDC(nullptr);
    int oldMapMode = SetMapMode(dc, MM_TEXT);
    int oldBkMode = SetBkMode(dc, this->GetBackgroundMode());
    HGDIOBJ oldFont = SelectObject(dc, this->m_hFont);
    RECT rect = { 0, 0, this->m_Size.cx, this->m_Size.cy };

    if (singleLine) {
        DrawTextExA(dc, reinterpret_cast<LPSTR>(this->m_szText), this->m_nTextLength, &rect, this->m_nTextFormat | DT_CALCRECT | DT_SINGLELINE, &this->m_dtp);
        this->m_sizeHExtent.cx = rect.right;
        this->m_sizeHExtent.cy = rect.bottom;
    } else {
        DrawTextExA(dc, reinterpret_cast<LPSTR>(this->m_szText), this->m_nTextLength, &rect, this->m_nTextFormat | DT_CALCRECT | DT_WORDBREAK, &this->m_dtp);
        this->m_sizeVExtent.cx = rect.right;
        this->m_sizeVExtent.cy = rect.bottom;
    }

    SetMapMode(dc, oldMapMode);
    SetBkMode(dc, oldBkMode);
    SelectObject(dc, oldFont);
    DeleteDC(dc);
}

// VERIFY: 63dbda1f auto
// OFFSET: 0x0095C510
void CLCDText::SetLeftMargin(int32_t margin) {
    this->m_dtp.iLeftMargin = margin;
}

// VERIFY: 04708bff auto
// OFFSET: 0x0095C530
int32_t CLCDText::GetLeftMargin() {
    return this->m_dtp.iLeftMargin;
}

// VERIFY: 541ccf6a auto
// OFFSET: 0x0095C520
void CLCDText::SetRightMargin(int32_t margin) {
    this->m_dtp.iRightMargin = margin;
}

// VERIFY: 0c36b239 auto
// OFFSET: 0x0095C540
int32_t CLCDText::GetRightMargin() {
    return this->m_dtp.iRightMargin;
}

// VERIFY: 0616fe02 auto
// OFFSET: 0x0095C670
void CLCDText::SetAlignment(int32_t alignment) {
    UINT format = this->m_nTextFormat & ~this->m_nTextAlignment;
    this->m_nTextAlignment = alignment;
    this->m_nTextFormat = alignment | format;
}

// VERIFY: 5ff97ac8 auto
// OFFSET: 0x0095C6A0
void CLCDText::DrawControlText(CLCDGfxBase& gfx) {
    RECT rect;
    rect.left = 0;
    rect.top = 0;
    rect.right = this->GetLogicalSize().cx;
    rect.bottom = this->GetLogicalSize().cy;
    DrawTextExW(gfx.GetHDC(), this->m_szText, this->m_nTextLength, &rect, this->m_nTextFormat, &this->m_dtp);

    if (this->m_bInverted) {
        InvertRect(gfx.GetHDC(), &rect);
    }
}

// VERIFY: dfcf685b manual
// OFFSET: 0x0095C730
void CLCDText::OnDraw(CLCDGfxBase& gfx) {
    if (this->GetBackgroundMode() == OPAQUE) {
        HBRUSH brush = CreateSolidBrush(this->m_crBackgroundColor);
        RECT full = { 0, 0, this->m_Size.cx, this->m_Size.cy };
        FillRect(gfx.GetHDC(), &full, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        RECT logical = { 0, 0, this->m_sizeLogical.cx, this->m_sizeLogical.cy };
        FillRect(gfx.GetHDC(), &logical, brush);
        DeleteObject(brush);
    }

    if (!this->m_nTextLength) {
        return;
    }

    int oldMapMode = SetMapMode(gfx.GetHDC(), MM_TEXT);
    int oldBkMode = SetBkMode(gfx.GetHDC(), this->GetBackgroundMode());
    COLORREF oldBkColor = SetBkColor(gfx.GetHDC(), this->m_crBackgroundColor);
    HGDIOBJ oldFont = SelectObject(gfx.GetHDC(), this->m_hFont);
    COLORREF oldTextColor = SetTextColor(gfx.GetHDC(), this->m_crForegroundColor);

    if (this->m_bRecalcExtent) {
        UINT format = this->m_nTextFormat;
        RECT rect = { 0, 0, this->GetWidth(), this->GetHeight() };
        DrawTextExW(gfx.GetHDC(), this->m_szText, this->m_nTextLength, &rect, format | DT_CALCRECT | DT_WORDBREAK, &this->m_dtp);
        this->m_sizeVExtent.cx = rect.right;
        this->m_sizeVExtent.cy = rect.bottom;

        format = this->m_nTextFormat;
        rect.left = 0;
        rect.top = 0;
        rect.right = this->GetWidth();
        rect.bottom = this->GetHeight();
        DrawTextExW(gfx.GetHDC(), this->m_szText, this->m_nTextLength, &rect, format | DT_CALCRECT, &this->m_dtp);
        this->m_sizeHExtent.cx = rect.right;
        this->m_sizeHExtent.cy = rect.bottom;
        this->m_bRecalcExtent = 0;
    }

    if (this->IsVisible()) {
        this->DrawControlText(gfx);
    }

    SetMapMode(gfx.GetHDC(), oldMapMode);
    SetTextColor(gfx.GetHDC(), oldTextColor);
    SetBkMode(gfx.GetHDC(), oldBkMode);
    SetBkColor(gfx.GetHDC(), oldBkColor);
    SelectObject(gfx.GetHDC(), oldFont);
}
