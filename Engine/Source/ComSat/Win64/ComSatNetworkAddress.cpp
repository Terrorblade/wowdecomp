
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdlib>
#include <cstring>
#include "Engine/Source/ComSat/ComSatNetwork.h"

// OFFSET: 0x00885C40
void ComSat::Network::Address::SetPort(int32_t port) {
    this->m_port = htons(static_cast<u_short>(port));
}

// OFFSET: 0x00885C60
int32_t ComSat::Network::Address::GetPort() const {
    return ntohs(this->m_port);
}

// OFFSET: 0x00885CD0
void ComSat::Network::Address::SetAddress(const char* address) {
    const char* colon = strchr(address, ':');
    if (colon) {
        if (strlen(address) < 128) {
            char host[128];
            strcpy(host, address);
            host[colon - address] = '\0';
            in_addr parsed;
            this->m_ip = inet_pton(AF_INET, host, &parsed) == 1 ? parsed.s_addr : INADDR_NONE;
            this->m_port = htons(static_cast<u_short>(strtol(colon + 1, nullptr, 10)));
        }
    } else {
        in_addr parsed;
        this->m_ip = inet_pton(AF_INET, address, &parsed) == 1 ? parsed.s_addr : INADDR_NONE;
    }
}

