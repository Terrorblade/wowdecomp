
#include <cctype>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>
#include <windows.h>
#include "Engine/Source/BlizzardCore/Include/Blizzard/Util/MD5.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Streaming/Manifests.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Streaming/Storage.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Streaming/Streaming.h"
#include "Engine/Source/BlizzardCore/Source/System/Debug/ErrorStack.h"
#include "Engine/Source/BlizzardCore/Source/System/File/File.h"
#include "Engine/Source/BlizzardCore/Source/System/File/Filestack.h"
#include "Engine/Source/BlizzardCore/Source/System/File/Filestack_Streaming.h"
#include "Engine/Source/BlizzardCore/Source/System/Log/Log.h"
#include "Engine/Source/BlizzardCore/Source/System/Memory/MemoryStorm.h"
#include "Engine/Source/BlizzardCore/Source/System/String/Path.h"
#include "Engine/Source/BlizzardCore/Source/System/String/String.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/Thread.h"

// STATIC: 0x009E6204
static const int32_t s_queueOrder[6] = { 0, 1, 0, 2, 0, 1 };

// STATIC: 0x009E621C
static const int32_t s_queueGroupBase[2] = { 0, 4 };

// STATIC: 0x00AB9344
static bool s_useBackgroundQueues = true;

// STATIC: 0x00AB9346
static bool s_throttleProgress = true;

// STATIC: 0x00B3252C
static HANDLE s_throttleEvent;

// STATIC: 0x00B32530
static CRITICAL_SECTION s_queueLock;

// STATIC: 0x00B32548
static HANDLE s_threadEvents[2];

// STATIC: 0x00B32550
static HANDLE s_pauseEvent;

// STATIC: 0x00B32554
static Blizzard::Streaming::FILE_THREAD_CMD* s_currentCommands[2];

// STATIC: 0x00B3255C
static std::deque<Blizzard::Streaming::FILE_THREAD_CMD>* s_queues[7];

// STATIC: 0x00B325A0
static CRITICAL_SECTION s_pauseLock;

// STATIC: 0x00B325B8
static bool s_paused;

// STATIC: 0x00B325DC
static HANDLE s_queueEvent;

// STATIC: 0x00B38182
static volatile bool s_stopThreads;

// STATIC: 0x00B38184
static volatile int32_t s_threadMask;

// STATIC: 0x00B38188
static int32_t s_busyMask;

// STATIC: 0x00B38294
static Blizzard::Streaming::DigestStateMap* s_digestStates;

// STATIC: 0x00B38298
static Blizzard::Streaming::PathMap* s_paths;

// STATIC: 0x00B3829C
static int32_t s_commandsTaken;

// STATIC: 0x00B382A0
static int32_t s_commandsFreed;

// STATIC: 0x00B386D0
Blizzard::Streaming::DownloadProgress Blizzard::Streaming::s_progress;

// STATIC: 0x00B38A54
static int32_t s_queueRotation[2];

// OFFSET: 0x00428030
int32_t Blizzard::Streaming::GetIdleThread() {
    for (int32_t thread = 0; thread < 2; thread++) {
        if (((1 << thread) & s_threadMask) && !((1 << thread) & s_busyMask)) {
            return thread;
        }
    }

    return -1;
}

// OFFSET: 0x00428060
void Blizzard::Streaming::DownloadProgress::Sample(uint32_t time) {
    if (++this->m_index >= 51) {
        this->m_index = 0;
    }

    this->m_samples[this->m_index].m_time = 0;
    this->m_samples[this->m_index].m_bytes = this->m_streamed;
    this->m_samples[this->m_index].m_time = time;
}

// OFFSET: 0x004280C0
int32_t Blizzard::Streaming::DownloadProgress::GetRate() {
    uint32_t last = this->m_index;
    uint32_t first;

    if (last == 50) {
        first = 0;
    } else {
        first = last + 1;

        if (!last) {
            last = 51;
        }
    }

    uint32_t end = last - 1;

    while (!this->m_samples[first].m_time) {
        if (first == end) {
            break;
        }

        if (++first > 50) {
            first = 0;
        }
    }

    for (; !this->m_samples[end].m_time; end--) {
        if (first == end) {
            break;
        }

        if (!end) {
            end = 51;
        }
    }

    int64_t bytes = this->m_samples[end].m_bytes - this->m_samples[first].m_bytes;
    int32_t time = this->m_samples[end].m_time - this->m_samples[first].m_time;

    if (first == end) {
        uint32_t next = end + 1;

        if (next == 51) {
            next = 0;
        }

        if (this->m_samples[next].m_time) {
            bytes = this->m_samples[next].m_bytes;
            time = this->m_samples[next].m_time - this->m_lastTime;
        }
    }

    if (bytes > 0) {
        return static_cast<int32_t>(1000 * (bytes / time));
    }

    return 0;
}

// OFFSET: 0x0042E020
void Blizzard::Streaming::HashFileName(Blizzard::Util::Digest<16>* digest, const char* name) {
    char lower[260];
    memset(lower, 0, sizeof(lower));

    if (name) {
        char* dst = lower;

        do {
            char c = *name++;

            if (!c) {
                break;
            }

            *dst++ = c;
        } while (dst < &lower[259]);

        *dst = '\0';
    } else {
        lower[0] = '\0';
    }

    for (char* cursor = lower; *cursor; cursor++) {
        *cursor = tolower(*cursor);
    }

    Blizzard::Util::HashString(digest->m_data, lower);
}

// OFFSET: 0x0042E0B0
void Blizzard::Streaming::WaitForLoads() {
    for (;;) {
        EnterCriticalSection(&s_queueLock);

        bool busy = false;

        for (int32_t i = 0; i < 2; i++) {
            if (s_currentCommands[i] && s_currentCommands[i]->m_type == 1) {
                busy = true;
                break;
            }
        }

        LeaveCriticalSection(&s_queueLock);

        if (!busy) {
            return;
        }

        Sleep(1);
    }
}

// OFFSET: 0x00437ED0
bool Blizzard::Streaming::CancelQueue(int32_t priority) {
    EnterCriticalSection(&s_queueLock);

    std::deque<FILE_THREAD_CMD>& queue = *s_queues[priority];

    for (std::deque<FILE_THREAD_CMD>::iterator it = queue.begin(); it != queue.end(); ++it) {
        if (it->m_completion && it->m_type == 2) {
            it->m_completion->m_result = static_cast<int32_t>(s_queues[priority]->size());
            it->m_completion->m_done = true;
            SetEvent(*it->m_completion->m_event);
            it->m_completion = nullptr;
        }

        it->m_type = 0;
    }

    for (DigestStateMap::iterator it = s_digestStates->begin(); it != s_digestStates->end(); ++it) {
        if (it->second.m_state && it->second.m_priority == priority) {
            it->second.m_state = 0;
        }
    }

    LeaveCriticalSection(&s_queueLock);
    return true;
}

// OFFSET: 0x00438120
bool Blizzard::Streaming::PopCommand(FILE_THREAD_CMD* command, int32_t queue) {
    if (!s_queues[queue]->size()) {
        return false;
    }

    *command = s_queues[queue]->front();
    s_queues[queue]->pop_front();
    s_commandsTaken++;

    return true;
}

// OFFSET: 0x004381E0
bool Blizzard::Streaming::SelectQueue(FILE_THREAD_CMD* command, int32_t group, int32_t* queue) {
    if (group >= 1 && !s_useBackgroundQueues) {
        return false;
    }

    int32_t base = s_queueGroupBase[group];

    for (int32_t i = 0; i < 6; i++) {
        *queue = base + s_queueOrder[s_queueRotation[group]];
        s_queueRotation[group] = (s_queueRotation[group] + 1) % 6;

        if (PopCommand(command, *queue)) {
            return true;
        }
    }

    return false;
}

// OFFSET: 0x00438260
int32_t Blizzard::Streaming::GetNextCommand(FILE_THREAD_CMD** command, int32_t thread) {
    FILE_THREAD_CMD* next = new FILE_THREAD_CMD();
    int32_t queue = 7;

    if (SelectQueue(next, 0, &queue) || (queue = 3, PopCommand(next, 3)) || SelectQueue(next, 1, &queue)) {
        s_currentCommands[thread] = next;
        *command = next;
        return queue;
    }

    delete next;
    s_commandsFreed++;
    return 7;
}

// OFFSET: 0x00438360
void Blizzard::Streaming::DownloadProgress::FireCallback() {
    if (!this->m_enabled) {
        return;
    }

    if (s_throttleProgress && Blizzard::Time::Milliseconds() - this->m_lastTime < 5000) {
        return;
    }

    PROGRESSCALLBACK callback = this->m_callback;

    if (callback) {
        callback(static_cast<uint32_t>(this->m_total), static_cast<uint32_t>(this->m_total >> 32), static_cast<uint32_t>(this->m_downloaded), static_cast<uint32_t>(this->m_downloaded >> 32), static_cast<uint32_t>(this->m_streamed), static_cast<uint32_t>(this->m_streamed >> 32), this->GetRate());
    }
}

// OFFSET: 0x00439E30
void Blizzard::Streaming::AddDownloaded(int32_t size) {
    s_progress.m_downloaded += size;
    s_progress.FireCallback();
}

// OFFSET: 0x00439E50
void Blizzard::Streaming::AddStreamed(int32_t size) {
    s_progress.m_streamed += size;

    uint32_t now = static_cast<uint32_t>(Blizzard::Time::Milliseconds());

    if (now - s_progress.m_samples[s_progress.m_index].m_time > 100) {
        s_progress.Sample(now);
        s_progress.FireCallback();
    }
}

// OFFSET: 0x0043AF30
bool Blizzard::Streaming::RemoveCompletion(Completion* completion, int32_t priority) {
    if (!s_queues[priority]->size()) {
        return false;
    }

    EnterCriticalSection(&s_queueLock);

    std::deque<FILE_THREAD_CMD>& queue = *s_queues[priority];

    for (std::deque<FILE_THREAD_CMD>::iterator it = queue.end(); it != queue.begin();) {
        --it;

        if (it->m_completion == completion) {
            it->m_completion = nullptr;
            LeaveCriticalSection(&s_queueLock);
            return true;
        }
    }

    LeaveCriticalSection(&s_queueLock);
    return false;
}

// OFFSET: 0x00444550
bool Blizzard::Streaming::QueueLoad(Blizzard::File::StreamRecord* file, int32_t priority, LoadRange* range, Completion* completion) {
    Blizzard::File::Filesystem* fs = System_File::Stacked::s_manager;

    while (fs && fs->m_original != &System_File::Stacked::s_streamstack) {
        fs = fs->m_next;
    }

    FILE_THREAD_CMD command;
    command.m_fs = fs;
    command.m_type = 1;
    command.m_file = file;
    command.m_range = static_cast<LoadRange*>(Blizzard::Memory::Allocate(sizeof(LoadRange)));

    if (range) {
        memmove(command.m_range, range, sizeof(LoadRange));
    }

    command.m_completion = completion;

    EnterCriticalSection(&s_queueLock);
    s_queues[priority]->push_back(command);
    LeaveCriticalSection(&s_queueLock);

    return Blizzard::Storage::RequestRange(file->m_streaming->m_entry, range->m_blockOffset, range->m_blockSize, 1);
}

// OFFSET: 0x00444620
bool Blizzard::Streaming::RequestRanges(Blizzard::File::StreamRecord* file, LoadRange* ranges, int32_t count) {
    int32_t state = 0;
    int32_t requests = 0;
    int32_t blocks = 0;
    std::vector<Completion*> completions;

    LoadRange current;
    current.m_offset = 0;
    current.m_size = 0;
    current.m_state = 0;

    for (int32_t i = 0; i < count; i++) {
        LoadRange& range = ranges[i];

        if (range.m_state > 4) {
            continue;
        }

        if (!state) {
            if (range.m_state < 2) {
                continue;
            }

            current.m_blockOffset = range.m_blockOffset;
            current.m_blockSize = range.m_blockSize;
            current.m_state = 2;
            blocks++;
            state = 2;
            continue;
        }

        if (current.m_blockOffset + current.m_blockSize != range.m_blockOffset) {
            Completion* completion = new Completion();
            Blizzard::Lock::Event* event = new Blizzard::Lock::Event();
            completion->m_event = event;
            *event = CreateEventA(nullptr, FALSE, FALSE, nullptr);
            completions.push_back(completion);

            QueueLoad(file, 0, &current, completion);
            SetEvent(s_throttleEvent);
            SetEvent(s_queueEvent);
            requests++;

            if (range.m_state < 2) {
                state = 0;
            } else {
                current.m_blockOffset = range.m_blockOffset;
                current.m_blockSize = range.m_blockSize;
                state = range.m_state;
            }

            current.m_state = state;
        } else {
            current.m_blockSize += range.m_blockSize;
            blocks++;
        }
    }

    if (state) {
        if (requests || blocks < 4) {
            Completion* completion = new Completion();
            Blizzard::Lock::Event* event = new Blizzard::Lock::Event();
            completion->m_event = event;
            *event = CreateEventA(nullptr, FALSE, FALSE, nullptr);
            completions.push_back(completion);
            QueueLoad(file, 0, &current, completion);
        } else {
            int32_t half = Blizzard::Storage::s_blockSize * (blocks - blocks / 2);

            LoadRange second;
            second.m_offset = 0;
            second.m_size = 0;
            second.m_state = 2;
            second.m_blockOffset = current.m_blockOffset + half;
            second.m_blockSize = current.m_blockSize - half;
            current.m_blockSize = half;

            Completion* completion = new Completion();
            Blizzard::Lock::Event* event = new Blizzard::Lock::Event();
            completion->m_event = event;
            *event = CreateEventA(nullptr, FALSE, FALSE, nullptr);
            completions.push_back(completion);
            QueueLoad(file, 0, &current, completion);

            completion = new Completion();
            event = new Blizzard::Lock::Event();
            completion->m_event = event;
            *event = CreateEventA(nullptr, FALSE, FALSE, nullptr);
            completions.push_back(completion);
            QueueLoad(file, 0, &second, completion);
        }
    }

    SetEvent(s_throttleEvent);
    SetEvent(s_queueEvent);

    bool success = true;

    for (std::vector<Completion*>::iterator it = completions.begin(); it != completions.end(); ++it) {
        WaitForSingleObject(*(*it)->m_event, INFINITE);
        CloseHandle(*(*it)->m_event);

        if (!(*it)->m_success) {
            success = false;
        }

        delete (*it)->m_event;
        (*it)->m_event = nullptr;
        delete *it;
        *it = nullptr;
    }

    completions.erase(completions.begin(), completions.end());

    return success;
}

// OFFSET: 0x00444AA0
bool Blizzard::Streaming::QueueCompletion(Completion* completion, int32_t priority) {
    if (!s_queues[priority]->size()) {
        completion->m_result = 0;
        completion->m_done = false;
        completion->m_success = false;
        return false;
    }

    FILE_THREAD_CMD command;
    command.m_fs = nullptr;
    command.m_type = 2;
    command.m_file = nullptr;
    command.m_range = nullptr;
    command.m_completion = completion;

    EnterCriticalSection(&s_queueLock);
    s_queues[priority]->push_back(command);
    LeaveCriticalSection(&s_queueLock);
    SetEvent(s_queueEvent);

    return true;
}

// OFFSET: 0x004480B0
bool Blizzard::Streaming::MakeLocalAsync(Blizzard::File::StreamRecord* file, int64_t offset, int32_t size, int32_t priority, const char* name) {
    if (!file->m_streaming || !file->m_streaming->m_entry) {
        return false;
    }

    Blizzard::Util::Digest<16> digest;
    HashFileName(&digest, name);

    DigestStateMap::iterator it = s_digestStates->find(digest);

    if (it != s_digestStates->end()) {
        if (it->second.m_state == 1) {
            return true;
        }

        if (it->second.m_state == 2 && it->second.m_priority <= priority) {
            return true;
        }
    }

    StorageEntry* entry = file->m_streaming->m_entry;

    if (!entry) {
        return false;
    }

    int32_t count;
    LoadRange* ranges = Blizzard::Storage::GetRange(entry, offset, size, &count, true);

    if (!ranges || !count) {
        return false;
    }

    bool queued = false;

    for (int32_t i = 0; i < count; i++) {
        if (static_cast<uint32_t>(ranges[i].m_state - 2) <= 2) {
            QueueLoad(file, priority, &ranges[i], nullptr);
            queued = true;
        }
    }

    if (queued) {
        (*s_digestStates)[digest].m_state = 2;
        (*s_digestStates)[digest].m_priority = priority;

        if (s_useBackgroundQueues) {
            SetEvent(s_queueEvent);
        }
    } else {
        (*s_digestStates)[digest].m_state = 1;
        AddDownloaded(size);
    }

    Blizzard::Memory::Free(ranges);
    return true;
}

// OFFSET: 0x0044F120
int32_t Blizzard::Streaming::CheckFileRange(const char* path, int64_t offset, int32_t size, const char* name, int32_t flags) {
    Blizzard::Util::Digest<16> digest;
    HashFileName(&digest, name);

    DigestStateMap::iterator it = s_digestStates->find(digest);
    int32_t previous = 0;

    if (it != s_digestStates->end()) {
        previous = it->second.m_state;

        if (previous == 1) {
            return 1;
        }
    }

    if (!path) {
        return -1;
    }

    std::string key = ResolveFilePath(std::string(path));

    if (!Blizzard::Storage::s_storage) {
        return -1;
    }

    std::map<std::string, StorageEntry>::iterator entry = Blizzard::Storage::s_storage->find(key);

    if (entry == Blizzard::Storage::s_storage->end()) {
        return -1;
    }

    int32_t count;
    LoadRange* ranges = Blizzard::Storage::GetRange(&entry->second, offset, size, &count, false);

    if (!ranges || !count) {
        return -1;
    }

    int32_t result = 1;

    for (LoadRange* range = ranges; count > 0 && result; range++, count--) {
        switch (range->m_state) {
            case 1:
            case 3:
                if (result == 1) {
                    result = 2;
                }

                break;

            case 2:
            case 4:
                result = 0;
                break;
        }
    }

    Blizzard::Memory::Free(ranges);

    if (previous != result) {
        (*s_digestStates)[digest].m_state = result;
    }

    return result;
}

// OFFSET: 0x0044F370
int32_t Blizzard::Streaming::GetIsLocalAmount(const char* path, int64_t offset, int32_t size, const char* name) {
    Blizzard::Util::Digest<16> digest;
    HashFileName(&digest, name);

    DigestStateMap::iterator it = s_digestStates->find(digest);

    if (it != s_digestStates->end() && it->second.m_state == 1) {
        return size;
    }

    std::string key = ResolveFilePath(std::string(path));

    if (!Blizzard::Storage::s_storage) {
        return size;
    }

    std::map<std::string, StorageEntry>::iterator entry = Blizzard::Storage::s_storage->find(key);

    if (entry == Blizzard::Storage::s_storage->end()) {
        return size;
    }

    int32_t local = size;
    int32_t count;
    LoadRange* ranges = Blizzard::Storage::GetRange(&entry->second, offset, size, &count, false);

    if (ranges && count) {
        local = 0;

        for (int32_t i = 0; i < count; i++) {
            if (!ranges[i].m_state) {
                local += ranges[i].m_size;
            }
        }

        Blizzard::Memory::Free(ranges);
    }

    return local;
}

// OFFSET: 0x0044F560
bool Blizzard::Streaming::MakeLocalBlocking(Blizzard::File::StreamRecord* file, int64_t offset, int32_t size, int32_t priority, const char* name) {
    Blizzard::Util::Digest<16> digest;
    HashFileName(&digest, name);

    DigestStateMap::iterator it = s_digestStates->find(digest);

    if (it != s_digestStates->end() && it->second.m_state == 1) {
        return true;
    }

    std::string key = ResolveFilePath(std::string(file->m_pathname));

    if (!Blizzard::Storage::s_storage) {
        return false;
    }

    std::map<std::string, StorageEntry>::iterator entry = Blizzard::Storage::s_storage->find(key);

    if (entry == Blizzard::Storage::s_storage->end()) {
        return false;
    }

    int32_t count;
    LoadRange* ranges = Blizzard::Storage::GetRange(&entry->second, offset, size, &count, true);

    if (!ranges || !count) {
        return false;
    }

    std::vector<Completion*> completions;
    bool queued = false;

    for (int32_t i = 0; i < count; i++) {
        LoadRange& range = ranges[i];

        if (static_cast<uint32_t>(range.m_state - 2) > 2) {
            continue;
        }

        if (range.m_blockSize <= 0) {
            Blizzard::Memory::Free(ranges);
            return false;
        }

        LoadRange request;
        request.m_state = 2;
        request.m_blockOffset = range.m_blockOffset;
        request.m_blockSize = range.m_blockSize;
        queued = true;

        Completion* completion = new Completion();
        Blizzard::Lock::Event* event = new Blizzard::Lock::Event();
        completion->m_event = event;
        *event = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        completions.push_back(completion);

        QueueLoad(file, priority, &request, completion);
        SetEvent(s_throttleEvent);
        SetEvent(s_queueEvent);
    }

    if (!queued) {
        Blizzard::Memory::Free(ranges);
        return true;
    }

    SetEvent(s_throttleEvent);
    SetEvent(s_queueEvent);

    bool success = true;

    for (std::vector<Completion*>::iterator it = completions.begin(); it != completions.end(); ++it) {
        WaitForSingleObject(*(*it)->m_event, INFINITE);
        CloseHandle(*(*it)->m_event);

        if (!(*it)->m_success) {
            success = false;
        }

        delete (*it)->m_event;
        (*it)->m_event = nullptr;
        delete *it;
        *it = nullptr;
    }

    completions.erase(completions.begin(), completions.end());
    Blizzard::Memory::Free(ranges);

    return success;
}

// OFFSET: 0x00458A90
bool System_File::Stacked::stream_flush(Blizzard::File::Filesystem* fs, FileParms* parms) {
    Blizzard::File::StreamRecord* file = parms->m_stream;

    if (file->m_streaming && file->m_streaming->m_entry) {
        EnterCriticalSection(&s_queueLock);

        for (int32_t i = 0; i < 7; i++) {
            std::deque<Blizzard::Streaming::FILE_THREAD_CMD>* queue = s_queues[i];

            if (queue->size()) {
                for (std::deque<Blizzard::Streaming::FILE_THREAD_CMD>::iterator it = queue->begin(); it != queue->end(); ++it) {
                    if (file == it->m_file && it->m_type == 1) {
                        it->m_type = 0;
                    }
                }
            }
        }

        for (int32_t i = 0; i < 2; i++) {
            if (s_currentCommands[i] && file == s_currentCommands[i]->m_file && s_currentCommands[i]->m_type == 1) {
                s_currentCommands[i]->m_type = 0;
            }
        }

        LeaveCriticalSection(&s_queueLock);

        if (!Blizzard::Storage::FlushFile(fs->m_next, file)) {
            return false;
        }
    }

    if (!fs->m_next) {
        return false;
    }

    return (*reinterpret_cast<FileFunc*>(reinterpret_cast<uintptr_t>(fs->m_next) + parms->m_verb))(fs->m_next, parms);
}

// OFFSET: 0x0045A8F0
bool Blizzard::Streaming::LoadFile(Blizzard::File::Filesystem* fs, Blizzard::File::StreamRecord* file, LoadRange* range) {
    Blizzard::Storage::StorageEntryRef ref(file->m_streaming->m_entry);
    StorageEntry* entry = ref.m_entry;

    if (!entry) {
        EnterCriticalSection(&Blizzard::Storage::s_storageLock);
        LeaveCriticalSection(&Blizzard::Storage::s_storageLock);
        return false;
    }

    int32_t count = 0;
    LoadRange* ranges = file->m_streaming ? Blizzard::Storage::GetRange(entry, range->m_blockOffset, range->m_blockSize, &count, true) : nullptr;

    if (!ranges || !count) {
        EnterCriticalSection(&Blizzard::Storage::s_storageLock);
        entry->m_refCount--;
        LeaveCriticalSection(&Blizzard::Storage::s_storageLock);
        return false;
    }

    bool requested = false;
    int32_t failState = 3;

    EnterCriticalSection(&Blizzard::Storage::s_storageLock);

    for (int32_t i = 0; i < count; i++) {
        LoadRange& loadRange = ranges[i];

        if (loadRange.m_state == 3) {
            failState = 1;
            Blizzard::Storage::RequestRange(entry, loadRange.m_blockOffset, loadRange.m_blockSize, 4);
            requested = true;
        } else if (loadRange.m_state == 2 || loadRange.m_state == 4) {
            failState = 0;
            Blizzard::Storage::RequestRange(entry, loadRange.m_blockOffset, loadRange.m_blockSize, 4);
            requested = true;
        }
    }

    LeaveCriticalSection(&Blizzard::Storage::s_storageLock);
    Blizzard::Memory::Free(ranges);

    if (!requested) {
        Blizzard::Log::Printf(3, "Logs/Profiling_File_Defer_Cancelled.log", "%10lld, %10d - %s\n", range->m_blockOffset, range->m_blockSize, file->m_pathname);
        EnterCriticalSection(&Blizzard::Storage::s_storageLock);
        entry->m_refCount--;
        LeaveCriticalSection(&Blizzard::Storage::s_storageLock);
        return true;
    }

    Blizzard::Log::Printf(3, "Logs/Profiling_File_Defer.log", "%10lld, %10d - %s\n", range->m_blockOffset, range->m_blockSize, file->m_pathname);
    s_statBytesDownloaded += range->m_blockSize;

    uint8_t* data = static_cast<uint8_t*>(Blizzard::Memory::Allocate(range->m_blockSize));

    std::string url;
    url = ManifestInfo::GetRemotePath(file->m_streaming->m_path);

    Manifests::TransportItem* transport = ManifestInfo::GetTransportItem(file->m_streaming->m_path);
    int32_t maxRetry = ManifestInfo::GetServerItemMaxRetry(std::string(url), file->m_streaming->m_path);

    bool result = Transport::Read(transport, std::string(url), maxRetry, data, file->m_info.m_size, range->m_blockOffset, range->m_blockSize);

    if (!result) {
        ManifestInfo::DeactivateServer(std::string(url), file->m_streaming->m_path);
        Blizzard::Memory::Free(data);
    } else if (range->m_blockSize > Blizzard::Storage::s_blockSize) {
        int32_t written = 0;
        int32_t left = range->m_blockSize;

        while (left > 0) {
            int32_t chunk = Blizzard::Storage::s_blockSize < left ? Blizzard::Storage::s_blockSize : left;
            result &= Blizzard::Storage::QueueWrite(fs, file, &data[written], range->m_blockOffset + written, chunk, false);
            written += chunk;
            left -= chunk;
        }

        Blizzard::Memory::Free(data);
    } else {
        result = Blizzard::Storage::QueueWrite(fs, file, data, range->m_blockOffset, range->m_blockSize, true);
    }

    if (!result) {
        Blizzard::Storage::SetRangeAs(entry, range->m_blockOffset, range->m_blockSize, failState);
    }

    EnterCriticalSection(&Blizzard::Storage::s_storageLock);
    entry->m_refCount--;
    LeaveCriticalSection(&Blizzard::Storage::s_storageLock);

    return result;
}

// OFFSET: 0x0045B150
uint32_t Blizzard::Streaming::DownloadThread(void* param) {
    int32_t thread = *static_cast<int32_t*>(param);
    delete static_cast<int32_t*>(param);

    EnterCriticalSection(&s_queueLock);
    int32_t bit = 1 << thread;
    s_threadMask |= bit;
    LeaveCriticalSection(&s_queueLock);

    if (!s_stopThreads) {
        for (;;) {
            if (Blizzard::Lock::EventWait(100, s_queueEvent) || Blizzard::Lock::EventWait(5, s_threadEvents[thread])) {
                while (!s_stopThreads) {
                    if (s_paused) {
                        WaitForSingleObject(s_pauseEvent, INFINITE);
                        SetEvent(s_pauseEvent);
                    }

                    EnterCriticalSection(&s_queueLock);
                    s_busyMask |= bit;

                    FILE_THREAD_CMD* command = nullptr;
                    int32_t queue = GetNextCommand(&command, thread);

                    if (queue == 7) {
                        LeaveCriticalSection(&s_queueLock);
                        break;
                    }

                    int32_t pending = 0;

                    for (int32_t i = 0; i < 7; i++) {
                        pending += static_cast<int32_t>(s_queues[i]->size());
                    }

                    if (pending > 0) {
                        int32_t idle = GetIdleThread();

                        if (idle >= 0) {
                            SetEvent(s_threadEvents[idle]);
                        }
                    }

                    LeaveCriticalSection(&s_queueLock);

                    bool success = true;
                    bool requeue = false;

                    if (!command->m_type) {
                        success = false;
                    } else if (command->m_type == 1) {
                        if (LoadFile(command->m_fs, command->m_file, command->m_range)) {
                            command->m_type = 0;
                        } else if (command->m_type && !command->m_completion) {
                            requeue = true;
                            success = false;
                        }
                    } else if (command->m_type == 2) {
                        command->m_type = 0;

                        if (!pending) {
                            WaitForLoads();
                        }
                    }

                    if (command->m_completion) {
                        command->m_completion->m_result = static_cast<int32_t>(s_queues[queue]->size());
                        command->m_completion->m_done = false;
                        command->m_completion->m_success = success;
                        SetEvent(*command->m_completion->m_event);
                        command->m_completion = nullptr;
                    }

                    EnterCriticalSection(&s_queueLock);

                    if (requeue) {
                        s_queues[queue]->push_front(*command);
                        AddStreamed(0);
                    }

                    FILE_THREAD_CMD* current = s_currentCommands[thread];

                    if (current) {
                        s_currentCommands[thread] = nullptr;
                        Blizzard::Memory::Free(current->m_range);
                        delete current;
                        s_commandsFreed++;
                    }

                    LeaveCriticalSection(&s_queueLock);

                    System_File::FileError& error = System_File::s_error;

                    if (error.m_errorStack) {
                        Blizzard::Debug::FreeErrorStack(error.m_errorStack);
                    }

                    error.m_errorStack = nullptr;

                    if (queue > 2) {
                        WaitForSingleObject(s_throttleEvent, FileManifest::GetBGPreloadSleep());
                    }
                }
            }

            EnterCriticalSection(&s_queueLock);
            s_busyMask &= ~bit;
            LeaveCriticalSection(&s_queueLock);

            if (s_stopThreads) {
                break;
            }
        }
    }

    EnterCriticalSection(&s_queueLock);
    s_threadMask &= ~bit;
    LeaveCriticalSection(&s_queueLock);

    return 1;
}

// OFFSET: 0x004616A0
bool Blizzard::Streaming::InitializeStreaming(const char* manifest, const char* storagePath, const char* variant, const char* dataPath, bool required) {
    if (s_streamingReady) {
        return true;
    }

    char cwd[260];
    cwd[0] = '\0';

    if (!Blizzard::File::GetWorkingDirectory(cwd, sizeof(cwd))) {
        Blizzard::File::AddToLastErrorStack(0, "InitializeStreaming - Failed to get working directory", 20);
        return false;
    }

    char root[260];

    if (!manifest || Blizzard::String::EqualI(manifest, s_httpPrefix, s_httpPrefix ? strlen(s_httpPrefix) : 0)) {
        Blizzard::String::Copy(root, cwd, sizeof(root));
    } else {
        char absolute[260];

        if (!Blizzard::File::MakeAbsolutePath(manifest, absolute, sizeof(absolute), false)) {
            Blizzard::File::AddToLastErrorStack(0, "InitializeStreaming - Failed to make absolute manifest path", 20);
            return false;
        }

        int32_t length = static_cast<int32_t>(Blizzard::String::FindFilename(absolute) - absolute + 1);
        Blizzard::String::Copy(root, absolute, length <= 260 ? length : 260);
    }

    if (dataPath && (!Blizzard::File::MakeAbsolutePath(dataPath, root, sizeof(root), false) || !Blizzard::File::IsDirectory(dataPath))) {
        Blizzard::File::AddToLastErrorStack(0, "InitializeStreaming - Invalid data path specified", 20);
    }

    if (variant && *variant) {
        Blizzard::String::Copy(Blizzard::Storage::s_variantName, variant, sizeof(Blizzard::Storage::s_variantName));
    }

    if (!Blizzard::File::SetWorkingDirectory(root)) {
        Blizzard::File::AddToLastErrorStack(0, "InitializeStreaming - Failed to change working directory", 20);
        return false;
    }

    s_paths = new PathMap();

    bool manifestLoaded = ManifestInfo::Initialize(manifest);
    s_isStreamingTrial = FileManifest::GetIsStreamingTrial();

    if (!manifestLoaded) {
        Blizzard::File::SetWorkingDirectory(cwd);
        Blizzard::File::AddToLastErrorStack(0, "InitializeStreaming - Could not load manifest", 20);
        return false;
    }

    bool storage = true;

    if (storagePath && *storagePath) {
        if (!Blizzard::Storage::Initialize(storagePath, required)) {
            if (required) {
                storage = false;
            } else {
                storage = Blizzard::Storage::Initialize();
                Blizzard::File::Delete(storagePath);
            }
        }
    } else {
        storage = Blizzard::Storage::Initialize();
    }

    if (!storage) {
        Blizzard::File::SetWorkingDirectory(cwd);
        Blizzard::File::AddToLastErrorStack(0, "InitializeStreaming - Could not initialize storage", 20);
        return false;
    }

    s_digestStates = new DigestStateMap();

    for (int32_t i = 0; i < 7; i++) {
        s_queues[i] = new std::deque<FILE_THREAD_CMD>();
    }

    InitializeCriticalSection(&s_queueLock);
    s_queueEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    s_throttleEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);

    for (int32_t i = 0; i < 2; i++) {
        s_currentCommands[i] = nullptr;
        s_threadEvents[i] = CreateEventA(nullptr, FALSE, FALSE, nullptr);

        int32_t* thread = new int32_t;
        *thread = i;
        Blizzard::Thread::CreateDetached(DownloadThread, thread, Blizzard::String::QuickFormat<1024>("Stream BG DL %d", i).m_buffer);
    }

    s_paused = false;
    InitializeCriticalSection(&s_pauseLock);
    s_pauseEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);

    System_File::Stacked::PushStreaming();
    s_streamingReady = true;

    if (Blizzard::File::SetWorkingDirectory(cwd)) {
        return true;
    }

    Blizzard::File::AddToLastErrorStack(0, "InitializeStreaming - Failed to restore working directory", 20);
    return false;
}

// OFFSET: 0x004294C0
char* GetComponentVersionData(uint32_t* size) {
    *size = 0;

    HRSRC resource = FindResourceExA(nullptr, "BLIZZARDCOMPONENT", "BLIZZARDCOMPONENT", 0);

    if (!resource) {
        return nullptr;
    }

    HGLOBAL handle = LoadResource(nullptr, resource);

    if (!handle) {
        return nullptr;
    }

    const void* source = LockResource(handle);
    *size = SizeofResource(nullptr, resource);

    char* data = new char[*size + 1];
    data[*size] = '\0';
    memcpy(data, source, *size);

    return data;
}

// OFFSET: 0x0044B570
std::string Blizzard::Streaming::ResolveFilePath(const std::string& path) {
    if (!s_paths) {
        return std::string();
    }

    if ((*s_paths)[path].size()) {
        return (*s_paths)[path];
    }

    char absolute[260];

    if (!Blizzard::File::MakeAbsolutePath(path.c_str(), absolute, 260, false)) {
        return std::string();
    }

    char normalized[260];
    char* dst = normalized;
    const char* src = absolute;
    uint32_t left = 260;

    while (true) {
        char c = *src++;
        left--;

        if (!c) {
            *dst = '\0';
            break;
        }

        *dst++ = c == '/' ? '\\' : c;

        if (!left) {
            dst[-1] = '\0';
            break;
        }
    }

    StringToLower(normalized);
    (*s_paths)[path].assign(normalized, strlen(normalized));
    return std::string((*s_paths)[path]);
}

// OFFSET: 0x0044EFB0
bool Blizzard::Streaming::IsStreamedFile(const char* path) {
    if (!Blizzard::Streaming::s_streamingReady || !path) {
        return false;
    }

    std::string resolved = Blizzard::Streaming::ResolveFilePath(std::string(path));

    if (!Blizzard::Storage::s_storage) {
        return false;
    }

    return Blizzard::Storage::s_storage->find(resolved) != Blizzard::Storage::s_storage->end();
}

// OFFSET: 0x00457680
void StopStreaming() {
    if (Blizzard::Streaming::s_streamingReady) {
        Blizzard::Streaming::s_progress.FireCallback();
        s_stopThreads = true;
        SetEvent(s_queueEvent);
        SetEvent(s_throttleEvent);
        s_paused = false;
        SetEvent(s_pauseEvent);

        while (s_threadMask > 0) {
            Sleep(10);
        }

        CloseHandle(s_queueEvent);
        CloseHandle(s_throttleEvent);

        for (uint32_t i = 0; i < sizeof(s_threadEvents) / sizeof(s_threadEvents[0]); i++) {
            CloseHandle(s_threadEvents[i]);
            delete s_currentCommands[i];
        }

        DeleteCriticalSection(&s_queueLock);
        s_queueLock.DebugInfo = nullptr;
        CloseHandle(s_pauseEvent);
        DeleteCriticalSection(&s_pauseLock);
        s_pauseLock.DebugInfo = nullptr;

        delete s_digestStates;

        for (uint32_t i = 0; i < sizeof(s_queues) / sizeof(s_queues[0]); i++) {
            delete s_queues[i];
        }

        delete s_paths;
        s_paths = nullptr;
        Blizzard::Streaming::s_streamingReady = false;
    }

    Blizzard::Storage::Shutdown();

    delete Blizzard::Streaming::ManifestInfo::s_instance;
    Blizzard::Streaming::ManifestInfo::s_instance = nullptr;
}

