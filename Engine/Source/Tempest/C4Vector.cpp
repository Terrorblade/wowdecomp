#include <cmath>
#include "Engine/Source/Tempest/C4Vector.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "Engine/Source/Tempest/CMath.h"

// VERIFY: 42d2cfec manual
// OFFSET: 0x00979110
void C4Vector::NormalizeSafe() {
    float length = this->w * this->w + this->z * this->z + this->y * this->y + this->x * this->x;

    if (length > 0.00000023841858f) {
        float scale = 1.0f / sqrt(length);
        this->x = this->x * scale;
        this->y = this->y * scale;
        this->z = scale * this->z;
        this->w = scale * this->w;
    }
}

// VERIFY: f1de32d2 manual
// OFFSET: 0x00984C90
C4Vector::C4Vector(const CImVector& color) {
    this->x = color.r * 0.0039215689f;
    this->y = color.g * 0.0039215689f;
    this->z = color.b * 0.0039215689f;
    this->w = 0.0039215689f * color.a;
}
