#include "HoT/Streaming/ArchivePatcher.h"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <utility>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/SFile.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/SFileArchives.h"
#include "HoT/Platform/Files.h"
#include "HoT/Platform/Time.h"
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

bool Session::IsOpen() const {
    return this->m_open;
}

bool Session::Open(const ArchivePatchJob& job) {
    *this = Session();
    this->m_job = &job;

    ArchiveMount mount;
    this->m_registered = ArchiveRegistry::Find(job.m_archivePath, mount);

    if (this->m_registered) {
        if (!ArchiveRegistry::EnsureWritable(job.m_archivePath, &this->m_result.m_error, &this->m_result.m_busy)) {
            return false;
        }

        ArchiveRegistry::Find(job.m_archivePath, mount);
        this->m_raw = mount.m_archive ? mount.m_archive->m_archive : nullptr;

        if (!this->m_raw) {
            this->m_result.m_error = "writable handle missing after reopen";
            return false;
        }
    } else {
        const std::string transientName = Text::NarrowAcp(job.m_archivePath);

        if (!Blizzard::Mopaq::SFileOpenArchive(transientName.c_str(), 0, s_openDefault | s_openWritable, &this->m_raw) || !this->m_raw) {
            this->m_raw = nullptr;
            this->m_result.m_error = "cannot open unmounted archive for writing";
            return false;
        }
    }

    this->m_open = true;
    return true;
}

// The registry handle can only change if something closed or reopened the archive between frames.
bool Session::StillHeld() const {
    if (!this->m_registered) {
        return true;
    }

    ArchiveMount mount;
    return ArchiveRegistry::Find(this->m_job->m_archivePath, mount) && mount.m_archive && mount.m_archive->m_archive == this->m_raw;
}

bool Session::Step(unsigned long long budgetMs) {
    if (!this->m_open || this->m_failed || this->m_manifestWritten) {
        return true;
    }

    if (!this->StillHeld()) {
        this->m_result.m_error = "archive handle changed while patching";
        this->m_failed = true;
        return true;
    }

    const ArchivePatchJob& job = *this->m_job;
    const unsigned long long start = Platform::MonotonicMs();

    while (this->m_next < job.m_writes.size()) {
        const ArchiveWrite& w = job.m_writes[this->m_next];

        if (!ReadBinaryFile(w.m_blobPath, this->m_data) || static_cast<long long>(this->m_data.size()) != w.m_size) {
            this->m_result.m_error = "blob missing for " + w.m_name;
            this->m_failed = true;
            return true;
        }

        if (!AddFile(this->m_raw, w.m_name.c_str(), this->m_data.data(), static_cast<uint32_t>(this->m_data.size()), s_addReplaceExisting | s_addCompress)) {
            this->m_result.m_error = "write failed for " + w.m_name;
            this->m_failed = true;
            return true;
        }

        this->m_result.m_writtenNames.push_back(w.m_name);
        this->m_next++;

        if (Platform::MonotonicMs() - start >= budgetMs) {
            return false;
        }
    }

    if (!AddFile(this->m_raw, ArchiveManifest::s_fileName, job.m_targetManifest.data(), static_cast<uint32_t>(job.m_targetManifest.size()), s_addReplaceExisting | s_addCompress)) {
        this->m_result.m_error = "manifest write failed";
        this->m_failed = true;
        return true;
    }

    this->m_manifestWritten = true;
    return true;
}

ArchivePatchResult Session::Close() {
    ArchivePatchResult res = std::move(this->m_result);

    if (this->m_open) {
        bool ok = !this->m_failed && this->m_manifestWritten;

        if (this->m_registered) {
            std::string commitError;

            if (!ArchiveRegistry::Commit(this->m_job->m_archivePath, &commitError)) {
                ok = false;

                if (res.m_error.empty()) {
                    res.m_error = commitError;
                }
            }
        } else {
            Blizzard::Mopaq::SFileCloseArchive(this->m_raw);
        }

        res.m_ok = ok;
    }

    *this = Session();
    return res;
}

void Session::Abandon() {
    // A registered archive is flushed by whoever closes its SArchive, a transient one only by this.
    if (this->m_open && !this->m_registered) {
        Blizzard::Mopaq::SFileCloseArchive(this->m_raw);
    }

    *this = Session();
}

}
