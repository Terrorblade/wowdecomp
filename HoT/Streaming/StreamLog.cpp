#include "HoT/Streaming/StreamLog.h"
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include "HoT/Core/Log.h"
#include "HoT/Platform/Process.h"
#include "HoT/Platform/Time.h"
#include "HoT/Streaming/Paths.h"

namespace HoT::Streaming {

static constexpr std::uintmax_t s_truncateAt = 1u << 20;

static std::mutex s_mutex;
static std::ofstream s_file;
static bool s_opened = false;

static std::string Stamp() {
    Platform::LocalTime t = Platform::LocalNow();
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d", t.m_year, t.m_month, t.m_day, t.m_hour, t.m_minute, t.m_second, t.m_millisecond);
    return buf;
}

static void EnsureOpen() {
    if (s_opened) {
        return;
    }

    s_opened = true;
    std::error_code ec;
    std::filesystem::path path = Paths::LogsDir() / "hotstream.log";
    std::filesystem::create_directories(path.parent_path(), ec);
    std::uintmax_t size = std::filesystem::file_size(path, ec);
    std::ios::openmode mode = std::ios::out | ((!ec && size > s_truncateAt) ? std::ios::trunc : std::ios::app);
    s_file.open(path, mode);

    if (s_file) {
        s_file << Stamp() << " === client start, pid " << Platform::CurrentProcessId() << "\n";
    }
}

void StreamLog(const char* format, ...) {
    char buf[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if (Log::Enabled(Log::LEVEL_DEBUG)) {
        Log::Write(Log::LEVEL_DEBUG, "stream", 0, buf);
    }

    std::lock_guard<std::mutex> lock(s_mutex);
    EnsureOpen();

    if (s_file) {
        s_file << Stamp() << ' ' << buf << '\n';
        s_file.flush();
    }
}

}
