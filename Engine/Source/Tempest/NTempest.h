#ifndef ENGINE_SOURCE_TEMPEST_NTEMPEST_H
#define ENGINE_SOURCE_TEMPEST_NTEMPEST_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Tempest/C3Vector.h"

class C2Vector;
class C3Ray;
class C4Plane;
class CAaBox;
class CAaSphere;
struct C3Segment;

enum SolidIntersect {
    SOLID_NONE = 0,
    SOLID_SPHERE = 1,
    SOLID_BOX = 2,
    SOLID_BOTH = 3
};

namespace NTempest {

    float Distance(const CAaBox& box, const C3Vector& point);
    float DistanceFromPolygon(const C3Vector& point, const C3Vector* verts, uint32_t count, const C4Plane& plane);
    float DistanceFromPolygonEdge(const C3Vector& point, const C3Vector* verts, uint32_t count);
    float DistanceSquared(const CAaBox& box, const C2Vector& point);
    float DistanceSquared(const CAaBox& box, const C3Vector& point);
    float DistanceSquared(const C3Segment& segment, const C3Vector& point, float* t);
    bool HitBoundingBox(const float* origin, const float* dir, const float* minB, const float* maxB, float* t, float* coord);
    bool Intersect(const C3Ray& ray, const C3Vector* verts, const uint16_t* indices, float* t, C2Vector* uv, float tolerance);
    bool Intersect(const C3Ray& ray, const C3Vector* verts, const uint32_t* indices, float* t, C2Vector* uv, float tolerance);
    bool Intersect(const C3Ray& ray, const C4Plane& plane, float* t, C3Vector* hit, float tolerance);
    bool Intersect(const C3Ray& ray, const CAaBox& box, float* t, C3Vector* hit);
    bool Intersect(const C3Vector& point, const C3Vector* verts, uint32_t count, C3Vector::EAxis axis);
    bool Intersect2d(const CAaBox& box, const CAaSphere& sphere, SolidIntersect mode);
    bool Intersect2dBoxSphere(const float* minB, const float* maxB, const float* center, float radius, SolidIntersect mode);
}

void HSVtoRGB(const C3Vector& hsv, C3Vector& rgb);
float PerlinNoise2D(float x, float y, int32_t octaves);
void RGBtoHSV(const C3Vector& rgb, C3Vector& hsv);

#endif
