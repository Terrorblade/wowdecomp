#ifndef WOW_SOURCE_OBJECT_WGUID_H
#define WOW_SOURCE_OBJECT_WGUID_H

#include <cstddef>
#include <cstdint>

class CDataStore;
constexpr uint64_t WOWGUID_SPELL_QUERY_TAG = 0x1FE0000000000000ull;
constexpr uint64_t WOWGUID_NONE_TOKEN = 0xFFFFFFFFFFFFFFFFull;
constexpr uint64_t WOWGUID_EMPTY_UNIT_TOKEN = 0xFFFFFFFFFFFFFFFEull;

struct WOWGUID {
    /* 0000 */ uint64_t m_guid;

    explicit operator bool() const {
        return this->m_guid != 0;
    }

    bool operator==(const WOWGUID& guid) const {
        return this->m_guid == guid.m_guid;
    }

    bool operator!=(const WOWGUID& guid) const {
        return this->m_guid != guid.m_guid;
    }
};

struct SmartGUID : public WOWGUID {
    CDataStore& Put(CDataStore& msg) const;
};

CDataStore& operator>>(CDataStore& msg, SmartGUID& guid);
CDataStore& operator<<(CDataStore& msg, const SmartGUID& guid);

#endif
