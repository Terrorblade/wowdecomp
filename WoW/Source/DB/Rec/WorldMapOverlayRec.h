#ifndef WOW_SOURCE_DB_REC_WORLDMAPOVERLAYREC_H
#define WOW_SOURCE_DB_REC_WORLDMAPOVERLAYREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_WORLD_MAP_OVERLAY_AREAS = 4;

class WorldMapOverlayRec {
    public:
    static const int32_t s_numColumns = 17;
    static const int32_t s_rowSize = 68;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_mapAreaID;
    /* 0008 */ int32_t m_areaID [NUM_WORLD_MAP_OVERLAY_AREAS];
    /* 0018 */ int32_t m_mapPointX;
    /* 001C */ int32_t m_mapPointY;
    /* 0020 */ const char* m_textureName;
    /* 0024 */ int32_t m_textureWidth;
    /* 0028 */ int32_t m_textureHeight;
    /* 002C */ int32_t m_offsetX;
    /* 0030 */ int32_t m_offsetY;
    /* 0034 */ int32_t m_hitRectTop;
    /* 0038 */ int32_t m_hitRectLeft;
    /* 003C */ int32_t m_hitRectBottom;
    /* 0040 */ int32_t m_hitRectRight;

    WorldMapOverlayRec();
    ~WorldMapOverlayRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
