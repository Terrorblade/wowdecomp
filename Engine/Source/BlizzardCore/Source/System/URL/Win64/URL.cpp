
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <windows.h>
#include <wininet.h>
#include "Engine/Source/BlizzardCore/Source/System/URL/URL.h"
#include "Engine/Source/BlizzardCore/Source/System/String/String.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/Thread.h"
#include "Engine/Source/BlizzardCore/Source/System/Unicode/Unicode.h"

// STATIC: 0x00B38010
static HINTERNET s_session;

// OFFSET: 0x0042CA00
static uint32_t ReadResponseData(void* param) {
    Blizzard::URL::HttpRequest* request = static_cast<Blizzard::URL::HttpRequest*>(param);

    if (request->m_error) {
        switch (request->m_error) {
            case ERROR_INTERNET_TIMEOUT:
                request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_TIMEOUT;
                break;

            case ERROR_INTERNET_NAME_NOT_RESOLVED:
                request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_NAME_NOT_RESOLVED;
                break;

            case ERROR_INTERNET_CANNOT_CONNECT:
                request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_CANNOT_CONNECT;
                break;

            default:
                request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_FAILED;
                break;
        }

        InternetCloseHandle(request->m_connect);
        return 0;
    }

    char contentType[256];
    DWORD contentTypeLength = sizeof(contentType);
    HttpQueryInfoA(request->m_request, HTTP_QUERY_CONTENT_TYPE, contentType, &contentTypeLength, nullptr);

    DWORD statusCode;
    DWORD statusCodeLength = sizeof(statusCode);
    if (!HttpQueryInfoA(request->m_request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeLength, nullptr)) {
        statusCode = 0;
    }

    request->m_callback->GetHTTPStatusCode(statusCode);

    if (statusCode >= HTTP_STATUS_OK && statusCode < HTTP_STATUS_AMBIGUOUS) {
        char lastModified[256];
        DWORD lastModifiedLength = sizeof(lastModified);
        if (HttpQueryInfoA(request->m_request, HTTP_QUERY_LAST_MODIFIED, lastModified, &lastModifiedLength, nullptr)) {
            request->m_callback->GetLastModified(lastModified);
        }

        char contentLength[256];
        DWORD contentLengthLength = sizeof(contentLength);
        if (HttpQueryInfoA(request->m_request, HTTP_QUERY_CONTENT_LENGTH, contentLength, &contentLengthLength, nullptr)) {
            request->m_callback->GetContentLength(static_cast<uint32_t>(atol(contentLength)));
        }

        if (!request->m_callback->WantData()) {
            request->m_done = true;
            InternetCloseHandle(request->m_connect);
            return 0;
        }

        char data[16384];

        do {
            INTERNET_BUFFERSA buffers;
            memset(&buffers, 0, sizeof(buffers));
            buffers.dwStructSize = sizeof(buffers);
            buffers.lpvBuffer = data;
            buffers.dwBufferLength = sizeof(data);

            if (!InternetReadFileExA(request->m_request, &buffers, 0, reinterpret_cast<DWORD_PTR>(request))) {
                if (GetLastError() != ERROR_IO_PENDING) {
                    break;
                }

                WaitForSingleObject(request->m_completeEvent, INFINITE);
            }

            if (!request->m_done) {
                if (!buffers.dwBufferLength) {
                    request->m_done = true;
                    continue;
                }

                WaitForSingleObject(request->m_dataEvent, INFINITE);
            }

            if (buffers.dwBufferLength) {
                if (!request->m_callback->ReceivedData(data, buffers.dwBufferLength)) {
                    request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_ABORTED;
                    request->m_done = true;
                }

                if (request->m_callback->Stop()) {
                    request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_STOPPED;
                    request->m_done = true;
                }
            }
        } while (!request->m_done);

        InternetCloseHandle(request->m_connect);
        return 0;
    }

    switch (statusCode) {
        case HTTP_STATUS_NOT_MODIFIED:
            request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_NOT_MODIFIED;
            break;

        case HTTP_STATUS_BAD_REQUEST:
            request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_BAD_REQUEST;
            break;

        case HTTP_STATUS_FORBIDDEN:
            request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_FORBIDDEN;
            break;

        case HTTP_STATUS_NOT_FOUND:
            request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_NOT_FOUND;
            break;

        default:
            request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_FAILED;
            break;
    }

    InternetCloseHandle(request->m_connect);
    return 0;
}

// OFFSET: 0x0042CD80
Blizzard::URL::HttpRequest::~HttpRequest() {
    CloseHandle(this->m_dataEvent);
    CloseHandle(this->m_completeEvent);
    CloseHandle(this->m_requestEvent);
    CloseHandle(this->m_connectEvent);
}

// OFFSET: 0x0042CDC0
static HINTERNET ConnectServer(Blizzard::URL::HttpRequest* request, const char* serverName, INTERNET_PORT serverPort) {
    request->m_state = 0;

    HINTERNET connect = InternetConnectA(request->m_session, serverName, serverPort, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, reinterpret_cast<DWORD_PTR>(request));

    if (!connect) {
        if (GetLastError() != ERROR_IO_PENDING) {
            return nullptr;
        }

        WaitForSingleObject(request->m_connectEvent, INFINITE);
    }

    DWORD timeout = request->m_callback->GetTimeout();
    InternetSetOptionA(connect, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(connect, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    return connect;
}

// OFFSET: 0x00437840
Blizzard::URL::HttpRequest::HttpRequest() {
    this->m_connectEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    this->m_requestEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    this->m_completeEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    this->m_dataEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    this->m_session = nullptr;
    this->m_connect = nullptr;
    this->m_request = nullptr;
    this->m_state = 0;
    this->m_done = false;
    this->m_connected = false;
    this->m_callback = nullptr;
    this->m_error = 0;
    this->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_SUCCESS;
}

// OFFSET: 0x00451290
static void CALLBACK InternetCallback(HINTERNET internet, DWORD_PTR context, DWORD internetStatus, LPVOID statusInformation, DWORD statusInformationLength) {
    Blizzard::URL::HttpRequest* request = reinterpret_cast<Blizzard::URL::HttpRequest*>(context);

    if (!request->m_state) {
        if (internetStatus == INTERNET_STATUS_HANDLE_CREATED) {
            request->m_connect = reinterpret_cast<HINTERNET>(static_cast<INTERNET_ASYNC_RESULT*>(statusInformation)->dwResult);
            SetEvent(request->m_connectEvent);
        } else if (internetStatus == INTERNET_STATUS_HANDLE_CLOSING && internet == request->m_connect) {
            request->m_done = true;
        }

        return;
    }

    switch (internetStatus) {
        case INTERNET_STATUS_RECEIVING_RESPONSE:
            if (!request->m_connected) {
                request->m_callback->ConnectedOK();
                request->m_connected = true;
            }

            break;

        case INTERNET_STATUS_RESPONSE_RECEIVED:
            SetEvent(request->m_dataEvent);

            if (!*static_cast<DWORD*>(statusInformation)) {
                request->m_done = true;
            }

            break;

        case INTERNET_STATUS_HANDLE_CREATED:
            request->m_request = reinterpret_cast<HINTERNET>(static_cast<INTERNET_ASYNC_RESULT*>(statusInformation)->dwResult);
            SetEvent(request->m_requestEvent);
            break;

        case INTERNET_STATUS_HANDLE_CLOSING:
            if (internet == request->m_connect) {
                SetEvent(request->m_dataEvent);
                SetEvent(request->m_completeEvent);
                request->m_callback->Completed(request->m_status);
                delete request;
            }

            break;

        case INTERNET_STATUS_REQUEST_COMPLETE:
            if (request->m_state == 2) {
                SetEvent(request->m_completeEvent);
            } else {
                request->m_state = 2;
                request->m_error = static_cast<INTERNET_ASYNC_RESULT*>(statusInformation)->dwError;
                Blizzard::Thread::CreateDetachedFromPool(nullptr, ReadResponseData, request, 0, 0);
            }

            break;
    }
}

// OFFSET: 0x00451420
static HINTERNET GetSession() {
    if (!s_session) {
        s_session = InternetOpenA("Blizzard Web Client", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, INTERNET_FLAG_ASYNC);

        if (s_session) {
            if (InternetSetStatusCallbackA(s_session, InternetCallback) == INTERNET_INVALID_STATUS_CALLBACK) {
                s_session = nullptr;
                return nullptr;
            }

            return s_session;
        }
    }

    return s_session;
}

// OFFSET: 0x00451470
bool Blizzard::URL::DownloadURL(const char* url, DownloadCallback* callback) {
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

    HttpRequest* request = new HttpRequest();

    if (!request) {
        return false;
    }

    request->m_callback = callback;
    request->m_session = GetSession();

    if (!request->m_session) {
        delete request;
        return false;
    }

    request->m_connect = ConnectServer(request, hostName, port);

    if (!request->m_connect) {
        delete request;
        return false;
    }

    const char* verb;
    if (callback->GetPostData()) {
        verb = "POST";
    } else if (callback->WantData()) {
        verb = "GET";
    } else {
        verb = "HEAD";
    }

    request->m_state = 1;
    request->m_request = HttpOpenRequestA(request->m_connect, verb, urlPath, nullptr, nullptr, nullptr, (components.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_FLAG_SECURE : 0) | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_UI, reinterpret_cast<DWORD_PTR>(request));

    if (!request->m_request) {
        if (GetLastError() != ERROR_IO_PENDING) {
            delete request;
            return false;
        }

        WaitForSingleObject(request->m_requestEvent, INFINITE);
    }

    char headers[1024];
    memset(headers, 0, sizeof(headers));

    char header[1024];

    const char* postData = callback->GetPostData();
    DWORD postDataLength = 0;

    if (postData) {
        if (callback->GetPostContentType()) {
            Blizzard::String::Format(header, sizeof(header), "Content-Type: %s\n\n", callback->GetPostContentType());
        } else {
            Blizzard::String::Format(header, sizeof(header), "Content-Type: text/html\n\n");
        }

        Blizzard::String::Append(headers, header, sizeof(headers));

        postDataLength = Blizzard::String::Length(postData);
        Blizzard::String::Format(header, sizeof(header), "Content-Length: %d\n", postDataLength);
        Blizzard::String::Append(headers, header, sizeof(headers));
    }

    const char* ifModifiedSince = callback->WantIfModifiedSince();

    if (ifModifiedSince) {
        Blizzard::String::Format(header, sizeof(header), "If-Modified-Since: %s\n", ifModifiedSince);
        Blizzard::String::Append(headers, header, sizeof(headers));
    }

    DownloadCallback::ByteRange range = callback->GetByteRange();

    if (range.m_start != -1 || range.m_end != -1) {
        Blizzard::String::Format(header, sizeof(header), "Range: bytes=%lld-%lld\n", range.m_start, range.m_end);
        Blizzard::String::Append(headers, header, sizeof(headers));
    }

    DownloadCallback::Cookie cookie = callback->GetCookie();

    if (cookie.m_name && *cookie.m_name && cookie.m_value) {
        InternetSetCookieA(Blizzard::StringConversion_CodepageToUTF8::ToNative(url).m_data, Blizzard::StringConversion_CodepageToUTF8::ToNative(cookie.m_name).m_data, Blizzard::StringConversion_CodepageToUTF8::ToNative(cookie.m_value).m_data);
    }

    if (!HttpSendRequestA(request->m_request, headers, static_cast<DWORD>(-1), const_cast<char*>(postData), postDataLength) && GetLastError() != ERROR_IO_PENDING) {
        delete request;
        return false;
    }

    return true;
}

