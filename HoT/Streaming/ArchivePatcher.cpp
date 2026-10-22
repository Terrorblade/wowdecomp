#include "HoT/Streaming/ArchivePatcher.h"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/SFile.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/SFileArchives.h"
#include "HoT/Platform/Files.h"
#include "HoT/Streaming/ArchiveManifest.h"
#include "HoT/Streaming/ArchiveRegistry.h"
#include "HoT/Streaming/TextConv.h"

namespace HoT::Streaming::ArchivePatcher {

static constexpr uint32_t s_openDefault = 0xC00;
static constexpr uint32_t s_openWritable = 0x10;
static constexpr uint32_t s_addReplaceExisting = 0x1;
static constexpr uint32_t s_addCompress = 0x200;

static bool ReadBinaryFile(const std::wstring& path, std::string& out) {
    std::ifstream f(std::filesystem::path(path), std::ios::binary | std::ios::ate);

    if (!f) {
        return false;
    }

    std::streamsize size = f.tellg();

    if (size < 0) {
        return false;
    }

    out.resize(static_cast<size_t>(size));
    f.seekg(0, std::ios::beg);
    return size == 0 || static_cast<bool>(f.read(out.data(), size));
}

static bool AddFile(System_SFile::ARCHIVEREC* raw, const char* name, const void* data, uint32_t size, uint32_t flags) {
    IOMpqUnit* file = nullptr;

    if (!Blizzard::Mopaq::SFileCreateFile(raw, name, size, flags, &file) || !file) {
        return false;
    }

    bool ok = size == 0 || Blizzard::Mopaq::SFileWriteFile(file, data, size);
    Blizzard::Mopaq::SFileCloseFile(file);
    return ok;
}

ArchivePatchResult Apply(const ArchivePatchJob& job) {
    ArchivePatchResult res;

    ArchiveMount mount;
    const bool registered = ArchiveRegistry::Find(job.m_archivePath, mount);
    System_SFile::ARCHIVEREC* raw = nullptr;
    std::string transientName;

    if (registered) {
        if (!ArchiveRegistry::EnsureWritable(job.m_archivePath, &res.m_error)) {
            return res;
        }

        ArchiveRegistry::Find(job.m_archivePath, mount);
        raw = mount.m_archive ? mount.m_archive->m_archive : nullptr;

        if (!raw) {
            res.m_error = "writable handle missing after reopen";
            return res;
        }
    } else {
        transientName = Text::NarrowAcp(job.m_archivePath);

        if (!Blizzard::Mopaq::SFileOpenArchive(transientName.c_str(), 0, s_openDefault | s_openWritable, &raw) || !raw) {
            res.m_error = "cannot open unmounted archive for writing";
            return res;
        }
    }

    bool ok = true;
    std::string data;

    for (const ArchiveWrite& w : job.m_writes) {
        if (!ReadBinaryFile(w.m_blobPath, data) || static_cast<long long>(data.size()) != w.m_size) {
            res.m_error = "blob missing for " + w.m_name;
            ok = false;
            break;
        }

        if (!AddFile(raw, w.m_name.c_str(), data.data(), static_cast<uint32_t>(data.size()), s_addReplaceExisting | s_addCompress)) {
            res.m_error = "write failed for " + w.m_name;
            ok = false;
            break;
        }

        res.m_writtenNames.push_back(w.m_name);
    }

    if (ok && !AddFile(raw, ArchiveManifest::s_fileName, job.m_targetManifest.data(), static_cast<uint32_t>(job.m_targetManifest.size()), s_addReplaceExisting | s_addCompress)) {
        res.m_error = "manifest write failed";
        ok = false;
    }

    if (registered) {
        std::string commitError;

        if (!ArchiveRegistry::Commit(job.m_archivePath, &commitError)) {
            ok = false;

            if (res.m_error.empty()) {
                res.m_error = commitError;
            }
        }
    } else {
        Blizzard::Mopaq::SFileCloseArchive(raw);
    }

    res.m_ok = ok;

    if (ok) {
        for (const ArchiveWrite& w : job.m_writes) {
            Platform::RemoveFile(w.m_blobPath);
        }
    }

    return res;
}

}
