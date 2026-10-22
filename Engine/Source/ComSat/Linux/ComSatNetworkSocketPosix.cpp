#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include "Engine/Source/ComSat/ComSatNetwork.h"
#include "Engine/Source/Os/Linux/OsPosix.h"

// Address keeps a Windows sockaddr_in starting at m_family; glibc's sockaddr_in has the same 16-byte layout.
static_assert(sizeof(sockaddr_in) == 16, "sockaddr_in layout");

// The Windows constructor also turns off SIO_UDP_CONNRESET (ICMP port unreachable failing later recvfrom calls).
// Unconnected Linux UDP sockets do not report ICMP errors, so there is nothing to disable.
ComSat::Network::SocketPosix::SocketPosix() {
    int fd = OsPosixSocket(AF_INET, SOCK_DGRAM, 0, false);
    this->m_socket = static_cast<uintptr_t>(static_cast<intptr_t>(fd));

    if (fd >= 0) {
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        int value = 0x8000;
        setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &value, sizeof(value));
    }
}

ComSat::Network::SocketPosix::~SocketPosix() {
    int fd = static_cast<int>(static_cast<intptr_t>(this->m_socket));

    if (fd >= 0) {
        close(fd);
    }
}

bool ComSat::Network::SocketPosix::Bind(int32_t port) {
    ComSat::Network::Address address;
    address.SetPort(port);
    int reuse = 1;
    int fd = static_cast<int>(static_cast<intptr_t>(this->m_socket));
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    return bind(fd, reinterpret_cast<const sockaddr*>(&address.m_family), static_cast<socklen_t>(address.m_length)) != -1;
}

int32_t ComSat::Network::SocketPosix::SendTo(const ComSat::Network::Address& address, const uint8_t* data, uint32_t size) {
    int fd = static_cast<int>(static_cast<intptr_t>(this->m_socket));
    return static_cast<int32_t>(sendto(fd, data, size, MSG_NOSIGNAL, reinterpret_cast<const sockaddr*>(&address.m_family), static_cast<socklen_t>(address.m_length)));
}

bool ComSat::Network::SocketPosix::RecvFrom(ComSat::Network::Address& address, uint8_t* data, uint32_t& size) {
    memset(data, 0, size);
    int fd = static_cast<int>(static_cast<intptr_t>(this->m_socket));
    socklen_t length = static_cast<socklen_t>(address.m_length);
    ssize_t received = recvfrom(fd, data, size, 0, reinterpret_cast<sockaddr*>(&address.m_family), &length);
    address.m_length = static_cast<int32_t>(length);
    if (received < 0) {
        return false;
    }
    if (!received) {
        return false;
    }
    size = static_cast<uint32_t>(received);
    return true;
}

#endif
