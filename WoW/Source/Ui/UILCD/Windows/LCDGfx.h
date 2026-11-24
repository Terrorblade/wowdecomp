#ifndef WOW_SOURCE_UI_UILCD_WINDOWS_LCDGFX_H
#define WOW_SOURCE_UI_UILCD_WINDOWS_LCDGFX_H

#include <cstdint>
#include <windows.h>
#include "ThirdParty/lgLcd/lglcd.h"
#include "Engine/Source/Tempest/C2iVector.h"

class CLCDGfxBase {
    public:
    /* 0004 */ lgLcdBitmapHeader* m_pLCDScreen;
    /* 0008 */ int32_t m_nWidth;
    /* 000C */ int32_t m_nHeight;
    /* 0010 */ BITMAPINFO* m_pBitmapInfo;
    /* 0014 */ HDC m_hDC;
    /* 0018 */ HBITMAP m_hBitmap;
    /* 001C */ HBITMAP m_hPrevBitmap;
    /* 0020 */ uint8_t* m_pBitmapBits;

    CLCDGfxBase();
    virtual ~CLCDGfxBase();
    virtual HRESULT Initialize();
    virtual void Shutdown();
    virtual void ClearScreen();
    virtual void BeginDraw();
    virtual void EndDraw();
    virtual HDC GetHDC();
    virtual lgLcdBitmapHeader* GetLCDScreen();
    virtual BITMAPINFO* GetBitmapInfo();
    virtual HBITMAP GetHBITMAP();
    virtual uint32_t GetFamily() = 0;
    virtual int32_t GetWidth();
    virtual int32_t GetHeight();
    HRESULT CreateBitmap(uint16_t bitCount);
};

class CLCDGfxMono : public CLCDGfxBase {
    public:
    /* 0024 */ lgLcdBitmap160x43x1 m_lcdBitmap;

    CLCDGfxMono();
    virtual ~CLCDGfxMono();
    virtual HRESULT Initialize();
    virtual void ClearScreen();
    virtual lgLcdBitmapHeader* GetLCDScreen();
    virtual uint32_t GetFamily();
};

class CLCDGfxColor : public CLCDGfxBase {
    public:
    /* 0024 */ lgLcdBitmapQVGAx32 m_lcdBitmap;

    CLCDGfxColor();
    virtual ~CLCDGfxColor();
    virtual HRESULT Initialize();
    virtual lgLcdBitmapHeader* GetLCDScreen();
    virtual uint32_t GetFamily();
};

#endif
