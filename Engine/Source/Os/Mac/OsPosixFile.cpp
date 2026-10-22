#include <sys/stat.h>
#include "Engine/Source/Os/Linux/OsPosix.h"

// stat carries the birth time on macOS.
void OsPosixFileTimes(int32_t dirfd, const char* path, const struct stat& st, timespec* created, timespec* modified, timespec* accessed) {
    *created = st.st_birthtimespec;
    *modified = st.st_mtimespec;
    *accessed = st.st_atimespec;
}
