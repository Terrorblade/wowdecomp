#include "HoT/MSDF/Console.h"
#include <cstdio>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace HoT::MSDF::Console {

static int s_lockFile = -1;

// The prompts use the terminal the client was started from, there is no console to allocate. Without a terminal on
// stdin this behaves like a failed AllocConsole and pre-generation is skipped.
Session::Session() {
    this->m_open = isatty(STDIN_FILENO) != 0;
}

Session::~Session() {
}

bool ReadInt(int& value) {
    return scanf("%d", &value) == 1;
}

bool ReadHex(uint32_t& value) {
    return scanf("%x", &value) == 1;
}

bool ReadDouble(double& value) {
    return scanf("%lf", &value) == 1;
}

// An flock() held for the session replaces the delete on close file, it is dropped even if the client crashes.
bool AcquireLock(const std::string& path) {
    int handle = open(path.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0644);

    if (handle < 0) {
        return false;
    }

    if (flock(handle, LOCK_EX | LOCK_NB) != 0) {
        close(handle);
        return false;
    }

    s_lockFile = handle;
    return true;
}

void ReleaseLock() {
    if (s_lockFile >= 0) {
        close(s_lockFile);
        s_lockFile = -1;
    }
}

}
