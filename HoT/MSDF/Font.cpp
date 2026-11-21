#include "HoT/MSDF/Font.h"
#include <algorithm>
#include <cstring>
#include <immintrin.h>
#include <freetype/ftbbox.h>
#include <freetype/ftoutln.h>
#include <msdfgen.h>
#include <msdfgen-ext.h>
#include "Engine/Source/Gx/CGxTex.h"
#include "Engine/Source/Gx/CGxTexFlags.h"
#include "Engine/Source/Gx/Gx.h"
#include "HoT/MSDF/Cache.h"
#include "HoT/MSDF/Utils.h"
#include "HoT/MSDF/Validator.h"

namespace HoT::MSDF {

void Font::AtlasPage::Clear() {
    this->m_nextX = ATLAS_GUTTER;
    this->m_nextY = ATLAS_GUTTER;
    this->m_rowHeight = 0;
    this->m_codepoints.clear();
}

void Font::AtlasPage::MarkDirty(int32_t minX, int32_t minY, int32_t maxX, int32_t maxY) {
    if (!this->m_dirty) {
        this->m_dirty = true;
        this->m_dirtyMinX = minX;
        this->m_dirtyMinY = minY;
        this->m_dirtyMaxX = maxX;
        this->m_dirtyMaxY = maxY;
        return;
    }

    this->m_dirtyMinX = std::min(this->m_dirtyMinX, minX);
    this->m_dirtyMinY = std::min(this->m_dirtyMinY, minY);
    this->m_dirtyMaxX = std::max(this->m_dirtyMaxX, maxX);
    this->m_dirtyMaxY = std::max(this->m_dirtyMaxY, maxY);
}

Font::Font(FT_Face face, const uint8_t* fontData, size_t dataSize) : m_ftFace(face) {
    if (!face || !FreetypeLibrary()) {
        return;
    }

    this->m_msdfFont = msdfgen::loadFontData(FreetypeLibrary(), fontData, static_cast<int>(dataSize));

    if (!this->m_msdfFont) {
        return;
    }

    this->m_cache = std::make_unique<Cache>(fontData, dataSize, face->family_name ? face->family_name : "Unknown", face->style_name ? face->style_name : "", SDF_RENDER_SIZE, SDF_SPREAD);
    this->m_valid = this->m_cache->GetManifestSize() || AllowUnsafeFonts() || Validator::IsFontCompatible(this->m_msdfFont);

    if (this->m_valid) {
        this->m_glyphPool.reserve(4096);

        // Generate printable ASCII now so the first use of the font does not hitch.
        for (uint32_t cp = 32; cp <= 126; cp++) {
            this->GetGlyph(cp);
        }
    }
}

Font::~Font() {
    this->Release();
}

void Font::Release() {
    this->m_valid = false;
    this->m_glyphPool.clear();
    this->ClearHotCache();

    for (std::unique_ptr<AtlasPage>& page : this->m_atlasPages) {
        if (page->m_texture) {
            GxTexDestroy(page->m_texture);
            page->m_texture = nullptr;
        }
    }

    this->m_atlasPages.clear();
    this->m_cache.reset();

    if (this->m_msdfFont) {
        msdfgen::destroyFont(this->m_msdfFont);
        this->m_msdfFont = nullptr;
    }
}

CGxTex* Font::PageTexture(uint32_t page) const {
    return page < this->m_atlasPages.size() ? this->m_atlasPages[page]->m_texture : nullptr;
}

void Font::FlushDirtyPages() {
    for (std::unique_ptr<AtlasPage>& page : this->m_atlasPages) {
        if (!page->m_dirty || !page->m_texture) {
            continue;
        }

        GxTexUpdate(page->m_texture, page->m_dirtyMinX, page->m_dirtyMinY, page->m_dirtyMaxX, page->m_dirtyMaxY, 0);
        page->m_dirty = false;
    }
}

void Font::TextureCallback(EGxTexCommand command, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevel, void* userArg, uint32_t& texelStrideInBytes, const void*& texels) {
    if (command != GxTex_Latch) {
        return;
    }

    AtlasPage* page = static_cast<AtlasPage*>(userArg);
    texelStrideInBytes = ATLAS_SIZE * 4;
    texels = page->m_pixels.data();
}

void Font::ClearHotCache() {
    for (HotGlyph& hot : this->m_hotCache) {
        hot.m_codepoint = 0xFFFFFFFF;
        hot.m_metrics = nullptr;
    }
}

void Font::UpdateHotCache(uint32_t codepoint, const GlyphMetrics* metrics) {
    HotGlyph& hot = this->m_hotCache[codepoint & 63];
    hot.m_codepoint = codepoint;
    hot.m_metrics = metrics;
}

const GlyphMetrics* Font::FindInCache(uint32_t codepoint) {
    const HotGlyph& hot = this->m_hotCache[codepoint & 63];

    if (hot.m_codepoint == codepoint && hot.m_metrics) {
        return hot.m_metrics;
    }

    auto it = this->m_glyphPool.find(codepoint);

    if (it == this->m_glyphPool.end()) {
        return nullptr;
    }

    this->UpdateHotCache(codepoint, &it->second);

    return &it->second;
}

const GlyphMetrics* Font::GetGlyph(uint32_t codepoint) {
    if (!this->m_msdfFont) {
        return nullptr;
    }

    if (const GlyphMetrics* cached = this->FindInCache(codepoint)) {
        return cached;
    }

    return this->LoadAndCache(codepoint);
}

const GlyphMetrics* Font::LoadAndCache(uint32_t codepoint) {
    auto [it, inserted] = this->m_glyphPool.try_emplace(codepoint);
    GlyphMetrics& metrics = it->second;

    if (this->m_cache->TryLoadGlyph(codepoint, metrics)) {
        this->UploadGlyphToAtlas(metrics, codepoint);
        metrics.m_pixels = nullptr;
        this->UpdateHotCache(codepoint, &metrics);
        return &metrics;
    }

    GlyphToStore storage;
    storage.m_codepoint = codepoint;

    if (FT_Set_Pixel_Sizes(this->m_ftFace, SDF_RENDER_SIZE, SDF_RENDER_SIZE) != 0) {
        this->m_glyphPool.erase(it);
        return nullptr;
    }

    FT_UInt glyphIndex = FT_Get_Char_Index(this->m_ftFace, codepoint);

    if (FT_Load_Glyph(this->m_ftFace, glyphIndex, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING) != 0) {
        this->m_glyphPool.erase(it);
        return nullptr;
    }

    FT_GlyphSlot slot = this->m_ftFace->glyph;
    storage.m_bitmapLeft = slot->bitmap_left;
    storage.m_bitmapTop = slot->bitmap_top;

    bool hasOutline = slot->format == FT_GLYPH_FORMAT_OUTLINE && slot->outline.n_contours > 0;

    if (hasOutline) {
        FT_BBox bbox;
        FT_Outline_Get_BBox(&slot->outline, &bbox);

        uint16_t w = static_cast<uint16_t>(std::max(0, static_cast<int32_t>(((bbox.xMax + 63) >> 6) - (bbox.xMin >> 6))));
        uint16_t h = static_cast<uint16_t>(std::max(0, static_cast<int32_t>(((bbox.yMax + 63) >> 6) - (bbox.yMin >> 6))));

        if (w > 0 && h > 0) {
            uint16_t sdfW = w + 2 * SDF_SPREAD;
            uint16_t sdfH = h + 2 * SDF_SPREAD;
            storage.m_pixels.reserve(static_cast<size_t>(sdfW) * sdfH * 4);

            if (GenerateMSDF(storage.m_pixels, this->m_msdfFont, codepoint, sdfW, sdfH)) {
                storage.m_width = sdfW;
                storage.m_height = sdfH;
                storage.m_dataSize = static_cast<uint32_t>(storage.m_pixels.size());
                metrics.m_width = storage.m_width;
                metrics.m_height = storage.m_height;
                metrics.m_bitmapLeft = storage.m_bitmapLeft;
                metrics.m_bitmapTop = storage.m_bitmapTop;
                metrics.m_pixels = storage.m_pixels.data();
                this->UploadGlyphToAtlas(metrics, codepoint);
                metrics.m_pixels = nullptr;
                this->UpdateHotCache(codepoint, &metrics);
            }
        }
    }

    this->m_cache->StoreGlyph(std::move(storage));

    return &metrics;
}

bool Font::CreateAtlasPage() {
    std::unique_ptr<AtlasPage> page = std::make_unique<AtlasPage>();
    page->m_pixels.assign(static_cast<size_t>(ATLAS_SIZE) * ATLAS_SIZE * 4, 0);

    CGxTexFlags flags(GxTex_Linear, 0, 0, 0, 0, 0, 1, 0, 0, 0);

    if (!GxTexCreate(GxTex_2d, ATLAS_SIZE, ATLAS_SIZE, 0, GxTex_Argb8888, GxTex_Argb8888, flags, page.get(), TextureCallback, "MSDFAtlas", page->m_texture) || !page->m_texture) {
        return false;
    }

    page->MarkDirty(0, 0, ATLAS_SIZE, ATLAS_SIZE);
    this->m_atlasPages.push_back(std::move(page));

    return true;
}

bool Font::UploadGlyphToAtlas(GlyphMetrics& metrics, uint32_t codepoint) {
    if (!metrics.m_pixels || metrics.m_width == 0 || metrics.m_height == 0) {
        return true;
    }

    if (metrics.m_width + 2 * ATLAS_GUTTER > ATLAS_SIZE || metrics.m_height + 2 * ATLAS_GUTTER > ATLAS_SIZE) {
        metrics.m_width = 0;
        metrics.m_height = 0;
        return false;
    }

    int32_t pageIndex = -1;
    AtlasPage* target = nullptr;

    for (size_t i = 0; i < this->m_atlasPages.size(); i++) {
        AtlasPage* page = this->m_atlasPages[i].get();

        if (page->m_nextX + metrics.m_width + ATLAS_GUTTER <= ATLAS_SIZE && page->m_nextY + metrics.m_height + ATLAS_GUTTER <= ATLAS_SIZE) {
            pageIndex = static_cast<int32_t>(i);
            target = page;
            break;
        }

        int32_t nextY = page->m_nextY + page->m_rowHeight + ATLAS_GUTTER;

        if (nextY + metrics.m_height + ATLAS_GUTTER <= ATLAS_SIZE) {
            page->m_nextX = ATLAS_GUTTER;
            page->m_nextY = nextY;
            page->m_rowHeight = 0;
            pageIndex = static_cast<int32_t>(i);
            target = page;
            break;
        }
    }

    if (pageIndex == -1) {
        if (this->m_atlasPages.size() >= MAX_ATLAS_PAGES) {
            // Every page is full, recycle the oldest one and let strings using it rebuild.
            pageIndex = this->m_oldestPage;
            target = this->m_atlasPages[this->m_oldestPage].get();

            for (uint32_t cp : target->m_codepoints) {
                HotGlyph& hot = this->m_hotCache[cp & 63];

                if (hot.m_codepoint == cp) {
                    hot.m_codepoint = 0xFFFFFFFF;
                    hot.m_metrics = nullptr;
                }

                this->m_glyphPool.erase(cp);
            }

            target->Clear();
            std::fill(target->m_pixels.begin(), target->m_pixels.end(), 0);
            target->MarkDirty(0, 0, ATLAS_SIZE, ATLAS_SIZE);
            this->m_evictionCount++;
            this->m_oldestPage = (this->m_oldestPage + 1) % MAX_ATLAS_PAGES;
        } else {
            if (!this->CreateAtlasPage()) {
                return false;
            }

            pageIndex = static_cast<int32_t>(this->m_atlasPages.size() - 1);
            target = this->m_atlasPages.back().get();
        }
    }

    if (!target->m_texture) {
        return false;
    }

    const size_t pitch = ATLAS_SIZE * 4;
    const size_t rowBytes = static_cast<size_t>(metrics.m_width) * 4;
    const uint8_t* src = metrics.m_pixels;
    uint8_t* dest = target->m_pixels.data() + target->m_nextY * pitch + target->m_nextX * 4;

    for (uint16_t y = 0; y < metrics.m_height; y++) {
        memcpy(dest, src, rowBytes);
        dest += pitch;
        src += rowBytes;
    }

    target->MarkDirty(target->m_nextX, target->m_nextY, target->m_nextX + metrics.m_width, target->m_nextY + metrics.m_height);

    float atlasSize = static_cast<float>(ATLAS_SIZE);
    metrics.m_u0 = static_cast<float>(target->m_nextX) / atlasSize;
    metrics.m_v0 = static_cast<float>(target->m_nextY) / atlasSize;
    metrics.m_u1 = static_cast<float>(target->m_nextX + metrics.m_width) / atlasSize;
    metrics.m_v1 = static_cast<float>(target->m_nextY + metrics.m_height) / atlasSize;
    metrics.m_page = static_cast<uint16_t>(pageIndex);

    target->m_nextX += metrics.m_width + ATLAS_GUTTER;
    target->m_rowHeight = std::max(target->m_rowHeight, static_cast<int32_t>(metrics.m_height));
    target->m_codepoints.push_back(codepoint);

    return true;
}

bool Font::GenerateMSDF(std::vector<uint8_t>& outData, msdfgen::FontHandle* font, uint32_t codepoint, int32_t sdfW, int32_t sdfH) {
    if (sdfW <= 0 || sdfH <= 0 || sdfW > 512 || sdfH > 512) {
        return false;
    }

    msdfgen::Shape shape;

    if (!msdfgen::loadGlyph(shape, font, static_cast<msdfgen::unicode_t>(codepoint))) {
        return false;
    }

    if (shape.contours.empty()) {
        outData.assign(static_cast<size_t>(sdfW) * sdfH * 4, 0);
        return true;
    }

    msdfgen::resolveShapeGeometry(shape);
    msdfgen::edgeColoringInkTrap(shape, 3.0, 0);

    msdfgen::Shape::Bounds bounds = shape.getBounds();
    double shapeW = bounds.r - bounds.l;
    double shapeH = bounds.t - bounds.b;

    if (shapeW <= 0 || shapeH <= 0) {
        return false;
    }

    double usableW = static_cast<double>(sdfW) - 2.0 * SDF_SPREAD;
    double usableH = static_cast<double>(sdfH) - 2.0 * SDF_SPREAD;

    if (usableW <= 0 || usableH <= 0) {
        return false;
    }

    double scale = std::min(usableW / shapeW, usableH / shapeH);
    msdfgen::Projection projection(msdfgen::Vector2(scale, scale), msdfgen::Vector2(SDF_SPREAD / scale - bounds.l, SDF_SPREAD / scale - bounds.b));

    const int32_t totalPixels = sdfW * sdfH;
    std::vector<float> msdfBuf = Pools::s_float.AcquireSized(static_cast<size_t>(totalPixels) * 3);
    std::vector<float> sdfBuf = Pools::s_float.AcquireSized(static_cast<size_t>(totalPixels));

    msdfgen::BitmapRef<float, 3> msdfBitmap(msdfBuf.data(), sdfW, sdfH);
    msdfgen::BitmapRef<float, 1> sdfBitmap(sdfBuf.data(), sdfW, sdfH);

    msdfgen::MSDFGeneratorConfig config;
    config.overlapSupport = true;

    msdfgen::Range msdfRange(SDF_SPREAD / scale);
    msdfgen::generateMSDF(msdfBitmap, shape, projection, msdfRange, config);
    msdfgen::SDFTransformation msdfTransform(projection, msdfRange);
    msdfgen::distanceSignCorrection(msdfBitmap, shape, msdfTransform, msdfgen::FILL_NONZERO);

    // The alpha channel is a wider plain SDF the pixel shader uses for outlines.
    msdfgen::Range sdfRange(SDF_SPREAD / scale * 5.0);
    msdfgen::generateSDF(sdfBitmap, shape, projection, sdfRange);
    msdfgen::SDFTransformation sdfTransform(projection, sdfRange);
    msdfgen::distanceSignCorrection(sdfBitmap, shape, sdfTransform, msdfgen::FILL_NONZERO);

    outData.resize(static_cast<size_t>(totalPixels) * 4);

    uint8_t* dest = outData.data();
    const float* srcMSDF = msdfBuf.data();
    const float* srcSDF = sdfBuf.data();
    int32_t i = 0;

    const __m128 v255 = _mm_set1_ps(255.0f);
    const __m128 v0 = _mm_set1_ps(0.0f);
    const __m128i shuffleMask = _mm_setr_epi8(0, 1, 2, 12, 3, 4, 5, 13, 6, 7, 8, 14, 9, 10, 11, 15);

    for (; i <= totalPixels - 4; i += 4) {
        __m128 m0 = _mm_loadu_ps(srcMSDF + i * 3);
        __m128 m1 = _mm_loadu_ps(srcMSDF + i * 3 + 4);
        __m128 m2 = _mm_loadu_ps(srcMSDF + i * 3 + 8);
        __m128 s0 = _mm_loadu_ps(srcSDF + i);

        m0 = _mm_min_ps(_mm_max_ps(_mm_mul_ps(m0, v255), v0), v255);
        m1 = _mm_min_ps(_mm_max_ps(_mm_mul_ps(m1, v255), v0), v255);
        m2 = _mm_min_ps(_mm_max_ps(_mm_mul_ps(m2, v255), v0), v255);
        s0 = _mm_min_ps(_mm_max_ps(_mm_mul_ps(s0, v255), v0), v255);

        __m128i p01 = _mm_packus_epi32(_mm_cvtps_epi32(m0), _mm_cvtps_epi32(m1));
        __m128i p2s = _mm_packus_epi32(_mm_cvtps_epi32(m2), _mm_cvtps_epi32(s0));
        __m128i full = _mm_packus_epi16(p01, p2s);

        _mm_storeu_si128(reinterpret_cast<__m128i*>(dest + i * 4), _mm_shuffle_epi8(full, shuffleMask));
    }

    for (; i < totalPixels; i++) {
        dest[i * 4 + 0] = static_cast<uint8_t>(std::clamp(srcMSDF[i * 3 + 0] * 255.0f, 0.0f, 255.0f));
        dest[i * 4 + 1] = static_cast<uint8_t>(std::clamp(srcMSDF[i * 3 + 1] * 255.0f, 0.0f, 255.0f));
        dest[i * 4 + 2] = static_cast<uint8_t>(std::clamp(srcMSDF[i * 3 + 2] * 255.0f, 0.0f, 255.0f));
        dest[i * 4 + 3] = static_cast<uint8_t>(std::clamp(srcSDF[i] * 255.0f, 0.0f, 255.0f));
    }

    Pools::s_float.Release(std::move(msdfBuf));
    Pools::s_float.Release(std::move(sdfBuf));

    return true;
}

}
