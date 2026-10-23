
#include <cstdint>
#include <cstring>
#include <windows.h>
#include <wininet.h>
#include "Engine/Source/Os/OsURLDownload.h"
#include "Engine/Source/Storm/H/storm.h"

// OFFSET: 0x0086F960
static uint32_t STORMAPI OsUrlDownloadCallback(void* param) {
    OsUrlDownload* download = static_cast<OsUrlDownload*>(param);

    if (download->m_error) {
        download->m_errorCode = download->m_error == ERROR_INTERNET_TIMEOUT ? HTTPError::Timeout : HTTPError::Failed;
    } else {
        char contentType[256];
        DWORD contentTypeLength = sizeof(contentType);
        HttpQueryInfoA(download->m_request, HTTP_QUERY_CONTENT_TYPE, contentType, &contentTypeLength, nullptr);

        DWORD statusCode;
        DWORD statusCodeLength = sizeof(statusCode);
        if (HttpQueryInfoA(download->m_request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeLength, nullptr)) {
            download->m_statusCode = statusCode;
        } else {
            download->m_statusCode = 0;
        }

        if (download->m_statusCode >= HTTP_STATUS_OK && download->m_statusCode < HTTP_STATUS_AMBIGUOUS) {
            char data[16384];

            do {
                INTERNET_BUFFERSA buffers = {};
                buffers.dwStructSize = sizeof(buffers);
                buffers.lpvBuffer = data;
                buffers.dwBufferLength = sizeof(data);

                if (!InternetReadFileExA(download->m_request, &buffers, 0, reinterpret_cast<DWORD_PTR>(download))) {
                    if (GetLastError() != ERROR_IO_PENDING) {
                        break;
                    }

                    WaitForSingleObject(download->m_completeEvent, INFINITE);
                }

                if (!buffers.dwBufferLength || download->m_done == true) {
                    download->m_done = true;
                } else {
                    WaitForSingleObject(download->m_dataEvent, INFINITE);
                }

                if (download->m_callback && !download->m_callback(download->m_param, data, buffers.dwBufferLength, false, HTTPError::NoError)) {
                    download->m_done = true;
                }
            } while (!download->m_done);
        } else {
            download->m_errorCode = download->m_statusCode == HTTP_STATUS_NOT_FOUND ? HTTPError::NotFound : HTTPError::Failed;
        }
    }

    InternetCloseHandle(download->m_connect);

    return 0;
}

// OFFSET: 0x0086FC80
static void CALLBACK fnInternetCallback(HINTERNET internet, DWORD_PTR context, DWORD internetStatus, LPVOID statusInformation, DWORD statusInformationLength) {
    OsUrlDownload* download = reinterpret_cast<OsUrlDownload*>(context);

    if (!download->m_state) {
        if (internetStatus == INTERNET_STATUS_HANDLE_CREATED) {
            download->m_connect = reinterpret_cast<HINTERNET>(static_cast<INTERNET_ASYNC_RESULT*>(statusInformation)->dwResult);
            SetEvent(download->m_connectEvent);
        } else if (internetStatus == INTERNET_STATUS_HANDLE_CLOSING && internet == download->m_connect) {
            download->m_done = true;
        }

        return;
    }

    switch (internetStatus) {
        case INTERNET_STATUS_RESPONSE_RECEIVED:
            SetEvent(download->m_dataEvent);

            if (*static_cast<DWORD*>(statusInformation)) {
                break;
            }

        case INTERNET_STATUS_CONNECTION_CLOSED:
            download->m_done = true;
            break;

        case INTERNET_STATUS_HANDLE_CREATED:
            download->m_request = reinterpret_cast<HINTERNET>(static_cast<INTERNET_ASYNC_RESULT*>(statusInformation)->dwResult);
            SetEvent(download->m_requestEvent);
            break;

        case INTERNET_STATUS_HANDLE_CLOSING:
            if (internet == download->m_connect) {
                OsURLDownloadCallback callback = download->m_callback;
                void* param = download->m_param;
                HTTPError::ErrorCode errorCode = download->m_errorCode;

                InternetSetStatusCallback(download->m_internet, nullptr);

                DWORD_PTR contextValue = 0;
                InternetSetOptionA(download->m_internet, INTERNET_OPTION_CONTEXT_VALUE, &contextValue, sizeof(contextValue));

                SetEvent(download->m_dataEvent);
                SetEvent(download->m_completeEvent);

                delete download;

                if (callback) {
                    callback(param, nullptr, 0, true, errorCode);
                }
            }

            break;

        case INTERNET_STATUS_REQUEST_COMPLETE:
            if (download->m_state == 2) {
                SetEvent(download->m_completeEvent);
            } else {
                download->m_state = 2;
                download->m_error = static_cast<INTERNET_ASYNC_RESULT*>(statusInformation)->dwError;

                uint32_t threadId;
                SCreateThread(OsUrlDownloadCallback, download, &threadId, nullptr, const_cast<char*>("OsUrlDownloadCallback"), 0);
            }

            break;
    }
}

// INLINED
OsUrlDownload::OsUrlDownload() {
    this->m_internet = nullptr;
    this->m_connect = nullptr;
    this->m_request = nullptr;
    this->m_connectEvent = nullptr;
    this->m_requestEvent = nullptr;
    this->m_completeEvent = nullptr;
    this->m_dataEvent = nullptr;
    this->m_state = 0;
    this->m_done = false;
    this->m_statusCode = 0;
    this->m_callback = nullptr;
    this->m_error = 0;
    this->m_errorCode = HTTPError::NoError;
}

// OFFSET: 0x0086FE40
bool OsUrlDownload::DownloadString(const char* url, OsURLDownloadCallback callback, void* param, int32_t timeout) {
    char hostName[1024];
    char urlPath[1024];

    URL_COMPONENTSA components;
    memset(&components, 0, sizeof(components));
    components.dwStructSize = sizeof(components);
    components.lpszHostName = hostName;
    components.dwHostNameLength = sizeof(hostName);
    components.lpszUrlPath = urlPath;
    components.dwUrlPathLength = sizeof(urlPath);

    if (!InternetCrackUrlA(url, 0, 0, &components)) {
        return false;
    }

    INTERNET_PORT port = components.nPort;

    OsUrlDownload* download = new (__FILE__, __LINE__) OsUrlDownload();

    download->m_connectEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    download->m_requestEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    download->m_completeEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    download->m_dataEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    download->m_callback = callback;
    download->m_param = param;

    download->m_internet = InternetOpenA("Blizzard Web Client", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, INTERNET_FLAG_ASYNC);

    if (download->m_internet && InternetSetStatusCallback(download->m_internet, fnInternetCallback) != INTERNET_INVALID_STATUS_CALLBACK) {
        download->m_state = 0;
        download->m_connect = InternetConnectA(download->m_internet, hostName, port, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, reinterpret_cast<DWORD_PTR>(download));

        if (download->m_connect || GetLastError() == ERROR_IO_PENDING) {
            if (!download->m_connect) {
                WaitForSingleObject(download->m_connectEvent, INFINITE);
            }

            if (timeout > 0) {
                InternetSetOptionA(download->m_connect, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
                InternetSetOptionA(download->m_connect, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
            }

            download->m_state = 1;
            download->m_request = HttpOpenRequestA(download->m_connect, "GET", urlPath, nullptr, nullptr, nullptr, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_UI, reinterpret_cast<DWORD_PTR>(download));

            if (download->m_request || GetLastError() == ERROR_IO_PENDING) {
                if (!download->m_request) {
                    WaitForSingleObject(download->m_requestEvent, INFINITE);
                }

                if (download->m_request && (HttpSendRequestA(download->m_request, nullptr, 0, nullptr, 0) || GetLastError() == ERROR_IO_PENDING)) {
                    return true;
                }
            }
        }
    }

    delete download;

    return false;
}

// OFFSET: 0x00870040
bool OsURLDownload(const char* url, OsURLDownloadCallback callback, void* param) {
    return OsUrlDownload::DownloadString(url, callback, param, 5000);
}

