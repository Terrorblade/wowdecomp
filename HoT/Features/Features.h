#ifndef HOT_FEATURES_FEATURES_H
#define HOT_FEATURES_FEATURES_H

#include <cstdint>

struct lua_State;
struct PlayerQuestLogEntry;

namespace HoT {
class Module;
}

namespace HoT::Feature {

enum FeatureId : uint32_t {
    FEATURE_GEM_SYSTEM = 0,
    FEATURE_AOE_LOOT = 1,
    FEATURE_TRANSMOG = 2,
    FEATURE_PET_AUTO_LOOT = 3,
};

// Server toggled systems, false until the server sent its mask.
bool IsEnabled(uint32_t featureId);

Module* GetModule();

}

namespace HoT::CooldownRate {

// Resets the rate, called after Spell_C_ClearCooldowns.
void Forget();

// Called from CGCooldown::SetCooldown, OnLayerUpdate and the destructor to keep scaled cooldown frames in sync.
void OnCooldownSet(const void* frame, uint32_t start, uint32_t duration);
void OnCooldownUpdate(const void* frame, uint32_t& startTime);
void OnCooldownDestroyed(const void* frame);

Module* GetModule();

}

namespace HoT::Crafting {

bool IsOpen();

// Returns true when crafting took the click, called from Script_UseContainerItem with its (bag, slot) arguments.
bool HandleUseContainerItem(lua_State* L);

Module* GetModule();

}

namespace HoT::GameTele {
Module* GetModule();
}

namespace HoT::GroupFinder {
Module* GetModule();
}

namespace HoT::ItemGen {
Module* GetModule();
}

namespace HoT::ItemLevel {
Module* GetModule();
}

namespace HoT::LootWindow {

// The lootStayOpenWhileMoving setting.
bool StaysOpenWhileMoving();

Module* GetModule();

}

namespace HoT::Minigame {
Module* GetModule();
}

namespace HoT::MovementForce {
Module* GetModule();
}

namespace HoT::Ping {
Module* GetModule();
}

namespace HoT::PlayerSettings {
Module* GetModule();
}

namespace HoT::QuestLog {

// The quest log holds 100 server pushed slots instead of the 25 player descriptor slots.
constexpr uint32_t NUM_SLOTS = 100;
constexpr uint32_t NUM_ENTRIES = 160;
constexpr uint32_t NUM_SORT_TYPES = 32;
constexpr uint32_t NUM_POIS = 100;

// Null when the slot is out of range.
PlayerQuestLogEntry* GetSlot(uint32_t slot);

// Party members are answered from the cache the server sends, a missing member is requested at most every 3 seconds.
bool IsUnitOnQuest(uint64_t guid, int32_t questId);

// Clears the slots and the party cache, called from CGQuestLog::InitializeGame and ShutdownGame.
void Reset();

// Called from CGPartyInfo::RemoveAll.
void ClearPartyQuests();

Module* GetModule();

}

namespace HoT::Talent {
Module* GetModule();
}

namespace HoT::TestFrame {
Module* GetModule();
}

namespace HoT::TowerDefence {
Module* GetModule();
}

namespace HoT::Transmog {
Module* GetModule();
}

#endif
