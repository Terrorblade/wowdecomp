#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cstddef>
#include <cstring>
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>
#include "Engine/Source/BlizzardCore/Source/System/File/Linux/LinuxPath.h"

namespace {

bool Append(char* out, size_t outSize, size_t& length, const char* str, size_t count) {
    if (length + count + 1 > outSize) {
        return false;
    }

    memcpy(out + length, str, count);
    length += count;
    out[length] = '\0';

    return true;
}

// Looks for name in the directory out[0, length) (empty means the current directory) ignoring case.
bool FindCaseInsensitive(const char* out, size_t length, const char* name, size_t nameLength, char* match, size_t matchSize) {
    char directory[4096];

    if (length == 0) {
        directory[0] = '.';
        directory[1] = '\0';
    } else {
        if (length >= sizeof(directory)) {
            return false;
        }

        memcpy(directory, out, length);
        directory[length] = '\0';
    }

    DIR* dir = opendir(directory);

    if (!dir) {
        return false;
    }

    bool found = false;

    while (dirent* entry = readdir(dir)) {
        if (strlen(entry->d_name) == nameLength && strncasecmp(entry->d_name, name, nameLength) == 0 && nameLength < matchSize) {
            memcpy(match, entry->d_name, nameLength + 1);
            found = true;
            break;
        }
    }

    closedir(dir);

    return found;
}

}

bool System_File::LinuxResolvePath(const char* path, char* out, size_t outSize, bool forCreate) {
    if (!path || !out || outSize == 0) {
        return false;
    }

    size_t pathLength = strlen(path);

    if (pathLength + 1 > outSize) {
        return false;
    }

    // Fast path: the path exists as written once the separators are converted.
    for (size_t i = 0; i <= pathLength; i++) {
        out[i] = path[i] == '\\' ? '/' : path[i];
    }

    struct stat st;

    if (pathLength == 0 || lstat(out, &st) == 0) {
        return true;
    }

    char converted[4096];

    if (pathLength + 1 > sizeof(converted)) {
        return false;
    }

    memcpy(converted, out, pathLength + 1);

    size_t length = 0;
    out[0] = '\0';
    const char* cursor = converted;
    bool missing = false;

    if (*cursor == '/') {
        Append(out, outSize, length, "/", 1);

        while (*cursor == '/') {
            cursor++;
        }
    }

    while (*cursor) {
        const char* end = strchr(cursor, '/');
        size_t count = end ? static_cast<size_t>(end - cursor) : strlen(cursor);

        if (!missing && !(count == 1 && cursor[0] == '.') && !(count == 2 && cursor[0] == '.' && cursor[1] == '.')) {
            size_t saved = length;

            if (!Append(out, outSize, length, cursor, count)) {
                return false;
            }

            if (lstat(out, &st) != 0) {
                char match[256];
                length = saved;
                out[length] = '\0';

                if (FindCaseInsensitive(out, length, cursor, count, match, sizeof(match))) {
                    if (!Append(out, outSize, length, match, count)) {
                        return false;
                    }
                } else {
                    missing = true;

                    if (!Append(out, outSize, length, cursor, count)) {
                        return false;
                    }
                }
            }
        } else if (!Append(out, outSize, length, cursor, count)) {
            return false;
        }

        if (!end) {
            break;
        }

        // Collapse runs of separators but keep a trailing one.
        while (*end == '/') {
            end++;
        }

        if (!Append(out, outSize, length, "/", 1)) {
            return false;
        }

        cursor = end;
    }

    return forCreate || !missing;
}

#endif
