#include "Engine/Source/Gx/CGxDeviceMetal/MetalResources.h"
#include "Engine/Source/Gx/CGxDeviceMetal/MetalContext.h"

#include <algorithm>

namespace GxMetal {

// Upload heap

void UploadHeap::Init(id<MTLDevice> device) {
    this->m_device = device;
}

UploadAllocation UploadHeap::Allocate(uint32_t size, uint32_t alignment, uint64_t serial, uint64_t completed) {
    static const uint32_t kChunkSize = 8 * 1024 * 1024;

    auto tryChunk = [&](size_t index, UploadAllocation& allocation) {
        Chunk& chunk = this->m_chunks[index];
        uint32_t offset = (chunk.m_used + alignment - 1) & ~(alignment - 1);
        if (offset + size > chunk.m_size) {
            return false;
        }
        chunk.m_used = offset + size;
        chunk.m_lastSerial = serial;
        allocation.m_buffer = chunk.m_buffer;
        allocation.m_offset = offset;
        allocation.m_data = static_cast<uint8_t*>(chunk.m_buffer.contents) + offset;
        return true;
    };

    UploadAllocation allocation;

    if (this->m_current != SIZE_MAX && tryChunk(this->m_current, allocation)) {
        return allocation;
    }

    // A chunk whose allocations the GPU no longer needs starts over
    for (size_t i = 0; i < this->m_chunks.size(); i++) {
        Chunk& chunk = this->m_chunks[i];
        if (i != this->m_current && chunk.m_lastSerial <= completed && chunk.m_size >= size) {
            chunk.m_used = 0;
            if (tryChunk(i, allocation)) {
                this->m_current = i;
                return allocation;
            }
        }
    }

    Chunk chunk;
    chunk.m_size = std::max(kChunkSize, (size + 0xFFFF) & ~0xFFFFu);
    chunk.m_buffer = [this->m_device newBufferWithLength:chunk.m_size options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
    chunk.m_used = 0;
    chunk.m_lastSerial = serial;
    this->m_chunks.push_back(chunk);
    this->m_current = this->m_chunks.size() - 1;
    tryChunk(this->m_current, allocation);
    return allocation;
}

// Buffers

Buffer::Buffer(Context* context, uint32_t length, bool dynamic, bool keepShadow) : m_context(context), m_length(length), m_dynamic(dynamic) {
    if (length) {
        this->m_buffer = [context->m_device newBufferWithLength:length options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
    }
    if (keepShadow) {
        this->m_shadow.resize(length);
    }
}

char* Buffer::Lock(uint32_t offset, uint32_t size, BufferLock lock) {
    if (!this->m_buffer || offset > this->m_length) {
        return nullptr;
    }

    if (size == 0 || offset + size > this->m_length) {
        size = this->m_length - offset;
    }

    uint64_t completed = this->m_context->CompletedSerial();
    LockRecord record = { offset, size, nullptr, false, {} };

    if (lock == BufferLock_Discard && this->m_dynamic && this->m_use.m_lastUse > completed) {
        // Rename: the GPU keeps the old memory, the client gets memory the GPU is done with
        this->m_spares.emplace_back(this->m_buffer, this->m_use.m_lastUse);

        id<MTLBuffer> next = nil;
        for (size_t i = 0; i + 1 < this->m_spares.size(); i++) {
            if (this->m_spares[i].second <= completed) {
                next = this->m_spares[i].first;
                this->m_spares.erase(this->m_spares.begin() + i);
                break;
            }
        }

        if (!next) {
            next = [this->m_context->m_device newBufferWithLength:this->m_length options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
            if (!next) {
                return nullptr;
            }
        }

        this->m_buffer = next;
        this->m_use = GpuUse();
    }

    if (lock != BufferLock_Normal || this->m_use.m_lastUse <= completed) {
        // The client guarantees, or the serials show, that the GPU does not read the range
        record.m_data = static_cast<uint8_t*>(this->m_buffer.contents) + offset;
    } else {
        // The GPU still reads the buffer: stage the write and copy it in order. The pools are
        // write-only, so the staged range needs no previous contents.
        record.m_staged = true;
        record.m_staging = this->m_context->AllocateUpload(size, 16);
        record.m_data = record.m_staging.m_data;
    }

    this->m_locks.push_back(record);
    return reinterpret_cast<char*>(record.m_data);
}

void Buffer::Unlock() {
    if (this->m_locks.empty()) {
        return;
    }

    LockRecord record = this->m_locks.back();
    this->m_locks.pop_back();

    if (!this->m_shadow.empty() && record.m_size) {
        memcpy(this->m_shadow.data() + record.m_offset, record.m_data, record.m_size);
    }

    if (record.m_staged && record.m_size) {
        this->m_context->CopyBuffer(record.m_staging, this->m_buffer, record.m_offset, record.m_size, this->m_use);
    }
}

// Textures

Texture* Texture::Create(Context* context, MTLTextureType type, uint32_t width, uint32_t height, uint32_t levels, const FormatInfo& format, bool renderTarget, bool autoMips) {
    if (format.m_pixelFormat == MTLPixelFormatInvalid || !width || !height || (renderTarget && !format.m_renderable)) {
        return nullptr;
    }

    uint32_t fullChain = 1;
    for (uint32_t size = std::max(width, height); size > 1; size >>= 1) {
        fullChain++;
    }

    if (levels == 0 || levels > fullChain) {
        levels = fullChain;
    }

    // Depth textures have a single level
    if (format.m_depth) {
        levels = 1;
    }

    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor new];
    descriptor.textureType = type;
    descriptor.pixelFormat = format.m_pixelFormat;
    descriptor.width = width;
    descriptor.height = type == MTLTextureTypeCube ? width : height;
    descriptor.mipmapLevelCount = levels;
    descriptor.storageMode = MTLStorageModePrivate;
    descriptor.usage = MTLTextureUsageShaderRead;

    if (renderTarget || format.m_depth) {
        descriptor.usage |= MTLTextureUsageRenderTarget;
    }

    if (format.m_swizzled) {
        descriptor.usage |= MTLTextureUsagePixelFormatView;
    }

    id<MTLTexture> texture = [context->m_device newTextureWithDescriptor:descriptor];
    if (!texture) {
        return nullptr;
    }

    Texture* created = new Texture(context);
    created->m_format = format;
    created->m_type = type;
    created->m_width = width;
    created->m_height = type == MTLTextureTypeCube ? width : height;
    created->m_levels = levels;
    created->m_renderTarget = renderTarget;
    created->m_autoMips = autoMips;
    created->m_texture = texture;

    if (format.m_swizzled) {
        created->m_view = [texture newTextureViewWithPixelFormat:format.m_pixelFormat textureType:type levels:NSMakeRange(0, levels) slices:NSMakeRange(0, type == MTLTextureTypeCube ? 6 : 1) swizzle:format.m_swizzle];
    }

    return created;
}

Texture* Texture::CreateTarget(Context* context, const FormatInfo& format, uint32_t width, uint32_t height, uint32_t samples) {
    if (!format.m_renderable || !width || !height) {
        return nullptr;
    }

    // Metal aborts on a sample count the GPU lacks, and a gxMultisample of 8 from a Windows config is one on Apple GPUs
    while (samples > 1 && ![context->m_device supportsTextureSampleCount:samples]) {
        samples--;
    }

    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format.m_pixelFormat width:width height:height mipmapped:NO];
    descriptor.storageMode = MTLStorageModePrivate;
    // Targets stay shader readable, multisampled ones too, so code drawing with Metal itself (mods) can read
    // the frame without resolving its attachments
    descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;

    if (samples > 1) {
        descriptor.textureType = MTLTextureType2DMultisample;
        descriptor.sampleCount = samples;
    }

    id<MTLTexture> texture = [context->m_device newTextureWithDescriptor:descriptor];
    if (!texture) {
        return nullptr;
    }

    Texture* created = new Texture(context);
    created->m_format = format;
    created->m_width = width;
    created->m_height = height;
    created->m_samples = samples;
    created->m_renderTarget = true;
    created->m_texture = texture;

    if (samples > 1 && !format.m_depth) {
        MTLTextureDescriptor* resolve = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format.m_pixelFormat width:width height:height mipmapped:NO];
        resolve.storageMode = MTLStorageModePrivate;
        resolve.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        created->m_resolve = [context->m_device newTextureWithDescriptor:resolve];
        if (!created->m_resolve) {
            created->Release();
            return nullptr;
        }
    }

    return created;
}

id<MTLTexture> Texture::ResolvedTexture() {
    if (this->m_samples > 1) {
        if (this->m_resolveDirty) {
            this->m_context->Resolve(this);
        }
        return this->m_resolve;
    }
    return this->m_texture;
}

SamplerKind Texture::Kind() const {
    if (this->m_format.m_depth) {
        return SAMPLERKIND_DEPTH;
    }
    return this->m_type == MTLTextureTypeCube ? SAMPLERKIND_CUBE : SAMPLERKIND_2D;
}

void Texture::MarkWritten(uint32_t level) {
    if (this->m_samples > 1) {
        this->m_resolveDirty = true;
    }

    if (this->m_autoMips && level == 0) {
        this->m_mipsDirty = true;
    }
}

// Queries

Query::~Query() {
    this->m_context->EndOcclusionQuery(this);
}

void Query::Begin() {
    if (this->m_type != Type_Occlusion) {
        return;
    }

    if (this->m_state == State_Building) {
        this->m_context->EndOcclusionQuery(this);
    }

    this->m_context->BeginOcclusionQuery(this);
    this->m_state = State_Building;
}

void Query::End() {
    if (this->m_type == Type_Occlusion) {
        if (this->m_state == State_Building) {
            this->m_context->EndOcclusionQuery(this);
        } else {
            // Ending without beginning counts nothing
            this->m_slots.clear();
        }
    }

    this->m_serial = this->m_context->CurrentSerial();
    this->m_state = State_Issued;
}

bool Query::Result(uint32_t* samples, bool flush) {
    if (this->m_state == State_Building) {
        return false;
    }

    if (this->m_state == State_Issued && this->m_context->CompletedSerial() < this->m_serial) {
        if (flush && this->m_serial == this->m_context->CurrentSerial()) {
            this->m_context->Flush(false);
        }

        if (this->m_context->CompletedSerial() < this->m_serial) {
            return false;
        }
    }

    if (samples) {
        uint64_t total = 1;

        if (this->m_type == Type_Occlusion) {
            total = 0;
            for (const Slot& slot : this->m_slots) {
                total += *reinterpret_cast<const uint64_t*>(static_cast<const uint8_t*>(slot.m_buffer.contents) + slot.m_offset);
            }
        }

        *samples = static_cast<uint32_t>(std::min<uint64_t>(total, UINT32_MAX));
    }

    return true;
}

}
