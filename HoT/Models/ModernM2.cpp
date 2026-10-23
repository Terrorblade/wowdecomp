#include "HoT/Models/ModernM2.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include "Engine/Source/Base/SIMD.h"
#include "Engine/Source/Model2/M2Data.h"
#include "Engine/Source/Os/OsMemory.h"
#include "HoT/Core/Log.h"
#include "HoT/Core/Modules.h"
#include "HoT/Editor/Resources.h"
#include "HoT/Models/FileDataId.h"

namespace HoT::Models {

static constexpr uint32_t Tag(const char (&s)[5]) {
    return static_cast<uint32_t>(static_cast<uint8_t>(s[0])) | (static_cast<uint32_t>(static_cast<uint8_t>(s[1])) << 8) | (static_cast<uint32_t>(static_cast<uint8_t>(s[2])) << 16) | (static_cast<uint32_t>(static_cast<uint8_t>(s[3])) << 24);
}

static const uint32_t MAGIC_MD20 = 0x3032444D;
static const uint32_t MAGIC_MD21 = 0x3132444D;
static const uint32_t MAGIC_AFM2 = 0x324D4641;
static const uint32_t MAGIC_SKIN = 0x4E494B53;

static const uint32_t CLIENT_VERSION = 264;
static const uint32_t SOURCE_VERSION_MIN = 272;
static const uint32_t SOURCE_VERSION_MAX = 274;

static const uint32_t PARTICLE_STRIDE_SOURCE = 0x1EC;
static const uint32_t CAMERA_STRIDE_SOURCE = 0x74;
static const uint32_t MAX_BATCHES = 0x4000;

static const uint32_t SEQUENCE_KEYS_INLINE = 0x20;
static const uint32_t PARTICLE_FLAG_COMPRESSED_GRAVITY = 0x800000;
static const uint32_t PARTICLE_FLAG_MULTI_TEXTURE = 0x10000000;
static const uint16_t MATERIAL_FLAG_MASK = 0x1F | 0x40 | 0x80;
static const uint16_t MODERN_BLEND_MAX = 7;

// Byte offsets inside the client particle, camera and track records.
static const uint32_t PARTICLE_FLAGS = 0x04;
static const uint32_t PARTICLE_TEXTURE_INDEX = 0x16;
static const uint32_t PARTICLE_GRAVITY_TRACK = 0x84;
static const uint32_t TRACK_LOOP_INDEX = 0x02;
static const uint32_t TRACK_SEQUENCE_KEYS = 0x0C;
static const uint32_t CAMERA_TYPE = 0x00;
static const uint32_t CAMERA_FIELD_OF_VIEW = 0x04;
static const uint32_t CAMERA_FAR_CLIP = 0x08;
static const uint32_t CAMERA_NEAR_CLIP = 0x0C;

enum SkipMask : uint32_t {
    SKIP_TXAC = 0x01,
    SKIP_LDV1 = 0x02,
    SKIP_SKID = 0x08,
    SKIP_PHYS_BONE = 0x10,
    SKIP_OTHER = 0x80,
};

static_assert(sizeof(M2Particle) == 0x1DC, "particle record");
static_assert(sizeof(M2Camera) == 0x64, "camera record");
static_assert(sizeof(M2SkinSectionFile) == 0x30, "skin section file record");
static_assert(sizeof(M2Batch) == 0x18, "batch record");

struct AnimFile {
    uint16_t m_id;
    uint16_t m_variation;
    uint32_t m_fileDataId;
};

class ModernModel {
    public:
    ~ModernModel();
    void* Alloc(const void* anchor, size_t size);

    std::string m_name;
    uint32_t m_version = 0;
    uint32_t m_skipMask = 0;
    bool m_extended = false;
    std::vector<uint32_t> m_textureIds;
    std::vector<uint32_t> m_skinIds;
    std::vector<AnimFile> m_animIds;
    std::vector<uint32_t> m_boneIds;
    std::vector<uint16_t> m_sourceSequenceIds;
    std::vector<void*> m_allocations;
    std::string m_path;
};

// M2Array offsets are self-relative 32-bit values, so anything a header array points at must sit near the model.
static void* AllocNear(const void* anchor, size_t size) {
    return OsAllocNear(anchor, size);
}

static void FreeNear(void* ptr) {
    OsFreeNear(ptr);
}

ModernModel::~ModernModel() {
    for (void* ptr : this->m_allocations) {
        FreeNear(ptr);
    }
}

void* ModernModel::Alloc(const void* anchor, size_t size) {
    void* ptr = AllocNear(anchor, size ? size : 1);

    if (ptr) {
        this->m_allocations.push_back(ptr);
    }

    return ptr;
}

static uint32_t Read32(const void* p) {
    uint32_t value;
    memcpy(&value, p, sizeof(value));
    return value;
}

static uint16_t Read16(const void* p) {
    uint16_t value;
    memcpy(&value, p, sizeof(value));
    return value;
}

static void Write32(void* p, uint32_t value) {
    memcpy(p, &value, sizeof(value));
}

static void Write16(void* p, uint16_t value) {
    memcpy(p, &value, sizeof(value));
}

// count records of stride bytes at offset lie inside size bytes, computed so a hostile count cannot wrap.
static bool Fits(uint32_t size, uint32_t offset, uint32_t count, uint32_t stride) {
    if (!count) {
        return true;
    }

    return offset <= size && static_cast<uint64_t>(count) * stride <= static_cast<uint64_t>(size) - offset;
}

static bool StartsWithPath(const char* path, const char* prefix) {
    for (; *prefix; path++, prefix++) {
        char a = *path == '/' ? '\\' : static_cast<char>(tolower(static_cast<unsigned char>(*path)));

        if (a != *prefix) {
            return false;
        }
    }

    return true;
}

// Character and item component skins spell a section's index start as (level << 16) | indexStart, every other source keeps level as a LOD marker.
static bool UsesExtendedIndexStart(const char* name) {
    return StartsWithPath(name, "character\\") || StartsWithPath(name, "characterhd\\") || StartsWithPath(name, "item\\objectcomponents\\");
}

bool IsModernContainer(const void* data, uint32_t size) {
    return data && size >= 8 && Read32(data) == MAGIC_MD21;
}

struct Scan {
    uint32_t m_bodyOffset = 0;
    uint32_t m_bodySize = 0;
};

static bool ScanContainer(const uint8_t* buf, uint32_t size, Scan& scan, ModernModel& model) {
    uint32_t offset = 0;

    while (offset + 8 <= size) {
        uint32_t tag = Read32(buf + offset);
        uint32_t chunkSize = Read32(buf + offset + 4);

        if (chunkSize > size - offset - 8) {
            break;
        }

        const uint8_t* payload = buf + offset + 8;

        switch (tag) {
            case Tag("MD21"):
                scan.m_bodyOffset = offset + 8;
                scan.m_bodySize = chunkSize;
                break;
            case Tag("TXID"):
                for (uint32_t i = 0; i < chunkSize / 4; i++) {
                    model.m_textureIds.push_back(Read32(payload + i * 4));
                }

                break;
            case Tag("SFID"):
                for (uint32_t i = 0; i < chunkSize / 4; i++) {
                    model.m_skinIds.push_back(Read32(payload + i * 4));
                }

                break;
            case Tag("AFID"):
                for (uint32_t i = 0; i < chunkSize / 8; i++) {
                    AnimFile anim;
                    anim.m_id = Read16(payload + i * 8);
                    anim.m_variation = Read16(payload + i * 8 + 2);
                    anim.m_fileDataId = Read32(payload + i * 8 + 4);
                    model.m_animIds.push_back(anim);
                }

                break;
            case Tag("BFID"):
                for (uint32_t i = 0; i < chunkSize / 4; i++) {
                    model.m_boneIds.push_back(Read32(payload + i * 4));
                }

                model.m_skipMask |= SKIP_PHYS_BONE;
                break;
            case Tag("PFID"):
                model.m_skipMask |= SKIP_PHYS_BONE;
                break;
            case Tag("SKID"):
                model.m_skipMask |= SKIP_SKID;
                break;
            case Tag("TXAC"):
                model.m_skipMask |= SKIP_TXAC;
                break;
            case Tag("LDV1"):
                model.m_skipMask |= SKIP_LDV1;
                break;
            default:
                model.m_skipMask |= SKIP_OTHER;
                break;
        }

        offset += 8 + chunkSize;
    }

    return scan.m_bodyOffset && scan.m_bodySize >= sizeof(M2Data) && scan.m_bodySize <= size - scan.m_bodyOffset;
}

static int32_t SequenceIndexById(const M2Sequence* sequences, uint32_t count, uint16_t id) {
    for (uint32_t i = 0; i < count; i++) {
        if (sequences[i].m_id == id) {
            return static_cast<int32_t>(i);
        }
    }

    return -1;
}

static void PatchSequenceLookup(uint16_t* lookup, uint32_t lookupCount, uint16_t oldPos, uint16_t newId, uint16_t newPos) {
    if (!lookup) {
        return;
    }

    if (newId < lookupCount && lookup[newId] == oldPos) {
        lookup[newId] = newPos;
        return;
    }

    for (uint32_t i = 0; i < lookupCount; i++) {
        if (lookup[i] == oldPos) {
            lookup[i] = newPos;
            break;
        }
    }
}

// Splits the u16 in and out blend times back to the client's one value and remaps the source ids the client has no slot for.
static void FixSequences(uint8_t* base, uint32_t size, M2Data* header, ModernModel& model) {
    uint32_t count = header->m_sequences.m_count;

    if (!count || !Fits(size, header->m_sequences.m_offset, count, sizeof(M2Sequence))) {
        return;
    }

    M2Sequence* sequences = reinterpret_cast<M2Sequence*>(base + header->m_sequences.m_offset);
    uint16_t* lookup = nullptr;
    uint32_t lookupCount = header->m_sequenceIdxHashById.m_count;

    if (lookupCount && Fits(size, header->m_sequenceIdxHashById.m_offset, lookupCount, sizeof(uint16_t))) {
        lookup = reinterpret_cast<uint16_t*>(base + header->m_sequenceIdxHashById.m_offset);
    }

    model.m_sourceSequenceIds.resize(count);

    for (uint32_t i = 0; i < count; i++) {
        uint16_t id = sequences[i].m_id;
        uint16_t anim = id;
        model.m_sourceSequenceIds[i] = id;

        // HoT: the mount flight ids 548 to 562 keep their own id since the client plays them natively now.
        switch (id) {
            case 564:
                anim = 37;
                break;
            case 572:
                anim = 39;
                break;
            case 574:
                anim = 187;
                break;
            default:
                break;
        }

        if (anim != id) {
            int32_t oldPos = SequenceIndexById(sequences, count, anim);
            PatchSequenceLookup(lookup, lookupCount, static_cast<uint16_t>(oldPos), anim, static_cast<uint16_t>(i));
            sequences[i].m_id = anim;
        }

        sequences[i].m_blendTime &= 0xFFFF;
    }
}

// HoT: blend mode 7 is kept for the new Gx blend state and the shadow flags 0x40 and 0x80 survive the mask.
static void FixMaterials(uint8_t* base, uint32_t size, M2Data* header) {
    uint32_t count = header->m_materials.m_count;

    if (!count || !Fits(size, header->m_materials.m_offset, count, sizeof(M2Material))) {
        return;
    }

    M2Material* materials = reinterpret_cast<M2Material*>(base + header->m_materials.m_offset);

    for (uint32_t i = 0; i < count; i++) {
        if (materials[i].m_blendMode > MODERN_BLEND_MAX) {
            materials[i].m_blendMode = M2BLEND_ADD;
            materials[i].m_flags |= 5;
        }

        materials[i].m_flags &= MATERIAL_FLAG_MASK;
    }
}

// Packed gravity keys {int8 x, int8 y, int16 magnitude} become the plain downward scalar the client reads.
static bool ExpandGravityKeys(uint8_t* base, uint32_t size, const M2Data* header, uint8_t* record, uint32_t flags) {
    uint8_t* track = record + PARTICLE_GRAVITY_TRACK;
    int16_t loop = static_cast<int16_t>(Read16(track + TRACK_LOOP_INDEX));
    uint32_t slots = Read32(track + TRACK_SEQUENCE_KEYS);
    uint32_t offset = Read32(track + TRACK_SEQUENCE_KEYS + 4);

    if (slots && offset) {
        if (!Fits(size, offset, slots, sizeof(M2Array<float>))) {
            return false;
        }

        const M2Sequence* sequences = nullptr;

        if (loop == -1 && header->m_sequences.m_count && Fits(size, header->m_sequences.m_offset, header->m_sequences.m_count, sizeof(M2Sequence))) {
            sequences = reinterpret_cast<const M2Sequence*>(base + header->m_sequences.m_offset);
        }

        for (uint32_t s = 0; s < slots; s++) {
            if (sequences && s < header->m_sequences.m_count && !(sequences[s].m_flags & SEQUENCE_KEYS_INLINE)) {
                continue;
            }

            const uint8_t* slot = base + offset + s * sizeof(M2Array<float>);
            uint32_t keyCount = Read32(slot);
            uint32_t keyOffset = Read32(slot + 4);

            if (!keyCount || !keyOffset) {
                continue;
            }

            if (!Fits(size, keyOffset, keyCount, 4)) {
                return false;
            }

            for (uint32_t k = 0; k < keyCount; k++) {
                uint8_t* key = base + keyOffset + k * 4;
                float dx = static_cast<int8_t>(key[0]) * (1.0f / 128.0f);
                float dy = static_cast<int8_t>(key[1]) * (1.0f / 128.0f);
                int16_t packed = static_cast<int16_t>(Read16(key + 2));
                float planar = dx * dx + dy * dy;
                float z = sqrtf(planar < 1.0f ? 1.0f - planar : 0.0f);
                float magnitude = packed * 0.04238648f;

                if (magnitude < 0.0f) {
                    z = -z;
                    magnitude = -magnitude;
                }

                float scalar = -(z * magnitude);
                memcpy(key, &scalar, sizeof(scalar));
            }
        }
    }

    Write32(record + PARTICLE_FLAGS, flags & ~PARTICLE_FLAG_COMPRESSED_GRAVITY);
    return true;
}

// The source emitter is the client emitter with the multi-texture scroll fields appended.
static bool NormalizeParticle(uint8_t* base, uint32_t size, const M2Data* header, const uint8_t* src, uint8_t* dst) {
    memmove(dst, src, sizeof(M2Particle));

    uint32_t flags = Read32(dst + PARTICLE_FLAGS);
    uint16_t texture = Read16(dst + PARTICLE_TEXTURE_INDEX);

    // Only the first of the three packed layer ids has a home in a single texture emitter.
    if (flags & PARTICLE_FLAG_MULTI_TEXTURE) {
        texture &= 0x1F;
    }

    if (header->m_textures.m_count && texture >= header->m_textures.m_count) {
        texture = 0;
    }

    Write16(dst + PARTICLE_TEXTURE_INDEX, texture);

    if ((flags & PARTICLE_FLAG_COMPRESSED_GRAVITY) && !ExpandGravityKeys(base, size, header, dst, flags)) {
        return false;
    }

    return true;
}

// The source camera has no field of view float and an animated field of view track after the body.
static float FirstFovKey(const uint8_t* base, uint32_t size, const M2Data* header, const uint8_t* track) {
    const float defaultFov = 0.7853982f;
    int16_t loop = static_cast<int16_t>(Read16(track + 2));
    uint32_t slots = Read32(track + 0x0C);
    uint32_t offset = Read32(track + 0x10);

    if (!slots || !offset || !Fits(size, offset, slots, sizeof(M2Array<float>))) {
        return defaultFov;
    }

    if (loop == -1) {
        if (!header->m_sequences.m_count || !Fits(size, header->m_sequences.m_offset, 1, sizeof(M2Sequence))) {
            return defaultFov;
        }

        const M2Sequence* sequences = reinterpret_cast<const M2Sequence*>(base + header->m_sequences.m_offset);

        if (!(sequences[0].m_flags & SEQUENCE_KEYS_INLINE)) {
            return defaultFov;
        }
    }

    const uint8_t* slot = base + offset;
    uint32_t keyCount = Read32(slot);
    uint32_t keyOffset = Read32(slot + 4);

    if (!keyCount || !keyOffset || !Fits(size, keyOffset, keyCount, 0x0C)) {
        return defaultFov;
    }

    float fov;
    memcpy(&fov, base + keyOffset, sizeof(fov));
    return fov;
}

static bool NormalizeCamera(uint8_t* base, uint32_t size, const M2Data* header, const uint8_t* src, uint8_t* dst) {
    uint32_t type = Read32(src);
    uint32_t farClip = Read32(src + 0x04);
    uint32_t nearClip = Read32(src + 0x08);
    float fov = FirstFovKey(base, size, header, src + 0x60);

    memmove(dst + 0x10, src + 0x0C, 0x54);
    Write32(dst + CAMERA_TYPE, type);
    memcpy(dst + CAMERA_FIELD_OF_VIEW, &fov, sizeof(fov));
    Write32(dst + CAMERA_FAR_CLIP, farClip);
    Write32(dst + CAMERA_NEAR_CLIP, nearClip);
    return true;
}

// Every widened record shrinks, so the rewrite runs front to back in place.
template <class T>
static bool NormalizeArray(uint8_t* base, uint32_t size, const M2Data* header, M2Array<T>& array, uint32_t sourceStride, bool (*rewrite)(uint8_t*, uint32_t, const M2Data*, const uint8_t*, uint8_t*)) {
    if (!array.m_count) {
        return true;
    }

    if (!Fits(size, array.m_offset, array.m_count, sourceStride)) {
        return false;
    }

    const uint8_t* src = base + array.m_offset;
    uint8_t* dst = base + array.m_offset;

    for (uint32_t i = 0; i < array.m_count; i++) {
        if (!rewrite(base, size, header, src, dst)) {
            return false;
        }

        src += sourceStride;
        dst += sizeof(T);
    }

    return true;
}

ModernModel* PrepareModel(M2Data* data, uint32_t& size, const char* fileName) {
    uint8_t* buf = reinterpret_cast<uint8_t*>(data);
    ModernModel* model = new ModernModel();
    model->m_name = fileName ? fileName : "";
    // An editor preview reads its skins the way the model it was made from does.
    std::string source = HoT::Editor::Resources::PreviewSource(fileName);
    model->m_extended = UsesExtendedIndexStart(source.empty() ? model->m_name.c_str() : source.c_str());

    Scan scan;
    const char* fail = nullptr;

    if (!ScanContainer(buf, size, scan, *model)) {
        fail = "no MD20 body in container";
    } else {
        M2Data* body = reinterpret_cast<M2Data*>(buf + scan.m_bodyOffset);
        model->m_version = body->m_version;

        if (body->m_magic != MAGIC_MD20) {
            fail = "body magic";
        } else if (body->m_version < SOURCE_VERSION_MIN || body->m_version > SOURCE_VERSION_MAX) {
            fail = "inner version outside the supported source window";
        } else if ((model->m_skipMask & SKIP_SKID) || (!body->m_bones.m_count && !body->m_sequences.m_count)) {
            // A split skeleton (.skel) model would stand unboned, refusing it reads like an absent file.
            fail = "split skeleton (.skel) model";
        }
    }

    if (!fail) {
        memmove(buf, buf + scan.m_bodyOffset, scan.m_bodySize);
        size = scan.m_bodySize;

        M2Data* header = data;
        header->m_flags &= ~0x60u;
        FixSequences(buf, size, header, *model);
        FixMaterials(buf, size, header);

        // The skin chooser indexes a 4 entry threshold table with 4 - numSkinProfiles, profile 0 is full detail.
        if (header->m_numSkinProfiles > 4) {
            model->m_skipMask |= SKIP_LDV1;
            header->m_numSkinProfiles = 1;
        }

        if (!NormalizeArray(buf, size, header, header->m_particles, PARTICLE_STRIDE_SOURCE, NormalizeParticle) || !NormalizeArray(buf, size, header, header->m_cameras, CAMERA_STRIDE_SOURCE, NormalizeCamera)) {
            fail = "record normalization rejected a record";
        } else {
            header->m_version = CLIENT_VERSION;
        }
    }

    if (fail) {
        LOG_WARN << "m2native: '" << model->m_name << "' native fill failed: " << fail << " (v=" << model->m_version << " skips=0x" << std::hex << model->m_skipMask << ")";
        delete model;
        return nullptr;
    }

    return model;
}

void FinishModel(ModernModel* model, M2Data* data) {
    uint32_t resolved = 0;
    uint32_t unresolved = 0;

    for (uint32_t i = 0; i < data->m_textures.m_count; i++) {
        M2Texture* texture = &data->m_textures[i];

        if (texture->m_type != 0 || (texture->m_filename.m_count >= 2 && texture->m_filename.m_offset)) {
            continue;
        }

        uint32_t fileDataId = i < model->m_textureIds.size() ? model->m_textureIds[i] : 0;

        if (!fileDataId) {
            continue;
        }

        const char* path = Fdid::ResolveTexture(fileDataId);

        if (!path) {
            unresolved++;
            LOG_WARN << "m2native: TXID " << fileDataId << " unresolved (texture " << i << "), solid white fallback";
            continue;
        }

        size_t length = strlen(path) + 1;
        char* name = static_cast<char*>(model->Alloc(texture, length));

        if (!name) {
            continue;
        }

        memcpy(name, path, length);
        texture->m_filename.Set(name);
        texture->m_filename.m_count = static_cast<uint32_t>(length);
        resolved++;
    }

    for (uint32_t i = 0; i < data->m_ribbons.m_count; i++) {
        M2Ribbon* ribbon = &data->m_ribbons[i];

        if (data->m_textures.m_count) {
            for (uint32_t n = 0; n < ribbon->m_textureIndices.m_count; n++) {
                if (ribbon->m_textureIndices[n] >= data->m_textures.m_count) {
                    ribbon->m_textureIndices[n] = static_cast<uint16_t>(data->m_textures.m_count - 1);
                }
            }
        }

        if (data->m_materials.m_count) {
            for (uint32_t n = 0; n < ribbon->m_materialIndices.m_count; n++) {
                if (ribbon->m_materialIndices[n] >= data->m_materials.m_count) {
                    ribbon->m_materialIndices[n] = static_cast<uint16_t>(data->m_materials.m_count - 1);
                }
            }
        }
    }

    for (uint32_t i = 0; i < data->m_cameraIndicesById.m_count; i++) {
        uint16_t& camera = data->m_cameraIndicesById[i];

        if (camera != 0xFFFF && camera >= data->m_cameras.m_count) {
            camera = 0xFFFF;
        }
    }

    LOG_INFO << "m2native: '" << model->m_name << "' read natively (v=" << model->m_version << " tex=" << resolved << "/" << (resolved + unresolved) << " skips=0x" << std::hex << model->m_skipMask << ")";
}

void Destroy(ModernModel* model) {
    delete model;
}

const char* SkinFilePath(const ModernModel* model, uint32_t profile) {
    if (!model || profile >= model->m_skinIds.size()) {
        return nullptr;
    }

    return Fdid::ResolveModel(model->m_skinIds[profile]);
}

const char* AnimFilePath(ModernModel* model, const char* modelFileName, uint32_t sequenceIndex, uint32_t id, uint32_t variation) {
    if (!model || sequenceIndex >= model->m_sourceSequenceIds.size()) {
        return nullptr;
    }

    uint16_t sourceId = model->m_sourceSequenceIds[sequenceIndex];

    for (const AnimFile& anim : model->m_animIds) {
        if (anim.m_id == sourceId && anim.m_variation == variation && anim.m_fileDataId) {
            const char* path = Fdid::ResolveModel(anim.m_fileDataId);

            if (path) {
                return path;
            }
        }
    }

    if (sourceId == id) {
        return nullptr;
    }

    std::string name = modelFileName;
    size_t dot = name.find_last_of('.');

    if (dot != std::string::npos) {
        name.resize(dot);
    }

    char suffix[32];
    snprintf(suffix, sizeof(suffix), "%04d-%02d.anim", sourceId, variation);
    model->m_path = name + suffix;
    return model->m_path.c_str();
}

uint32_t UnwrapAnim(void* buffer, uint32_t size) {
    uint8_t* bytes = static_cast<uint8_t*>(buffer);

    if (!bytes || size < 8 || Read32(bytes) != MAGIC_AFM2) {
        return size;
    }

    uint32_t payload = Read32(bytes + 4);

    if (payload > size - 8) {
        return size;
    }

    memmove(bytes, bytes + 8, payload);
    return payload;
}

struct SplitRun {
    uint32_t m_first;
    uint32_t m_count;
};

struct SkinBuild {
    std::vector<uint32_t> m_lookup;
    std::vector<uint32_t> m_bones;
    std::vector<uint16_t> m_indices;
    std::vector<M2SkinSection> m_sections;
    std::vector<uint8_t> m_bad;
    std::vector<SplitRun> m_splitMap;
    std::vector<uint16_t> m_boneCombos;
    bool m_split = false;
};

static uint16_t LookupSingle(std::vector<uint16_t>& lookup, uint16_t value) {
    for (size_t n = 0; n < lookup.size(); n++) {
        if (lookup[n] == value) {
            return static_cast<uint16_t>(n);
        }
    }

    lookup.push_back(value);
    return static_cast<uint16_t>(lookup.size() - 1);
}

static uint16_t LookupPair(std::vector<uint16_t>& lookup, uint16_t a, uint16_t b) {
    for (size_t n = 0; n + 1 < lookup.size(); n++) {
        if (lookup[n] == a && lookup[n + 1] == b) {
            return static_cast<uint16_t>(n);
        }
    }

    size_t count = lookup.size();

    if (count > 1 && lookup[count - 1] == a) {
        lookup.push_back(b);
        return static_cast<uint16_t>(count - 1);
    }

    lookup.push_back(a);
    lookup.push_back(b);
    return static_cast<uint16_t>(count);
}

static uint16_t NextLookup(uint16_t base, uint32_t count) {
    uint32_t next = static_cast<uint32_t>(base) + 1;
    return next < count ? static_cast<uint16_t>(next) : base;
}

// Splits every drawn section whose bone palette exceeds maxBones with a greedy triangle bin packer.
static bool SplitSections(SkinBuild& build, const M2Data* data, uint32_t maxBones, const char* name) {
    bool needed = false;

    for (const M2SkinSection& section : build.m_sections) {
        if (section.m_indexCount && section.m_boneCount > maxBones) {
            needed = true;
            break;
        }
    }

    if (!needed) {
        return true;
    }

    const uint16_t* boneCombos = data->m_boneCombos.m_count ? data->m_boneCombos.Data() : nullptr;
    uint32_t boneComboCount = data->m_boneCombos.m_count;

    if (!boneCombos || boneComboCount > 0x10000) {
        LOG_WARN << "modern-assets: '" << name << "' boneCombos count " << boneComboCount << " out of range, skipping bone split";
        return true;
    }

    uint32_t slotCount = static_cast<uint32_t>(build.m_lookup.size());
    bool wide = slotCount > 0xFFFF;
    std::vector<uint32_t> lookup;
    std::vector<uint32_t> bones;
    std::vector<uint16_t> indices;
    std::vector<M2SkinSection> sections;
    std::vector<uint32_t> origin;
    std::vector<uint16_t> combos(boneCombos, boneCombos + boneComboCount);
    lookup.reserve(slotCount);
    bones.reserve(slotCount);
    indices.reserve(build.m_indices.size());

    auto slotOf = [&](const M2SkinSection& section, uint32_t index) -> uint32_t {
        uint16_t value = build.m_indices[index];

        if (!wide) {
            return value;
        }

        return section.m_vertexStart + static_cast<uint16_t>(value - static_cast<uint16_t>(section.m_vertexStart));
    };

    auto boneOf = [&](uint32_t comboBase, const uint8_t* influences, uint32_t j) -> uint16_t {
        uint32_t comboIndex = comboBase + influences[j];
        return comboIndex < boneComboCount ? boneCombos[comboIndex] : 0;
    };

    for (uint32_t si = 0; si < build.m_sections.size(); si++) {
        const M2SkinSection& s = build.m_sections[si];

        if (!s.m_indexCount) {
            M2SkinSection placeholder = s;
            placeholder.m_vertexStart = 0;
            placeholder.m_vertexCount = 0;
            placeholder.m_indexStart = 0;
            placeholder.m_indexCount = 0;
            placeholder.m_boneCount = 1;
            sections.push_back(placeholder);
            origin.push_back(si);
            continue;
        }

        uint32_t triCount = s.m_indexCount / 3;
        uint32_t emitted = 0;
        uint32_t curTriStart = 0;
        std::vector<uint16_t> curGlobals;

        auto emit = [&](uint32_t triFrom, uint32_t triTo, std::vector<uint16_t>& globals) -> bool {
            if (triFrom >= triTo) {
                return true;
            }

            std::sort(globals.begin(), globals.end());
            uint32_t comboIndex = static_cast<uint32_t>(combos.size());

            if (comboIndex > 0xFFFF) {
                return false;
            }

            combos.insert(combos.end(), globals.begin(), globals.end());

            uint32_t vertexStart = static_cast<uint32_t>(lookup.size());
            uint32_t indexStart = static_cast<uint32_t>(indices.size());
            std::unordered_map<uint32_t, uint32_t> remap;

            for (uint32_t t = triFrom; t < triTo; t++) {
                for (uint32_t k = 0; k < 3; k++) {
                    uint32_t slot = slotOf(s, s.m_indexStart + t * 3 + k);

                    if (slot >= slotCount) {
                        return false;
                    }

                    auto it = remap.find(slot);
                    uint32_t newSlot;

                    if (it == remap.end()) {
                        newSlot = static_cast<uint32_t>(lookup.size());
                        remap.emplace(slot, newSlot);
                        lookup.push_back(build.m_lookup[slot]);

                        const uint8_t* influences = reinterpret_cast<const uint8_t*>(&build.m_bones[slot]);
                        uint8_t local[4];

                        for (uint32_t j = 0; j < 4; j++) {
                            uint32_t comboSlot = s.m_boneComboIndex + influences[j];
                            uint16_t bone = comboSlot < boneComboCount ? boneCombos[comboSlot] : globals[0];
                            auto found = std::lower_bound(globals.begin(), globals.end(), bone);
                            local[j] = found != globals.end() && *found == bone ? static_cast<uint8_t>(found - globals.begin()) : 0;
                        }

                        uint32_t packed;
                        memcpy(&packed, local, sizeof(packed));
                        bones.push_back(packed);
                    } else {
                        newSlot = it->second;
                    }

                    indices.push_back(static_cast<uint16_t>(newSlot));
                }
            }

            uint32_t vertexCount = static_cast<uint32_t>(lookup.size()) - vertexStart;

            if (vertexCount > 0xFFFF) {
                return false;
            }

            M2SkinSection section = s;
            section.m_vertexStart = vertexStart;
            section.m_vertexCount = vertexCount;
            section.m_indexStart = indexStart;
            section.m_indexCount = static_cast<uint32_t>(indices.size()) - indexStart;
            section.m_boneCount = static_cast<uint16_t>(globals.size());
            section.m_boneComboIndex = static_cast<uint16_t>(comboIndex);
            sections.push_back(section);
            origin.push_back(si);
            emitted++;

            return true;
        };

        for (uint32_t t = 0; t < triCount; t++) {
            uint16_t g[12];
            uint32_t gn = 0;

            for (uint32_t k = 0; k < 3; k++) {
                uint32_t slot = slotOf(s, s.m_indexStart + t * 3 + k);

                if (slot >= slotCount) {
                    LOG_WARN << "modern-assets: '" << name << "' section " << si << " names a vertex past the skin, skipping bone split";
                    return true;
                }

                const uint8_t* influences = reinterpret_cast<const uint8_t*>(&build.m_bones[slot]);

                for (uint32_t j = 0; j < 4; j++) {
                    uint16_t bone = boneOf(s.m_boneComboIndex, influences, j);

                    if (std::find(g, g + gn, bone) == g + gn && gn < 12) {
                        g[gn++] = bone;
                    }
                }
            }

            size_t unionSize = curGlobals.size();

            for (uint32_t e = 0; e < gn; e++) {
                if (std::find(curGlobals.begin(), curGlobals.end(), g[e]) == curGlobals.end()) {
                    unionSize++;
                }
            }

            if (unionSize > maxBones && t > curTriStart) {
                if (!emit(curTriStart, t, curGlobals)) {
                    LOG_WARN << "modern-assets: '" << name << "' bone split overflowed, keeping the clamp";
                    return true;
                }

                curGlobals.clear();
                curTriStart = t;
            }

            for (uint32_t e = 0; e < gn; e++) {
                if (std::find(curGlobals.begin(), curGlobals.end(), g[e]) == curGlobals.end()) {
                    curGlobals.push_back(g[e]);
                }
            }
        }

        if (!emit(curTriStart, triCount, curGlobals)) {
            LOG_WARN << "modern-assets: '" << name << "' bone split overflowed, keeping the clamp";
            return true;
        }

        if (!emitted) {
            M2SkinSection section = s;
            section.m_vertexCount = 0;
            section.m_indexCount = 0;
            section.m_boneCount = 1;
            sections.push_back(section);
            origin.push_back(si);
        }
    }

    if (sections.size() > MAX_BATCHES) {
        return true;
    }

    build.m_splitMap.assign(build.m_sections.size(), SplitRun { 0, 0 });

    for (uint32_t i = 0; i < sections.size(); i++) {
        SplitRun& run = build.m_splitMap[origin[i]];

        if (!run.m_count) {
            run.m_first = i;
        }

        run.m_count++;
    }

    std::vector<uint8_t> bad(sections.size(), 0);

    for (uint32_t i = 0; i < sections.size(); i++) {
        bad[i] = !sections[i].m_indexCount || build.m_bad[origin[i]];
    }

    build.m_lookup.swap(lookup);
    build.m_bones.swap(bones);
    build.m_indices.swap(indices);
    build.m_sections.swap(sections);
    build.m_bad.swap(bad);
    build.m_boneCombos.swap(combos);
    build.m_split = true;

    return true;
}

// The final packed shader the stock SubstituteSimpleShaders would compute, given the blend pair and the coordinate lookup.
static uint16_t FinalShader(const M2Batch& batch, const uint16_t* pair, const std::vector<uint16_t>& coords, uint16_t blendMode) {
    if (batch.m_shader & 0x8000) {
        return batch.m_shader;
    }

    uint32_t coordIndex = batch.m_textureCoordComboIndex;

    if (!pair) {
        uint16_t coord = coordIndex < coords.size() ? coords[coordIndex] : 0;
        uint16_t op = blendMode != 0;

        if (coord > 2) {
            op |= 8;
        }

        uint16_t shader = static_cast<uint16_t>(op << 4);

        if (coord == 1) {
            shader |= 0x4000;
        }

        return shader;
    }

    uint32_t ops[2] = { 0, 0 };
    uint16_t shader = 0;

    for (uint32_t j = 0; j < batch.m_textureCount && j < 2; j++) {
        uint32_t op = pair[j];

        if (j == 0 && !blendMode) {
            op = 0;
        }

        uint16_t coord = coordIndex + j < coords.size() ? coords[coordIndex + j] : 0;
        ops[j] = coord > 2 ? op | 8 : op;

        if (coord == 1 && j + 1 == batch.m_textureCount) {
            shader |= 0x4000;
        }
    }

    return static_cast<uint16_t>(shader | ops[1] | (ops[0] << 4));
}

struct BatchPiece {
    M2Batch m_batch;
    uint16_t m_pair[2];
    bool m_hasPair;
};

static uint16_t MaterialBlend(const M2Data* data, uint16_t materialIndex) {
    return materialIndex < data->m_materials.m_count ? data->m_materials[materialIndex].m_blendMode : 0;
}

// Splits one source env batch into a primary plus a follower second pass over the same geometry.
static bool EnvSplit(uint16_t low, uint16_t shaderId, M2Batch primary, uint32_t weightCount, uint32_t transformCount, std::vector<BatchPiece>& out, std::vector<uint16_t>& coords) {
    M2Batch follower = primary;
    follower.m_materialIndex = static_cast<uint16_t>(primary.m_materialIndex + 1);
    follower.m_materialLayer = 1;
    follower.m_textureCount = 1;

    switch (low) {
        case 0:
        case 3:
        case 9:
        case 17:
        case 24: {
            primary.m_textureCount = 2;
            uint16_t coord = LookupPair(coords, 0, 0xFFFF);
            primary.m_textureCoordComboIndex = coord;
            follower.m_textureCoordComboIndex = coord;
            out.push_back({ primary, { 1, 4 }, true });
            out.push_back({ follower, { 1, 4 }, false });
            return true;
        }
        case 1:
        case 15: {
            primary.m_textureCount = 1;
            follower.m_textureComboIndex = static_cast<uint16_t>(primary.m_textureComboIndex + 1);

            if (static_cast<uint32_t>(primary.m_textureWeightComboIndex) + 1 < weightCount) {
                follower.m_textureWeightComboIndex = static_cast<uint16_t>(primary.m_textureWeightComboIndex + 1);
            }

            follower.m_textureTransformComboIndex = NextLookup(primary.m_textureTransformComboIndex, transformCount);
            uint16_t coord = LookupPair(coords, 0, shaderId == 0x8001 ? 0xFFFF : 1);
            primary.m_textureCoordComboIndex = coord;
            follower.m_textureCoordComboIndex = static_cast<uint16_t>(coord + 1);
            primary.m_shader = 0;
            follower.m_shader = 0;
            out.push_back({ primary, { 0, 0 }, false });
            out.push_back({ follower, { 0, 0 }, false });
            return true;
        }
        case 2: {
            uint16_t coord = LookupPair(coords, 0, 0xFFFF);
            primary.m_textureCoordComboIndex = coord;
            follower.m_textureCoordComboIndex = coord;
            out.push_back({ primary, { 1, 3 }, primary.m_textureCount > 1 });
            out.push_back({ follower, { 1, 3 }, false });
            return true;
        }
        default:
            return false;
    }
}

// Decodes one modern batch shader id into the batch fields and coordinate lookup the client draws with.
static void DecodeBatch(M2Batch b, uint32_t weightCount, uint32_t transformCount, std::vector<BatchPiece>& out, std::vector<uint16_t>& coords) {
    uint16_t shaderId = b.m_shader;
    uint16_t textureCount = b.m_textureCount;
    b.m_flags &= 0x10;

    if (shaderId >= 0x8000) {
        uint16_t sourceIndex = shaderId & 0x7FFF;

        // Source effects 0 to 2 are the Diffuse_T1_Env effects GetEffect knows as 1 to 3.
        if (sourceIndex <= 2) {
            b.m_shader = static_cast<uint16_t>(0x8000 | (sourceIndex + 1));
            b.m_textureCount = 2;
            b.m_textureCoordComboIndex = LookupPair(coords, 0, 0xFFFF);
            out.push_back({ b, { 0, 0 }, false });
            return;
        }

        uint16_t low = shaderId & 0xFF;

        if (EnvSplit(low, shaderId, b, weightCount, transformCount, out, coords)) {
            return;
        }

        switch (low) {
            case 5:
            case 8:
            case 10:
            case 12:
            case 16:
            case 23:
                shaderId = 0;
                textureCount = 1;
                break;
            case 21:
            case 33:
            case 35:
                // The DLL swapped in a Combiners_Mod_AddAlpha shader here, this is its fallback blend.
                shaderId = 0x4011;
                textureCount = 2;
                break;
            default:
                shaderId = 0x0010;
                textureCount = 1;
                break;
        }
    }

    uint16_t blend1 = (shaderId >> 4) & 7;
    uint16_t blend2 = shaderId & 7;
    bool twoTex = textureCount > 1 && (shaderId & 0x4000) && blend1 && blend2;

    if (!twoTex) {
        textureCount = 1;
    }

    b.m_shader = 0;

    if (textureCount == 1) {
        b.m_textureCoordComboIndex = LookupSingle(coords, (shaderId & 0x80) ? 0xFFFF : 0);
    } else {
        uint16_t t0;
        uint16_t t1;

        if (shaderId & 0x80) {
            t0 = 0xFFFF;
            t1 = (shaderId & 0x8) ? 0xFFFF : 0;
        } else {
            t0 = 0;
            t1 = (shaderId & 0x8) ? 0xFFFF : ((shaderId & 0x4000) ? 1 : 0);
        }

        b.m_textureCoordComboIndex = LookupPair(coords, t0, t1);
    }

    b.m_textureCount = textureCount;
    out.push_back({ b, { blend1, blend2 }, twoTex });
}

template <class T>
static uint8_t* PlaceArray(uint8_t* cursor, M2Array<T>& array, const T* source, uint32_t count) {
    array.m_count = count;
    array.Set(count ? reinterpret_cast<T*>(cursor) : nullptr);

    if (count) {
        memcpy(cursor, source, sizeof(T) * count);
    }

    return cursor + ((sizeof(T) * count + 15) & ~static_cast<size_t>(15));
}

M2SkinProfile* BuildSkin(ModernModel* model, M2Data* data, const M2SkinProfile* raw, uint32_t size, uint32_t maxBones) {
    const uint8_t* base = reinterpret_cast<const uint8_t*>(raw);
    const char* name = model->m_name.c_str();

    if (size < sizeof(M2SkinProfile) || raw->m_magic != MAGIC_SKIN) {
        LOG_WARN << "modern-assets: '" << name << "' skin magic";
        return nullptr;
    }

    if (!Fits(size, raw->m_vertices.m_offset, raw->m_vertices.m_count, sizeof(uint16_t)) || !Fits(size, raw->m_indices.m_offset, raw->m_indices.m_count, sizeof(uint16_t)) || !Fits(size, raw->m_bones.m_offset, raw->m_bones.m_count, sizeof(ubyte4)) || !Fits(size, raw->m_skinSections.m_offset, raw->m_skinSections.m_count, sizeof(M2SkinSectionFile)) || !Fits(size, raw->m_batches.m_offset, raw->m_batches.m_count, sizeof(M2Batch))) {
        LOG_WARN << "modern-assets: '" << name << "' skin array outside the file";
        return nullptr;
    }

    uint32_t slotCount = raw->m_vertices.m_count;
    uint32_t indexCount = raw->m_indices.m_count;
    bool wide = slotCount > 0xFFFF;

    if (raw->m_bones.m_count < slotCount) {
        LOG_WARN << "modern-assets: '" << name << "' skin has fewer bone entries than vertices";
        return nullptr;
    }

    SkinBuild build;
    build.m_lookup.resize(slotCount);
    build.m_bones.resize(slotCount);
    build.m_indices.resize(indexCount);

    const uint16_t* rawLookup = reinterpret_cast<const uint16_t*>(base + raw->m_vertices.m_offset);

    for (uint32_t i = 0; i < slotCount; i++) {
        // Past 65535 slots the 16-bit lookup cannot name a vertex, such models keep their vertices in slot order.
        build.m_lookup[i] = wide ? i : rawLookup[i];
    }

    if (slotCount) {
        memcpy(build.m_bones.data(), base + raw->m_bones.m_offset, sizeof(uint32_t) * slotCount);
    }

    if (indexCount) {
        memcpy(build.m_indices.data(), base + raw->m_indices.m_offset, sizeof(uint16_t) * indexCount);
    }

    const M2SkinSectionFile* files = reinterpret_cast<const M2SkinSectionFile*>(base + raw->m_skinSections.m_offset);
    uint32_t sectionCount = raw->m_skinSections.m_count;
    build.m_sections.resize(sectionCount);
    build.m_bad.assign(sectionCount, 0);

    uint32_t running = 0;
    bool runningValid = true;

    for (uint32_t i = 0; i < sectionCount; i++) {
        const M2SkinSectionFile& file = files[i];
        M2SkinSection& s = build.m_sections[i];
        s.m_skinSectionId = model->m_extended ? file.m_skinSectionId : (file.m_skinSectionId | (static_cast<uint32_t>(file.m_level) << 16));
        s.m_vertexStart = file.m_vertexStart;
        s.m_vertexCount = file.m_vertexCount;
        s.m_indexStart = file.m_indexStart;
        s.m_indexCount = file.m_indexCount;
        s.m_boneCount = file.m_boneCount;
        s.m_boneComboIndex = file.m_boneComboIndex;
        // The vertex transform table has entries for 0 to 4 influences.
        s.m_boneInfluences = (std::min)(static_cast<uint16_t>(file.m_boneInfluences & 0xFF), static_cast<uint16_t>(4));
        s.m_centerBoneIndex = file.m_centerBoneIndex;
        s.m_centerPosition = file.m_centerPosition;
        s.m_sortCenterPosition = file.m_sortCenterPosition;
        s.m_sortRadius = file.m_sortRadius;

        // Past 65535 slots the first vertex is a running total, valid while it agrees with the 16-bit field.
        if (wide) {
            if (runningValid && running <= 0xFFFF && static_cast<uint16_t>(running) != file.m_vertexStart) {
                runningValid = false;
            }

            if (runningValid) {
                s.m_vertexStart = running;
            }

            running += file.m_vertexCount;

            if (running > slotCount) {
                runningValid = false;
            }
        }

        bool park = file.m_level > 0 && !model->m_extended;

        if (!park && file.m_indexCount) {
            uint32_t start = model->m_extended ? ((static_cast<uint32_t>(file.m_level) << 16) | file.m_indexStart) : file.m_indexStart;

            if (start > indexCount || file.m_indexCount > indexCount - start) {
                LOG_WARN << "modern-assets: submesh " << i << " index window past skin indexCount, parking";
                park = true;
            } else {
                s.m_indexStart = start;
            }
        }

        if (park) {
            s.m_vertexStart = 0;
            s.m_vertexCount = 0;
            s.m_indexStart = 0;
            s.m_indexCount = 0;
            s.m_boneComboIndex = 0;
            s.m_centerBoneIndex = 0;
        }

        if (!s.m_indexCount) {
            build.m_bad[i] = 1;
        }
    }

    if (!SplitSections(build, data, maxBones, name)) {
        return nullptr;
    }

    uint32_t comboCount = build.m_split ? static_cast<uint32_t>(build.m_boneCombos.size()) : data->m_boneCombos.m_count;

    for (M2SkinSection& s : build.m_sections) {
        if (!s.m_indexCount) {
            if (s.m_boneCount < 1) {
                s.m_boneCount = 1;
            }

            continue;
        }

        uint32_t cap = maxBones;
        uint32_t byCombo = comboCount > s.m_boneComboIndex ? comboCount - s.m_boneComboIndex : 1;
        cap = (std::max)((std::min)(cap, byCombo), 1u);

        if (s.m_boneCount > cap) {
            s.m_boneCount = static_cast<uint16_t>(cap);
        }

        if (s.m_boneCount < 1) {
            s.m_boneCount = 1;
        }

        if (!s.m_boneInfluences) {
            s.m_boneInfluences = 1;
        }
    }

    // Texture transform lookup the source omits when it has transforms.
    uint32_t batchCount = (std::min)(raw->m_batches.m_count, MAX_BATCHES);
    const M2Batch* sourceBatches = reinterpret_cast<const M2Batch*>(base + raw->m_batches.m_offset);

    if (raw->m_batches.m_count > MAX_BATCHES) {
        LOG_WARN << "modern-m2: '" << name << "' skin batchCount " << raw->m_batches.m_count << " exceeds cap, clamping";
    }

    if (data->m_textureTransforms.m_count && !data->m_textureTransformCombos.m_count) {
        uint32_t needed = data->m_textureTransforms.m_count;

        for (uint32_t i = 0; i < batchCount; i++) {
            uint32_t first = sourceBatches[i].m_textureTransformComboIndex;
            uint32_t count = sourceBatches[i].m_textureCount ? sourceBatches[i].m_textureCount : 1;

            if (first != 0xFFFF && first <= 0xFFFF - count && needed < first + count) {
                needed = first + count;
            }
        }

        uint16_t* transforms = static_cast<uint16_t*>(model->Alloc(&data->m_textureTransformCombos, sizeof(uint16_t) * needed));

        if (transforms) {
            for (uint32_t i = 0; i < needed; i++) {
                transforms[i] = i < data->m_textureTransforms.m_count ? static_cast<uint16_t>(i) : 0xFFFF;
            }

            data->m_textureTransformCombos.Set(transforms);
            data->m_textureTransformCombos.m_count = needed;
        }
    }

    uint32_t weightCount = data->m_textureWeightCombos.m_count;
    uint32_t transformCount = data->m_textureTransformCombos.m_count;
    std::vector<uint16_t> coords;
    std::vector<M2Batch> batches;
    batches.reserve(batchCount);

    for (uint32_t i = 0; i < batchCount; i++) {
        M2Batch source = sourceBatches[i];
        SplitRun run { source.m_skinSectionIndex, 1 };

        if (build.m_split && source.m_skinSectionIndex < build.m_splitMap.size()) {
            run = build.m_splitMap[source.m_skinSectionIndex];
        }

        std::vector<BatchPiece> pieces;
        DecodeBatch(source, weightCount, transformCount, pieces, coords);

        for (BatchPiece& piece : pieces) {
            piece.m_batch.m_shader = FinalShader(piece.m_batch, piece.m_hasPair ? piece.m_pair : nullptr, coords, MaterialBlend(data, piece.m_batch.m_materialIndex));
        }

        for (uint32_t s = 0; s < run.m_count; s++) {
            uint32_t sectionIndex = run.m_first + s;

            // A batch naming a section the skin does not have cannot be drawn or culled.
            if (sectionIndex >= build.m_sections.size()) {
                continue;
            }

            bool bad = build.m_bad[sectionIndex] != 0;

            for (const BatchPiece& piece : pieces) {
                M2Batch batch = piece.m_batch;
                batch.m_skinSectionIndex = static_cast<uint16_t>(sectionIndex);

                if (batch.m_materialIndex >= data->m_materials.m_count) {
                    batch.m_materialIndex = 0;
                    bad = true;
                }

                if (bad || !data->m_materials.m_count) {
                    batch.m_shader = 0x8000;
                }

                batches.push_back(batch);
            }
        }
    }

    if (batches.size() > MAX_BATCHES) {
        batches.resize(MAX_BATCHES);
    }

    for (uint32_t i = 0; i < batches.size(); i++) {
        if (batches[i].m_materialLayer > i) {
            batches[i].m_materialLayer = 0;
        }
    }

    if (!coords.empty()) {
        uint16_t* lookup = static_cast<uint16_t*>(model->Alloc(&data->m_textureCoordCombos, sizeof(uint16_t) * coords.size()));

        if (!lookup) {
            return nullptr;
        }

        memcpy(lookup, coords.data(), sizeof(uint16_t) * coords.size());
        data->m_textureCoordCombos.Set(lookup);
        data->m_textureCoordCombos.m_count = static_cast<uint32_t>(coords.size());
    }

    if (build.m_split) {
        uint16_t* combos = static_cast<uint16_t*>(model->Alloc(&data->m_boneCombos, sizeof(uint16_t) * build.m_boneCombos.size()));

        if (!combos) {
            return nullptr;
        }

        memcpy(combos, build.m_boneCombos.data(), sizeof(uint16_t) * build.m_boneCombos.size());
        data->m_boneCombos.Set(combos);
        data->m_boneCombos.m_count = static_cast<uint32_t>(build.m_boneCombos.size());
    }

    // The lookup must name every vertex with VertexAt, otherwise the model vertices are laid out in slot order.
    uint32_t newSlots = static_cast<uint32_t>(build.m_lookup.size());
    bool newWide = newSlots > 0xFFFF;
    bool materialize = false;

    for (uint32_t i = 0; i < newSlots && !materialize; i++) {
        uint32_t source = build.m_lookup[i];
        materialize = newWide ? source != i : source > 0xFFFF;
    }

    if (materialize) {
        uint32_t modelVertices = data->m_vertices.m_count;
        M2Vertex* vertices = static_cast<M2Vertex*>(model->Alloc(&data->m_vertices, sizeof(M2Vertex) * newSlots));

        if (!vertices) {
            return nullptr;
        }

        for (uint32_t i = 0; i < newSlots; i++) {
            uint32_t source = build.m_lookup[i];

            if (source >= modelVertices) {
                LOG_WARN << "modern-assets: '" << name << "' skin names vertex " << source << " past the model";
                return nullptr;
            }

            vertices[i] = data->m_vertices[source];
            build.m_lookup[i] = i;
        }

        data->m_vertices.Set(vertices);
        data->m_vertices.m_count = newSlots;
    } else if (newWide && data->m_vertices.m_count < newSlots) {
        LOG_WARN << "modern-assets: '" << name << "' wide skin is larger than the model vertex array";
        return nullptr;
    }

    std::vector<uint16_t> lookup16(newSlots);

    for (uint32_t i = 0; i < newSlots; i++) {
        lookup16[i] = static_cast<uint16_t>(build.m_lookup[i]);
    }

    size_t total = (sizeof(M2SkinProfile) + 15) & ~static_cast<size_t>(15);
    total += (sizeof(uint16_t) * newSlots + 15) & ~static_cast<size_t>(15);
    total += (sizeof(uint16_t) * build.m_indices.size() + 15) & ~static_cast<size_t>(15);
    total += (sizeof(uint32_t) * build.m_bones.size() + 15) & ~static_cast<size_t>(15);
    total += (sizeof(M2SkinSection) * build.m_sections.size() + 15) & ~static_cast<size_t>(15);
    total += (sizeof(M2Batch) * batches.size() + 15) & ~static_cast<size_t>(15);

    if (total > 0x7FFFFFFF) {
        return nullptr;
    }

    uint8_t* memory = static_cast<uint8_t*>(SIMD::AllocA16(static_cast<uint32_t>(total), __FILE__, __LINE__));

    if (!memory) {
        return nullptr;
    }

    M2SkinProfile* skin = reinterpret_cast<M2SkinProfile*>(memory);
    memset(skin, 0, sizeof(M2SkinProfile));
    skin->m_magic = MAGIC_SKIN;
    skin->m_boneCountMax = maxBones;

    uint8_t* cursor = memory + ((sizeof(M2SkinProfile) + 15) & ~static_cast<size_t>(15));
    cursor = PlaceArray(cursor, skin->m_vertices, lookup16.data(), newSlots);
    cursor = PlaceArray(cursor, skin->m_indices, build.m_indices.data(), static_cast<uint32_t>(build.m_indices.size()));
    cursor = PlaceArray(cursor, skin->m_bones, reinterpret_cast<const ubyte4*>(build.m_bones.data()), static_cast<uint32_t>(build.m_bones.size()));
    cursor = PlaceArray(cursor, skin->m_skinSections, build.m_sections.data(), static_cast<uint32_t>(build.m_sections.size()));
    PlaceArray(cursor, skin->m_batches, batches.data(), static_cast<uint32_t>(batches.size()));

    if (build.m_split) {
        LOG_INFO << "modern-assets: '" << name << "' split sections over " << maxBones << " bones into " << build.m_sections.size() << " sections";
    }

    return skin;
}

class ModernM2Module : public Module {
    public:
    const char* Name() const override {
        return "ModernM2";
    }

    void OnInitializeGlobal() override {
        Fdid::BeginWarm();
    }
};

Module* GetModule() {
    static ModernM2Module module;
    return &module;
}

}
