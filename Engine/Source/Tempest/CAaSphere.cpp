#include <cmath>
#include <cstdint>
#include "Engine/Source/Tempest/CAaSphere.h"
#include "Engine/Source/Storm/H/storm.h"

// VERIFY: 0e73d0a5 manual
// OFFSET: 0x006FC1E0
CAaSphere::CAaSphere() {
    this->r = 0.0f;
}

// VERIFY: 96f5035c manual
// OFFSET: 0x007FA140
CAaBox CAaSphere::Box() const {
    CAaBox result;
    result.b.x = this->c.x - this->r;
    result.b.y = this->c.y - this->r;
    result.b.z = this->c.z - this->r;
    result.t.x = this->c.x + this->r;
    result.t.y = this->c.y + this->r;
    result.t.z = this->c.z + this->r;
    return result;
}

// VERIFY: 34fc52e7 manual
// OFFSET: 0x009855F0
CAaBox CAaSphere::FindExtrema(const CAaSphere* spheres, uint32_t count) {
    CAaBox result;

    if (!count) {
        result.b.x = 0.0f;
        result.b.y = 0.0f;
        result.b.z = 0.0f;
        result.t.x = 0.0f;
        result.t.y = 0.0f;
        result.t.z = 0.0f;
        return result;
    }

    CAaBox box = spheres[0].Box();

    for (uint32_t i = 1; i < count; i++) {
        const CAaSphere& sphere = spheres[i];
        float maxX = sphere.r + sphere.c.x;
        float maxY = sphere.r + sphere.c.y;
        float maxZ = sphere.r + sphere.c.z;
        float minX = sphere.c.x - sphere.r;
        float minY = sphere.c.y - sphere.r;
        float minZ = sphere.c.z - sphere.r;

        if (box.t.z > maxZ) {
            maxZ = box.t.z;
        }

        if (box.t.y > maxY) {
            maxY = box.t.y;
        }

        if (box.t.x > maxX) {
            maxX = box.t.x;
        }

        if (box.b.z < minZ) {
            minZ = box.b.z;
        }

        if (box.b.y < minY) {
            minY = box.b.y;
        }

        if (box.b.x < minX) {
            minX = box.b.x;
        }

        box.b.x = minX;
        box.b.y = minY;
        box.b.z = minZ;
        box.t.x = maxX;
        box.t.y = maxY;
        box.t.z = maxZ;
    }

    result = box;
    return result;
}

// VERIFY: 1d040cbb manual
// OFFSET: 0x00985750
CAaSphere CAaSphere::BoundingAccurate(const CAaSphere* spheres, uint32_t count) {
    CAaSphere result;

    if (!count) {
        result.c.x = 0.0f;
        result.c.y = 0.0f;
        result.c.z = 0.0f;
        result.r = 0.0f;
        return result;
    }

    if (count == 1) {
        result = spheres[0];
        return result;
    }

    CAaBox box = FindExtrema(spheres, count);

    if (box.IsEmpty()) {
        result.c = box.b;
        result.r = 0.0f;
        return result;
    }

    C3Vector center;
    center.x = (box.t.x + box.b.x) * 0.5f;
    center.y = (box.t.y + box.b.y) * 0.5f;
    center.z = 0.5f * (box.t.z + box.b.z);

    float radius = 0.0f;

    for (uint32_t i = 0; i < count; i++) {
        const CAaSphere& sphere = spheres[i];
        float dy = sphere.c.y - center.y;
        float dx = sphere.c.x - center.x;
        float distance = dx * dx + dy * dy + (sphere.c.z - center.z) * (sphere.c.z - center.z);
        float slack = radius - sphere.r;

        if (slack < 0.0f || slack * slack < distance) {
            radius = sqrt(distance) + sphere.r;
        }
    }

    result.c = center;
    result.r = radius;
    return result;
}
