#ifndef ENGINE_SOURCE_CONSOLEBASE_CONSOLEVAR_H
#define ENGINE_SOURCE_CONSOLEBASE_CONSOLEVAR_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Base/RCString.h"
#include "Engine/Source/Os/OsFile.h"
#include "Engine/Source/Storm/H/stpl.h"

enum CVAR_FLAGS : uint32_t {
    CVAR_NONE = 0,
    CVAR_ARCHIVE = 1,
    CVAR_LATCH = 2,
    CVAR_READ_ONLY = 0x4,
    CVAR_PROTECTED = 0x8,
    CVAR_HIDDEN = 0x40,
    CVAR_UNKNOWN_80 = 0x80,
    CVAR_CONSOLE_READ_ONLY = 0x100,
    CVAR_CODE_REGISTERED = 0x80000000,
    CVAR_ARCHIVE_LATCH = 3,
    CVAR_LATCH_READ_ONLY = 6,
    CVAR_ACCOUNT_HIDDEN = 0x50,
    CVAR_ACCOUNT_CONSOLE_READ_ONLY = 0x110,
    CVAR_CHARACTER_CONSOLE_READ_ONLY = 0x120,
};

enum CVAR_ARCHIVE_SCOPE : uint32_t {
    CVAR_SCOPE_CONFIG = 0,
    CVAR_SCOPE_ACCOUNT = 0x10,
    CVAR_SCOPE_CHARACTER = 0x20,
    CVAR_SCOPE_MASK = 0x30,
};

struct CVar;

struct CVarLogBuffer {
    /* 0000 */ char* m_buffer;
    /* 0004 */ uint32_t m_size;
};

struct CVar : public TSHashObject<CVar, HASHKEY_STRI> {
    static const char* s_configFile;
    static bool m_needsSave;
    static bool m_initialized;
    static bool s_archiveCodeRegisteredOnly;

    static void ArchiveCodeRegisteredOnly();
    static int32_t Delete(const char* fileName);
    static void DeleteAccountCVars();
    static void DeleteCharacterCVars();
    static void Destroy();
    static void Initialize(const char* fileName);
    static int32_t IterateForArchive(uint32_t category, uint32_t excludeFlags, int32_t (*callback)(const char* name, const char* value, void* arg), void* arg);
    static int32_t Load(HOSFILE file);
    static int32_t Load(const char* fileName);
    static int32_t LogAll(char* buffer, uint32_t size, uint32_t category, uint32_t excludeFlags);
    static int32_t LogCvar(const char* name, const char* value, void* arg);
    static CVar* Lookup(const char* name);
    static CVar* LookupRegistered(const char* name);
    static int32_t IsLoginTarget(const char* name, size_t length);
    static int32_t CommandSetsLoginTarget(const char* command);
    static CVar* Register(const char* name, const char* help, uint32_t flags, const char* value, bool (*callback)(CVar*, const char*, const char*, void*), uint32_t category, bool setCommand, void* arg, bool unk);
    static int32_t SaveAll();
    static int32_t SaveAll(HOSFILE file, uint32_t category, uint32_t excludeFlags);
    static int32_t SaveCvar(const char* name, const char* value, void* arg);

    /* 0018 */ uint32_t m_category;
    /* 001C */ uint32_t m_flags;
    /* 0020 */ RCString m_stringValue;
    /* 002C */ float m_floatValue;
    /* 0030 */ int32_t m_intValue;
    /* 0034 */ int32_t m_modified;
    /* 0038 */ RCString m_defaultValue;
    /* 0044 */ RCString m_resetValue;
    /* 0050 */ RCString m_latchedValue;
    /* 005C */ RCString m_help;
    /* 0068 */ bool (*m_callback)(CVar*, const char*, const char*, void*);
    /* 006C */ void* m_arg;

    CVar();
    void Default();
    void InternalSet(const char* value, bool setValue, bool setReset, bool setDefault, bool setNeedsSave);
    void Reset();
    bool Set(const char* value, bool setValue, bool setReset, bool setDefault, bool setNeedsSave);
    bool Set(int32_t value, bool setValue, bool setReset, bool setDefault, bool setNeedsSave);
    void SetReadOnly(bool readOnly);
    bool Update();
};

class CVarArrayWrapper {
    public:
    /* 0000 */ CVar* m_cvar;
    /* 0004 */ uint8_t m_defaultValue;

    CVarArrayWrapper(CVar* cvar);
    bool RawGetValue(uint32_t index) const;
    bool RawSetValue(uint32_t index, bool value);
    void UpdateFromVersion(int32_t version);
};

class CVarIntListWrapper {
    public:
    static constexpr uint32_t CVAR_INT_LIST_MAX_VALUE = 93 * 93 * 93 - 1;
    static constexpr uint32_t CVAR_INT_LIST_MAX_STRING_SIZE = 256;

    enum CVAR_INT_LIST_OP : uint32_t {
        CVAR_INT_LIST_OP_NONE = 0,
        CVAR_INT_LIST_OP_GET = 1,
        CVAR_INT_LIST_OP_ADD = 2,
    };

    /* 0000 */ CVar* m_cvar;
    /* 0004 */ uint32_t m_pos;
    /* 0008 */ char m_buffer[CVAR_INT_LIST_MAX_STRING_SIZE];
    /* 0108 */ uint32_t m_state;

    void AddNextValue(uint32_t value);
    uint32_t GetNextValue();
    void UpdateFromVersion(int32_t version);
    void WriteVersionTag();
};

// INLINED
inline CVarArrayWrapper::CVarArrayWrapper(CVar* cvar) {
    this->m_cvar = cvar;
    this->m_defaultValue = 0;
    uint8_t version = static_cast<uint8_t>(cvar->m_stringValue.m_str[0]);

    if (version != 2) {
        this->UpdateFromVersion(version);
    }
}

#endif
