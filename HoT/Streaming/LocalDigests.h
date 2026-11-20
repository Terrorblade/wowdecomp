#ifndef HOT_STREAMING_LOCALDIGESTS_H
#define HOT_STREAMING_LOCALDIGESTS_H

#include <string>

// Digests of local files cached by size and write time in .hotlauncher\hotstream-local.txt.
namespace HoT::Streaming::LocalDigests {

enum class Kind {
    Quick,
    Sha256,
    ContentId,
};

bool Get(const std::wstring& path, Kind kind, std::string& out);
void Remember(const std::wstring& path, Kind kind, const std::string& digest);
void Save();

// Drops entries for files under the game folder that are gone, from the patching pass only so followers never save.
void Prune();

}

#endif
