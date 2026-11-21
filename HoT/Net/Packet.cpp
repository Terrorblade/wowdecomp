#include "HoT/Net/Packet.h"
#include <cstring>
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"

namespace HoT::Net {

Packet::Packet(uint32 opcode) : m_ds(&m_owned) {
    this->m_ds->Put(opcode);
}

Packet::Packet(CDataStore* incoming) : m_ds(incoming) {
}

Packet& Packet::PutInt8(int8 v) {
    this->m_ds->Put(static_cast<uint8_t>(v));
    return *this;
}

Packet& Packet::PutUInt8(uint8 v) {
    this->m_ds->Put(v);
    return *this;
}

Packet& Packet::PutInt16(int16 v) {
    this->m_ds->Put(static_cast<uint16_t>(v));
    return *this;
}

Packet& Packet::PutUInt16(uint16 v) {
    this->m_ds->Put(v);
    return *this;
}

Packet& Packet::PutInt32(int32 v) {
    this->m_ds->Put(static_cast<uint32_t>(v));
    return *this;
}

Packet& Packet::PutUInt32(uint32 v) {
    this->m_ds->Put(v);
    return *this;
}

Packet& Packet::PutInt64(int64 v) {
    this->m_ds->Put(static_cast<uint64_t>(v));
    return *this;
}

Packet& Packet::PutUInt64(uint64 v) {
    this->m_ds->Put(v);
    return *this;
}

Packet& Packet::PutFloat(float v) {
    this->m_ds->Put(v);
    return *this;
}

Packet& Packet::PutDouble(double v) {
    uint64_t bits;
    memcpy(&bits, &v, sizeof(bits));
    this->m_ds->Put(bits);
    return *this;
}

Packet& Packet::PutString(const char* v) {
    this->m_ds->PutString(v ? v : "");
    return *this;
}

Packet& Packet::PutBytes(const void* src, uint32 len) {
    if (len) {
        this->m_ds->PutData(src, len);
    }

    return *this;
}

int8 Packet::GetInt8() {
    return static_cast<int8>(this->GetUInt8());
}

uint8 Packet::GetUInt8() {
    uint8_t v = 0;
    this->m_ds->Get(v);
    return v;
}

int16 Packet::GetInt16() {
    return static_cast<int16>(this->GetUInt16());
}

uint16 Packet::GetUInt16() {
    uint16_t v = 0;
    this->m_ds->Get(v);
    return v;
}

int32 Packet::GetInt32() {
    int32_t v = 0;
    this->m_ds->Get(v);
    return v;
}

uint32 Packet::GetUInt32() {
    uint32_t v = 0;
    this->m_ds->Get(v);
    return v;
}

int64 Packet::GetInt64() {
    return static_cast<int64>(this->GetUInt64());
}

uint64 Packet::GetUInt64() {
    uint64_t v = 0;
    this->m_ds->Get(v);
    return v;
}

float Packet::GetFloat() {
    float v = 0.0f;
    this->m_ds->Get(v);
    return v;
}

double Packet::GetDouble() {
    uint64_t bits = this->GetUInt64();
    double v;
    memcpy(&v, &bits, sizeof(v));
    return v;
}

void Packet::GetString(char* out, uint32 size) {
    if (size) {
        out[0] = '\0';
    }

    this->m_ds->GetString(out, size);
}

void Packet::GetBytes(void* dst, uint32 len) {
    if (!len) {
        return;
    }

    if (!this->CanRead(len)) {
        memset(dst, 0, len);
        this->m_ds->m_read = this->m_ds->m_size + 1;
        return;
    }

    this->m_ds->GetData(dst, len);
}

bool Packet::CanRead(uint32 bytes) const {
    return this->m_ds->m_read <= this->m_ds->m_size && this->m_ds->m_size - this->m_ds->m_read >= bytes;
}

uint32 Packet::Remaining() const {
    return this->m_ds->m_read <= this->m_ds->m_size ? this->m_ds->m_size - this->m_ds->m_read : 0;
}

uint32 Packet::ClampCount(uint32 count, uint32 wireSize) const {
    uint32 fit = wireSize ? this->Remaining() / wireSize : this->Remaining();
    return count < fit ? count : fit;
}

void Packet::Send() {
    this->m_ds->Finalize();
    ClientServices::Send2(this->m_ds);
}

}
