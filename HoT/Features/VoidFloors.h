#ifndef HOT_FEATURES_VOIDFLOORS_H
#define HOT_FEATURES_VOIDFLOORS_H

#include <cstdint>

class C3Vector;

// Fall into void floors for spots too small for the ADT MFBO grid (3x3 heights per tile).
namespace HoT::VoidFloors {

// Raises the MFBO lower bound at pos to any custom floor covering it, returns the new result.
int32_t Apply(const C3Vector& pos, float* height, int32_t result);

// Custom floor i on the current map, for the editor overlay.
bool GetCurrent(uint32_t index, float* x, float* y, float* radius, float* floor);

}

#endif
