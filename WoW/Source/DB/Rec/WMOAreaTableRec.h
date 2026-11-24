#ifndef WOW_SOURCE_DB_REC_WMOAREATABLEREC_H
#define WOW_SOURCE_DB_REC_WMOAREATABLEREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum WMO_AREA_FLAGS : int32_t {
    WMO_AREA_FLAG_NONE                           = 0x00000000,
    WMO_AREA_FLAG_RENDER_MINIMAP                 = 0x00000001,
    WMO_AREA_FLAG_FORCE_INDOORS                  = 0x00000002,
    WMO_AREA_FLAG_FORCE_OUTDOORS                 = 0x00000004,
    WMO_AREA_FLAG_GENERATE_SINGLE_EXTERIOR_MAP   = 0x00000008,
    WMO_AREA_FLAG_STORMWIND                      = 0x00000010,
    WMO_AREA_FLAG_CHUNK_USES_TERRAIN_FOR_MINIMAP = 0x00000020,
};

enum WMO_GROUP_ID : int32_t {
    WMO_GROUP_ID_RAVENS_WATCH = 23015,
    WMO_GROUP_ID_EAGLES_EYE = 23016,
};

class WMOAreaTableRec {
    public:
    static const int32_t s_numColumns = 28;
    static const int32_t s_rowSize = 112;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_wmoid;
    /* 0008 */ int32_t m_nameSetID;
    /* 000C */ int32_t m_wmogroupID;
    /* 0010 */ int32_t m_soundProviderPref;
    /* 0014 */ int32_t m_soundProviderPrefUnderwater;
    /* 0018 */ int32_t m_ambienceID;
    /* 001C */ int32_t m_zoneMusic;
    /* 0020 */ int32_t m_introSound;
    /* 0024 */ int32_t m_flags;
    /* 0028 */ int32_t m_areaTableID;
    /* 002C */ const char* m_areaName;

    WMOAreaTableRec();
    ~WMOAreaTableRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
