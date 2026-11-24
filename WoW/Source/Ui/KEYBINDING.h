#ifndef WOW_SOURCE_UI_KEYBINDING_H
#define WOW_SOURCE_UI_KEYBINDING_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Base/RCString.h"
#include "Engine/Source/Storm/H/stpl.h"

struct HASHKEY_STRI;
class KEYBINDING;

struct KEYBINDINGDATA {
    /* 0000 */ int32_t m_index;
    /* 0004 */ RCString m_command;
};

class KEYBINDING : public TSHashObject<KEYBINDING, HASHKEY_STRI> {
    public:

    enum FLAGS : uint32_t {
        NONE = 0x0,
        DEFAULT = 0x1,
    };

    /* 0018 */ uint32_t m_flags;
    /* 001C */ KEYBINDINGDATA m_data [4];

    KEYBINDING();
    KEYBINDING& operator=(const KEYBINDING& source);
};

#endif
