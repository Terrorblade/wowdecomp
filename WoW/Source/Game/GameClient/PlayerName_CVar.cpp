#include "PlayerName.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/ConsoleBase/Types.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/Object/ObjectClient/Object_C.h"

// VERIFY: dc49a19d manual
// OFFSET: 0x007E60E0
bool ObjectNameShowCallback(CVar* var, const char* oldValue, const char* value, void* arg) {
    uint32_t mask = s_nameMask;

    if (SStrToInt(value)) {
        s_nameMask |= static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg));
    } else {
        s_nameMask &= ~static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg));
    }

    if (s_nameMask == mask) {
        return true;
    }

    for (PLAYERNAMEDESC* desc = s_playerNames.Head(); desc; desc = s_playerNames.Next(desc)) {
        desc->m_flags |= PLAYERNAMEDESC::NAME_REGENERATE;
    }

    return true;
}

// VERIFY: b96c6600 auto
// OFFSET: 0x007E6150
void PlayerNameRegisterCVars() {
    CVar::Register("UnitNameOwn", nullptr, CVAR_SCOPE_ACCOUNT, "0", ObjectNameShowCallback, CATEGORY_GAME, false, reinterpret_cast<void*>(OBJECT_NAME_OWN), false);
    CVar::Register("UnitNameNPC", nullptr, CVAR_SCOPE_ACCOUNT, "0", ObjectNameShowCallback, CATEGORY_GAME, false, reinterpret_cast<void*>(OBJECT_NAME_NPC), false);
    CVar::Register("UnitNamePlayerGuild", nullptr, CVAR_SCOPE_ACCOUNT, "1", ObjectNameShowCallback, CATEGORY_GAME, false, reinterpret_cast<void*>(OBJECT_NAME_PLAYER_GUILD), false);
    CVar::Register("UnitNamePlayerPVPTitle", nullptr, CVAR_SCOPE_ACCOUNT, "1", ObjectNameShowCallback, CATEGORY_GAME, false, reinterpret_cast<void*>(OBJECT_NAME_PLAYER_PVP_TITLE), false);
    CVar::Register("UnitNameEnemyPlayerName", nullptr, CVAR_SCOPE_ACCOUNT, "1", ObjectNameShowCallback, CATEGORY_GAME, false, reinterpret_cast<void*>(OBJECT_NAME_ENEMY_PLAYER), false);
    CVar::Register("UnitNameEnemyPetName", nullptr, CVAR_SCOPE_ACCOUNT, "1", ObjectNameShowCallback, CATEGORY_GAME, false, reinterpret_cast<void*>(OBJECT_NAME_ENEMY_PET), false);
    CVar::Register("UnitNameEnemyGuardianName", nullptr, CVAR_SCOPE_ACCOUNT, "0", ObjectNameShowCallback, CATEGORY_GAME, false, reinterpret_cast<void*>(OBJECT_NAME_ENEMY_GUARDIAN), false);
    CVar::Register("UnitNameEnemyTotemName", nullptr, CVAR_SCOPE_ACCOUNT, "0", ObjectNameShowCallback, CATEGORY_GAME, false, reinterpret_cast<void*>(OBJECT_NAME_ENEMY_TOTEM), false);
    CVar::Register("UnitNameFriendlyPlayerName", nullptr, CVAR_SCOPE_ACCOUNT, "1", ObjectNameShowCallback, CATEGORY_GAME, false, reinterpret_cast<void*>(OBJECT_NAME_FRIENDLY_PLAYER), false);
    CVar::Register("UnitNameFriendlyPetName", nullptr, CVAR_SCOPE_ACCOUNT, "1", ObjectNameShowCallback, CATEGORY_GAME, false, reinterpret_cast<void*>(OBJECT_NAME_FRIENDLY_PET), false);
    CVar::Register("UnitNameFriendlyGuardianName", nullptr, CVAR_SCOPE_ACCOUNT, "0", ObjectNameShowCallback, CATEGORY_GAME, false, reinterpret_cast<void*>(OBJECT_NAME_FRIENDLY_GUARDIAN), false);
    CVar::Register("UnitNameFriendlyTotemName", nullptr, CVAR_SCOPE_ACCOUNT, "0", ObjectNameShowCallback, CATEGORY_GAME, false, reinterpret_cast<void*>(OBJECT_NAME_FRIENDLY_TOTEM), false);
    CVar::Register("UnitNameNonCombatCreatureName", nullptr, CVAR_SCOPE_ACCOUNT, "0", ObjectNameShowCallback, CATEGORY_GAME, false, reinterpret_cast<void*>(OBJECT_NAME_NON_COMBAT_CREATURE), false);
}
