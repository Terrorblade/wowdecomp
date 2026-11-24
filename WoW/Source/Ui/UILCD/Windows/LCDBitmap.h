#ifndef WOW_SOURCE_UI_UILCD_WINDOWS_LCDBITMAP_H
#define WOW_SOURCE_UI_UILCD_WINDOWS_LCDBITMAP_H

#include <cstdint>
#include <windows.h>
#include "WoW/Source/Ui/UILCD/Windows/LCDBase.h"
#include "Engine/Source/Tempest/C2iVector.h"

class CLCDBitmap : public CLCDBase {
    public:
    /* 0040 */ HBITMAP m_hBitmap;
    /* 0044 */ DWORD m_dwROP;
    /* 0048 */ float m_fZoom;
    /* 004C */ int32_t m_bAlpha;
    /* 0050 */ int32_t m_nBitmapWidth;
    /* 0054 */ int32_t m_nBitmapHeight;

    CLCDBitmap();
    virtual ~CLCDBitmap();
    virtual void OnDraw(CLCDGfxBase& gfx);

    void SetBitmap(HBITMAP bitmap);
    void SetAlpha(int32_t alpha);
    void SetBitmapSize(int32_t width, int32_t height);
};

#endif
