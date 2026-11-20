#ifndef HOT_STREAMING_ARCHIVEPATCHER_H
#define HOT_STREAMING_ARCHIVEPATCHER_H

#include <string>
#include <vector>

namespace System_SFile {
class ARCHIVEREC;
}

namespace HoT::Streaming {

struct ArchiveWrite {
    std::string m_name;
    std::wstring m_blobPath;
    long long m_size = 0;
    std::string m_sha256;
};

struct ArchivePatchJob {
    std::wstring m_archivePath;
    std::string m_relPath;
    std::vector<ArchiveWrite> m_writes;
    std::string m_targetManifest;
    std::string m_contentId;
};

struct ArchivePatchResult {
    bool m_ok = false;
    bool m_busy = false;
    std::vector<std::string> m_writtenNames;
    std::string m_error;
};

// Writes a downloaded batch into a mounted archive through the client's mopaq layer, main thread only.
namespace ArchivePatcher {

// One job spread over frames, each file is written whole inside one Step and the archive stays writable until Close.
class Session {
    public:
    // False when the archive cannot be opened for writing, Close then returns why.
    bool Open(const ArchivePatchJob& job);

    // Writes whole files until budgetMs has passed, at least one per call, true once nothing is left to write.
    bool Step(unsigned long long budgetMs);

    // Commits or closes the archive, the session can be opened again afterwards.
    ArchivePatchResult Close();

    // Drops the job as the game closes, a transient archive is still closed so its header is rewritten.
    void Abandon();

    bool IsOpen() const;

    private:
    bool StillHeld() const;

    const ArchivePatchJob* m_job = nullptr;
    System_SFile::ARCHIVEREC* m_raw = nullptr;
    bool m_registered = false;
    bool m_open = false;
    bool m_failed = false;
    bool m_manifestWritten = false;
    size_t m_next = 0;
    std::string m_data;
    ArchivePatchResult m_result;
};

}

}

#endif
