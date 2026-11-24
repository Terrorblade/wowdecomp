#ifndef ENGINE_SOURCE_GXU_TYPES_H
#define ENGINE_SOURCE_GXU_TYPES_H

#include <cstddef>
#include <cstdint>

enum EGxStringFlags : uint32_t {
    EGxStringFlags_None = 0,
    EGxStringFlags_Shadow = 1,
    EGxStringFlags_NoWordWrap = 2,
    EGxStringFlags_FixedSize = 4,
    EGxStringFlags_UniformColor = 8,
    EGxStringFlags_FixedWidth = 0x10,
    EGxStringFlags_Gradient = 0x20,
    EGxStringFlags_NonSpaceWrap = 0x40,
    EGxStringFlags_Unscaled = 0x80,
    EGxStringFlags_IgnoreColorCodes = 0x100,
    EGxStringFlags_IgnoreNewlineCodes = 0x200,
    EGxStringFlags_IgnoreHyperlinkCodes = 0x400,
    EGxStringFlags_IgnorePipeCodes = 0x800,
    EGxStringFlags_IgnoreTextureCodes = 0x1000,
    EGxStringFlags_Indented = 0x2000,
};

enum EGxFontHJusts {
    GxHJ_Left = 0,
    GxHJ_Center = 1,
    GxHJ_Right = 2,
    GxHJ_Last = 3,
};

enum EGxFontVJusts {
    GxVJ_Top = 0,
    GxVJ_Middle = 1,
    GxVJ_Bottom = 2,
    GxVJ_Last = 3,
};

enum QUOTEDCODE {
    CODE_INVALIDCODE = 0,
    CODE_COLORON = 1,
    CODE_COLORRESTORE = 2,
    CODE_NEWLINE = 3,
    CODE_PIPE = 4,
    CODE_HYPERLINKSTART = 5,
    CODE_HYPERLINKSTOP = 6,
    CODE_EMBEDDEDTEXTURESTART = 7,
    CODE_EMBEDDEDTEXTURESTOP = 8,
    NUM_QUOTEDCODES = 9,
};

struct QUOTEDCODEINFO {
    /* 0000 */ uint32_t m_ignoreFlag;
    /* 0004 */ char m_code;
    /* 0008 */ int32_t m_escaped;
};

struct GRADIENTINFO {
    /* 0000 */ uint8_t m_texturePage;
    /* 0004 */ uint32_t m_vertIndex;
};

struct GXUFONTHYPERLINKINFO {
    /* 0000 */ float m_lineHeight;
    /* 0004 */ float m_indentation;
    /* 0008 */ float m_baseLineHeight;
    /* 000C */ float m_right;
    /* 0010 */ const char* m_linkData;
    /* 0014 */ uint32_t m_linkDataLength;
    /* 0018 */ const char* m_linkText;
    /* 001C */ uint32_t m_linkTextLength;
    /* 0020 */ const char* m_colorCode;
    /* 0024 */ uint32_t m_colorCodeLength;

    GXUFONTHYPERLINKINFO();
};

struct HFACE__ {
    /* 0000 */ int32_t m_unused;
};

typedef HFACE__* HFACE;

#endif
