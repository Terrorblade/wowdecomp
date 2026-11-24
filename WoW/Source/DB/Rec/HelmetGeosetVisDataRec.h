#ifndef WOW_SOURCE_DB_REC_HELMETGEOSETVISDATAREC_H
#define WOW_SOURCE_DB_REC_HELMETGEOSETVISDATAREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_HELMET_HIDE_GEOSETS = 7;

class HelmetGeosetVisDataRec {
    public:
    static const int32_t s_numColumns = 8;
    static const int32_t s_rowSize = 32;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_hideGeoset [NUM_HELMET_HIDE_GEOSETS];

    HelmetGeosetVisDataRec();
    ~HelmetGeosetVisDataRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
