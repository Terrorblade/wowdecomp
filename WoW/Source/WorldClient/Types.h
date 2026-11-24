#ifndef WOW_SOURCE_WORLDCLIENT_TYPES_H
#define WOW_SOURCE_WORLDCLIENT_TYPES_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Gx/Types.h"
#include "Engine/Source/Storm/H/stpl.h"

enum MAP_OBJECT_CREATE_FLAGS : uint32_t {
    MAP_OBJECT_CREATE_FLAG_COLLIDABLE = 0x1,
    MAP_OBJECT_CREATE_FLAG_NO_BLOB_SHADOW = 0x2,
    MAP_OBJECT_CREATE_FLAG_ALWAYS_ANIMATE = 0x4,
    MAP_OBJECT_CREATE_FLAG_LINK_MULTIPLE_WMO_GROUPS = 0x8,
    MAP_OBJECT_CREATE_FLAG_USE_CHARACTER_AMBIENT = 0x10,
    MAP_OBJECT_CREATE_FLAG_UNKNOWN_5 = 0x20,
};
constexpr int32_t MAP_OBJECT_CREATE_MULTIPLE_GROUPS_SHIFT = 3;

class CMapChunk;
class CMapChunkDoodadDefLink;
class CMapDoodadDef;
class CMapObjDef;
class CMapObjDefGroup;
class CMapObjDefMapObjDefGroupLink;
class CTexture;

class CMapAreaTexture {
    public:
    /* 0000 */ char* m_textureName;
    /* 0004 */ CTexture* m_texture;
};

class CMapChunkDoodadDefLink {
    public:
    /* 0000 */ uint32_t m_objectIndex;
    /* 0004 */ CMapDoodadDef* m_owner;
    /* 0008 */ CMapChunk* m_ref;
    /* 000C */ TSLink<CMapChunkDoodadDefLink> m_refLink;
    /* 0014 */ TSLink<CMapChunkDoodadDefLink> m_ownerLink;
};

class CMapObjDefMapObjDefGroupLink {
    public:
    /* 0000 */ uint32_t m_objectIndex;
    /* 0004 */ CMapObjDefGroup* m_owner;
    /* 0008 */ CMapObjDef* m_ref;
    /* 000C */ TSLink<CMapObjDefMapObjDefGroupLink> m_refLink;
    /* 0014 */ TSLink<CMapObjDefMapObjDefGroupLink> m_ownerLink;
};

enum Enables : uint32_t {
    Enable_Doodads = 1,
    Enable_Chunks = 2,
    Enable_Lod = 4,
    Enable_Texture = 8,
    Enable_Sky = 16,
    Enable_Culling = 32,
    Enable_Shadow = 64,
    Enable_Collision = 128,
    Enable_MapObjs = 0x100,
    Enable_MapObjLight = 0x200,
    Enable_Footprints = 0x400,
    Enable_MapObjTex = 0x800,
    Enable_Portals = 0x1000,
    Enable_PortalVis = 0x2000,
    Enable_ObjectFade = 0x4000,
    Enable_ObjectFadeZFill = 0x8000,
    Enable_DebugBSP = 0x10000,
    Enable_CrappyBatches = 0x20000,
    Enable_ZoneBounds = 0x40000,
    Enable_MapObjBSP = 0x80000,
    Enable_DetailDoodads = 0x100000,
    Enable_CollisionDebug = 0x200000,
    Enable_AABoxes = 0x400000,
    Enable_Trilinear = 0x800000,
    Enable_Water = 0x1000000,
    Enable_Particulates = 0x2000000,
    Enable_LowDetail = 0x4000000,
    Enable_Specular = 0x8000000,
    Enable_PixelShaders = 0x10000000,
    Enable_Wireframe = 0x20000000,
    Enable_ShowNormals = 0x40000000,
    Enable_Anisotropic = 0x80000000,
    Enable_InitialMask = 0x7104B73,
};

enum Enables2 {
    Enable_VertexShader = 1,
    Enable_HwPcf = 2,
    Enable_ProjectedTextures = 4,
};

enum MapObjFlag {
    MAPOBJ_FLAG_NONE = 0,
    MAPOBJ_FLAG_UNPLACED = 1,
    MAPOBJ_FLAG_INTERIOR = 2,
    MAPOBJ_FLAG_EXTERIOR = 4,
    MAPOBJ_FLAG_REFS_CREATED = 8,
    MAPOBJ_FLAG_GROUP_INIT = 16,
    MAPOBJ_FLAG_DISABLED = 32,
    MAPOBJ_FLAG_IMPASSABLE = 64,
    MAPOBJ_FLAG_PREPARED = 128,
    MAPOBJ_FLAG_NO_HITTEST = 0x100,
    MAPOBJ_FLAG_IN_MAPOBJ = 0x200,
    MAPOBJ_FLAG_NO_DEPTH_SORT = 0x400,
    MAPOBJ_FLAG_BIODOME = 0x800,
    MAPOBJ_FLAG_PROJ_TEX = 0x1000,
    MAPOBJ_FLAG_NO_MAPOBJ_LINK = 0x2000,
    MAPOBJ_FLAG_ALL_LOADED = 0x4000,
    MAPOBJ_FLAG_INT_FOG = 0x8000,
    MAPOBJ_FLAG_MAPOBJDEF = 0x10000,
    MAPOBJ_FLAG_SHADOW_20000 = 0x20000,
};

struct SIffChunk {

    enum TOKEN : uint32_t {
        TOKEN_MAHO = 0x4D41484F,
        TOKEN_MAOF = 0x4D414F46,
        TOKEN_MARE = 0x4D415245,
        TOKEN_MCAL = 0x4D43414C,
        TOKEN_MCCV = 0x4D434356,
        TOKEN_MCLQ = 0x4D434C51,
        TOKEN_MCLY = 0x4D434C59,
        TOKEN_MCNK = 0x4D434E4B,
        TOKEN_MCNR = 0x4D434E52,
        TOKEN_MCRF = 0x4D435246,
        TOKEN_MCSE = 0x4D435345,
        TOKEN_MCSH = 0x4D435348,
        TOKEN_MCVP = 0x4D435650,
        TOKEN_MCVT = 0x4D435654,
        TOKEN_MHDR = 0x4D484452,
        TOKEN_MH2O = 0x4D48324F,
        TOKEN_MTXF = 0x4D545846,
        TOKEN_MCIN = 0x4D43494E,
        TOKEN_MTEX = 0x4D544558,
        TOKEN_MMDX = 0x4D4D4458,
        TOKEN_MMID = 0x4D4D4944,
        TOKEN_MWID = 0x4D574944,
        TOKEN_MDDF = 0x4D444446,
        TOKEN_MFBO = 0x4D46424F,
        TOKEN_MVER = 0x4D564552,
        TOKEN_MPHD = 0x4D504844,
        TOKEN_MAIN = 0x4D41494E,
        TOKEN_MODF = 0x4D4F4446,
        TOKEN_MWMO = 0x4D574D4F,
    };

    /* 0000 */ uint32_t m_token;
    /* 0004 */ uint32_t m_size;
    /* 0008 */ void* m_data;
};

struct SMLayer {

    enum LAYER_FLAGS : int32_t {
        NONE = 0,
        ANIMATION_DIRECTION_MASK = 7,
        ANIMATION_SPEED_INDEX_MASK = 7,
        ANIMATED = 0x40,
        OVERBRIGHT = 0x80,
        USE_ALPHA_MAP = 0x100,
        ALPHA_MAP_COMPRESSED = 0x200,
        USE_CUBE_MAP_REFLECTION = 0x400,
        ANIMATED_OR_OVERBRIGHT = 0xC0,
        BIG_ALPHA_MERGE_BLOCK_MASK = 0x4C0,
    };

    enum LAYER_BITS : int32_t {
        ANIMATION_SPEED_SHIFT = 3,
    };

    /* 0000 */ uint32_t m_textureId;
    /* 0004 */ uint32_t m_flags;
    /* 0008 */ uint32_t m_offsetInMCAL;
    /* 000C */ uint32_t m_offectId;
};

struct SMOPoly {

    enum FLAGS : uint8_t {
        F_TRANSITION = 0x01,
        F_NOCAMCOLLIDE = 0x02,
        F_DETAIL = 0x04,
        F_COLLISION = 0x08,
        F_HINT = 0x10,
        F_RENDER = 0x20,
        UNKNOWN_0x40 = 0x40,
        F_COLLIDE_HIT = 0x80,
    };

    enum IGNORE_FLAGS : uint16_t {
        IGNORE_BLENDED = 0x100,
        IGNORE_OPAQUE = 0x200,
    };

    /* 0000 */ uint8_t m_flags;
    /* 0001 */ uint8_t m_materialId;
};

struct SMOPortalRef {
    /* 0000 */ uint16_t m_portalIndex;
    /* 0002 */ uint16_t m_groupIndex;
    /* 0004 */ int16_t m_side;
    /* 0006 */ uint16_t m_filler;
};

typedef float DepthCoordTable[256];

#endif
