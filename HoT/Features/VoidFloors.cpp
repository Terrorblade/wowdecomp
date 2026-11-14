#include "HoT/Features/VoidFloors.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/Ui/GameUI.h"

namespace HoT::VoidFloors {

struct Point {
    float m_x;
    float m_y;
};

// A floor raises the void floor inside its circle, radius 0 covers the whole map.
struct VoidFloor {
    int32_t m_mapId;
    float m_x;
    float m_y;
    float m_radius;
    float m_floor;
};

// No custom floor applies inside an exclusion, the map's own MFBO still does.
struct Exclusion {
    int32_t m_mapId;
    const Point* m_points;
    uint32_t m_count;
};

// Keep in sync with WotLKExtensions Rendering/VoidFloors.cpp and EmulatorV2 Maps/VoidFloors.cpp.
static const Point s_valourKeep[] = {
    { 258.6f, -556.4f }, { 279.1f, -591.8f }, { 289.2f, -596.8f }, { 294.3f, -596.9f }, { 334.9f, -582.0f },
    { 386.2f, -546.6f }, { 411.9f, -505.7f }, { 417.0f, -480.0f }, { 416.9f, -459.5f }, { 406.6f, -423.7f },
    { 396.3f, -408.5f }, { 365.5f, -378.1f }, { 355.3f, -373.0f }, { 299.3f, -373.1f }, { 294.2f, -378.2f },
    { 289.1f, -388.2f }, { 258.1f, -510.7f },
};

static const VoidFloor s_floors[] = {
    // The Continuum, Titan bowl teleport (area trigger 5879, top at 655).
    { 775, -13.0f, 879.0f, 75.0f, 680.0f },
    // Valour Keep, anywhere outside the keep walls.
    { 763, 0.0f, 0.0f, 0.0f, 86.0f },
};

static const Exclusion s_exclusions[] = {
    // Valour Keep, the keep itself, its underground rooms sit below the floor.
    { 763, s_valourKeep, sizeof(s_valourKeep) / sizeof(s_valourKeep[0]) },
};

static bool Inside(const Exclusion& exclusion, float x, float y) {
    bool inside = false;

    for (uint32_t i = 0, j = exclusion.m_count - 1; i < exclusion.m_count; j = i++) {
        const Point& a = exclusion.m_points[i];
        const Point& b = exclusion.m_points[j];

        if ((a.m_y > y) != (b.m_y > y) && x < (b.m_x - a.m_x) * (y - a.m_y) / (b.m_y - a.m_y) + a.m_x) {
            inside = !inside;
        }
    }

    return inside;
}

int32_t Apply(const C3Vector& pos, float* height, int32_t result) {
    int32_t mapId = CGGameUI::m_iCurrentMapID;

    for (const Exclusion& exclusion : s_exclusions) {
        if (exclusion.m_mapId == mapId && Inside(exclusion, pos.x, pos.y)) {
            return result;
        }
    }

    for (const VoidFloor& floor : s_floors) {
        if (floor.m_mapId != mapId) {
            continue;
        }

        float dx = pos.x - floor.m_x;
        float dy = pos.y - floor.m_y;

        if (floor.m_radius > 0.0f && dx * dx + dy * dy > floor.m_radius * floor.m_radius) {
            continue;
        }

        if (!result || *height < floor.m_floor) {
            *height = floor.m_floor;
        }

        result = 1;
    }

    return result;
}

bool GetExclusionPoint(uint32_t exclusion, uint32_t point, float* x, float* y) {
    for (const Exclusion& entry : s_exclusions) {
        if (entry.m_mapId != CGGameUI::m_iCurrentMapID || exclusion-- != 0) {
            continue;
        }

        if (point >= entry.m_count) {
            return false;
        }

        *x = entry.m_points[point].m_x;
        *y = entry.m_points[point].m_y;
        return true;
    }

    return false;
}

bool IsExcluded(float x, float y) {
    for (const Exclusion& entry : s_exclusions) {
        if (entry.m_mapId == CGGameUI::m_iCurrentMapID && Inside(entry, x, y)) {
            return true;
        }
    }

    return false;
}

bool GetCurrent(uint32_t index, float* x, float* y, float* radius, float* floor) {
    for (const VoidFloor& entry : s_floors) {
        if (entry.m_mapId != CGGameUI::m_iCurrentMapID) {
            continue;
        }

        if (index-- == 0) {
            *x = entry.m_x;
            *y = entry.m_y;
            *radius = entry.m_radius;
            *floor = entry.m_floor;
            return true;
        }
    }

    return false;
}

}
