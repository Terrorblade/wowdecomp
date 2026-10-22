#include <fcntl.h>
#include <sys/stat.h>
#include "Engine/Source/Os/Linux/OsPosix.h"

// The creation time is the birth time where the filesystem records one, else the change time.
void OsPosixFileTimes(int32_t dirfd, const char* path, const struct stat& st, timespec* created, timespec* modified, timespec* accessed) {
    *created = st.st_ctim;
    struct statx stx;

    if (statx(dirfd, path, path[0] ? 0 : AT_EMPTY_PATH, STATX_BTIME, &stx) == 0 && (stx.stx_mask & STATX_BTIME)) {
        created->tv_sec = stx.stx_btime.tv_sec;
        created->tv_nsec = stx.stx_btime.tv_nsec;
    }

    *modified = st.st_mtim;
    *accessed = st.st_atim;
}
