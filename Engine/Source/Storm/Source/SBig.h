#ifndef ENGINE_SOURCE_STORM_SOURCE_SBIG_H
#define ENGINE_SOURCE_STORM_SOURCE_SBIG_H

#include <cstdint>
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Storm/H/storm.h"

class BigBuffer {
    public:
    /* 0000 */ mutable TSGrowableArray<uint32_t> m_data;
    /* 0010 */ uint32_t m_offset;

    BigBuffer();
    uint32_t& operator[](uint32_t index);
    uint32_t operator[](uint32_t index) const;
    void Clear();
    uint32_t Count() const;
    int32_t IsUsed(uint32_t index) const;
    void SetCount(uint32_t count);
    void SetOffset(uint32_t offset);
    void Trim() const;

    private:
    void GrowToFit(uint32_t index);
};

class BigStack {
    public:
    enum {
        SIZE = 16,
    };

    /* 0000 */ BigBuffer m_buffer[SIZE];
    /* 0140 */ uint32_t m_used;

    BigStack();
    ~BigStack();
    BigBuffer& Alloc(uint32_t* count);
    void Free(uint32_t count);
    BigBuffer& MakeDistinct(BigBuffer& orig, int32_t required);
    void UnmakeDistinct(BigBuffer& orig, BigBuffer& distinct);
};

class BigData {
    public:
    /* 0000 */ BigBuffer m_primary;
    /* 0014 */ mutable BigStack m_stack;
    /* 0158 */ mutable TSGrowableArray<uint8_t> m_output;

    TSGrowableArray<uint8_t>& Output() const;
    BigBuffer& Primary();
    const BigBuffer& Primary() const;
    BigStack& Stack() const;
};

// INLINED
inline BigBuffer::BigBuffer() {
    this->m_offset = 0;
}

// INLINED
inline uint32_t& BigBuffer::operator[](uint32_t index) {
    this->GrowToFit(index);
    return this->m_data[index + this->m_offset];
}

// INLINED
inline void BigBuffer::Clear() {
    this->m_data.SetCount(this->m_offset);
}

// INLINED
inline uint32_t BigBuffer::Count() const {
    return this->m_data.Count() - this->m_offset;
}

// INLINED
inline int32_t BigBuffer::IsUsed(uint32_t index) const {
    return index + this->m_offset < this->m_data.Count();
}

// INLINED
inline void BigBuffer::SetOffset(uint32_t offset) {
    this->m_offset = offset;

    if (offset) {
        this->GrowToFit(0xFFFFFFFF);
    }
}

// INLINED
inline BigBuffer& BigStack::Alloc(uint32_t* count) {
    if (count) {
        ++*count;
    }

    return this->m_buffer[this->m_used++];
}

// INLINED
inline void BigStack::Free(uint32_t count) {
    this->m_used -= count;
}

// INLINED
inline BigBuffer& BigStack::MakeDistinct(BigBuffer& orig, int32_t required) {
    return required ? this->Alloc(nullptr) : orig;
}

// INLINED
inline void BigStack::UnmakeDistinct(BigBuffer& orig, BigBuffer& distinct) {
    if (&orig != &distinct) {
        orig = distinct;
        this->Free(1);
    }
}

// INLINED
inline TSGrowableArray<uint8_t>& BigData::Output() const {
    return this->m_output;
}

// INLINED
inline BigBuffer& BigData::Primary() {
    return this->m_primary;
}

// INLINED
inline const BigBuffer& BigData::Primary() const {
    return this->m_primary;
}

// INLINED
inline BigStack& BigData::Stack() const {
    return this->m_stack;
}

#endif
