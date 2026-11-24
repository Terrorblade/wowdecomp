#ifndef WOW_SOURCE_OBJECT_OBJECTCLIENT_VISIBLEITEM_C_H
#define WOW_SOURCE_OBJECT_OBJECTCLIENT_VISIBLEITEM_C_H

#include "WoW/Common/ComponentCore/Types.h"

#include <cstddef>
#include <cstdint>

class VisibleItem_C {
    public:
    /* 0000 */ int32_t m_itemId;
    /* 0004 */ uint16_t m_enchantment [2];

    uint32_t GetClassID();
    uint32_t GetSubtypeID();
    uint32_t GetInventoryType();
    uint32_t GetDisplayID();
    uint32_t GetMaterial();
    uint32_t GetSheatheType();
    uint32_t GetSoundOverrideSubclassID();
    int32_t GetItemEnchantmentVisualID();
};

#endif
