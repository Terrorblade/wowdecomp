#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include "Engine/Source/Os/OsURLDownload.h"
#include "Engine/Source/Storm/H/storm.h"

// WinINet is replaced by a minimal HTTP/1.0 client over BSD sockets on a worker thread, which keeps the Windows
// behaviour: DownloadString returns at once, the callback runs on the worker thread with the body in pieces (and a
// final empty piece), then once more with done set and the error code. Redirects are followed like WinINet does.
// Only http:// URLs are supported (no TLS library is linked); for any other scheme DownloadString returns false, which
// every caller handles as a failed download.

struct OsUrlRequest {
    OsUrlDownload m_download;
    char m_url[2048];
    int32_t m_timeout;
};

static bool ParseHttpUrl(const char* url, char* host, size_t hostSize, char* port, size_t portSize, char* path, size_t pathSize) {
    if (strncasecmp(url, "http://", 7)) {
        return false;
    }

    const char* start = url + 7;
    const char* end = start + strcspn(start, "/?#");
    const char* colon = static_cast<const char*>(memchr(start, ':', end - start));
    const char* hostEnd = colon ? colon : end;

    if (hostEnd == start || static_cast<size_t>(hostEnd - start) >= hostSize) {
        return false;
    }

    memcpy(host, start, hostEnd - start);
    host[hostEnd - start] = '\0';

    if (colon && end - colon > 1) {
        if (static_cast<size_t>(end - colon - 1) >= portSize) {
            return false;
        }

        memcpy(port, colon + 1, end - colon - 1);
        port[end - colon - 1] = '\0';
    } else {
        SStrCopy(port, "80", static_cast<uint32_t>(portSize));
    }

    const char* query = *end == '#' ? "" : end;
    size_t queryLength = strcspn(query, "#");

    if (queryLength + 2 > pathSize) {
        return false;
    }

    char* dst = path;

    if (*query != '/') {
        *dst++ = '/';
    }

    memcpy(dst, query, queryLength);
    dst[queryLength] = '\0';

    return true;
}

// Waits for the socket to become readable or writable; false on timeout or error. A timeout of 0 or less waits forever.
static bool WaitSocket(int sock, short events, int32_t timeout, bool* timedOut) {
    pollfd fd;
    fd.fd = sock;
    fd.events = events;
    fd.revents = 0;

    int result;

    do {
        result = poll(&fd, 1, timeout > 0 ? timeout : -1);
    } while (result < 0 && errno == EINTR);

    if (result == 0) {
        *timedOut = true;
    }

    return result > 0;
}

static int ConnectHost(const char* host, const char* port, int32_t timeout, bool* timedOut) {
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* list = nullptr;

    if (getaddrinfo(host, port, &hints, &list)) {
        return -1;
    }

    int sock = -1;

    for (addrinfo* ai = list; ai && sock < 0; ai = ai->ai_next) {
        sock = socket(ai->ai_family, ai->ai_socktype | SOCK_CLOEXEC | SOCK_NONBLOCK, ai->ai_protocol);

        if (sock < 0) {
            continue;
        }

        if (connect(sock, ai->ai_addr, ai->ai_addrlen) && errno != EINPROGRESS) {
            close(sock);
            sock = -1;
            continue;
        }

        int error = 0;
        socklen_t length = sizeof(error);

        if (!WaitSocket(sock, POLLOUT, timeout, timedOut) || getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &length) || error) {
            close(sock);
            sock = -1;
        }
    }

    freeaddrinfo(list);

    return sock;
}

static bool SendAll(int sock, const char* data, size_t size, int32_t timeout, bool* timedOut) {
    while (size) {
        ssize_t sent = send(sock, data, size, MSG_NOSIGNAL);

        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }

            if ((errno == EAGAIN || errno == EWOULDBLOCK) && WaitSocket(sock, POLLOUT, timeout, timedOut)) {
                continue;
            }

            return false;
        }

        data += sent;
        size -= static_cast<size_t>(sent);
    }

    return true;
}

// Reads up to size bytes; returns the count, 0 at end of stream, -1 on error or timeout.
static ssize_t RecvSome(int sock, char* data, size_t size, int32_t timeout, bool* timedOut) {
    while (true) {
        ssize_t received = recv(sock, data, size, 0);

        if (received >= 0) {
            return received;
        }

        if (errno == EINTR) {
            continue;
        }

        if ((errno == EAGAIN || errno == EWOULDBLOCK) && WaitSocket(sock, POLLIN, timeout, timedOut)) {
            continue;
        }

        return -1;
    }
}

// Finds a header's value in the response head (NUL-terminated, lines separated by CRLF).
static bool FindHeader(const char* head, const char* name, char* value, size_t valueSize) {
    size_t nameLength = strlen(name);

    for (const char* line = strstr(head, "\r\n"); line && line[2]; line = strstr(line + 2, "\r\n")) {
        const char* field = line + 2;

        if (!strncasecmp(field, name, nameLength) && field[nameLength] == ':') {
            const char* src = field + nameLength + 1;

            while (*src == ' ' || *src == '\t') {
                src++;
            }

            size_t length = strcspn(src, "\r\n");

            if (length >= valueSize) {
                return false;
            }

            memcpy(value, src, length);
            value[length] = '\0';

            return true;
        }
    }

    return false;
}

static void RunRequest(OsUrlRequest* request) {
    OsUrlDownload* download = &request->m_download;
    char data[16384];

    for (int32_t redirects = 0;; redirects++) {
        char host[1024];
        char port[16];
        char path[2048];
        bool timedOut = false;

        if (!ParseHttpUrl(request->m_url, host, sizeof(host), port, sizeof(port), path, sizeof(path))) {
            download->m_errorCode = HTTPError::Failed;
            return;
        }

        int sock = ConnectHost(host, port, request->m_timeout, &timedOut);

        if (sock < 0) {
            download->m_errorCode = timedOut ? HTTPError::Timeout : HTTPError::Failed;
            return;
        }

        char query[4096];
        int32_t length = SStrPrintf(query, sizeof(query), "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: Blizzard Web Client\r\nAccept: */*\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n", path, host);

        if (!SendAll(sock, query, length, request->m_timeout, &timedOut)) {
            close(sock);
            download->m_errorCode = timedOut ? HTTPError::Timeout : HTTPError::Failed;
            return;
        }

        // Read the status line and headers; any body bytes after them stay in data.
        size_t bytes = 0;
        char* body = nullptr;

        while (!body) {
            if (bytes >= sizeof(data) - 1) {
                break;
            }

            ssize_t received = RecvSome(sock, data + bytes, sizeof(data) - 1 - bytes, request->m_timeout, &timedOut);

            if (received <= 0) {
                break;
            }

            bytes += static_cast<size_t>(received);
            data[bytes] = '\0';
            body = strstr(data, "\r\n\r\n");
        }

        uint32_t statusCode = 0;

        if (!body || sscanf(data, "HTTP/%*u.%*u %u", &statusCode) != 1) {
            close(sock);
            download->m_errorCode = timedOut ? HTTPError::Timeout : HTTPError::Failed;
            return;
        }

        download->m_statusCode = statusCode;
        body[2] = '\0';
        body += 4;

        if (statusCode >= 300 && statusCode < 400 && statusCode != 304 && redirects < 5) {
            char location[2048];

            if (FindHeader(data, "Location", location, sizeof(location))) {
                close(sock);

                if (location[0] == '/') {
                    char base[2048];
                    SStrPrintf(base, sizeof(base), "http://%s:%s%s", host, port, location);
                    SStrCopy(request->m_url, base, sizeof(request->m_url));
                } else {
                    SStrCopy(request->m_url, location, sizeof(request->m_url));
                }

                continue;
            }
        }

        if (statusCode < 200 || statusCode >= 300) {
            close(sock);
            download->m_errorCode = statusCode == 404 ? HTTPError::NotFound : HTTPError::Failed;
            return;
        }

        size_t pending = data + bytes - body;
        memmove(data, body, pending);

        while (!download->m_done) {
            if (!pending) {
                ssize_t received = RecvSome(sock, data, sizeof(data), request->m_timeout, &timedOut);

                if (received < 0) {
                    // WinINet reports a failed read as the end of the data.
                    received = 0;
                }

                pending = static_cast<size_t>(received);
            }

            if (!pending) {
                download->m_done = true;
            }

            if (download->m_callback && !download->m_callback(download->m_param, data, static_cast<uint32_t>(pending), false, HTTPError::NoError)) {
                download->m_done = true;
            }

            pending = 0;
        }

        close(sock);
        return;
    }
}

static void* OsUrlDownloadThread(void* param) {
    OsUrlRequest* request = static_cast<OsUrlRequest*>(param);

    RunRequest(request);

    OsURLDownloadCallback callback = request->m_download.m_callback;
    void* callbackParam = request->m_download.m_param;
    HTTPError::ErrorCode errorCode = request->m_download.m_errorCode;

    delete request;

    if (callback) {
        callback(callbackParam, nullptr, 0, true, errorCode);
    }

    return nullptr;
}

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

bool OsUrlDownload::DownloadString(const char* url, OsURLDownloadCallback callback, void* param, int32_t timeout) {
    char host[1024];
    char port[16];
    char path[2048];

    if (!url || SStrLen(url) >= 2048 || !ParseHttpUrl(url, host, sizeof(host), port, sizeof(port), path, sizeof(path))) {
        return false;
    }

    OsUrlRequest* request = new (__FILE__, __LINE__) OsUrlRequest();
    SStrCopy(request->m_url, url, sizeof(request->m_url));
    request->m_timeout = timeout;
    request->m_download.m_callback = callback;
    request->m_download.m_param = param;
    request->m_download.m_state = 1;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_t thread;
    int result = pthread_create(&thread, &attr, OsUrlDownloadThread, request);
    pthread_attr_destroy(&attr);

    if (result) {
        delete request;
        return false;
    }

    return true;
}

bool OsURLDownload(const char* url, OsURLDownloadCallback callback, void* param) {
    return OsUrlDownload::DownloadString(url, callback, param, 5000);
}

#endif
