#ifndef ENGINE_OS_LINUX_COMPAT_H
#define ENGINE_OS_LINUX_COMPAT_H

// Force-included into every Linux translation unit, C and C++ (see the top-level CMakeLists.txt). The decompiled
// code is written against MSVC and the Windows headers; this supplies the handful of their names that the portable
// code uses, with the sizes they have on Windows (LONG and DWORD stay 32-bit under LP64), so that code compiles
// unchanged. Platform code lives under Linux/ directories instead.

#include <alloca.h>
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <malloc.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// Calling conventions. -fms-extensions covers __int64, __declspec and __forceinline.
#define __stdcall
#define __cdecl
#define __fastcall
#define __thiscall
#define WINAPI
#define APIENTRY
#define CALLBACK
#define STDMETHODCALLTYPE

// Basic Windows types.
typedef int32_t BOOL;
typedef uint8_t BYTE;
typedef uint8_t BOOLEAN;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef uint32_t ULONG;
typedef int32_t INT;
typedef uint32_t UINT;
typedef float FLOAT;
typedef char CHAR;
typedef int16_t SHORT;
typedef uint16_t USHORT;
typedef int64_t LONGLONG;
typedef uint64_t ULONGLONG;
typedef uint64_t DWORD64;
typedef intptr_t INT_PTR;
typedef uintptr_t UINT_PTR;
typedef intptr_t LONG_PTR;
typedef uintptr_t ULONG_PTR;
typedef uintptr_t DWORD_PTR;
typedef size_t SIZE_T;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef char* LPSTR;
typedef const char* LPCSTR;
typedef DWORD* LPDWORD;
typedef LONG HRESULT;
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;

typedef void* HANDLE;
#define DECLARE_HANDLE(name) struct name##__ { int unused; }; typedef struct name##__* name
DECLARE_HANDLE(HWND);
DECLARE_HANDLE(HDC);
DECLARE_HANDLE(HGLRC);
DECLARE_HANDLE(HINSTANCE);
DECLARE_HANDLE(HICON);
DECLARE_HANDLE(HCURSOR);
DECLARE_HANDLE(HMENU);
DECLARE_HANDLE(HKEY);
typedef HINSTANCE HMODULE;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define INFINITE 0xFFFFFFFF
#define MAX_PATH 260
#define CP_ACP 0
#define CP_UTF8 65001

typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG HighPart;
    };
    struct {
        DWORD LowPart;
        LONG HighPart;
    } u;
    LONGLONG QuadPart;
} LARGE_INTEGER;

typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT;

typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT;

typedef struct _OVERLAPPED {
    ULONG_PTR Internal;
    ULONG_PTR InternalHigh;
    union {
        struct {
            DWORD Offset;
            DWORD OffsetHigh;
        };
        void* Pointer;
    };
    HANDLE hEvent;
} OVERLAPPED;

// CRITICAL_SECTION is 40 bytes on x64 Windows, the same as glibc's pthread_mutex_t, so structs that embed one keep
// their layout. It is a recursive mutex, like the Windows object.
typedef struct _CRITICAL_SECTION {
    pthread_mutex_t mutex;
} CRITICAL_SECTION;

static inline void InitializeCriticalSection(CRITICAL_SECTION* cs) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&cs->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}

static inline void DeleteCriticalSection(CRITICAL_SECTION* cs) {
    pthread_mutex_destroy(&cs->mutex);
}

static inline void EnterCriticalSection(CRITICAL_SECTION* cs) {
    pthread_mutex_lock(&cs->mutex);
}

static inline BOOL TryEnterCriticalSection(CRITICAL_SECTION* cs) {
    return pthread_mutex_trylock(&cs->mutex) == 0;
}

static inline void LeaveCriticalSection(CRITICAL_SECTION* cs) {
    pthread_mutex_unlock(&cs->mutex);
}

// Interlocked operations: full barriers, returning what the Windows functions return.
static inline LONG InterlockedIncrement(volatile LONG* p) {
    return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST);
}

static inline LONG InterlockedDecrement(volatile LONG* p) {
    return __atomic_sub_fetch(p, 1, __ATOMIC_SEQ_CST);
}

static inline LONG InterlockedExchange(volatile LONG* p, LONG value) {
    return __atomic_exchange_n(p, value, __ATOMIC_SEQ_CST);
}

static inline LONG InterlockedExchangeAdd(volatile LONG* p, LONG value) {
    return __atomic_fetch_add(p, value, __ATOMIC_SEQ_CST);
}

static inline LONG InterlockedCompareExchange(volatile LONG* p, LONG exchange, LONG comparand) {
    __atomic_compare_exchange_n(p, &comparand, exchange, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return comparand;
}

static inline void* InterlockedCompareExchangePointer(void* volatile* p, void* exchange, void* comparand) {
    __atomic_compare_exchange_n(p, &comparand, exchange, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return comparand;
}

static inline void* InterlockedExchangePointer(void* volatile* p, void* value) {
    return __atomic_exchange_n(p, value, __ATOMIC_SEQ_CST);
}

#define _InterlockedCompareExchange InterlockedCompareExchange

#ifdef __cplusplus
// Some counters are declared `long`, which is LONG on Windows but 64-bit here.
static inline long InterlockedIncrement(volatile long* p) {
    return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST);
}

static inline long InterlockedDecrement(volatile long* p) {
    return __atomic_sub_fetch(p, 1, __ATOMIC_SEQ_CST);
}

static inline long InterlockedExchange(volatile long* p, long value) {
    return __atomic_exchange_n(p, value, __ATOMIC_SEQ_CST);
}

static inline long InterlockedExchangeAdd(volatile long* p, long value) {
    return __atomic_fetch_add(p, value, __ATOMIC_SEQ_CST);
}

static inline long InterlockedCompareExchange(volatile long* p, long exchange, long comparand) {
    __atomic_compare_exchange_n(p, &comparand, exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return comparand;
}
#endif

// MSVC CRT names.
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define stricmp strcasecmp
#define strnicmp strncasecmp
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#define _alloca alloca
#define _msize malloc_usable_size
#define _finite isfinite
#define _isnan isnan
#define _strtoi64 strtoll
#define _strtoui64 strtoull
#define _atoi64 atoll
#define _ismbcspace isspace

static inline int fopen_s(FILE** stream, const char* path, const char* mode) {
    *stream = fopen(path, mode);
    return *stream ? 0 : -1;
}

static inline char* _strupr(char* s) {
    for (char* p = s; *p; p++) {
        *p = (char)toupper((unsigned char)*p);
    }
    return s;
}

static inline char* _strlwr(char* s) {
    for (char* p = s; *p; p++) {
        *p = (char)tolower((unsigned char)*p);
    }
    return s;
}

// _rotl/_rotr are clang builtins under -fms-extensions.

#define __debugbreak() __builtin_debugtrap()

// MSVC's type_info::raw_name() names the allocation tags (CDataAllocator, TExtraInstanceRecycler, ...); name() is the
// portable spelling.
#define raw_name name

#endif
