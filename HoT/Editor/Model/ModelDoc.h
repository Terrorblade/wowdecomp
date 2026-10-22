#ifndef HOT_EDITOR_MODEL_MODELDOC_H
#define HOT_EDITOR_MODEL_MODELDOC_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "Engine/Source/Model2/M2Data.h"

// The raw bytes of one .m2 file, edited by patching records in place and appending new array data.
namespace HoT::Editor::Model {

enum class ValueType : uint8_t {
    Float,
    Vec2,
    Vec3,
    Fixed16,
    UInt8,
    UInt16,
    // Modern compressed gravity, a 4 byte key decoded to the { x, y, z } vector, the client uses -z.
    PackedGravity,
};

uint32_t ValueComponents(ValueType type);
uint32_t ValueSize(ValueType type);

struct TrackKeys {
    uint32_t m_sequence = 0;
    std::vector<uint32_t> m_times;
    std::vector<float> m_values;
};

struct Track {
    uint16_t m_interpolation = 0;
    int32_t m_globalSequence = -1;
    std::vector<TrackKeys> m_sequences;
};

struct PartTrack {
    std::vector<float> m_times;
    std::vector<float> m_values;
};

struct Chunk {
    uint32_t m_tag = 0;
    std::vector<uint8_t> m_data;
};

// Byte offsets of the header arrays inside the MD20 body.
enum HeaderField : uint32_t {
    HEADER_NAME = 0x08,
    HEADER_FLAGS = 0x10,
    HEADER_LOOPS = 0x14,
    HEADER_SEQUENCES = 0x1C,
    HEADER_BONES = 0x2C,
    HEADER_VERTICES = 0x3C,
    HEADER_SKIN_PROFILES = 0x44,
    HEADER_COLORS = 0x48,
    HEADER_TEXTURES = 0x50,
    HEADER_MATERIALS = 0x70,
    HEADER_TEXTURE_COMBOS = 0x80,
    HEADER_ATTACHMENTS = 0xF0,
    HEADER_ATTACHMENT_LOOKUP = 0xF8,
    HEADER_RIBBONS = 0x120,
    HEADER_PARTICLES = 0x128,
};

static const uint32_t SEQUENCE_STRIDE = 0x40;
static const uint32_t BONE_STRIDE = 0x58;
static const uint32_t TEXTURE_STRIDE = 0x10;
static const uint32_t MATERIAL_STRIDE = 0x04;
static const uint32_t ATTACHMENT_STRIDE = 0x28;
static const uint32_t RIBBON_STRIDE = 0xB0;
static const uint32_t PARTICLE_STRIDE = 0x1DC;
static const uint32_t PARTICLE_STRIDE_MODERN = 0x1EC;

static const uint32_t SEQUENCE_FLAG_INLINE = 0x20;
static const uint32_t SEQUENCE_FLAG_ALIAS = 0x40;
static const uint32_t PARTICLE_FLAG_PACKED_GRAVITY = 0x800000;
static const uint32_t PARTICLE_FLAG_MULTI_TEXTURE = 0x10000000;

class ModelDoc {
    public:
    // path is a normalized game path ending in .m2.
    bool Load(const std::string& path, std::string& error);

    // The file bytes, the MD21 container is rebuilt around the grown body for modern models.
    std::vector<uint8_t> Build() const;
    uint64_t BuildSize() const;

    const std::string& Path() const {
        return this->m_path;
    }

    void SetPath(const std::string& path) {
        this->m_path = path;
    }

    bool IsModern() const {
        return this->m_modern;
    }

    uint32_t Version() const;
    uint32_t BodySize() const {
        return static_cast<uint32_t>(this->m_body.size());
    }

    uint32_t Revision() const {
        return this->m_revision;
    }

    bool Dirty() const {
        return this->m_dirty;
    }

    void MarkChanged() {
        this->m_revision++;
        this->m_dirty = true;
    }

    void MarkSaved() {
        this->m_dirty = false;
    }

    // Bounded access to the MD20 body, positions are absolute from the MD20 start.
    bool Read(uint32_t pos, void* out, uint32_t size) const;
    bool Write(uint32_t pos, const void* data, uint32_t size);

    template <class T>
    bool Get(uint32_t pos, T& value) const {
        return this->Read(pos, &value, sizeof(T));
    }

    template <class T>
    bool Put(uint32_t pos, const T& value) {
        return this->Write(pos, &value, sizeof(T));
    }

    bool ArrayAt(uint32_t pos, uint32_t& count, uint32_t& offset) const;
    bool Fits(uint32_t offset, uint32_t count, uint32_t stride) const;
    uint32_t Count(uint32_t field) const;
    uint32_t RecordPos(uint32_t field, uint32_t stride, uint32_t index) const;
    uint32_t ParticleStride() const {
        return this->m_modern ? PARTICLE_STRIDE_MODERN : PARTICLE_STRIDE;
    }

    uint32_t SequenceFlags(uint32_t index) const;

    // A per sequence slot of an animated track whose keys live in this file.
    bool IsInlineSlot(uint16_t loopIndex, uint32_t slot) const;

    bool ReadString(uint32_t pos, std::string& out) const;
    bool ReadTrack(uint32_t pos, ValueType type, Track& out, std::string& error) const;
    bool ReadPartTrack(uint32_t pos, ValueType type, PartTrack& out, std::string& error) const;
    bool ReadU16Array(uint32_t pos, std::vector<uint16_t>& out) const;
    bool ReadVec3Array(uint32_t pos, std::vector<float>& out) const;

    bool ValidateTrack(ValueType type, const Track& track, std::string& error) const;
    bool ValidatePartTrack(ValueType type, const PartTrack& track, bool needsKey, std::string& error) const;

    // Writers append fresh array data and repoint the M2Array at pos, repeated writes with the same owner reuse the tail.
    bool WriteString(uint32_t pos, const std::string& value, const std::string& owner, std::string& error);
    bool WriteTrack(uint32_t pos, ValueType type, const Track& track, const std::string& owner, std::string& error);
    bool WritePartTrack(uint32_t pos, ValueType type, const PartTrack& track, bool needsKey, const std::string& owner, std::string& error);
    bool WriteU16Array(uint32_t pos, const std::vector<uint16_t>& values, const std::string& owner, std::string& error);
    bool WriteVec3Array(uint32_t pos, const std::vector<float>& values, const std::string& owner, std::string& error);

    // Points a top-level header array at a fresh copy of the given records.
    bool ReplaceRecords(uint32_t field, const std::vector<uint8_t>& records, uint32_t count, std::string& error);
    bool CopyRecords(uint32_t field, uint32_t stride, std::vector<uint8_t>& out) const;

    // Modern TXID file data id of a texture, 0 when none, the setter grows TXID when the chunk exists.
    uint32_t TextureFileDataId(uint32_t index) const;
    void SetTextureFileDataId(uint32_t index, uint32_t fileDataId);
    uint32_t SkinFileDataId(uint32_t profile) const;
    bool HasAnimFileDataIds() const;
    uint32_t AnimFileDataId(uint16_t id, uint16_t variation) const;
    std::vector<std::string> ChunkNames() const;

    // Keeps TXAC, EXP2 and PGD1 in step with the particle array, removeIndex below 0 means one emitter was appended.
    void AdjustParticleChunks(uint32_t oldCount, int32_t removeIndex, int32_t copyFrom, std::string& notes);

    const std::vector<M2Batch>& Batches() const {
        return this->m_batches;
    }

    private:
    bool Parse(const std::vector<uint8_t>& file, std::string& error);
    void LoadBatches();
    bool Append(const void* data, uint32_t size, uint32_t& offset, std::string& error);
    bool WriteArrays(uint32_t pos, const std::vector<std::pair<const void*, std::pair<uint32_t, uint32_t>>>& arrays, const std::string& owner, std::string& error);
    Chunk* FindChunk(uint32_t tag);
    const Chunk* FindChunk(uint32_t tag) const;
    void BeginOwned(const std::string& owner);
    void EndOwned(const std::string& owner, uint32_t start);
    uint32_t Align16() const;

    std::string m_path;
    bool m_modern = false;
    std::vector<uint8_t> m_body;
    std::vector<Chunk> m_chunks;
    size_t m_bodyChunk = 0;
    std::vector<uint8_t> m_trailing;
    std::vector<M2Batch> m_batches;
    uint32_t m_revision = 0;
    bool m_dirty = false;
    std::string m_tailOwner;
    uint32_t m_tailStart = 0;
    uint32_t m_tailEnd = 0;
};

}

#endif
