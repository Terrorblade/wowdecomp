#include "HoT/DB/Refresh.h"
#include "HoT/Core/Modules.h"
#include "HoT/DB/Tables.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Common/ComponentCore/ComponentUtils.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Magic/MagicClient/SpellTable.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/LFGInfo.h"
#include "WoW/Source/Ui/SpellBookFrame.h"
#include "WoW/Source/WorldClient/CDetailDoodadData.h"
#include "WoW/Source/WorldClient/DayNight.h"
#include "WoW/Source/WorldClient/DetailDoodad.h"
#include "WoW/Source/WorldClient/Map.h"

namespace HoT::DB {

static void RefreshLights(const char* stem) {
    // Light.dbc was first transformed before any map loaded, so a reload inside a dungeon must not pick up the dungeon offset.
    int32_t dungeon = CMap::bDungeon;
    CMap::bDungeon = 0;
    InitLights(true);
    CMap::bDungeon = dungeon;

    if (DayNight::g_areaLights.m_count) {
        DayNight::LightLoad(DayNight::g_areaLights, DayNight::s_mapId);
    }
}

// Existing entries keep their loaded model because detail doodad instances hold its texture.
static void RefreshGroundEffectDoodads(const char* stem) {
    uint32_t oldCount = CDetailDoodad::s_doodadList.Count();
    uint32_t count = g_groundEffectDoodadDB.m_maxID + 1;

    if (count > oldCount) {
        CDetailDoodad::s_doodadList.SetCount(count);

        for (uint32_t i = oldCount; i < count; i++) {
            CDetailDoodad::s_doodadList[i] = nullptr;
        }
    }

    for (uint32_t i = 0; i < count; i++) {
        GroundEffectDoodadRec* record = g_groundEffectDoodadDB.GetRecord(i);

        if (!record) {
            continue;
        }

        if (CDetailDoodad::s_doodadList[i]) {
            CDetailDoodad::s_doodadList[i]->m_record = record;
        } else {
            CDetailDoodadData* data = new (__FILE__, __LINE__) CDetailDoodadData();
            data->m_record = record;
            CDetailDoodad::s_doodadList[i] = data;
        }
    }
}

// The old lookups leak on purpose, the component thread reads them without a lock.
static void RefreshCharacterComponents(const char* stem) {
    uint32_t count = 2 * g_chrRacesDB.m_maxID + 2;
    st_race* lookup = nullptr;
    uint32_t* facialFeatures = nullptr;

    if (!BuildComponentArray(count, &lookup) || !CountFacialFeatures(count, &facialFeatures)) {
        return;
    }

    CCharacterComponent::s_chrVarArray = lookup;
    CCharacterComponent::s_characterFacialHairStylesList = facialFeatures;
    CCharacterComponent::s_chrVarArrayLength = count;
}

static void RefreshResistances(const char* stem) {
    InitResistances();
}

static void RefreshTerrainTypes(const char* stem) {
    InitTerrainTypeFlags();
}

static void RefreshItemSubclasses(const char* stem) {
    InitWeaponSubclasses();
    SDBItemSubclassInitialize();
}

static void RefreshGameTables(const char* stem) {
    GameTablesInitialize();
}

// Built per world session by ClientInitializeGame, so out of the world there is nothing to rebuild.
static void RefreshSpellTable(const char* stem) {
    if (!ClntObjMgrGetActivePlayerObj()) {
        return;
    }

    SpellTableDestroy();
    SpellTableInitialize();
}

void GrowKnownSpellBits() {
    if (!s_knownSpellBits.m_count || g_spellDB.m_maxID < 0) {
        return;
    }

    uint32_t count = static_cast<uint32_t>(g_spellDB.m_maxID) + 1;

    if (count <= s_knownSpellBits.m_count) {
        return;
    }

    uint32_t oldWords = s_knownSpellBits.m_bits.m_count;
    uint32_t words = ((count - 1) >> 5) + 1;

    if (words > oldWords) {
        s_knownSpellBits.m_bits.SetCount(words);

        for (uint32_t i = oldWords; i < words; i++) {
            s_knownSpellBits.m_bits.m_data[i] = 0;
        }
    }

    s_knownSpellBits.m_count = count;
}

static void RefreshSpells(const char* stem) {
    GrowKnownSpellBits();
}

static void RefreshLfgEntries(const char* stem) {
    CGLookingForGroup::UpdateLFDEntries();
    CGLookingForGroup::UpdateLFREntries();

    if (CGLookingForGroup::m_playerLockInfo.m_valid) {
        CGLookingForGroup::UpdateRandomDungeons();
    }
}

class RefreshModule : public Module {
    public:
    const char* Name() const override {
        return "DBRefresh";
    }

    void OnInitializeGlobal() override {
        OnTableReloaded("Light", RefreshLights);
        OnTableReloaded("GroundEffectDoodad", RefreshGroundEffectDoodads);
        OnTableReloaded("CharSections", RefreshCharacterComponents);
        OnTableReloaded("CharacterFacialHairStyles", RefreshCharacterComponents);
        OnTableReloaded("ChrRaces", RefreshCharacterComponents);
        OnTableReloaded("Resistances", RefreshResistances);
        OnTableReloaded("TerrainType", RefreshTerrainTypes);
        OnTableReloaded("ItemClass", RefreshItemSubclasses);
        OnTableReloaded("ItemSubClass", RefreshItemSubclasses);
        OnTableReloaded("GameTables", RefreshGameTables);
        OnTableReloaded("SkillLine", RefreshSpellTable);
        OnTableReloaded("SkillLineAbility", RefreshSpellTable);
        OnTableReloaded("SkillRaceClassInfo", RefreshSpellTable);
        OnTableReloaded("SkillCostsData", RefreshSpellTable);
        OnTableReloaded("Spell", RefreshSpells);
        OnTableReloaded("LfgDungeons", RefreshLfgEntries);
        OnTableReloaded("LfgDungeonGroup", RefreshLfgEntries);
        OnTableReloaded("LfgDungeonExpansion", RefreshLfgEntries);
    }
};

Module* GetRefreshModule() {
    static RefreshModule module;
    return &module;
}

}
