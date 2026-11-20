#ifndef HOT_STREAMING_MPQFILE_H
#define HOT_STREAMING_MPQFILE_H

#include <map>
#include <string>
#include <vector>

// Reads one named file straight out of an MPQ on disk, safe from any thread and without mounting it.
namespace HoT::Streaming::MpqFile {

bool ReadFile(const std::wstring& archivePath, const char* name, std::string& out, std::string* error = nullptr, bool* unreadable = nullptr);

// SHA-256 of the archive's (hotmanifest) or "" when it has none, false only when the archive could not be read.
bool ContentId(const std::wstring& archivePath, std::string& out);

struct NamedDigest {
    std::string m_name;
    std::string m_sha256;
};

using NameMap = std::map<std::string, NamedDigest>;

NameMap Names(const std::wstring& archivePath);
void DiffNames(const NameMap& before, const NameMap& after, std::vector<std::string>& out);

}

#endif
