#include "HoT/Streaming/LocalDigests.h"
#if defined(_WIN32)
#include <windows.h>
#endif
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <utility>
#include <vector>
#include "HoT/Streaming/MpqFile.h"
#include "HoT/Streaming/Paths.h"
#include "HoT/Streaming/Sha256.h"
#include "HoT/Streaming/TextConv.h"

namespace fs = std::filesystem;

namespace HoT::Streaming::LocalDigests {

static constexpr const char* s_header = "hotdigests1";
static constexpr const char* s_noContentId = "-";
static constexpr size_t s_kinds = 3;

struct Entry {
    long long m_size = -1;
    long long m_mtime = -1;
    std::string m_digests[s_kinds];
};

static std::mutex s_mutex;
static std::mutex s_computeMutex;
static std::mutex s_saveMutex;
static std::map<std::wstring, Entry> s_entries;
static bool s_loaded = false;
static bool s_dirty = false;

static fs::path CacheFile() {
    return Paths::WorkDir() / "hotstream-local.txt";
}

static void MigrateLegacyCacheFile() {
    const fs::path file = CacheFile();
    std::error_code ec;

    if (fs::exists(file, ec)) {
        return;
    }

    const fs::path legacy = Paths::LegacyCacheDir() / "hotstream-local.txt";

    if (!fs::exists(legacy, ec)) {
        return;
    }

    fs::create_directories(file.parent_path(), ec);
    fs::rename(legacy, file, ec);
}

static bool Stat(const std::wstring& path, long long& size, long long& mtime) {
    std::error_code ec;
    const auto bytes = fs::file_size(path, ec);

    if (ec) {
        return false;
    }

    const auto written = fs::last_write_time(path, ec);

    if (ec) {
        return false;
    }

    size = static_cast<long long>(bytes);
    mtime = static_cast<long long>(written.time_since_epoch().count());
    return true;
}

static std::vector<std::string> SplitFields(const std::string& line) {
    std::vector<std::string> fields;
    size_t start = 0;

    for (;;) {
        const size_t bar = line.find('|', start);

        if (bar == std::string::npos) {
            fields.push_back(line.substr(start));
            return fields;
        }

        fields.push_back(line.substr(start, bar - start));
        start = bar + 1;
    }
}

static Kind LegacyKind(const std::wstring& key, const std::string& digest) {
    if (IsQuickDigest(digest)) {
        return Kind::Quick;
    }

    const bool mpq = key.size() >= 4 && key.compare(key.size() - 4, 4, L".mpq") == 0;
    return mpq ? Kind::ContentId : Kind::Sha256;
}

static void LoadLocked() {
    s_loaded = true;
    MigrateLegacyCacheFile();
    std::ifstream in(CacheFile(), std::ios::binary);
    std::string line;
    bool first = true;
    bool legacy = true;

    while (in && std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (first) {
            first = false;

            if (line == s_header) {
                legacy = false;
                continue;
            }
        }

        const std::vector<std::string> f = SplitFields(line);

        if (f.size() != (legacy ? 4u : 3u + s_kinds) || f.back().empty()) {
            continue;
        }

        const std::wstring key = Text::LowerPath(legacy ? Text::WidenAcp(f.back()) : Text::WidenUtf8(f.back()));
        Entry e;
        e.m_size = _atoi64(f[0].c_str());
        e.m_mtime = _atoi64(f[1].c_str());

        if (legacy) {
            if (f[2].empty()) {
                continue;
            }

            e.m_digests[static_cast<size_t>(LegacyKind(key, f[2]))] = f[2];
            s_dirty = true;
        } else {
            for (size_t k = 0; k < s_kinds; k++) {
                e.m_digests[k] = f[2 + k];
            }
        }

        s_entries[key] = std::move(e);
    }
}

static bool Cached(const std::wstring& key, long long size, long long mtime, Kind kind, std::string& out) {
    std::lock_guard<std::mutex> lock(s_mutex);

    if (!s_loaded) {
        LoadLocked();
    }

    const auto it = s_entries.find(key);

    if (it == s_entries.end() || it->second.m_size != size || it->second.m_mtime != mtime) {
        return false;
    }

    const std::string& digest = it->second.m_digests[static_cast<size_t>(kind)];

    if (digest.empty()) {
        return false;
    }

    out = (digest == s_noContentId) ? std::string() : digest;
    return true;
}

static void Store(const std::wstring& key, long long size, long long mtime, Kind kind, const std::string& digest) {
    std::lock_guard<std::mutex> lock(s_mutex);

    if (!s_loaded) {
        LoadLocked();
    }

    Entry& e = s_entries[key];

    if (e.m_size != size || e.m_mtime != mtime) {
        e = Entry();
        e.m_size = size;
        e.m_mtime = mtime;
    }

    const std::string value = (kind == Kind::ContentId && digest.empty()) ? std::string(s_noContentId) : digest;

    if (e.m_digests[static_cast<size_t>(kind)] == value) {
        return;
    }

    e.m_digests[static_cast<size_t>(kind)] = value;
    s_dirty = true;
}

static bool Compute(const std::wstring& path, Kind kind, std::string& out) {
    switch (kind) {
        case Kind::Quick:
            out = QuickDigestFile(path);
            return !out.empty();
        case Kind::Sha256:
            out = Sha256File(path);
            return !out.empty();
        case Kind::ContentId:
            return MpqFile::ContentId(path, out);
    }

    return false;
}

bool Get(const std::wstring& path, Kind kind, std::string& out) {
    out.clear();
    long long size = 0;
    long long mtime = 0;

    if (!Stat(path, size, mtime)) {
        return false;
    }

    const std::wstring key = Text::LowerPath(path);

    if (Cached(key, size, mtime, kind, out)) {
        return true;
    }

    std::lock_guard<std::mutex> computing(s_computeMutex);

    if (Cached(key, size, mtime, kind, out)) {
        return true;
    }

    if (!Compute(path, kind, out)) {
        out.clear();
        return false;
    }

    Store(key, size, mtime, kind, out);
    return true;
}

void Remember(const std::wstring& path, Kind kind, const std::string& digest) {
    long long size = 0;
    long long mtime = 0;

    if (!digest.empty() && Stat(path, size, mtime)) {
        Store(Text::LowerPath(path), size, mtime, kind, digest);
    }
}

void Save() {
    std::lock_guard<std::mutex> saving(s_saveMutex);
    std::vector<std::pair<std::wstring, Entry>> snapshot;

    {
        std::lock_guard<std::mutex> lock(s_mutex);

        if (!s_dirty) {
            return;
        }

        snapshot.assign(s_entries.begin(), s_entries.end());
        s_dirty = false;
    }

    const fs::path file = CacheFile();
    const fs::path tmp = file.wstring() + L".tmp";
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);

    std::ofstream out(tmp, std::ios::trunc | std::ios::binary);

    if (out) {
        out << s_header << "\r\n";

        for (const auto& kv : snapshot) {
            const Entry& e = kv.second;
            out << e.m_size << "|" << e.m_mtime;

            for (const std::string& digest : e.m_digests) {
                out << "|" << digest;
            }

            out << "|" << Text::NarrowUtf8(kv.first) << "\r\n";
        }

        out.close();
    }

#if defined(_WIN32)
    if (out && MoveFileExW(tmp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        return;
    }
#else
    if (out) {
        fs::rename(tmp, file, ec);

        if (!ec) {
            return;
        }
    }
#endif

    fs::remove(tmp, ec);
    std::lock_guard<std::mutex> lock(s_mutex);
    s_dirty = true;
}

}
