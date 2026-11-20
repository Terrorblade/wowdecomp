#include <cstdint>
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Tempest/CAaBox.h"

// VERIFY: 1bea3dca manual
// OFFSET: 0x0070BCF0
bool CAaBox::IsValid() const {
    return this->t.x > this->b.x && this->t.y > this->b.y && this->t.z > this->b.z;
}

// VERIFY: 040a12ae manual
// OFFSET: 0x0070BD20
bool CAaBox::IsEmpty() const {
    return this->t.x <= this->b.x || this->t.y <= this->b.y || this->t.z <= this->b.z;
}

// VERIFY: 0f1d380e manual
// OFFSET: 0x007150D0
CAaBox operator|(const CAaBox& l, const CAaBox& r) {
    C3Vector t = C3Vector::Max(l.t, r.t);
    C3Vector b = C3Vector::Min(l.b, r.b);

    CAaBox result;
    result.b = b;
    result.t = t;
    return result;
}

// VERIFY: 724307ab manual
// OFFSET: 0x00715130
CAaBox CAaBox::operator|=(const CAaBox& box) {
    *this = *this | box;
    return *this;
}

// VERIFY: 77b1dfb8 auto
// OFFSET: 0x0078F480
CAaBox::CAaBox(const C3Vector& b, const C3Vector& t) {
    this->b = b;
    this->t = t;
}

// VERIFY: 4ac67881 manual
// OFFSET: 0x007AE2B0
bool CAaBox::IntersectsXY(const CAaBox& box) const {
    return this->t.x >= box.b.x && this->t.y >= box.b.y && this->b.x <= box.t.x && this->b.y <= box.t.y;
}

// VERIFY: 2895a20e manual
// OFFSET: 0x00984860
CAaBox CAaBox::Transform(const CAaBox& box, const C44Matrix& m) {
    CAaBox result;
    result.b.x = m.d0;
    result.b.y = m.d1;
    result.b.z = m.d2;
    result.t.x = m.d0;
    result.t.y = m.d1;
    result.t.z = m.d2;

    const float* rows = &m.a0;

    for (uint32_t i = 0; i < 3; i++) {
        float* lo = &result.b.x + i;
        float* hi = &result.t.x + i;

        float e = box.b.x * rows[0 + i];
        float f = rows[0 + i] * box.t.x;

        if (f <= e) {
            *lo = f + *lo;
        } else {
            *lo = e + *lo;
            e = f;
        }

        *hi = e + *hi;

        e = box.b.y * rows[4 + i];
        f = box.t.y * rows[4 + i];

        if (f <= e) {
            *lo = f + *lo;
        } else {
            *lo = e + *lo;
            e = f;
        }

        *hi = e + *hi;

        e = rows[8 + i] * box.b.z;
        f = rows[8 + i] * box.t.z;

        if (f <= e) {
            *lo = f + *lo;
        } else {
            *lo = e + *lo;
            e = f;
        }

        *hi = e + *hi;
    }

    return result;
}

// VERIFY: d22b3ba3 manual
// OFFSET: 0x00984930
CAaBox CAaBox::Bounding(const C3Vector* points, uint32_t count) {
    CAaBox result;

    if (!count) {
        return result;
    }

    result.b = points[0];
    result.t = points[0];

    for (uint32_t i = 1; i < count; i++) {
        const C3Vector& point = points[i];

        if (result.b.x > point.x) {
            result.b.x = point.x;
        }

        if (result.b.y > point.y) {
            result.b.y = point.y;
        }

        if (result.b.z > point.z) {
            result.b.z = point.z;
        }

        if (result.t.x < point.x) {
            result.t.x = point.x;
        }

        if (result.t.y < point.y) {
            result.t.y = point.y;
        }

        if (result.t.z < point.z) {
            result.t.z = point.z;
        }
    }

    return result;
}

// VERIFY: 0a872e55 auto
// OFFSET: 0x005FECB0
CAaBox& CAaBox::operator*=(float scale) {
    this->b.x = this->b.x * scale;
    this->b.y = this->b.y * scale;
    this->b.z = this->b.z * scale;
    this->t.x = this->t.x * scale;
    this->t.y = this->t.y * scale;
    this->t.z = scale * this->t.z;
    return *this;
}

// VERIFY: f730ab05 auto
// OFFSET: 0x0070BD70
void CAaBox::Expand(float amount) {
    this->b.x = this->b.x - amount;
    this->b.y = this->b.y - amount;
    this->b.z = this->b.z - amount;
    this->t.x = this->t.x + amount;
    this->t.y = this->t.y + amount;
    this->t.z = amount + this->t.z;
}

// VERIFY: 6ac424a6 manual
// OFFSET: 0x00715080
void CAaBox::Set(float min, float max) {
    this->b = C3Vector(min, min, min);
    this->t = C3Vector(max, max, max);
}

// VERIFY: ab29f5db manual
// OFFSET: 0x006CB930
bool CAaBox::ContainsPointExclusive(const C3Vector& point) const {
    return this->b.x < point.x && this->b.y < point.y && this->b.z < point.z && this->t.x > point.x && this->t.y > point.y && this->t.z > point.z;
}

// VERIFY: 230daf18 auto
// OFFSET: 0x006CB900
CAaBox::CAaBox(const C3Vector& point) {
    this->b = point;
    this->t = point;
}

// VERIFY: c685adae auto
// OFFSET: 0x006FC150
void CAaBox::Expand(const C3Vector& amount) {
    this->b.x -= amount.x;
    this->b.y -= amount.y;
    this->b.z -= amount.z;
    this->t.x += amount.x;
    this->t.y += amount.y;
    this->t.z += amount.z;
}

// VERIFY: a822e7ee auto
// OFFSET: 0x004F5DE0
C3Vector CAaBox::Center() const {
    float y = this->t.y + this->b.y;
    float z = this->t.z + this->b.z;
    C3Vector center;
    center.x = (this->t.x + this->b.x) * 0.5f;
    center.y = y * 0.5f;
    center.z = 0.5f * z;
    return center;
}

// VERIFY: 1cc9efa2 auto
// OFFSET: 0x00705630
void CAaBox::operator+=(const C3Vector& v) {
    this->b.x = v.x + this->b.x;
    this->b.y = this->b.y + v.y;
    this->b.z = this->b.z + v.z;
    this->t.x = this->t.x + v.x;
    this->t.y = this->t.y + v.y;
    this->t.z = this->t.z + v.z;
}

// VERIFY: dacfd862 auto
// OFFSET: 0x007BD450
bool CAaBox::IsInverted() const {
    return this->b.x > this->t.x && this->b.y > this->t.y && this->b.z > this->t.z;
}
