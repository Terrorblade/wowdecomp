#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include "Engine/Source/Os/Linux/OsPosix.h"

// macOS has no SOCK_CLOEXEC, SOCK_NONBLOCK, accept4 or eventfd, so the flags are set after the fact.
static void SetFlags(int32_t fd, bool nonBlocking) {
    fcntl(fd, F_SETFD, FD_CLOEXEC);

    int32_t flags = fcntl(fd, F_GETFL);

    if (flags >= 0) {
        fcntl(fd, F_SETFL, nonBlocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
    }
}

static void SetNoSigPipe(int32_t fd) {
    int32_t one = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
}

int32_t OsPosixSocket(int32_t domain, int32_t type, int32_t protocol, bool nonBlocking) {
    int32_t fd = socket(domain, type, protocol);

    if (fd >= 0) {
        SetFlags(fd, nonBlocking);
        SetNoSigPipe(fd);
    }

    return fd;
}

// An accepted socket inherits O_NONBLOCK from the listener here, accept4 on Linux leaves it blocking.
int32_t OsPosixAccept(int32_t fd, sockaddr* address, socklen_t* length) {
    int32_t accepted;

    do {
        accepted = accept(fd, address, length);
    } while (accepted < 0 && errno == EINTR);

    if (accepted >= 0) {
        SetFlags(accepted, false);
        SetNoSigPipe(accepted);
    }

    return accepted;
}

bool OsPosixWakeCreate(OsPosixWake* wake) {
    int32_t fds[2];

    if (pipe(fds) != 0) {
        wake->m_readFd = -1;
        wake->m_writeFd = -1;
        return false;
    }

    SetFlags(fds[0], true);
    SetFlags(fds[1], true);
    wake->m_readFd = fds[0];
    wake->m_writeFd = fds[1];
    return true;
}

void OsPosixWakeSignal(const OsPosixWake* wake) {
    if (wake->m_writeFd < 0) {
        return;
    }

    uint8_t one = 1;
    ssize_t written;

    // A full pipe already holds a pending wake-up.
    do {
        written = write(wake->m_writeFd, &one, sizeof(one));
    } while (written < 0 && errno == EINTR);
}

void OsPosixWakeDrain(const OsPosixWake* wake) {
    if (wake->m_readFd < 0) {
        return;
    }

    uint8_t buffer[64];
    ssize_t bytes;

    do {
        bytes = read(wake->m_readFd, buffer, sizeof(buffer));
    } while (bytes > 0 || (bytes < 0 && errno == EINTR));
}

void OsPosixWakeDestroy(OsPosixWake* wake) {
    if (wake->m_readFd >= 0) {
        close(wake->m_readFd);
    }

    if (wake->m_writeFd >= 0 && wake->m_writeFd != wake->m_readFd) {
        close(wake->m_writeFd);
    }

    wake->m_readFd = -1;
    wake->m_writeFd = -1;
}
