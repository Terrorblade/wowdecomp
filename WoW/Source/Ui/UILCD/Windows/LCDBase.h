#ifndef WOW_SOURCE_UI_UILCD_WINDOWS_LCDBASE_H
#define WOW_SOURCE_UI_UILCD_WINDOWS_LCDBASE_H

#include <cstdint>
#include <windows.h>

class CLCDGfxBase;

enum LCD_OBJECT_TYPE : int32_t {
    LCD_OBJECT_TYPE_UNKNOWN_0 = 0,
    LCD_OBJECT_TYPE_TEXT = 1,
    LCD_OBJECT_TYPE_DEFAULT = 4
};

class CLCDBase {
    public:
    /* 0004 */ SIZE m_Size;
    /* 000C */ POINT m_Origin;
    /* 0014 */ int32_t m_bVisible;
    /* 0018 */ int32_t m_bInverted;
    /* 001C */ POINT m_ptLogical;
    /* 0024 */ SIZE m_sizeLogical;
    /* 002C */ int32_t m_nBkMode;
    /* 0030 */ int32_t m_nObjectType;
    /* 0034 */ COLORREF m_crBackgroundColor;
    /* 0038 */ COLORREF m_crForegroundColor;
    /* 003C */ int32_t m_nUserData;

    CLCDBase();
    virtual ~CLCDBase();
    virtual int32_t Initialize();
    virtual void Shutdown();
    virtual void SetOrigin(POINT pt);
    virtual void SetOriginXY(int32_t x, int32_t y);
    virtual POINT& GetOrigin();
    virtual void SetSizeWH(int32_t width, int32_t height);
    virtual void SetSize(SIZE& size);
    virtual SIZE& GetSize();
    virtual int32_t GetWidth();
    virtual int32_t GetHeight();
    virtual void Show(int32_t show);
    virtual int32_t IsVisible();
    virtual void Invert(int32_t enable);
    virtual void ResetUpdate();
    virtual void SetLogicalOriginXY(int32_t x, int32_t y);
    virtual void SetLogicalOrigin(POINT& pt);
    virtual POINT& GetLogicalOrigin();
    virtual void SetLogicalSizeWH(int32_t width, int32_t height);
    virtual void SetLogicalSize(SIZE& size);
    virtual SIZE& GetLogicalSize();
    virtual void SetBackgroundMode(int32_t mode);
    virtual int32_t GetBackgroundMode();
    virtual int32_t GetObjectType();
    virtual void SetObjectType(int32_t type);
    virtual void SetForegroundColor(COLORREF color);
    virtual void SetBackgroundColor(COLORREF color);
    virtual void OnPrepareDraw(CLCDGfxBase& gfx);
    virtual void OnDraw(CLCDGfxBase& gfx);
    virtual void OnUpdate(uint32_t timestamp);
    virtual void SetUserData(int32_t data);
    virtual int32_t GetUserData();
};

#endif
