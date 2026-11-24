#ifndef WOW_SOURCE_GAME_GAMECLIENT_PETNAMECACHE_H
#define WOW_SOURCE_GAME_GAMECLIENT_PETNAMECACHE_H

#include "WoW/Source/Game/Types.h"

#include <cstddef>
#include <cstdint>

class CDataStore;

class PetNameCache {
    public:
    static const uint32_t s_cacheVersion = 1;

    struct DeclinedNames {
        /* 0000 */ char m_names[NUM_DECLENSIONS][96];
    };

    /* 0000 */ char m_name[80];
    /* 0050 */ PetNameCache::DeclinedNames* m_declinedNames;
    /* 0054 */ uint32_t m_petNumber;
    /* 0058 */ uint32_t m_timestamp;

    PetNameCache();
    ~PetNameCache();
    PetNameCache& operator=(const PetNameCache& source);
    void Pack(CDataStore* msg);
    void Unpack(CDataStore* msg);
};

#endif
