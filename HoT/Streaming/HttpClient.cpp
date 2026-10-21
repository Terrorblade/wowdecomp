#include "HoT/Streaming/HttpClient.h"
#if defined(_WIN32)
#include <windows.h>
#include <winhttp.h>
#else
#include <curl/curl.h>
#include <filesystem>
#include "HoT/Streaming/TextConv.h"
#endif
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>
#include "HoT/Streaming/Sha256.h"

#if defined(_WIN32)
#pragma comment(lib, "winhttp.lib")
#endif

namespace HoT::Streaming {

#if defined(_WIN32)
static std::mutex s_sessionMutex;
static HINTERNET s_session = nullptr;

static HINTERNET SharedSession() {
    std::lock_guard<std::mutex> lock(s_sessionMutex);

    if (s_session) {
        return s_session;
    }

    s_session = WinHttpOpen(L"HoTStreaming/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

    if (!s_session) {
        s_session = WinHttpOpen(L"HoTStreaming/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }

    if (s_session) {
        WinHttpSetTimeouts(s_session, 15000, 15000, 30000, 30000);
    }

    return s_session;
}

struct Conn {
    HINTERNET m_connect = nullptr;
    HINTERNET m_request = nullptr;

    ~Conn() {
        if (this->m_request) {
            WinHttpCloseHandle(this->m_request);
        }

        if (this->m_connect) {
            WinHttpCloseHandle(this->m_connect);
        }
    }
};

static bool OpenGet(Conn& c, const std::wstring& url, long long resumeFrom) {
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {};
    wchar_t path[8192] = {};
    wchar_t extra[4096] = {};
    uc.lpszHostName = host;
    uc.dwHostNameLength = sizeof(host) / sizeof(host[0]);
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = sizeof(path) / sizeof(path[0]);
    uc.lpszExtraInfo = extra;
    uc.dwExtraInfoLength = sizeof(extra) / sizeof(extra[0]);

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) {
        return false;
    }

    const bool secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    std::wstring fullPath = std::wstring(path) + extra;

    HINTERNET session = SharedSession();

    if (!session) {
        return false;
    }

    c.m_connect = WinHttpConnect(session, host, uc.nPort, 0);

    if (!c.m_connect) {
        return false;
    }

    c.m_request = WinHttpOpenRequest(c.m_connect, L"GET", fullPath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);

    if (!c.m_request) {
        return false;
    }

    if (resumeFrom > 0) {
        wchar_t range[64];
        swprintf(range, sizeof(range) / sizeof(range[0]), L"Range: bytes=%lld-", resumeFrom);
        WinHttpAddRequestHeaders(c.m_request, range, static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
    }

    return true;
}

static bool SendAndStatus(Conn& c, DWORD& status) {
    if (!WinHttpSendRequest(c.m_request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        return false;
    }

    if (!WinHttpReceiveResponse(c.m_request, nullptr)) {
        return false;
    }

    DWORD size = sizeof(status);

    if (!WinHttpQueryHeaders(c.m_request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)) {
        return false;
    }

    return true;
}
#else
// The WinHTTP session settings carried over to each libcurl easy handle: resolve + connect 15 s each, 30 s without
// data on an established transfer, redirects followed, proxy from the environment, no content decoding.
// libcurl copies string options, so locals passed to curl_easy_setopt need not outlive the call.
static std::once_flag s_curlInit;

struct Conn {
    CURL* m_curl = nullptr;
    char m_error[CURL_ERROR_SIZE] = {};

    ~Conn() {
        if (this->m_curl) {
            curl_easy_cleanup(this->m_curl);
        }
    }
};

static bool OpenGet(Conn& c, const std::wstring& url, long long resumeFrom) {
    std::call_once(s_curlInit, []() { curl_global_init(CURL_GLOBAL_DEFAULT); });

    c.m_curl = curl_easy_init();

    if (!c.m_curl) {
        return false;
    }

    std::string narrowUrl = Text::NarrowUtf8(url);

    if (curl_easy_setopt(c.m_curl, CURLOPT_URL, narrowUrl.c_str()) != CURLE_OK) {
        return false;
    }

    curl_easy_setopt(c.m_curl, CURLOPT_PROTOCOLS_STR, "http,https");
    // WinHTTP's default redirect policy refuses to go from HTTPS down to HTTP.
    const bool secure = narrowUrl.size() >= 6 && Text::LowerAscii(narrowUrl.substr(0, 6)) == "https:";
    curl_easy_setopt(c.m_curl, CURLOPT_REDIR_PROTOCOLS_STR, secure ? "https" : "http,https");
    curl_easy_setopt(c.m_curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c.m_curl, CURLOPT_USERAGENT, "HoTStreaming/1.0");
    curl_easy_setopt(c.m_curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c.m_curl, CURLOPT_CONNECTTIMEOUT_MS, 30000L);
    curl_easy_setopt(c.m_curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(c.m_curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(c.m_curl, CURLOPT_ERRORBUFFER, c.m_error);

    if (resumeFrom > 0) {
        std::string range = std::to_string(resumeFrom) + "-";
        curl_easy_setopt(c.m_curl, CURLOPT_RANGE, range.c_str());
    }

    return true;
}

static uint32_t ResponseStatus(Conn& c) {
    long status = 0;
    curl_easy_getinfo(c.m_curl, CURLINFO_RESPONSE_CODE, &status);
    return static_cast<uint32_t>(status);
}

// libcurl wants a narrow path, libstdc++ converts a wide path to UTF-8 like TextConv does.
static std::filesystem::path FilePath(const std::wstring& path) {
    return std::filesystem::path(path);
}
#endif

static bool HashPrefix(const std::wstring& path, long long count, Sha256Stream& hash) {
#if defined(_WIN32)
    std::ifstream in(path, std::ios::binary);
#else
    std::ifstream in(FilePath(path), std::ios::binary);
#endif

    if (!in) {
        return false;
    }

    std::vector<char> buf(1 << 20);

    while (count > 0) {
        std::streamsize want = static_cast<std::streamsize>(std::min<long long>(count, static_cast<long long>(buf.size())));
        in.read(buf.data(), want);
        std::streamsize got = in.gcount();

        if (got <= 0) {
            return false;
        }

        hash.Update(buf.data(), static_cast<size_t>(got));
        count -= got;
    }

    return true;
}

DownloadThrottle::DownloadThrottle(std::function<long long()> bytesPerSecond) : m_bytesPerSecond(std::move(bytesPerSecond)), m_start(std::chrono::steady_clock::now()) {
}

void DownloadThrottle::Account(long long bytes) {
    double sleepSec = 0;

    {
        std::lock_guard<std::mutex> lock(this->m_mutex);
        long long cap = this->m_bytesPerSecond ? this->m_bytesPerSecond() : 0;

        if (cap != this->m_limit) {
            this->m_limit = cap;
            this->m_bytes = 0;
            this->m_start = std::chrono::steady_clock::now();
        }

        if (this->m_limit <= 0) {
            return;
        }

        this->m_bytes += bytes;
        double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - this->m_start).count();
        double aheadSec = static_cast<double>(this->m_bytes) / this->m_limit - elapsed;

        if (aheadSec > 0.005) {
            sleepSec = aheadSec < 0.5 ? aheadSec : 0.5;
        }
    }

    if (sleepSec > 0) {
        std::this_thread::sleep_for(std::chrono::duration<double>(sleepSec));
    }
}

#if defined(_WIN32)
bool HttpGetString(const std::wstring& url, std::string& out) {
    out.clear();
    Conn c;

    if (!OpenGet(c, url, 0)) {
        return false;
    }

    DWORD status = 0;

    if (!SendAndStatus(c, status) || status < 200 || status >= 300) {
        return false;
    }

    for (;;) {
        DWORD avail = 0;

        // A failure here is a connection error mid stream, zero is the clean end of the response.
        if (!WinHttpQueryDataAvailable(c.m_request, &avail)) {
            return false;
        }

        if (avail == 0) {
            break;
        }

        std::vector<char> buf(avail);
        DWORD read = 0;

        if (!WinHttpReadData(c.m_request, buf.data(), avail, &read)) {
            return false;
        }

        if (read == 0) {
            break;
        }

        out.append(buf.data(), read);
    }

    return true;
}

bool HttpDownloadFile(const std::wstring& url, const std::wstring& destPath, const DownloadOptions& opt) {
    if (opt.m_sha256Out) {
        opt.m_sha256Out->clear();
    }

    Conn c;

    if (!OpenGet(c, url, opt.m_resumeFrom)) {
        return false;
    }

    DWORD status = 0;

    if (!SendAndStatus(c, status)) {
        return false;
    }

    const bool resuming = (opt.m_resumeFrom > 0 && status == 206);

    if (status != 200 && status != 206) {
        return false;
    }

    std::unique_ptr<Sha256Stream> hash;

    if (opt.m_sha256Out) {
        hash = std::make_unique<Sha256Stream>();

        if (resuming && !HashPrefix(destPath, opt.m_resumeFrom, *hash)) {
            hash.reset();
        }
    }

    std::ofstream f(destPath, std::ios::binary | (resuming ? std::ios::app : std::ios::trunc));

    if (!f) {
        return false;
    }

    long long written = resuming ? opt.m_resumeFrom : 0;
    std::vector<char> buf(1 << 20);

    for (;;) {
        if (opt.m_cancel && opt.m_cancel()) {
            return false;
        }

        DWORD avail = 0;

        // A dropped connection must not pass off a truncated file.
        if (!WinHttpQueryDataAvailable(c.m_request, &avail)) {
            return false;
        }

        if (avail == 0) {
            break;
        }

        DWORD want = avail < buf.size() ? avail : static_cast<DWORD>(buf.size());
        DWORD read = 0;

        if (!WinHttpReadData(c.m_request, buf.data(), want, &read)) {
            return false;
        }

        if (read == 0) {
            break;
        }

        f.write(buf.data(), read);

        if (!f) {
            return false;
        }

        if (hash) {
            hash->Update(buf.data(), read);
        }

        written += read;

        if (opt.m_onBytes) {
            opt.m_onBytes(written);
        }

        if (opt.m_throttle) {
            opt.m_throttle->Account(read);
        }
    }

    f.close();

    if (!f) {
        return false;
    }

    if (hash) {
        *opt.m_sha256Out = hash->Finish();
    }

    return true;
}

#else
struct StringSink {
    Conn* m_conn = nullptr;
    std::string* m_out = nullptr;
    bool m_started = false;
};

static size_t StringWrite(char* data, size_t size, size_t count, void* user) {
    StringSink* sink = static_cast<StringSink*>(user);
    size_t n = size * count;

    // Like the WinHTTP path, a non 2xx body is never read.
    if (!sink->m_started) {
        uint32_t status = ResponseStatus(*sink->m_conn);

        if (status < 200 || status >= 300) {
            return 0;
        }

        sink->m_started = true;
    }

    sink->m_out->append(data, n);
    return n;
}

bool HttpGetString(const std::wstring& url, std::string& out) {
    out.clear();
    Conn c;

    if (!OpenGet(c, url, 0)) {
        return false;
    }

    StringSink sink;
    sink.m_conn = &c;
    sink.m_out = &out;
    curl_easy_setopt(c.m_curl, CURLOPT_WRITEFUNCTION, &StringWrite);
    curl_easy_setopt(c.m_curl, CURLOPT_WRITEDATA, &sink);

    // A transfer error covers both a failed request and a connection dropped mid stream.
    if (curl_easy_perform(c.m_curl) != CURLE_OK) {
        return false;
    }

    uint32_t status = ResponseStatus(c);

    if (status < 200 || status >= 300) {
        out.clear();
        return false;
    }

    return true;
}

struct FileSink {
    Conn* m_conn = nullptr;
    const std::wstring* m_destPath = nullptr;
    const DownloadOptions* m_opt = nullptr;
    std::unique_ptr<Sha256Stream> m_hash;
    std::ofstream m_file;
    long long m_written = 0;
    bool m_started = false;
    bool m_failed = false;
};

// Runs once the final response status is known, before any body byte is stored.
static bool BeginFile(FileSink& sink) {
    sink.m_started = true;
    const DownloadOptions& opt = *sink.m_opt;
    uint32_t status = ResponseStatus(*sink.m_conn);

    if (status != 200 && status != 206) {
        return false;
    }

    const bool resuming = (opt.m_resumeFrom > 0 && status == 206);

    if (opt.m_sha256Out) {
        sink.m_hash = std::make_unique<Sha256Stream>();

        if (resuming && !HashPrefix(*sink.m_destPath, opt.m_resumeFrom, *sink.m_hash)) {
            sink.m_hash.reset();
        }
    }

    sink.m_file.open(FilePath(*sink.m_destPath), std::ios::binary | (resuming ? std::ios::app : std::ios::trunc));

    if (!sink.m_file) {
        return false;
    }

    sink.m_written = resuming ? opt.m_resumeFrom : 0;
    return true;
}

static size_t FileWrite(char* data, size_t size, size_t count, void* user) {
    FileSink* sink = static_cast<FileSink*>(user);
    const DownloadOptions& opt = *sink->m_opt;
    size_t n = size * count;

    if (!sink->m_started && !BeginFile(*sink)) {
        sink->m_failed = true;
        return 0;
    }

    if (opt.m_cancel && opt.m_cancel()) {
        sink->m_failed = true;
        return 0;
    }

    // A failed write aborts the transfer, a truncated file must not pass.
    sink->m_file.write(data, static_cast<std::streamsize>(n));

    if (!sink->m_file) {
        sink->m_failed = true;
        return 0;
    }

    if (sink->m_hash) {
        sink->m_hash->Update(data, n);
    }

    sink->m_written += static_cast<long long>(n);

    if (opt.m_onBytes) {
        opt.m_onBytes(sink->m_written);
    }

    if (opt.m_throttle) {
        opt.m_throttle->Account(static_cast<long long>(n));
    }

    return n;
}

// Lets a cancel interrupt a stalled transfer instead of waiting for the next chunk.
static int FileProgress(void* user, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    FileSink* sink = static_cast<FileSink*>(user);
    const DownloadOptions& opt = *sink->m_opt;
    return (opt.m_cancel && opt.m_cancel()) ? 1 : 0;
}

bool HttpDownloadFile(const std::wstring& url, const std::wstring& destPath, const DownloadOptions& opt) {
    if (opt.m_sha256Out) {
        opt.m_sha256Out->clear();
    }

    Conn c;

    if (!OpenGet(c, url, opt.m_resumeFrom)) {
        return false;
    }

    FileSink sink;
    sink.m_conn = &c;
    sink.m_destPath = &destPath;
    sink.m_opt = &opt;
    curl_easy_setopt(c.m_curl, CURLOPT_WRITEFUNCTION, &FileWrite);
    curl_easy_setopt(c.m_curl, CURLOPT_WRITEDATA, &sink);
    curl_easy_setopt(c.m_curl, CURLOPT_XFERINFOFUNCTION, &FileProgress);
    curl_easy_setopt(c.m_curl, CURLOPT_XFERINFODATA, &sink);
    curl_easy_setopt(c.m_curl, CURLOPT_NOPROGRESS, 0L);

    if (curl_easy_perform(c.m_curl) != CURLE_OK || sink.m_failed) {
        return false;
    }

    // An empty body never reaches the write callback, the file is still created or truncated.
    if (!sink.m_started) {
        if (!BeginFile(sink)) {
            return false;
        }

        if (opt.m_cancel && opt.m_cancel()) {
            return false;
        }
    }

    sink.m_file.close();

    if (!sink.m_file) {
        return false;
    }

    if (sink.m_hash) {
        *opt.m_sha256Out = sink.m_hash->Finish();
    }

    return true;
}
#endif
}
