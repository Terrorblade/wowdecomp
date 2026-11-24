#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include "WoW/Common/WowTime.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Storm/H/storm.h"

// STATIC: 0x00ADC064
static const char* s_weekDayNames[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

// STATIC: 0x00CAC11C
static int32_t s_localTimeOffset;

// VERIFY: c0d1d008 auto
// OFFSET: 0x0076C1B0
static void LocalTimeSafe(tm* out, const time_t* t) {
    tm* local = localtime(t);

    if (local) {
        *out = *local;
    } else {
        out->tm_sec = 0;
        out->tm_min = 0;
        out->tm_hour = 0;
        out->tm_mday = 0;
        out->tm_mon = 0;
        out->tm_year = 0;
        out->tm_wday = 0;
        out->tm_yday = 0;
        out->tm_isdst = 0;
    }
}

// VERIFY: e059d8fe auto
// OFFSET: 0x0076C190
WowTime::WowTime() {
    this->m_minute = -1;
    this->m_hour = -1;
    this->m_weekDay = -1;
    this->m_monthDay = -1;
    this->m_month = -1;
    this->m_year = -1;
    this->m_flags = 0;
    this->m_tzHours = 0;
}

// VERIFY: 96d6691c manual
// OFFSET: 0x0076C1F0
int32_t WowTime::GetDaysSinceEpoch() const {
    if (this->m_year < 0 || this->m_month < 0 || this->m_monthDay < 0) {
        return 0;
    }

    tm date = {};
    date.tm_mon = this->m_month;
    date.tm_mday = this->m_monthDay + 1;
    date.tm_isdst = -1;
    date.tm_year = this->m_year % 100 + 100;

    return static_cast<int32_t>(mktime(&date)) / 86400;
}

// VERIFY: 4181298f manual
// OFFSET: 0x0076C280
void WowTime::AddDays(int32_t days, bool keepTime) {
    if (this->m_year < 0 || this->m_month < 0 || this->m_monthDay < 0) {
        return;
    }

    tm date = {};
    date.tm_year = this->m_year + 100;
    date.tm_mon = this->m_month;
    date.tm_mday = this->m_monthDay + 1;
    date.tm_isdst = -1;

    if (keepTime) {
        date.tm_hour = this->m_hour;
        date.tm_min = this->m_minute;
    }

    time_t t = mktime(&date) + days * 86400;

    if (!keepTime) {
        t += 3600;
    }

    LocalTimeSafe(&date, &t);

    this->m_year = date.tm_year - 100;
    this->m_month = date.tm_mon;
    this->m_monthDay = date.tm_mday - 1;
    this->m_weekDay = date.tm_wday;

    if (keepTime) {
        this->m_hour = date.tm_hour;
        this->m_minute = date.tm_min;
    }
}

// VERIFY: 9ccf10fd auto
// OFFSET: 0x0076C360
int32_t WowTime::GetHourAndMinutes() const {
    if (this->m_hour < 0 || this->m_minute < 0) {
        return 0;
    }

    return this->m_minute + 60 * this->m_hour;
}

// VERIFY: 0e0af546 manual
// OFFSET: 0x0076C380
void WowTime::SetHourAndMinutes(int32_t minutes) {
    this->m_minute = minutes % 60;
    this->m_hour = minutes / 60;
}

// VERIFY: ecc484b4 auto
// OFFSET: 0x0076C3C0
bool WowTime::SetHourAndMinutes(uint32_t hour, uint32_t minute) {
    if (hour >= 24 || minute >= 60) {
        return false;
    }

    this->m_hour = hour;
    this->m_minute = minute;

    return true;
}

// VERIFY: 3db571bf manual
// OFFSET: 0x0076C3F0
void WowTime::AddMinutes(int32_t minutes) {
    int32_t total = this->GetHourAndMinutes() + minutes;
    int32_t days = total / 1440;

    if (days) {
        total -= 1440 * days;
        this->AddDays(days, false);
    }

    if (total < 0) {
        this->AddDays(-1, false);
        total += 1440;
    }

    this->SetHourAndMinutes(total);
}

// VERIFY: 6cebf721 auto
// OFFSET: 0x0076C480
bool WowTime::SetDate(uint32_t month, uint32_t monthDay, uint32_t year) {
    if (month >= 12 || monthDay >= 32) {
        return false;
    }

    if (year >= 2000) {
        year -= 2000;
    }

    if (year > 31) {
        return false;
    }

    this->m_month = month;
    this->m_monthDay = monthDay;
    this->m_year = year;

    return true;
}

// VERIFY: 589d1e7b manual
// OFFSET: 0x0076C4C0
void WowTime::AddHolidayDuration(int32_t minutes) {
    WowTime start = *this;

    if (minutes / 1440 > 0) {
        this->AddDays(minutes / 1440, true);
    }

    this->AddMinutes(minutes % 1440);

    int32_t expected = (start.m_minute + minutes % 1440 + 60 * start.m_hour) % 1440;

    if (expected == this->GetHourAndMinutes()) {
        return;
    }

    if ((this->GetHourAndMinutes() - expected + 1440) % 1440 == 60) {
        if (this->GetHourAndMinutes() < 60) {
            this->AddDays(-1, false);
        }

        this->SetHourAndMinutes(expected);
    } else {
        this->AddMinutes(60);
    }
}

// VERIFY: 0959c484 auto
// OFFSET: 0x0076C5E0
int32_t WowTime::CompareMonth(const WowTime& time) const {
    if (this->m_month > time.m_month) {
        return 1;
    }

    return this->m_month < time.m_month ? -1 : 0;
}

// VERIFY: 9ded7e66 auto
// OFFSET: 0x0076C610
int32_t WowTime::CompareMonthDay(const WowTime& time) const {
    if (this->m_monthDay > time.m_monthDay) {
        return 1;
    }

    return this->m_monthDay < time.m_monthDay ? -1 : 0;
}

// VERIFY: f1d49521 auto
// OFFSET: 0x0076C640
int32_t WowTime::CompareWeekDay(const WowTime& time) const {
    if (this->m_weekDay > time.m_weekDay) {
        return 1;
    }

    return this->m_weekDay < time.m_weekDay ? -1 : 0;
}

// VERIFY: de8a3c40 auto
// OFFSET: 0x0076C670
int32_t WowTime::CompareHour(const WowTime& time) const {
    if (this->m_hour > time.m_hour) {
        return 1;
    }

    return this->m_hour < time.m_hour ? -1 : 0;
}

// VERIFY: ab318632 auto
// OFFSET: 0x0076C6A0
int32_t WowTime::CompareMinute(const WowTime& time) const {
    if (this->m_minute > time.m_minute) {
        return 1;
    }

    return this->m_minute < time.m_minute ? -1 : 0;
}

// VERIFY: 67b7820e manual
// OFFSET: 0x0076C6D0
bool WowTime::operator<(const WowTime& time) const {
    if (&time == this) {
        return false;
    }

    int32_t result;

    if (time.m_year >= 0 && this->m_year >= 0) {
        if (this->m_year > time.m_year) {
            return false;
        }

        if (this->m_year < time.m_year) {
            return true;
        }
    }

    if (time.m_month >= 0 && this->m_month >= 0 && (result = this->CompareMonth(time)) != 0) {
        return result < 0;
    }

    if (time.m_monthDay >= 0 && this->m_monthDay >= 0 && (result = this->CompareMonthDay(time)) != 0) {
        return result < 0;
    }

    if (time.m_weekDay >= 0 && this->m_weekDay >= 0 && (result = this->CompareWeekDay(time)) != 0) {
        return result < 0;
    }

    if (time.m_hour >= 0 && this->m_hour >= 0 && (result = this->CompareHour(time)) != 0) {
        return result < 0;
    }

    return time.m_minute >= 0 && this->m_minute >= 0 && (result = this->CompareMinute(time)) != 0 && result < 0;
}

// VERIFY: b34e86a7 manual
// OFFSET: 0x0076C7B0
bool WowTime::operator>(const WowTime& time) const {
    if (&time == this) {
        return false;
    }

    int32_t result;

    if (time.m_year >= 0 && this->m_year >= 0) {
        if (this->m_year > time.m_year) {
            return true;
        }

        if (this->m_year < time.m_year) {
            return false;
        }
    }

    if (time.m_month >= 0 && this->m_month >= 0 && (result = this->CompareMonth(time)) != 0) {
        return result > 0;
    }

    if (time.m_monthDay >= 0 && this->m_monthDay >= 0 && (result = this->CompareMonthDay(time)) != 0) {
        return result > 0;
    }

    if (time.m_weekDay >= 0 && this->m_weekDay >= 0 && (result = this->CompareWeekDay(time)) != 0) {
        return result > 0;
    }

    if (time.m_hour >= 0 && this->m_hour >= 0 && (result = this->CompareHour(time)) != 0) {
        return result > 0;
    }

    return time.m_minute >= 0 && this->m_minute >= 0 && (result = this->CompareMinute(time)) != 0 && result > 0;
}

// VERIFY: 5ebd3f55 manual
// OFFSET: 0x0076C890
bool WowTime::operator==(const WowTime& time) const {
    if (time.m_year >= 0 && this->m_year >= 0 && time.m_year != this->m_year) {
        return false;
    }

    if (time.m_month >= 0 && this->m_month >= 0 && time.m_month != this->m_month) {
        return false;
    }

    if (time.m_monthDay >= 0 && this->m_monthDay >= 0 && time.m_monthDay != this->m_monthDay) {
        return false;
    }

    if (time.m_weekDay >= 0 && this->m_weekDay >= 0 && time.m_weekDay != this->m_weekDay) {
        return false;
    }

    if (time.m_hour >= 0 && this->m_hour >= 0 && time.m_hour != this->m_hour) {
        return false;
    }

    if (time.m_minute >= 0 && this->m_minute >= 0 && time.m_minute != this->m_minute) {
        return false;
    }

    return true;
}

// VERIFY: 464da28b auto
// OFFSET: 0x0076C910
void WowTime::WowEncodeTime(uint32_t& value, int32_t minute, int32_t hour, int32_t weekDay, int32_t monthDay, int32_t month, int32_t year, int32_t flags) {
    value = minute & 0x3F | ((hour & 0x1F | ((weekDay & 7 | ((monthDay & 0x3F | ((month & 0xF | ((year & 0x1F | ((flags & 3) << 5)) << 4)) << 6)) << 3)) << 5)) << 6);
}

// VERIFY: d693ecf3 auto
// OFFSET: 0x0076C970
void WowTime::WowDecodeTime(uint32_t value, int32_t* minute, int32_t* hour, int32_t* weekDay, int32_t* monthDay, int32_t* month, int32_t* year, int32_t* flags) {
    if (minute) {
        int32_t field = value & PACKED_MINUTE_MASK;
        *minute = field == PACKED_MINUTE_MASK ? -1 : field;
    }

    if (hour) {
        int32_t field = (value >> PACKED_HOUR_SHIFT) & PACKED_HOUR_MASK;
        *hour = field == PACKED_HOUR_MASK ? -1 : field;
    }

    if (weekDay) {
        int32_t field = (value >> PACKED_WEEKDAY_SHIFT) & PACKED_WEEKDAY_MASK;
        *weekDay = field == PACKED_WEEKDAY_MASK ? -1 : field;
    }

    if (monthDay) {
        int32_t field = (value >> PACKED_MONTHDAY_SHIFT) & PACKED_MONTHDAY_MASK;
        *monthDay = field == PACKED_MONTHDAY_MASK ? -1 : field;
    }

    if (month) {
        int32_t field = (value >> PACKED_MONTH_SHIFT) & PACKED_MONTH_MASK;
        *month = field == PACKED_MONTH_MASK ? -1 : field;
    }

    if (year) {
        int32_t field = (value >> PACKED_YEAR_SHIFT) & PACKED_YEAR_MASK;
        *year = field == PACKED_YEAR_MASK ? -1 : field;
    }

    if (flags) {
        int32_t field = (value >> PACKED_FLAGS_SHIFT) & PACKED_FLAGS_MASK;
        *flags = field == PACKED_FLAGS_MASK ? -1 : field;
    }
}

// VERIFY: 0e001625 auto
// OFFSET: 0x0076CA50
void WowTime::WowEncodeTime(uint32_t& value, const WowTime* time) {
    value = time->m_minute & 0x3F | ((time->m_hour & 0x1F | ((time->m_weekDay & 7 | ((time->m_monthDay & 0x3F | ((time->m_month & 0xF | ((time->m_year & 0x1F | ((time->m_flags & 3) << 5)) << 4)) << 6)) << 3)) << 5)) << 6);
}

// VERIFY: 847111ea auto
// OFFSET: 0x0076CAB0
void WowTime::WowDecodeTime(uint32_t value, WowTime* time) {
    WowTime::WowDecodeTime(value, &time->m_minute, &time->m_hour, &time->m_weekDay, &time->m_monthDay, &time->m_month, &time->m_year, &time->m_flags);
}

// VERIFY: dfb4e79d auto
// OFFSET: 0x0076CAE0
void WowTime::WowDecodeTimeToDbDate(uint32_t value, DBDATESTRUCT* date) {
    int32_t minute = -1;
    int32_t hour = -1;
    int32_t weekDay;
    int32_t monthDay = -1;
    int32_t month = -1;
    int32_t year = -1;
    int32_t flags;

    WowTime::WowDecodeTime(value, &minute, &hour, &weekDay, &monthDay, &month, &year, &flags);

    if (date) {
        date->m_month = month + 1;
        date->m_day = monthDay + 1;
        date->m_year = year + 2000;
        date->m_hour = hour;
        date->m_minute = minute;
        date->m_second = 0;
    }
}

// VERIFY: 6da3090e auto
// OFFSET: 0x0076CB60
CDataStore& operator>>(CDataStore& msg, WowTime& time) {
    uint32_t value;
    msg.Get(value);
    WowTime::WowDecodeTime(value, &time.m_minute, &time.m_hour, &time.m_weekDay, &time.m_monthDay, &time.m_month, &time.m_year, &time.m_flags);

    return msg;
}

// VERIFY: 745bb1fd auto
// OFFSET: 0x0076CBA0
void WowTime::ConvertToLocal(WowTime& time) const {
    if (!this->m_tzHours) {
        return;
    }

    if (!s_localTimeOffset) {
        time_t now;
        ::time(&now);
        s_localTimeOffset = static_cast<int32_t>(now + 3600 * this->m_tzHours - mktime(gmtime(&now)));

        if (!s_localTimeOffset) {
            return;
        }
    }

    tm date = {};
    date.tm_mday = time.m_monthDay + 1;
    date.tm_year = time.m_year + 100;
    date.tm_mon = time.m_month;
    date.tm_isdst = -1;
    date.tm_hour = time.m_hour;
    date.tm_min = time.m_minute;

    time_t local = s_localTimeOffset + mktime(&date);
    tm* converted = localtime(&local);

    if (converted) {
        date = *converted;
    } else {
        date.tm_sec = 0;
        date.tm_min = 0;
        date.tm_hour = 0;
        date.tm_mday = 0;
        date.tm_mon = 0;
        date.tm_year = 0;
        date.tm_wday = 0;
        date.tm_yday = 0;
        date.tm_isdst = 0;
    }

    time.m_year = date.tm_year - 100;
    time.m_month = date.tm_mon;
    time.m_monthDay = date.tm_mday - 1;
    time.m_hour = date.tm_hour;
    time.m_minute = date.tm_min;
    time.m_weekDay = -1;
}

// VERIFY: d5134eab auto
// OFFSET: 0x0076CCC0
bool WowTime::operator<=(const WowTime& time) const {
    return *this == time || *this < time;
}

// VERIFY: 66ce3e06 auto
// OFFSET: 0x0076CD00
bool WowTime::operator>=(const WowTime& time) const {
    return *this == time || *this > time;
}

// VERIFY: a61e8a57 auto
// OFFSET: 0x0076CD40
char* WowTime::WowGetTimeString(WowTime* time, char* buffer, int32_t size) {
    uint32_t value;
    WowTime::WowEncodeTime(value, time);

    if (!value) {
        SStrPrintf(buffer, size, "Not Set");
        return buffer;
    }

    char year[8];
    char month[8];
    char monthDay[8];
    char weekDay[8];
    char hour[8];
    char minute[8];

    if (time->m_year >= 0) {
        SStrPrintf(year, sizeof(year), "%i", time->m_year + 2000);
    } else {
        SStrPrintf(year, sizeof(year), "A");
    }

    if (time->m_month >= 0) {
        SStrPrintf(month, sizeof(month), "%i", time->m_month + 1);
    } else {
        SStrPrintf(month, sizeof(month), "A");
    }

    if (time->m_monthDay >= 0) {
        SStrPrintf(monthDay, sizeof(monthDay), "%i", time->m_monthDay + 1);
    } else {
        SStrPrintf(monthDay, sizeof(monthDay), "A");
    }

    if (time->m_weekDay >= 0) {
        SStrPrintf(weekDay, sizeof(weekDay), s_weekDayNames[time->m_weekDay]);
    } else {
        SStrPrintf(weekDay, sizeof(weekDay), "Any");
    }

    if (time->m_hour >= 0) {
        SStrPrintf(hour, sizeof(hour), "%i", time->m_hour);
    } else {
        SStrPrintf(hour, sizeof(hour), "A");
    }

    if (time->m_minute >= 0) {
        SStrPrintf(minute, sizeof(minute), "%2.2i", time->m_minute);
    } else {
        SStrPrintf(minute, sizeof(minute), "A");
    }

    SStrPrintf(buffer, size, "%s/%s/%s (%s) %s:%s", month, monthDay, year, weekDay, hour, minute);

    return buffer;
}

// VERIFY: 8917863a manual
// OFFSET: 0x0076CF10
bool WowTime::InRange(const WowTime& start, const WowTime& end) const {
    if (start == end || start < end) {
        return (*this == start || *this > start) && *this < end;
    }

    return *this == start || *this > start || *this < end;
}
