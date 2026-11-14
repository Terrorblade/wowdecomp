#ifndef HOT_FEATURES_VOIDFLOORS_H
#define HOT_FEATURES_VOIDFLOORS_H

#include <cstdint>

class C3Vector;

// Fall into void floors for spots too small for the ADT MFBO grid (3x3 heights per tile).
namespace HoT::VoidFloors {

// Raises the MFBO lower bound at pos to any custom floor covering it, returns the new result.
int32_t Apply(const C3Vector& pos, float* height, int32_t result);

// For the editor overlay, all on the current map.
// Floor i, radius 0 covers the whole map.
bool GetCurrent(uint32_t index, float* x, float* y, float* radius, float* floor);
// Point i of exclusion outline n.
bool GetExclusionPoint(uint32_t exclusion, uint32_t point, float* x, float* y);
bool IsExcluded(float x, float y);

}

#endif
