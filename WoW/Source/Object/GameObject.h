#ifndef WOW_SOURCE_OBJECT_GAMEOBJECT_H
#define WOW_SOURCE_OBJECT_GAMEOBJECT_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/Object/WGUID.h"
#include "WoW/Source/Object/Object.h"
#include "Engine/Source/Tempest/C4Quaternion.h"

class CGGameObjectData;

enum DESTRUCTIBLE_STATE : int32_t {
    DESTRUCTIBLE_STATE_INTACT = 0,
    DESTRUCTIBLE_STATE_DAMAGED = 1,
    DESTRUCTIBLE_STATE_DESTROYED = 2,
    DESTRUCTIBLE_STATE_REBUILDING = 3,
    NUM_DESTRUCTIBLE_STATES = 4,
};

class CGGameObject {
    public:
    static uint32_t s_mirrorFieldIndex[4];

    static uint32_t DescriptorToMirrorOffset(uint32_t offset, uint32_t size, int32_t local, uint32_t base);
    static uint32_t MirrorIndexFromFieldIndex(uint32_t fieldIndex, uint32_t count, int32_t local);
    static int32_t TotalFieldsSaved();

    /* 0000 */ CGGameObjectData* m_gameObject;
    /* 0004 */ void* m_gameObjectMirror;
};

class CGGameObjectData {
    public:
    enum FIELD_ID : uint32_t {
        OBJECT_FIELD_CREATED_BY = 0,
        GAMEOBJECT_DISPLAYID = 2,
        GAMEOBJECT_FLAGS = 3,
        GAMEOBJECT_PARENTROTATION = 4,
        GAMEOBJECT_DYNAMIC = 8,
        GAMEOBJECT_FACTION = 9,
        GAMEOBJECT_LEVEL = 10,
        GAMEOBJECT_BYTES_1 = 11,
        GAMEOBJECT_END = 12,
    };

    enum FIELD_SIZE : uint32_t {
        OBJECT_FIELD_CREATED_BY_SIZE = 2,
        GAMEOBJECT_PARENTROTATION_SIZE = 4,
    };

    enum : uint16_t {
        DYNFLAG_UNKNOWN_0 = 0x1,
        DYNFLAG_UNKNOWN_1 = 2,
        DYNFLAG_INTERACTION_DISABLED = 0x4,
        DYNFLAG_PASSIVE_HIGHLIGHT = 0x8,
        DYNFLAG_STOP_AT_CURRENT_LOCATION = 0x10,
    };

    enum : uint32_t {
        DESTRUCTIBLE_DATA_SHIFT = 8,
        DESTRUCTIBLE_STATE_SHIFT = 9,
        DESTRUCTIBLE_STATE_IN_DATA_SHIFT = 1,
        DESTRUCTIBLE_DATA_VALUE_MASK = 0xF,
        DESTRUCTIBLE_STATE_IN_DATA_MASK = 0xE,
        DESTRUCTIBLE_STATE_VALUE_MASK = 0x7,
    };

    enum : uint32_t {
        FLAG_UNKNOWN_0 = 0x1,
        FLAG_LOCKED = 2,
        FLAG_UNKNOWN_2 = 0x4,
        FLAG_TRANSPORT = 0x8,
        FLAG_UNKNOWN_4 = 0x10,
        FLAG_INTERACTION_BLOCK_MASK = 0x11,
        FLAG_KEEP_ANIMATION_TIME = 0x80,
        FLAG_DESTRUCTIBLE_IMPACT_TOGGLE = 0x100,
        FLAG_DESTRUCTIBLE_DATA_MASK = 0xF00,
    };
    /* 0000 */ WOWGUID m_createdBy;
    /* 0008 */ uint32_t m_displayID;
    /* 000C */ uint32_t m_flags;
    /* 0010 */ float m_parentRotation[4];
    /* 0020 */ uint16_t m_dynamicFlags;
    /* 0022 */ uint16_t m_pathProgress;
    /* 0024 */ uint32_t m_faction;
    /* 0028 */ uint32_t m_level;
    /* 002C */ int8_t m_state;
    /* 002D */ int8_t m_typeID;
    /* 002E */ int8_t m_artKit;
    /* 002F */ uint8_t m_animProgress;
};

#endif
