#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <vector>
#include "Engine/Source/BlizzardCore/Source/Packages/Streaming/Manifests.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Streaming/Storage.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Streaming/Streaming.h"
#include "Engine/Source/BlizzardCore/Source/System/File/File.h"
#include "Engine/Source/BlizzardCore/Source/System/File/Filestack_Streaming.h"
#include "Engine/Source/BlizzardCore/Source/System/Log/Log.h"
#include "Engine/Source/BlizzardCore/Source/System/Memory/MemoryStorm.h"
#include "Engine/Source/BlizzardCore/Source/System/String/String.h"
#include "Engine/Source/BlizzardCore/Source/System/Unicode/Unicode.h"

// STATIC: 0x00AB9240
Blizzard::File::Filesystem System_File::Stacked::s_streamstack = {
    &System_File::Stacked::s_streamstack,
    nullptr,
    {
        System_File::Stacked::PassThrough,
        System_File::Stacked::stream_close,
        System_File::Stacked::PassThrough,
        System_File::Stacked::PassThrough,
        System_File::Stacked::stream_dirwalk,
        System_File::Stacked::stream_exists,
        System_File::Stacked::stream_flush,
        System_File::Stacked::stream_getfileinfo,
        System_File::Stacked::PassThrough,
        System_File::Stacked::PassThrough,
        System_File::Stacked::PassThrough,
        System_File::Stacked::PassThrough,
        System_File::Stacked::PassThrough,
        System_File::Stacked::PassThrough,
        System_File::Stacked::PassThrough,
        System_File::Stacked::PassThrough,
        System_File::Stacked::PassThrough,
        System_File::Stacked::stream_open,
        System_File::Stacked::stream_read,
        System_File::Stacked::stream_readp,
        System_File::Stacked::PassThrough,
        System_File::Stacked::PassThrough,
        System_File::Stacked::PassThrough,
        System_File::Stacked::PassThrough,
        System_File::Stacked::PassThrough,
        System_File::Stacked::PassThrough,
        System_File::Stacked::PassThrough,
        System_File::Stacked::PassThrough,
        System_File::Stacked::stream_shutdown
    }
};

// VERIFY: d80e4dd6 manual
// OFFSET: 0x00427AD0
int32_t STORMAPI System_File::Stacked::ExistsAt(Blizzard::File::Filesystem* fs, const char* path) {
    FileParms parms;
    parms.m_path = const_cast<char*>(path);
    parms.m_verb = offsetof(Blizzard::File::Filesystem, m_verbs.m_exists);
    parms.m_info = &parms.m_info2;

    if (!fs || !fs->m_verbs.m_exists(fs, &parms)) {
        return 0;
    }

    return parms.m_info->m_existence;
}

// VERIFY: c29b39e1 manual
// OFFSET: 0x00427B30
Blizzard::File::StreamRecord* STORMAPI System_File::Stacked::OpenAt(Blizzard::File::Filesystem* fs, Blizzard::File::StreamRecord* file, const char* path, int32_t mode) {
    FileParms parms;
    parms.m_stream = file;
    parms.m_verb = offsetof(Blizzard::File::Filesystem, m_verbs.m_open);
    parms.m_path = const_cast<char*>(path);
    parms.m_mode = mode;

    if (!fs || !fs->m_verbs.m_open(fs, &parms)) {
        return nullptr;
    }

    return parms.m_stream;
}

// VERIFY: 71a6dd7e manual
// OFFSET: 0x00427B90
bool STORMAPI System_File::Stacked::CloseAt(Blizzard::File::Filesystem* fs, Blizzard::File::StreamRecord* file) {
    FileParms parms;
    parms.m_stream = file;
    parms.m_verb = offsetof(Blizzard::File::Filesystem, m_verbs.m_close);

    if (!fs) {
        return false;
    }

    return fs->m_verbs.m_close(fs, &parms);
}

// VERIFY: 9a01a8e1 manual
// OFFSET: 0x00427BE0
bool System_File::Stacked::stream_read(Blizzard::File::Filesystem* fs, FileParms* parms) {
    Blizzard::File::StreamingInfo* streaming = parms->m_stream->m_streaming;

    if ((streaming && streaming->m_entry) || !fs->m_next) {
        return false;
    }

    return (*reinterpret_cast<FileFunc*>(reinterpret_cast<uintptr_t>(fs->m_next) + parms->m_verb))(fs->m_next, parms);
}

// VERIFY: 74024bbf manual
// OFFSET: 0x0042D5C0
void System_File::Stacked::PushStreaming() {
    Blizzard::File::Filesystem* fs = static_cast<Blizzard::File::Filesystem*>(malloc(sizeof(Blizzard::File::Filesystem)));
    Blizzard::String::MemCopy(fs, &s_streamstack, sizeof(Blizzard::File::Filesystem));
    fs->m_next = s_manager;
    s_manager = fs;
    HoistAll();
}

// VERIFY: 4816cb8b manual
// OFFSET: 0x004354C0
bool STORMAPI System_File::Stacked::CreatePartFile(Blizzard::File::Filesystem* fs, FileParms* parms) {
    char name[260];
    Blizzard::Storage::BuildVariantFileName(name, sizeof(name), parms->m_path, "part");

    FileParms create;
    create.m_verb = offsetof(Blizzard::File::Filesystem, m_verbs.m_open);
    create.m_stream = nullptr;
    create.m_path = name;
    create.m_mode = 0x403;

    if (!fs->m_next || !fs->m_next->m_verbs.m_open(fs->m_next, &create) || !create.m_stream) {
        return false;
    }

    create.m_verb = offsetof(Blizzard::File::Filesystem, m_verbs.m_close);

    if (fs->m_next) {
        fs->m_next->m_verbs.m_close(fs->m_next, &create);
    }

    return true;
}

// VERIFY: 78534a15 manual
// OFFSET: 0x00439AA0
Blizzard::File::StreamRecord* STORMAPI System_File::Stacked::AllocStreamingRecord(const char* path) {
    uint32_t length = static_cast<uint32_t>(path ? strlen(path) : 0);

    Blizzard::File::StreamRecord* record = static_cast<Blizzard::File::StreamRecord*>(Blizzard::Memory::Allocate(sizeof(Blizzard::File::StreamRecord) + sizeof(Blizzard::File::StreamingInfo) + length + 1, 8, __FILE__, __LINE__));
    record->m_streaming = reinterpret_cast<Blizzard::File::StreamingInfo*>(record + 1);
    record->m_pathname = reinterpret_cast<char*>(record->m_streaming + 1);
    new (record->m_streaming) Blizzard::File::StreamingInfo;
    Blizzard::String::Copy(record->m_pathname, path, length + 1);

    return record;
}

// VERIFY: 03e5c89b manual
// OFFSET: 0x0043D710
Blizzard::File::StreamRecord* FileStack_Streaming::InitRecord(Blizzard::File::StreamRecord* record) {
    Blizzard::String::MemCopy(&record->m_streaming->m_info, &record->m_info, sizeof(Blizzard::File::FileInfo));
    record->m_info.m_path = record->m_pathname;
    record->m_info.m_size = m_file->m_size;
    record->m_info.m_regularFile = true;
    record->m_infoValid = true;
    record->m_info.m_createTime = 0;
    record->m_info.m_modTime = 0;
    record->m_info.m_accessTime = 0;
    record->m_info.m_attributes = ATTRIBUTE_NORMAL;
    record->m_streaming->m_entry = m_entry;
    record->m_streaming->m_path = m_key;

    return record;
}

// VERIFY: 5c9f211d manual
// OFFSET: 0x00449CD0
bool StreamDirWalk::Callback(const Blizzard::File::ProcessDirParms& parms) {
    static_cast<StreamDirWalk*>(parms.m_cookie)->m_files[std::string(parms.m_filename)] = parms.m_isDir;
    return true;
}

// VERIFY: 206882ea manual
// OFFSET: 0x00452190
bool System_File::Stacked::stream_readp(Blizzard::File::Filesystem* fs, FileParms* parms) {
    Blizzard::File::StreamRecord* file = parms->m_stream;

    if (!file->m_streaming || !file->m_streaming->m_entry) {
        if (!fs->m_next) {
            return false;
        }

        return (*reinterpret_cast<FileFunc*>(reinterpret_cast<uintptr_t>(fs->m_next) + parms->m_verb))(fs->m_next, parms);
    }

    uint8_t* start = static_cast<uint8_t*>(parms->m_buffer);
    int32_t count;
    Blizzard::Streaming::LoadRange* ranges = Blizzard::Storage::GetRange(file->m_streaming->m_entry, parms->m_pos, parms->m_amount, &count, true);

    if (!ranges || !count) {
        parms->m_amount = 0;
        return false;
    }

    for (int32_t i = 0; i < count; i++) {
        if (static_cast<uint32_t>(ranges[i].m_state) >= 2) {
            if (!Blizzard::Streaming::RequestRanges(file, ranges, count)) {
                Blizzard::Memory::Free(ranges);
                parms->m_amount = 0;
                return false;
            }

            break;
        }
    }

    uint8_t* buffer = start;
    bool result = true;

    for (Blizzard::Streaming::LoadRange* range = ranges; count > 0 && result; count--, range++) {
        if (Blizzard::Storage::ReadLocal(fs->m_next, file, file->m_streaming->m_entry, buffer, range->m_offset, range->m_size)) {
            buffer += range->m_size;
        } else {
            result = false;
        }

        if (!range->m_state) {
            Blizzard::Log::Printf(3, "Logs/Profiling_File_Local.log", "%10lld, %10d - %s\n", range->m_offset, range->m_size, file->m_pathname);
            Blizzard::Streaming::s_statBytesLocal += range->m_size;
        }
    }

    parms->m_amount = static_cast<int32_t>(buffer - start);
    Blizzard::Memory::Free(ranges);

    return result;
}

// VERIFY: 563da2f5 manual
// OFFSET: 0x004542B0
bool StreamDirWalk::Walk(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    char buffer[1024];

    System_File::Stacked::FileParms walk = *parms;
    walk.m_cookie = this;
    walk.m_dircallback = Callback;

    if (!fs->m_next || !(*reinterpret_cast<System_File::Stacked::FileFunc*>(reinterpret_cast<uintptr_t>(fs->m_next) + walk.m_verb))(fs->m_next, &walk)) {
        return false;
    }

    std::vector<std::string> files = Blizzard::Streaming::FileManifest::GetFileList(parms->m_path);

    for (std::vector<std::string>::iterator it = files.begin(); it != files.end(); ++it) {
        if (m_files.find(*it) == m_files.end()) {
            m_files[*it] = false;
        }
    }

    Blizzard::File::ProcessDirParms dir;
    dir.m_pathRoot = parms->m_path;
    dir.m_cookie = parms->m_cookie;

    for (std::map<std::string, bool>::iterator it = m_files.begin(); it != m_files.end(); ++it) {
        System_File::FromNativeName(buffer, sizeof(buffer), Blizzard::StringConversion_CodepageToUTF8::ToNative(it->first.c_str()).m_data);
        dir.m_filename = buffer;
        dir.m_isDir = it->second;

        if (!parms->m_dircallback(dir)) {
            return false;
        }
    }

    return true;
}

// VERIFY: ba20daaa manual
// OFFSET: 0x00454540
bool System_File::Stacked::stream_exists(Blizzard::File::Filesystem* fs, FileParms* parms) {
    if (!parms->m_path) {
        parms->m_info->m_existence = EXISTENCE_NONE;
        return true;
    }

    std::string key = Blizzard::Streaming::ResolveFilePath(std::string(parms->m_path));
    Blizzard::Streaming::Manifests::FileItem* item = Blizzard::Streaming::ManifestInfo::GetFileItem(key);

    if (item) {
        parms->m_info->m_existence = item->m_size ? EXISTENCE_FILE : EXISTENCE_DIRECTORY;
        return true;
    }

    if (!fs->m_next) {
        return false;
    }

    return (*reinterpret_cast<FileFunc*>(reinterpret_cast<uintptr_t>(fs->m_next) + parms->m_verb))(fs->m_next, parms);
}

// VERIFY: 8fa7cb2e manual
// OFFSET: 0x00454680
bool System_File::Stacked::stream_getfileinfo(Blizzard::File::Filesystem* fs, FileParms* parms) {
    if (!parms->m_path) {
        if (parms->m_stream) {
            Blizzard::Streaming::Manifests::FileItem* item = nullptr;

            if (parms->m_stream->m_streaming) {
                item = Blizzard::Streaming::ManifestInfo::GetFileItem(parms->m_stream->m_streaming->m_path);
            }

            if (!fs->m_next || !(*reinterpret_cast<FileFunc*>(reinterpret_cast<uintptr_t>(fs->m_next) + parms->m_verb))(fs->m_next, parms)) {
                return false;
            }

            if (item) {
                parms->m_info->m_size = item->m_size;
            }
        }

        return true;
    }

    std::string key = Blizzard::Streaming::ResolveFilePath(std::string(parms->m_path));
    Blizzard::Streaming::Manifests::FileItem* item = Blizzard::Streaming::ManifestInfo::GetFileItem(key);

    if (item) {
        parms->m_info->m_size = item->m_size;
        parms->m_info->m_path = parms->m_path;
        parms->m_info->m_createTime = 0;
        parms->m_info->m_modTime = 0;
        parms->m_info->m_accessTime = 0;
        parms->m_info->m_regularFile = true;
        parms->m_info->m_existence = item->m_size > 0 ? EXISTENCE_FILE : EXISTENCE_DIRECTORY;
        parms->m_info->m_attributes = ATTRIBUTE_NORMAL;
        return true;
    }

    if (!fs->m_next) {
        return false;
    }

    return (*reinterpret_cast<FileFunc*>(reinterpret_cast<uintptr_t>(fs->m_next) + parms->m_verb))(fs->m_next, parms);
}

// VERIFY: 07b05e40 manual
// OFFSET: 0x004589F0
bool System_File::Stacked::stream_dirwalk(Blizzard::File::Filesystem* fs, FileParms* parms) {
    StreamDirWalk walk;
    return walk.Walk(fs, parms);
}

// VERIFY: e830e690 manual
// OFFSET: 0x00459020
bool System_File::Stacked::stream_shutdown(Blizzard::File::Filesystem* fs, FileParms* parms) {
    StopStreaming();

    if (!fs->m_next) {
        return false;
    }

    return (*reinterpret_cast<FileFunc*>(reinterpret_cast<uintptr_t>(fs->m_next) + parms->m_verb))(fs->m_next, parms);
}

// VERIFY: 23df52d2 manual
// OFFSET: 0x0045A830
bool System_File::Stacked::stream_open(Blizzard::File::Filesystem* fs, FileParms* parms) {
    FileStack_Streaming stack;
    Blizzard::File::StreamRecord* file = stack.StreamingOpen(fs, parms);

    if (file) {
        parms->m_stream = file;
        return true;
    }

    if (!fs->m_next) {
        return false;
    }

    return (*reinterpret_cast<FileFunc*>(reinterpret_cast<uintptr_t>(fs->m_next) + parms->m_verb))(fs->m_next, parms);
}
