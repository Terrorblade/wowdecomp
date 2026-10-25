#include "HoT/Streaming/LauncherManifest.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace HoT::Streaming {

static std::string ReadFile(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);

    if (!f) {
        return "";
    }

    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void SkipWs(const std::string& s, size_t& i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) {
        i++;
    }
}

static bool ParseStr(const std::string& s, size_t& i, std::string& out) {
    SkipWs(s, i);

    if (i >= s.size() || s[i] != '"') {
        return false;
    }

    i++;
    out.clear();

    while (i < s.size()) {
        char c = s[i++];

        if (c == '"') {
            return true;
        }

        if (c != '\\') {
            out += c;
            continue;
        }

        if (i >= s.size()) {
            return false;
        }

        char e = s[i++];

        switch (e) {
            case '"':
                out += '"';
                break;
            case '\\':
                out += '\\';
                break;
            case '/':
                out += '/';
                break;
            case 'n':
                out += '\n';
                break;
            case 't':
                out += '\t';
                break;
            case 'r':
                out += '\r';
                break;
            case 'b':
                out += '\b';
                break;
            case 'f':
                out += '\f';
                break;
            case 'u': {
                if (i + 4 > s.size()) {
                    return false;
                }

                unsigned v = std::strtoul(s.substr(i, 4).c_str(), nullptr, 16);
                i += 4;
                out += (v < 0x80) ? static_cast<char>(v) : '?';
                break;
            }
            default:
                return false;
        }
    }

    return false;
}

static bool FindString(const std::string& s, const char* key, std::string& out) {
    std::string needle = std::string("\"") + key + "\"";
    size_t k = s.find(needle);

    if (k == std::string::npos) {
        return false;
    }

    size_t i = k + needle.size();
    SkipWs(s, i);

    if (i >= s.size() || s[i] != ':') {
        return false;
    }

    i++;
    return ParseStr(s, i, out);
}

// Walks the flat objects of the array under arrayKey, onValue returns true when it consumed the value at the given index.
template <typename OnObject, typename OnValue>
static bool ParseObjectArray(const std::string& json, const char* arrayKey, OnObject onObject, OnValue onValue) {
    std::string needle = std::string("\"") + arrayKey + "\"";
    size_t f = json.find(needle);

    if (f == std::string::npos) {
        return false;
    }

    size_t i = json.find('[', f);

    if (i == std::string::npos) {
        return false;
    }

    i++;

    while (true) {
        SkipWs(json, i);

        if (i >= json.size() || json[i] == ']') {
            break;
        }

        if (json[i] != '{') {
            i++;
            continue;
        }

        onObject();

        for (; i < json.size(); i++) {
            char c = json[i];

            if (c == '"') {
                size_t ks = i;
                std::string key;

                if (!ParseStr(json, ks, key)) {
                    return false;
                }

                size_t after = ks;
                SkipWs(json, after);

                if (after < json.size() && json[after] == ':') {
                    size_t v = after + 1;
                    SkipWs(json, v);

                    if (onValue(key, v)) {
                        i = v - 1;
                    } else {
                        // Unknown key, skip its name and let the loop continue.
                        i = ks - 1;
                    }
                } else {
                    i = ks - 1;
                }
            } else if (c == '}') {
                break;
            }
        }

        if (i < json.size()) {
            i++;
        }
    }

    return true;
}

bool ParseManifest(const std::string& json, Manifest& out) {
    FindString(json, "baseUrl", out.m_baseUrl);

    ManifestFile mf;

    auto flushFile = [&] {
        if (!mf.m_path.empty() && mf.m_requiredLevel <= kManifestLevel) {
            out.m_files.push_back(std::move(mf));
        }

        mf = ManifestFile();
    };

    bool ok = ParseObjectArray(json, "files", flushFile, [&](const std::string& key, size_t& v) {
        if (key == "path") {
            ParseStr(json, v, mf.m_path);
        } else if (key == "sha256") {
            ParseStr(json, v, mf.m_sha256);
        } else if (key == "quick") {
            ParseStr(json, v, mf.m_quick);
        } else if (key == "size") {
            mf.m_size = std::strtoll(json.c_str() + v, nullptr, 10);
        } else if (key == "hd") {
            mf.m_hd = (json.compare(v, 4, "true") == 0);
        } else if (key == "platform") {
            ParseStr(json, v, mf.m_platform);
        } else if (key == "requires") {
            mf.m_requiredLevel = std::atoi(json.c_str() + v);
        } else {
            return false;
        }

        return true;
    });

    flushFile();

    if (!ok) {
        return false;
    }

    ManifestArchive ma;

    auto flushArchive = [&] {
        if (!ma.m_path.empty() && !ma.m_contentId.empty()) {
            out.m_archives.push_back(std::move(ma));
        }

        ma = ManifestArchive();
    };

    ParseObjectArray(json, "archives", flushArchive, [&](const std::string& key, size_t& v) {
        if (key == "path") {
            ParseStr(json, v, ma.m_path);
        } else if (key == "contentId") {
            ParseStr(json, v, ma.m_contentId);
        } else {
            return false;
        }

        return true;
    });

    flushArchive();
    return true;
}

void LoadLauncherConfig(const std::wstring& installDir, LauncherConfig& out) {
    out.m_manifestUrl = L"https://download.houroftwilight.net/patch/manifest.json";
    out.m_patchBaseUrl = "https://download.houroftwilight.net/patch";
    out.m_launcherExe = "HoTLauncher.exe";

    std::string json = ReadFile(std::filesystem::path(installDir) / "launcher.json");

    if (json.empty()) {
        return;
    }

    std::string v;

    if (FindString(json, "manifestUrl", v) && !v.empty()) {
        out.m_manifestUrl.assign(v.begin(), v.end());
    }

    if (FindString(json, "patchBaseUrl", v) && !v.empty()) {
        out.m_patchBaseUrl = v;
    }

    if (FindString(json, "launcherExe", v) && !v.empty()) {
        out.m_launcherExe = v;
    }
}

}
