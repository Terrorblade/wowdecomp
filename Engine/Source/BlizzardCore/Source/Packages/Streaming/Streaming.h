#ifndef ENGINE_SOURCE_BLIZZARDCORE_SOURCE_PACKAGES_STREAMING_STREAMING_H
#define ENGINE_SOURCE_BLIZZARDCORE_SOURCE_PACKAGES_STREAMING_STREAMING_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "Engine/Source/BlizzardCore/Include/Blizzard/Util/Digest.h"
#include "Engine/Source/BlizzardCore/Source/System/File/Types.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Streaming/Manifests.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Streaming/Storage.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Streaming/StreamingManager.h"
#include "Engine/Source/BlizzardCore/Source/System/Lock/Lock.h"
#include "Engine/Source/BlizzardCore/Source/System/Time/Time.h"
#include "Engine/Source/BlizzardCore/Source/System/URL/URL.h"

namespace Blizzard {
namespace Streaming {

struct Completion {
    /* 0000 */ Blizzard::Lock::Event* m_event;
    /* 0004 */ int32_t m_result;
    /* 0008 */ bool m_done;
    /* 0009 */ bool m_success;
};

#pragma pack(push, 4)
struct StorageBlock {
    /* 0000 */ int32_t m_state;
    /* 0004 */ int64_t m_offset;
    /* 000C */ int64_t m_storeOffset;
    /* 0014 */ int64_t m_partOffset;
    /* 001C */ int32_t m_size;
};
#pragma pack(pop)

struct StorageEntry {
    /* 0000 */ std::string m_path;
    /* 001C */ std::string m_version;
    /* 0038 */ bool m_closed;
    /* 0039 */ bool m_headerWritten;
    /* 003A */ bool m_parsed;
    /* 003C */ int32_t m_flags;
    /* 0040 */ int64_t m_size;
    /* 0048 */ int64_t m_storeSize;
    /* 0050 */ int32_t m_blockSize;
    /* 0054 */ std::vector<uint8_t> m_locality;
    /* 0064 */ std::vector<StorageBlock> m_blocks;
    /* 0078 */ int64_t m_headerOffset;
    /* 0080 */ Blizzard::File::StreamRecord* m_file;
    /* 0084 */ int32_t m_openCount;
    /* 0088 */ Blizzard::Lock::Mutex m_lock;
    /* 00A0 */ int32_t m_released;
    /* 00A4 */ int32_t m_refCount;
};

#pragma pack(push, 1)
struct StorageHeader {
    /* 0000 */ char m_path[260];
    /* 0104 */ char m_version[32];
    /* 0124 */ int32_t m_flags;
    /* 0128 */ uint8_t m_closed;
    /* 0129 */ int64_t m_size;
    /* 0131 */ int64_t m_storeSize;
    /* 0139 */ int32_t m_blockSize;
    /* 013D */ int32_t m_blockCount;
};

struct StorageBlockRecord {
    /* 0000 */ int32_t m_state;
    /* 0004 */ int64_t m_offset;
    /* 000C */ int64_t m_storeOffset;
    /* 0014 */ int32_t m_size;
};
#pragma pack(pop)

#pragma pack(push, 4)
struct PartFileHeader {
    /* 0000 */ int32_t m_magic;
    /* 0004 */ char m_version[32];
    /* 0024 */ int32_t m_flags;
    /* 0028 */ int64_t m_size;
    /* 0030 */ int32_t m_blockSize;
};

struct PartBlockRecord {
    /* 0000 */ int32_t m_state;
    /* 0004 */ int64_t m_storeOffset;
    /* 000C */ int64_t m_partOffset;
};
#pragma pack(pop)

struct LoadRange {
    /* 0000 */ int64_t m_offset;
    /* 0008 */ int32_t m_size;
    /* 0010 */ int64_t m_blockOffset;
    /* 0018 */ int32_t m_blockSize;
    /* 001C */ int32_t m_state;
};

struct WriteRequest {
    /* 0000 */ Blizzard::File::Filesystem* m_fs;
    /* 0004 */ int32_t m_pending;
    /* 0008 */ Blizzard::File::StreamRecord* m_file;
    /* 000C */ StorageEntry* m_entry;
    /* 0010 */ uint8_t* m_data;
    /* 0018 */ int64_t m_offset;
    /* 0020 */ int32_t m_size;
};

struct FILE_THREAD_CMD {
    /* 0000 */ Blizzard::File::Filesystem* m_fs;
    /* 0004 */ int32_t m_type;
    /* 0008 */ Blizzard::File::StreamRecord* m_file;
    /* 000C */ LoadRange* m_range;
    /* 0010 */ Completion* m_completion;
};

struct DigestState {
    /* 0000 */ int32_t m_state;
    /* 0004 */ int32_t m_priority;
};

struct ProgressSample {
    /* 0000 */ int64_t m_bytes;
    /* 0008 */ uint32_t m_time;
};

typedef void (*PROGRESSCALLBACK)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, int32_t);

class DownloadProgress {
    public:
    /* 0000 */ int64_t m_total;
    /* 0008 */ int64_t m_downloaded;
    /* 0010 */ int64_t m_streamed;
    /* 0018 */ uint32_t m_lastTime;
    /* 0020 */ ProgressSample m_samples[51];
    /* 0350 */ uint32_t m_index;
    /* 0354 */ PROGRESSCALLBACK m_callback;
    /* 0358 */ bool m_enabled;

    DownloadProgress();
    ~DownloadProgress();
    void FireCallback();
    int32_t GetRate();
    void ResetTime();
    void Sample(uint32_t time);
};

typedef std::map<Blizzard::Util::Digest<16>, DigestState> DigestStateMap;
typedef std::map<std::string, std::string> PathMap;
typedef std::map<std::string, StorageEntry> StorageEntryMap;

extern Blizzard::Time::Timestamp s_elapsedSinceStart;
extern int64_t s_statBytesRead;
extern int64_t s_statBytesDownloaded;
extern int64_t s_statBytesLocal;
extern int32_t s_statRetries;
extern char s_statFrameTimes[260];
extern bool s_streamingReady;
extern bool s_isStreamingTrial;

extern DownloadProgress s_progress;

void AddDownloaded(int32_t size);
void AddStreamed(int32_t size);
bool CancelQueue(int32_t priority);
uint32_t DownloadThread(void* param);
int32_t GetIdleThread();
int32_t GetNextCommand(FILE_THREAD_CMD** command, int32_t thread);
void HashFileName(Blizzard::Util::Digest<16>* digest, const char* name);
bool InitializeStreaming(const char* manifest, const char* storagePath, const char* variant, const char* dataPath, bool required);
bool LoadFile(Blizzard::File::Filesystem* fs, Blizzard::File::StreamRecord* file, LoadRange* range);
bool MakeLocalAsync(Blizzard::File::StreamRecord* file, int64_t offset, int32_t size, int32_t priority, const char* name);
bool MakeLocalBlocking(Blizzard::File::StreamRecord* file, int64_t offset, int32_t size, int32_t priority, const char* name);
bool PopCommand(FILE_THREAD_CMD* command, int32_t queue);
bool QueueLoad(Blizzard::File::StreamRecord* file, int32_t priority, LoadRange* range, Completion* completion);
bool RequestRanges(Blizzard::File::StreamRecord* file, LoadRange* ranges, int32_t count);
bool SelectQueue(FILE_THREAD_CMD* command, int32_t group, int32_t* queue);
void WaitForLoads();
bool QueueCompletion(Completion* completion, int32_t priority);
bool RemoveCompletion(Completion* completion, int32_t priority);
int32_t CheckFileRange(const char* path, int64_t offset, int32_t size, const char* name, int32_t flags);
int32_t GetIsLocalAmount(const char* path, int64_t offset, int32_t size, const char* name);
bool IsStreamedFile(const char* path);
std::string ResolveFilePath(const std::string& path);
bool IsStreamingMode();
void SetZone(int32_t areaId, int32_t subzoneId);
bool ScanDirectoryForManifest(const Blizzard::File::ProcessDirParms& parms);
void MakePartFileName(char* name, uint32_t size);

}
}

namespace Blizzard {
namespace Storage {

extern Blizzard::Lock::Mutex s_storageLock;
#if defined(WOWDECOMP_PLATFORM_LINUX)
extern Blizzard::Lock::Event s_storageEvent;
#else
extern void* s_storageEvent;
#endif

}
}

bool CheckIfStreamingAndTrial();
void StringToLower(char* str);
StreamingManager* GetStreamingManagerSingleton();
void SetClientInitialized(uint8_t initialized);
void ShutdownStreamingIfTrial();
void StopStreaming();
int32_t StreamingGetRace();
bool StreamingIsRaceDataReady(int32_t raceID);
void StreamingSetAreaOfInterest(int32_t chunkY, int32_t chunkX);
void StreamingSetMapId(int32_t mapId);
void StreamingSetRace(int32_t raceID);
void StreamingSetState(int32_t state);
char* GetComponentVersionData(uint32_t* size);

void SetActiveDataSource(bool restore);
void StartDataPreloadThread();
void StreamingSetEnabled(bool enabled);
float StreamingGetLoadFraction(int32_t raceID);

#endif
