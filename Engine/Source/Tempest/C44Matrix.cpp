#include <cmath>
#include "Engine/Source/Tempest/C33Matrix.h"
#include "Engine/Source/Tempest/C34Matrix.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Tempest/C4Quaternion.h"

// VERIFY: cbb36d4b auto
// OFFSET: 0x004E26F0
C44Matrix::C44Matrix(float scale) {
    this->a0 = scale;
    this->a1 = 0.0f;
    this->a2 = 0.0f;
    this->a3 = 0.0f;
    this->b0 = 0.0f;
    this->b1 = scale;
    this->b2 = 0.0f;
    this->b3 = 0.0f;
    this->c0 = 0.0f;
    this->c1 = 0.0f;
    this->c2 = scale;
    this->c3 = 0.0f;
    this->d0 = 0.0f;
    this->d1 = 0.0f;
    this->d2 = 0.0f;
    this->d3 = 1.0f;
}

// INLINED
C44Matrix::C44Matrix(float a0, float a1, float a2, float a3, float b0, float b1, float b2, float b3, float c0, float c1, float c2, float c3, float d0, float d1, float d2, float d3) {
    this->a0 = a0;
    this->a1 = a1;
    this->a2 = a2;
    this->a3 = a3;
    this->b0 = b0;
    this->b1 = b1;
    this->b2 = b2;
    this->b3 = b3;
    this->c0 = c0;
    this->c1 = c1;
    this->c2 = c2;
    this->c3 = c3;
    this->d0 = d0;
    this->d1 = d1;
    this->d2 = d2;
    this->d3 = d3;
}

// VERIFY: bcc62f69 auto
// OFFSET: 0x0057C2A0
void C44Matrix::Identity() {
    this->d3 = 1.0f;
    this->c2 = 1.0f;
    this->b1 = 1.0f;
    this->a0 = 1.0f;
    this->d2 = 0.0f;
    this->d1 = 0.0f;
    this->d0 = 0.0f;
    this->c3 = 0.0f;
    this->c1 = 0.0f;
    this->c0 = 0.0f;
    this->b3 = 0.0f;
    this->b2 = 0.0f;
    this->b0 = 0.0f;
    this->a3 = 0.0f;
    this->a2 = 0.0f;
    this->a1 = 0.0f;
}

// VERIFY: b9812d24 manual
// OFFSET: 0x00407F40
C44Matrix::C44Matrix() {
    this->a0 = 1.0f;
    this->a1 = 0.0f;
    this->a2 = 0.0f;
    this->a3 = 0.0f;
    this->b0 = 0.0f;
    this->b2 = 0.0f;
    this->b3 = 0.0f;
    this->c0 = 0.0f;
    this->c1 = 0.0f;
    this->c3 = 0.0f;
    this->d0 = 0.0f;
    this->d1 = 0.0f;
    this->d2 = 0.0f;
    this->b1 = 1.0f;
    this->c2 = 1.0f;
    this->d3 = 1.0f;
}

// VERIFY: d44fd3fc manual
// OFFSET: 0x00407F80
C44Matrix& C44Matrix::operator=(const C44Matrix& m) {
    this->a0 = m.a0;
    this->a1 = m.a1;
    this->a2 = m.a2;
    this->a3 = m.a3;
    this->b0 = m.b0;
    this->b1 = m.b1;
    this->b2 = m.b2;
    this->b3 = m.b3;
    this->c0 = m.c0;
    this->c1 = m.c1;
    this->c2 = m.c2;
    this->c3 = m.c3;
    this->d0 = m.d0;
    this->d1 = m.d1;
    this->d2 = m.d2;
    this->d3 = m.d3;
    return *this;
}

// VERIFY: 0d73c48d manual
// OFFSET: 0x004C1830
bool operator!=(const C44Matrix& l, const C44Matrix& r) {
    return r.a0 != l.a0 || r.a1 != l.a1 || r.a2 != l.a2 || r.a3 != l.a3 || r.b0 != l.b0 || r.b1 != l.b1 || r.b2 != l.b2 || r.b3 != l.b3 || r.c0 != l.c0 || r.c1 != l.c1 || r.c2 != l.c2 || r.c3 != l.c3 || r.d0 != l.d0 || r.d1 != l.d1 || r.d2 != l.d2 || r.d3 != l.d3;
}

// VERIFY: 32c45e98 auto
// OFFSET: 0x004C1930
float C44Matrix::Determinant() const {
    float b0 = this->b0;
    float b1 = this->b1;
    float b2 = this->b2;
    float b3 = this->b3;
    float c0 = this->c0;
    float c1 = this->c1;
    float c2 = this->c2;
    float c3 = this->c3;
    float d0 = this->d0;
    float d1 = this->d1;
    float d2 = this->d2;
    float d3 = this->d3;

    float result = (b1 * c2 * d3 + c3 * b2 * d1 + b3 * c1 * d2 - c2 * b3 * d1 - d3 * (c1 * b2) - b1 * c3 * d2) * this->a0
        - (c2 * b0 * d3 + c3 * b2 * d0 + b3 * c0 * d2 - b3 * c2 * d0 - d3 * (c0 * b2) - b0 * c3 * d2) * this->a1
        + (c1 * b0 * d3 + c3 * b1 * d0 + b3 * c0 * d1 - b3 * c1 * d0 - d3 * (c0 * b1) - b0 * c3 * d1) * this->a2;

    return result - (c1 * b0 * d2 + c2 * b1 * d0 + b2 * c0 * d1 - b2 * c1 * d0 - d2 * (c0 * b1) - b0 * c2 * d1) * this->a3;
}

// VERIFY: 52341b0c auto
// OFFSET: 0x004C1B30
void C44Matrix::Translate(const C3Vector& move) {
    this->d0 = this->c0 * move.z + this->b0 * move.y + move.x * this->a0 + this->d0;
    this->d1 = this->c1 * move.z + this->b1 * move.y + this->a1 * move.x + this->d1;
    this->d2 = this->c2 * move.z + this->b2 * move.y + this->a2 * move.x + this->d2;
}

// VERIFY: 29a51461 auto
// OFFSET: 0x004C1B90
void C44Matrix::Scale(const C3Vector& scale) {
    this->a0 = scale.x * this->a0;
    this->a1 = this->a1 * scale.x;
    this->a2 = this->a2 * scale.x;
    this->b0 = this->b0 * scale.y;
    this->b1 = scale.y * this->b1;
    this->b2 = scale.y * this->b2;
    this->c0 = scale.z * this->c0;
    this->c1 = this->c1 * scale.z;
    this->c2 = this->c2 * scale.z;
}

// VERIFY: 3c164388 auto
// OFFSET: 0x004C1BF0
void C44Matrix::Scale(float scale) {
    this->a0 = this->a0 * scale;
    this->a1 = this->a1 * scale;
    this->a2 = this->a2 * scale;
    this->b0 = this->b0 * scale;
    this->b1 = this->b1 * scale;
    this->b2 = this->b2 * scale;
    this->c0 = this->c0 * scale;
    this->c1 = this->c1 * scale;
    this->c2 = scale * this->c2;
}

// VERIFY: e67f98ba auto
// OFFSET: 0x004C1C40
void C44Matrix::FromQuaternion(const C4Quaternion& q, C44Matrix& m) {
    float x2 = q.x * 2.0f;
    float y2 = q.y * 2.0f;
    float z2 = 2.0f * q.z;
    float wx = q.w * x2;
    float wy = q.w * y2;
    float wz = q.w * z2;
    float xx = x2 * q.x;
    float xy = q.x * y2;
    float xz = q.x * z2;
    float yy = y2 * q.y;
    float yz = q.y * z2;
    float zz = z2 * q.z;

    m.a0 = 1.0f - (zz + yy);
    m.a1 = xy + wz;
    m.a2 = xz - wy;
    m.b0 = xy - wz;
    m.b1 = 1.0f - (zz + xx);
    m.b2 = yz + wx;
    m.c0 = wy + xz;
    m.c1 = yz - wx;
    m.c2 = 1.0f - (xx + yy);
}

// VERIFY: 9230ed60 manual
// OFFSET: 0x004C1D80
C44Matrix::C44Matrix(const C33Matrix& m) {
    this->a0 = m.a0;
    this->a1 = m.a1;
    this->a2 = m.a2;
    this->a3 = 0.0f;
    this->b0 = m.b0;
    this->b1 = m.b1;
    this->b2 = m.b2;
    this->b3 = 0.0f;
    this->c0 = m.c0;
    this->c1 = m.c1;
    this->c2 = m.c2;
    this->c3 = 0.0f;
    this->d0 = 0.0f;
    this->d1 = 0.0f;
    this->d2 = 0.0f;
    this->d3 = 1.0f;
}

// VERIFY: 639b5ec2 manual
// OFFSET: 0x004C1DE0
C44Matrix::C44Matrix(const C4Quaternion& q) {
    this->a3 = 0.0f;
    this->b3 = 0.0f;
    this->c3 = 0.0f;
    this->d0 = 0.0f;
    this->d1 = 0.0f;
    this->d2 = 0.0f;
    this->d3 = 1.0f;
    FromQuaternion(q, *this);
}

// VERIFY: 16753655 manual
// OFFSET: 0x004C1E60
C44Matrix operator+(const C44Matrix& l, const C44Matrix& r) {
    C44Matrix result;
    result.a0 = l.a0 + r.a0;
    result.a1 = l.a1 + r.a1;
    result.a2 = l.a2 + r.a2;
    result.a3 = l.a3 + r.a3;
    result.b0 = l.b0 + r.b0;
    result.b1 = l.b1 + r.b1;
    result.b2 = l.b2 + r.b2;
    result.b3 = l.b3 + r.b3;
    result.c0 = l.c0 + r.c0;
    result.c1 = l.c1 + r.c1;
    result.c2 = l.c2 + r.c2;
    result.c3 = l.c3 + r.c3;
    result.d0 = l.d0 + r.d0;
    result.d1 = l.d1 + r.d1;
    result.d2 = l.d2 + r.d2;
    result.d3 = l.d3 + r.d3;
    return result;
}

// VERIFY: 11dc36b4 auto
// OFFSET: 0x004C1F00
C44Matrix operator*(const C44Matrix& l, const C44Matrix& r) {
    C44Matrix result;
    result.a0 = r.c0 * l.a2 + l.a1 * r.b0 + l.a3 * r.d0 + l.a0 * r.a0;
    result.a1 = r.c1 * l.a2 + l.a1 * r.b1 + r.a1 * l.a0 + r.d1 * l.a3;
    result.a2 = l.a1 * r.b2 + l.a3 * r.d2 + r.a2 * l.a0 + l.a2 * r.c2;
    result.a3 = r.c3 * l.a2 + r.a3 * l.a0 + l.a1 * r.b3 + r.d3 * l.a3;
    result.b0 = l.b1 * r.b0 + r.c0 * l.b2 + l.b3 * r.d0 + l.b0 * r.a0;
    result.b1 = l.b1 * r.b1 + r.c1 * l.b2 + r.d1 * l.b3 + l.b0 * r.a1;
    result.b2 = r.d2 * l.b3 + l.b2 * r.c2 + l.b1 * r.b2 + l.b0 * r.a2;
    result.b3 = l.b1 * r.b3 + r.d3 * l.b3 + r.c3 * l.b2 + l.b0 * r.a3;
    result.c0 = l.c1 * r.b0 + r.c0 * l.c2 + l.c3 * r.d0 + l.c0 * r.a0;
    result.c1 = l.c1 * r.b1 + r.d1 * l.c3 + r.c1 * l.c2 + l.c0 * r.a1;
    result.c2 = l.c3 * r.d2 + l.c2 * r.c2 + l.c1 * r.b2 + l.c0 * r.a2;
    result.c3 = l.c1 * r.b3 + r.d3 * l.c3 + r.c3 * l.c2 + l.c0 * r.a3;
    result.d0 = l.d1 * r.b0 + r.c0 * l.d2 + l.d3 * r.d0 + l.d0 * r.a0;
    result.d1 = l.d1 * r.b1 + r.d1 * l.d3 + r.c1 * l.d2 + l.d0 * r.a1;
    result.d2 = l.d3 * r.d2 + l.d2 * r.c2 + l.d1 * r.b2 + l.d0 * r.a2;
    result.d3 = l.d1 * r.b3 + r.d3 * l.d3 + r.c3 * l.d2 + l.d0 * r.a3;
    return result;
}

// VERIFY: 007e753f auto
// OFFSET: 0x004C2120
C44Matrix operator*(const C44Matrix& l, float s) {
    C44Matrix result;
    result.a0 = l.a0 * s;
    result.a1 = l.a1 * s;
    result.a2 = l.a2 * s;
    result.a3 = l.a3 * s;
    result.b0 = l.b0 * s;
    result.b1 = l.b1 * s;
    result.b2 = l.b2 * s;
    result.b3 = l.b3 * s;
    result.c0 = l.c0 * s;
    result.c1 = l.c1 * s;
    result.c2 = l.c2 * s;
    result.c3 = l.c3 * s;
    result.d0 = l.d0 * s;
    result.d1 = l.d1 * s;
    result.d2 = l.d2 * s;
    result.d3 = s * l.d3;
    return result;
}

// VERIFY: ea7afa9a manual
// OFFSET: 0x004C2300
C3Vector operator*=(C3Vector& v, const C44Matrix& m) {
    float y = m.c1 * v.z + m.b1 * v.y + m.a1 * v.x + m.d1;
    float z = m.c2 * v.z + m.b2 * v.y + m.a2 * v.x + m.d2;
    v.x = m.c0 * v.z + m.b0 * v.y + v.x * m.a0 + m.d0;
    v.y = y;
    v.z = z;
    return v;
}

// VERIFY: 09407440 auto
// OFFSET: 0x004C2370
C44Matrix& C44Matrix::operator*=(const C44Matrix& m) {
    *this = *this * m;
    return *this;
}

// VERIFY: 00776b33 auto
// OFFSET: 0x004C23A0
C44Matrix& C44Matrix::operator*=(float s) {
    *this = *this * s;
    return *this;
}

// VERIFY: 606764cd manual
// OFFSET: 0x004C23D0
C44Matrix C44Matrix::Transpose() const {
    C44Matrix result;
    result.a0 = this->a0;
    result.a1 = this->b0;
    result.a2 = this->c0;
    result.a3 = this->d0;
    result.b0 = this->a1;
    result.b1 = this->b1;
    result.b2 = this->c1;
    result.b3 = this->d1;
    result.c0 = this->a2;
    result.c1 = this->b2;
    result.c2 = this->c2;
    result.c3 = this->d2;
    result.d0 = this->a3;
    result.d1 = this->b3;
    result.d2 = this->c3;
    result.d3 = this->d3;
    return result;
}

// VERIFY: 1a5a19b8 manual
// OFFSET: 0x004C2440
C44Matrix C44Matrix::Adjoint() const {
    float a0 = this->a0;
    float a1 = this->a1;
    float a2 = this->a2;
    float a3 = this->a3;
    float b0 = this->b0;
    float b1 = this->b1;
    float b2 = this->b2;
    float b3 = this->b3;
    float c0 = this->c0;
    float c1 = this->c1;
    float c2 = this->c2;
    float c3 = this->c3;
    float d0 = this->d0;
    float d1 = this->d1;
    float d2 = this->d2;
    float d3 = this->d3;

    C44Matrix result;
    result.a0 = b2 * c3 * d1 + b3 * c1 * d2 + b1 * c2 * d3 - b3 * c2 * d1 - b2 * c1 * d3 - b1 * c3 * d2;
    result.a1 = -(a2 * c3 * d1 + a3 * c1 * d2 + a1 * c2 * d3 - a3 * c2 * d1 - a2 * c1 * d3 - a1 * c3 * d2);
    result.a2 = a1 * b2 * d3 + a2 * b3 * d1 + a3 * b1 * d2 - a3 * b2 * d1 - a2 * b1 * d3 - a1 * b3 * d2;
    result.a3 = -(a2 * b3 * c1 + a3 * b1 * c2 + a1 * b2 * c3 - a3 * b2 * c1 - a2 * b1 * c3 - a1 * b3 * c2);
    result.b0 = -(b3 * c0 * d2 + b0 * c2 * d3 + b2 * c3 * d0 - b3 * c2 * d0 - b2 * c0 * d3 - b0 * c3 * d2);
    result.b1 = a2 * c3 * d0 + a3 * c0 * d2 + a0 * c2 * d3 - a3 * c2 * d0 - a2 * c0 * d3 - a0 * c3 * d2;
    result.b2 = -(a2 * b3 * d0 + a3 * b0 * d2 + a0 * b2 * d3 - a3 * b2 * d0 - a2 * b0 * d3 - a0 * b3 * d2);
    result.b3 = a2 * b3 * c0 + a3 * b0 * c2 + a0 * b2 * c3 - a3 * b2 * c0 - a2 * b0 * c3 - a0 * b3 * c2;
    result.c0 = b1 * c3 * d0 + b3 * c0 * d1 + b0 * c1 * d3 - b3 * c1 * d0 - b1 * c0 * d3 - b0 * c3 * d1;
    result.c1 = -(a1 * c3 * d0 + a3 * c0 * d1 + a0 * c1 * d3 - a3 * c1 * d0 - a1 * c0 * d3 - a0 * c3 * d1);
    result.c2 = a1 * b3 * d0 + a3 * b0 * d1 + a0 * b1 * d3 - a3 * b1 * d0 - a1 * b0 * d3 - a0 * b3 * d1;
    result.c3 = -(a1 * b3 * c0 + a3 * b0 * c1 + a0 * b1 * c3 - a3 * b1 * c0 - a1 * b0 * c3 - a0 * b3 * c1);
    result.d0 = -(b2 * c0 * d1 + b0 * c1 * d2 + b1 * c2 * d0 - b2 * c1 * d0 - b1 * c0 * d2 - b0 * c2 * d1);
    result.d1 = a1 * c2 * d0 + a2 * c0 * d1 + a0 * c1 * d2 - a2 * c1 * d0 - a1 * c0 * d2 - a0 * c2 * d1;
    result.d2 = -(a1 * b2 * d0 + a2 * b0 * d1 + a0 * b1 * d2 - a2 * b1 * d0 - a1 * b0 * d2 - a0 * b2 * d1);
    result.d3 = a0 * b1 * c2 + a1 * b2 * c0 + a2 * b0 * c1 - b1 * a2 * c0 - c2 * (b0 * a1) - a0 * b2 * c1;
    return result;
}

// VERIFY: 0cf132bb auto
// OFFSET: 0x004C2F90
C44Matrix C44Matrix::Inverse(float determinant) const {
    return this->Adjoint() * (1.0f / determinant);
}

// VERIFY: 8fbe6bb9 manual
// OFFSET: 0x004C2FC0
C44Matrix C44Matrix::AffineInverse() const {
    C33Matrix rotation(*this);

    C44Matrix result;
    result.a0 = rotation.a0;
    result.a1 = rotation.b0;
    result.a2 = rotation.c0;
    result.a3 = 0.0f;
    result.b3 = 0.0f;
    result.b0 = rotation.a1;
    result.b1 = rotation.b1;
    result.b2 = rotation.c1;
    result.c0 = rotation.a2;
    result.c1 = rotation.b2;
    result.c2 = rotation.c2;
    result.c3 = 0.0f;
    result.d3 = 1.0f;

    float x = -this->d0;
    float y = -this->d1;
    float z = -this->d2;
    result.d0 = rotation.a0 * x + rotation.a1 * y + rotation.a2 * z;
    result.d1 = rotation.b0 * x + rotation.b2 * z + y * rotation.b1;
    result.d2 = z * rotation.c2 + y * rotation.c1 + x * rotation.c0;

    return result;
}

// VERIFY: 61be4e70 manual
// OFFSET: 0x004C30A0
C44Matrix C44Matrix::AffineInverse(float scale) const {
    if (fabs(scale - 1.0f) < 0.00000095367432f) {
        return this->AffineInverse();
    }

    C33Matrix rotation(*this);
    C33Matrix transposed;
    transposed.a0 = rotation.a0;
    transposed.a1 = rotation.b0;
    transposed.a2 = rotation.c0;
    transposed.b0 = rotation.a1;
    transposed.b1 = rotation.b1;
    transposed.b2 = rotation.c1;
    transposed.c0 = rotation.a2;
    transposed.c1 = rotation.b2;
    transposed.c2 = rotation.c2;

    C44Matrix result(transposed);
    float s = 1.0f / (scale * scale);
    result.a0 = result.a0 * s;
    result.a1 = result.a1 * s;
    result.a2 = result.a2 * s;
    result.b0 = result.b0 * s;
    result.b1 = result.b1 * s;
    result.b2 = result.b2 * s;
    result.c0 = result.c0 * s;
    result.c1 = result.c1 * s;
    result.c2 = s * result.c2;

    C3Vector move;
    move.x = -this->d0;
    move.y = -this->d1;
    move.z = -this->d2;
    result.Translate(move);

    return result;
}

// VERIFY: 1b98d34d manual
// OFFSET: 0x004C31B0
C44Matrix C44Matrix::RotationAroundX(float angle) {
    float c = cos(angle);
    float s = sin(angle);

    C44Matrix result;
    result.a0 = 1.0f;
    result.a1 = 0.0f;
    result.a2 = 0.0f;
    result.a3 = 0.0f;
    result.b0 = 0.0f;
    result.b1 = c;
    result.b2 = s;
    result.b3 = 0.0f;
    result.c0 = 0.0f;
    result.c1 = -s;
    result.c2 = c;
    result.c3 = 0.0f;
    result.d0 = 0.0f;
    result.d1 = 0.0f;
    result.d2 = 0.0f;
    result.d3 = 1.0f;
    return result;
}

// VERIFY: ce7b26fe manual
// OFFSET: 0x004C3220
C44Matrix C44Matrix::RotationAroundY(float angle) {
    float c = cos(angle);
    float s = sin(angle);

    C44Matrix result;
    result.a0 = c;
    result.a1 = 0.0f;
    result.a2 = -s;
    result.a3 = 0.0f;
    result.b0 = 0.0f;
    result.b1 = 1.0f;
    result.d3 = 1.0f;
    result.b2 = 0.0f;
    result.b3 = 0.0f;
    result.c1 = 0.0f;
    result.c3 = 0.0f;
    result.d0 = 0.0f;
    result.d1 = 0.0f;
    result.d2 = 0.0f;
    result.c0 = s;
    result.c2 = c;
    return result;
}

// VERIFY: 8c65162a manual
// OFFSET: 0x004C3290
C44Matrix C44Matrix::RotationAroundZ(float angle) {
    float c = cos(angle);
    float s = sin(angle);

    C44Matrix result;
    result.a0 = c;
    result.a1 = s;
    result.a2 = 0.0f;
    result.a3 = 0.0f;
    result.b0 = -s;
    result.b1 = c;
    result.b2 = 0.0f;
    result.b3 = 0.0f;
    result.c0 = 0.0f;
    result.c1 = 0.0f;
    result.c2 = 1.0f;
    result.d3 = 1.0f;
    result.c3 = 0.0f;
    result.d0 = 0.0f;
    result.d1 = 0.0f;
    result.d2 = 0.0f;
    return result;
}

// VERIFY: 598de8c7 auto
// OFFSET: 0x004C3300
void C44Matrix::RotateAroundX(float angle) {
    *this = RotationAroundX(angle) * *this;
}

// VERIFY: a0038923 auto
// OFFSET: 0x004C3340
void C44Matrix::RotateAroundY(float angle) {
    *this = RotationAroundY(angle) * *this;
}

// VERIFY: dac14547 auto
// OFFSET: 0x004C3380
void C44Matrix::RotateAroundZ(float angle) {
    *this = RotationAroundZ(angle) * *this;
}

// VERIFY: 3b81b419 auto
// OFFSET: 0x004C33C0
void C44Matrix::Rotate(const C4Quaternion& q) {
    *this = C44Matrix(q) * *this;
}

// VERIFY: a7b7706e manual
// OFFSET: 0x004C3460
C44Matrix C44Matrix::Rotation(float angle, const C3Vector& axis, bool normalized) {
    float x = axis.x;
    float y = axis.y;
    float z = axis.z;

    if (!normalized) {
        float scale = 1.0f / sqrt(z * z + y * y + x * x);
        x = x * scale;
        y = y * scale;
        z = z * scale;
    }

    float c = cos(angle);
    float s = sin(angle);
    float t = 1.0f - c;

    C44Matrix result;
    result.a0 = x * x * t + c;
    result.a1 = y * x * t + s * z;
    result.a2 = z * x * t - y * s;
    result.a3 = 0.0f;
    result.b0 = y * x * t - s * z;
    result.b1 = y * y * t + c;
    result.b2 = t * (z * y) + x * s;
    result.b3 = 0.0f;
    result.c0 = z * x * t + y * s;
    result.c1 = t * (z * y) - x * s;
    result.c2 = c + t * (z * z);
    result.c3 = 0.0f;
    result.d0 = 0.0f;
    result.d1 = 0.0f;
    result.d2 = 0.0f;
    result.d3 = 1.0f;
    return result;
}

// VERIFY: 9912ac4c auto
// OFFSET: 0x0057C2E0
C3Vector TransformDirection(const C3Vector& v, const C44Matrix& m) {
    C3Vector result;
    result.x = m.c0 * v.z + m.b0 * v.y + v.x * m.a0;
    result.y = m.c1 * v.z + m.b1 * v.y + m.a1 * v.x;
    result.z = m.c2 * v.z + m.b2 * v.y + m.a2 * v.x;
    return result;
}

// VERIFY: 80d7997d auto
// OFFSET: 0x006A43A0
C44Matrix C44Matrix::Inverse() const {
    return this->Inverse(this->Determinant());
}

// VERIFY: 158e48ed auto
// OFFSET: 0x004C1730
bool operator==(const C44Matrix& l, const C44Matrix& r) {
    return r.a0 == l.a0 && r.a1 == l.a1 && r.a2 == l.a2 && r.a3 == l.a3 && r.b0 == l.b0 && r.b1 == l.b1 && r.b2 == l.b2 && r.b3 == l.b3 && r.c0 == l.c0 && r.c1 == l.c1 && r.c2 == l.c2 && r.c3 == l.c3 && r.d0 == l.d0 && r.d1 == l.d1 && r.d2 == l.d2 && r.d3 == l.d3;
}

// VERIFY: 0dc594ff auto
// OFFSET: 0x004C1E20
C44Matrix& C44Matrix::operator=(const C4Quaternion& q) {
    FromQuaternion(q, *this);
    this->a3 = 0.0f;
    this->b3 = 0.0f;
    this->c3 = 0.0f;
    this->d0 = 0.0f;
    this->d1 = 0.0f;
    this->d2 = 0.0f;
    this->d3 = 1.0f;
    return *this;
}

// VERIFY: 12861045 auto
// OFFSET: 0x004C2210
C3Vector operator*(const C44Matrix& m, const C3Vector& v) {
    C3Vector result;
    result.x = m.a2 * v.z + m.a1 * v.y + m.a0 * v.x + m.a3;
    result.y = m.b2 * v.z + m.b0 * v.x + m.b1 * v.y + m.b3;
    result.z = m.c2 * v.z + m.c0 * v.x + m.c1 * v.y + m.c3;
    return result;
}

// VERIFY: 124d7460 auto
// OFFSET: 0x004C3680
C44Matrix::C44Matrix(const C34Matrix& m) {
    this->a0 = m.a0;
    this->a1 = m.a1;
    this->a2 = m.a2;
    this->a3 = 0.0f;
    this->b0 = m.b0;
    this->b1 = m.b1;
    this->b2 = m.b2;
    this->b3 = 0.0f;
    this->c0 = m.c0;
    this->c1 = m.c1;
    this->c2 = m.c2;
    this->c3 = 0.0f;
    this->d0 = m.d0;
    this->d1 = m.d1;
    this->d2 = m.d2;
    this->d3 = 1.0f;
}

// VERIFY: d0f80fc6 auto
// OFFSET: 0x0078FBD0
C44Matrix::C44Matrix(const C3Vector& x, const C3Vector& y, const C3Vector& z, const C3Vector& w) {
    this->a0 = x.x;
    this->a1 = x.y;
    this->a2 = x.z;
    this->a3 = 0.0f;
    this->b0 = y.x;
    this->b1 = y.y;
    this->b2 = y.z;
    this->b3 = 0.0f;
    this->c0 = z.x;
    this->c1 = z.y;
    this->c2 = z.z;
    this->c3 = 0.0f;
    this->d0 = w.x;
    this->d1 = w.y;
    this->d2 = w.z;
    this->d3 = 1.0f;
}
