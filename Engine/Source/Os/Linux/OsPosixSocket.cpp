#include <cerrno>
#include <sys/eventfd.h>
#include <unistd.h>
#include "Engine/Source/Os/Linux/OsPosix.h"

int32_t OsPosixSocket(int32_t domain, int32_t type, int32_t protocol, bool nonBlocking) {
    return socket(domain, type | SOCK_CLOEXEC | (nonBlocking ? SOCK_NONBLOCK : 0), protocol);
}

int32_t OsPosixAccept(int32_t fd, sockaddr* address, socklen_t* length) {
    return accept4(fd, address, length, SOCK_CLOEXEC);
}

bool OsPosixWakeCreate(OsPosixWake* wake) {
    int32_t fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    wake->m_readFd = fd;
    wake->m_writeFd = fd;
    return fd >= 0;
}

void OsPosixWakeSignal(const OsPosixWake* wake) {
    if (wake->m_writeFd < 0) {
        return;
    }

    uint64_t one = 1;
    ssize_t written;

    do {
        written = write(wake->m_writeFd, &one, sizeof(one));
    } while (written < 0 && errno == EINTR);
}

void OsPosixWakeDrain(const OsPosixWake* wake) {
    if (wake->m_readFd < 0) {
        return;
    }

    uint64_t value;
    ssize_t bytes;

    do {
        bytes = read(wake->m_readFd, &value, sizeof(value));
    } while (bytes < 0 && errno == EINTR);
}

void OsPosixWakeDestroy(OsPosixWake* wake) {
    if (wake->m_readFd >= 0) {
        close(wake->m_readFd);
    }

    wake->m_readFd = -1;
    wake->m_writeFd = -1;
}
