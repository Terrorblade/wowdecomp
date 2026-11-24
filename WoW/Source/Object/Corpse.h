#ifndef WOW_SOURCE_OBJECT_CORPSE_H
#define WOW_SOURCE_OBJECT_CORPSE_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/Object/WGUID.h"
#include "WoW/Source/Object/Object.h"

class CGCorpseData;

enum CORPSE_FIELD_FLAGS : uint32_t {
    CORPSE_FIELD_FLAG_NONE               = 0x00000000u,
    CORPSE_FIELD_FLAG_BONES              = 0x00000001u,
    CORPSE_FIELD_FLAG_PVP                = 0x00000004u,
    CORPSE_FIELD_FLAG_HIDE_HELM          = 0x00000008u,
    CORPSE_FIELD_FLAG_HIDE_CLOAK         = 0x00000010u,
    CORPSE_FIELD_FLAG_SKINNABLE          = 0x00000020u,
};

enum CORPSE_DYNAMIC_FLAGS : uint32_t {
    CORPSE_DFLAG_LOOTABLE = 0x1,
};

class CGCorpse {
    public:
    static const uint32_t s_mirrorFields[3];

    static uint32_t DescriptorToMirrorOffset(uint32_t offset, uint32_t size, int32_t local, uint32_t base);
    static uint32_t GetSavedIndexFromDataIndex(uint32_t index, uint32_t count, int32_t local);
    static int32_t TotalFieldsSaved();

    /* 0000 */ CGCorpseData* m_corpse;
    /* 0004 */ void* m_corpseMirror;
};

class CGCorpseData {
    public:
    enum FIELD_ID : uint32_t {
        CORPSE_FIELD_OWNER = 0,
        CORPSE_FIELD_PARTY = 2,
        CORPSE_FIELD_DISPLAY_ID = 4,
        CORPSE_FIELD_ITEM = 5,
        CORPSE_FIELD_BYTES_1 = 24,
        CORPSE_FIELD_BYTES_2 = 25,
        CORPSE_FIELD_GUILD = 26,
        CORPSE_FIELD_FLAGS = 27,
        CORPSE_FIELD_DYNAMIC_FLAGS = 28,
        CORPSE_FIELD_PAD = 29,
        CORPSE_END = 30,
    };

    enum FIELD_SIZE : uint32_t {
        CORPSE_FIELD_OWNER_SIZE = 2,
        CORPSE_FIELD_PARTY_SIZE = 2,
        CORPSE_FIELD_ITEM_SIZE = 19,
    };

    /* 0000 */ WOWGUID m_corpseFieldOwner;
    /* 0008 */ WOWGUID m_corpseFieldParty;
    /* 0010 */ uint32_t m_corpseFieldDisplayId;
    /* 0014 */ uint32_t m_corpseFieldItem [19];
    /* 0060 */ uint8_t m_corpseFieldBytes1 [4];
    /* 0064 */ uint8_t m_corpseFieldBytes2 [4];
    /* 0068 */ uint32_t m_corpseFieldGuild;
    /* 006C */ uint32_t m_corpseFieldFlags;
    /* 0070 */ uint32_t m_corpseFieldDynamicFlags;
    /* 0074 */ uint32_t m_corpseFieldPad;
};

#endif
