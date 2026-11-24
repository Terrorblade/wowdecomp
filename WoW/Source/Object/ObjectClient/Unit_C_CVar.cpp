#include "Unit_C.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"

// STATIC: 0x00CA11A0
CVar* CGUnit_C::s_cvShowFootPrintParticles;

// STATIC: 0x00CA1194
CVar* CGUnit_C::s_cvPathingDistTolerance;

// VERIFY: 18dce5cf auto
// OFFSET: 0x00715330
void CGUnit_C::ClientInitialize() {
    CGUnit_C::s_cvShowFootPrintParticles = CVar::Register("showfootprintparticles", "toggles rendering of footprint particles", CVAR_ARCHIVE, "1", nullptr, 1, false, nullptr, false);
    CGUnit_C::s_cvPathingDistTolerance = CVar::Register("pathDistTol", "Sets acceptable distance from pathing destination in yards", CVAR_NONE, "1", nullptr, 4, false, nullptr, false);
}
