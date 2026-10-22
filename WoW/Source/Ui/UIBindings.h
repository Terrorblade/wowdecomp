#ifndef WOW_SOURCE_UI_UIBINDINGS_H
#define WOW_SOURCE_UI_UIBINDINGS_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Event/Types.h"
#include "Engine/Source/Os/OsFile.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "WoW/Source/Ui/KEYBINDING.h"
#include "WoW/Source/Ui/MODIFIEDCLICK.h"
#include "WoW/Source/Ui/Types.h"

class CKeyEvent;
class CMouseEvent;
class CSimpleFrame;
struct HASHKEY_STR;
struct HASHKEY_STRI;
class KEYBINDING;
struct KEYCOMMAND;
class MODIFIEDCLICK;
struct MD5_CTX;
struct OVERRIDEKEYBINDING;
struct OVERRIDEKEYBINDINGENTRY;
class XMLNode;
struct lua_State;

struct BINDINGPREFIX {
    /* 0000 */ const char* m_prefix;
    /* 0004 */ uint32_t m_length;
};

struct KEYCOMMAND : public TSHashObject<KEYCOMMAND, HASHKEY_STRI> {
    /* 0018 */ int32_t m_index;
    /* 001C */ int32_t m_function;
    /* 0020 */ int32_t m_runOnUp;
    /* 0024 */ int32_t m_pressure;
    /* 0028 */ int32_t m_angle;

    KEYCOMMAND();
};

// INLINED
// m_function is a FrameScript function reference, -1 when the command has no script.
inline KEYCOMMAND::KEYCOMMAND() : m_index(0), m_function(-1), m_runOnUp(0), m_pressure(0), m_angle(0) {
}

struct OVERRIDEKEYBINDINGENTRY : public TSLinkedNode<OVERRIDEKEYBINDINGENTRY> {
    /* 0008 */ void* m_owner;
    /* 000C */ KEYBINDING m_binding;
};

struct OVERRIDEKEYBINDING : public TSHashObject<OVERRIDEKEYBINDING, HASHKEY_STRI> {
    /* 0018 */ TSList<OVERRIDEKEYBINDINGENTRY> m_lists [2];
    /* 0030 */ TSList<OVERRIDEKEYBINDINGENTRY> m_freeList;

    ~OVERRIDEKEYBINDING();
    KEYBINDING* GetActiveBinding();
    void SetOverrideBinding(void* owner, int32_t priority, const char* command);
};

struct CLICKFRAME : public TSHashObject<CLICKFRAME, HASHKEY_STR> {
    /* 0018 */ CSimpleFrame* m_frame;
};

class CGUIBindingsStatus : public CStatus {
    public:
    virtual void Add(STATUS_TYPE severity, const char* format, ...);
};

class CGUIBindings {
    public:
    static CGUIBindings* s_bindings;

    static int32_t AddMetaPrefix(uint32_t modifiers, char*& binding, int32_t& maxLength);
    static void AddModifiers(uint32_t& modifiers, const char*& binding);
    static void Initialize();
    static char* KeyEventToString(const CKeyEvent& evt, char* binding, int32_t maxLength);
    static void LoadBindings();
    static void LoadBindings(BINDING_SET set, const char* data);
    static void LoadBindingsCallback(HOSFILE localFile, HOSFILE file, void* param);
    static char* MouseEventToString(const CMouseEvent& evt, char* binding, int32_t maxLength);
    static void SaveBindings();
    static void SaveBindingsCallback(HOSFILE localFile, HOSFILE file, void* param);
    static void Shutdown();
    static void StripModifiers(uint32_t& modifiers, const char* binding);

    /* 0000 */ int32_t m_numCommands;
    /* 0004 */ int32_t m_numHiddenCommands;
    /* 0008 */ int32_t m_numModifiedClicks;
    /* 000C */ TSHashTable<KEYCOMMAND, HASHKEY_STRI> m_commands;
    /* 0034 */ TSHashTable<KEYBINDING, HASHKEY_STRI> m_bindings [4];
    /* 00D4 */ TSHashTable<OVERRIDEKEYBINDING, HASHKEY_STRI> m_overrideBindings;
    /* 00FC */ TSHashTable<MODIFIEDCLICK, HASHKEY_STRI> m_modifiedClicks;
    /* 0124 */ int32_t m_saveSet;
    /* 0128 */ uint32_t m_dirtySets;
    /* 012C */ int32_t m_keyStateOverride;
    /* 0130 */ uint32_t m_heldModifiers;
    /* 0134 */ uint32_t m_bindingModeMask;
    /* 0138 */ uint32_t m_bindingModeToggles;

    CGUIBindings();
    ~CGUIBindings();
    void AdjustCommandKeyIndices(BINDING_SET set, BINDING_MODE mode, const char* command, int32_t index);
    int32_t Bind(BINDING_SET set, BINDING_MODE mode, const char* key, const char* command);
    void ClearOverrideBindings(void* owner, bool checkPermission);
    void CopyBindings(BINDING_SET source, BINDING_SET dest);
    void EnableBindingMode(BINDING_MODE mode, int32_t enable);
    int32_t ExecCommand(const char* command, int32_t isDown, float pressure, int32_t allowPressure, int32_t isPressure, int32_t isAngle, float angle, float precision, const char* taint);
    int32_t ExecKey(uint32_t modifiers, const char* key, int32_t isDown, int32_t allowPressure, BINDING_MODE mode);
    void FinishLoadBindings();
    const char* GetBindingCommand(KEYBINDING* binding, BINDING_MODE mode);
    int32_t GetBindingIndex(KEYBINDING* binding, BINDING_MODE mode);
    void GetCommand(int32_t index, const char** command);
    const char* GetCommandAction(BINDING_MODE mode, const char* key, int32_t checkOverride);
    const char* GetCommandKey(BINDING_MODE mode, const char* command, int32_t index);
    KEYBINDING* GetKeyBinding(BINDING_MODE mode, const char* key);
    const char* GetModifiedClick(int32_t index);
    int32_t GetNumCommandKeys(BINDING_SET set, BINDING_MODE mode, const char* command);
    KEYBINDING* GetReducedKeyBinding(BINDING_MODE mode, const char* key, char* reducedKey, uint32_t maxLength);
    int32_t IsModifiedClick(const char* action);
    int32_t Load(const char* file, MD5_CTX* md5, CStatus* status);
    void LoadBinding(const char* file, XMLNode* node, CStatus* status);
    void LoadModifiedClick(const char* file, XMLNode* node, CStatus* status);
    void SaveBindings(BINDING_SET set, HOSFILE file);
    void SetOverrideBinding(void* owner, int32_t priority, const char* key, const char* command, bool checkPermission);
};

CSimpleFrame* GetClickFrame(lua_State* L, const char* name, uint32_t length);
MOUSEBUTTON IndexToMouseButton(uint32_t index);
int32_t MouseButtonToIndex(uint32_t button);
MOUSEBUTTON StringToMouseButton(const char* string);

void UIBindingsRegisterScriptFunctions();
void UIBindingsUnegisterScriptFunctions();

#endif
