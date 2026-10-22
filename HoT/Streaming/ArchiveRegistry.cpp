#include "HoT/Streaming/ArchiveRegistry.h"
#include <filesystem>
#include <mutex>
#include <vector>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/SFileArchives.h"
#include "HoT/Streaming/Paths.h"
#include "HoT/Streaming/TextConv.h"

namespace fs = std::filesystem;

namespace HoT::Streaming {

static constexpr uint32_t s_openWritable = 0x10;

static std::mutex& Mutex() {
    static std::mutex mutex;
    return mutex;
}

static std::vector<ArchiveMount>& Mounts() {
    static std::vector<ArchiveMount> mounts;
    return mounts;
}

static bool Fail(std::string* error, const std::string& what) {
    if (error) {
        *error = what;
    }

    return false;
}

void OnArchiveOpened(const char* path, int32_t priority, uint32_t flags, SArchive* archive) {
    if (!path || !archive) {
        return;
    }

    ArchiveMount m;
    m.m_key = ArchiveRegistry::KeyFor(std::string(path));
    m.m_openName = path;
    m.m_priority = priority;
    m.m_flags = flags;
    m.m_archive = archive;

    std::lock_guard<std::mutex> lock(Mutex());
    std::vector<ArchiveMount>& mounts = Mounts();

    for (auto it = mounts.begin(); it != mounts.end();) {
        it = (it->m_archive == archive || it->m_key == m.m_key) ? mounts.erase(it) : it + 1;
    }

    mounts.push_back(std::move(m));
}

void OnArchiveClosed(SArchive* archive) {
    std::lock_guard<std::mutex> lock(Mutex());
    std::vector<ArchiveMount>& mounts = Mounts();

    for (auto it = mounts.begin(); it != mounts.end();) {
        it = (it->m_archive == archive) ? mounts.erase(it) : it + 1;
    }
}

namespace ArchiveRegistry {

std::wstring KeyFor(const std::string& openName) {
    return KeyFor(Text::WidenAcp(openName));
}

std::wstring KeyFor(const std::wstring& path) {
    fs::path p(path);

    if (p.is_relative()) {
        p = Paths::GameDir() / p;
    }

    return Text::LowerPath(p.lexically_normal().wstring());
}

bool Find(const std::wstring& absPath, ArchiveMount& out) {
    const std::wstring key = KeyFor(absPath);
    std::lock_guard<std::mutex> lock(Mutex());

    for (const ArchiveMount& m : Mounts()) {
        if (m.m_key == key) {
            out = m;
            return true;
        }
    }

    return false;
}

size_t Count() {
    std::lock_guard<std::mutex> lock(Mutex());
    return Mounts().size();
}

static void SetWritable(const std::wstring& key, bool writable) {
    std::lock_guard<std::mutex> lock(Mutex());

    for (ArchiveMount& e : Mounts()) {
        if (e.m_key == key) {
            e.m_writable = writable;
        }
    }
}

static bool Reopen(const std::wstring& absPath, bool writable, std::string* error) {
    ArchiveMount m;

    if (!Find(absPath, m)) {
        return Fail(error, "archive is not mounted");
    }

    if (m.m_writable == writable) {
        return true;
    }

    System_SFile::ARCHIVEREC* raw = m.m_archive->m_archive;

    if (raw) {
        Blizzard::Mopaq::SFileCloseArchive(raw);
    }

    m.m_archive->m_archive = nullptr;

    const uint32_t wanted = writable ? (m.m_flags | s_openWritable) : m.m_flags;
    System_SFile::ARCHIVEREC* fresh = nullptr;
    bool ok = Blizzard::Mopaq::SFileOpenArchive(m.m_openName.c_str(), m.m_priority, wanted, &fresh) && fresh;

    if (!ok) {
        fresh = nullptr;
        bool restored = Blizzard::Mopaq::SFileOpenArchive(m.m_openName.c_str(), m.m_priority, m.m_flags, &fresh) && fresh;
        m.m_archive->m_archive = restored ? fresh : nullptr;
        SetWritable(m.m_key, false);

        if (!restored) {
            return Fail(error, "archive reopen failed and the read-only handle could not be restored");
        }

        return Fail(error, writable ? "cannot reopen archive for writing" : "cannot reopen archive read-only");
    }

    m.m_archive->m_archive = fresh;
    SetWritable(m.m_key, writable);
    return true;
}

bool EnsureWritable(const std::wstring& absPath, std::string* error) {
    return Reopen(absPath, true, error);
}

bool Commit(const std::wstring& absPath, std::string* error) {
    return Reopen(absPath, false, error);
}

}

}
