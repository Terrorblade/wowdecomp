#ifndef HOT_STREAMING_ARCHIVEREGISTRY_H
#define HOT_STREAMING_ARCHIVEREGISTRY_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct SArchive;

namespace HoT::Streaming {

// Called from SFile::OpenArchive on success and from SFile::CloseArchive before the close.
void OnArchiveOpened(const char* path, int32_t priority, uint32_t flags, SArchive* archive);
void OnArchiveClosed(SArchive* archive);

struct ArchiveMount {
    std::wstring m_key;
    std::string m_openName;
    std::wstring m_originalKey;
    std::string m_originalName;
    int32_t m_priority = 0;
    uint32_t m_flags = 0;
    SArchive* m_archive = nullptr;
    bool m_writable = false;
};

// Every archive opened through SFile::OpenArchive, keyed by lowercased absolute path.
namespace ArchiveRegistry {

bool Find(const std::wstring& absPath, ArchiveMount& out);
size_t Count();
std::vector<ArchiveMount> All();

bool EnsureWritable(const std::wstring& absPath, std::string* error = nullptr);
bool Commit(const std::wstring& absPath, std::string* error = nullptr);

bool Retarget(SArchive* archive, const std::string& openName, std::string* error = nullptr);
bool ReopenAround(const std::wstring& absPath, const std::function<bool()>& work, std::string* error = nullptr);

std::wstring KeyFor(const std::string& openName);
std::wstring KeyFor(const std::wstring& path);

}

}

#endif
