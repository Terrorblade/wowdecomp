#ifndef WOW_SOURCE_DB_REC_TERRAINTYPEREC_H
#define WOW_SOURCE_DB_REC_TERRAINTYPEREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum TERRAIN_TYPE_FLAGS : int32_t {
    TERRAIN_TYPE_FLAG_NONE = 0x0,
    TERRAIN_TYPE_FLAG_FOOTPRINTS = 0x1,
};

class TerrainTypeRec {
    public:
    static const int32_t s_numColumns = 6;
    static const int32_t s_rowSize = 24;
    static const bool s_generatedID = true;

    /* 0000 */ int32_t m_terrainID;
    /* 0004 */ const char* m_terrainDesc;
    /* 0008 */ int32_t m_footstepSprayRun;
    /* 000C */ int32_t m_footstepSprayWalk;
    /* 0010 */ int32_t m_soundID;
    /* 0014 */ int32_t m_flags;
    /* 0018 */ int32_t m_generatedID;

    TerrainTypeRec();
    ~TerrainTypeRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
