#ifndef ENGINE_SOURCE_OS_OSVERSIONHASH_H
#define ENGINE_SOURCE_OS_OSVERSIONHASH_H

#include <cstdint>
#include "Engine/Source/Storm/H/stpl.h"

class OsFileVersionInfo {
    public:
    /* 0000 */ TSFixedArray<uint8_t> m_data;
    /* 000C */ uint16_t m_fileVersion[4];
    /* 0014 */ uint16_t m_productVersion[4];

    OsFileVersionInfo(const char* filename);
};

class IOsVersionHashCallback {
    public:
    virtual void Process(const void* data, uint32_t size) = 0;
};

template <class T>
class IOsVersionHashCallback2 : public IOsVersionHashCallback {
    public:
    /* 0004 */ T* m_object;

    IOsVersionHashCallback2(T* object);
    void Process(const void* data, uint32_t size) override;
};

struct OsVersionHashEntry {

    enum TYPE : uint32_t {
        IMPORT_THUNKS = 0,
        IMPORT_DESCRIPTOR = 1,
        RELOCATION = 2,
        ZEROES = 3
    };

    /* 0000 */ uint32_t m_offset;
    /* 0004 */ uint32_t m_size;
    /* 0008 */ uint32_t m_type;
    /* 000C */ uint32_t m_param;
};

class OsVersionHash {
    public:
    /* 0000 */ OsVersionHashEntry* m_entries;
    /* 0004 */ uint32_t m_count;
    /* 0008 */ uint32_t m_capacity;

    OsVersionHash();
    ~OsVersionHash();
    bool AddEntry(const OsVersionHashEntry& entry);
};

int32_t IOsVersionChecksum(IOsVersionHashCallback& callback, const char* path, int32_t loaded);

// INLINED
template <class T>
IOsVersionHashCallback2<T>::IOsVersionHashCallback2(T* object) : m_object(object) {
}

// VERIFY: 99333a2f auto
// OFFSET: 0x006B9CE0
template <class T>
void IOsVersionHashCallback2<T>::Process(const void* data, uint32_t size) {
    this->m_object->Process(data, size);
}

#endif
