#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sys/stat.h>
#include "Engine/Source/Storm/Source/Linux/StormLinux.h"
#include "Engine/Source/Storm/Source/SLog.h"
#include "Engine/Source/Storm/Source/SOption.h"
#include "Engine/Source/Storm/H/storm.h"

static int32_t s_logsysteminit;

static uintptr_t s_sequence;

static char s_defaultdir[MAX_PATH];

static CRITICAL_SECTION s_defaultdir_critsect;

static _LOG* s_loghead[4];

static CRITICAL_SECTION s_critsect[4];

static void FlushLog(_LOG* logptr) {
    if (logptr->bufferused) {
        FILE* file = static_cast<FILE*>(logptr->file);
        fwrite(logptr->buffer, 1, logptr->bufferused, file);
        fflush(file);
        logptr->bufferused = 0;
        logptr->pendpoint = 0;
    }
}

static _LOG* LockLog(HSLOG log, HLOCKEDLOG* lockedhandle, int32_t createifnecessary) {
    if (!log) {
        *lockedhandle = reinterpret_cast<HLOCKEDLOG>(-1);
        return nullptr;
    }

    uintptr_t index = reinterpret_cast<uintptr_t>(log) & 3;
    EnterCriticalSection(&s_critsect[index]);
    *lockedhandle = reinterpret_cast<HLOCKEDLOG>(index);

    _LOG** prevptr = &s_loghead[index];

    for (_LOG* logptr = s_loghead[index]; logptr; logptr = logptr->next) {
        if (logptr->log == log) {
            return logptr;
        }

        prevptr = &logptr->next;
    }

    if (!createifnecessary) {
        LeaveCriticalSection(&s_critsect[index]);
        *lockedhandle = reinterpret_cast<HLOCKEDLOG>(-1);
        return nullptr;
    }

    _LOG* logptr = static_cast<_LOG*>(calloc(1, sizeof(_LOG)));
    *prevptr = logptr;

    if (!logptr) {
        LeaveCriticalSection(&s_critsect[index]);
        *lockedhandle = reinterpret_cast<HLOCKEDLOG>(-1);
        return nullptr;
    }

    logptr->log = log;
    logptr->next = nullptr;
    logptr->filename[0] = 0;
    logptr->file = INVALID_HANDLE_VALUE;
    logptr->bufferused = 0;
    logptr->pendpoint = 0;

    return logptr;
}

static void OutputIndent(_LOG* logptr) {
    int32_t indent = logptr->indent;

    if (indent > 0) {
        if (indent >= 128) {
            indent = 128;
        }

        memset(&logptr->buffer[logptr->bufferused], ' ', indent);
        logptr->buffer[logptr->bufferused + indent] = 0;
        logptr->bufferused += indent;
    }
}

// Lines end in "\n" rather than the Windows "\r\n".
static void OutputReturn(_LOG* logptr) {
    strcpy(&logptr->buffer[logptr->bufferused], "\n");
    logptr->bufferused += 1;
}

static void OutputTime(_LOG* logptr, int32_t show) {
    static uint32_t lasttime;
    static char timestr[64];
    static uint32_t timestrlen;

    if (!logptr->timeStamp) {
        return;
    }

    uint32_t time = StormLinux::GetTickCount();

    if (time != lasttime) {
        lasttime = time;

        timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        tm local;
        localtime_r(&now.tv_sec, &local);
        snprintf(timestr, sizeof(timestr), "%u/%u %02u:%02u:%02u.%03u  ", local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec, static_cast<uint32_t>(now.tv_nsec / 1000000));
        timestrlen = SStrLen(timestr);
    }

    if (show) {
        memcpy(&logptr->buffer[logptr->bufferused], timestr, timestrlen + 1);
    } else {
        memset(&logptr->buffer[logptr->bufferused], ' ', timestrlen);
        logptr->buffer[logptr->bufferused + timestrlen] = 0;
    }

    logptr->bufferused += timestrlen;
}

static void UnlockDeleteLog(_LOG* logptr, HLOCKEDLOG lockedhandle) {
    uintptr_t index = reinterpret_cast<uintptr_t>(lockedhandle);
    _LOG** prevptr = &s_loghead[index];

    for (_LOG* curr = s_loghead[index]; curr; curr = curr->next) {
        if (curr == logptr) {
            *prevptr = curr->next;
            free(curr);
            break;
        }

        prevptr = &curr->next;
    }

    LeaveCriticalSection(&s_critsect[index]);
}

static void UnlockLog(HLOCKEDLOG lockedhandle) {
    LeaveCriticalSection(&s_critsect[reinterpret_cast<uintptr_t>(lockedhandle)]);
}

static const char* PrependDefaultDir(char* newfilename, uint32_t newfilenamesize, const char* filename) {
    if (!filename || !*filename || filename[0] == '/' || filename[1] == ':' || SStrChr(filename, '\\')) {
        return filename;
    }

    EnterCriticalSection(&s_defaultdir_critsect);

    if (s_defaultdir[0]) {
        size_t chars = SStrCopy(newfilename, s_defaultdir, newfilenamesize);
        SStrCopy(&newfilename[chars], filename, newfilenamesize - chars);
    } else {
        StormLinux::GetExePath(newfilename, newfilenamesize);

        char* slash = SStrChrR(newfilename, '/');

        if (slash) {
            *slash = 0;
        }

        SStrPack(newfilename, "/", newfilenamesize);
        SStrPack(newfilename, filename, newfilenamesize);
    }

    LeaveCriticalSection(&s_defaultdir_critsect);

    return newfilename;
}

void STORMAPI SLogClose(HSLOG log) {
    if (!s_logsysteminit) {
        return;
    }

    HLOCKEDLOG lockedhandle;
    _LOG* logptr = LockLog(log, &lockedhandle, 0);

    if (!logptr) {
        return;
    }

    if (logptr->file != INVALID_HANDLE_VALUE) {
        FlushLog(logptr);
        fclose(static_cast<FILE*>(logptr->file));
    }

    UnlockDeleteLog(logptr, lockedhandle);
}

void STORMAPI SLogFlush(HSLOG log) {
    HLOCKEDLOG lockedhandle;
    _LOG* logptr = LockLog(log, &lockedhandle, 0);

    if (logptr) {
        if (logptr->file != INVALID_HANDLE_VALUE) {
            FlushLog(logptr);
        }

        UnlockLog(lockedhandle);
    }
}

void STORMAPI SLogFlushAll() {
    for (int32_t i = 0; i < 4; i++) {
        EnterCriticalSection(&s_critsect[i]);

        for (_LOG* logptr = s_loghead[i]; logptr; logptr = logptr->next) {
            if (logptr->file != INVALID_HANDLE_VALUE) {
                FlushLog(logptr);
            }
        }

        LeaveCriticalSection(&s_critsect[i]);
    }
}

void STORMAPI SLogInitialize() {
    if (s_logsysteminit) {
        return;
    }

    for (int32_t i = 0; i < 4; i++) {
        InitializeCriticalSection(&s_critsect[i]);
    }

    InitializeCriticalSection(&s_defaultdir_critsect);
    s_logsysteminit = 1;
}

// Creates every missing directory leading up to the file in path (a resolved Linux path).
static int32_t CreateFileDirectory(const char* path) {
    if (!path) {
        SErrSetLastError(STORM_WIN_ERROR_INVALID_PARAMETER);
        return 0;
    }

    char buffer[MAX_PATH * 4];
    SStrCopy(buffer, path, sizeof(buffer));

    char* slash = SStrChrR(buffer, '/');

    if (!slash || slash == buffer) {
        return 1;
    }

    *slash = 0;

    for (char* curr = SStrChr(buffer + 1, '/'); curr; curr = SStrChr(curr + 1, '/')) {
        *curr = 0;
        mkdir(buffer, 0755);
        *curr = '/';
    }

    return mkdir(buffer, 0755) == 0;
}

static int32_t OpenLogFile(const char* filename, void** file, uint32_t flags) {
    if (!filename || !*filename) {
        *file = INVALID_HANDLE_VALUE;
        return 0;
    }

    char newfilename[MAX_PATH];
    const char* path = PrependDefaultDir(newfilename, sizeof(newfilename), filename);
    int32_t append = (flags & SLOG_FLAG_APPEND) != 0;

    char resolved[MAX_PATH * 4];

    if (!StormLinux::ResolvePath(path, resolved, sizeof(resolved), true)) {
        *file = INVALID_HANDLE_VALUE;
        return 0;
    }

    CreateFileDirectory(resolved);

    FILE* stream = fopen(resolved, append ? "ab" : "wb");
    *file = stream ? static_cast<void*>(stream) : INVALID_HANDLE_VALUE;

    return stream != nullptr;
}

int32_t STORMAPI SLogCreate(const char* filename, uint32_t flags, HSLOG* log) {
    if (!filename || !*filename || !log) {
        SErrSetLastError(STORM_WIN_ERROR_INVALID_PARAMETER);
        return 0;
    }

    *log = nullptr;

    if (flags & SLOG_FLAG_NOFILE) {
        filename = "";
        flags &= ~SLOG_FLAG_OPENFILE;
    }

    void* file = INVALID_HANDLE_VALUE;

    if ((flags & SLOG_FLAG_OPENFILE) && !OpenLogFile(filename, &file, flags)) {
        return 0;
    }

    *log = reinterpret_cast<HSLOG>(++s_sequence);

    HLOCKEDLOG lockedhandle;
    _LOG* logptr = LockLog(*log, &lockedhandle, 1);

    if (!logptr) {
        *log = nullptr;
        return 0;
    }

    logptr->file = file;
    SStrCopy(logptr->filename, filename, sizeof(logptr->filename));
    logptr->flags = flags;
    logptr->timeStamp = 1;
    logptr->indent = 0;
    UnlockLog(lockedhandle);

    return 1;
}

void STORMAPI SLogDestroy() {
    SLogFlushAll();

    for (int32_t i = 0; i < 4; i++) {
        EnterCriticalSection(&s_critsect[i]);

        while (s_loghead[i]) {
            char filename[MAX_PATH];
            SStrCopy(filename, s_loghead[i]->filename, STORM_MAX_STR);
            SLogClose(s_loghead[i]->log);
            SErrReportNamedResourceLeak("HSLOG", filename);
        }

        LeaveCriticalSection(&s_critsect[i]);
    }

    s_logsysteminit = 0;

    for (int32_t i = 0; i < 4; i++) {
        DeleteCriticalSection(&s_critsect[i]);
    }

    DeleteCriticalSection(&s_defaultdir_critsect);
}

static int32_t PrepareLog(_LOG* logptr) {
    if (logptr->file != INVALID_HANDLE_VALUE) {
        return 1;
    }

    if (OpenLogFile(logptr->filename, &logptr->file, logptr->flags)) {
        return 1;
    }

    logptr->filename[0] = 0;
    return 0;
}

void STORMAPI SLogVWrite(HSLOG log, const char* format, va_list arglist) {
    HLOCKEDLOG lockedhandle;
    _LOG* logptr = LockLog(log, &lockedhandle, 0);

    if (!logptr) {
        return;
    }

    if (PrepareLog(logptr)) {
        OutputTime(logptr, 1);
        OutputIndent(logptr);
        SStrVPrintf(&logptr->buffer[logptr->bufferused], sizeof(logptr->buffer) - logptr->bufferused, format, arglist);
        logptr->bufferused += SStrLen(&logptr->buffer[logptr->bufferused]);
        OutputReturn(logptr);

        if (g_opt.echotooutputdebugstring) {
            fputs(&logptr->buffer[logptr->pendpoint], stderr);
        }

        logptr->pendpoint = logptr->bufferused;

        if (logptr->bufferused >= 0xC000) {
            FlushLog(logptr);
        }
    }

    UnlockLog(lockedhandle);
}

void SLogWrite(HSLOG log, const char* format, ...) {
    va_list arglist;
    va_start(arglist, format);
    SLogVWrite(log, format, arglist);
    va_end(arglist);
}

#endif
