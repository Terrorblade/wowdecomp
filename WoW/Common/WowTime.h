#ifndef WOW_COMMON_WOWTIME_H
#define WOW_COMMON_WOWTIME_H

#include <cstddef>
#include <cstdint>

class CDataStore;

struct DBDATESTRUCT {
    /* 0000 */ uint16_t m_year;
    /* 0002 */ uint8_t m_month;
    /* 0003 */ uint8_t m_day;
    /* 0004 */ uint8_t m_hour;
    /* 0005 */ uint8_t m_minute;
    /* 0006 */ uint8_t m_second;
};

class WowTime {
    public:
    static void WowDecodeTime(uint32_t value, int32_t* minute, int32_t* hour, int32_t* weekDay, int32_t* monthDay, int32_t* month, int32_t* year, int32_t* flags);
    static void WowDecodeTime(uint32_t value, WowTime* time);
    static void WowDecodeTimeToDbDate(uint32_t value, DBDATESTRUCT* date);
    static void WowEncodeTime(uint32_t& value, const WowTime* time);
    static void WowEncodeTime(uint32_t& value, int32_t minute, int32_t hour, int32_t weekDay, int32_t monthDay, int32_t month, int32_t year, int32_t flags);
    static char* WowGetTimeString(WowTime* time, char* buffer, int32_t size);

    /* 0000 */ int32_t m_minute;
    /* 0004 */ int32_t m_hour;
    /* 0008 */ int32_t m_weekDay;
    /* 000C */ int32_t m_monthDay;
    /* 0010 */ int32_t m_month;
    /* 0014 */ int32_t m_year;
    /* 0018 */ int32_t m_flags;
    /* 001C */ int32_t m_tzHours;

    WowTime();
    void AddDays(int32_t days, bool keepTime);
    void AddHolidayDuration(int32_t minutes);
    void AddMinutes(int32_t minutes);
    int32_t CompareHour(const WowTime& time) const;
    int32_t CompareMinute(const WowTime& time) const;
    int32_t CompareMonth(const WowTime& time) const;
    int32_t CompareMonthDay(const WowTime& time) const;
    int32_t CompareWeekDay(const WowTime& time) const;
    void ConvertToLocal(WowTime& time) const;
    int32_t GetDaysSinceEpoch() const;
    int32_t GetHourAndMinutes() const;
    bool InRange(const WowTime& start, const WowTime& end) const;
    bool SetDate(uint32_t month, uint32_t monthDay, uint32_t year);
    void SetHourAndMinutes(int32_t minutes);
    bool SetHourAndMinutes(uint32_t hour, uint32_t minute);
    bool operator<(const WowTime& time) const;
    bool operator<=(const WowTime& time) const;
    bool operator==(const WowTime& time) const;
    bool operator>(const WowTime& time) const;
    bool operator>=(const WowTime& time) const;

    enum PACKED_FIELD : int32_t {
        PACKED_MINUTE_SHIFT = 0,
        PACKED_MINUTE_MASK = 0x3F,
        PACKED_HOUR_SHIFT = 6,
        PACKED_HOUR_MASK = 0x1F,
        PACKED_WEEKDAY_SHIFT = 11,
        PACKED_WEEKDAY_MASK = 7,
        PACKED_MONTHDAY_SHIFT = 14,
        PACKED_MONTHDAY_MASK = 0x3F,
        PACKED_MONTH_SHIFT = 20,
        PACKED_MONTH_MASK = 0xF,
        PACKED_YEAR_SHIFT = 24,
        PACKED_YEAR_MASK = 0x1F,
        PACKED_FLAGS_SHIFT = 29,
        PACKED_FLAGS_MASK = 3,
    };

};

CDataStore& operator>>(CDataStore& msg, WowTime& time);

#endif
