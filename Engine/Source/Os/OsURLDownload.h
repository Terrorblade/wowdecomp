#ifndef ENGINE_SOURCE_OS_OSURLDOWNLOAD_H
#define ENGINE_SOURCE_OS_OSURLDOWNLOAD_H

#include <cstdint>

namespace HTTPError {
enum ErrorCode {
    NoError = 0,
    Failed = 1,
    Timeout = 2,
    NotFound = 3,
};
}

typedef bool (*OsURLDownloadCallback)(void* param, const char* data, uint32_t size, bool done, HTTPError::ErrorCode error);

class OsUrlDownload {
    public:

    enum STATE : int32_t {
        STATE_CONNECTING = 0,
        STATE_REQUESTING = 1,
        STATE_REQUEST_COMPLETE = 2,
    };

    /* 0000 */ void* m_internet;
    /* 0004 */ void* m_connect;
    /* 0008 */ void* m_request;
    /* 000C */ void* m_connectEvent;
    /* 0010 */ void* m_requestEvent;
    /* 0014 */ void* m_completeEvent;
    /* 0018 */ void* m_dataEvent;
    /* 001C */ int32_t m_state;
    /* 0020 */ bool m_done;
    /* 0024 */ uint32_t m_statusCode;
    /* 0028 */ uint32_t m_error;
    /* 002C */ HTTPError::ErrorCode m_errorCode;
    /* 0030 */ OsURLDownloadCallback m_callback;
    /* 0034 */ void* m_param;

    static bool DownloadString(const char* url, OsURLDownloadCallback callback, void* param, int32_t timeout);

    OsUrlDownload();
};

bool OsURLDownload(const char* url, OsURLDownloadCallback callback, void* param);

#endif
