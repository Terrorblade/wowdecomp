#ifndef HOT_NET_PACKET_H
#define HOT_NET_PACKET_H

#include "Engine/Source/Base/CDataStore.h"
#include "HoT/Core/Types.h"

namespace HoT::Net {

class Packet {
    public:
    explicit Packet(uint32 opcode);
    explicit Packet(CDataStore* incoming);

    Packet(const Packet&) = delete;
    Packet& operator=(const Packet&) = delete;

    Packet& PutInt8(int8 v);
    Packet& PutUInt8(uint8 v);
    Packet& PutInt16(int16 v);
    Packet& PutUInt16(uint16 v);
    Packet& PutInt32(int32 v);
    Packet& PutUInt32(uint32 v);
    Packet& PutInt64(int64 v);
    Packet& PutUInt64(uint64 v);
    Packet& PutFloat(float v);
    Packet& PutDouble(double v);
    Packet& PutString(const char* v);
    Packet& PutBytes(const void* src, uint32 len);

    int8 GetInt8();
    uint8 GetUInt8();
    int16 GetInt16();
    uint16 GetUInt16();
    int32 GetInt32();
    uint32 GetUInt32();
    int64 GetInt64();
    uint64 GetUInt64();
    float GetFloat();
    double GetDouble();
    void GetString(char* out, uint32 size);
    void GetBytes(void* dst, uint32 len);

    bool CanRead(uint32 bytes) const;
    uint32 Remaining() const;

    // Caps a server sent element count by what the remaining bytes could hold.
    uint32 ClampCount(uint32 count, uint32 wireSize) const;

    void Send();

    CDataStore* Raw() { return this->m_ds; }

    private:
    CDataStore m_owned;
    CDataStore* m_ds;
};

}

#endif
