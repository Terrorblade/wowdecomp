#ifndef WOW_SOURCE_CLIENT_H
#define WOW_SOURCE_CLIENT_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Event/Types.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Storm/Source/SThread.h"
#include "WoW/Source/DB/Types.h"
#include "WoW/Source/DB/WowClientDB.h"
#include "WoW/Source/DB/Rec/Startup_StringsRec.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/Net/Types.h"

struct CVar;
struct SArchive;
class CDataStore;

class CAsyncObject;
class CObjectHeapList;

enum TRANSFER_ABORT_REASON : int32_t {
    TRANSFER_ABORT_UNKNOWN_0 = 0,
    TRANSFER_ABORT_ERROR = 1,
    TRANSFER_ABORT_MAX_PLAYERS = 2,
    TRANSFER_ABORT_NOT_FOUND = 3,
    TRANSFER_ABORT_TOO_MANY_INSTANCES = 4,
    TRANSFER_ABORT_UNKNOWN_5 = 5,
    TRANSFER_ABORT_ZONE_IN_COMBAT = 6,
    TRANSFER_ABORT_INSUFFICIENT_EXPANSION = 7,
    TRANSFER_ABORT_DIFFICULTY = 8,
    TRANSFER_ABORT_UNIQUE_MESSAGE = 9,
    TRANSFER_ABORT_TOO_MANY_REALM_INSTANCES = 10,
    TRANSFER_ABORT_NEED_GROUP = 11,
    TRANSFER_ABORT_UNKNOWN_12 = 12,
    TRANSFER_ABORT_UNKNOWN_13 = 13,
    TRANSFER_ABORT_UNKNOWN_14 = 14,
    TRANSFER_ABORT_REALM_ONLY = 15,
    TRANSFER_ABORT_MAP_NOT_ALLOWED = 16,
};

enum WOWCMDOPT {
    CMD_RES_800x600 = 16,
    CMD_RES_1024x768 = 17,
    CMD_RES_1280x960 = 18,
    CMD_RES_1280x1024 = 19,
    CMD_RES_1600x1200 = 20,
    CMD_UP_TO_DATE = 21,
    CMD_16_BIT = 22,
    CMD_NO_FIX_LAG = 24,
    CMD_NO_SOUND = 26,
    CMD_SOUND_CHAOS = 27,
    CMD_DEPTH_16 = 29,
    CMD_DEPTH_24 = 30,
    CMD_DEPTH_32 = 31,
    CMD_WINDOWED = 32,
    CMD_CONSOLE = 35,
    CMD_HW_DETECT = 36,
    CMD_GX_OVERRIDE = 39,
};

enum ARCHIVE_TYPE : int32_t {
    ARCHIVE_REQUIRED  = 1,
    ARCHIVE_OPTIONAL  = 2,
    ARCHIVE_ALTERNATE = 3,
    ARCHIVE_STREAMING = 4,
};

enum ARCHIVE_INDEX : int32_t {
    ARCHIVE_INDEX_ALTERNATE = 0,
    ARCHIVE_INDEX_INTERFACE = 1,
    ARCHIVE_INDEX_MISC = 2,
    ARCHIVE_INDEX_MODEL = 3,
    ARCHIVE_INDEX_TEXTURE = 4,
    ARCHIVE_INDEX_TERRAIN = 5,
    ARCHIVE_INDEX_WMO = 6,
    ARCHIVE_INDEX_SOUND = 7,
    ARCHIVE_INDEX_FONTS = 8,
    ARCHIVE_INDEX_DBC = 9,
    ARCHIVE_INDEX_SPEECH = 10,
    ARCHIVE_INDEX_EXPANSION_LOC = 11,
    ARCHIVE_INDEX_LICHKING_LOC = 12,
    ARCHIVE_INDEX_EXPANSION_SPEECH = 13,
    ARCHIVE_INDEX_LICHKING_SPEECH = 14,
    ARCHIVE_INDEX_EXPANSION = 15,
    ARCHIVE_INDEX_LICHKING = 16,
    ARCHIVE_INDEX_COMMON = 17,
    ARCHIVE_INDEX_COMMON_2 = 18,
    ARCHIVE_INDEX_LOCALE = 19,
    ARCHIVE_INDEX_SPEECH_LOCALE = 20,
    ARCHIVE_INDEX_EXPANSION_LOCALE = 21,
    ARCHIVE_INDEX_LICHKING_LOCALE = 22,
    ARCHIVE_INDEX_EXPANSION_SPEECH_LOCALE = 23,
    ARCHIVE_INDEX_LICHKING_SPEECH_LOCALE = 24,
    ARCHIVE_INDEX_DEVELOPMENT = 25,
    ARCHIVE_INDEX_STREAMING = 26,
    ARCHIVE_INDEX_STREAMING_LOC = 27,
    NUM_ARCHIVES = 28,
};

enum EXPANSION_LEVEL : uint8_t {
    EXPANSION_CLASSIC = 0,
    EXPANSION_BURNING_CRUSADE = 1,
    EXPANSION_LICH_KING = 2,
    NUM_EXPANSIONS = 3,
};

enum WOW_LOCALE : int32_t {
    LOCALE_en_US = 0,
    LOCALE_ko_KR = 1,
    LOCALE_fr_FR = 2,
    LOCALE_de_DE = 3,
    LOCALE_zh_CN = 4,
    LOCALE_zh_TW = 5,
    LOCALE_es_ES = 6,
    LOCALE_es_MX = 7,
    LOCALE_ru_RU = 8,
    NUM_LOCALES = 9,
};

struct ARCHIVEINFO {
    /* 0000 */ const char* m_name;
    /* 0004 */ int32_t m_unk04;
    /* 0008 */ int32_t m_unk08;
    /* 000C */ bool m_opened;
};

extern CVar* g_accountListVar;
extern CVar* g_accountNameVar;
extern CVar* g_accountUsesTokenVar;
extern CVar* g_checkAddonVersionVar;
extern HEVENTCONTEXT g_clientEventContext;
extern bool g_hasIsoLocale[NUM_DB_LOCALES];
extern CVar* g_lastCharacterIndexVar;
extern CVar* g_readContestVar;
extern CVar* g_readEULAVar;
extern CVar* g_readScanningVar;
extern CVar* g_readTerminationWithoutNoticeVar;
extern CVar* g_readTOSVar;
extern WowClientDB<Startup_StringsRec> g_startup_StringsDB;
extern CVar* s_cvarUIFaster;
extern CVar* s_cvExpansionMovie;
extern CVar* s_cvMovie;
extern CVar* s_cvTextureCacheSize;
extern CVar* s_cvTextureFilteringMode;
extern CVar* s_desktopGammaCvar;
extern uint8_t s_expansionLevel;
extern CVar* s_gammaCvar;
extern int32_t s_locale;
extern const char* s_iniLocaleNames[NUM_LOCALES];
extern uint32_t s_localeIds[NUM_LOCALES];
extern char s_localeCode[5];
extern int32_t s_isPvPRealm;
extern int32_t s_region;

// HoT: exposes the open archive list to the map editor.
SArchive* const* ClientGetArchiveHandles(uint32_t& count);
void AsyncFileReadRegisterCVars();
void BeginCloseGame();
int32_t CCommand_Perf(const char* command, const char* arguments);
int32_t CCommand_ReloadUI(const char* command, const char* arguments);
int32_t CCommand_TimingInfo(const char* command, const char* arguments);
bool CheckDataFilesExist();
void ClientDestroyGame(int32_t charSelect, int32_t resumeGlue, int32_t worldLogin);
void ClientInitializeGame(uint32_t mapId, C3Vector position);
void ClientPostClose(int32_t reason);
void ClientReloadWorld(uint32_t zoneID, const char* directory, const C3Vector& position, float facing);
uint32_t ClientSetTimer(uint32_t delay, EVENTHANDLERFUNC handler, void* param);
uint32_t ClientSetTimer(uint32_t delay, EVENTHANDLERGUIDFUNC handler, uint64_t guid, void* param);
void ClientRegisterConsoleCommands();
int32_t CommonMain();
int32_t DestroyEngineCallback(const void* data, void* param);
uint8_t GetClientExpansionLevel();
const char* GetInstallLocationString();
uint32_t GetM2CvarFlags();
int32_t InitializeEngineCallback(const void* data, void* param);
bool InitializeGlobal();
bool LocaleCallback(CVar* var, const char* oldValue, const char* value, void* arg);
#if defined(_WIN32)
int main();
#endif
int32_t Packet_SMSG_NOTIFICATION(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_PLAYED_TIME(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_TRANSFER_ABORTED(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_TRANSFER_PENDING(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
bool ProcessAffinityMaskCallback(CVar* var, const char* oldValue, const char* value, void* arg);
int32_t STORMAPI SendErrorLog(uint32_t errorcode, const char* description, const char* filename, int32_t linenumber, const char* errorstring);
void SetPaths();
bool TextureCacheSizeCallback(CVar* var, const char* oldValue, const char* value, void* arg);
bool TimingMethodCallback(CVar* var, const char* oldValue, const char* value, void* arg);
bool UIFasterCallback(CVar* var, const char* oldValue, const char* value, void* arg);
int32_t STORMAPI WowLogHeader(char* buffer, uint32_t bufferchars);
void ZoneDebugDestroy();

struct ARCHIVELISTPARAMS {
    /* 0000 */ const char* m_prefix;
    /* 0004 */ uint32_t m_prefixLength;
    /* 0008 */ int32_t (*m_callback)(const char* fileName, void* param);
    /* 000C */ void* m_param;
};

struct AdditionalCheckInterface {
    /* 0000 */ void* (*m_memAlloc)(uint32_t bytes);
    /* 0004 */ void (*m_memFree)(void* ptr);
    /* 0008 */ void* (*m_loadLibrary)(const char* fileName);
    /* 000C */ void* (*m_getLibraryEntryPoint)(void* module, const char* name);
    /* 0010 */ void (*m_freeLibrary)(void* module);
};

extern uint8_t s_botCheckEnabled;
extern int32_t s_botCheckDelay;
extern volatile long s_additionalCheckArmed;

void CheckAndSendBotDetected();
void AdditionalCheck1();
uint8_t AdditionalCheck1Scan(const AdditionalCheckInterface* iface, int32_t unused_a2);
void InitTokenSeed(const uint8_t* key);
void* Client_MemAlloc(uint32_t bytes);
void Client_MemFree(void* ptr);
void SendBotDetected2(uint32_t msgId, uint32_t code);
int32_t GatherAdditionalChecks(uint8_t* check1, uint8_t* check2, uint8_t* check3);
void ClientUpdateTaxiBenchmarkMode();
void ClientKillTimer(uint32_t id, EVENTHANDLERFUNC handler, const char* name);
void ListInterfaceArchive(const char* prefix, int32_t (*callback)(const char* fileName, void* param), void* param);

#endif
