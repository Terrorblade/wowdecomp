#ifndef ENGINE_SOURCE_FRAME_CSIMPLETEXTURE_H
#define ENGINE_SOURCE_FRAME_CSIMPLETEXTURE_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Base/CDataAllocator.h"
#include "Engine/Source/Frame/CSimpleRegion.h"
#include "Engine/Source/Frame/Types.h"
#include "Engine/Source/Gx/Types.h"
#include "Engine/Source/Tempest/C2Vector.h"
#include "Engine/Source/Tempest/C3Vector.h"

class CImVector;
class CRect;
class CRenderBatch;
class CSimpleFrame;
class CGxShader;
class CGxTex;
class CStatus;
class CTexture;
class XMLNode;

class CSimpleTexture : public CSimpleRegion {
    public:

    enum FlagBitIndices : int32_t {
        FLAG_HORIZ_TILE_BIT_INDEX = 2,
        FLAG_VERT_TILE_BIT_INDEX = 3,
    };

    enum TEXTURE_FLAGS : uint32_t {
        FLAG_NONBLOCKING = 0x1,
        FLAG_TEXCOORDS_DIRTY = 0x2,
        FLAG_HORIZ_TILE = 0x4,
        FLAG_VERT_TILE = 0x8,
        FLAG_INITIALIZATION_MASK = 0xF,
    };

    static CDataAllocator s_allocator;
    static int32_t s_count;
    static CGxShader* s_imageModePixelShaders[2];
    static const char* s_imageModePixelShaderNames[2];
    static int32_t s_metatable;
    static int32_t s_objectType;
    static const char* s_objectTypeName;
    static EGxTexFilter s_textureFilterMode;

    static void operator delete(void* ptr);
    static void CreateScriptMetaTable();
    static void Destroy();
    static void DestroyScriptMetaTable();
    static CGxShader* GetImageModePixelShader(TextureImageMode mode);
    static int32_t GetObjectType();
    static void Init();
    static void RegisterScriptMethods(lua_State* L);

    /* 00D4 */ CTexture* m_texture;
    /* 00D8 */ EGxBlend m_alphaMode;
    /* 00DC */ CGxShader* m_shader;
    /* 00E0 */ C3Vector m_positions [4];
    /* 0110 */ C3Vector m_storedPositions [4];
    /* 0140 */ C2Vector m_texCoords [4];
    /* 0160 */ uint32_t m_textureFlags;

    virtual ~CSimpleTexture();
    virtual int32_t GetScriptMetaTable();
    virtual bool IsA(int32_t type);
    virtual bool IsA(const char* typeName);
    virtual const char* GetObjectTypeName();
    virtual void PreOnAnimUpdate();
    virtual void AddAnimTranslation(const C2Vector& translation);
    virtual void AddAnimRotation(FRAMEPOINT origin, const C2Vector& offset, float radians);
    virtual void AddAnimScale(FRAMEPOINT origin, const C2Vector& offset, const C2Vector& scale);
    virtual void Draw(CRenderBatch* batch);
    virtual void StoreAnimPosition();
    virtual void ResetAnimPosition();
    virtual void LoadXML(XMLNode* node, CStatus* status);
    virtual float GetWidth();
    virtual float GetHeight();
    virtual int32_t IsObjectLoaded();
    virtual void OnFrameSizeChanged(const CRect& rect);

    CSimpleTexture(CSimpleFrame* frame, uint32_t drawLayer, int32_t show);
    CGxTex* GetGxTex();
    void GetTexCoord(C2Vector* texCoords);
    int32_t GetTexture(char* buffer, uint32_t bufferLen);
    void PostLoadXML(XMLNode* node, CStatus* status);
    void SetBlendMode(EGxBlend blend);
    void SetPosition(const CRect& rect, C3Vector* position);
    void SetRotation(float radians, float centerX, float centerY);
    void SetShader(CGxShader* shader);
    void SetTexCoord(const C2Vector* texCoords);
    void SetTexCoord(const CRect& texRect);
    int32_t SetTexture(CTexture* texture);
    int32_t SetTexture(const CImVector& color);
    int32_t SetTexture(const char* fileName, int32_t wrapU, int32_t wrapV, EGxTexFilter filter, TextureImageMode mode);
};

#endif
