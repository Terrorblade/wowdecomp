#include "HoT/Editor/Model/ModelDoc.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "HoT/Editor/Model/ModelFiles.h"
#include "HoT/Models/FileDataId.h"

namespace HoT::Editor::Model {

static constexpr uint32_t Tag(const char (&s)[5]) {
    return static_cast<uint32_t>(static_cast<uint8_t>(s[0])) | (static_cast<uint32_t>(static_cast<uint8_t>(s[1])) << 8) | (static_cast<uint32_t>(static_cast<uint8_t>(s[2])) << 16) | (static_cast<uint32_t>(static_cast<uint8_t>(s[3])) << 24);
}

static const uint32_t MAGIC_MD20 = 0x3032444D;
static const uint32_t MAGIC_MD21 = 0x3132444D;
static const uint32_t MAGIC_SKIN = 0x4E494B53;
static const uint32_t CLIENT_VERSION = 264;
static const uint32_t SOURCE_VERSION_MIN = 272;
static const uint32_t SOURCE_VERSION_MAX = 274;
static const uint32_t MAX_BODY = 0x3FFFFFFF;
static const uint32_t MAX_KEYS = 0x40000;
static const uint32_t MAX_BATCHES = 0x4000;
static const float GRAVITY_UNIT = 0.04238648f;
static const float VALUE_LIMIT = 1.0e7f;

static const uint32_t EXP2_STRIDE = 0x1C;
static const uint32_t PGD1_STRIDE = 0x02;
static const uint32_t TXAC_STRIDE = 0x02;

static uint32_t Rd32(const uint8_t* p) {
    uint32_t value;
    memcpy(&value, p, sizeof(value));
    return value;
}

static void Wr32(uint8_t* p, uint32_t value) {
    memcpy(p, &value, sizeof(value));
}

static uint32_t AlignUp(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

uint32_t ValueComponents(ValueType type) {
    switch (type) {
        case ValueType::Vec2:
            return 2;
        case ValueType::Vec3:
        case ValueType::PackedGravity:
            return 3;
        default:
            return 1;
    }
}

uint32_t ValueSize(ValueType type) {
    switch (type) {
        case ValueType::Float:
        case ValueType::PackedGravity:
            return 4;
        case ValueType::Vec2:
            return 8;
        case ValueType::Vec3:
            return 12;
        case ValueType::Fixed16:
        case ValueType::UInt16:
            return 2;
        case ValueType::UInt8:
            return 1;
    }

    return 4;
}

static void DecodeValue(ValueType type, const uint8_t* p, float* out) {
    switch (type) {
        case ValueType::Float:
        case ValueType::Vec2:
        case ValueType::Vec3:
            memcpy(out, p, ValueSize(type));
            break;
        case ValueType::Fixed16: {
            int16_t value;
            memcpy(&value, p, sizeof(value));
            out[0] = value / 32767.0f;
            break;
        }
        case ValueType::UInt8:
            out[0] = p[0];
            break;
        case ValueType::UInt16: {
            uint16_t value;
            memcpy(&value, p, sizeof(value));
            out[0] = value;
            break;
        }
        case ValueType::PackedGravity: {
            // Same decode as ExpandGravityKeys, so -z is the scalar the client uses.
            float dx = static_cast<int8_t>(p[0]) * (1.0f / 128.0f);
            float dy = static_cast<int8_t>(p[1]) * (1.0f / 128.0f);
            int16_t packed;
            memcpy(&packed, p + 2, sizeof(packed));
            float planar = dx * dx + dy * dy;
            float z = sqrtf(planar < 1.0f ? 1.0f - planar : 0.0f);
            float magnitude = packed * GRAVITY_UNIT;

            if (magnitude < 0.0f) {
                z = -z;
                magnitude = -magnitude;
            }

            out[0] = dx * magnitude;
            out[1] = dy * magnitude;
            out[2] = z * magnitude;
            break;
        }
    }
}

static int32_t RoundClamp(float value, int32_t low, int32_t high) {
    long rounded = lroundf(value);
    return static_cast<int32_t>(std::clamp<long>(rounded, low, high));
}

static void EncodeValue(ValueType type, const float* in, uint8_t* p) {
    switch (type) {
        case ValueType::Float:
        case ValueType::Vec2:
        case ValueType::Vec3:
            memcpy(p, in, ValueSize(type));
            break;
        case ValueType::Fixed16: {
            int16_t value = static_cast<int16_t>(RoundClamp(in[0] * 32767.0f, -32767, 32767));
            memcpy(p, &value, sizeof(value));
            break;
        }
        case ValueType::UInt8:
            p[0] = static_cast<uint8_t>(RoundClamp(in[0], 0, 255));
            break;
        case ValueType::UInt16: {
            uint16_t value = static_cast<uint16_t>(RoundClamp(in[0], 0, 65535));
            memcpy(p, &value, sizeof(value));
            break;
        }
        case ValueType::PackedGravity: {
            float magnitude = sqrtf(in[0] * in[0] + in[1] * in[1] + in[2] * in[2]);
            memset(p, 0, 4);

            if (magnitude < 1.0e-6f) {
                break;
            }

            p[0] = static_cast<uint8_t>(static_cast<int8_t>(RoundClamp(in[0] / magnitude * 128.0f, -128, 127)));
            p[1] = static_cast<uint8_t>(static_cast<int8_t>(RoundClamp(in[1] / magnitude * 128.0f, -128, 127)));
            int16_t packed = static_cast<int16_t>(RoundClamp(magnitude / GRAVITY_UNIT, 0, 32767));

            if (in[2] < 0.0f) {
                packed = static_cast<int16_t>(-packed);
            }

            memcpy(p + 2, &packed, sizeof(packed));
            break;
        }
    }
}

static bool ValidValue(ValueType type, const float* in, std::string& error) {
    uint32_t components = ValueComponents(type);

    for (uint32_t i = 0; i < components; i++) {
        float v = in[i];

        if (!std::isfinite(v) || fabsf(v) > VALUE_LIMIT) {
            error = "value out of range";
            return false;
        }

        switch (type) {
            case ValueType::Fixed16:
                if (v < -1.0f || v > 1.0f) {
                    error = "alpha values must be between 0 and 1";
                    return false;
                }

                break;
            case ValueType::UInt8:
                if (v < 0.0f || v > 255.0f) {
                    error = "value must be between 0 and 255";
                    return false;
                }

                break;
            case ValueType::UInt16:
                if (v < 0.0f || v > 65535.0f) {
                    error = "value must be between 0 and 65535";
                    return false;
                }

                break;
            case ValueType::PackedGravity:
                if (fabsf(v) > 32767.0f * GRAVITY_UNIT) {
                    error = "packed gravity is limited to 1388";
                    return false;
                }

                break;
            default:
                break;
        }
    }

    return true;
}

bool ModelDoc::Load(const std::string& path, std::string& error) {
    std::vector<uint8_t> file;

    if (!Files::ReadGameFile(path, file)) {
        error = "cannot open " + path;
        return false;
    }

    this->m_path = path;

    if (!this->Parse(file, error)) {
        return false;
    }

    this->LoadBatches();
    this->m_revision = 1;
    this->m_dirty = false;
    return true;
}

bool ModelDoc::Parse(const std::vector<uint8_t>& file, std::string& error) {
    this->m_body.clear();
    this->m_chunks.clear();
    this->m_trailing.clear();
    this->m_modern = false;

    if (file.size() < 8 || file.size() > MAX_BODY) {
        error = "file size is not a model";
        return false;
    }

    uint32_t magic = Rd32(file.data());

    if (magic == MAGIC_MD21) {
        this->m_modern = true;
        size_t offset = 0;
        bool found = false;

        while (offset + 8 <= file.size()) {
            uint32_t tag = Rd32(file.data() + offset);
            uint32_t size = Rd32(file.data() + offset + 4);

            if (size > file.size() - offset - 8) {
                break;
            }

            Chunk chunk;
            chunk.m_tag = tag;
            const uint8_t* payload = file.data() + offset + 8;

            if (tag == MAGIC_MD21 && !found) {
                this->m_body.assign(payload, payload + size);
                this->m_bodyChunk = this->m_chunks.size();
                found = true;
            } else {
                chunk.m_data.assign(payload, payload + size);
            }

            if (tag == Tag("SKID")) {
                error = "split skeleton (.skel) models are refused by the model loader";
                return false;
            }

            this->m_chunks.push_back(std::move(chunk));
            offset += 8 + static_cast<size_t>(size);
        }

        this->m_trailing.assign(file.begin() + offset, file.end());

        if (!found) {
            error = "no MD21 body in the container";
            return false;
        }
    } else if (magic == MAGIC_MD20) {
        this->m_body = file;
    } else {
        error = "not an M2 model";
        return false;
    }

    if (this->m_body.size() < sizeof(M2Data)) {
        error = "model header is truncated";
        return false;
    }

    uint32_t bodyMagic = 0;
    uint32_t version = 0;
    this->Get(0, bodyMagic);
    this->Get(4, version);

    if (bodyMagic != MAGIC_MD20) {
        error = "model body magic";
        return false;
    }

    if (this->m_modern ? (version < SOURCE_VERSION_MIN || version > SOURCE_VERSION_MAX) : version != CLIENT_VERSION) {
        error = "model version " + std::to_string(version) + " is not one the client loads";
        return false;
    }

    struct Check {
        uint32_t field;
        uint32_t stride;
    };

    const Check checks[] = {
        { HEADER_NAME, 1 },
        { HEADER_LOOPS, 4 },
        { HEADER_SEQUENCES, SEQUENCE_STRIDE },
        { 0x24, 2 },
        { HEADER_BONES, BONE_STRIDE },
        { 0x34, 2 },
        { HEADER_VERTICES, sizeof(M2Vertex) },
        { HEADER_COLORS, sizeof(M2Color) },
        { HEADER_TEXTURES, TEXTURE_STRIDE },
        { HEADER_MATERIALS, MATERIAL_STRIDE },
        { HEADER_TEXTURE_COMBOS, 2 },
        { HEADER_RIBBONS, RIBBON_STRIDE },
        { HEADER_PARTICLES, this->ParticleStride() },
    };

    for (const Check& check : checks) {
        uint32_t count = 0;
        uint32_t offset = 0;

        if (!this->ArrayAt(check.field, count, offset) || !this->Fits(offset, count, check.stride)) {
            error = "model header array at 0x" + std::to_string(check.field) + " points outside the file";
            return false;
        }
    }

    for (uint32_t i = 0; i < this->Count(HEADER_TEXTURES); i++) {
        uint32_t count = 0;
        uint32_t offset = 0;

        if (!this->ArrayAt(this->RecordPos(HEADER_TEXTURES, TEXTURE_STRIDE, i) + 8, count, offset) || !this->Fits(offset, count, 1)) {
            error = "texture name outside the file";
            return false;
        }
    }

    if (this->m_modern && !this->Count(HEADER_BONES) && !this->Count(HEADER_SEQUENCES)) {
        error = "split skeleton (.skel) models are refused by the model loader";
        return false;
    }

    return true;
}

void ModelDoc::LoadBatches() {
    this->m_batches.clear();

    std::string skin = Files::StripExtension(this->m_path) + "00.skin";
    std::vector<uint8_t> file;
    bool read = Files::ReadGameFile(skin, file);

    if (!read && this->m_modern) {
        const char* path = Models::Fdid::ResolveModel(this->SkinFileDataId(0));
        read = path && Files::ReadGameFile(path, file);
    }

    if (!read || file.size() < sizeof(M2SkinProfile) || Rd32(file.data()) != MAGIC_SKIN) {
        return;
    }

    uint32_t count = Rd32(file.data() + 0x24);
    uint32_t offset = Rd32(file.data() + 0x28);

    if (offset > file.size() || static_cast<uint64_t>(count) * sizeof(M2Batch) > file.size() - offset) {
        return;
    }

    count = (std::min)(count, MAX_BATCHES);
    this->m_batches.resize(count);

    if (count) {
        memcpy(this->m_batches.data(), file.data() + offset, sizeof(M2Batch) * count);
    }
}

std::vector<uint8_t> ModelDoc::Build() const {
    if (!this->m_modern) {
        return this->m_body;
    }

    std::vector<uint8_t> out;
    out.reserve(static_cast<size_t>(this->BuildSize()));

    for (size_t i = 0; i < this->m_chunks.size(); i++) {
        const Chunk& chunk = this->m_chunks[i];
        const std::vector<uint8_t>& data = i == this->m_bodyChunk ? this->m_body : chunk.m_data;
        uint8_t header[8];
        Wr32(header, chunk.m_tag);
        Wr32(header + 4, static_cast<uint32_t>(data.size()));
        out.insert(out.end(), header, header + 8);
        out.insert(out.end(), data.begin(), data.end());
    }

    out.insert(out.end(), this->m_trailing.begin(), this->m_trailing.end());
    return out;
}

uint64_t ModelDoc::BuildSize() const {
    if (!this->m_modern) {
        return this->m_body.size();
    }

    uint64_t size = this->m_trailing.size();

    for (size_t i = 0; i < this->m_chunks.size(); i++) {
        size += 8 + (i == this->m_bodyChunk ? this->m_body.size() : this->m_chunks[i].m_data.size());
    }

    return size;
}

std::vector<std::string> ModelDoc::ChunkNames() const {
    std::vector<std::string> names;

    for (const Chunk& chunk : this->m_chunks) {
        char name[5];
        memcpy(name, &chunk.m_tag, 4);
        name[4] = '\0';

        for (int32_t i = 0; i < 4; i++) {
            if (static_cast<unsigned char>(name[i]) < 32 || static_cast<unsigned char>(name[i]) > 126) {
                name[i] = '?';
            }
        }

        names.push_back(name);
    }

    return names;
}

uint32_t ModelDoc::Version() const {
    uint32_t version = 0;
    this->Get(4, version);
    return version;
}

bool ModelDoc::Read(uint32_t pos, void* out, uint32_t size) const {
    if (pos > this->m_body.size() || size > this->m_body.size() - pos) {
        return false;
    }

    memcpy(out, this->m_body.data() + pos, size);
    return true;
}

bool ModelDoc::Write(uint32_t pos, const void* data, uint32_t size) {
    if (pos > this->m_body.size() || size > this->m_body.size() - pos) {
        return false;
    }

    memcpy(this->m_body.data() + pos, data, size);
    return true;
}

bool ModelDoc::ArrayAt(uint32_t pos, uint32_t& count, uint32_t& offset) const {
    return this->Get(pos, count) && this->Get(pos + 4, offset);
}

bool ModelDoc::Fits(uint32_t offset, uint32_t count, uint32_t stride) const {
    if (!count) {
        return true;
    }

    return offset <= this->m_body.size() && static_cast<uint64_t>(count) * stride <= this->m_body.size() - offset;
}

uint32_t ModelDoc::Count(uint32_t field) const {
    uint32_t count = 0;
    this->Get(field, count);
    return count;
}

uint32_t ModelDoc::RecordPos(uint32_t field, uint32_t stride, uint32_t index) const {
    uint32_t offset = 0;
    this->Get(field + 4, offset);
    return offset + index * stride;
}

uint32_t ModelDoc::SequenceFlags(uint32_t index) const {
    uint32_t flags = 0;

    if (index < this->Count(HEADER_SEQUENCES)) {
        this->Get(this->RecordPos(HEADER_SEQUENCES, SEQUENCE_STRIDE, index) + 0x0C, flags);
    }

    return flags;
}

bool ModelDoc::IsInlineSlot(uint16_t loopIndex, uint32_t slot) const {
    if (loopIndex != 0xFFFF) {
        return true;
    }

    return slot < this->Count(HEADER_SEQUENCES) && (this->SequenceFlags(slot) & SEQUENCE_FLAG_INLINE);
}

bool ModelDoc::ReadString(uint32_t pos, std::string& out) const {
    uint32_t count = 0;
    uint32_t offset = 0;
    out.clear();

    if (!this->ArrayAt(pos, count, offset) || !this->Fits(offset, count, 1)) {
        return false;
    }

    for (uint32_t i = 0; i < count && this->m_body[offset + i]; i++) {
        out += static_cast<char>(this->m_body[offset + i]);
    }

    return true;
}

bool ModelDoc::ReadTrack(uint32_t pos, ValueType type, Track& out, std::string& error) const {
    uint16_t trackType = 0;
    uint16_t loop = 0;
    uint32_t timesCount = 0;
    uint32_t timesOffset = 0;
    uint32_t keysCount = 0;
    uint32_t keysOffset = 0;

    if (!this->Get(pos, trackType) || !this->Get(pos + 2, loop) || !this->ArrayAt(pos + 4, timesCount, timesOffset) || !this->ArrayAt(pos + 0x0C, keysCount, keysOffset)) {
        error = "track outside the file";
        return false;
    }

    out.m_interpolation = trackType;
    out.m_globalSequence = loop == 0xFFFF ? -1 : loop;
    out.m_sequences.clear();

    uint32_t slots = (std::min)(timesCount, keysCount);

    if (slots && (!this->Fits(timesOffset, slots, 8) || !this->Fits(keysOffset, slots, 8))) {
        error = "track outside the file";
        return false;
    }

    uint32_t stride = ValueSize(type);
    uint32_t components = ValueComponents(type);

    for (uint32_t i = 0; i < slots; i++) {
        if (!this->IsInlineSlot(loop, i)) {
            continue;
        }

        uint32_t tc = 0;
        uint32_t to = 0;
        uint32_t kc = 0;
        uint32_t ko = 0;
        this->ArrayAt(timesOffset + i * 8, tc, to);
        this->ArrayAt(keysOffset + i * 8, kc, ko);
        uint32_t count = (std::min)(tc, kc);

        if (!count) {
            continue;
        }

        if (count > MAX_KEYS || !this->Fits(to, count, 4) || !this->Fits(ko, count, stride)) {
            error = "track keys outside the file";
            return false;
        }

        TrackKeys keys;
        keys.m_sequence = i;
        keys.m_times.resize(count);
        keys.m_values.resize(static_cast<size_t>(count) * components);
        memcpy(keys.m_times.data(), this->m_body.data() + to, static_cast<size_t>(count) * 4);

        for (uint32_t k = 0; k < count; k++) {
            DecodeValue(type, this->m_body.data() + ko + k * stride, &keys.m_values[static_cast<size_t>(k) * components]);
        }

        out.m_sequences.push_back(std::move(keys));
    }

    return true;
}

bool ModelDoc::ReadPartTrack(uint32_t pos, ValueType type, PartTrack& out, std::string& error) const {
    uint32_t timesCount = 0;
    uint32_t timesOffset = 0;
    uint32_t valuesCount = 0;
    uint32_t valuesOffset = 0;
    uint32_t stride = ValueSize(type);
    uint32_t components = ValueComponents(type);

    if (!this->ArrayAt(pos, timesCount, timesOffset) || !this->ArrayAt(pos + 8, valuesCount, valuesOffset) || timesCount > MAX_KEYS || valuesCount > MAX_KEYS || !this->Fits(timesOffset, timesCount, 2) || !this->Fits(valuesOffset, valuesCount, stride)) {
        error = "particle track outside the file";
        return false;
    }

    out.m_times.resize(timesCount);
    out.m_values.resize(static_cast<size_t>(valuesCount) * components);

    for (uint32_t i = 0; i < timesCount; i++) {
        int16_t value;
        memcpy(&value, this->m_body.data() + timesOffset + i * 2, sizeof(value));
        out.m_times[i] = value / 32767.0f;
    }

    for (uint32_t i = 0; i < valuesCount; i++) {
        DecodeValue(type, this->m_body.data() + valuesOffset + i * stride, &out.m_values[static_cast<size_t>(i) * components]);
    }

    return true;
}

bool ModelDoc::ReadU16Array(uint32_t pos, std::vector<uint16_t>& out) const {
    uint32_t count = 0;
    uint32_t offset = 0;

    if (!this->ArrayAt(pos, count, offset) || count > MAX_KEYS || !this->Fits(offset, count, 2)) {
        return false;
    }

    out.resize(count);

    if (count) {
        memcpy(out.data(), this->m_body.data() + offset, static_cast<size_t>(count) * 2);
    }

    return true;
}

bool ModelDoc::ReadVec3Array(uint32_t pos, std::vector<float>& out) const {
    uint32_t count = 0;
    uint32_t offset = 0;

    if (!this->ArrayAt(pos, count, offset) || count > MAX_KEYS || !this->Fits(offset, count, 12)) {
        return false;
    }

    out.resize(static_cast<size_t>(count) * 3);

    if (count) {
        memcpy(out.data(), this->m_body.data() + offset, static_cast<size_t>(count) * 12);
    }

    return true;
}

bool ModelDoc::ValidateTrack(ValueType type, const Track& track, std::string& error) const {
    if (track.m_interpolation > 3) {
        error = "interpolation must be 0 to 3";
        return false;
    }

    bool global = track.m_globalSequence >= 0;

    if (global && static_cast<uint32_t>(track.m_globalSequence) >= this->Count(HEADER_LOOPS)) {
        error = "global sequence " + std::to_string(track.m_globalSequence) + " does not exist";
        return false;
    }

    if (track.m_globalSequence < -1) {
        error = "global sequence must be -1 or an index";
        return false;
    }

    uint32_t sequenceCount = this->Count(HEADER_SEQUENCES);
    uint32_t components = ValueComponents(type);
    std::vector<uint32_t> seen;

    for (const TrackKeys& keys : track.m_sequences) {
        std::string label = "sequence " + std::to_string(keys.m_sequence);

        if (global ? keys.m_sequence >= 4096 : keys.m_sequence >= sequenceCount) {
            error = label + " does not exist";
            return false;
        }

        if (!global && !this->IsInlineSlot(0xFFFF, keys.m_sequence)) {
            error = label + " keys live in a .anim file and cannot be edited";
            return false;
        }

        if (std::find(seen.begin(), seen.end(), keys.m_sequence) != seen.end()) {
            error = label + " is listed twice";
            return false;
        }

        seen.push_back(keys.m_sequence);

        if (keys.m_times.size() > MAX_KEYS || keys.m_values.size() != keys.m_times.size() * components) {
            error = label + " needs one value per time";
            return false;
        }

        for (size_t i = 1; i < keys.m_times.size(); i++) {
            if (keys.m_times[i] < keys.m_times[i - 1]) {
                error = label + " times must be ascending";
                return false;
            }
        }

        for (size_t i = 0; i < keys.m_times.size(); i++) {
            if (!ValidValue(type, &keys.m_values[i * components], error)) {
                error = label + ": " + error;
                return false;
            }
        }
    }

    return true;
}

bool ModelDoc::ValidatePartTrack(ValueType type, const PartTrack& track, bool needsKey, std::string& error) const {
    uint32_t components = ValueComponents(type);

    if (track.m_times.size() > MAX_KEYS || track.m_values.size() != track.m_times.size() * components) {
        error = "needs one value per time";
        return false;
    }

    if (needsKey && track.m_times.empty()) {
        error = "needs at least one key";
        return false;
    }

    for (size_t i = 0; i < track.m_times.size(); i++) {
        float t = track.m_times[i];

        if (!std::isfinite(t) || t < 0.0f || t > 1.0f) {
            error = "times must be between 0 and 1";
            return false;
        }

        if (i && !(t > track.m_times[i - 1])) {
            error = "times must be strictly ascending";
            return false;
        }

        if (!ValidValue(type, &track.m_values[i * components], error)) {
            return false;
        }
    }

    return true;
}

uint32_t ModelDoc::Align16() const {
    return AlignUp(static_cast<uint32_t>(this->m_body.size()), 16);
}

bool ModelDoc::Append(const void* data, uint32_t size, uint32_t& offset, std::string& error) {
    uint32_t start = this->Align16();

    if (start > MAX_BODY || size > MAX_BODY - start) {
        error = "model would grow past 1 GB";
        return false;
    }

    this->m_body.resize(start, 0);

    if (size) {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        this->m_body.insert(this->m_body.end(), bytes, bytes + size);
    }

    offset = start;
    return true;
}

void ModelDoc::BeginOwned(const std::string& owner) {
    if (!owner.empty() && owner == this->m_tailOwner && this->m_tailEnd == this->m_body.size() && this->m_tailStart <= this->m_tailEnd) {
        this->m_body.resize(this->m_tailStart);
    }

    this->m_tailOwner.clear();
}

void ModelDoc::EndOwned(const std::string& owner, uint32_t start) {
    this->m_tailOwner = owner;
    this->m_tailStart = start;
    this->m_tailEnd = static_cast<uint32_t>(this->m_body.size());
}

bool ModelDoc::WriteTrack(uint32_t pos, ValueType type, const Track& track, const std::string& owner, std::string& error) {
    uint16_t oldLoop = 0;
    uint32_t oldTimesCount = 0;
    uint32_t oldTimesOffset = 0;
    uint32_t oldKeysCount = 0;
    uint32_t oldKeysOffset = 0;

    if (!this->Get(pos + 2, oldLoop) || !this->ArrayAt(pos + 4, oldTimesCount, oldTimesOffset) || !this->ArrayAt(pos + 0x0C, oldKeysCount, oldKeysOffset) || pos + 0x14 > this->m_body.size()) {
        error = "track outside the file";
        return false;
    }

    if (!this->ValidateTrack(type, track, error)) {
        return false;
    }

    uint16_t loop = track.m_globalSequence < 0 ? 0xFFFF : static_cast<uint16_t>(track.m_globalSequence);
    uint32_t sequenceCount = this->Count(HEADER_SEQUENCES);
    uint32_t slots = 0;

    struct Preserved {
        uint32_t slot;
        uint8_t times[8];
        uint8_t keys[8];
    };

    // Slots of sequences stored in .anim files keep their entries, they are relocated against the anim file.
    std::vector<Preserved> preserved;

    if (loop == 0xFFFF && oldLoop == 0xFFFF) {
        uint32_t oldSlots = (std::min)(oldTimesCount, oldKeysCount);

        if (oldSlots && this->Fits(oldTimesOffset, oldSlots, 8) && this->Fits(oldKeysOffset, oldSlots, 8)) {
            slots = (std::min)(oldSlots, sequenceCount);

            for (uint32_t i = 0; i < slots; i++) {
                if (this->IsInlineSlot(0xFFFF, i)) {
                    continue;
                }

                Preserved entry;
                entry.slot = i;
                this->Read(oldTimesOffset + i * 8, entry.times, 8);
                this->Read(oldKeysOffset + i * 8, entry.keys, 8);

                if (Rd32(entry.times) || Rd32(entry.keys)) {
                    preserved.push_back(entry);
                }
            }
        }
    }

    for (const TrackKeys& keys : track.m_sequences) {
        if (!keys.m_times.empty()) {
            slots = (std::max)(slots, keys.m_sequence + 1);
        }
    }

    bool anyKeys = !preserved.empty();

    for (const TrackKeys& keys : track.m_sequences) {
        anyKeys = anyKeys || !keys.m_times.empty();
    }

    if (!anyKeys) {
        slots = 0;
    }

    this->BeginOwned(owner);
    uint32_t start = static_cast<uint32_t>(this->m_body.size());
    uint32_t base = this->Align16();
    uint32_t stride = ValueSize(type);
    uint32_t components = ValueComponents(type);
    std::vector<uint8_t> blob(static_cast<size_t>(slots) * 16, 0);

    auto place = [&](const void* data, size_t size) -> uint32_t {
        uint32_t local = AlignUp(static_cast<uint32_t>(blob.size()), 16);
        blob.resize(local, 0);
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        blob.insert(blob.end(), bytes, bytes + size);
        return base + local;
    };

    for (const Preserved& entry : preserved) {
        memcpy(&blob[entry.slot * 8], entry.times, 8);
        memcpy(&blob[slots * 8 + entry.slot * 8], entry.keys, 8);
    }

    for (const TrackKeys& keys : track.m_sequences) {
        if (keys.m_times.empty()) {
            continue;
        }

        uint32_t count = static_cast<uint32_t>(keys.m_times.size());
        std::vector<uint8_t> values(static_cast<size_t>(count) * stride);

        for (uint32_t k = 0; k < count; k++) {
            EncodeValue(type, &keys.m_values[static_cast<size_t>(k) * components], &values[static_cast<size_t>(k) * stride]);
        }

        uint32_t timesAt = place(keys.m_times.data(), static_cast<size_t>(count) * 4);
        uint32_t valuesAt = place(values.data(), values.size());
        Wr32(&blob[keys.m_sequence * 8], count);
        Wr32(&blob[keys.m_sequence * 8 + 4], timesAt);
        Wr32(&blob[slots * 8 + keys.m_sequence * 8], count);
        Wr32(&blob[slots * 8 + keys.m_sequence * 8 + 4], valuesAt);
    }

    uint32_t offset = 0;

    if (slots && (!this->Append(blob.data(), static_cast<uint32_t>(blob.size()), offset, error) || offset != base)) {
        if (error.empty()) {
            error = "track write misplaced";
        }

        return false;
    }

    uint32_t timesArray[2] = { slots, slots ? base : 0 };
    uint32_t keysArray[2] = { slots, slots ? base + slots * 8 : 0 };
    this->Put(pos, track.m_interpolation);
    this->Put(pos + 2, loop);
    this->Write(pos + 4, timesArray, 8);
    this->Write(pos + 0x0C, keysArray, 8);
    this->EndOwned(owner, start);
    return true;
}

bool ModelDoc::WriteArrays(uint32_t pos, const std::vector<std::pair<const void*, std::pair<uint32_t, uint32_t>>>& arrays, const std::string& owner, std::string& error) {
    if (pos + 8 * arrays.size() > this->m_body.size()) {
        error = "array outside the file";
        return false;
    }

    this->BeginOwned(owner);
    uint32_t start = static_cast<uint32_t>(this->m_body.size());

    for (size_t i = 0; i < arrays.size(); i++) {
        uint32_t count = arrays[i].second.first;
        uint32_t bytes = arrays[i].second.second;
        uint32_t offset = 0;

        if (count && !this->Append(arrays[i].first, bytes, offset, error)) {
            return false;
        }

        uint32_t header[2] = { count, count ? offset : 0 };
        this->Write(pos + static_cast<uint32_t>(i) * 8, header, 8);
    }

    this->EndOwned(owner, start);
    return true;
}

bool ModelDoc::WritePartTrack(uint32_t pos, ValueType type, const PartTrack& track, bool needsKey, const std::string& owner, std::string& error) {
    if (!this->ValidatePartTrack(type, track, needsKey, error)) {
        return false;
    }

    uint32_t count = static_cast<uint32_t>(track.m_times.size());
    uint32_t stride = ValueSize(type);
    uint32_t components = ValueComponents(type);
    std::vector<int16_t> times(count);
    std::vector<uint8_t> values(static_cast<size_t>(count) * stride);

    for (uint32_t i = 0; i < count; i++) {
        times[i] = static_cast<int16_t>(RoundClamp(track.m_times[i] * 32767.0f, 0, 32767));
        EncodeValue(type, &track.m_values[static_cast<size_t>(i) * components], &values[static_cast<size_t>(i) * stride]);
    }

    return this->WriteArrays(pos, { { times.data(), { count, count * 2 } }, { values.data(), { count, count * stride } } }, owner, error);
}

bool ModelDoc::WriteString(uint32_t pos, const std::string& value, const std::string& owner, std::string& error) {
    if (value.size() > 1024) {
        error = "text is too long";
        return false;
    }

    uint32_t count = value.empty() ? 0 : static_cast<uint32_t>(value.size() + 1);
    return this->WriteArrays(pos, { { value.c_str(), { count, count } } }, owner, error);
}

bool ModelDoc::WriteU16Array(uint32_t pos, const std::vector<uint16_t>& values, const std::string& owner, std::string& error) {
    uint32_t count = static_cast<uint32_t>(values.size());
    return this->WriteArrays(pos, { { values.data(), { count, count * 2 } } }, owner, error);
}

bool ModelDoc::WriteVec3Array(uint32_t pos, const std::vector<float>& values, const std::string& owner, std::string& error) {
    uint32_t count = static_cast<uint32_t>(values.size() / 3);
    return this->WriteArrays(pos, { { values.data(), { count, count * 12 } } }, owner, error);
}

bool ModelDoc::ReplaceRecords(uint32_t field, const std::vector<uint8_t>& records, uint32_t count, std::string& error) {
    this->m_tailOwner.clear();
    uint32_t offset = 0;

    if (count && !this->Append(records.data(), static_cast<uint32_t>(records.size()), offset, error)) {
        return false;
    }

    uint32_t header[2] = { count, count ? offset : 0 };
    return this->Write(field, header, 8);
}

bool ModelDoc::CopyRecords(uint32_t field, uint32_t stride, std::vector<uint8_t>& out) const {
    uint32_t count = 0;
    uint32_t offset = 0;

    if (!this->ArrayAt(field, count, offset) || !this->Fits(offset, count, stride)) {
        return false;
    }

    out.assign(this->m_body.begin() + offset, this->m_body.begin() + offset + static_cast<size_t>(count) * stride);
    return true;
}

Chunk* ModelDoc::FindChunk(uint32_t tag) {
    for (size_t i = 0; i < this->m_chunks.size(); i++) {
        if (i != this->m_bodyChunk && this->m_chunks[i].m_tag == tag) {
            return &this->m_chunks[i];
        }
    }

    return nullptr;
}

const Chunk* ModelDoc::FindChunk(uint32_t tag) const {
    return const_cast<ModelDoc*>(this)->FindChunk(tag);
}

static uint32_t ChunkEntry(const Chunk* chunk, uint32_t index, uint32_t stride) {
    if (!chunk || static_cast<uint64_t>(index + 1) * stride > chunk->m_data.size()) {
        return 0;
    }

    return Rd32(chunk->m_data.data() + static_cast<size_t>(index) * stride);
}

uint32_t ModelDoc::TextureFileDataId(uint32_t index) const {
    return this->m_modern ? ChunkEntry(this->FindChunk(Tag("TXID")), index, 4) : 0;
}

void ModelDoc::SetTextureFileDataId(uint32_t index, uint32_t fileDataId) {
    Chunk* chunk = this->m_modern ? this->FindChunk(Tag("TXID")) : nullptr;

    if (!chunk) {
        return;
    }

    if (chunk->m_data.size() < static_cast<size_t>(index + 1) * 4) {
        if (!fileDataId) {
            return;
        }

        chunk->m_data.resize(static_cast<size_t>(index + 1) * 4, 0);
    }

    Wr32(chunk->m_data.data() + static_cast<size_t>(index) * 4, fileDataId);
}

uint32_t ModelDoc::SkinFileDataId(uint32_t profile) const {
    return this->m_modern ? ChunkEntry(this->FindChunk(Tag("SFID")), profile, 4) : 0;
}

uint32_t ModelDoc::AnimFileDataId(uint16_t id, uint16_t variation) const {
    const Chunk* chunk = this->m_modern ? this->FindChunk(Tag("AFID")) : nullptr;

    if (!chunk) {
        return 0;
    }

    for (size_t pos = 0; pos + 8 <= chunk->m_data.size(); pos += 8) {
        const uint8_t* entry = chunk->m_data.data() + pos;

        if ((entry[0] | (entry[1] << 8)) == id && (entry[2] | (entry[3] << 8)) == variation) {
            return Rd32(entry + 4);
        }
    }

    return 0;
}

bool ModelDoc::HasAnimFileDataIds() const {
    const Chunk* chunk = this->m_modern ? this->FindChunk(Tag("AFID")) : nullptr;
    return chunk && !chunk->m_data.empty();
}

// Rewrites a chunk that starts with an M2Array of per particle records, offsets relative to the chunk data.
static bool AdjustChunkArray(Chunk& chunk, uint32_t stride, uint32_t oldCount, int32_t removeIndex, int32_t copyFrom, const uint8_t* defaults) {
    std::vector<uint8_t>& data = chunk.m_data;

    if (data.size() < 8) {
        return false;
    }

    uint32_t count = Rd32(data.data());
    uint32_t offset = Rd32(data.data() + 4);

    if (count != oldCount || (count && (offset < 8 || offset > data.size() || static_cast<uint64_t>(count) * stride > data.size() - offset))) {
        return false;
    }

    std::vector<uint8_t> records(data.begin() + offset, data.begin() + offset + static_cast<size_t>(count) * stride);

    if (removeIndex >= 0) {
        records.erase(records.begin() + static_cast<size_t>(removeIndex) * stride, records.begin() + static_cast<size_t>(removeIndex + 1) * stride);
    } else if (copyFrom >= 0 && static_cast<uint32_t>(copyFrom) < count) {
        std::vector<uint8_t> copy(records.begin() + static_cast<size_t>(copyFrom) * stride, records.begin() + static_cast<size_t>(copyFrom + 1) * stride);
        records.insert(records.end(), copy.begin(), copy.end());
    } else {
        records.insert(records.end(), defaults, defaults + stride);
    }

    uint32_t newCount = removeIndex >= 0 ? count - 1 : count + 1;
    uint32_t newOffset = AlignUp(static_cast<uint32_t>(data.size()), 4);
    data.resize(newOffset, 0);
    data.insert(data.end(), records.begin(), records.end());
    Wr32(data.data(), newCount);
    Wr32(data.data() + 4, newCount ? newOffset : 0);
    return true;
}

void ModelDoc::AdjustParticleChunks(uint32_t oldCount, int32_t removeIndex, int32_t copyFrom, std::string& notes) {
    if (!this->m_modern) {
        return;
    }

    uint32_t materials = this->Count(HEADER_MATERIALS);

    if (Chunk* txac = this->FindChunk(Tag("TXAC"))) {
        // TXAC holds 2 bytes per material followed by 2 bytes per particle emitter.
        if (txac->m_data.size() == static_cast<size_t>(materials + oldCount) * TXAC_STRIDE) {
            size_t at = static_cast<size_t>(materials) * TXAC_STRIDE;

            if (removeIndex >= 0) {
                txac->m_data.erase(txac->m_data.begin() + at + static_cast<size_t>(removeIndex) * TXAC_STRIDE, txac->m_data.begin() + at + static_cast<size_t>(removeIndex + 1) * TXAC_STRIDE);
            } else {
                uint8_t entry[TXAC_STRIDE] = { 0, 0 };

                if (copyFrom >= 0 && static_cast<uint32_t>(copyFrom) < oldCount) {
                    memcpy(entry, txac->m_data.data() + at + static_cast<size_t>(copyFrom) * TXAC_STRIDE, TXAC_STRIDE);
                }

                txac->m_data.insert(txac->m_data.end(), entry, entry + TXAC_STRIDE);
            }
        } else {
            notes += " TXAC size did not match the materials and emitters, left as is.";
        }
    }

    if (Chunk* exp2 = this->FindChunk(Tag("EXP2"))) {
        uint8_t defaults[EXP2_STRIDE] = {};
        float one = 1.0f;
        memcpy(defaults + 4, &one, 4);
        memcpy(defaults + 8, &one, 4);

        if (!AdjustChunkArray(*exp2, EXP2_STRIDE, oldCount, removeIndex, copyFrom, defaults)) {
            notes += " EXP2 did not match the emitter count, left as is.";
        }
    }

    if (Chunk* pgd1 = this->FindChunk(Tag("PGD1"))) {
        uint8_t defaults[PGD1_STRIDE] = {};

        if (!AdjustChunkArray(*pgd1, PGD1_STRIDE, oldCount, removeIndex, copyFrom, defaults)) {
            notes += " PGD1 did not match the emitter count, left as is.";
        }
    }
}

}
