#ifndef ENGINE_SOURCE_TEMPEST_CAABOX_H
#define ENGINE_SOURCE_TEMPEST_CAABOX_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Tempest/C3Vector.h"

class C44Matrix;

class CAaBox {
    public:
    static CAaBox Bounding(const C3Vector* points, uint32_t count);
    static CAaBox Transform(const CAaBox& box, const C44Matrix& m);

    /* 0000 */ C3Vector b;
    /* 000C */ C3Vector t;

    CAaBox() = default;
    CAaBox(const C3Vector& b, const C3Vector& t);
    explicit CAaBox(const C3Vector& point);
    bool ContainsPoint(const C3Vector& point) const;
    bool ContainsPointExclusive(const C3Vector& point) const;
    bool Intersects(const CAaBox& box) const;
    bool IntersectsXY(const CAaBox& box) const;
    bool IsEmpty() const;
    bool IsInverted() const;
    bool IsValid() const;
    void Expand(float amount);
    void Expand(const C3Vector& amount);
    C3Vector Center() const;
    void Set(float min, float max);
    CAaBox& operator*=(float scale);
    void operator+=(const C3Vector& v);
    CAaBox operator|=(const CAaBox& box);
};

CAaBox operator|(const CAaBox& l, const CAaBox& r);

// VERIFY: 417ec50b manual
// OFFSET: 0x0075B5B0
inline bool CAaBox::ContainsPoint(const C3Vector& point) const {
    return this->b.x <= point.x && this->b.y <= point.y && this->b.z <= point.z && this->t.x >= point.x && this->t.y >= point.y && this->t.z >= point.z;
}

// VERIFY: 195b7d66 manual
// OFFSET: 0x0078F370
inline bool CAaBox::Intersects(const CAaBox& box) const {
    return this->t.x >= box.b.x && this->t.y >= box.b.y && this->t.z >= box.b.z && this->b.x <= box.t.x && this->b.y <= box.t.y && this->b.z <= box.t.z;
}

#endif
