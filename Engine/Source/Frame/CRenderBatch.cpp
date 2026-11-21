#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include "HoT/Runtime/LuaRuntime.h"
#include "Engine/Source/Frame/CRenderBatch.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include "Engine/Source/Gx/CGxTex.h"
#include "Engine/Source/Gxu/GxuFont.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Services/TextBlock.h"

// STATIC: 0x00AC0F04
static uint16_t s_quadIndices[6] = { 0, 1, 2, 2, 1, 3 };

// VERIFY: d7a68913 auto
// OFFSET: 0x00482770
static int32_t SortByTexture(const void* elem1, const void* elem2) {
    const CSimpleBatchedMesh* mesh1 = static_cast<const CSimpleBatchedMesh*>(elem1);
    const CSimpleBatchedMesh* mesh2 = static_cast<const CSimpleBatchedMesh*>(elem2);

    // Compare pointers directly, a truncated 64-bit difference gives qsort an inconsistent order.
    if (mesh1->m_textureID != mesh2->m_textureID) {
        return reinterpret_cast<uintptr_t>(mesh1->m_textureID) < reinterpret_cast<uintptr_t>(mesh2->m_textureID) ? -1 : 1;
    }

    int32_t alphaMode1 = (mesh1->m_color && mesh1->m_alphaMode < GxBlend_Alpha) ? GxBlend_Alpha : mesh1->m_alphaMode;
    int32_t alphaMode2 = (mesh2->m_color && mesh2->m_alphaMode < GxBlend_Alpha) ? GxBlend_Alpha : mesh2->m_alphaMode;

    if (alphaMode1 != alphaMode2) {
        return alphaMode1 - alphaMode2;
    }

    if (mesh1->m_shader != mesh2->m_shader) {
        return reinterpret_cast<uintptr_t>(mesh1->m_shader) < reinterpret_cast<uintptr_t>(mesh2->m_shader) ? -1 : 1;
    }

    return 0;
}

// VERIFY: 2cc3a1bc auto
// OFFSET: 0x00484450
void CRenderBatch::Finish() {
    if (this->m_texturelist.Count() > 1) {
        qsort(this->m_texturelist.Ptr(), this->m_texturelist.Count(), sizeof(CSimpleBatchedMesh), &SortByTexture);
    }
}

// VERIFY: f0079ae6 auto
// OFFSET: 0x004846F0
void CRenderBatch::Queue(CTexture* texture, EGxBlend alphaMode, int32_t posCount, const C3Vector* position, const C2Vector* texCoord, int32_t colorCount, const CImVector* color, int32_t idxCount, const uint16_t* indices, CGxShader* shader) {
    CGxTex* gxTex = TextureGetGxTex(texture, 1, nullptr);

    if (!gxTex) {
        return;
    }

    uint32_t index = this->m_texturelist.Count();
    this->m_texturelist.SetCount(index + 1);

    CSimpleBatchedMesh* mesh = &this->m_texturelist[index];
    mesh->m_texture = texture;
    mesh->m_textureID = gxTex;
    mesh->m_alphaMode = alphaMode;
    mesh->m_shader = shader;
    mesh->m_posCount = posCount;
    mesh->m_position = position;
    mesh->m_texCoord = texCoord;
    mesh->m_color = color;
    mesh->m_colorCount = colorCount;
    mesh->m_indices = indices;
    mesh->m_idxCount = idxCount;
    mesh->m_onAtlas = TextureOnAtlas(texture);

    if (mesh->m_onAtlas) {
        TextureGetAtlasOffsetAndScale(mesh->m_texture, mesh->m_atlasOffset, mesh->m_atlasScale);
    }

    this->m_count++;
}

// VERIFY: 90c75e14 auto
// OFFSET: 0x00485530
CRenderBatch::CRenderBatch() {
    this->m_count = 0;
    this->m_stringbatch = nullptr;
}

// VERIFY: 6542125c manual
// OFFSET: 0x004858E0
void CRenderBatch::QueueCallback(void (*callback)(void*), void* param) {
    RENDERCALLBACKNODE* node = this->m_callbacks.NewNode(2, 0, 0);
    node->m_callback = callback;
    node->m_param = param;
    this->m_count++;
}

// VERIFY: 0c4ec2c0 auto
// OFFSET: 0x00485F00
void CRenderBatch::Clear() {
    this->m_texturelist.SetCount(0);

    if (this->m_stringbatch) {
        GxuFontDestroyBatch(this->m_stringbatch);
        this->m_stringbatch = nullptr;
    }

    this->m_callbacks.Clear();
    this->m_count = 0;
}

// VERIFY: 731464f9 manual
// OFFSET: [0x00486FC0, 0x00487160]
CRenderBatch::~CRenderBatch() {
    this->Clear();
}

// VERIFY: d1a22f87 auto
// OFFSET: 0x004843A0
void CRenderBatch::QueueFontString(CSimpleFontString* string) {
    HoT::LuaRuntime::Count(HoT::LuaRuntime::Counter::FontStrings);
    CRect rect;

    if (!string->GetRect(&rect)) {
        return;
    }

    if (rect.maxY < 0.0f || rect.minY > 1.0f || rect.maxX < 0.0f || rect.minX > 1.0f) {
        return;
    }

    if (!string->m_string) {
        return;
    }

    CGxString* gxString = TextBlockGetStringPtr(string->m_string);

    if (!gxString) {
        return;
    }

    if (!this->m_stringbatch) {
        this->m_stringbatch = GxuFontCreateBatch(false, false);
    }

    GxuFontAddToBatch(this->m_stringbatch, gxString);
    this->m_count++;
}

// VERIFY: 597baa7e manual
// OFFSET: 0x004847A0
void CRenderBatch::QueueTexture(CSimpleTexture* texture) {
    HoT::LuaRuntime::Count(HoT::LuaRuntime::Counter::Textures);
    CGxTex* gxTex = TextureGetGxTex(texture->m_texture, (texture->m_textureFlags & 1) ? 0 : 2, nullptr);

    if (!gxTex) {
        texture->NotifyDelayedLoad();
        return;
    }

    if (texture->IsResizePending()) {
        texture->Resize(1);
    }

    uint32_t flags = texture->m_textureFlags;

    if (flags & 2) {
        C2Vector texCoords[4];
        texture->GetTexCoord(texCoords);

        float left = texture->m_positions[0].x;
        float right = texture->m_positions[2].x;
        float bottom = texture->m_positions[1].y;
        float top = texture->m_positions[0].y;

        if (flags & 4) {
            uint32_t width = gxTex->m_width;
            float ndcWidth = DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * (right - left));

            if (width && ndcWidth > 0.0f) {
                if (texCoords[0].x != 0.0f) {
                    texCoords[0].x = ndcWidth / width;
                }

                if (texCoords[1].x != 0.0f) {
                    texCoords[1].x = ndcWidth / width;
                }

                if (texCoords[2].x != 0.0f) {
                    texCoords[2].x = ndcWidth / width;
                }

                if (texCoords[3].x != 0.0f) {
                    texCoords[3].x = ndcWidth / width;
                }
            }
        }

        if (texture->m_textureFlags & 8) {
            uint32_t height = gxTex->m_height;
            float ndcHeight = DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * (top - bottom));

            if (height && ndcHeight > 0.0f) {
                if (texCoords[0].y != 0.0f) {
                    texCoords[0].y = ndcHeight / height;
                }

                if (texCoords[1].y != 0.0f) {
                    texCoords[1].y = ndcHeight / height;
                }

                if (texCoords[2].y != 0.0f) {
                    texCoords[2].y = ndcHeight / height;
                }

                if (texCoords[3].y != 0.0f) {
                    texCoords[3].y = ndcHeight / height;
                }
            }
        }

        texture->m_textureFlags &= ~2u;
        texture->m_texCoords[0] = texCoords[0];
        texture->m_texCoords[1] = texCoords[1];
        texture->m_texCoords[2] = texCoords[2];
        texture->m_texCoords[3] = texCoords[3];
    }

    uint32_t colorCount = texture->m_colorCount;
    const CImVector* colors = colorCount ? texture->m_colors : nullptr;

    this->Queue(texture->m_texture, texture->m_alphaMode, 4, texture->m_positions, texture->m_texCoords, colorCount > 1 ? 4 : 0, colors, 6, s_quadIndices, texture->m_shader);
}
