#ifndef WOW_SOURCE_DB_REC_RANDPROPPOINTSREC_H
#define WOW_SOURCE_DB_REC_RANDPROPPOINTSREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_RAND_PROP_POINTS = 5;

class RandPropPointsRec {
    public:
    static const int32_t s_numColumns = 16;
    static const int32_t s_rowSize = 64;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_epic [NUM_RAND_PROP_POINTS];
    /* 0018 */ int32_t m_superior [NUM_RAND_PROP_POINTS];
    /* 002C */ int32_t m_good [NUM_RAND_PROP_POINTS];

    RandPropPointsRec();
    ~RandPropPointsRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
