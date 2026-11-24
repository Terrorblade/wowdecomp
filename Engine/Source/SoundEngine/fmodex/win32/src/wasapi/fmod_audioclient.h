#ifndef _FMOD_AUDIOCLIENT_H
#define _FMOD_AUDIOCLIENT_H

#include "fmod_settings.h"

#ifdef FMOD_SUPPORT_WASAPI

#include "fmod_codec_wav.h"

// GUIDs
static const GUID  IID_IAudioClock                  = { 0xcd63314f, 0x3fba, 0x4a1b, { 0x81, 0x2c, 0xef, 0x96, 0x35, 0x87, 0x28, 0xe7 } };
static const GUID  IID_IAudioClient                 = { 0x1cb9ad4c, 0xdbfa, 0x4c32, { 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2 } };
static const GUID  IID_IAudioRenderClient           = { 0xf294acfc, 0x3146, 0x4483, { 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2 } };
static const GUID  IID_IAudioCaptureClient          = { 0xc8adbd64, 0xe71e, 0x48a0, { 0xa4, 0xde, 0x18, 0x5c, 0x39, 0x5c, 0xd3, 0x17 } };
static const GUID  _KSDATAFORMAT_SUBTYPE_PCM        = { 0x00000001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const GUID  _KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

// Helper defines
typedef        LONGLONG    REFERENCE_TIME;
typedef const  FMOD_GUID*  LPCFMOD_GUID;
typedef enum
{ 
    AUDCLNT_SHAREMODE_SHARED, 
    AUDCLNT_SHAREMODE_EXCLUSIVE 
} AUDCLNT_SHAREMODE;

// Error defines
#ifndef FACILITY_AUDCLNT
#define FACILITY_AUDCLNT              0x889
#endif
#define AUDCLNT_ERR(n)                MAKE_HRESULT(SEVERITY_ERROR, FACILITY_AUDCLNT, n)
#define AUDCLNT_SUCCESS(n)            MAKE_SCODE(SEVERITY_SUCCESS, FACILITY_AUDCLNT, n)
#define AUDCLNT_E_NOT_INITIALIZED               AUDCLNT_ERR(0x001)
#define AUDCLNT_E_ALREADY_INITIALIZED           AUDCLNT_ERR(0x002)
#define AUDCLNT_E_WRONG_ENDPOINT_TYPE           AUDCLNT_ERR(0x003)
#define AUDCLNT_E_DEVICE_INVALIDATED            AUDCLNT_ERR(0x004)
#define AUDCLNT_E_NOT_STOPPED                   AUDCLNT_ERR(0x005)
#define AUDCLNT_E_BUFFER_TOO_LARGE              AUDCLNT_ERR(0x006)
#define AUDCLNT_E_OUT_OF_ORDER                  AUDCLNT_ERR(0x007)
#define AUDCLNT_E_UNSUPPORTED_FORMAT            AUDCLNT_ERR(0x008)
#define AUDCLNT_E_INVALID_SIZE                  AUDCLNT_ERR(0x009)
#define AUDCLNT_E_DEVICE_IN_USE                 AUDCLNT_ERR(0x00a)
#define AUDCLNT_E_BUFFER_OPERATION_PENDING      AUDCLNT_ERR(0x00b)
#define AUDCLNT_E_THREAD_NOT_REGISTERED         AUDCLNT_ERR(0x00c)
#define AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED    AUDCLNT_ERR(0x00e)
#define AUDCLNT_E_ENDPOINT_CREATE_FAILED        AUDCLNT_ERR(0x00f)
#define AUDCLNT_E_SERVICE_NOT_RUNNING           AUDCLNT_ERR(0x010)
#define AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED      AUDCLNT_ERR(0x011)
#define AUDCLNT_E_EXCLUSIVE_MODE_ONLY           AUDCLNT_ERR(0x012)
#define AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL  AUDCLNT_ERR(0x013)
#define AUDCLNT_E_EVENTHANDLE_NOT_SET           AUDCLNT_ERR(0x014)
#define AUDCLNT_E_INCORRECT_BUFFER_SIZE         AUDCLNT_ERR(0x015)
#define AUDCLNT_E_BUFFER_SIZE_ERROR             AUDCLNT_ERR(0x016)
#define AUDCLNT_E_CPUUSAGE_EXCEEDED             AUDCLNT_ERR(0x017)
#define AUDCLNT_S_BUFFER_EMPTY                  AUDCLNT_SUCCESS(0x001)
#define AUDCLNT_S_THREAD_ALREADY_REGISTERED     AUDCLNT_SUCCESS(0x002)
#define AUDCLNT_S_POSITION_STALLED		        AUDCLNT_SUCCESS(0x003)

// AudioClient stream flags
#define AUDCLNT_STREAMFLAGS_CROSSPROCESS  0x00010000
#define AUDCLNT_STREAMFLAGS_LOOPBACK      0x00020000
#define AUDCLNT_STREAMFLAGS_EVENTCALLBACK 0x00040000
#define AUDCLNT_STREAMFLAGS_NOPERSIST     0x00080000

// Used to create and initialize a connection to the audio engine 
struct IAudioClient : public IUnknown
{
  public:
    virtual HRESULT F_STDCALL Initialize         (AUDCLNT_SHAREMODE ShareMode, DWORD StreamFlags, REFERENCE_TIME hnsBufferDuration, REFERENCE_TIME hnsPeriodicity, const FMOD::WAVE_FORMATEX *pFormat, LPCFMOD_GUID AudioSessionGuid) = 0;
    virtual HRESULT F_STDCALL GetBufferSize      (UINT32 *pNumBufferFrames) = 0;
    virtual HRESULT F_STDCALL GetStreamLatency   (REFERENCE_TIME *phnsLatency) = 0;
    virtual HRESULT F_STDCALL GetCurrentPadding  (UINT32 *pNumPaddingFrames) = 0;
    virtual HRESULT F_STDCALL IsFormatSupported  (AUDCLNT_SHAREMODE ShareMode, const FMOD::WAVE_FORMATEX *pFormat, FMOD::WAVE_FORMATEX **ppClosestMatch) = 0;
    virtual HRESULT F_STDCALL GetMixFormat       (FMOD::WAVE_FORMATEX **ppDeviceFormat) = 0;
    virtual HRESULT F_STDCALL GetDevicePeriod    (REFERENCE_TIME *phnsDefaultDevicePeriod, REFERENCE_TIME *phnsMinimumDevicePeriod) = 0;
    virtual HRESULT F_STDCALL Start              (void) = 0;
    virtual HRESULT F_STDCALL Stop               (void) = 0;
    virtual HRESULT F_STDCALL Reset              (void) = 0;
    virtual HRESULT F_STDCALL SetEventHandle     (HANDLE eventHandle) = 0;
    virtual HRESULT F_STDCALL GetService         (REFIID riid, void **ppv) = 0;
};

// Used for monitoring the stream data rate and stream position
struct IAudioClock : public IUnknown
{
  public:
    virtual HRESULT STDMETHODCALLTYPE GetFrequency        (UINT64 *pu64Frequency) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPosition         (UINT64 *pu64Position, UINT64 *pu64QPCPosition) = 0;    
    virtual HRESULT STDMETHODCALLTYPE GetCharacteristics  (DWORD *pdwCharacteristics) = 0;
};

// Used by audio-rendering applications to stream their output data to a rendering endpoint device
struct IAudioRenderClient : public IUnknown
{
  public:
    virtual HRESULT F_STDCALL GetBuffer      (UINT32 NumFramesRequested, BYTE **ppData) = 0;
    virtual HRESULT F_STDCALL ReleaseBuffer  (UINT32 NumFramesWritten, DWORD dwFlags) = 0;
};

// Used by a capture application to read capture data from a capture endpoint buffer
struct IAudioCaptureClient : public IUnknown
{
  public:
    virtual HRESULT F_STDCALL GetBuffer          (BYTE **ppData, UINT32 *pNumFramesToRead, DWORD *pdwFlags, UINT64 *pu64DevicePosition, UINT64 *pu64QPCPosition) = 0;
    virtual HRESULT F_STDCALL ReleaseBuffer      (UINT32 NumFramesRead) = 0;
    virtual HRESULT F_STDCALL GetNextPacketSize  (UINT32 *pNumFramesInNextPacket) = 0;
};

#endif  /* FMOD_SUPPORT_WASAPI */

#endif  /* _FMOD_AUDIOCLIENT_H */