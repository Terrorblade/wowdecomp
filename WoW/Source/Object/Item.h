#ifndef WOW_SOURCE_OBJECT_ITEM_H
#define WOW_SOURCE_OBJECT_ITEM_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/Object/WGUID.h"
#include "WoW/Source/Object/Object.h"

class CGItemData;

class CGItem {
    public:
    static const uint32_t s_mirrorFields[47];

    static uint32_t DescriptorToMirrorOffset(uint32_t offset, uint32_t size, int32_t local, uint32_t base);
    static uint32_t GetSavedIndexFromDataIndex(uint32_t index, uint32_t count, int32_t local);
    static int32_t TotalFieldsSaved();

    /* 0004 */ CGItemData* m_item;
    /* 0008 */ void* m_itemMirror;

    virtual uint32_t GetItemStaticFlags(int32_t a1) = 0;

    int32_t IsUsable() const;
    uint32_t GetEnchantmentId(int32_t slot);
    uint32_t GetEnchantmentDuration(int32_t slot);
    int32_t GetEnchantmentCharges(int32_t slot);
    uint32_t GetDurability() const;
    uint32_t GetMaxDurability() const;
    int32_t GetPetitionSignatures();
};

struct ItemEnchantment {
    /* 0000 */ uint32_t m_id;
    /* 0004 */ uint32_t m_duration;
    /* 0008 */ int16_t m_charges;
    /* 000A */ uint16_t unk_000A;
};

class CGItemData {
    public:
    enum FIELD_ID : uint32_t {
        ITEM_FIELD_OWNER = 0,
        ITEM_FIELD_CONTAINED = 2,
        ITEM_FIELD_CREATOR = 4,
        ITEM_FIELD_GIFTCREATOR = 6,
        ITEM_FIELD_STACK_COUNT = 8,
        ITEM_FIELD_DURATION = 9,
        ITEM_FIELD_SPELL_CHARGES = 10,
        ITEM_FIELD_FLAGS = 15,
        ITEM_FIELD_ENCHANTMENT_1_1 = 16,
        ITEM_FIELD_ENCHANTMENT_1_3 = 18,
        ITEM_FIELD_ENCHANTMENT_2_1 = 19,
        ITEM_FIELD_ENCHANTMENT_2_3 = 21,
        ITEM_FIELD_ENCHANTMENT_3_1 = 22,
        ITEM_FIELD_ENCHANTMENT_3_3 = 24,
        ITEM_FIELD_ENCHANTMENT_4_1 = 25,
        ITEM_FIELD_ENCHANTMENT_4_3 = 27,
        ITEM_FIELD_ENCHANTMENT_5_1 = 28,
        ITEM_FIELD_ENCHANTMENT_5_3 = 30,
        ITEM_FIELD_ENCHANTMENT_6_1 = 31,
        ITEM_FIELD_ENCHANTMENT_6_3 = 33,
        ITEM_FIELD_ENCHANTMENT_7_1 = 34,
        ITEM_FIELD_ENCHANTMENT_7_3 = 36,
        ITEM_FIELD_ENCHANTMENT_8_1 = 37,
        ITEM_FIELD_ENCHANTMENT_8_3 = 39,
        ITEM_FIELD_ENCHANTMENT_9_1 = 40,
        ITEM_FIELD_ENCHANTMENT_9_3 = 42,
        ITEM_FIELD_ENCHANTMENT_10_1 = 43,
        ITEM_FIELD_ENCHANTMENT_10_3 = 45,
        ITEM_FIELD_ENCHANTMENT_11_1 = 46,
        ITEM_FIELD_ENCHANTMENT_11_3 = 48,
        ITEM_FIELD_ENCHANTMENT_12_1 = 49,
        ITEM_FIELD_ENCHANTMENT_12_3 = 51,
        ITEM_FIELD_PROPERTY_SEED = 52,
        ITEM_FIELD_RANDOM_PROPERTIES_ID = 53,
        ITEM_FIELD_DURABILITY = 54,
        ITEM_FIELD_MAXDURABILITY = 55,
        ITEM_FIELD_CREATE_PLAYED_TIME = 56,
        ITEM_FIELD_PAD = 57,
        ITEM_END = 58,
    };

    enum FIELD_SIZE : uint32_t {
        ITEM_FIELD_OWNER_SIZE = 2,
        ITEM_FIELD_CONTAINED_SIZE = 2,
        ITEM_FIELD_CREATOR_SIZE = 2,
        ITEM_FIELD_GIFTCREATOR_SIZE = 2,
        ITEM_FIELD_SPELL_CHARGES_SIZE = 5,
        ITEM_FIELD_ENCHANTMENT_1_1_SIZE = 2,
        ITEM_FIELD_ENCHANTMENT_2_1_SIZE = 2,
        ITEM_FIELD_ENCHANTMENT_3_1_SIZE = 2,
        ITEM_FIELD_ENCHANTMENT_4_1_SIZE = 2,
        ITEM_FIELD_ENCHANTMENT_5_1_SIZE = 2,
        ITEM_FIELD_ENCHANTMENT_6_1_SIZE = 2,
        ITEM_FIELD_ENCHANTMENT_7_1_SIZE = 2,
        ITEM_FIELD_ENCHANTMENT_8_1_SIZE = 2,
        ITEM_FIELD_ENCHANTMENT_9_1_SIZE = 2,
        ITEM_FIELD_ENCHANTMENT_10_1_SIZE = 2,
        ITEM_FIELD_ENCHANTMENT_11_1_SIZE = 2,
        ITEM_FIELD_ENCHANTMENT_12_1_SIZE = 2,
    };

    enum ENCHANTMENT_SLOT : int32_t {
        PERMANENT = 0,
        PETITION = 0,
        TEMPORARY = 1,
        SOCKET_1 = 2,
        SOCKET_2 = 3,
        SOCKET_3 = 4,
        SOCKET_BONUS = 5,
        PRISMATIC = 6,
        RANDOM_PROPERTY_1 = 7,
        RANDOM_PROPERTY_2 = 8,
        RANDOM_PROPERTY_3 = 9,
        RANDOM_PROPERTY_4 = 10,
        RANDOM_PROPERTY_5 = 11,
    };

    static const int32_t NUM_ENCHANTMENTS = 12;
    /* 0000 */ WOWGUID m_itemFieldOwner;
    /* 0008 */ WOWGUID m_itemFieldContained;
    /* 0010 */ WOWGUID m_itemFieldCreator;
    /* 0018 */ WOWGUID m_itemFieldGiftcreator;
    /* 0020 */ int32_t m_itemFieldStackCount;
    /* 0024 */ uint32_t m_itemFieldDuration;
    /* 0028 */ int32_t m_itemFieldSpellCharges [ITEM_FIELD_SPELL_CHARGES_SIZE];
    /* 003C */ uint32_t m_itemFieldFlags;
    /* 0040 */ ItemEnchantment m_itemFieldEnchantment [NUM_ENCHANTMENTS];
    /* 00D0 */ uint32_t m_itemFieldPropertySeed;
    /* 00D4 */ int32_t m_itemFieldRandomPropertiesId;
    /* 00D8 */ uint32_t m_itemFieldDurability;
    /* 00DC */ uint32_t m_itemFieldMaxdurability;
    /* 00E0 */ uint32_t m_itemFieldCreatePlayedTime;
    /* 00E4 */ uint32_t m_itemFieldPad;
};

#endif
