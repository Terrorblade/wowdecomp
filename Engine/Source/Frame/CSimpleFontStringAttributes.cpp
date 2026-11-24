#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleFontStringAttributes.h"
#include <cmath>
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Services/TextBlock.h"
#include "Engine/Source/Storm/H/storm.h"

// INLINED
CSimpleFontStringAttributes::CSimpleFontStringAttributes() {
    this->m_fontHeight = 0.0f;
    this->m_spacing = 0.0f;
    this->m_fontFlags = 0;
    this->m_styleFlags = TEXTBLOCK_DEFAULT_STYLE;
    this->m_color.Set(1.0f, 1.0f, 1.0f, 1.0f);
    this->m_flags = 0;
}

// VERIFY: bbf3b0cd auto
// OFFSET: 0x00482D10
int32_t CSimpleFontStringAttributes::SetFont(const char* fontName, float fontHeight, uint32_t fontFlags) {
    if (!fontName) {
        return 0;
    }

    if (this->m_font.m_str && !SStrCmpI(fontName, this->m_font.m_str, STORM_MAX_STR) && fabsf(fontHeight - this->m_fontHeight) < 0.00000023841858f && fontFlags == this->m_fontFlags) {
        return 1;
    }

    if (!(fontHeight > 0.0f)) {
        return 0;
    }

    HTEXTFONT font = TextBlockGenerateFont(fontName, fontFlags, fontHeight);

    if (!font) {
        return 0;
    }

    HandleClose(reinterpret_cast<HOBJECT>(font));
    this->m_font.Copy(fontName);
    this->m_flags |= FLAG_FONT_PRESENT | FLAG_FONT_UPDATE;
    this->m_fontHeight = fontHeight;
    this->m_fontFlags = fontFlags;

    return 1;
}

// VERIFY: da866dc0 auto
// OFFSET: 0x00482DC0
void CSimpleFontStringAttributes::SetShadow(const CImVector& color, const C2Vector& offset) {
    if (*reinterpret_cast<const uint32_t*>(&color) != *reinterpret_cast<uint32_t*>(&this->m_shadowColor) || offset.x != this->m_shadowOffset.x || offset.y != this->m_shadowOffset.y || !(this->m_flags & FLAG_SHADOW_PRESENT)) {
        this->m_shadowColor = color;
        this->m_shadowOffset = offset;
        this->m_flags |= FLAG_SHADOW_PRESENT | FLAG_SHADOW_UPDATE;
    }
}

// VERIFY: b65d65ae manual
// OFFSET: 0x004A3740
void CSimpleFontStringAttributes::SetShadowOffset(const C2Vector& offset) {
    this->SetShadow(this->m_shadowColor, offset);
}

// VERIFY: 1018d942 auto
// OFFSET: 0x00483510
void CSimpleFontStringAttributes::Update(CSimpleFontStringAttributes& attributes, int32_t flags) {
    uint32_t mask = this->m_flags & flags;

    if ((this->m_flags & FLAG_FONT_PRESENT) && (mask & FLAG_FONT_UPDATE)) {
        attributes.SetFont(this->m_font.m_str, this->m_fontHeight, this->m_fontFlags);
    }

    if ((this->m_flags & FLAG_STYLE_PRESENT) && (mask & FLAG_STYLE_UPDATE)) {
        if (this->m_styleFlags != attributes.m_styleFlags || !(attributes.m_flags & FLAG_STYLE_PRESENT)) {
            attributes.m_flags |= FLAG_STYLE_PRESENT | FLAG_STYLE_UPDATE;
            attributes.m_styleFlags = this->m_styleFlags;
        }
    }

    if ((this->m_flags & FLAG_COLOR_PRESENT) && (mask & FLAG_COLOR_UPDATE)) {
        if (*reinterpret_cast<uint32_t*>(&this->m_color) != *reinterpret_cast<uint32_t*>(&attributes.m_color) || !(attributes.m_flags & FLAG_COLOR_PRESENT)) {
            attributes.m_color = this->m_color;
            attributes.m_flags |= FLAG_COLOR_PRESENT | FLAG_COLOR_UPDATE;
        }
    }

    if ((this->m_flags & FLAG_SHADOW_PRESENT) && (mask & FLAG_SHADOW_UPDATE)) {
        attributes.SetShadow(this->m_shadowColor, this->m_shadowOffset);
    }

    if ((this->m_flags & FLAG_SPACING_PRESENT) && (mask & FLAG_SPACING_UPDATE)) {
        if (fabsf(this->m_spacing - attributes.m_spacing) >= 0.00000023841858f || !(attributes.m_flags & FLAG_SPACING_PRESENT)) {
            attributes.m_flags |= FLAG_SPACING_PRESENT | FLAG_SPACING_UPDATE;
            attributes.m_spacing = this->m_spacing;
        }
    }
}

// VERIFY: 988687b9 manual
// OFFSET: 0x004872F0
void CSimpleFontStringAttributes::Update(CSimpleFontString* string, int32_t flags) {
    if (!string) {
        DebugValidate("string");
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    uint32_t mask = this->m_flags & flags;

    if ((this->m_flags & FLAG_FONT_PRESENT) && (mask & FLAG_FONT_UPDATE)) {
        string->SetFont(this->m_font.m_str, this->m_fontHeight, this->m_fontFlags, false);
    }

    if ((this->m_flags & FLAG_STYLE_PRESENT) && (mask & FLAG_STYLE_UPDATE)) {
        string->SetStyleFlags(this->m_styleFlags);
    }

    if ((this->m_flags & FLAG_COLOR_PRESENT) && (mask & FLAG_COLOR_UPDATE)) {
        string->SetVertexColor(this->m_color);
    }

    if ((this->m_flags & FLAG_SHADOW_PRESENT) && (mask & FLAG_SHADOW_UPDATE)) {
        if (this->m_shadowOffset.x == 0.0f && this->m_shadowOffset.y == 0.0f) {
            string->RemoveShadow();
        } else {
            string->AddShadow(this->m_shadowColor, this->m_shadowOffset);
        }
    }

    if ((this->m_flags & FLAG_SPACING_PRESENT) && (mask & FLAG_SPACING_UPDATE)) {
        string->SetSpacing(this->m_spacing);
    }
}

// VERIFY: 00763e2e auto
// OFFSET: 0x00483600
CSimpleFontStringAttributes& CSimpleFontStringAttributes::operator=(const CSimpleFontStringAttributes& source) {
    if (source.m_flags & FLAG_FONT_PRESENT) {
        this->SetFont(source.m_font.m_str, source.m_fontHeight, source.m_fontFlags);
    }

    if (source.m_flags & FLAG_STYLE_PRESENT) {
        if (source.m_styleFlags != this->m_styleFlags || !(this->m_flags & FLAG_STYLE_PRESENT)) {
            this->m_flags |= FLAG_STYLE_PRESENT | FLAG_STYLE_UPDATE;
            this->m_styleFlags = source.m_styleFlags;
        }
    }

    if (source.m_flags & FLAG_COLOR_PRESENT) {
        if (*reinterpret_cast<const uint32_t*>(&source.m_color) != *reinterpret_cast<uint32_t*>(&this->m_color) || !(this->m_flags & FLAG_COLOR_PRESENT)) {
            this->m_color = source.m_color;
            this->m_flags |= FLAG_COLOR_PRESENT | FLAG_COLOR_UPDATE;
        }
    }

    if (source.m_flags & FLAG_SHADOW_PRESENT) {
        this->SetShadow(source.m_shadowColor, source.m_shadowOffset);
    }

    if (source.m_flags & FLAG_SPACING_PRESENT) {
        if (fabsf(source.m_spacing - this->m_spacing) >= 0.00000023841858f || !(this->m_flags & FLAG_SPACING_PRESENT)) {
            this->m_flags |= FLAG_SPACING_PRESENT | FLAG_SPACING_UPDATE;
            this->m_spacing = source.m_spacing;
        }
    }

    return *this;
}

// VERIFY: ec752594 auto
// OFFSET: 0x00496390
void CSimpleFontStringAttributes::SetJustifyH(uint8_t justify) {
    uint32_t styleFlags = (this->m_styleFlags & ~TEXTBLOCK_JUSTIFY_H_MASK) | (justify & TEXTBLOCK_JUSTIFY_H_MASK);

    if (styleFlags != this->m_styleFlags || !(this->m_flags & FLAG_STYLE_PRESENT)) {
        this->m_flags |= FLAG_STYLE_PRESENT | FLAG_STYLE_UPDATE;
        this->m_styleFlags = styleFlags;
    }
}

// VERIFY: 15b1ef9e auto
// OFFSET: 0x004963C0
void CSimpleFontStringAttributes::SetJustifyV(uint8_t justify) {
    uint32_t styleFlags = (this->m_styleFlags & ~TEXTBLOCK_JUSTIFY_V_MASK) | (justify & TEXTBLOCK_JUSTIFY_V_MASK);

    if (styleFlags != this->m_styleFlags || !(this->m_flags & FLAG_STYLE_PRESENT)) {
        this->m_flags |= FLAG_STYLE_PRESENT | FLAG_STYLE_UPDATE;
        this->m_styleFlags = styleFlags;
    }
}
