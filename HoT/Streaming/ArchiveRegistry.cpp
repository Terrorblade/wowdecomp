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
    m.m_originalKey = m.m_key;
    m.m_originalName = m.m_openName;

    std::lock_guard<std::mutex> lock(Mutex());
    std::vector<ArchiveMount>& mounts = Mounts();

    for (auto it = mounts.begin(); it != mounts.end();) {
        it = (it->m_archive == archive) ? mounts.erase(it) : it + 1;
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
    const std::vector<ArchiveMount>& mounts = Mounts();

    for (auto it = mounts.rbegin(); it != mounts.rend(); it++) {
        if (it->m_key == key) {
            out = *it;
            return true;
        }
    }

    return false;
}

static bool FindArchive(SArchive* archive, ArchiveMount& out) {
    std::lock_guard<std::mutex> lock(Mutex());

    for (const ArchiveMount& m : Mounts()) {
        if (m.m_archive == archive) {
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

std::vector<ArchiveMount> All() {
    std::lock_guard<std::mutex> lock(Mutex());
    return Mounts();
}

static void SetWritable(SArchive* archive, bool writable) {
    std::lock_guard<std::mutex> lock(Mutex());

    for (ArchiveMount& e : Mounts()) {
        if (e.m_archive == archive) {
            e.m_writable = writable;
        }
    }
}

static void SetOpenName(SArchive* archive, const std::string& openName) {
    std::lock_guard<std::mutex> lock(Mutex());

    for (ArchiveMount& e : Mounts()) {
        if (e.m_archive == archive) {
            e.m_openName = openName;
            e.m_key = KeyFor(openName);
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
        SetWritable(m.m_archive, false);

        if (!restored) {
            return Fail(error, "archive reopen failed and the read-only handle could not be restored");
        }

        return Fail(error, writable ? "cannot reopen archive for writing" : "cannot reopen archive read-only");
    }

    m.m_archive->m_archive = fresh;
    SetWritable(m.m_archive, writable);
    return true;
}

bool EnsureWritable(const std::wstring& absPath, std::string* error) {
    return Reopen(absPath, true, error);
}

bool Commit(const std::wstring& absPath, std::string* error) {
    return Reopen(absPath, false, error);
}

bool Retarget(SArchive* archive, const std::string& openName, std::string* error) {
    ArchiveMount m;

    if (!archive || !FindArchive(archive, m)) {
        return Fail(error, "archive is not registered");
    }

    if (m.m_writable) {
        return Fail(error, "archive is open for writing");
    }

    if (archive->m_archive) {
        Blizzard::Mopaq::SFileCloseArchive(archive->m_archive);
    }

    archive->m_archive = nullptr;
    System_SFile::ARCHIVEREC* fresh = nullptr;

    if (!Blizzard::Mopaq::SFileOpenArchive(openName.c_str(), m.m_priority, m.m_flags, &fresh) || !fresh) {
        fresh = nullptr;
        bool restored = Blizzard::Mopaq::SFileOpenArchive(m.m_openName.c_str(), m.m_priority, m.m_flags, &fresh) && fresh;
        archive->m_archive = restored ? fresh : nullptr;
        return Fail(error, restored ? "cannot open " + openName : "cannot open " + openName + " and the previous archive could not be restored");
    }

    archive->m_archive = fresh;
    SetOpenName(archive, openName);
    return true;
}

bool ReopenAround(const std::wstring& absPath, const std::function<bool()>& work, std::string* error) {
    ArchiveMount m;

    if (!Find(absPath, m)) {
        return work() ? true : Fail(error, "work failed");
    }

    if (m.m_writable) {
        return Fail(error, "archive is open for writing");
    }

    if (m.m_archive->m_archive) {
        Blizzard::Mopaq::SFileCloseArchive(m.m_archive->m_archive);
    }

    m.m_archive->m_archive = nullptr;
    const bool worked = work();
    System_SFile::ARCHIVEREC* fresh = nullptr;
    bool ok = Blizzard::Mopaq::SFileOpenArchive(m.m_openName.c_str(), m.m_priority, m.m_flags, &fresh) && fresh;
    m.m_archive->m_archive = ok ? fresh : nullptr;

    if (!ok) {
        return Fail(error, "cannot reopen " + m.m_openName);
    }

    return worked ? true : Fail(error, "work failed");
}

}

}
