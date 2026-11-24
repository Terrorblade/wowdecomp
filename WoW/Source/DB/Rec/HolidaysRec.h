#ifndef WOW_SOURCE_DB_REC_HOLIDAYSREC_H
#define WOW_SOURCE_DB_REC_HOLIDAYSREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum HOLIDAY_CALENDAR_FILTER_TYPE : int32_t {
    HOLIDAY_CALENDAR_FILTER_NONE            = -1,
    HOLIDAY_CALENDAR_FILTER_WEEKLY_HOLIDAYS = 0,
    HOLIDAY_CALENDAR_FILTER_DARKMOON        = 1,
    HOLIDAY_CALENDAR_FILTER_BATTLEGROUNDS   = 2,
};

class HolidaysRec {
    public:
    static const int32_t s_numColumns = 55;
    static const int32_t s_rowSize = 220;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_duration [10];
    /* 002C */ int32_t m_date [26];
    /* 0094 */ int32_t m_region;
    /* 0098 */ int32_t m_looping;
    /* 009C */ int32_t m_calendarFlags [10];
    /* 00C4 */ int32_t m_holidayNameID;
    /* 00C8 */ int32_t m_holidayDescriptionID;
    /* 00CC */ const char* m_textureFileName;
    /* 00D0 */ int32_t m_priority;
    /* 00D4 */ int32_t m_calendarFilterType;
    /* 00D8 */ int32_t m_flags;

    HolidaysRec();
    ~HolidaysRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
