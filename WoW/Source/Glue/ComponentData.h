#ifndef WOW_SOURCE_GLUE_COMPONENTDATA_H
#define WOW_SOURCE_GLUE_COMPONENTDATA_H

#include <cstddef>
#include <cstdint>
#include "WoW/Common/ComponentCore/Types.h"
#include "WoW/Source/Glue/Types.h"

class CM2Model;

class ComponentData {
    public:

    enum DATA_FLAGS : uint32_t {
        NPC_SKIN = 0x1,
        HIGH_RESOLUTION = 0x2,
        INITIAL_CLEAR_MASK = 0x3u,
    };

    enum DATA_BITS : int32_t {
        HIGH_RESOLUTION_BIT_INDEX = 1,
    };

    /* 0000 */ CharacterPreferences m_preferences;
    /* 0020 */ CM2Model* m_model;
    /* 0024 */ uint32_t m_flags;
    /* 0028 */ char m_npcSkinTexture [260];
    /* 012C */ uint32_t m_geosets [NUM_CHARGEOSETS];

    ComponentData();
    void SetPreferences(const CharacterPreferences* preferences);
};

#endif
