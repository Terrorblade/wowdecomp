#ifndef WOW_SOURCE_DB_REC_BARBERSHOPSTYLEREC_H
#define WOW_SOURCE_DB_REC_BARBERSHOPSTYLEREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum BARBER_SHOP_STYLE_TYPE : int32_t {
    BARBER_SHOP_HAIR_STYLE  = 0,
    BARBER_SHOP_HAIR_COLOR  = 1,
    BARBER_SHOP_FACIAL_HAIR = 2,
    BARBER_SHOP_SKIN_COLOR  = 3,
    NUM_BARBER_SHOP_STYLE_TYPES = 4,
};

class BarberShopStyleRec {
    public:
    static const int32_t s_numColumns = 40;
    static const int32_t s_rowSize = 160;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_type;
    /* 0008 */ const char* m_displayName;
    /* 000C */ const char* m_description;
    /* 0010 */ float m_costModifier;
    /* 0014 */ int32_t m_race;
    /* 0018 */ int32_t m_sex;
    /* 001C */ int32_t m_data;

    BarberShopStyleRec();
    ~BarberShopStyleRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
