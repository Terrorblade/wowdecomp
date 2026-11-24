#ifndef WOW_SOURCE_DB_REC_ITEMDISPLAYINFOREC_H
#define WOW_SOURCE_DB_REC_ITEMDISPLAYINFOREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

class ItemDisplayInfoRec {
    public:

    enum GEOSET_GROUP : int32_t {
        PRIMARY = 0,
        SECONDARY = 1,
        ROBE = 2,
        COUNT = 3,
    };

    enum DISPLAY_FLAGS : int32_t {
        GUILD_TABARD = 0x1,
        HIDE_PANTS_AND_BELT = 0x4,
    };

    enum DISPLAY_BITS : int32_t {
        HIDE_PANTS_AND_BELT_BIT_INDEX = 2,
    };

    static const int32_t s_numColumns = 25;
    static const int32_t s_rowSize = 100;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ const char* m_modelName [2];
    /* 000C */ const char* m_modelTexture [2];
    /* 0014 */ const char* m_inventoryIcon [2];
    /* 001C */ int32_t m_geosetGroup [COUNT];
    /* 0028 */ int32_t m_flags;
    /* 002C */ int32_t m_spellVisualID;
    /* 0030 */ int32_t m_groupSoundIndex;
    /* 0034 */ int32_t m_helmetGeosetVisID [2];
    /* 003C */ const char* m_texture [8];
    /* 005C */ int32_t m_itemVisual;
    /* 0060 */ int32_t m_particleColorID;

    ItemDisplayInfoRec();
    ~ItemDisplayInfoRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
