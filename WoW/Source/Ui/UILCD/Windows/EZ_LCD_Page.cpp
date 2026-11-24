#include "WoW/Source/Ui/UILCD/Windows/EZ_LCD_Page.h"
#include <new>
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/Ui/UILCD/Windows/LCDBitmap.h"
#include "WoW/Source/Ui/UILCD/Windows/LCDText.h"

// VERIFY: 7e56e1fb auto
// OFFSET: 0x0095AFF0
CEzLcdPage::CEzLcdPage() {
    this->m_container = nullptr;
}

// VERIFY: 822a4fdd manual
// OFFSET: 0x0095B040
CEzLcdPage::CEzLcdPage(CEzLcd* container) {
    this->m_container = container;

    for (int32_t i = 0; i < 11; i++) {
        this->m_buttonIsPressed[i] = 0;
        this->m_buttonWasPressed[i] = 0;
    }
}

// VERIFY: 02a82783 manual
// OFFSET: 0x0095B010
CEzLcdPage::~CEzLcdPage() {
}

// VERIFY: 8be292d0 manual
// OFFSET: 0x0095B080
CLCDText* CEzLcdPage::AddText(int32_t type, int32_t size, int32_t alignment, int32_t maxLengthPixels, int32_t userData, int32_t numberOfLines) {
    int32_t height = 13;
    int32_t pointSize = 8;
    int32_t logicalOriginY = -1;

    if (type != LCD_OBJECT_TYPE_TEXT) {
        return nullptr;
    }

    CLCDText* text = new (SMemAlloc(sizeof(CLCDText), __FILE__, __LINE__, 0)) CLCDText();
    text->Initialize();
    POINT origin = { 0, 0 };
    text->SetOrigin(origin);
    text->SetFontFaceName("Microsoft Sans Serif");
    text->SetAlignment(alignment);
    text->SetBackgroundMode(OPAQUE);

    switch (size) {
        case 6:
        case 7:
            height = 12 * numberOfLines;
            pointSize = 7;
            logicalOriginY = -3;
            break;

        case 8:
            height = 13 * numberOfLines;
            pointSize = size;
            logicalOriginY = -1;
            break;

        case 12:
            text->SetFontWeight(FW_BOLD);
            height = 20 * numberOfLines;
            pointSize = 12;
            logicalOriginY = -2;
            break;
    }

    text->SetSizeWH(maxLengthPixels, height);
    text->SetFontPointSize(pointSize);
    text->SetLogicalOriginXY(0, logicalOriginY);
    text->SetObjectType(LCD_OBJECT_TYPE_TEXT);

    if (numberOfLines > 1) {
        text->SetWordWrap(1);
    }

    text->SetUserData(userData);
    this->AddObject(text);
    return text;
}

// VERIFY: 183b66a1 manual
// OFFSET: 0x0095B1F0
CLCDText* CEzLcdPage::AddColorText(int32_t type, int32_t size, int32_t alignment, int32_t maxLengthPixels, int32_t userData, int32_t numberOfLines, int32_t fontWeight) {
    int32_t height = 24;
    int32_t pointSize = 16;
    int32_t logicalOriginY = -3;

    if (type != LCD_OBJECT_TYPE_TEXT) {
        return nullptr;
    }

    CLCDText* text = new CLCDText();
    text->Initialize();
    POINT origin = { 0, 0 };
    text->SetOrigin(origin);
    text->SetFontFaceName("Microsoft Sans Serif");
    text->SetAlignment(alignment);
    text->SetBackgroundMode(TRANSPARENT);

    if (size == 6) {
        height = 14 * numberOfLines;
        pointSize = 8;
        logicalOriginY = 0;
    } else if (size == 7) {
        height = 20 * numberOfLines;
        pointSize = 12;
        logicalOriginY = -2;
    } else if (size == 8) {
        height = 24 * numberOfLines;
        pointSize = 16;
        logicalOriginY = -3;
    } else if (size == 12) {
        height = 37 * numberOfLines;
        pointSize = 24;
        logicalOriginY = -3;
    }

    text->SetSizeWH(maxLengthPixels, height);
    text->SetFontPointSize(pointSize);
    text->SetLogicalOriginXY(0, logicalOriginY);
    text->SetObjectType(LCD_OBJECT_TYPE_TEXT);
    text->SetUserData(userData);

    if (numberOfLines > 1) {
        text->SetWordWrap(1);
    }

    this->AddObject(text);
    return text;
}

// VERIFY: c98b6901 manual
// OFFSET: 0x0095B370
CLCDBitmap* CEzLcdPage::AddBitmap(int32_t width, int32_t height, int32_t bitmapWidth, int32_t bitmapHeight) {
    CLCDBitmap* bitmap = new CLCDBitmap();
    bitmap->Initialize();
    POINT origin = { 0, 0 };
    bitmap->SetOrigin(origin);
    bitmap->SetSizeWH(width, height);
    bitmap->SetBitmapSize(bitmapWidth, bitmapHeight);
    bitmap->SetAlpha(1);
    bitmap->SetUserData(-1);
    this->AddObject(bitmap);
    return bitmap;
}
