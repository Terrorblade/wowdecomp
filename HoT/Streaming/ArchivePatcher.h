#ifndef HOT_STREAMING_ARCHIVEPATCHER_H
#define HOT_STREAMING_ARCHIVEPATCHER_H

#include <string>
#include <vector>

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

ArchivePatchResult Apply(const ArchivePatchJob& job);

}

}

#endif
