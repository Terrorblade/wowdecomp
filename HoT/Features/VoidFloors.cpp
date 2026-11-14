#include "HoT/Features/VoidFloors.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/Ui/GameUI.h"

namespace HoT::VoidFloors {

struct VoidFloor {
    int32_t m_mapId;
    float m_x;
    float m_y;
    float m_radius;
    float m_floor;
};

// Keep in sync with WotLKExtensions Rendering/VoidFloors.cpp.
static const VoidFloor s_floors[] = {
    // The Continuum, Titan bowl teleport (area trigger 5879, top at 655).
    { 775, -13.0f, 879.0f, 75.0f, 680.0f },
};

int32_t Apply(const C3Vector& pos, float* height, int32_t result) {
    for (const VoidFloor& floor : s_floors) {
        if (floor.m_mapId != CGGameUI::m_iCurrentMapID) {
            continue;
        }

        float dx = pos.x - floor.m_x;
        float dy = pos.y - floor.m_y;

        if (dx * dx + dy * dy > floor.m_radius * floor.m_radius) {
            continue;
        }

        if (!result || *height < floor.m_floor) {
            *height = floor.m_floor;
        }

        result = 1;
    }

    return result;
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
