#include "OggDecompress.h"
#include <cstdint>
#include <cstring>
#include <vector>
#include <vorbis/vorbisfile.h>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/BlizzardCore/Source/System/Memory/MemoryStorm.h"

// VERIFY: 70759a69 manual
// OFFSET: 0x0041D140
static int OggSeek(void* source, ogg_int64_t offset, int whence) {
    OggMemoryStream* stream = static_cast<OggMemoryStream*>(source);
    uint32_t position;

    if (whence == 0) {
        position = static_cast<uint32_t>(offset);
    } else if (whence == 1) {
        position = static_cast<uint32_t>(offset) + stream->m_position;
    } else if (whence == 2) {
        position = static_cast<uint32_t>(offset) + stream->m_size;
    } else {
        position = stream->m_position;
    }

    if (position > stream->m_size) {
        return -1;
    }

    stream->m_position = position;
    return 0;
}

// VERIFY: 4c4b1c84 auto
// OFFSET: 0x0041D190
static ogg_int32_t OggTell(void* source) {
    return static_cast<OggMemoryStream*>(source)->m_position;
}

// VERIFY: 37fbe6bc manual
// OFFSET: 0x0041D1A0
size_t OggMemoryStream::Read(void* buffer, size_t size, size_t count) {
    size_t left = this->m_size - this->m_position;
    size_t bytes = size * count;
    size_t copy = size * ((left < bytes ? left : bytes) / size);

    memcpy(buffer, this->m_data + this->m_position, copy);
    this->m_position += static_cast<uint32_t>(copy);

    return copy;
}

// VERIFY: ca21f279 auto
// OFFSET: 0x0041D300
static size_t OggRead(void* buffer, size_t size, size_t count, void* source) {
    return static_cast<OggMemoryStream*>(source)->Read(buffer, size, count);
}

// VERIFY: 41efffe8 auto
// OFFSET: 0x008E5250
static int OggClose(void* source) {
    return 0;
}

// VERIFY: f640fb08 manual
// OFFSET: 0x0041D600
OggChunkList::~OggChunkList() {
    for (std::vector<OggChunk>::iterator it = this->m_chunks->begin(); it != this->m_chunks->end(); ++it) {
        Blizzard::Memory::Free(it->m_data, 0, __FILE__, __LINE__);
    }
}

// VERIFY: 3701e837 manual
// OFFSET: 0x0041DDA0
bool DecompressOgg(const void* data, uint32_t size, OggDecompressBuffer* buffer) {
    std::vector<OggChunk> chunks;
    OggChunkList chunkList;
    chunkList.m_chunks = &chunks;

    OggMemoryStream stream;
    stream.m_data = static_cast<const uint8_t*>(data);
    stream.m_size = size;
    stream.m_position = 0;

    ov_callbacks callbacks;
    callbacks.read_func = OggRead;
    callbacks.seek_func = OggSeek;
    callbacks.close_func = OggClose;
    callbacks.tell_func = OggTell;

    OggVorbis_File file;

    if (ov_open_callbacks(&stream, &file, nullptr, 0, callbacks)) {
        return false;
    }

    uint32_t rate = 0;
    uint32_t channels = 0;
    uint32_t total = 0;

    vorbis_info* info = ov_info(&file, -1);

    while (info) {
        if (info->channels != 1 && info->channels != 2) {
            ov_clear(&file);
            return false;
        }

        rate = rate < static_cast<uint32_t>(info->rate) ? info->rate : rate;
        channels = channels < static_cast<uint32_t>(info->channels) ? info->channels : channels;

        char samples[0x2000];
        ogg_int32_t read = ov_read(&file, samples, sizeof(samples), 0, 2, 1, nullptr);

        if (read < 0) {
            ov_clear(&file);
            return false;
        }

        if (!read) {
            buffer->Resize(total + 44);

            uint8_t* header = buffer->m_data;
            *reinterpret_cast<uint32_t*>(header + 0) = 'FFIR';
            *reinterpret_cast<uint32_t*>(header + 4) = total + 36;
            *reinterpret_cast<uint32_t*>(header + 8) = 'EVAW';
            *reinterpret_cast<uint32_t*>(header + 12) = ' tmf';
            *reinterpret_cast<uint32_t*>(header + 16) = 16;
            *reinterpret_cast<uint16_t*>(header + 20) = 1;
            *reinterpret_cast<uint16_t*>(header + 22) = channels;
            *reinterpret_cast<uint32_t*>(header + 24) = rate;
            *reinterpret_cast<uint32_t*>(header + 28) = 2 * rate * channels;
            *reinterpret_cast<uint16_t*>(header + 32) = 2 * channels;
            *reinterpret_cast<uint16_t*>(header + 34) = 16;
            *reinterpret_cast<uint32_t*>(header + 36) = 'atad';
            *reinterpret_cast<uint32_t*>(header + 40) = total;

            uint32_t offset = 44;

            for (std::vector<OggChunk>::iterator it = chunks.begin(); it != chunks.end(); ++it) {
                if (it->m_rate != rate || it->m_channels != channels) {
                    ov_clear(&file);
                    return false;
                }

                memcpy(buffer->m_data + offset, it->m_data, it->m_size);
                offset += it->m_size;
            }

            ov_clear(&file);
            return true;
        }

        void* chunkData = Blizzard::Memory::Allocate(read, 0, __FILE__, __LINE__);
        memcpy(chunkData, samples, 2 * (read >> 1));

        OggChunk chunk;
        chunk.m_rate = info->rate;
        chunk.m_channels = info->channels;
        chunk.m_data = chunkData;
        chunk.m_size = read;
        chunks.push_back(chunk);

        info = ov_info(&file, -1);
        total += read;
    }

    ov_clear(&file);
    return false;
}

// VERIFY: 90d5a819 auto
// OFFSET: 0x0041D0B0
bool ShutdownOggDecompression() {
    return System_SFile2::ShutdownSoundCache();
}

// VERIFY: 06689c00 auto
// OFFSET: 0x0041E170
bool InitOggDecompression() {
    return System_SFile2::InitSoundCache(DecompressOgg);
}
