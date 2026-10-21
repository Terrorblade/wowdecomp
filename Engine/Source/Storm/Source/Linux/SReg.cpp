#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include "Engine/Source/Storm/Source/Linux/StormLinux.h"
#include "Engine/Source/Storm/H/storm.h"

// The registry is emulated by one INI-style file, $XDG_CONFIG_HOME/wowdecomp/registry.ini (or
// ~/.config/wowdecomp/registry.ini). Sections are "<hive>\<full key name>" with hive HKCU or HKLM, entries are
// "<value name>=sz:<string>" or "<value name>=dword:<number>". Key and value names compare case-insensitively, as
// in the registry. The file is read on first use and rewritten after every save.

#define SREG_TYPE_SZ 1
#define SREG_TYPE_DWORD 4

#define SREG_HIVE_CURRENT_USER 0
#define SREG_HIVE_LOCAL_MACHINE 1

struct SREGENTRY {
    int32_t hive;
    std::string key;
    std::string name;
    uint32_t type;
    std::string value;
};

static pthread_mutex_t s_regLock = PTHREAD_MUTEX_INITIALIZER;
static std::vector<SREGENTRY>* s_entries;
static char s_regPath[MAX_PATH * 4];

static const char* const s_hiveNames[2] = { "HKCU", "HKLM" };

int32_t STORMAPI SRegGetBaseKey(uint32_t flags, char* buffer, uint32_t bufferchars) {
    if (flags & SREG_BATTLE_NET) {
        SStrCopy(buffer, "Software\\Battle.net\\", bufferchars);
    } else {
        SStrCopy(buffer, "Software\\Blizzard Entertainment\\", bufferchars);
    }

    return 1;
}

static void BuildFullKeyName(const char* keyname, uint32_t flags, char* buffer, uint32_t bufferchars) {
    *buffer = 0;

    if (!(flags & SREG_ABSOLUTE)) {
        SRegGetBaseKey(flags, buffer, bufferchars);
    }

    SStrPack(buffer, keyname, bufferchars);
}

static void BuildRegistryPath() {
    const char* base = getenv("XDG_CONFIG_HOME");
    std::string dir;

    if (base && *base) {
        dir = base;
    } else {
        const char* home = getenv("HOME");
        dir = std::string(home && *home ? home : ".") + "/.config";
    }

    dir += "/wowdecomp";
    SStrCopy(s_regPath, (dir + "/registry.ini").c_str(), sizeof(s_regPath));
}

static void CreateRegistryDirectory() {
    char dir[sizeof(s_regPath)];
    SStrCopy(dir, s_regPath, sizeof(dir));

    for (char* curr = dir + 1; *curr; curr++) {
        if (*curr == '/') {
            *curr = 0;
            mkdir(dir, 0755);
            *curr = '/';
        }
    }
}

static std::string Escape(const std::string& value) {
    std::string result;

    for (char c : value) {
        if (c == '\\') {
            result += "\\\\";
        } else if (c == '\n') {
            result += "\\n";
        } else if (c == '\r') {
            result += "\\r";
        } else {
            result += c;
        }
    }

    return result;
}

static std::string Unescape(const char* value) {
    std::string result;

    for (const char* c = value; *c; c++) {
        if (*c == '\\' && c[1]) {
            c++;
            result += *c == 'n' ? '\n' : *c == 'r' ? '\r' : *c;
        } else {
            result += *c;
        }
    }

    return result;
}

static void LoadRegistryLocked() {
    if (s_entries) {
        return;
    }

    s_entries = new std::vector<SREGENTRY>();
    BuildRegistryPath();

    FILE* file = fopen(s_regPath, "r");

    if (!file) {
        return;
    }

    int32_t hive = -1;
    std::string key;
    std::string line;
    char chunk[1024];

    while (fgets(chunk, sizeof(chunk), file)) {
        line += chunk;

        if (line.empty() || line.back() != '\n') {
            if (!feof(file)) {
                continue;
            }
        }

        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }

        if (line.empty() || line[0] == ';' || line[0] == '#') {
            line.clear();
            continue;
        }

        if (line[0] == '[' && line.back() == ']') {
            std::string section = line.substr(1, line.size() - 2);
            size_t slash = section.find('\\');
            hive = -1;

            if (slash != std::string::npos) {
                std::string hiveName = section.substr(0, slash);

                for (int32_t i = 0; i < 2; i++) {
                    if (!strcasecmp(hiveName.c_str(), s_hiveNames[i])) {
                        hive = i;
                    }
                }

                key = section.substr(slash + 1);
            }

            line.clear();
            continue;
        }

        size_t equals = line.find('=');

        if (hive >= 0 && equals != std::string::npos) {
            SREGENTRY entry;
            entry.hive = hive;
            entry.key = key;
            entry.name = Unescape(line.substr(0, equals).c_str());
            std::string data = line.substr(equals + 1);

            if (!strncasecmp(data.c_str(), "dword:", 6)) {
                entry.type = SREG_TYPE_DWORD;
                entry.value = data.substr(6);
                s_entries->push_back(entry);
            } else if (!strncasecmp(data.c_str(), "sz:", 3)) {
                entry.type = SREG_TYPE_SZ;
                entry.value = Unescape(data.c_str() + 3);
                s_entries->push_back(entry);
            }
        }

        line.clear();
    }

    fclose(file);
}

static int32_t SaveRegistryLocked() {
    CreateRegistryDirectory();

    std::string temp = std::string(s_regPath) + ".tmp";
    FILE* file = fopen(temp.c_str(), "w");

    if (!file) {
        return 0;
    }

    std::vector<bool> written(s_entries->size(), false);

    for (size_t i = 0; i < s_entries->size(); i++) {
        if (written[i]) {
            continue;
        }

        const SREGENTRY& first = (*s_entries)[i];
        fprintf(file, "[%s\\%s]\n", s_hiveNames[first.hive], first.key.c_str());

        for (size_t j = i; j < s_entries->size(); j++) {
            const SREGENTRY& entry = (*s_entries)[j];

            if (written[j] || entry.hive != first.hive || strcasecmp(entry.key.c_str(), first.key.c_str())) {
                continue;
            }

            if (entry.type == SREG_TYPE_DWORD) {
                fprintf(file, "%s=dword:%s\n", Escape(entry.name).c_str(), entry.value.c_str());
            } else {
                fprintf(file, "%s=sz:%s\n", Escape(entry.name).c_str(), Escape(entry.value).c_str());
            }

            written[j] = true;
        }

        fprintf(file, "\n");
    }

    int32_t result = fflush(file) == 0;
    result = (fclose(file) == 0) && result;

    if (!result || rename(temp.c_str(), s_regPath) != 0) {
        unlink(temp.c_str());
        return 0;
    }

    return 1;
}

static SREGENTRY* FindEntryLocked(int32_t hive, const char* key, const char* name) {
    for (SREGENTRY& entry : *s_entries) {
        if (entry.hive == hive && !strcasecmp(entry.key.c_str(), key) && !strcasecmp(entry.name.c_str(), name)) {
            return &entry;
        }
    }

    return nullptr;
}

// Same contract as RegQueryValueEx: the data must fit in bytes (strings including their terminator).
static int32_t ILoadValue(int32_t hive, const char* subkeyname, const char* valuename, uint32_t* datatype, void* buffer, uint32_t bytes, uint32_t* bytesread) {
    SREGENTRY* entry = FindEntryLocked(hive, subkeyname, valuename);

    if (!entry) {
        return 0;
    }

    *datatype = entry->type;

    if (entry->type == SREG_TYPE_DWORD) {
        if (bytes < sizeof(uint32_t)) {
            return 0;
        }

        uint32_t value = static_cast<uint32_t>(strtoul(entry->value.c_str(), nullptr, 0));
        memcpy(buffer, &value, sizeof(value));
        *bytesread = sizeof(value);
        return 1;
    }

    uint32_t length = static_cast<uint32_t>(entry->value.size()) + 1;

    if (length > bytes) {
        return 0;
    }

    memcpy(buffer, entry->value.c_str(), length);
    *bytesread = length;
    return 1;
}

static int32_t InternalLoadEntry(const char* keyname, const char* valuename, uint32_t flags, uint32_t* datatype, void* buffer, uint32_t bytes, uint32_t* bytesread) {
    *bytesread = 0;
    *datatype = SREG_TYPE_DWORD;

    char fullkeyname[MAX_PATH];
    BuildFullKeyName(keyname, flags, fullkeyname, MAX_PATH);

    pthread_mutex_lock(&s_regLock);
    LoadRegistryLocked();

    int32_t result = 0;

    if (!(flags & SREG_EXCLUDE_CURRENT_USER)) {
        result = ILoadValue(SREG_HIVE_CURRENT_USER, fullkeyname, valuename, datatype, buffer, bytes, bytesread);
    }

    if (!result && !(flags & SREG_EXCLUDE_LOCAL_MACHINE)) {
        result = ILoadValue(SREG_HIVE_LOCAL_MACHINE, fullkeyname, valuename, datatype, buffer, bytes, bytesread);
    }

    pthread_mutex_unlock(&s_regLock);

    return result;
}

static int32_t InternalSaveEntry(const char* keyname, const char* valuename, uint32_t flags, uint32_t datatype, const std::string& value) {
    char fullkeyname[MAX_PATH];
    BuildFullKeyName(keyname, flags, fullkeyname, MAX_PATH);
    int32_t hive = flags & SREG_EXCLUDE_CURRENT_USER ? SREG_HIVE_LOCAL_MACHINE : SREG_HIVE_CURRENT_USER;

    pthread_mutex_lock(&s_regLock);
    LoadRegistryLocked();

    SREGENTRY* entry = FindEntryLocked(hive, fullkeyname, valuename);

    if (entry) {
        entry->type = datatype;
        entry->value = value;
    } else {
        SREGENTRY newEntry;
        newEntry.hive = hive;
        newEntry.key = fullkeyname;
        newEntry.name = valuename;
        newEntry.type = datatype;
        newEntry.value = value;
        s_entries->push_back(newEntry);
    }

    int32_t result = SaveRegistryLocked();
    pthread_mutex_unlock(&s_regLock);

    return result;
}

int32_t STORMAPI SRegLoadString(const char* keyname, const char* valuename, uint32_t flags, char* buffer, uint32_t buffersize) {
    if (!keyname || !*keyname || !valuename || !*valuename || !buffer || !buffersize) {
        SErrSetLastError(STORM_WIN_ERROR_INVALID_PARAMETER);
        return 0;
    }

    uint32_t datatype;
    uint32_t bytesread;

    if (!InternalLoadEntry(keyname, valuename, flags, &datatype, buffer, buffersize, &bytesread)) {
        return 0;
    }

    switch (datatype) {
        case SREG_TYPE_SZ:
            buffer[buffersize - 1 < bytesread ? buffersize - 1 : bytesread] = 0;
            break;
        case SREG_TYPE_DWORD: {
            uint32_t value;
            memcpy(&value, buffer, sizeof(value));
            SStrPrintf(buffer, buffersize, "%u", value);
            break;
        }
    }

    return 1;
}

int32_t STORMAPI SRegLoadValue(const char* keyname, const char* valuename, uint32_t flags, uint32_t* value) {
    if (!keyname || !*keyname || !valuename || !*valuename || !value) {
        SErrSetLastError(STORM_WIN_ERROR_INVALID_PARAMETER);
        return 0;
    }

    char buffer[256];
    buffer[0] = 0;
    uint32_t datatype;
    uint32_t bytesread;

    if (!InternalLoadEntry(keyname, valuename, flags, &datatype, buffer, sizeof(buffer), &bytesread)) {
        return 0;
    }

    switch (datatype) {
        case SREG_TYPE_SZ:
            *value = static_cast<uint32_t>(strtoul(buffer, nullptr, 0));
            break;
        case SREG_TYPE_DWORD:
            memcpy(value, buffer, sizeof(*value));
            break;
    }

    return 1;
}

int32_t STORMAPI SRegSaveString(const char* keyname, const char* valuename, uint32_t flags, const char* string) {
    if (!keyname || !*keyname || !valuename || !*valuename || !string) {
        SErrSetLastError(STORM_WIN_ERROR_INVALID_PARAMETER);
        return 0;
    }

    return InternalSaveEntry(keyname, valuename, flags, SREG_TYPE_SZ, string);
}

int32_t STORMAPI SRegSaveValue(const char* keyname, const char* valuename, uint32_t flags, uint32_t value) {
    if (!keyname || !*keyname || !valuename || !*valuename) {
        SErrSetLastError(STORM_WIN_ERROR_INVALID_PARAMETER);
        return 0;
    }

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%u", value);

    return InternalSaveEntry(keyname, valuename, flags, SREG_TYPE_DWORD, buffer);
}

#endif
