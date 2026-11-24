#ifndef WOW_SOURCE_DB_REC_LIGHTREC_H
#define WOW_SOURCE_DB_REC_LIGHTREC_H

#include "Engine/Source/Tempest/CRange.h"

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_LIGHT_PARAMS = 8;

enum LIGHT_ID : int32_t {
    LIGHT_ID_DEFAULT = 1,
};

class LightRec {
    public:
    static const int32_t s_numColumns = 15;
    static const int32_t s_rowSize = 60;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_continentID;
    /* 0008 */ float m_gameCoords [3];
    /* 0014 */ float m_gameFalloffStart;
    /* 0018 */ float m_gameFalloffEnd;
    /* 001C */ int32_t m_lightParamsID [NUM_LIGHT_PARAMS];

    LightRec();
    ~LightRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
