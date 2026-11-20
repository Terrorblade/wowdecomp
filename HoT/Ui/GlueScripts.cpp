#include "HoT/Ui/GlueScripts.h"
#include "HoT/Core/Log.h"
#include "HoT/Core/Modules.h"
#include "HoT/Lua/Script.h"
#include "Engine/Source/Model2/M2Model.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Source/Glue/CSimpleModelFFX.h"
#include "WoW/Source/Glue/CharacterCreation.h"
#include "WoW/Source/Glue/CharacterSelection.h"

namespace HoT::GlueScripts {

static int32_t Script_TestGlueLua(lua_State* L) {
    return 0;
}

static int32_t Script_OverrideSlotDisplay(lua_State* L) {
    int32_t slot = static_cast<int32_t>(lua_tonumber(L, 1));
    int32_t displayId = static_cast<int32_t>(lua_tonumber(L, 2));
    CCharacterComponent* component = CCharacterCreation::m_character;

    if (!component) {
        LOG_DEBUG << "OverrideSlotDisplay: no character component";
        return 0;
    }

    if (!component->m_data.m_model) {
        LOG_DEBUG << "OverrideSlotDisplay: no base model loaded yet";
        return 0;
    }

    // AddItem indexes m_itemDisplayID with the slot unchecked.
    if (slot < 0 || slot >= NUM_ITEM_SLOTS) {
        return 0;
    }

    component->AddItem(static_cast<ITEM_SLOT>(slot), displayId, 0);
    return 0;
}

// Picking the race that is already selected normally does nothing, this rebuilds it so newly mounted art shows up.
static int32_t Script_RebuildCharacterCreateModel(lua_State* L) {
    uint32_t race = CCharacterCreation::m_raceIndex;
    uint32_t count = CCharacterCreation::m_races.Count();

    if (race >= count || !CCharacterCreation::m_character) {
        LOG_INFO << "glue: rebuild skipped, race " << race << " is not within " << count;
        return 0;
    }

    CCharacterCreation::SetSelectedRace(race, true);
    LOG_INFO << "glue: rebuilt character create model for race " << race;
    return 0;
}

// ShowCharacter skips records that already have a component, so the cached ones are detached and freed first.
static int32_t Script_RebuildCharacterSelectModel(lua_State* L) {
    int32_t index = CCharacterSelection::s_selectionIndex;
    int32_t count = static_cast<int32_t>(CCharacterSelection::s_characterList.Count());

    if (index < 0 || index >= count) {
        LOG_INFO << "glue: select rebuild skipped, index " << index << " of " << count;
        return 0;
    }

    if (CCharacterSelection::m_modelFrame && CCharacterSelection::m_modelFrame->m_model) {
        CCharacterSelection::m_modelFrame->m_model->DetachAllChildrenById(0);
        CCharacterSelection::m_modelFrame->m_model->DetachAllChildrenById(1);
    }

    int32_t cleared = 0;

    for (int32_t i = 0; i < count; i++) {
        CharacterSelectionDisplay& display = CCharacterSelection::s_characterList[i];

        if (display.m_petModel) {
            display.m_petModel->Release();
            display.m_petModel = nullptr;
        }

        if (display.m_component) {
            CCharacterComponent::FreeComponent(display.m_component);
            display.m_component = nullptr;
            cleared++;
        }
    }

    CCharacterSelection::ShowCharacter();
    LOG_INFO << "glue: freed " << cleared << " cached character component(s), rebuilt index " << index;
    return 0;
}

class GlueScriptsModule : public Module {
    public:
    const char* Name() const override {
        return "GlueScripts";
    }

    void OnInitializeGlobal() override {
        Lua::RegisterFunction("TestGlueLua", Script_TestGlueLua, Lua::State::Glue);
        Lua::RegisterFunction("OverrideSlotDisplay", Script_OverrideSlotDisplay, Lua::State::Glue);
        Lua::RegisterFunction("RebuildCharacterCreateModel", Script_RebuildCharacterCreateModel, Lua::State::Glue);
        Lua::RegisterFunction("RebuildCharacterSelectModel", Script_RebuildCharacterSelectModel, Lua::State::Glue);
    }
};

Module* GetModule() {
    static GlueScriptsModule module;
    return &module;
}

}
