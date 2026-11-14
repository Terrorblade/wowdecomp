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
    { 260.0f, -555.0f }, { 280.0f, -555.0f }, { 280.0f, -560.0f }, { 290.0f, -565.0f }, { 290.0f, -575.0f },
    { 285.0f, -575.0f }, { 285.0f, -585.0f }, { 280.0f, -585.0f }, { 280.0f, -590.0f }, { 290.0f, -590.0f },
    { 290.0f, -595.0f }, { 295.0f, -595.0f }, { 295.0f, -580.0f }, { 300.0f, -580.0f }, { 305.0f, -570.0f },
    { 315.0f, -570.0f }, { 320.0f, -580.0f }, { 335.0f, -580.0f }, { 335.0f, -575.0f }, { 340.0f, -575.0f },
    { 340.0f, -555.0f }, { 345.0f, -555.0f }, { 350.0f, -545.0f }, { 360.0f, -545.0f }, { 360.0f, -540.0f },
    { 365.0f, -540.0f }, { 365.0f, -545.0f }, { 385.0f, -545.0f }, { 380.0f, -535.0f }, { 385.0f, -535.0f },
    { 385.0f, -525.0f }, { 390.0f, -525.0f }, { 390.0f, -520.0f }, { 410.0f, -505.0f }, { 410.0f, -480.0f },
    { 415.0f, -480.0f }, { 415.0f, -470.0f }, { 410.0f, -470.0f }, { 415.0f, -460.0f }, { 400.0f, -450.0f },
    { 400.0f, -430.0f }, { 405.0f, -430.0f }, { 405.0f, -425.0f }, { 395.0f, -420.0f }, { 395.0f, -410.0f },
    { 380.0f, -410.0f }, { 380.0f, -405.0f }, { 370.0f, -400.0f }, { 370.0f, -385.0f }, { 365.0f, -385.0f },
    { 365.0f, -380.0f }, { 355.0f, -380.0f }, { 355.0f, -375.0f }, { 340.0f, -375.0f }, { 340.0f, -380.0f },
    { 335.0f, -380.0f }, { 335.0f, -375.0f }, { 300.0f, -375.0f }, { 300.0f, -380.0f }, { 295.0f, -380.0f },
    { 295.0f, -390.0f }, { 290.0f, -390.0f }, { 290.0f, -395.0f }, { 300.0f, -400.0f }, { 300.0f, -410.0f },
    { 290.0f, -415.0f }, { 290.0f, -440.0f }, { 300.0f, -445.0f }, { 300.0f, -455.0f }, { 285.0f, -465.0f },
    { 285.0f, -480.0f }, { 265.0f, -495.0f }, { 260.0f, -520.0f }, { 265.0f, -520.0f }, { 270.0f, -530.0f },
    { 260.0f, -535.0f },
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
