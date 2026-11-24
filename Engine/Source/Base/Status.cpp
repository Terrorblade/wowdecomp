#include <cstdarg>
#include <cstdint>
#include <cstring>
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Storm/H/storm.h"

// STATIC: 0x00AC0CE4
static CStatus s_errorList;

static const uint32_t MAX_GLOBAL_ENTRIES = 256;

// Nothing ever clears the global list, so it skips repeats and keeps only the newest entries instead of growing all session.
static bool SkipGlobalEntry(CStatus* status, const char* text) {
    if (status != &s_errorList) {
        return false;
    }

    uint32_t count = 0;

    for (CStatus::STATUSENTRY* entry = status->m_statusList.Head(); entry; entry = status->m_statusList.Next(entry)) {
        if (entry->m_text && !SStrCmp(entry->m_text, text, STORM_MAX_STR)) {
            return true;
        }

        count++;
    }

    while (count >= MAX_GLOBAL_ENTRIES) {
        status->m_statusList.DeleteNode(status->m_statusList.Head());
        count--;
    }

    return false;
}

// INLINED
CStatus::STATUSENTRY::~STATUSENTRY() {
    if (this->m_text) {
        SMemFree(this->m_text, __FILE__, __LINE__, 0);
    }
}

// INLINED
CStatus::CStatus() {
    this->m_highestSeverity = STATUS_INFO;
}

// INLINED
void CStatus::Clear() {
    this->m_statusList.Clear();
    this->m_highestSeverity = STATUS_INFO;
}

// VERIFY: d83a4f5e auto
// OFFSET: 0x0047C090
CStatus& GetGlobalStatusObj() {
    return s_errorList;
}

// VERIFY: 8cc1593d manual
// OFFSET: 0x0047C0A0
void CStatus::Add(const CStatus& source) {
    CStatus& src = const_cast<CStatus&>(source);

    for (STATUSENTRY* entry = src.m_statusList.Head(); entry; entry = src.m_statusList.Next(entry)) {
        this->Add(entry->m_severity, entry->m_text);
    }
}

// VERIFY: d9203a7f manual
// OFFSET: 0x0047C0F0
int32_t CStatus::IsEmpty() {
    return this->m_statusList.Head() == nullptr;
}

// VERIFY: d68224ab manual
// OFFSET: 0x0047C110
char* CStatus::GetErrorStr(char* buffer, size_t bufchars, STATUS_TYPE minSeverity, const char* separator) {
    char* curr = buffer;
    *curr = 0;

    size_t separatorchars = separator && *separator ? SStrLen(separator) : 0;

    for (STATUSENTRY* entry = this->m_statusList.Head(); entry; entry = this->m_statusList.Next(entry)) {
        if (entry->m_severity < minSeverity) {
            continue;
        }

        size_t textchars = SStrLen(entry->m_text);

        if (textchars + separatorchars >= bufchars) {
            return buffer;
        }

        SStrCopy(curr, entry->m_text, bufchars);
        curr += textchars;
        bufchars -= textchars;

        if (separatorchars) {
            SStrPack(curr, separator, bufchars);
            bufchars -= separatorchars;
            curr += separatorchars;
        }
    }

    return buffer;
}

// VERIFY: 2dc1dd20 manual
// OFFSET: 0x0047C1D0
uint32_t CStatus::GetErrorStrLen(STATUS_TYPE minSeverity, const char* separator) {
    size_t separatorchars = 0;
    uint32_t length = 0;

    if (separator && *separator) {
        separatorchars = SStrLen(separator);
    }

    for (STATUSENTRY* entry = this->m_statusList.Head(); entry; entry = this->m_statusList.Next(entry)) {
        if (entry->m_severity >= minSeverity) {
            length += static_cast<uint32_t>(SStrLen(entry->m_text) + separatorchars);
        }
    }

    return length;
}

// VERIFY: ccc312bc manual
// OFFSET: 0x0047C240
char* CStatus::GetErrorStrAlloc(STATUS_TYPE minSeverity, const char* separator) {
    size_t bufchars = this->GetErrorStrLen(minSeverity, separator) + 1;
    char* buffer = static_cast<char*>(SMemAlloc(bufchars, __FILE__, __LINE__, 0));

    if (buffer) {
        this->GetErrorStr(buffer, bufchars, minSeverity, separator);
    }

    return buffer;
}

// VERIFY: c97894d9 manual
// OFFSET: 0x0047C290
void CStatus::Prepend(STATUS_TYPE severity, const char* format, ...) {
    char buffer[256];
    memset(buffer, 0, sizeof(buffer));

    if (!format) {
        DebugValidate("format");
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    va_list args;
    va_start(args, format);
    SStrVPrintf(buffer, sizeof(buffer), format, args);

    if (!buffer[0] || SkipGlobalEntry(this, buffer)) {
        return;
    }

    STATUSENTRY* entry = this->m_statusList.NewNode(TSLIST_LINK_AFTER, 0, 0);
    entry->m_text = SStrDupA(buffer, __FILE__, __LINE__);
    entry->m_severity = severity;

    if (severity > this->m_highestSeverity) {
        this->m_highestSeverity = severity;
    }
}

// VERIFY: 29d4c2bf manual
// OFFSET: 0x0047C360
void CStatus::Add(STATUS_TYPE severity, const char* format, ...) {
    char buffer[256];
    memset(buffer, 0, sizeof(buffer));

    if (!format) {
        DebugValidate("format");
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    va_list args;
    va_start(args, format);
    SStrVPrintf(buffer, sizeof(buffer), format, args);

    if (!buffer[0] || SkipGlobalEntry(this, buffer)) {
        return;
    }

    STATUSENTRY* entry = this->m_statusList.NewNode(TSLIST_LINK_BEFORE, 0, 0);
    entry->m_text = SStrDupA(buffer, __FILE__, __LINE__);
    entry->m_severity = severity;

    if (severity > this->m_highestSeverity) {
        this->m_highestSeverity = severity;
    }
}

// VERIFY: 25d08b32 manual
// OFFSET: [0x0047C500, 0x0055F830]
CStatus::~CStatus() {
    this->Clear();
}

void CStatus::Display() {
}
