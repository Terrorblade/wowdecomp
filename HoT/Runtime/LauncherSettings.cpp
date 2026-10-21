#include "HoT/Runtime/LauncherSettings.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <utility>
#if defined(_WIN32)
#include <windows.h>
#else
#include "HoT/Streaming/Paths.h"
#endif
#include "HoT/Core/Log.h"

namespace fs = std::filesystem;

namespace HoT::LauncherSettings {

static std::once_flag s_once;
static std::mutex s_mutex;
static std::atomic<bool> s_hdPatch{ false };
static std::atomic<double> s_maxDownloadMBps{ 0.0 };
static std::atomic<int32_t> s_selectedRealmId{ 2 };
static std::string s_installDir;
static std::string s_launcherPath;

static fs::path SettingsPath() {
#if defined(_WIN32)
    char buffer[MAX_PATH];
    GetModuleFileNameA(nullptr, buffer, MAX_PATH);

    return fs::path(buffer).parent_path() / ".hotlauncher" / "settings.json";
#else
    return Streaming::Paths::WorkDir() / "settings.json";
#endif
}

static std::string ReadFile(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);

    if (!f) {
        return "";
    }

    std::ostringstream ss;
    ss << f.rdbuf();

    return ss.str();
}

// Position of the value for a top level key, or npos.
static size_t ValuePos(const std::string& json, const char* key) {
    std::string needle = std::string("\"") + key + "\"";
    size_t k = json.find(needle);

    if (k == std::string::npos) {
        return std::string::npos;
    }

    size_t c = json.find(':', k + needle.size());

    if (c == std::string::npos) {
        return std::string::npos;
    }

    size_t v = c + 1;

    while (v < json.size() && (json[v] == ' ' || json[v] == '\t' || json[v] == '\r' || json[v] == '\n')) {
        v++;
    }

    return v < json.size() ? v : std::string::npos;
}

static bool ReadBool(const std::string& json, const char* key, bool& out) {
    size_t v = ValuePos(json, key);

    if (v == std::string::npos) {
        return false;
    }

    out = json.compare(v, 4, "true") == 0;

    return true;
}

static bool ReadNumber(const std::string& json, const char* key, double& out) {
    size_t v = ValuePos(json, key);

    if (v == std::string::npos) {
        return false;
    }

    out = strtod(json.c_str() + v, nullptr);

    return true;
}

static void AppendUtf8(std::string& out, unsigned cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// The launcher escapes every non ASCII character as \uXXXX, this turns them back into UTF-8.
static bool ReadString(const std::string& json, const char* key, std::string& out) {
    size_t v = ValuePos(json, key);

    if (v == std::string::npos || json[v] != '"') {
        return false;
    }

    out.clear();

    for (size_t i = v + 1; i < json.size(); i++) {
        char c = json[i];

        if (c == '"') {
            return true;
        }

        if (c == '\\' && i + 1 < json.size()) {
            char e = json[++i];

            if (e == 'u' && i + 4 < json.size()) {
                unsigned cp = strtoul(json.substr(i + 1, 4).c_str(), nullptr, 16);
                i += 4;

                if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 < json.size() && json[i + 1] == '\\' && json[i + 2] == 'u') {
                    unsigned lo = strtoul(json.substr(i + 3, 4).c_str(), nullptr, 16);

                    if (lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        i += 6;
                    }
                }

                AppendUtf8(out, cp);
                continue;
            }

            c = e;
        }

        out += c;
    }

    return false;
}

// End of the value token at v, settings.json is flat so it is a quoted string or a bare literal.
static size_t ValueEnd(const std::string& json, size_t v) {
    if (json[v] == '"') {
        for (size_t i = v + 1; i < json.size(); i++) {
            if (json[i] == '\\') {
                i++;
            } else if (json[i] == '"') {
                return i + 1;
            }
        }

        return json.size();
    }

    size_t i = v;

    while (i < json.size() && json[i] != ',' && json[i] != '}' && json[i] != ' ' && json[i] != '\t' && json[i] != '\r' && json[i] != '\n') {
        i++;
    }

    return i;
}

// Rewrites one key in place so keys this build knows nothing about survive the write.
static std::string ReplaceValue(std::string json, const char* key, const std::string& raw) {
    size_t v = ValuePos(json, key);

    if (v != std::string::npos) {
        json.replace(v, ValueEnd(json, v) - v, raw);
        return json;
    }

    size_t open = json.find('{');

    if (open == std::string::npos) {
        return std::string("{\n  \"") + key + "\": " + raw + "\n}";
    }

    std::string entry = std::string("\n  \"") + key + "\": " + raw;
    size_t next = json.find_first_not_of(" \t\r\n", open + 1);

    if (next != std::string::npos && json[next] != '}') {
        entry += ",";
    }

    json.insert(open + 1, entry);

    return json;
}

static void LoadFromDisk() {
    fs::path path = SettingsPath();
    std::string json = ReadFile(path);

    if (json.empty()) {
        LOG_INFO << "launcher settings: none at " << path.string() << ", using defaults";
        return;
    }

    bool hdPatch = false;

    if (ReadBool(json, "hdPatch", hdPatch)) {
        s_hdPatch = hdPatch;
    }

    double mbps = 0.0;

    if (ReadNumber(json, "maxDownloadMBps", mbps) && mbps > 0.0) {
        s_maxDownloadMBps = mbps;
    }

    double realmId = 0.0;

    if (ReadNumber(json, "selectedRealmId", realmId) && realmId > 0.0) {
        s_selectedRealmId = static_cast<int32_t>(realmId);
    }

    ReadString(json, "installDir", s_installDir);
    ReadString(json, "launcherPath", s_launcherPath);

    LOG_INFO << "launcher settings: hdPatch=" << (s_hdPatch.load() ? "1" : "0") << " realm=" << s_selectedRealmId.load() << " maxMBps=" << s_maxDownloadMBps.load() << " launcher=" << (s_launcherPath.empty() ? "<unset>" : s_launcherPath);
}

static void Load() {
    std::call_once(s_once, LoadFromDisk);
}

static void Persist(const char* key, const std::string& rawValue) {
    std::lock_guard<std::mutex> lock(s_mutex);

    fs::path path = SettingsPath();
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);

    std::string json = ReadFile(path);

    if (json.find('{') == std::string::npos) {
        json = "{}";
    }

    json = ReplaceValue(std::move(json), key, rawValue);

    std::ofstream f(path, std::ios::trunc | std::ios::binary);

    if (!f) {
        LOG_ERROR << "launcher settings: could not write " << path.string();
        return;
    }

    f.write(json.data(), static_cast<std::streamsize>(json.size()));
    LOG_INFO << "launcher settings: wrote " << key << "=" << rawValue;
}

bool HdPatch() {
    Load();
    return s_hdPatch.load();
}

double MaxDownloadMBps() {
    Load();
    return s_maxDownloadMBps.load();
}

int64_t MaxDownloadBytesPerSecond() {
    double mbps = MaxDownloadMBps();

    return mbps > 0.0 ? static_cast<int64_t>(mbps * 1024 * 1024) : 0;
}

int32_t SelectedRealmId() {
    Load();
    return s_selectedRealmId.load();
}

std::string InstallDir() {
    Load();
    std::lock_guard<std::mutex> lock(s_mutex);

    return s_installDir;
}

std::string LauncherPath() {
    Load();
    std::lock_guard<std::mutex> lock(s_mutex);

    return s_launcherPath;
}

void SetHdPatch(bool enabled) {
    Load();

    if (s_hdPatch.exchange(enabled) == enabled) {
        return;
    }

    Persist("hdPatch", enabled ? "true" : "false");
}

void SetMaxDownloadMBps(double mbps) {
    if (mbps < 0.0) {
        mbps = 0.0;
    }

    Load();

    if (s_maxDownloadMBps.exchange(mbps) == mbps) {
        return;
    }

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%g", mbps);
    Persist("maxDownloadMBps", buffer);
}

}
