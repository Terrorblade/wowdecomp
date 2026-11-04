#include "HoT/Streaming/Channels.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/SFileArchives.h"
#include "HoT/Core/Modules.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "HoT/Platform/Files.h"
#include "HoT/Platform/Time.h"
#include "HoT/Runtime/Cvars.h"
#include "HoT/Streaming/ArchiveManifest.h"
#include "HoT/Streaming/ArchiveRegistry.h"
#include "HoT/Streaming/BackgroundDownloader.h"
#include "HoT/Streaming/MpqFile.h"
#include "HoT/Streaming/Paths.h"
#include "HoT/Streaming/StreamLog.h"
#include "HoT/Streaming/TextConv.h"
#include "WoW/Source/Net/Types.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

namespace fs = std::filesystem;

namespace HoT::Streaming::Channels {

static constexpr uint32_t s_mountFlags = 0xC00;
static constexpr uint64_t s_kickIntervalMs = 30000;

static std::mutex s_stateMutex;
static std::string s_lastChannel = s_release;
static std::vector<std::string> s_seen = { s_release };
static std::string s_startupBinaryChannel;
static bool s_stateLoaded = false;
static std::string s_requestedShared = s_release;

static std::string s_realmChannel = s_release;
static bool s_realmAnswered = false;

static std::mutex s_layoutMutex;
static std::vector<LayoutEntry> s_layout;
static bool s_layoutKnown = false;
static bool s_devCurrent = false;
static unsigned s_layoutGeneration = 0;
static std::atomic<int32_t> s_devQueued{ 0 };

static std::string s_mounted = s_release;
static unsigned s_appliedGeneration = 0;
static bool s_resyncPending = false;
static uint64_t s_lastKickMs = 0;
static std::map<std::wstring, SArchive*> s_devOnlyMounts;
static std::vector<std::string> s_deferredMounts;
static std::vector<SArchive*> s_releaseMounts;

static std::mutex s_reportMutex;
static std::map<std::wstring, std::wstring> s_reportMap;
static std::vector<std::wstring> s_extraMounts;

std::string Normalize(const std::string& name) {
    return Text::LowerAscii(name) == s_dev ? s_dev : s_release;
}

static fs::path StateFile() {
    return Paths::WorkDir() / "channels.json";
}

static void SkipWs(const std::string& s, size_t& i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) {
        i++;
    }
}

static bool ReadJsonString(const std::string& s, size_t& i, std::string& out) {
    SkipWs(s, i);

    if (i >= s.size() || s[i] != '"') {
        return false;
    }

    out.clear();

    for (i++; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            out += s[++i];
            continue;
        }

        if (s[i] == '"') {
            i++;
            return true;
        }

        out += s[i];
    }

    return false;
}

static bool FindKey(const std::string& s, const char* key, size_t& i) {
    size_t k = s.find(std::string("\"") + key + "\"");

    if (k == std::string::npos) {
        return false;
    }

    i = k + std::strlen(key) + 2;
    SkipWs(s, i);

    if (i >= s.size() || s[i] != ':') {
        return false;
    }

    i++;
    return true;
}

static void LoadStateLocked() {
    if (s_stateLoaded) {
        return;
    }

    s_stateLoaded = true;
    std::ifstream f(StateFile(), std::ios::binary);
    std::string json;

    if (f) {
        json.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }

    size_t i = 0;
    std::string value;

    if (FindKey(json, "lastChannel", i) && ReadJsonString(json, i, value)) {
        s_lastChannel = Normalize(value);
    }

    if (FindKey(json, "seen", i)) {
        SkipWs(json, i);

        if (i < json.size() && json[i] == '[') {
            i++;

            for (;;) {
                SkipWs(json, i);

                if (i >= json.size() || json[i] == ']' || !ReadJsonString(json, i, value)) {
                    break;
                }

                std::string name = Normalize(value);

                if (std::find(s_seen.begin(), s_seen.end(), name) == s_seen.end()) {
                    s_seen.push_back(name);
                }

                SkipWs(json, i);

                if (i < json.size() && json[i] == ',') {
                    i++;
                }
            }
        }
    }

}

static void SaveStateLocked() {
    std::string json = "{ \"lastChannel\": \"" + s_lastChannel + "\", \"seen\": [";

    for (size_t i = 0; i < s_seen.size(); i++) {
        json += (i ? ", \"" : "\"") + s_seen[i] + "\"";
    }

    json += "] }\n";

    std::error_code ec;
    fs::create_directories(StateFile().parent_path(), ec);
    fs::path tmp = StateFile();
    tmp += ".tmp";

    {
        std::ofstream f(tmp, std::ios::trunc | std::ios::binary);

        if (!f || !f.write(json.data(), static_cast<std::streamsize>(json.size()))) {
            StreamLog("channels: cannot write %s", Text::NarrowAcp(tmp.wstring()).c_str());
            return;
        }
    }

    if (!Platform::RenameReplacing(tmp, StateFile())) {
        StreamLog("channels: cannot replace channels.json");
    }
}

static bool Record(const std::string& channel) {
    std::lock_guard<std::mutex> lock(s_stateMutex);
    LoadStateLocked();
    bool changed = s_lastChannel != channel;
    s_lastChannel = channel;

    if (std::find(s_seen.begin(), s_seen.end(), channel) == s_seen.end()) {
        s_seen.push_back(channel);
        changed = true;
    }

    if (changed) {
        SaveStateLocked();
    }

    return changed;
}

std::string LastChannel() {
    std::lock_guard<std::mutex> lock(s_stateMutex);
    LoadStateLocked();
    return s_lastChannel;
}

std::string Override() {
    std::string value = Text::LowerAscii(PatchChannelCvar::Value());
    return (value == s_release || value == s_dev) ? value : "";
}

bool WantsDev() {
    if (Override() == s_dev) {
        return true;
    }

    std::lock_guard<std::mutex> lock(s_stateMutex);
    LoadStateLocked();
    return s_requestedShared == s_dev || s_lastChannel == s_dev || std::find(s_seen.begin(), s_seen.end(), s_dev) != s_seen.end();
}

std::string BinaryChannel() {
    std::string forced = Override();
    return forced.empty() ? LastChannel() : forced;
}

bool BinaryChannelChanged() {
    std::string current = BinaryChannel();
    std::lock_guard<std::mutex> lock(s_stateMutex);
    return !s_startupBinaryChannel.empty() && current != s_startupBinaryChannel;
}

std::wstring DevManifestUrl(const std::wstring& releaseUrl) {
    static const std::wstring s_channelsDir = L"/channels/";
    static const std::wstring s_devName = L"channels/dev/manifest.json";
    std::wstring lower = releaseUrl;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) { return c >= L'A' && c <= L'Z' ? static_cast<wchar_t>(c + 32) : c; });
    size_t query = releaseUrl.find(L'?');
    size_t end = query == std::wstring::npos ? releaseUrl.size() : query;
    std::wstring tail = releaseUrl.substr(end);
    size_t slash = end == 0 ? std::wstring::npos : releaseUrl.find_last_of(L'/', end - 1);
    std::wstring dir = slash == std::wstring::npos ? L"" : releaseUrl.substr(0, slash + 1);
    size_t parentSlash = slash == std::wstring::npos || slash == 0 ? std::wstring::npos : releaseUrl.find_last_of(L'/', slash - 1);

    if (parentSlash != std::wstring::npos && parentSlash + 1 >= s_channelsDir.size() &&
        lower.compare(parentSlash + 1 - s_channelsDir.size(), s_channelsDir.size(), s_channelsDir) == 0) {
        return releaseUrl.substr(0, parentSlash + 1) + L"dev/manifest.json" + tail;
    }

    return dir + s_devName + tail;
}

fs::path DevRoot() {
    return Paths::WorkDir() / "channels" / "dev";
}

std::wstring DevPath(const std::string& relPath) {
    return (DevRoot() / Platform::HostPath(relPath)).wstring();
}

std::wstring DevPartPath(const std::string& relPath) {
    return (Paths::DownloadsDir() / "channels" / "dev" / Platform::HostPath(relPath)).wstring() + L".part";
}

static fs::path StubPath() {
    return Paths::WorkDir() / "channels" / "empty.MPQ";
}

static bool EnsureStub() {
    std::error_code ec;
    const fs::path stub = StubPath();

    if (fs::file_size(stub, ec) > 0 && !ec) {
        return true;
    }

    fs::create_directories(stub.parent_path(), ec);
    std::string name = Text::NarrowAcp(stub.wstring());
    System_SFile::ARCHIVEREC* raw = nullptr;

    if (!Blizzard::Mopaq::SFileCreateArchive(name.c_str(), 0, 0, &raw, nullptr) || !raw) {
        StreamLog("channels: cannot create the empty stub archive %s", name.c_str());
        return false;
    }

    Blizzard::Mopaq::SFileCloseArchive(raw);
    ec.clear();
    return fs::file_size(stub, ec) > 0 && !ec;
}

void PublishLayout(std::vector<LayoutEntry> layout, bool devCurrent) {
    std::lock_guard<std::mutex> lock(s_layoutMutex);
    s_layout = std::move(layout);
    s_layoutKnown = true;
    s_devCurrent = devCurrent;
    s_layoutGeneration++;
}

void NoteDevQueued(int32_t delta) {
    s_devQueued.fetch_add(delta);
}

void NoteDevFailed() {
    std::lock_guard<std::mutex> lock(s_layoutMutex);
    s_devCurrent = false;
}

static bool NeedsPass() {
    std::lock_guard<std::mutex> lock(s_layoutMutex);
    return !s_layoutKnown || !s_devCurrent;
}

static std::string Requested() {
    std::string forced = Override();
    return forced.empty() ? s_realmChannel : forced;
}

std::string Mounted() {
    return s_mounted;
}

bool IsReady() {
    std::string requested = Requested();

    if (requested == s_mounted) {
        return true;
    }

    return requested == s_dev && !IsEnabled();
}

static std::wstring KeyOf(const std::wstring& path) {
    return ArchiveRegistry::KeyFor(path);
}

static bool IsStub(const std::wstring& key) {
    return key == KeyOf(StubPath().wstring());
}

bool DeferMount(const std::string& openName) {
    if (s_mounted != s_dev) {
        return false;
    }

    const std::wstring key = KeyOf(Text::WidenAcp(openName));
    bool masked = false;

    {
        std::lock_guard<std::mutex> lock(s_layoutMutex);

        for (const LayoutEntry& e : s_layout) {
            masked = masked || (e.m_inRelease && e.m_differs && KeyOf(e.m_releasePath) == key);
        }
    }

    if (masked) {
        s_deferredMounts.push_back(openName);
    }

    return masked;
}

bool ReportedPath(const std::wstring& dataPath, std::wstring& out) {
    std::lock_guard<std::mutex> lock(s_reportMutex);
    auto it = s_reportMap.find(KeyOf(dataPath));

    if (it == s_reportMap.end()) {
        out = dataPath;
        return true;
    }

    out = it->second;
    return !out.empty();
}

std::vector<std::wstring> ExtraMounts() {
    std::lock_guard<std::mutex> lock(s_reportMutex);
    return s_extraMounts;
}

static bool IsStagedFor(const ArchiveMount& m, const LayoutEntry& e) {
    fs::path original(Text::WidenAcp(m.m_originalName));
    fs::path release(e.m_releasePath);
    std::wstring name = original.filename().wstring();

    if (!IsStagedName(name)) {
        return false;
    }

    std::wstring prefix = Text::LowerPath(release.stem().wstring() + L".new-");

    if (Text::LowerPath(name).compare(0, prefix.size(), prefix) != 0) {
        return false;
    }

    std::wstring dir = KeyOf(original.parent_path().wstring());
    fs::path staged = Paths::StagedDir() / Platform::HostPath(e.m_relPath).parent_path();
    return dir == KeyOf(staged.wstring()) || dir == KeyOf(release.parent_path().wstring());
}

static const LayoutEntry* EntryFor(const std::vector<LayoutEntry>& layout, const ArchiveMount& m) {
    for (const LayoutEntry& e : layout) {
        if (e.m_inRelease && (KeyOf(e.m_releasePath) == m.m_originalKey || IsStagedFor(m, e))) {
            return &e;
        }
    }

    return nullptr;
}

static bool ReadEntries(const std::wstring& path, std::map<std::string, std::string>& out) {
    std::string bytes;
    std::vector<ArchiveEntry> entries;

    if (!MpqFile::ReadFile(path, ArchiveManifest::s_fileName, bytes) || !ArchiveManifest::Parse(bytes, entries)) {
        return false;
    }

    for (const ArchiveEntry& e : entries) {
        out[e.m_name] = e.m_sha256;
    }

    return true;
}

struct NameSet {
    std::set<std::string> m_seen;
    std::vector<std::string> m_names;

    void Add(const std::string& name) {
        if (this->m_seen.insert(Text::LowerAscii(name)).second) {
            this->m_names.push_back(name);
        }
    }
};

static void AddDifferences(const std::map<std::string, std::string>& a, const std::map<std::string, std::string>& b, NameSet& names) {
    std::map<std::string, std::string> lowerB;

    for (const auto& kv : b) {
        lowerB[Text::LowerAscii(kv.first)] = kv.second;
    }

    std::set<std::string> matched;

    for (const auto& kv : a) {
        std::string key = Text::LowerAscii(kv.first);
        auto it = lowerB.find(key);

        if (it == lowerB.end() || _stricmp(it->second.c_str(), kv.second.c_str()) != 0) {
            names.Add(kv.first);
        }

        matched.insert(key);
    }

    for (const auto& kv : b) {
        if (matched.find(Text::LowerAscii(kv.first)) == matched.end()) {
            names.Add(kv.first);
        }
    }
}

static void AddAll(SArchive* archive, NameSet& names) {
    std::vector<std::string> list;
    CollectMountedNames(archive, list);

    for (const std::string& n : list) {
        names.Add(n);
    }
}

static bool RetargetCollecting(const ArchiveMount& m, const std::string& want, NameSet& names) {
    const std::wstring from = Text::WidenAcp(m.m_openName);
    const std::wstring to = Text::WidenAcp(want);
    std::map<std::string, std::string> before;
    std::map<std::string, std::string> after;
    const bool fromStub = IsStub(m.m_key);
    const bool toStub = IsStub(KeyOf(to));
    const bool haveBefore = fromStub || ReadEntries(from, before);
    const bool haveAfter = toStub || ReadEntries(to, after);

    if (!haveBefore || !haveAfter) {
        AddAll(m.m_archive, names);
    }

    std::string error;

    if (!ArchiveRegistry::Retarget(m.m_archive, want, &error)) {
        StreamLog("channels: switching %s to %s failed: %s", m.m_openName.c_str(), want.c_str(), error.c_str());
        return false;
    }

    if (haveBefore && haveAfter) {
        AddDifferences(before, after, names);
    } else {
        AddAll(m.m_archive, names);
    }

    StreamLog("channels: %s now reads %s", m.m_originalName.c_str(), want.c_str());
    return true;
}

static bool DevCopiesPresent(const std::vector<LayoutEntry>& layout) {
    std::error_code ec;

    for (const LayoutEntry& e : layout) {
        if (e.m_inDev && e.m_differs && !fs::exists(e.m_devPath, ec)) {
            return false;
        }
    }

    return true;
}

static bool CanApplyDev() {
    std::lock_guard<std::mutex> lock(s_layoutMutex);
    return s_layoutKnown && s_devCurrent && s_devQueued.load() == 0 && DevCopiesPresent(s_layout);
}

static void UpdateReport(const std::vector<LayoutEntry>& layout) {
    std::map<std::wstring, std::wstring> report;
    std::vector<std::wstring> extra;

    for (const ArchiveMount& m : ArchiveRegistry::All()) {
        if (s_devOnlyMounts.find(m.m_originalKey) != s_devOnlyMounts.end()) {
            const LayoutEntry* standIn = nullptr;

            for (const LayoutEntry& e : layout) {
                if (e.m_inRelease && KeyOf(e.m_devPath) == m.m_originalKey) {
                    standIn = &e;
                }
            }

            if (standIn) {
                report[KeyOf(standIn->m_releasePath)] = Text::WidenAcp(m.m_openName);
            } else {
                extra.push_back(Text::WidenAcp(m.m_openName));
            }

            continue;
        }

        if (m.m_key == m.m_originalKey) {
            continue;
        }

        const LayoutEntry* e = EntryFor(layout, m);
        std::wstring& slot = report[e ? KeyOf(e->m_releasePath) : m.m_originalKey];

        if (!IsStub(m.m_key)) {
            slot = Text::WidenAcp(m.m_openName);
        }
    }

    std::lock_guard<std::mutex> lock(s_reportMutex);
    s_reportMap.swap(report);
    s_extraMounts.swap(extra);
}

static void MountDeferred(NameSet& names, int32_t& switched) {
    for (const std::string& path : s_deferredMounts) {
        int32_t priority = NextMountPriority();
        SArchive* archive = nullptr;

        if (!SFile::OpenArchive(path.c_str(), priority, s_mountFlags, &archive) || !archive) {
            StreamLog("channels: deferred mount %s FAILED", path.c_str());
            continue;
        }

        StreamLog("channels: deferred mount %s (prio %d)", path.c_str(), priority);
        s_releaseMounts.push_back(archive);
        AddAll(archive, names);
        switched++;
    }

    s_deferredMounts.clear();
}

static void Apply(const std::string& target) {
    std::vector<LayoutEntry> layout;
    unsigned generation;

    {
        std::lock_guard<std::mutex> lock(s_layoutMutex);
        layout = s_layout;
        generation = s_layoutGeneration;
    }

    const bool toDev = target == s_dev;
    const std::string stub = Text::NarrowAcp(StubPath().wstring());
    const bool haveStub = !toDev || EnsureStub();
    std::vector<ArchiveMount> mounts = ArchiveRegistry::All();
    std::map<const LayoutEntry*, const ArchiveMount*> top;

    for (const ArchiveMount& m : mounts) {
        if (s_devOnlyMounts.find(m.m_originalKey) != s_devOnlyMounts.end()) {
            continue;
        }

        const LayoutEntry* e = EntryFor(layout, m);

        if (e && (!top[e] || top[e]->m_priority < m.m_priority)) {
            top[e] = &m;
        }
    }

    NameSet names;
    int32_t switched = 0;
    std::error_code ec;

    for (const ArchiveMount& m : mounts) {
        if (s_devOnlyMounts.find(m.m_originalKey) != s_devOnlyMounts.end()) {
            continue;
        }

        const LayoutEntry* e = EntryFor(layout, m);
        std::string want = m.m_originalName;

        if (toDev && e && e->m_differs) {
            if (e->m_inDev && top[e] == &m) {
                if (fs::exists(e->m_devPath, ec)) {
                    want = Text::NarrowAcp(e->m_devPath);
                } else {
                    want = m.m_openName;
                }
            } else if (haveStub) {
                want = stub;
            }
        }

        if (KeyOf(Text::WidenAcp(want)) == m.m_key) {
            continue;
        }

        if (RetargetCollecting(m, want, names)) {
            switched++;
        }
    }

    std::set<std::wstring> wantedDevOnly;

    for (const LayoutEntry& e : layout) {
        auto wrapper = top.find(&e);
        const bool unmounted = !e.m_inRelease || wrapper == top.end() || !wrapper->second;

        if (toDev && e.m_inDev && e.m_differs && unmounted && fs::exists(e.m_devPath, ec)) {
            wantedDevOnly.insert(KeyOf(e.m_devPath));
        }
    }

    for (auto it = s_devOnlyMounts.begin(); it != s_devOnlyMounts.end();) {
        if (wantedDevOnly.find(it->first) != wantedDevOnly.end()) {
            it++;
            continue;
        }

        AddAll(it->second, names);
        StreamLog("channels: closing dev-only %s", Text::NarrowAcp(it->first).c_str());
        SFile::CloseArchive(it->second);
        it = s_devOnlyMounts.erase(it);
        switched++;
    }

    for (const LayoutEntry& e : layout) {
        const std::wstring key = KeyOf(e.m_devPath);

        if (wantedDevOnly.find(key) == wantedDevOnly.end() || s_devOnlyMounts.find(key) != s_devOnlyMounts.end()) {
            continue;
        }

        std::string path = Text::NarrowAcp(e.m_devPath);
        int32_t priority = NextMountPriority();
        SArchive* archive = nullptr;

        if (!SFile::OpenArchive(path.c_str(), priority, s_mountFlags, &archive) || !archive) {
            StreamLog("channels: mounting dev-only %s FAILED", path.c_str());
            continue;
        }

        StreamLog("channels: mounted dev-only %s (prio %d)", path.c_str(), priority);
        s_devOnlyMounts[key] = archive;
        AddAll(archive, names);
        switched++;
    }

    if (!toDev) {
        MountDeferred(names, switched);
    }

    const bool changed = s_mounted != target;
    s_mounted = target;
    s_appliedGeneration = generation;
    s_resyncPending = false;
    UpdateReport(layout);

    if (switched > 0) {
        NoteMountsChanged();
        RefreshArchiveNames(names.m_names);
    }

    StreamLog("channels: %s mounted, %d archive(s) switched, %d name(s) refreshed", target.c_str(), switched, static_cast<int32_t>(names.m_names.size()));

    if (changed) {
        Lua::SignalEvent("HOT_PATCH_CHANNEL_CHANGED", "%s", target.c_str());
    }
}

static void PublishRequested() {
    std::string requested = Requested();
    std::lock_guard<std::mutex> lock(s_stateMutex);
    s_requestedShared = requested;
}

static void TryApply() {
    PublishRequested();

    if (ClntObjMgrGetActivePlayerObj()) {
        return;
    }

    const std::string target = Requested();

    if (target != s_mounted) {
        if (target == s_release || CanApplyDev()) {
            Apply(target);
            return;
        }

        const uint64_t now = Platform::MonotonicMs();

        if (NeedsPass() && now - s_lastKickMs > s_kickIntervalMs) {
            s_lastKickMs = now;
            Trigger();
        }

        return;
    }

    if (s_mounted != s_dev) {
        return;
    }

    unsigned generation;

    {
        std::lock_guard<std::mutex> lock(s_layoutMutex);
        generation = s_layoutGeneration;
    }

    if ((s_resyncPending || generation != s_appliedGeneration) && s_devQueued.load() == 0) {
        Apply(s_dev);
    }
}

void OnBatchFinished() {
    s_resyncPending = true;
}

void OnCharacterEnum() {
    if (!s_realmAnswered) {
        s_realmAnswered = true;
        s_realmChannel = s_release;
        StreamLog("channels: realm sent no channel, using release");
    }

    TryApply();
}

void Pump() {
    TryApply();
}

void OnCloseGame() {
    for (auto& kv : s_devOnlyMounts) {
        SFile::CloseArchive(kv.second);
    }

    for (SArchive* archive : s_releaseMounts) {
        SFile::CloseArchive(archive);
    }

    s_devOnlyMounts.clear();
    s_releaseMounts.clear();
}

static void Observe_SMSG_AUTH_RESPONSE(uint32 opcode, CDataStore* msg) {
    s_realmAnswered = false;
}

static int32_t Packet_SMSG_PATCH_CHANNEL(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint8_t version = packet.GetUInt8();
    char name[64] = {};

    if (packet.CanRead(1)) {
        packet.GetString(name, sizeof(name));
    }

    const std::string channel = Normalize(name);
    StreamLog("channels: realm wants %s (packet v%u, \"%s\")", channel.c_str(), static_cast<uint32_t>(version), name);
    s_realmAnswered = true;
    s_realmChannel = channel;

    if (Record(channel)) {
        PublishRequested();
        Trigger();
    }

    TryApply();
    return 1;
}

static int32_t Script_GetPatchChannel(lua_State* L) {
    lua_pushstring(L, s_mounted.c_str());
    return 1;
}

static int32_t Script_IsPatchChannelReady(lua_State* L) {
    lua_pushboolean(L, IsReady());
    return 1;
}

class ChannelsModule : public Module {
    public:
    const char* Name() const override {
        return "PatchChannels";
    }

    void OnInitializeGlobal() override {
        std::string binaries = BinaryChannel();

        {
            std::lock_guard<std::mutex> lock(s_stateMutex);
            s_startupBinaryChannel = binaries;
        }

        Lua::RegisterFunction("GetPatchChannel", Script_GetPatchChannel, Lua::State::Any);
        Lua::RegisterFunction("IsPatchChannelReady", Script_IsPatchChannelReady, Lua::State::Any);

        Net::RegisterRealmHandler(Net::SMSG_PATCH_CHANNEL, Packet_SMSG_PATCH_CHANNEL);
        Net::RegisterObserver(SMSG_AUTH_RESPONSE, Observe_SMSG_AUTH_RESPONSE);
        PublishRequested();
    }

    void OnIdle() override {
        Pump();
    }
};

Module* GetModule() {
    static ChannelsModule module;
    return &module;
}

}
