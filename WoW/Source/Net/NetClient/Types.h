#ifndef WOW_SOURCE_NET_NETCLIENT_TYPES_H
#define WOW_SOURCE_NET_NETCLIENT_TYPES_H

#include <cstddef>
#include <cstdint>

enum NETSTATE {
    NS_UNINITIALIZED = 0,
    NS_INITIALIZING = 1,
    NS_INITIALIZED = 2,
    NS_UNKNOWN_3 = 3,
    NS_DISCONNECTING = 3,
    NS_CONNECTING = 4,
    NS_CONNECTED = 5,
    NS_UNK_6 = 6,
};

enum PLAYER_TYPE {
    PLAYER_NORMAL = 0,
    PLAYER_BOT = 1,
};

#endif
