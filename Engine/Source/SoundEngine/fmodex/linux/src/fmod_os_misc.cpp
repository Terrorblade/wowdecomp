#if defined(WOWDECOMP_PLATFORM_LINUX)

#include "fmod_settings.h"

#include "fmod.h"
#include "fmod_os_misc.h"
#include "fmod_memory.h"

#include <SDL.h>

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

int gSizeofCriticalSection = sizeof(pthread_mutex_t);
int gSizeofSemaphore       = sizeof(sem_t);


/*
	[DESCRIPTION]
    Monotonic clock in microseconds, the time base of the FMOD_OS_Time functions.
*/
static unsigned long long FMOD_OS_Time_GetMicroseconds()
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (unsigned long long)ts.tv_sec * 1000000ULL + (unsigned long long)ts.tv_nsec / 1000ULL;
}


/*
	[DESCRIPTION]
    OS based memory alloc

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
void *FMOD_OS_Memory_Alloc(int size, FMOD_MEMORY_TYPE type)
{
    return malloc(size);
}


/*
	[DESCRIPTION]
    OS based memory re-alloc.

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
void *FMOD_OS_Memory_Realloc(void *ptr, int size, FMOD_MEMORY_TYPE type)
{
    return realloc(ptr, size);
}


/*
	[DESCRIPTION]
    OS based free

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
void FMOD_OS_Memory_Free(void *ptr, FMOD_MEMORY_TYPE type)
{
    free(ptr);
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_File_DriveStatus()
{
    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]
    'unicode' names are FMOD's 16-bit wide strings (UTF-16 as on Windows); they are converted to UTF-8.
    Backslashes are accepted as path separators, as on Windows.

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_File_Open(const char *name, char *mode, int unicode, unsigned int *filesize, void **handle)
{
    char        path[4096];
    int         pos = 0;
    FILE       *fp;
    struct stat st;

    if (!name || !handle)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (unicode)
    {
        const unsigned short *w = (const unsigned short *)name;

        for (; *w && pos < (int)sizeof(path) - 4; w++)
        {
            unsigned int c = *w;

            if (c >= 0xD800 && c < 0xDC00 && w[1] >= 0xDC00 && w[1] < 0xE000)
            {
                c = 0x10000 + ((c - 0xD800) << 10) + (w[1] - 0xDC00);
                w++;
            }

            if (c < 0x80)
            {
                path[pos++] = (char)c;
            }
            else if (c < 0x800)
            {
                path[pos++] = (char)(0xC0 | (c >> 6));
                path[pos++] = (char)(0x80 | (c & 0x3F));
            }
            else if (c < 0x10000)
            {
                path[pos++] = (char)(0xE0 | (c >> 12));
                path[pos++] = (char)(0x80 | ((c >> 6) & 0x3F));
                path[pos++] = (char)(0x80 | (c & 0x3F));
            }
            else
            {
                path[pos++] = (char)(0xF0 | (c >> 18));
                path[pos++] = (char)(0x80 | ((c >> 12) & 0x3F));
                path[pos++] = (char)(0x80 | ((c >> 6) & 0x3F));
                path[pos++] = (char)(0x80 | (c & 0x3F));
            }
        }
    }
    else
    {
        for (; name[pos] && pos < (int)sizeof(path) - 1; pos++)
        {
            path[pos] = name[pos];
        }
    }
    path[pos] = 0;

    for (pos = 0; path[pos]; pos++)
    {
        if (path[pos] == '\\')
        {
            path[pos] = '/';
        }
    }

    fp = fopen(path, "rb");
    if (!fp)
    {
        *handle = 0;

        if (errno != ENOENT && errno != ENOTDIR && errno != ENAMETOOLONG)
        {
            return FMOD_ERR_FILE_BAD;
        }

        return FMOD_ERR_FILE_NOTFOUND;
    }

    if (fstat(fileno(fp), &st) != 0 || S_ISDIR(st.st_mode))
    {
        fclose(fp);
        *handle = 0;
        return FMOD_ERR_FILE_BAD;
    }

    if (filesize)
    {
        *filesize = (unsigned int)st.st_size;
    }

    *handle = fp;

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_File_Close(void *handle)
{
    if (!handle)
    {
        return FMOD_ERR_FILE_BAD;
    }

    fclose((FILE *)handle);

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_File_Cancel(void *handle)
{
    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_File_Read(void *handle, void *buf, unsigned int count, unsigned int *read)
{
    size_t rd;

    if (!handle)
    {
        return FMOD_ERR_FILE_BAD;
    }

    rd = fread(buf, 1, count, (FILE *)handle);

    if (rd != count && ferror((FILE *)handle))
    {
        clearerr((FILE *)handle);
        return FMOD_ERR_FILE_BAD;
    }

    *read = (unsigned int)rd;

    if (rd != count)
    {
        return FMOD_ERR_FILE_EOF;
    }

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_File_Seek(void *handle, unsigned int offset)
{
    if (!handle)
    {
        return FMOD_ERR_FILE_BAD;
    }

    if (fseeko((FILE *)handle, (off_t)offset, SEEK_SET) != 0)
    {
        return FMOD_ERR_FILE_BAD;
    }

    return FMOD_OK;
}


/*
	[DESCRIPTION]
    Debug to console function

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Debug_OutputStr(const char *s)
{
    fputs(s, stderr);

    return FMOD_OK;
}


/*
	[DESCRIPTION]
    Gets the current time in nanoseconds.  (1000th of a millisecond)

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Time_GetNs(unsigned int *ns)
{
    *ns = (unsigned int)FMOD_OS_Time_GetMicroseconds();

    return FMOD_OK;
}


/*
	[DESCRIPTION]
    Gets the current time in milliseconds.  (1000th of a second)

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Time_GetMs(unsigned int *ms)
{
    *ms = (unsigned int)(FMOD_OS_Time_GetMicroseconds() / 1000ULL);

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]
    Sleep(0) on Windows gives up the time slice; sched_yield is the equivalent.

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Time_Sleep(unsigned int ms)
{
    struct timespec req, rem;

    if (!ms)
    {
        sched_yield();
        return FMOD_OK;
    }

    req.tv_sec  = ms / 1000;
    req.tv_nsec = (long)(ms % 1000) * 1000000L;

    while (nanosleep(&req, &rem) == -1 && errno == EINTR)
    {
        req = rem;
    }

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Thread_GetCurrentID(FMOD_UINT_NATIVE *id)
{
    *id = (FMOD_UINT_NATIVE)pthread_self();

    return FMOD_OK;
}


typedef struct
{
    THREAD_RETURNTYPE  (THREAD_CALLCONV *func)(void *param);
    void                *param;
    FMOD_THREAD_PRIORITY priority;
    char                 name[16];
} FMOD_OS_THREADSTART;


/*
	[DESCRIPTION]
    Thread entry point.  Applies the name and priority from inside the new thread, then runs the FMOD callback.

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]
    Priorities map to nice values of the thread (Linux schedules threads individually).  Raising priority above
    normal needs CAP_SYS_NICE or an RLIMIT_NICE allowance; without them the thread keeps normal priority, which is
    the documented fallback.  The critical (mixer) priority additionally tries SCHED_RR, the equivalent of
    THREAD_PRIORITY_TIME_CRITICAL + the "Pro Audio" MMCSS class on Windows, when RLIMIT_RTPRIO allows it.

	[SEE_ALSO]
*/
static void *FMOD_OS_Thread_Start(void *data)
{
    FMOD_OS_THREADSTART  start = *(FMOD_OS_THREADSTART *)data;
    int                  nicevalue = 0;

    free(data);

    if (start.name[0])
    {
        pthread_setname_np(pthread_self(), start.name);
    }

	switch (start.priority)
	{
		case FMOD_THREAD_PRIORITY_VERYLOW:  nicevalue =  10; break;
		case FMOD_THREAD_PRIORITY_LOW:      nicevalue =   5; break;
		case FMOD_THREAD_PRIORITY_NORMAL:   nicevalue =   0; break;
		case FMOD_THREAD_PRIORITY_HIGH:     nicevalue =  -5; break;
		case FMOD_THREAD_PRIORITY_VERYHIGH: nicevalue = -10; break;
		case FMOD_THREAD_PRIORITY_CRITICAL: nicevalue = -15; break;
	};

    if (nicevalue)
    {
        setpriority(PRIO_PROCESS, (id_t)syscall(SYS_gettid), nicevalue);
    }

    if (start.priority == FMOD_THREAD_PRIORITY_CRITICAL)
    {
        struct sched_param param;

        memset(&param, 0, sizeof(param));
        param.sched_priority = sched_get_priority_min(SCHED_RR);
        pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    }

    start.func(start.param);

    return 0;
}


/*
	[DESCRIPTION]

	[PARAMETERS]
	'priority'	-1 to 2, -1 = low 0 = normal, 1 = high, 2 = critical

	[RETURN_VALUE]

	[REMARKS]
    The thread is created detached, like a Windows thread whose handle is just closed by FMOD_OS_Thread_Destroy.
    FMOD's Thread class already waits for the thread function to finish (via its end semaphore) before that.

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Thread_Create(const char *name, THREAD_RETURNTYPE (THREAD_CALLCONV *func)(void *param), void *param, FMOD_THREAD_PRIORITY priority, void *stack, int stacksize, void **handle)
{
    FMOD_OS_THREADSTART *start;
    pthread_attr_t       attr;
    pthread_t            thread;
    int                  rc;

    if (!handle)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    start = (FMOD_OS_THREADSTART *)malloc(sizeof(FMOD_OS_THREADSTART));
    if (!start)
    {
        return FMOD_ERR_MEMORY;
    }

    start->func     = func;
    start->param    = param;
    start->priority = priority;
    start->name[0]  = 0;
    if (name)
    {
        strncpy(start->name, name, sizeof(start->name) - 1);    /* Linux thread names are limited to 15 characters. */
        start->name[sizeof(start->name) - 1] = 0;
    }

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    /*
        The FMOD stack sizes are tuned for Windows' default commit; keep the larger glibc default (8MB) unless asked
        for more.
    */
    if (stacksize > 0)
    {
        size_t defaultsize = 0;

        pthread_attr_getstacksize(&attr, &defaultsize);
        if ((size_t)stacksize > defaultsize)
        {
            pthread_attr_setstacksize(&attr, (size_t)stacksize);
        }
    }

    rc = pthread_create(&thread, &attr, FMOD_OS_Thread_Start, start);
    pthread_attr_destroy(&attr);

    if (rc != 0)
    {
        free(start);
        *handle = 0;
        return FMOD_ERR_MEMORY;
    }

    *handle = (void *)thread;

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]
    Nothing to release: the thread is detached (see FMOD_OS_Thread_Create).

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Thread_Destroy(void *handle)
{
    return FMOD_OK;
}


pthread_mutex_t gMemoryCrit;

/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]
    Recursive mutexes, matching the re-entrant Windows CRITICAL_SECTION.

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_CriticalSection_Create(FMOD_OS_CRITICALSECTION **crit, bool memorycrit)
{
    pthread_mutexattr_t attr;

    if (!crit)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (memorycrit)
    {
        *crit = (FMOD_OS_CRITICALSECTION *)&gMemoryCrit;    /* Can't use alloc on the critical section used in the memory system. */
    }
    else
    {
        *crit = (FMOD_OS_CRITICALSECTION *)FMOD_Memory_Alloc(sizeof(pthread_mutex_t));
        if (!*crit)
        {
            return FMOD_ERR_MEMORY;
        }
    }

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init((pthread_mutex_t *)(*crit), &attr);
    pthread_mutexattr_destroy(&attr);

    return FMOD_OK;
}

/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_CriticalSection_Free(FMOD_OS_CRITICALSECTION *crit)
{
    if (!crit)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    pthread_mutex_destroy((pthread_mutex_t *)crit);

    if (crit != (FMOD_OS_CRITICALSECTION *)&gMemoryCrit)
    {
        FMOD_Memory_Free(crit);
    }

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_CriticalSection_Enter(FMOD_OS_CRITICALSECTION *crit)
{
    if (!crit)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock((pthread_mutex_t *)crit);

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_CriticalSection_Leave(FMOD_OS_CRITICALSECTION *crit)
{
    if (!crit)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    pthread_mutex_unlock((pthread_mutex_t *)crit);

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]
    Counting semaphore starting at 0, like the Windows metered section (initial count 0, max 0xffff).

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Semaphore_Create(FMOD_OS_SEMAPHORE **sema)
{
    sem_t *s;

    if (!sema)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    s = (sem_t *)malloc(sizeof(sem_t));
    if (!s)
    {
        *sema = 0;
        return FMOD_ERR_MEMORY;
    }

    if (sem_init(s, 0, 0) != 0)
    {
        free(s);
        *sema = 0;
        return FMOD_ERR_MEMORY;
    }

    *sema = (FMOD_OS_SEMAPHORE *)s;

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Semaphore_Free(FMOD_OS_SEMAPHORE *sema)
{
    if (!sema)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    sem_destroy((sem_t *)sema);
    free(sema);

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Semaphore_Wait(FMOD_OS_SEMAPHORE *sema)
{
    if (!sema)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    while (sem_wait((sem_t *)sema) == -1 && errno == EINTR)
    {
    }

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Semaphore_Signal(FMOD_OS_SEMAPHORE *sema, bool interrupt)
{
    int value = 0;

    if (!sema)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    sem_getvalue((sem_t *)sema, &value);
    if (value < 0xffff)
    {
        sem_post((sem_t *)sema);
    }

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Library_Load(const char *dllname, FMOD_OS_LIBRARY **handle)
{
    if (!dllname || !handle)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *handle = (FMOD_OS_LIBRARY *)dlopen(dllname, RTLD_NOW | RTLD_LOCAL);
    if (!*handle)
    {
        return FMOD_ERR_FILE_NOTFOUND;
    }

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Library_GetProcAddress(FMOD_OS_LIBRARY *handle, const char *procname, void **address)
{
    if (!handle || !address)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *address = dlsym((void *)handle, procname);
    if (!*address)
    {
        return FMOD_ERR_FORMAT;
    }

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_Library_Free(FMOD_OS_LIBRARY *handle)
{
    if (!handle)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (dlclose((void *)handle) != 0)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]
    Counts SDL's playback + capture devices, like waveOut/waveIn on Windows.  Only meaningful once the SDL output
    has initialized SDL audio.  This runs on every System::update, so it samples at most once a second.

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_CheckDriverList(bool *devicelistchanged)
{
    static int          lastNumDrivers  = -1;
    static unsigned int lastCheckTime   = 0;
    int                 numDrivers      = -1;
    unsigned int        now;

    *devicelistchanged = false;

    if (!SDL_WasInit(SDL_INIT_AUDIO))
    {
        return FMOD_OK;
    }

    FMOD_OS_Time_GetMs(&now);
    if (lastNumDrivers != -1 && now - lastCheckTime < 1000)
    {
        return FMOD_OK;
    }
    lastCheckTime = now;

    numDrivers  = SDL_GetNumAudioDevices(0);
    numDrivers += SDL_GetNumAudioDevices(1);

    *devicelistchanged = (numDrivers != lastNumDrivers && lastNumDrivers != -1);
    lastNumDrivers = numDrivers;

    return FMOD_OK;
}


/*
	[DESCRIPTION]
    SSE detection for FMOD_OS_SupportsSIMD (used only when FMOD_SUPPORT_SIMD is set).

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
extern "C" int FMOD_OS_SupportsSSE()
{
#if defined(__x86_64__) || defined(__i386__)
    return __builtin_cpu_supports("sse") ? 1 : 0;
#else
    return 0;
#endif
}

#endif
