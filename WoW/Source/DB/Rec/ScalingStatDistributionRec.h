#ifndef WOW_SOURCE_DB_REC_SCALINGSTATDISTRIBUTIONREC_H
#define WOW_SOURCE_DB_REC_SCALINGSTATDISTRIBUTIONREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_SCALING_STAT_DISTRIBUTION_STATS = 10;

class ScalingStatDistributionRec {
    public:
    static const int32_t s_numColumns = 22;
    static const int32_t s_rowSize = 88;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_statID [NUM_SCALING_STAT_DISTRIBUTION_STATS];
    /* 002C */ int32_t m_bonus [NUM_SCALING_STAT_DISTRIBUTION_STATS];
    /* 0054 */ int32_t m_maxlevel;

    ScalingStatDistributionRec();
    ~ScalingStatDistributionRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
