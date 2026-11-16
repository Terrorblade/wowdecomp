#include "WGUID.h"
#include "Engine/Source/Base/CDataStore.h"

// VERIFY: df1d6b6b auto
// OFFSET: 0x0076DC20
CDataStore& operator>>(CDataStore& msg, SmartGUID& guid) {
    guid.m_guid = 0;

    uint8_t mask = 0;
    msg.Get(mask);

    for (uint32_t i = 0, bit = 0; i < 64; i += 8, bit++) {
        if (mask & (1 << bit)) {
            uint8_t byte = 0;
            msg.Get(byte);
            guid.m_guid |= static_cast<uint64_t>(byte) << i;
        }
    }

    return msg;
}

// VERIFY: 1f3fd382 auto
// OFFSET: 0x0076DC80
CDataStore& SmartGUID::Put(CDataStore& msg) const {
    uint8_t mask = 0;
    uint32_t maskPos = msg.m_size;
    msg.Put(static_cast<uint8_t>(0));

    for (uint32_t i = 0, bit = 0; i < 64; i += 8, bit++) {
        uint8_t byte = static_cast<uint8_t>(this->m_guid >> i);

        if (byte) {
            mask |= 1 << bit;
            msg.Put(byte);
        }
    }

    return msg.Set(maskPos, mask);
}

// VERIFY: e2576b60 auto
// OFFSET: 0x0076DD00
CDataStore& operator<<(CDataStore& msg, const SmartGUID& guid) {
    guid.Put(msg);
    return msg;
}
