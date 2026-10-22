#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "Engine/Source/BlizzardCore/Source/System/URL/URL.h"
#include "Engine/Source/BlizzardCore/Source/System/String/String.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/Thread.h"
#include "Engine/Source/Os/Linux/OsPosix.h"

// The Windows client downloads through WinInet in async mode: DownloadURL cracks the URL and sends the request, and
// a pool thread (ReadResponseData) reads the response and feeds the callback, which gets Completed(status) once the
// connection is closed. This is the same flow over a plain HTTP/1.1 client on BSD sockets: DownloadURL validates the
// URL and hands the request to a pool thread that connects, sends, reads and calls Completed. WinInet's automatic
// redirect handling is kept (up to 5 hops). There is no TLS here: an https:// URL fails the way a WinInet request
// that cannot reach its server does (Completed(DOWNLOAD_CANNOT_CONNECT)).

namespace {

enum RequestError {
    REQUEST_OK = 0,
    REQUEST_FAILED,
    REQUEST_TIMEOUT,
    REQUEST_NAME_NOT_RESOLVED,
    REQUEST_CANNOT_CONNECT,
};

struct ParsedURL {
    bool m_secure;
    std::string m_host;
    uint16_t m_port;
    std::string m_path;
};

struct HttpJob {
    Blizzard::URL::HttpRequest* m_request;
    ParsedURL m_url;
    std::string m_fullUrl;
    std::string m_verb;
    std::string m_headers;
    std::string m_postData;
};

class Connection {
    public:
    int m_socket = -1;
    char m_buffer[16384];
    size_t m_start = 0;
    size_t m_end = 0;
    bool m_timedOut = false;

    ~Connection() {
        if (this->m_socket >= 0) {
            close(this->m_socket);
        }
    }

    // Returns the number of buffered bytes, refilling from the socket when the buffer is empty. 0 on EOF or error.
    size_t Fill() {
        if (this->m_start < this->m_end) {
            return this->m_end - this->m_start;
        }

        this->m_start = 0;
        this->m_end = 0;

        for (;;) {
            ssize_t count = recv(this->m_socket, this->m_buffer, sizeof(this->m_buffer), 0);

            if (count > 0) {
                this->m_end = static_cast<size_t>(count);
                return this->m_end;
            }

            if (count < 0 && errno == EINTR) {
                continue;
            }

            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                this->m_timedOut = true;
            }

            return 0;
        }
    }

    bool ReadLine(std::string& line) {
        line.clear();

        for (;;) {
            if (!this->Fill()) {
                return false;
            }

            char* begin = this->m_buffer + this->m_start;
            char* newline = static_cast<char*>(memchr(begin, '\n', this->m_end - this->m_start));

            if (newline) {
                line.append(begin, newline - begin);
                this->m_start += (newline - begin) + 1;

                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                return true;
            }

            line.append(begin, this->m_end - this->m_start);
            this->m_start = this->m_end;

            if (line.size() > 65536) {
                return false;
            }
        }
    }

    // Reads up to size bytes; returns 0 on EOF or error.
    size_t Read(char* out, size_t size) {
        size_t available = this->Fill();

        if (!available) {
            return 0;
        }

        size_t count = available < size ? available : size;
        memcpy(out, this->m_buffer + this->m_start, count);
        this->m_start += count;

        return count;
    }

    bool SendAll(const char* data, size_t size) {
        while (size) {
            ssize_t count = send(this->m_socket, data, size, MSG_NOSIGNAL);

            if (count < 0) {
                if (errno == EINTR) {
                    continue;
                }

                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    this->m_timedOut = true;
                }

                return false;
            }

            data += count;
            size -= static_cast<size_t>(count);
        }

        return true;
    }
};

bool ParseURL(const char* url, ParsedURL& out) {
    const char* separator = strstr(url, "://");

    if (!separator) {
        return false;
    }

    std::string scheme(url, separator - url);

    if (!strcasecmp(scheme.c_str(), "http")) {
        out.m_secure = false;
        out.m_port = 80;
    } else if (!strcasecmp(scheme.c_str(), "https")) {
        out.m_secure = true;
        out.m_port = 443;
    } else {
        return false;
    }

    const char* authority = separator + 3;
    const char* pathStart = authority + strcspn(authority, "/?#");
    std::string hostPort(authority, pathStart - authority);

    size_t at = hostPort.rfind('@');

    if (at != std::string::npos) {
        hostPort.erase(0, at + 1);
    }

    size_t colon = std::string::npos;

    if (!hostPort.empty() && hostPort[0] == '[') {
        size_t close = hostPort.find(']');

        if (close == std::string::npos) {
            return false;
        }

        out.m_host = hostPort.substr(1, close - 1);

        if (close + 1 < hostPort.size() && hostPort[close + 1] == ':') {
            colon = close + 1;
        }
    } else {
        colon = hostPort.find(':');
        out.m_host = hostPort.substr(0, colon);
    }

    if (colon != std::string::npos && colon + 1 < hostPort.size()) {
        int32_t port = atoi(hostPort.c_str() + colon + 1);

        if (port <= 0 || port > 65535) {
            return false;
        }

        out.m_port = static_cast<uint16_t>(port);
    }

    if (out.m_host.empty() || out.m_host.size() >= 1024) {
        return false;
    }

    const char* fragment = strchr(pathStart, '#');
    out.m_path.assign(pathStart, fragment ? fragment - pathStart : strlen(pathStart));

    if (out.m_path.empty() || out.m_path[0] != '/') {
        out.m_path.insert(0, "/");
    }

    return out.m_path.size() < 1024;
}

RequestError Connect(Connection& connection, const ParsedURL& url, int32_t timeout) {
    char port[16];
    snprintf(port, sizeof(port), "%u", url.m_port);

    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* addresses = nullptr;

    if (getaddrinfo(url.m_host.c_str(), port, &hints, &addresses) != 0 || !addresses) {
        return REQUEST_NAME_NOT_RESOLVED;
    }

    RequestError error = REQUEST_CANNOT_CONNECT;

    for (addrinfo* address = addresses; address; address = address->ai_next) {
        int fd = OsPosixSocket(address->ai_family, address->ai_socktype, address->ai_protocol, false);

        if (fd < 0) {
            continue;
        }

        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        bool connected = connect(fd, address->ai_addr, address->ai_addrlen) == 0;

        if (!connected && errno == EINPROGRESS) {
            pollfd pfd;
            pfd.fd = fd;
            pfd.events = POLLOUT;
            pfd.revents = 0;

            int ready;

            do {
                ready = poll(&pfd, 1, timeout > 0 ? timeout : -1);
            } while (ready < 0 && errno == EINTR);

            if (ready == 0) {
                error = REQUEST_TIMEOUT;
            } else if (ready > 0) {
                int socketError = 0;
                socklen_t length = sizeof(socketError);

                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &socketError, &length) == 0 && socketError == 0) {
                    connected = true;
                }
            }
        }

        if (connected) {
            fcntl(fd, F_SETFL, flags);

            if (timeout > 0) {
                timeval tv;
                tv.tv_sec = timeout / 1000;
                tv.tv_usec = (timeout % 1000) * 1000;
                setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
                setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            }

            connection.m_socket = fd;
            error = REQUEST_OK;
            break;
        }

        close(fd);
    }

    freeaddrinfo(addresses);

    return error;
}

const char* FindHeader(const std::string& line, const char* name) {
    size_t length = strlen(name);

    if (line.size() <= length || strncasecmp(line.c_str(), name, length) || line[length] != ':') {
        return nullptr;
    }

    const char* value = line.c_str() + length + 1;

    while (*value == ' ' || *value == '\t') {
        value++;
    }

    return value;
}

// Delivers body bytes to the callback the way ReadResponseData does; returns false once the download should end.
bool Deliver(Blizzard::URL::HttpRequest* request, const char* data, int32_t size) {
    if (!request->m_callback->ReceivedData(data, size)) {
        request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_ABORTED;
        request->m_done = true;
    }

    if (request->m_callback->Stop()) {
        request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_STOPPED;
        request->m_done = true;
    }

    return !request->m_done;
}

void ReadBody(Blizzard::URL::HttpRequest* request, Connection& connection, bool chunked, int64_t contentLength) {
    char data[16384];

    if (!chunked) {
        int64_t left = contentLength;

        while (left != 0) {
            size_t want = sizeof(data);

            if (left > 0 && static_cast<int64_t>(want) > left) {
                want = static_cast<size_t>(left);
            }

            size_t count = connection.Read(data, want);

            if (!count) {
                break;
            }

            if (left > 0) {
                left -= static_cast<int64_t>(count);
            }

            if (!Deliver(request, data, static_cast<int32_t>(count))) {
                return;
            }
        }

        request->m_done = true;
        return;
    }

    std::string line;

    for (;;) {
        if (!connection.ReadLine(line)) {
            break;
        }

        int64_t chunk = strtoll(line.c_str(), nullptr, 16);

        if (chunk <= 0) {
            break;
        }

        while (chunk > 0) {
            size_t want = static_cast<int64_t>(sizeof(data)) > chunk ? static_cast<size_t>(chunk) : sizeof(data);
            size_t count = connection.Read(data, want);

            if (!count) {
                request->m_done = true;
                return;
            }

            chunk -= static_cast<int64_t>(count);

            if (!Deliver(request, data, static_cast<int32_t>(count))) {
                return;
            }
        }

        if (!connection.ReadLine(line)) {
            break;
        }
    }

    request->m_done = true;
}

void SetErrorStatus(Blizzard::URL::HttpRequest* request, RequestError error) {
    switch (error) {
        case REQUEST_TIMEOUT:
            request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_TIMEOUT;
            break;

        case REQUEST_NAME_NOT_RESOLVED:
            request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_NAME_NOT_RESOLVED;
            break;

        case REQUEST_CANNOT_CONNECT:
            request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_CANNOT_CONNECT;
            break;

        default:
            request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_FAILED;
            break;
    }
}

void RunRequest(HttpJob* job) {
    Blizzard::URL::HttpRequest* request = job->m_request;
    int32_t timeout = request->m_callback->GetTimeout();
    ParsedURL url = job->m_url;

    for (int32_t redirect = 0;; redirect++) {
        if (url.m_secure) {
            SetErrorStatus(request, REQUEST_CANNOT_CONNECT);
            return;
        }

        Connection connection;
        RequestError error = Connect(connection, url, timeout);

        if (error != REQUEST_OK) {
            SetErrorStatus(request, error);
            return;
        }

        std::string message = job->m_verb + " " + url.m_path + " HTTP/1.1\r\nHost: " + url.m_host;

        if (url.m_port != 80) {
            char port[16];
            snprintf(port, sizeof(port), ":%u", url.m_port);
            message += port;
        }

        message += "\r\nUser-Agent: Blizzard Web Client\r\nAccept: */*\r\nCache-Control: no-cache\r\nPragma: no-cache\r\nConnection: close\r\n";
        message += job->m_headers;
        message += "\r\n";
        message += job->m_postData;

        if (!connection.SendAll(message.data(), message.size())) {
            SetErrorStatus(request, connection.m_timedOut ? REQUEST_TIMEOUT : REQUEST_FAILED);
            return;
        }

        std::string line;

        if (!connection.ReadLine(line)) {
            SetErrorStatus(request, connection.m_timedOut ? REQUEST_TIMEOUT : REQUEST_FAILED);
            return;
        }

        if (!request->m_connected) {
            request->m_callback->ConnectedOK();
            request->m_connected = true;
        }

        uint32_t statusCode = 0;
        const char* space = strchr(line.c_str(), ' ');

        if (!strncmp(line.c_str(), "HTTP/", 5) && space) {
            statusCode = static_cast<uint32_t>(atoi(space + 1));
        }

        std::string lastModified;
        std::string location;
        bool haveLength = false;
        int64_t contentLength = -1;
        bool chunked = false;

        for (;;) {
            if (!connection.ReadLine(line)) {
                SetErrorStatus(request, connection.m_timedOut ? REQUEST_TIMEOUT : REQUEST_FAILED);
                return;
            }

            if (line.empty()) {
                break;
            }

            const char* value;

            if ((value = FindHeader(line, "Last-Modified"))) {
                lastModified = value;
            } else if ((value = FindHeader(line, "Content-Length"))) {
                haveLength = true;
                contentLength = strtoll(value, nullptr, 10);
            } else if ((value = FindHeader(line, "Transfer-Encoding"))) {
                chunked = strcasestr(value, "chunked") != nullptr;
            } else if ((value = FindHeader(line, "Location"))) {
                location = value;
            }
        }

        bool redirected = statusCode == 301 || statusCode == 302 || statusCode == 303 || statusCode == 307 || statusCode == 308;

        if (redirected && !location.empty() && redirect < 5) {
            ParsedURL next;

            if (location[0] == '/') {
                next = url;
                next.m_path = location;
            } else if (!ParseURL(location.c_str(), next)) {
                request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_FAILED;
                return;
            }

            if (statusCode == 303) {
                job->m_verb = "GET";
                job->m_postData.clear();
            }

            url = next;
            continue;
        }

        request->m_callback->GetHTTPStatusCode(statusCode);

        if (statusCode >= 200 && statusCode < 300) {
            if (!lastModified.empty()) {
                request->m_callback->GetLastModified(lastModified.c_str());
            }

            if (haveLength) {
                request->m_callback->GetContentLength(static_cast<uint32_t>(contentLength));
            }

            if (!request->m_callback->WantData() || job->m_verb == "HEAD" || statusCode == 204) {
                request->m_done = true;
                return;
            }

            ReadBody(request, connection, chunked, chunked ? -1 : contentLength);
            return;
        }

        switch (statusCode) {
            case 304:
                request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_NOT_MODIFIED;
                break;

            case 400:
                request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_BAD_REQUEST;
                break;

            case 403:
                request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_FORBIDDEN;
                break;

            case 404:
                request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_NOT_FOUND;
                break;

            default:
                request->m_status = Blizzard::URL::DownloadCallback::DOWNLOAD_FAILED;
                break;
        }

        return;
    }
}

uint32_t ReadResponseData(void* param) {
    HttpJob* job = static_cast<HttpJob*>(param);
    Blizzard::URL::HttpRequest* request = job->m_request;

    request->m_state = 2;
    RunRequest(job);

    request->m_done = true;
    request->m_callback->Completed(request->m_status);
    delete request;
    delete job;

    return 0;
}

}

Blizzard::URL::HttpRequest::~HttpRequest() {
}

Blizzard::URL::HttpRequest::HttpRequest() {
    this->m_connectEvent = nullptr;
    this->m_requestEvent = nullptr;
    this->m_completeEvent = nullptr;
    this->m_dataEvent = nullptr;
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

bool Blizzard::URL::DownloadURL(const char* url, DownloadCallback* callback) {
    ParsedURL parsed;

    if (!url || !ParseURL(url, parsed)) {
        return false;
    }

    HttpRequest* request = new HttpRequest();
    request->m_callback = callback;

    HttpJob* job = new HttpJob();
    job->m_request = request;
    job->m_url = parsed;
    job->m_fullUrl = url;

    const char* postData = callback->GetPostData();

    if (postData) {
        job->m_verb = "POST";
    } else if (callback->WantData()) {
        job->m_verb = "GET";
    } else {
        job->m_verb = "HEAD";
    }

    char header[1024];

    if (postData) {
        if (callback->GetPostContentType()) {
            Blizzard::String::Format(header, sizeof(header), "Content-Type: %s\r\n", callback->GetPostContentType());
        } else {
            Blizzard::String::Format(header, sizeof(header), "Content-Type: text/html\r\n");
        }

        job->m_headers += header;

        uint32_t postDataLength = Blizzard::String::Length(postData);
        Blizzard::String::Format(header, sizeof(header), "Content-Length: %u\r\n", postDataLength);
        job->m_headers += header;
        job->m_postData.assign(postData, postDataLength);
    }

    const char* ifModifiedSince = callback->WantIfModifiedSince();

    if (ifModifiedSince) {
        Blizzard::String::Format(header, sizeof(header), "If-Modified-Since: %s\r\n", ifModifiedSince);
        job->m_headers += header;
    }

    DownloadCallback::ByteRange range = callback->GetByteRange();

    if (range.m_start != -1 || range.m_end != -1) {
        Blizzard::String::Format(header, sizeof(header), "Range: bytes=%lld-%lld\r\n", static_cast<long long>(range.m_start), static_cast<long long>(range.m_end));
        job->m_headers += header;
    }

    DownloadCallback::Cookie cookie = callback->GetCookie();

    if (cookie.m_name && *cookie.m_name && cookie.m_value) {
        job->m_headers += "Cookie: ";
        job->m_headers += cookie.m_name;
        job->m_headers += "=";
        job->m_headers += cookie.m_value;
        job->m_headers += "\r\n";
    }

    request->m_state = 1;

    if (!Blizzard::Thread::CreateDetachedFromPool(nullptr, ReadResponseData, job, 0, 0)) {
        delete job;
        delete request;
        return false;
    }

    return true;
}

#endif
