#ifndef WOW_SOURCE_DB_REC_AREAPOIREC_H
#define WOW_SOURCE_DB_REC_AREAPOIREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_AREA_POI_ICONS = 9;

enum AREA_POI_FLAGS : int32_t {
    AREA_POI_FLAG_NONE                      = 0x00000000,
    AREA_POI_FLAG_SHOW_ON_MINIMAP           = 0x00000001,
    AREA_POI_FLAG_SHOW_MINIMAP_ICON         = 0x00000002,
    AREA_POI_FLAG_WORLDMAP_ZONE_ZOOM        = 0x00000004,
    AREA_POI_FLAG_WORLDMAP_CONTINENT_ZOOM   = 0x00000008,
    AREA_POI_FLAG_WORLDMAP_WORLD_ZOOM       = 0x00000010,
    AREA_POI_FLAG_ALWAYS_DRAW_ICON_WORLDMAP = 0x00000080,
    AREA_POI_FLAG_SHOW_WHEN_INDOORS         = 0x00000100,
    AREA_POI_FLAG_SHOW_IN_BATTLEMAP         = 0x00000200,
    AREA_POI_FLAG_ONLY_SHOW_IN_CURRENT_AREA = 0x00000400,
};

class AreaPOIRec {
    public:
    static const int32_t s_numColumns = 54;
    static const int32_t s_rowSize = 216;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_importance;
    /* 0008 */ int32_t m_icon [NUM_AREA_POI_ICONS];
    /* 002C */ int32_t m_factionID;
    /* 0030 */ float m_pos [3];
    /* 003C */ int32_t m_continentID;
    /* 0040 */ int32_t m_flags;
    /* 0044 */ int32_t m_areaID;
    /* 0048 */ const char* m_name;
    /* 004C */ const char* m_description;
    /* 0050 */ int32_t m_worldStateID;
    /* 0054 */ int32_t m_worldMapLink;

    AreaPOIRec();
    ~AreaPOIRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
