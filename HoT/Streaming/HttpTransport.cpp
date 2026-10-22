#include "HoT/Streaming/HttpClient.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
#include <curl/curl.h>
#include "HoT/Streaming/HttpTransport.h"
#include "HoT/Streaming/TextConv.h"

namespace HoT::Streaming {

// Each easy handle gets 30 s to connect, 30 s without data on an established transfer, redirects followed, proxy
// from the environment and no content decoding. libcurl copies string options, so locals passed to curl_easy_setopt
// need not outlive the call.
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
    // A redirect never goes from HTTPS down to HTTP.
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

// The wide path is native on Windows and converted to UTF-8 elsewhere, like TextConv does.
static std::filesystem::path FilePath(const std::wstring& path) {
    return std::filesystem::path(path);
}

struct StringSink {
    Conn* m_conn = nullptr;
    std::string* m_out = nullptr;
    bool m_started = false;
};

static size_t StringWrite(char* data, size_t size, size_t count, void* user) {
    StringSink* sink = static_cast<StringSink*>(user);
    size_t n = size * count;

    // A non 2xx body is never read.
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

}
