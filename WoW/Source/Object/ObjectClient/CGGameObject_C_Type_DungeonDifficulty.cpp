#include "CGGameObject_C_Type_DungeonDifficulty.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/MapRec.h"
#include "WoW/Source/Object/GameObjectDef.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/WorldClient/World.h"

// VERIFY: 4a56a128 manual
// OFFSET: 0x00712820
CGGameObject_C_Type_DungeonDifficulty::CGGameObject_C_Type_DungeonDifficulty(CGGameObject_C* owner) : CGGameObject_C_TypeBase(owner) {
}

// VERIFY: dc085aed auto
// OFFSET: 0x0070DD50
float CGGameObject_C_Type_DungeonDifficulty::GetBaseAlpha() const {
    if (this->m_owner->m_gameObject->m_dynamicFlags & CGGameObjectData::DYNFLAG_UNKNOWN_1) {
        return 1.0f;
    }

    return 0.5f;
}

// VERIFY: b07a95ae manual
// OFFSET: 0x00712870
CGGameObject_C_Type_DungeonDifficulty::~CGGameObject_C_Type_DungeonDifficulty() {
    SI2::StopSound(&this->m_sound, 0, -1.0f, 1);
}

// VERIFY: 826c3847 auto
// OFFSET: 0x00427A90
bool CGGameObject_C_Type_DungeonDifficulty::CanUse() const {
    return false;
}

// VERIFY: bbfb1f20 auto
// OFFSET: 0x0070DD70
void CGGameObject_C_Type_DungeonDifficulty::HandleAnimEvent(uint32_t eventId, uint32_t data, const C3Vector& position, uint32_t time) {
    int32_t render = 0;
    int32_t a3 = 0;
    this->ShouldRender(0, render, a3);

    if (!a3) {
        GameObject_C_ProcessAnimEvent(eventId, this->m_owner->m_gameObject->m_displayID, data, position, &this->m_sound);
    }
}

// VERIFY: 2227204f manual
// OFFSET: 0x007109D0
int32_t CGGameObject_C_Type_DungeonDifficulty::GetMapID() const {
    return this->m_owner->GetPropertyValue(CGameObjectDef::GetPropNum(this->m_owner->m_gameObject->m_typeID, CGameObjectDef::PROP_MAPID));
}

// VERIFY: e3596c37 manual
// OFFSET: 0x00710A10
int32_t CGGameObject_C_Type_DungeonDifficulty::GetDifficulty() const {
    return this->m_owner->GetPropertyValue(CGameObjectDef::GetPropNum(this->m_owner->m_gameObject->m_typeID, CGameObjectDef::PROP_DIFFICULTY));
}

// VERIFY: 53a12509 manual
// OFFSET: 0x00710A50
void CGGameObject_C_Type_DungeonDifficulty::UpdateDisplay(uint32_t time) {
    int32_t hidden = 1;
    MapRec* map = nullptr;

    if (this->m_owner->GetPropertyValue(CGameObjectDef::GetPropNum(this->m_owner->m_gameObject->m_typeID, CGameObjectDef::PROP_MAPID))) {
        map = g_mapDB.GetRecord(this->m_owner->GetPropertyValue(CGameObjectDef::GetPropNum(this->m_owner->m_gameObject->m_typeID, CGameObjectDef::PROP_MAPID)));
    }

    if (!map || map->m_instanceType != MAP_INSTANCE_TYPE_RAID_DUNGEON) {
        int32_t difficulty = this->m_owner->GetPropertyValue(CGameObjectDef::GetPropNum(this->m_owner->m_gameObject->m_typeID, CGameObjectDef::PROP_DIFFICULTY));

        if (CGGameUI::GetCurrentDungeonDifficulty() == difficulty) {
            hidden = 0;
        }
    } else if (map->m_flags & MAP_FLAG_DYNAMIC_DIFFICULTY) {
        int32_t difficulty = this->GetDifficulty();

        if (CGGameUI::GetEffectiveRaidDifficulty() == difficulty) {
            hidden = 0;
        }
    } else {
        int32_t difficulty = this->GetDifficulty();

        if (CGGameUI::GetCurrentRaidDifficulty() == difficulty) {
            hidden = 0;
        } else if (CGGameUI::GetCurrentRaidDifficulty() >= RAID_DIFFICULTY_10MAN_HEROIC) {
            difficulty = this->GetDifficulty();

            if (CGGameUI::GetCurrentRaidDifficulty() - RAID_DIFFICULTY_HEROIC_OFFSET == difficulty && !GetMapDifficultyEntryByMapIdAndDifficulty(this->GetMapID(), CGGameUI::GetCurrentRaidDifficulty(), nullptr)) {
                hidden = 0;
            }
        }
    }

    World::ObjectSetHidden(this->m_owner->m_worldObject, hidden);

    if (hidden && SI2::IsPlaying(&this->m_sound)) {
        SI2::StopSound(&this->m_sound, 0, -1.0f, 1);
    }
}
