#ifndef ENGINE_SOURCE_GX_CGXDEVICED3D_LINUX_INCLUDE_WINDOWS_H
#define ENGINE_SOURCE_GX_CGXDEVICED3D_LINUX_INCLUDE_WINDOWS_H

// The COM and GDI names the dxvk Direct3D 9 headers need on top of Compat.h, laid out like dxvk's windows_base.h.

#include <cstdint>
#include <cstring>

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

typedef BOOL WINBOOL;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef void VOID;
typedef void* PVOID;
typedef wchar_t WCHAR;
typedef const wchar_t* LPCWSTR;
typedef uint16_t ATOM;
typedef uint32_t COLORREF;

DECLARE_HANDLE(HMONITOR);

#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
} GUID;
#endif

typedef GUID UUID;
typedef GUID IID;

#define REFIID const IID&
#define REFGUID const GUID&
#define REFCLSID const GUID&

template <typename T>
constexpr GUID __uuidof_helper();

#define __uuidof(T) __uuidof_helper<T>()

inline bool operator==(const GUID& a, const GUID& b) {
    return memcmp(&a, &b, sizeof(GUID)) == 0;
}

inline bool operator!=(const GUID& a, const GUID& b) {
    return memcmp(&a, &b, sizeof(GUID)) != 0;
}

typedef struct LUID {
    DWORD LowPart;
    LONG HighPart;
} LUID;

typedef struct PALETTEENTRY {
    BYTE peRed;
    BYTE peGreen;
    BYTE peBlue;
    BYTE peFlags;
} PALETTEENTRY, *LPPALETTEENTRY;

typedef struct RGNDATAHEADER {
    DWORD dwSize;
    DWORD iType;
    DWORD nCount;
    DWORD nRgnSize;
    RECT rcBound;
} RGNDATAHEADER;

typedef struct RGNDATA {
    RGNDATAHEADER rdh;
    char Buffer[1];
} RGNDATA, *LPRGNDATA;

#define CONST const
#define CONST_VTBL const
#define interface struct
#define MIDL_INTERFACE(x) struct
#define DECLARE_INTERFACE(x) struct x
#define DECLARE_INTERFACE_(x, y) struct x : public y
#define DECLARE_INTERFACE_IID_(x, y, z) DECLARE_INTERFACE_(x, y)
#define BEGIN_INTERFACE
#define END_INTERFACE
#define STDMETHOD(name) virtual HRESULT name
#define STDMETHOD_(type, name) virtual type name
#define PURE = 0
#define THIS_
#define THIS
#define DECLSPEC_SELECTANY
#define __MSABI_LONG(x) x
#define DUMMYUNIONNAME
#define DUMMYSTRUCTNAME

#define DEFINE_GUID(iid, a, b, c, d, e, f, g, h, i, j, k) constexpr GUID iid = { a, b, c, { d, e, f, g, h, i, j, k } };

#define __CRT_UUID_DECL(type, a, b, c, d, e, f, g, h, i, j, k) \
    extern "C++" { template <> constexpr GUID __uuidof_helper<type>() { return GUID { a, b, c, { d, e, f, g, h, i, j, k } }; } } \
    extern "C++" { template <> constexpr GUID __uuidof_helper<type*>() { return __uuidof_helper<type>(); } }

#define S_OK ((HRESULT)0)
#define S_FALSE ((HRESULT)1)
#define E_FAIL ((HRESULT)0x80004005)
#define E_INVALIDARG ((HRESULT)0x80070057)
#define E_OUTOFMEMORY ((HRESULT)0x8007000E)
#define E_NOINTERFACE ((HRESULT)0x80004002)
#define MAKE_HRESULT(sev, fac, code) ((HRESULT)(((uint32_t)(sev) << 31) | ((uint32_t)(fac) << 16) | ((uint32_t)(code))))
#define FAILED(hr) ((HRESULT)(hr) < 0)
#define SUCCEEDED(hr) ((HRESULT)(hr) >= 0)

#ifndef LOWORD
#define LOWORD(l) ((WORD)(((uintptr_t)(l)) & 0xFFFF))
#endif

#ifndef HIWORD
#define HIWORD(l) ((WORD)((((uintptr_t)(l)) >> 16) & 0xFFFF))
#endif

struct IUnknown {
    virtual HRESULT QueryInterface(REFIID riid, void** object) = 0;
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;
};

DEFINE_GUID(IID_IUnknown, 0x00000000, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46)
__CRT_UUID_DECL(IUnknown, 0x00000000, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46)

#endif
