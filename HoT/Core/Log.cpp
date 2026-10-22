#include "HoT/Core/Log.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>

namespace HoT::Log {

#if defined(_DEBUG)
static Level s_level = LEVEL_DEBUG;
#else
static Level s_level = LEVEL_INFO;
#endif

static std::mutex s_mutex;
static FILE* s_file = nullptr;
static bool s_opened = false;

static const char* LevelName(Level level) {
    switch (level) {
        case LEVEL_ERROR:
            return "ERROR";
        case LEVEL_WARN:
            return "WARN";
        case LEVEL_INFO:
            return "INFO";
        default:
            return "DEBUG";
    }
}

static void EnsureOpen(const char* mode) {
    if (s_opened) {
        return;
    }

    s_opened = true;
    s_file = fopen("Logs/HoT.log", mode);
}

void Reset() {
    std::lock_guard<std::mutex> lock(s_mutex);

    if (s_file) {
        fclose(s_file);
        s_file = nullptr;
    }

    s_opened = false;
    EnsureOpen("w");
}

bool Enabled(Level level) {
    return level <= s_level;
}

void Write(Level level, const char* file, int line, const char* msg) {
    std::lock_guard<std::mutex> lock(s_mutex);
    EnsureOpen("a");

    if (!s_file) {
        return;
    }

    char stamp[32];
    time_t now = time(nullptr);
    strftime(stamp, sizeof(stamp), "%H:%M:%S", localtime(&now));

    const char* name = file ? strrchr(file, '\\') : nullptr;
    name = name ? name + 1 : (file ? file : "");

    fprintf(s_file, "%s %-5s %s:%d %s\n", stamp, LevelName(level), name, line, msg);
    fflush(s_file);
}

void Printf(Level level, const char* format, ...) {
    if (!Enabled(level)) {
        return;
    }

    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    Write(level, nullptr, 0, buffer);
}

}
