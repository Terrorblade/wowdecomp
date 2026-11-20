#include "fmod_settings.h"

#include "fmod.h"
#include "fmod_os_misc.h"
#include "fmod_memory.h"

#include "MeteredSection.h"

#include <windows.h>
#include <stdio.h>
#include <process.h>

#include "wasapi\fmod_mmdeviceapi.h"

#define USEMETEREDSECTIONS

int gSizeofCriticalSection = sizeof(CRITICAL_SECTION);
#ifdef USEMETEREDSECTIONS
int gSizeofSemaphore       = sizeof(METERED_SECTION);
#else
int gSizeofSemaphore       = 0;
#endif

#if defined(FMOD_DEBUG) && !defined(PLATFORM_WINDOWS64)

#define MS_VC_EXCEPTION 0x406D1388

typedef struct tagTHREADNAME_INFO
{
    DWORD dwType;       // Must be 0x1000.
    LPCSTR szName;      // Pointer to name (in user addr space).
    DWORD dwThreadID;   // Thread ID (-1=caller thread).
    DWORD dwFlags;      // Reserved for future use, must be zero.
} THREADNAME_INFO;

static void SetThreadName(DWORD dwThreadID, LPCSTR szThreadName)
{
    THREADNAME_INFO info;

    info.dwType     = 0x1000;
    info.szName     = szThreadName;
    info.dwThreadID = dwThreadID;
    info.dwFlags    = 0;

    __try
    {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(DWORD), (DWORD*)&info);
    }
    __except(EXCEPTION_CONTINUE_EXECUTION)
    {
    }
}

#endif


/*
	[DESCRIPTION]
    OS based memory alloc

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
// VERIFY: 230c986b manual
// OFFSET: 0x008D1CB0
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
// VERIFY: ad68686e manual
// OFFSET: 0x008D1CC0
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
// VERIFY: 083548c9 manual
// OFFSET: 0x004A9CE0
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

	[SEE_ALSO]
*/
// VERIFY: 0cb31481 auto
// OFFSET: 0x008D1CD0
FMOD_RESULT FMOD_OS_File_Open(const char *name, char *mode, int unicode, unsigned int *filesize, void **handle)
{
    if (unicode)
    {
        *handle = CreateFileW((WCHAR *)name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    else
    {
        *handle = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }

    if (*handle == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();

        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND && err != ERROR_INVALID_NAME)
        {
            return FMOD_ERR_FILE_BAD;
        }

        return FMOD_ERR_FILE_NOTFOUND;
    }

    *filesize = GetFileSize(*handle, NULL); 

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
// VERIFY: 5b3204ec auto
// OFFSET: 0x008D1D50
FMOD_RESULT FMOD_OS_File_Close(void *handle)
{
    if (handle == INVALID_HANDLE_VALUE)
    {
        return FMOD_ERR_FILE_BAD;
    }

    CloseHandle(handle);

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
// VERIFY: 93452c1c auto
// OFFSET: 0x008D1D70
FMOD_RESULT FMOD_OS_File_Read(void *handle, void *buf, unsigned int count, unsigned int *read)
{
    DWORD rd;

    if (!ReadFile(handle, buf, count, &rd, NULL))
    {
        DWORD lasterr = GetLastError();

        return FMOD_ERR_FILE_BAD;
    }

    *read = rd;

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
// VERIFY: 0122333c auto
// OFFSET: 0x008D1DC0
FMOD_RESULT FMOD_OS_File_Seek(void *handle, unsigned int offset)
{
    DWORD retval;

    retval = SetFilePointer(handle, offset, NULL, FILE_BEGIN);
    
    if (retval == (unsigned int)-1)
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
    OutputDebugStringA(s);

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
// VERIFY: dfe7462a auto
// OFFSET: 0x008D1DF0
FMOD_RESULT FMOD_OS_Time_GetNs(unsigned int *ns)
{
	LARGE_INTEGER	val,freq;

	if (QueryPerformanceCounter(&val))
	{
		QueryPerformanceFrequency(&freq);
		val.QuadPart *= 1000;
		val.QuadPart *= 1000;
		val.QuadPart /= freq.QuadPart;

		*ns = val.LowPart;
	}
    else
    {
    	*ns = timeGetTime() * 1000;
    }

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
// VERIFY: 9ccd9531 auto
// OFFSET: 0x008D1E60
FMOD_RESULT FMOD_OS_Time_GetMs(unsigned int *ms)
{
    *ms = timeGetTime();

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
// VERIFY: 8034e605 auto
// OFFSET: 0x008D1E80
FMOD_RESULT FMOD_OS_Time_Sleep(unsigned int ms)
{
    Sleep(ms);

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
// VERIFY: b7614508 auto
// OFFSET: 0x008D1EA0
FMOD_RESULT FMOD_OS_Thread_GetCurrentID(FMOD_UINT_NATIVE *id)
{
	*id = GetCurrentThreadId();

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]
	'priority'	-1 to 2, -1 = low 0 = normal, 1 = high, 2 = critical
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
// VERIFY: f43f741b auto
// OFFSET: 0x008D2110
FMOD_RESULT FMOD_OS_Thread_Create(const char *name, THREAD_RETURNTYPE (THREAD_CALLCONV *func)(void *param), void *param, FMOD_THREAD_PRIORITY priority, void *stack, int stacksize, void **handle)
{
    DWORD id;

    if (!handle)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, param, 0, &id);
    if (!*handle)
    {
        return FMOD_ERR_MEMORY;
    }

	switch (priority)
	{
		case FMOD_THREAD_PRIORITY_VERYLOW:
        {
			SetThreadPriority(*handle, THREAD_PRIORITY_LOWEST);
			break;
        }
		case FMOD_THREAD_PRIORITY_LOW:
        {
			SetThreadPriority(*handle, THREAD_PRIORITY_BELOW_NORMAL);
			break;
        }
		case FMOD_THREAD_PRIORITY_NORMAL:
        {
			SetThreadPriority(*handle, THREAD_PRIORITY_NORMAL);
			break;
        }
		case FMOD_THREAD_PRIORITY_HIGH:
        {
			SetThreadPriority(*handle, THREAD_PRIORITY_ABOVE_NORMAL);
			break;
        }
		case FMOD_THREAD_PRIORITY_VERYHIGH:
        {
			SetThreadPriority(*handle, THREAD_PRIORITY_HIGHEST);
			break;
        }
		case FMOD_THREAD_PRIORITY_CRITICAL:
        {
            FMOD_OS_LIBRARY *avrt = 0;

			SetThreadPriority(*handle, THREAD_PRIORITY_TIME_CRITICAL);

            if (FMOD_OS_Library_Load("avrt.dll", &avrt) == FMOD_OK)
            {
                FMOD_RESULT result;
                HANDLE (WINAPI *setmmthreadcharacteristics)(LPCSTR taskname, LPDWORD taskindex);
                DWORD taskindex;

                result = FMOD_OS_Library_GetProcAddress(avrt, "AvSetMmThreadCharacteristicsA", (void **)&setmmthreadcharacteristics);
                if (result != FMOD_OK)
                {
                    return result;
                }

                taskindex = 0;
                setmmthreadcharacteristics("Pro Audio", &taskindex);

                result = FMOD_OS_Library_Free(avrt);
                if (result != FMOD_OK)
                {
                    return result;
                }
            }
			break;
        }
	};

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
// VERIFY: 56b3a910 auto
// OFFSET: 0x008D1EC0
FMOD_RESULT FMOD_OS_Thread_Destroy(void *handle)
{
    CloseHandle(handle);

    return FMOD_OK;
}


CRITICAL_SECTION  gMemoryCrit;

/*
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
// VERIFY: e65249a0 auto
// OFFSET: 0x008D1EE0
FMOD_RESULT FMOD_OS_CriticalSection_Create(FMOD_OS_CRITICALSECTION **crit, bool memorycrit)
{
    FMOD_RESULT result = FMOD_OK;

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
        *crit = (FMOD_OS_CRITICALSECTION *)FMOD_Memory_Alloc(sizeof(CRITICAL_SECTION));
        if (!*crit)
        {
            return FMOD_ERR_MEMORY;
        }
    }
    
    InitializeCriticalSection((CRITICAL_SECTION *)(*crit));

    return result;
}

/*
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
// VERIFY: 97ad1e9e auto
// OFFSET: 0x008D1F40
FMOD_RESULT FMOD_OS_CriticalSection_Free(FMOD_OS_CRITICALSECTION *crit)
{
    if (!crit)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    DeleteCriticalSection((CRITICAL_SECTION *)crit);

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
// VERIFY: 2c037747 auto
// OFFSET: 0x008D1F80
FMOD_RESULT FMOD_OS_CriticalSection_Enter(FMOD_OS_CRITICALSECTION *crit)
{
    if (!crit)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    EnterCriticalSection((CRITICAL_SECTION *)crit);

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
// VERIFY: 1dd986c8 auto
// OFFSET: 0x008D1FA0
FMOD_RESULT FMOD_OS_CriticalSection_Leave(FMOD_OS_CRITICALSECTION *crit)
{
    if (!crit)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    LeaveCriticalSection((CRITICAL_SECTION *)crit);

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
// VERIFY: 21da4d2b auto
// OFFSET: 0x008D1FC0
FMOD_RESULT FMOD_OS_Semaphore_Create(FMOD_OS_SEMAPHORE **sema)
{
    if (!sema)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

#ifdef USEMETEREDSECTIONS
    *sema = (FMOD_OS_SEMAPHORE *)CreateMeteredSection(0, 0xffff, NULL);
#else
    *sema = (FMOD_OS_SEMAPHORE *)CreateSemaphore(NULL, 0, 0xffff, NULL);
#endif

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
// VERIFY: 90fda9b6 auto
// OFFSET: 0x008D1FF0
FMOD_RESULT FMOD_OS_Semaphore_Free(FMOD_OS_SEMAPHORE *sema)
{
    if (!sema)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

#ifdef USEMETEREDSECTIONS
    CloseMeteredSection((LPMETERED_SECTION)sema);
#else
    CloseHandle((HANDLE)sema);
#endif

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
// VERIFY: 89a4c1f0 auto
// OFFSET: 0x008D2010
FMOD_RESULT FMOD_OS_Semaphore_Wait(FMOD_OS_SEMAPHORE *sema)
{
    if (!sema)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

#ifdef USEMETEREDSECTIONS
    EnterMeteredSection((LPMETERED_SECTION)sema, INFINITE);
#else
    WaitForSingleObject((HANDLE)sema, INFINITE);
#endif

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
// VERIFY: 8bb869e7 auto
// OFFSET: 0x008D2030
FMOD_RESULT FMOD_OS_Semaphore_Signal(FMOD_OS_SEMAPHORE *sema, bool interrupt)
{
    if (!sema)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

#ifdef USEMETEREDSECTIONS
    LeaveMeteredSection((LPMETERED_SECTION)sema, 1, NULL);
#else
    ReleaseSemaphore((HANDLE)sema, 1, NULL);
#endif

    return FMOD_OK;
}


/*
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
// VERIFY: 85e899dc auto
// OFFSET: 0x008D2060
FMOD_RESULT FMOD_OS_Library_Load(const char *dllname, FMOD_OS_LIBRARY **handle)
{
    if (!dllname || !handle)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *handle = (FMOD_OS_LIBRARY *)LoadLibraryA(dllname);
    if (!*handle)
    {
        DWORD err = GetLastError();

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
// VERIFY: ac9c9848 auto
// OFFSET: 0x008D20A0
FMOD_RESULT FMOD_OS_Library_GetProcAddress(FMOD_OS_LIBRARY *handle, const char *procname, void **address)
{
    if (!handle || !address)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *address = (void *)GetProcAddress((HMODULE)handle, procname);
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
// VERIFY: c193f5df manual
// OFFSET: 0x008D20E0
FMOD_RESULT FMOD_OS_Library_Free(FMOD_OS_LIBRARY *handle)
{
    if (!handle)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (!FreeLibrary((HMODULE)handle))
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

	[SEE_ALSO]
*/
#ifdef FMOD_SUPPORT_WASAPI

static const GUID IID_IMMNotificationClient = { 0x7991eec9, 0x7e89, 0x4d85, { 0x83, 0x90, 0x6c, 0x70, 0x3c, 0xec, 0x60, 0xc0 } };

static volatile LONG gDeviceListChanged = 0;

class FMOD_OS_DeviceNotificationClient : public IMMNotificationClient
{
  public:

    HRESULT F_STDCALL QueryInterface(REFIID riid, void **object)
    {
        if (riid == IID_IUnknown || riid == IID_IMMNotificationClient)
        {
            *object = static_cast<IMMNotificationClient *>(this);
            return S_OK;
        }

        *object = NULL;
        return E_NOINTERFACE;
    }

    ULONG F_STDCALL AddRef()
    {
        return 1;
    }

    ULONG F_STDCALL Release()
    {
        return 1;
    }

    HRESULT F_STDCALL OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState)
    {
        InterlockedExchange(&gDeviceListChanged, 1);
        return S_OK;
    }

    HRESULT F_STDCALL OnDeviceAdded(LPCWSTR pwstrDeviceId)
    {
        InterlockedExchange(&gDeviceListChanged, 1);
        return S_OK;
    }

    HRESULT F_STDCALL OnDeviceRemoved(LPCWSTR pwstrDeviceId)
    {
        InterlockedExchange(&gDeviceListChanged, 1);
        return S_OK;
    }

    HRESULT F_STDCALL OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId)
    {
        if (role == eConsole)
        {
            InterlockedExchange(&gDeviceListChanged, 1);
        }
        return S_OK;
    }

    HRESULT F_STDCALL OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key)
    {
        return S_OK;
    }
};

static FMOD_OS_DeviceNotificationClient gDeviceNotificationClient;

static bool FMOD_OS_RegisterDeviceNotifications()
{
    static IMMDeviceEnumerator *enumerator = NULL;
    static bool                 attempted  = false;

    if (attempted)
    {
        return enumerator != NULL;
    }
    attempted = true;

    HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER | CLSCTX_LOCAL_SERVER, IID_IMMDeviceEnumerator, (void **)&enumerator);

    if (hr == CO_E_NOTINITIALIZED)
    {
        CoInitialize(NULL);
        hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER | CLSCTX_LOCAL_SERVER, IID_IMMDeviceEnumerator, (void **)&enumerator);
    }

    if (hr < 0 || !enumerator)
    {
        enumerator = NULL;
        return false;
    }

    if (enumerator->RegisterEndpointNotificationCallback(&gDeviceNotificationClient) < 0)
    {
        enumerator->Release();
        enumerator = NULL;
        return false;
    }

    return true;
}

#endif

FMOD_RESULT FMOD_OS_CheckDriverList(bool *devicelistchanged)
{
    static int  lastNumDrivers  = -1;
    int         numDrivers      = -1;

#ifdef FMOD_SUPPORT_WASAPI
    if (FMOD_OS_RegisterDeviceNotifications())
    {
        *devicelistchanged = InterlockedExchange(&gDeviceListChanged, 0) != 0;
        return FMOD_OK;
    }
#endif

    numDrivers  = waveOutGetNumDevs();
    numDrivers += waveInGetNumDevs();

    *devicelistchanged = (numDrivers != lastNumDrivers && lastNumDrivers != -1);
    lastNumDrivers = numDrivers;

    return FMOD_OK;
}



/*
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
