#include <cmath>
#include <cstddef>
#include <cstdint>
#include "Engine/Source/Tempest/C3Spline_Bezier3.h"
#include "Engine/Source/Tempest/C34Matrix.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Tempest/CMath.h"
#include "Engine/Source/Storm/H/storm.h"

// STATIC: 0x00AC3778
static const C44Matrix s_bezier3Basis(-1.0f, 3.0f, -3.0f, 1.0f, 3.0f, -6.0f, 3.0f, 0.0f, -3.0f, 3.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);

// VERIFY: dc9fe2ff manual
// OFFSET: 0x004C3E00
void C3Spline_Bezier3::ParametricSegT(float t, uint32_t& seg, float& segT) {
    float segCount = static_cast<float>(this->m_pointCount / 3);

    seg = CMath::fint_(segCount * t - 0.5f);
    segT = (t - 1.0f / segCount * seg) * segCount;
}

// VERIFY: 52f16011 manual
// OFFSET: 0x004C3E70
void C3Spline_Bezier3::EvaluateDer1(uint32_t seg, float t, C3Vector& vel) {
    // STATIC: 0x00B4A308
    static const C34Matrix s_bezier3Der1 = { -3.0f, 6.0f, -3.0f, 9.0f, -12.0f, 3.0f, -9.0f, 6.0f, 0.0f, 3.0f, 0.0f, 0.0f };

    C3Spline::EvaluateDer1(3 * seg, t, C44Matrix(s_bezier3Der1), vel);
}

// VERIFY: ab3e3930 auto
// OFFSET: 0x004C3F30
float C3Spline_Bezier3::ILength() {
    return C3Spline::ILength(this->m_pointCount / 3);
}

// VERIFY: acbe268f manual
// OFFSET: 0x004C3F50
void C3Spline_Bezier3::IValidateCache() {
    uint32_t segCount = this->m_pointCount / 3;

    for (uint32_t i = 0; i < segCount; i++) {
        this->SetCachedSegLength(C3Spline::SegLength(3 * i, s_bezier3Basis), i);
    }
}

// VERIFY: 6b4b8613 auto
// OFFSET: 0x004C4880
void C3Spline_Bezier3::IPosParametric(float t, C3Vector& pos) {
    uint32_t seg;
    float segT;

    this->ParametricSegT(t, seg, segT);
    C3Spline::Evaluate(3 * seg, segT, s_bezier3Basis, pos);
}

// VERIFY: f36e188f auto
// OFFSET: 0x004C48D0
void C3Spline_Bezier3::IPosArclength(float t, C3Vector& pos) {
    uint32_t seg;
    float segT;

    this->ArclengthSegT(t, s_bezier3Basis, this->m_pointCount / 3, seg, segT);
    C3Spline::Evaluate(3 * seg, segT, s_bezier3Basis, pos);
}

// VERIFY: a40845ff auto
// OFFSET: 0x004C4930
void C3Spline_Bezier3::IVelParametric(float t, C3Vector& vel) {
    uint32_t seg;
    float segT;

    this->ParametricSegT(t, seg, segT);
    this->EvaluateDer1(seg, segT, vel);
}

// VERIFY: 235feadf auto
// OFFSET: 0x004C4970
void C3Spline_Bezier3::IVelArclength(float t, C3Vector& vel) {
    uint32_t seg;
    float segT;

    this->ArclengthSegT(t, s_bezier3Basis, this->m_pointCount / 3, seg, segT);
    this->EvaluateDer1(seg, segT, vel);
}

// VERIFY: 72cbfd7e manual
// OFFSET: 0x004C49C0
void C3Spline_Bezier3::IFrameArclength(float t, C44Matrix& frame) {
    uint32_t seg;
    float segT;

    this->ArclengthSegT(t, s_bezier3Basis, this->m_pointCount / 3, seg, segT);
    C3Spline::Evaluate(3 * seg, segT, s_bezier3Basis, *reinterpret_cast<C3Vector*>(&frame.d0));

    C3Vector vel(0.0f, 0.0f, 0.0f);
    this->EvaluateDer1(seg, segT, vel);

    float length = sqrt(vel.z * vel.z + vel.y * vel.y + vel.x * vel.x);

    if (!(fabs(length) < 0.0099999998f)) {
        float inv = 1.0f / length;
        vel.x = vel.x * inv;
        frame.a0 = vel.x;
        vel.y = vel.y * inv;
        frame.a1 = vel.y;
        vel.z = vel.z * inv;
        frame.a2 = vel.z;
    }

    frame.b0 = -frame.a1;
    frame.b1 = frame.a0;
    frame.b2 = 0.0f;

    float inv = 1.0f / sqrt(frame.b0 * frame.b0 + frame.b1 * frame.b1);
    frame.b0 = inv * frame.b0;
    frame.b1 = inv * frame.b1;

    frame.c0 = -(frame.a2 * frame.b1);
    frame.c1 = frame.a2 * frame.b0;
    frame.c2 = frame.b1 * frame.a0 - frame.b0 * frame.a1;
}

// VERIFY: 3eb1cf44 manual
// OFFSET: 0x004C4D50
void C3Spline_Bezier3::ISetPoints(const C3Vector* points, uint32_t count) {
    uint32_t segCount = count / 3;
    this->m_segCount = segCount;

    if (segCount > 25) {
        this->m_extraSegLength.SetCount(segCount - 25);
    }

    C3Spline::ISetPoints(points, 3 * segCount - 3 + (segCount ? 4 : 0));
}
