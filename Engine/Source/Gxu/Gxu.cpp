#include <cmath>
#include <cstddef>
#include <cstdint>
#include "Engine/Source/Gxu/Gxu.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/C4Vector.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "Engine/Source/Tempest/CRect.h"
#include "Engine/Source/Tempest/CMath.h"

// VERIFY: 4c6a7536 manual
// OFFSET: 0x006BF370
void GxuXformCreateProjection_Exact(float fovyInRadians, float aspect, float minZ, float maxZ, C44Matrix& dst) {
    if (!(fovyInRadians > 0.0f && fovyInRadians < 3.1415927f)) {
        DebugLog("\"%s\", %s = %g", "fovyInRadians > 0.0f && fovyInRadians < PI", "fovyInRadians", fovyInRadians);
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    if (!(aspect > 0.0f)) {
        DebugLog("\"%s\", %s = %g", "aspect > 0.0f", "aspect", aspect);
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    if (!(minZ < maxZ)) {
        DebugLog("\"%s\", %s = %g, %s = %g", "minZ < maxZ", "minZ", minZ, "maxZ", maxZ);
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    float halfHeight = tan(fovyInRadians * 0.5f);
    halfHeight *= minZ;
    float halfWidth = halfHeight * aspect;

    dst.a0 = minZ / halfWidth;
    dst.b0 = 0.0f;
    dst.c0 = 0.0f;
    dst.d0 = 0.0f;
    dst.a1 = 0.0f;
    dst.b1 = minZ / halfHeight;
    dst.c1 = 0.0f;
    dst.d1 = 0.0f;
    dst.a2 = 0.0f;
    dst.b2 = 0.0f;
    dst.c2 = (minZ + maxZ) / (maxZ - minZ);
    dst.d2 = minZ * (maxZ * -2.0f) / (maxZ - minZ);
    dst.a3 = 0.0f;
    dst.b3 = 0.0f;
    dst.c3 = 1.0f;
    dst.d3 = 0.0f;
}

// VERIFY: 027cbdd8 manual
// OFFSET: 0x006BF4C0
void GxuXformCreateOrtho(float minX, float maxX, float minY, float maxY, float minZ, float maxZ, C44Matrix& dst) {
    if (maxX == minX) {
        DebugValidate("minX != maxX");
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    if (maxY == minY) {
        DebugValidate("minY != maxY");
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    if (!(minZ < maxZ)) {
        DebugValidate("minZ < maxZ");
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    dst.a0 = 2.0f / (maxX - minX);
    dst.b0 = 0.0f;
    dst.c0 = 0.0f;
    dst.d0 = -((minX + maxX) / (maxX - minX));
    dst.a1 = 0.0f;
    dst.b1 = 2.0f / (maxY - minY);
    dst.c1 = 0.0f;
    dst.d1 = -((maxY + minY) / (maxY - minY));
    dst.a2 = 0.0f;
    dst.b2 = 0.0f;
    dst.c2 = 2.0f / (maxZ - minZ);
    dst.d2 = -((maxZ + minZ) / (maxZ - minZ));
    dst.a3 = 0.0f;
    dst.b3 = 0.0f;
    dst.c3 = 0.0f;
    dst.d3 = 1.0f;
}

// VERIFY: 18789a10 manual
// OFFSET: 0x006BF5B0
void GxuXformCreateOrthoDepth(float minX, float maxX, float minY, float maxY, float minZ, float maxZ, C44Matrix& dst) {
    if (maxX == minX) {
        DebugValidate("minX != maxX");
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    if (maxY == minY) {
        DebugValidate("minY != maxY");
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    if (!(minZ < maxZ)) {
        DebugValidate("minZ < maxZ");
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    dst.a0 = 2.0f / (maxX - minX);
    dst.b0 = 0.0f;
    dst.c0 = 0.0f;
    dst.d0 = -((maxX + minX) / (maxX - minX));
    dst.a1 = 0.0f;
    dst.b1 = 2.0f / (maxY - minY);
    dst.c1 = 0.0f;
    dst.d1 = -((maxY + minY) / (maxY - minY));

    bool stereo = g_theGxDevicePtr->StereoEnabled();
    dst.a2 = 0.0f;
    dst.b2 = 0.0f;

    if (stereo) {
        dst.c2 = 1.00008f;
        dst.d2 = -0.400016f;
        dst.a3 = 0.0f;
        dst.b3 = 0.0f;
        dst.c3 = 1.0f;
        dst.d3 = 0.0f;
    } else {
        dst.c2 = 2.0f / (maxZ - minZ);
        dst.d2 = -((maxZ + minZ) / (maxZ - minZ));
        dst.a3 = 0.0f;
        dst.b3 = 0.0f;
        dst.c3 = 0.0f;
        dst.d3 = 1.0f;
    }
}

// VERIFY: 659431cb auto
// OFFSET: 0x006BF6D0
void GxuXformCalcFrustumCorners(const C44Matrix& view, const C44Matrix& proj, C3Vector* corners) {
    C44Matrix invView = view.Inverse(view.Determinant());
    C44Matrix invProj = proj.Inverse(proj.Determinant());
    C44Matrix toWorld = invProj * invView;
    C4Vector v;

    if (fabs(proj.d3 - 1.0f) >= 0.00000023841858f) {
        float nearW = -proj.d2 / (proj.c2 + 1.0f);
        float farW = -proj.d2 / (proj.c2 - 1.0f);

        v.x = -nearW;
        v.y = -nearW;
        v.z = -nearW;
        v.w = nearW;
        corners[0] = C3Vector(v * toWorld);

        v.x = -nearW;
        v.y = nearW;
        v.z = -nearW;
        v.w = nearW;
        corners[1] = C3Vector(v * toWorld);

        v.x = nearW;
        v.y = nearW;
        v.z = -nearW;
        v.w = nearW;
        corners[2] = C3Vector(v * toWorld);

        v.x = nearW;
        v.y = -nearW;
        v.z = -nearW;
        v.w = nearW;
        corners[3] = C3Vector(v * toWorld);

        v.x = -farW;
        v.y = -farW;
        v.z = farW;
        v.w = farW;
        corners[4] = C3Vector(v * toWorld);

        v.x = -farW;
        v.y = farW;
        v.z = farW;
        v.w = farW;
        corners[5] = C3Vector(v * toWorld);

        v.x = farW;
        v.y = farW;
        v.z = farW;
        v.w = farW;
        corners[6] = C3Vector(v * toWorld);

        v.x = farW;
        v.y = -farW;
        v.z = farW;
        v.w = farW;
        corners[7] = C3Vector(v * toWorld);
    } else {
        v.x = -1.0f;
        v.y = -1.0f;
        v.z = -1.0f;
        v.w = 1.0f;
        corners[0] = C3Vector(v * toWorld);

        v.x = -1.0f;
        v.y = 1.0f;
        v.z = -1.0f;
        v.w = 1.0f;
        corners[1] = C3Vector(v * toWorld);

        v.x = 1.0f;
        v.y = 1.0f;
        v.z = -1.0f;
        v.w = 1.0f;
        corners[2] = C3Vector(v * toWorld);

        v.x = 1.0f;
        v.y = -1.0f;
        v.z = -1.0f;
        v.w = 1.0f;
        corners[3] = C3Vector(v * toWorld);

        v.x = -1.0f;
        v.y = -1.0f;
        v.z = 1.0f;
        v.w = 1.0f;
        corners[4] = C3Vector(v * toWorld);

        v.x = -1.0f;
        v.y = 1.0f;
        v.z = 1.0f;
        v.w = 1.0f;
        corners[5] = C3Vector(v * toWorld);

        v.x = 1.0f;
        v.y = 1.0f;
        v.z = 1.0f;
        v.w = 1.0f;
        corners[6] = C3Vector(v * toWorld);

        v.x = 1.0f;
        v.y = -1.0f;
        v.z = 1.0f;
        v.w = 1.0f;
        corners[7] = C3Vector(v * toWorld);
    }
}

// VERIFY: 2b243284 manual
// OFFSET: 0x006BFB60
int32_t GxuXformCalc3dScreenCoords(uint32_t count, const C3Vector* points, C3Vector* screen) {
    CRect rect = { 0.0f, 0.0f, 0.0f, 0.0f };
    C44Matrix viewProj;
    C44Matrix world;

    g_theGxDevicePtr->CapsWindowSize(rect);
    float minZ = g_theGxDevicePtr->m_viewport.m_z.m_l;
    float maxZ = g_theGxDevicePtr->m_viewport.m_z.m_h;

    GxXformViewProj(viewProj);
    const CGxMatrixStack& stack = g_theGxDevicePtr->m_xforms[GxXform_World];
    world = stack.m_mtx[stack.m_level];
    C44Matrix toScreen = world * viewProj;

    for (uint32_t i = 0; i < count; i++) {
        C4Vector v;
        v.x = points[i].x;
        v.y = points[i].y;
        v.z = points[i].z;
        v.w = 1.0f;
        C4Vector t = v * toScreen;

        if (fabs(t.w - 0.0f) < 0.001f) {
            return 0;
        }

        float invW = 1.0f / t.w;
        screen[i].x = (t.x * invW * 0.5f + 0.5f) * (rect.maxX - rect.minX) + rect.minX;
        screen[i].y = (t.y * invW * 0.5f + 0.5f) * (rect.maxY - rect.minY) + rect.maxY;
        screen[i].z = (invW * t.z * 0.5f + 0.5f) * maxZ - minZ + minZ;
    }

    return 1;
}

// VERIFY: ad3a494d manual
// OFFSET: 0x006BFDA0
void GxuUpdateSingleColorTexture(EGxTexCommand command, uint32_t width, uint32_t height, uint32_t face, uint32_t level, void* userArg, uint32_t& texelStrideInBytes, const void*& texels) {
    // STATIC: 0x00C7D1B0
    static CImVector s_texels[64];

    switch (command) {
        case GxTex_Lock: {
            uint32_t color = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(userArg));

            for (uint32_t i = 0; i < 64; i++) {
                reinterpret_cast<uint32_t*>(s_texels)[i] = color;
            }

            break;
        }

        case GxTex_Latch:
            texelStrideInBytes = 4 * width;
            texels = s_texels;
            break;

        default:
            break;
    }
}

// VERIFY: 12b54d96 auto
// OFFSET: 0x006BFE00
void GxuXformCreateProjection_SG(float fovyInRadians, float aspect, float minZ, float maxZ, C44Matrix& dst) {
    float scale = aspect * aspect + 1.0f;
    scale = sqrt(scale);
    scale = 1.0f / scale;
    scale = scale * fovyInRadians;
    GxuXformCreateProjection_Exact(scale, aspect, minZ, maxZ, dst);
}

// VERIFY: dd24cd86 manual
// OFFSET: 0x006BFE60
void GxuXformCreateLookAtSgCompat(const C3Vector& eye, const C3Vector& center, const C3Vector& up, C44Matrix& dst) {
    dst = C44Matrix();

    C3Vector zv = center - eye;

    if (zv.SquaredMag() < 0.01f || up.SquaredMag() < 0.01f) {
        return;
    }

    zv.Normalize();
    C3Vector xv = C3Vector::Cross(zv, up);
    xv.Normalize();
    C3Vector yv = C3Vector::Cross(xv, zv);
    yv.Normalize();

    dst.a0 = xv.x;
    dst.a1 = yv.x;
    dst.a2 = zv.x;
    dst.b0 = xv.y;
    dst.b1 = yv.y;
    dst.b2 = zv.y;
    dst.c0 = xv.z;
    dst.c1 = yv.z;
    dst.c2 = zv.z;

    dst.Translate(-eye);
}

// VERIFY: e3cf6940 manual
// OFFSET: 0x006C0050
void GxuXformCreateLookAtXXX(const C3Vector& eye, const C3Vector& center, const C3Vector& up, C44Matrix& dst) {
    dst = C44Matrix();

    C3Vector zv = center - eye;

    if (zv.SquaredMag() < 0.01f) {
        DebugValidate("zv.SquaredMag() >= 0.01f");
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    if (up.SquaredMag() < 0.01f) {
        DebugValidate("up.SquaredMag() >= 0.01f");
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    zv.Normalize();
    C3Vector xv = C3Vector::Cross(up, zv);
    xv.Normalize();
    C3Vector yv = C3Vector::Cross(zv, xv);
    yv.Normalize();

    dst.a0 = xv.x;
    dst.a1 = yv.x;
    dst.a2 = zv.x;
    dst.b0 = xv.y;
    dst.b1 = yv.y;
    dst.b2 = zv.y;
    dst.c0 = xv.z;
    dst.c1 = yv.z;
    dst.c2 = zv.z;

    dst.Translate(-eye);
}
