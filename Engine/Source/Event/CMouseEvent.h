#ifndef ENGINE_SOURCE_EVENT_CMOUSEEVENT_H
#define ENGINE_SOURCE_EVENT_CMOUSEEVENT_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Event/CEvent.h"
#include "Engine/Source/Event/Types.h"

enum : uint32_t {
    EVENT_MOUSE_DOWN = 0x400500C8,
    EVENT_MOUSE_UP = 0x400500C9,
    EVENT_MOUSE_MOVE = 0x400500CA,
    EVENT_MOUSE_MOVE_RELATIVE = 0x400500CB,
    EVENT_MOUSE_WHEEL = 0x400500CD,
};

class CMouseEvent : public CEvent {
    public:
    /* 0010 */ MouseEventData m_data;

    CMouseEvent();
    CMouseEvent& operator=(const EVENT_DATA_MOUSE& data);
};

#endif
