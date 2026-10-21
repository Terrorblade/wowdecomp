#ifndef ENGINE_SOURCE_BLIZZARDCORE_SOURCE_SYSTEM_FILE_LINUX_LINUXPATH_H
#define ENGINE_SOURCE_BLIZZARDCORE_SOURCE_SYSTEM_FILE_LINUX_LINUXPATH_H

#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cstddef>

namespace System_File {

// Maps a game path to a host path: '\\' becomes '/', and every component that does not exist with the given case is
// matched case-insensitively against the directory contents (game data keeps whatever case the user's copy has).
// Components that match nothing are kept as given, so the result names the file to create when forCreate is set.
// Returns false only if the result does not fit in out (or, without forCreate, if the path does not exist).
bool LinuxResolvePath(const char* path, char* out, size_t outSize, bool forCreate);

}

#endif

#endif
