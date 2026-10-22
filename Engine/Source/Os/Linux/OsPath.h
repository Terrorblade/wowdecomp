#ifndef ENGINE_SOURCE_OS_LINUX_OSPATH_H
#define ENGINE_SOURCE_OS_LINUX_OSPATH_H

#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cstddef>
#include <cstring>
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>

// Game code builds Windows paths ("WTF\\Config.wtf", "Data\\enUS\\..."), and the game data keeps whatever case the
// user's copy has. OsLinuxResolvePath maps such a path to a host path:
//   - every '\\' becomes '/';
//   - every component that does not exist with the given case is matched case-insensitively (ASCII) against the
//     contents of its directory and replaced by the on-disk spelling;
//   - components that match nothing are kept as given, so with forCreate the result names the file or directory to
//     create (with its existing parent directories in their on-disk case).
// out always receives the best-effort result when it fits. Returns false if the result does not fit in out, or,
// without forCreate, if the resolved path does not exist. Header-only so any module (Os, BlizzardCore, Storm) can use
// it without a link dependency. out may not alias path.

// Replaces name (a NUL-terminated component inside a writable buffer) by the spelling of the first directory entry of
// dir that matches it case-insensitively. ASCII case folding keeps the length unchanged.
inline bool OsLinuxMatchComponent(const char* dir, char* name) {
    DIR* handle = opendir(dir);

    if (!handle) {
        return false;
    }

    bool found = false;
    struct dirent* entry;

    while ((entry = readdir(handle)) != nullptr) {
        if (!strcasecmp(entry->d_name, name) && strlen(entry->d_name) == strlen(name)) {
            memcpy(name, entry->d_name, strlen(name));
            found = true;
            break;
        }
    }

    closedir(handle);

    return found;
}

inline bool OsLinuxResolvePath(const char* path, char* out, size_t outSize, bool forCreate) {
    if (!out || !outSize) {
        return false;
    }

    out[0] = '\0';

    if (!path) {
        return false;
    }

    size_t length = strlen(path);

    if (length >= outSize) {
        return false;
    }

    for (size_t i = 0; i <= length; i++) {
        out[i] = path[i] == '\\' ? '/' : path[i];
    }

    if (!out[0]) {
        return forCreate;
    }

    struct stat info;

    if (!lstat(out, &info)) {
        return true;
    }

    char* component = out;

    while (*component) {
        while (*component == '/') {
            component++;
        }

        if (!*component) {
            break;
        }

        char* end = strchr(component, '/');

        if (end) {
            *end = '\0';
        }

        bool exists = !lstat(out, &info);

        if (!exists && strcmp(component, ".") && strcmp(component, "..")) {
            // Temporarily cut the buffer before this component to name its directory.
            if (component == out) {
                exists = OsLinuxMatchComponent(".", component);
            } else if (component == out + 1) {
                exists = OsLinuxMatchComponent("/", component);
            } else {
                char separator = component[-1];
                component[-1] = '\0';
                exists = OsLinuxMatchComponent(out, component);
                component[-1] = separator;
            }
        }

        if (!end) {
            return exists || forCreate;
        }

        *end = '/';

        if (!exists) {
            if (!forCreate) {
                return false;
            }

            // Nothing below a missing directory can be matched; keep the rest as given.
            return true;
        }

        component = end + 1;
    }

    return true;
}

#endif

#endif
