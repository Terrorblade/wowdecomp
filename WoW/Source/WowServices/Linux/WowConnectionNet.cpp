#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstdint>
#include "WoW/Source/WowServices/WowConnectionNet.h"

// The Windows network thread waits on one manual-reset event (m_event, set to wake it up) plus one WSAEventSelect
// event per connection, then hands each connection that reported network events to a worker. Here the wake-up
// event is an eventfd and the per-connection events are replaced by a poll() over the sockets; the select masks
// are rebuilt from the connection state on every pass, as PlatformRun does on Windows. The per-connection m_event
// is not used.

namespace {

const uint32_t kMaxWaitObjects = 64;

struct NetWakeEvent {
    int m_fd;
};

int WakeFd(void* event) {
    return event ? static_cast<NetWakeEvent*>(event)->m_fd : -1;
}

void SetWake(void* event) {
    int fd = WakeFd(event);

    if (fd >= 0) {
        uint64_t one = 1;
        ssize_t written;

        do {
            written = write(fd, &one, sizeof(one));
        } while (written < 0 && errno == EINTR);
    }
}

void ResetWake(void* event) {
    int fd = WakeFd(event);

    if (fd >= 0) {
        uint64_t value;
        ssize_t bytes;

        do {
            bytes = read(fd, &value, sizeof(value));
        } while (bytes < 0 && errno == EINTR);
    }
}

}

void WowConnectionNet::PlatformChangeState(WowConnection* connection, WOW_CONN_STATE state) {
    // The poll masks follow m_connState and the send list on the next pass; waking the network thread is enough.
    SetWake(this->m_event);
}

void WowConnectionNet::PlatformRun() {
    this->m_networkStarted = true;

    int32_t timeout = 500;

    if (this->m_stop) {
        return;
    }

    do {
        WowConnection* connections[kMaxWaitObjects];
        pollfd fds[kMaxWaitObjects];

        this->m_connectionsLock.Enter();

        connections[0] = nullptr;
        fds[0].fd = WakeFd(this->m_event);
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        uint32_t count = 1;
        bool disconnecting = false;

        for (WowConnection* connection = this->m_connections.Head(); connection; connection = this->m_connections.Next(connection)) {
            if (connection->m_serviceCount) {
                continue;
            }

            int16_t events;

            switch (connection->m_connState) {
                case WOWC_CONNECTING:
                    // FD_CONNECT: a non-blocking connect completes (or fails) by becoming writable.
                    events = POLLOUT;
                    break;

                case WOWC_LISTENING:
                    events = POLLIN;
                    break;

                case WOWC_CONNECTED:
                    events = (connection->m_sendList.Head() || connection->m_notifyWriteReady ? POLLOUT : 0) | POLLIN | POLLRDHUP;
                    break;

                case WOWC_DISCONNECTING:
                    events = 0;
                    timeout = 0;
                    break;

                default:
                    continue;
            }

            if (count < kMaxWaitObjects) {
                connection->m_refCount.Increment();
                fds[count].fd = connection->m_sock;
                fds[count].events = events;
                fds[count].revents = 0;
                connections[count] = connection;
                count++;

                if (connection->m_connState == WOWC_DISCONNECTING) {
                    disconnecting = true;
                }

                if (connection->m_lagTestRecvList.Head()) {
                    timeout = 0;
                }
            }
        }

        this->m_connectionsLock.Leave();

        int result;

        do {
            result = poll(fds, count, timeout);
        } while (result < 0 && errno == EINTR);

        // On Windows a disconnecting connection is serviced on the next pass that wakes up, which its still-armed
        // socket event makes immediate; with poll() nothing may be ready, so treat it as a wake-up itself.
        if (result > 0 || disconnecting) {
            if (fds[0].revents & POLLIN) {
                ResetWake(this->m_event);
            }

            for (uint32_t i = 1; i < count; i++) {
                WowConnection* connection = connections[i];
                int16_t revents = fds[i].revents;

                if (revents & POLLNVAL) {
                    // The socket was closed under us (reconnect from another thread); the next pass rebuilds the set.
                    revents = 0;
                }

                bool closed = (revents & (POLLHUP | POLLERR | POLLRDHUP)) != 0;
                uint32_t flags = 0;

                if (connection->m_connState == WOWC_CONNECTING) {
                    // Windows reports a finished or failed connect as FD_CONNECT (with an error code), not FD_CLOSE.
                    if (revents) {
                        flags = 1;
                    }
                } else {
                    if ((revents & POLLIN) || closed) {
                        flags = 2;
                    }

                    if ((revents & POLLOUT) || closed) {
                        flags |= 1;
                    }
                }

                if (connection->m_connState == WOWC_DISCONNECTING) {
                    flags |= 8;
                }

                if (WowConnection::s_lagTestDelayMin || WowConnection::s_lagTestDelayMax) {
                    flags |= 3;
                    timeout = 10;
                } else {
                    timeout = 500;
                }

                if (flags) {
                    this->SignalWorker(connection, flags);
                }
            }
        }

        for (uint32_t i = 1; i < count; i++) {
            connections[i]->Release();
        }
    } while (!this->m_stop);
}

void WowConnectionNet::PlatformInit(bool useEngine) {
    int fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);

    if (fd >= 0) {
        NetWakeEvent* event = new NetWakeEvent;
        event->m_fd = fd;
        this->m_event = event;
    }
}

void WowConnectionNet::PlatformDestruct(WowConnection* connection) {
    connection->m_event = nullptr;
}

void WowConnectionNet::PlatformWorkerReady() {
    SetWake(this->m_event);
}

void WowConnectionNet::PlatformAdd(WowConnection* connection) {
    int noDelay = 1;
    setsockopt(connection->m_sock, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay));

    SetWake(this->m_event);
}

void WowConnectionNet::PlatformRemove(WowConnection* connection) {
    SetWake(this->m_event);
}

void WowConnectionNet::PlatformStop() {
}

void WowConnectionNet::PlatformDestroy() {
    if (this->m_event) {
        NetWakeEvent* event = static_cast<NetWakeEvent*>(this->m_event);
        close(event->m_fd);
        delete event;
        this->m_event = nullptr;
    }
}

#endif
