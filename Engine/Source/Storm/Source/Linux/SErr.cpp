#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cerrno>
#include <csignal>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "Engine/Source/Os/Linux/OsPosix.h"
#include "Engine/Source/Storm/Source/Linux/StormLinux.h"
#include "Engine/Source/Storm/Source/MachineState.h"
#include "Engine/Source/Storm/Source/SOption.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Storm/H/storm.h"

// Error reporting as on Windows: the same text, the same "Errors/<date> Error.txt" / "Crash.txt" log next to the
// executable, the same handler chain and exit codes. Where Windows shows a message box the text goes to stderr and
// the box's default button is taken (Storm does not link SDL, and nothing can be clicked). Unhandled exceptions are
// fatal signals routed through the same path.

#define STORM_FACILITY 0x510
#define DDRAW_FACILITY 0x876
#define DSOUND_FACILITY 0x878

#define SERR_STR_ERROR 0
#define SERR_STR_CRITICAL 1
#define SERR_STR_PROGRAM 2
#define SERR_STR_FILE_LINE 3
#define SERR_STR_FUNCTION 4
#define SERR_STR_OBJECT 5
#define SERR_STR_HANDLE 6
#define SERR_STR_EXPRESSION 7
#define SERR_STR_DESCRIPTION 8
#define SERR_STR_TERMINATE 9
#define SERR_STR_KEEP_RUNNING 10
#define SERR_STR_FILE 11
#define SERR_STR_BREAK 12
#define SERR_STR_BREAK_OR_RUN 13
#define SERR_STR_EXCEPTION 14
#define SERR_STR_ACCESS_VIOLATION 15
#define SERR_STR_READ 16
#define SERR_STR_WRITTEN 17

// MessageBox results.
#define SERR_IDOK 1
#define SERR_IDCANCEL 2
#define SERR_IDYES 6
#define SERR_IDNO 7

struct APPFATINFO {
    const char* filename;
    int32_t linenumber;
    uint32_t threadId;
};

struct HANDLER : TSLinkedNode<HANDLER> {
    int32_t (STORMAPI* handler)(uint32_t, const char*, const char*, int32_t, const char*);
};

static const char* const s_displaystr[20] = {
    "ERROR #%u (0x%08x) %s",
    "This application has encountered a critical error:\n\n%s\n",
    "Program:\t%s\n",
    "File:\t%s\nLine:\t%d\n",
    "Function:\t%s\n",
    "Object:\t%s\n",
    "Handle:\t%s\n",
    "Expr:\t%s\n\n",
    "\n%s\n\n",
    "Press OK to terminate the application.",
    "Do you wish to keep running anyway?\n\tOK:\tKeep running\n\tCancel:\tTerminate application",
    "File:\t%s\n",
    "Do you wish to break to the debugger?\n\tYes:\tBreak to debugger\n\tNo:\tTerminate application",
    "Do you wish to break to the debugger?\n\tYes:\tBreak to debugger\n\tNo:\tTerminate application\n\tCancel:\tKeep running",
    "Exception:\t%s\n",
    // glibc's %p already prints the 0x that MSVC's leaves out.
    "The instruction at \"%p\" referenced memory at \"%p\".\nThe memory could not be \"%s\".",
    "read",
    "written",
    "Couldn't locate the \"DbgHelp.dll\" debugging DLL.\n\nSome debugging information will be missing from error log files. Press OK to continue, or press Cancel to terminate the program.",
    "Missing Debugging DLL"
};

static LONG s_critsectinit = -1;

static TSList<HANDLER> s_handlerlist;

static MACHINESTATEEXCEPTION* s_exceptionPointers;

static CRITICAL_SECTION s_exceptioncritsect;

static int32_t s_displaying;

static CRITICAL_SECTION s_critsect;

static APPFATINFO s_appFatInfo;

static thread_local uint32_t s_lasterror;

static char s_logtitle[128];

static int32_t (STORMAPI* s_logCallback)(char*, uint32_t);

static uint32_t s_noMiniDumps;

static char s_logLastFile[MAX_PATH * 4];

static volatile sig_atomic_t s_inExceptionFilter;

static int32_t CanBreakToDebugger() {
    return OsPosixDebuggerAttached();
}

static void Breakpoint() {
    // __debugbreak inside __try: only traps when a debugger is there to take it.
    if (CanBreakToDebugger()) {
        raise(SIGTRAP);
    }
}

// Storm has no string resources on Linux, so the built-in English text is always used.
static const char* GetErrorString(uint32_t id) {
    return s_displaystr[id];
}

static void InternalEnterCriticalSection(CRITICAL_SECTION* critsect) {
    if (InterlockedIncrement(&s_critsectinit)) {
        InterlockedDecrement(&s_critsectinit);
    } else {
        InitializeCriticalSection(&s_critsect);
        InitializeCriticalSection(&s_exceptioncritsect);
    }

    EnterCriticalSection(critsect);
}

static inline void InternalLeaveCriticalSection(CRITICAL_SECTION* critsect) {
    LeaveCriticalSection(critsect);
}

void SErrInitialize() {
    g_opt.serrleaksilentwarning = 1;
    SRegLoadValue("Internal", "No Minidumps", 0, &s_noMiniDumps);
}

static inline int32_t HandleToFd(void* handle) {
    return static_cast<int32_t>(reinterpret_cast<intptr_t>(handle));
}

static inline void* FdToHandle(int32_t fd) {
    return reinterpret_cast<void*>(static_cast<intptr_t>(fd));
}

static void WriteAll(int32_t fd, const char* data, size_t length) {
    while (length) {
        ssize_t written = write(fd, data, length);

        if (written < 0 && errno == EINTR) {
            continue;
        }

        if (written <= 0) {
            return;
        }

        data += written;
        length -= static_cast<size_t>(written);
    }
}

// Log lines end in "\n" rather than the Windows "\r\n".
static void WriteLine(void* param, const char* format, ...) {
    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer) - 2, format ? format : "", args);
    va_end(args);
    buffer[sizeof(buffer) - 2] = 0;
    SStrPack(buffer, "\n", sizeof(buffer));

    WriteAll(HandleToFd(param), buffer, SStrLen(buffer));
}

static void WriteMessageToLog(int32_t logfile, const char* message) {
    size_t length = SStrLen(message);

    WriteAll(logfile, "\n", 1);
    WriteAll(logfile, message, length);

    if (!length || message[length - 1] != '\n') {
        WriteAll(logfile, "\n", 1);
    }
}

static int32_t MakeDirectory(const char* path) {
    if (mkdir(path, 0755) == 0) {
        return 1;
    }

    if (errno != EEXIST && errno != EACCES) {
        return 0;
    }

    struct stat st;

    if (stat(path, &st) != 0) {
        return 0;
    }

    return S_ISDIR(st.st_mode) != 0;
}

static int32_t CreateErrorLogFile(const char* suffix, const char* ext, char* logpath, uint32_t logpathchars, const MACHINESTATETIME* time) {
    char logfilename[MAX_PATH];
    char exefullpath[MAX_PATH * 4];

    if (!suffix) {
        suffix = " Log";
    }

    StormLinux::GetExePath(exefullpath, sizeof(exefullpath));
    SStrCopy(logpath, exefullpath, logpathchars);

    // The executable's directory, or the current directory if it is unknown.
    char* slash = SStrChrR(logpath, '/');
    size_t baseLength = slash ? static_cast<size_t>(slash - logpath) + 1 : 0;
    logpath[baseLength] = 0;

    SStrPack(logpath, "Errors/", logpathchars);

    if (!MakeDirectory(logpath)) {
        logpath[baseLength] = 0;
    }

    snprintf(logfilename, sizeof(logfilename), "%04d-%02d-%02d %02d.%02d.%02d %s.%s", time->wYear, time->wMonth, time->wDay, time->wHour, time->wMinute, time->wSecond, suffix, ext);
    SStrPack(logpath, logfilename, logpathchars);

    return open(logpath, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
}

static int32_t CreateErrorLog(const char* suffix, const char* message, const MACHINESTATETIME* time) {
    char logpath[MAX_PATH * 4];
    MACHINESTATETIME localTime;

    if (!time) {
        GetMachineStateLocalTime(&localTime);
        time = &localTime;
    }

    int32_t logfile = CreateErrorLogFile(suffix, "txt", logpath, sizeof(logpath), time);

    if (logfile < 0) {
        return logfile;
    }

    LogComputerInfoHeader(LOG_OPTION_DEFAULT, WriteLine, FdToHandle(logfile), s_logtitle, time);
    WriteMessageToLog(logfile, message);

    if (s_logCallback) {
        char* buffer = static_cast<char*>(alloca(5200));
        buffer[0] = 0;
        s_logCallback(buffer, 5200);

        if (strlen(buffer)) {
            WriteMessageToLog(logfile, buffer);
        }
    }

    SStrCopy(s_logLastFile, logpath, sizeof(s_logLastFile));

    return logfile;
}

static const char* SErrGetErrorText(uint32_t errorcode) {
    switch (errorcode) {
        case STORM_ERROR_ASSERTION:
            return "Assertion Failure";
        case STORM_ERROR_NOT_ARCHIVE:
            return "Not Archive";
        case STORM_ERROR_VERSION_MISMATCH:
            return "Version Mismatch";
        case STORM_ERROR_MEM_NOT_ALLOCATED:
            return "Memory Already Freed";
        case STORM_ERROR_MEM_CORRUPTED:
            return "Memory Corrupt";
        case STORM_ERROR_MEM_INVALID:
            return "Memory Invalid Block";
        case STORM_ERROR_MEM_NOT_FREED:
            return "Memory Never Released";
        case STORM_ERROR_RESOURCES_NOT_RELEASED:
            return "Handle Never Released";
        case STORM_ERROR_OUT_OF_BOUNDS:
            return "Access Out Of Bounds";
        case STORM_ERROR_FILE_CORRUPTED:
            return "File Corrupt";
        case STORM_ERROR_FATAL:
            return "Fatal Exception";
        case STORM_ERROR_FATAL_CONDITION:
            return "Fatal Condition";
        default:
            return "";
    }
}

// Names the signal the way GetExceptionNameWin32 names the matching Windows exception, and gives that exception's
// code for the log line.
static uint32_t GetExceptionNameLinux(const siginfo_t* info, char* buffer, uint32_t buffersize) {
    switch (info->si_signo) {
        case SIGSEGV:
            SStrCopy(buffer, "ACCESS_VIOLATION", buffersize);
            return 0xC0000005;
        case SIGBUS:
            if (info->si_code == BUS_ADRALN) {
                SStrCopy(buffer, "DATATYPE_MISALIGNMENT", buffersize);
                return 0x80000002;
            }

            SStrCopy(buffer, "IN_PAGE_ERROR", buffersize);
            return 0xC0000006;
        case SIGILL:
            if (info->si_code == ILL_PRVOPC || info->si_code == ILL_PRVREG) {
                SStrCopy(buffer, "PRIV_INSTRUCTION", buffersize);
                return 0xC0000096;
            }

            SStrCopy(buffer, "ILLEGAL_INSTRUCTION", buffersize);
            return 0xC000001D;
        case SIGTRAP:
            SStrCopy(buffer, "BREAKPOINT", buffersize);
            return 0x80000003;
        case SIGFPE:
            switch (info->si_code) {
                case FPE_INTDIV:
                    SStrCopy(buffer, "INT_DIVIDE_BY_ZERO", buffersize);
                    return 0xC0000094;
                case FPE_INTOVF:
                    SStrCopy(buffer, "INT_OVERFLOW", buffersize);
                    return 0xC0000095;
                case FPE_FLTDIV:
                    SStrCopy(buffer, "FLT_DIVIDE_BY_ZERO", buffersize);
                    return 0xC000008E;
                case FPE_FLTOVF:
                    SStrCopy(buffer, "FLT_OVERFLOW", buffersize);
                    return 0xC0000091;
                case FPE_FLTUND:
                    SStrCopy(buffer, "FLT_UNDERFLOW", buffersize);
                    return 0xC0000093;
                case FPE_FLTRES:
                    SStrCopy(buffer, "FLT_INEXACT_RESULT", buffersize);
                    return 0xC000008F;
                case FPE_FLTSUB:
                    SStrCopy(buffer, "ARRAY_BOUNDS_EXCEEDED", buffersize);
                    return 0xC000008C;
                default:
                    SStrCopy(buffer, "FLT_INVALID_OPERATION", buffersize);
                    return 0xC0000090;
            }
        case SIGABRT:
            SStrCopy(buffer, "ABORT", buffersize);
            return 0x40000015;
        default: {
            const char* name = strsignal(info->si_signo);
            SStrCopy(buffer, name ? name : "unknown exception", buffersize);
            return 0xE0000000 | static_cast<uint32_t>(info->si_signo);
        }
    }
}

uint32_t STORMAPI SErrGetLastError() {
    return s_lasterror;
}

int32_t STORMAPI SErrIsDisplayingError() {
    return s_displaying;
}

void STORMAPI SErrPrepareAppFatal(const char* filename, int32_t linenumber) {
    InternalEnterCriticalSection(&s_critsect);
    s_appFatInfo.filename = filename;
    s_appFatInfo.linenumber = linenumber;
    s_appFatInfo.threadId = SGetCurrentThreadId();
    InternalLeaveCriticalSection(&s_critsect);
}

void STORMAPI SErrSetLastError(uint32_t errorcode) {
    s_lasterror = errorcode;
}

void STORMAPI SErrSetLogTitleString(const char* title) {
    InternalEnterCriticalSection(&s_critsect);
    SStrCopy(s_logtitle, title, sizeof(s_logtitle));
    InternalLeaveCriticalSection(&s_critsect);
}

void STORMAPI SErrSetLogCallback(int32_t (STORMAPI* callback)(char*, uint32_t)) {
    InternalEnterCriticalSection(&s_critsect);
    s_logCallback = callback;
    InternalLeaveCriticalSection(&s_critsect);
}

int32_t STORMAPI SErrGetLogLastPath(char* buf, uint32_t size) {
    InternalEnterCriticalSection(&s_critsect);
    int32_t result = 0;

    if (s_logLastFile[0]) {
        SStrCopy(buf, s_logLastFile, size);
        result = 1;
    }

    InternalLeaveCriticalSection(&s_critsect);

    return result;
}

static int32_t UndecorateObjectName(const char* source, char* dest, uint32_t destchars) {
    if (SStrLen(source) < 6) {
        return 0;
    }

    if (source[0] != '.') {
        return 0;
    }

    if (source[3] != 'U') {
        return 0;
    }

    if (!strpbrk(source + 4, "?@")) {
        return 0;
    }

    SStrCopy(dest, source + 4, destchars);

    char* end = strpbrk(dest, "?@");

    if (!end) {
        return 0;
    }

    *end = 0;

    for (char* c = end - 1; c >= dest && *c == '_'; --c) {
        *c = 0;
    }

    SStrPack(dest, " (", destchars);
    SStrPack(dest, source, destchars);
    SStrPack(dest, ")", destchars);

    return 1;
}

// FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM) for the Windows error codes the client stores; anything else has no
// text, as with an unknown code on Windows.
static const char* GetSystemErrorMessage(uint32_t errorcode) {
    switch (errorcode) {
        case 0:
            return "The operation completed successfully.";
        case STORM_WIN_ERROR_FILE_NOT_FOUND:
            return "The system cannot find the file specified.";
        case STORM_WIN_ERROR_PATH_NOT_FOUND:
            return "The system cannot find the path specified.";
        case STORM_WIN_ERROR_ACCESS_DENIED:
            return "Access is denied.";
        case STORM_WIN_ERROR_INVALID_HANDLE:
            return "The handle is invalid.";
        case STORM_WIN_ERROR_NOT_ENOUGH_MEMORY:
            return "Not enough storage is available to process this command.";
        case STORM_WIN_ERROR_WRITE_FAULT:
            return "The system cannot write to the specified device.";
        case STORM_WIN_ERROR_READ_FAULT:
            return "The system cannot read from the specified device.";
        case STORM_WIN_ERROR_HANDLE_EOF:
            return "Reached the end of the file.";
        case STORM_WIN_ERROR_FILE_EXISTS:
            return "The file exists.";
        case STORM_WIN_ERROR_INVALID_PARAMETER:
            return "The parameter is incorrect.";
        case STORM_WIN_ERROR_OPEN_FAILED:
            return "The system cannot open the device or file specified.";
        case STORM_WIN_ERROR_DISK_FULL:
            return "There is not enough space on the disk.";
        case STORM_WIN_ERROR_ALREADY_EXISTS:
            return "Cannot create a file when that file already exists.";
        default:
            return nullptr;
    }
}

int32_t STORMAPI SErrGetErrorStr(uint32_t errorcode, char* buffer, uint32_t bufferchars) {
    if (!buffer || !bufferchars) {
        SErrSetLastError(STORM_WIN_ERROR_INVALID_PARAMETER);
        return 0;
    }

    uint32_t facility = (errorcode >> 16) & 0xFFF;
    *buffer = 0;

    if (errorcode == 108) {
        SStrPrintf(buffer, bufferchars, "%s", "Invalid or corrupt archive.\n");
        return 1;
    }

    // Storm, DirectDraw and DirectSound codes come from module message tables, which do not exist here.
    if (facility == STORM_FACILITY || facility == DDRAW_FACILITY || facility == DSOUND_FACILITY) {
        return 0;
    }

    const char* message = GetSystemErrorMessage(errorcode);

    if (!message) {
        return 0;
    }

    SStrPrintf(buffer, bufferchars, "%s\n", message);
    return 1;
}

void STORMAPI SErrRegisterHandler(int32_t (STORMAPI* handler)(uint32_t, const char*, const char*, int32_t, const char*)) {
    InternalEnterCriticalSection(&s_critsect);
    s_handlerlist.NewNode(1, 0, 0)->handler = handler;
    InternalLeaveCriticalSection(&s_critsect);
}

int32_t STORMAPI SErrDestroy() {
    s_handlerlist.Clear();

    if (s_critsectinit != -1) {
        DeleteCriticalSection(&s_critsect);
        DeleteCriticalSection(&s_exceptioncritsect);
        s_critsectinit = -1;
    }

    return 1;
}

int32_t STORMAPI SErrDisplayError(uint32_t errorcode, const char* filename, int32_t linenumber, const char* description, int32_t recoverable, uint32_t exitcode, uint32_t unknown) {
    static int32_t lastlinenumber;
    static const char* lastfilename;

    if (g_opt.unk_0028) {
        if (filename == lastfilename && linenumber == lastlinenumber) {
            return 1;
        }

        lastfilename = filename;
        lastlinenumber = linenumber;
    }

    if (s_displaying) {
        return 0;
    }

    s_displaying = 1;

    static char appfilename[MAX_PATH * 4];
    static char appname[MAX_PATH];
    appfilename[0] = 0;
    appname[0] = 0;
    StormLinux::GetExePath(appfilename, sizeof(appfilename));

    const char* leaf = SStrChrR(appfilename, '/');
    SStrCopy(appname, leaf ? leaf + 1 : appfilename, sizeof(appname));

    if (SStrChrR(appname, '.')) {
        *SStrChrR(appname, '.') = 0;
    }

    static char errorstr[256];
    errorstr[0] = 0;
    SErrGetErrorStr(errorcode, errorstr, sizeof(errorstr));

    if (!errorstr[0]) {
        SStrPrintf(errorstr, sizeof(errorstr), GetErrorString(SERR_STR_ERROR), errorcode, errorcode, SErrGetErrorText(errorcode));
    }

    const char* localnameptr = filename;
    static char localnamebuffer[256];

    if (localnameptr && *localnameptr && (linenumber == SLOG_OBJECT || linenumber == SLOG_HANDLE) && UndecorateObjectName(localnameptr, localnamebuffer, sizeof(localnamebuffer))) {
        localnameptr = localnamebuffer;
    }

    static char outstr[2048];
    char* outptr = outstr;
    char* outend = outstr + sizeof(outstr);
    outstr[0] = 0;

    if (localnameptr) {
        outptr += SStrCopy(outptr, localnameptr, outend - outptr);
    }

    if (linenumber > 0) {
        SStrPrintf(outptr, outend - outptr, "(%u)", linenumber);
        outptr += SStrLen(outptr);
    }

    SStrPrintf(outptr, outend - outptr, " : error %u: ", errorcode);
    outptr += SStrLen(outptr);
    SStrCopy(outptr, errorstr, outend - outptr);
    SDebugPrintf("%s\n", outstr);

    outptr = outstr;
    SStrPrintf(outptr, outend - outptr, GetErrorString(SERR_STR_CRITICAL), errorstr);
    outptr += SStrLen(outptr);
    SStrPrintf(outptr, outend - outptr, GetErrorString(SERR_STR_PROGRAM), appfilename);
    outptr += SStrLen(outptr);

    if (localnameptr && *localnameptr) {
        switch (linenumber) {
            case SLOG_EXCEPTION:
                SStrPrintf(outptr, outend - outptr, GetErrorString(SERR_STR_EXCEPTION), localnameptr);
                break;
            case SLOG_FILE:
                SStrPrintf(outptr, outend - outptr, GetErrorString(SERR_STR_FILE), localnameptr);
                break;
            case SLOG_HANDLE:
                SStrPrintf(outptr, outend - outptr, GetErrorString(SERR_STR_HANDLE), localnameptr);
                break;
            case SLOG_OBJECT:
                SStrPrintf(outptr, outend - outptr, GetErrorString(SERR_STR_OBJECT), localnameptr);
                break;
            case SLOG_FUNCTION:
                SStrPrintf(outptr, outend - outptr, GetErrorString(SERR_STR_FUNCTION), localnameptr);
                break;
            default:
                SStrPrintf(outptr, outend - outptr, GetErrorString(SERR_STR_FILE_LINE), localnameptr, linenumber);
                break;
        }

        outptr += SStrLen(outptr);
    }

    if (errorcode == STORM_ERROR_ASSERTION) {
        SStrPrintf(outptr, outend - outptr, GetErrorString(SERR_STR_EXPRESSION), description ? description : "");
    } else {
        SStrPrintf(outptr, outend - outptr, GetErrorString(SERR_STR_DESCRIPTION), description ? description : "");
    }

    outptr += SStrLen(outptr);

    if (!g_opt.serrsuppresslogs) {
        LoadMachineStateSymbols();
        InternalEnterCriticalSection(&s_exceptioncritsect);

        MACHINESTATEEXCEPTION* exceptionPointers = s_exceptionPointers;
        s_exceptionPointers = nullptr;
        InternalLeaveCriticalSection(&s_exceptioncritsect);

        ucontext_t* context = exceptionPointers ? exceptionPointers->context : nullptr;

        static MACHINESTATETIME time;
        GetMachineStateLocalTime(&time);

        const char* suffix = context ? "Crash" : "Error";
        int32_t logfile = CreateErrorLog(suffix, outstr, &time);

        if (logfile >= 0) {
            LogMachineState(LOG_OPTION_DEFAULT | LOG_OPTION_FRAMEPARAMS, WriteLine, FdToHandle(logfile), context ? 0 : 2, context);
            close(logfile);
        }

        char* details = SStrStr(outstr, "Debug Details:");

        if (details) {
            outptr = details;
        }

        if (LogMiniDumpIsAvailable() && !s_noMiniDumps) {
            static char logpath[MAX_PATH * 4];
            int32_t dumpfile = CreateErrorLogFile(suffix, "dmp", logpath, sizeof(logpath), &time);

            if (dumpfile >= 0) {
                const char* title = s_logtitle;
                LogMiniDump(FdToHandle(dumpfile), exceptionPointers, 1, &title);
                close(dumpfile);
            }
        }

        UnloadMachineStateSymbols();
    }

    if (g_opt.unk_0028) {
        s_displaying = 0;
        return 1;
    }

    int32_t defaultresult;
    int32_t keeprunning;
    int32_t breakresult;

    if (!CanBreakToDebugger()) {
        if (recoverable) {
            outptr += SStrCopy(outptr, GetErrorString(SERR_STR_KEEP_RUNNING), outend - outptr);
            defaultresult = SERR_IDCANCEL;
            keeprunning = SERR_IDOK;
            breakresult = 0;
        } else {
            outptr += SStrCopy(outptr, GetErrorString(SERR_STR_TERMINATE), outend - outptr);
            defaultresult = SERR_IDOK;
            keeprunning = 0;
            breakresult = 0;
        }
    } else {
        if (!recoverable) {
            outptr += SStrCopy(outptr, GetErrorString(SERR_STR_BREAK), outend - outptr);
            defaultresult = SERR_IDNO;
            keeprunning = 0;
            breakresult = SERR_IDYES;
        } else {
            outptr += SStrCopy(outptr, GetErrorString(SERR_STR_BREAK_OR_RUN), outend - outptr);
            defaultresult = SERR_IDNO;
            keeprunning = SERR_IDCANCEL;
            breakresult = SERR_IDYES;
        }
    }

    InternalEnterCriticalSection(&s_critsect);

    int32_t result = keeprunning ? keeprunning : defaultresult;
    HANDLER* handler = s_handlerlist.Head();

    while (handler) {
        if (!handler->handler(errorcode, errorstr, localnameptr, linenumber, description ? description : "")) {
            break;
        }

        handler = s_handlerlist.Next(handler);
    }

    if (!handler) {
        // The message box: print it and take its default button.
        fprintf(stderr, "\n%s\n%s\n\n", appname, outstr);
        fflush(stderr);
        result = defaultresult;
    }

    InternalLeaveCriticalSection(&s_critsect);

    if (result == breakresult) {
        Breakpoint();
        s_displaying = 0;
        return 1;
    }

    if (result == keeprunning) {
        s_displaying = 0;
        return 1;
    }

    _exit(static_cast<int>(exitcode));
}

void STORMAPI SErrReportNamedResourceLeak(const char* handlename, const char* resourcename) {
    static int32_t checked;
    static uint32_t debugmode;

    if (!checked) {
        checked = 1;
        SRegLoadValue("Internal", "Debug Memory", 0, &debugmode);
    }

    if (!debugmode) {
        return;
    }

    char errormessage[256];

    if (resourcename && *resourcename) {
        SStrPrintf(errormessage, sizeof(errormessage), "%s (%s)", handlename, resourcename);
    } else {
        SStrPrintf(errormessage, sizeof(errormessage), "%s", handlename);
    }

    if (g_opt.serrleaksilentwarning) {
        char szMessage[200];
        SStrPrintf(szMessage, sizeof(szMessage), "Storm Error : handle never released -- %s\n", errormessage);
        fputs(szMessage, stderr);
    } else {
        SErrDisplayError(STORM_ERROR_RESOURCES_NOT_RELEASED, errormessage, SLOG_HANDLE, nullptr, 1, 1, 0x11111111);
    }
}

static void WriteExceptionFailed(const char* buffer, const char* message) {
    char path[MAX_PATH * 4];
    StormLinux::GetExePath(path, sizeof(path));
    char* slash = SStrChrR(path, '/');

    if (slash) {
        slash[1] = 0;
    } else {
        path[0] = 0;
    }

    SStrPack(path, "Errors/exception-failed.txt", sizeof(path));

    int32_t file = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);

    if (file >= 0) {
        WriteAll(file, "Exception handler died\n", SStrLen("Exception handler died\n"));
        WriteAll(file, "Original exception:\n", SStrLen("Original exception:\n"));
        WriteAll(file, buffer, SStrLen(buffer));
        WriteAll(file, "\n", 1);
        WriteAll(file, message, SStrLen(message));
        close(file);
    }
}

static char s_exceptionBuffer[120];
static char s_exceptionMessage[256];

// ExceptionFilterWin32. Returning restores the default action, so the fault re-executes and the process dies with
// the signal (EXCEPTION_CONTINUE_SEARCH). A fault inside the filter itself lands here again (SA_NODEFER), which is
// the __except branch of the Windows code.
static void ExceptionFilterLinux(int32_t signo, siginfo_t* info, void* ucontext) {
    if (s_inExceptionFilter) {
        WriteExceptionFailed(s_exceptionBuffer, s_exceptionMessage);
        signal(signo, SIG_DFL);
        return;
    }

    s_inExceptionFilter = 1;

    static MACHINESTATEEXCEPTION exception;
    exception.info = info;
    exception.context = static_cast<ucontext_t*>(ucontext);

    InternalEnterCriticalSection(&s_exceptioncritsect);

    if (!s_exceptionPointers) {
        s_exceptionPointers = &exception;
    }

    InternalLeaveCriticalSection(&s_exceptioncritsect);

    char exceptionname[40];
    exceptionname[0] = 0;
    s_exceptionBuffer[0] = 0;
    s_exceptionMessage[0] = 0;

    uintptr_t pc;
    uintptr_t fp;
    uintptr_t sp;
    OsPosixContextFrame(exception.context, &pc, &fp, &sp);
    void* address = reinterpret_cast<void*>(pc);
    uint32_t code = GetExceptionNameLinux(info, exceptionname, sizeof(exceptionname));
    SStrPrintf(s_exceptionBuffer, sizeof(s_exceptionBuffer), "0x%08X (%s) at %04X:%p", code, exceptionname, OsPosixContextCodeSegment(exception.context), address);

    if (signo == SIGSEGV) {
        int32_t written = OsPosixContextWriteFault(exception.context);
        char format[256];
        SStrCopy(format, GetErrorString(SERR_STR_ACCESS_VIOLATION), sizeof(format));
        SStrPrintf(s_exceptionMessage, sizeof(s_exceptionMessage), format, address, info->si_addr, GetErrorString(written ? SERR_STR_WRITTEN : SERR_STR_READ));
    }

    SErrDisplayError(STORM_ERROR_FATAL, s_exceptionBuffer, SLOG_EXCEPTION, s_exceptionMessage, 0, 1, 0x11111111);

    signal(signo, SIG_DFL);
    s_inExceptionFilter = 0;
}

void SErrDisplayAppFatalCustomV(uint32_t errorcode, const char* format, va_list args) {
    const char* filename = nullptr;
    int32_t linenumber = 0;

    InternalEnterCriticalSection(&s_critsect);

    if (s_appFatInfo.threadId == SGetCurrentThreadId()) {
        filename = s_appFatInfo.filename;
        linenumber = s_appFatInfo.linenumber;
        s_appFatInfo.filename = nullptr;
        s_appFatInfo.linenumber = 0;
        s_appFatInfo.threadId = 0;
    }

    InternalLeaveCriticalSection(&s_critsect);

    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer) - 1, format, args);
    buffer[sizeof(buffer) - 1] = 0;
    SErrDisplayError(errorcode, filename, linenumber, buffer, 0, 1, 0x11111111);
    StormExitProcess(1);
}

void SErrDisplayAppFatalCustom(uint32_t errorcode, const char* format, ...) {
    va_list args;
    va_start(args, format);
    SErrDisplayAppFatalCustomV(errorcode, format, args);
}

void SErrDisplayAppFatal(const char* format, ...) {
    va_list args;
    va_start(args, format);
    SErrDisplayAppFatalCustomV(STORM_ERROR_FATAL_CONDITION, format, args);
}

int32_t SErrDisplayErrorFmt(uint32_t errorcode, const char* filename, int32_t linenumber, int32_t recoverable, uint32_t exitcode, const char* format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer) - 1, format, args);
    va_end(args);
    buffer[sizeof(buffer) - 1] = 0;

    return SErrDisplayError(errorcode, filename, linenumber, buffer, recoverable, exitcode, 0x11111111);
}

// SetUnhandledExceptionFilter. The alternate stack (for the calling thread) lets stack overflows be reported too.
void STORMAPI SErrCatchUnhandledExceptions() {
    static uint8_t s_altstack[256 * 1024];

    stack_t altstack;
    altstack.ss_sp = s_altstack;
    altstack.ss_size = sizeof(s_altstack);
    altstack.ss_flags = 0;
    sigaltstack(&altstack, nullptr);

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = ExceptionFilterLinux;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER;
    sigemptyset(&action.sa_mask);

    static const int32_t signals[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT };

    for (int32_t signo : signals) {
        sigaction(signo, &action, nullptr);
    }
}

void SErrDisplayAppFatalMessage(const char* message, const char* filename, int32_t linenumber) {
    SErrPrepareAppFatal(filename, linenumber);
    SErrDisplayAppFatal("%s", message);
    __builtin_unreachable();
}

#endif
