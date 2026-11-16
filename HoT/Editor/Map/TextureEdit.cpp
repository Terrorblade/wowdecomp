#include "HoT/Editor/Map/TextureEdit.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/WorldClient/Map.h"
#include "WoW/Source/WorldClient/MapArea.h"
#include "WoW/Source/WorldClient/MapChunk.h"
#include "WoW/Source/WorldClient/MapRenderChunk.h"
#include "WoW/Source/WorldClient/Types.h"
#include "WoW/Source/Client.h"
#include "HoT/Editor/Map/TerrainEdit.h"

namespace HoT::Editor::Map::TextureEdit {

static const float s_chunkSize = 33.333332f;
static const float s_texelSize = 33.333332f / 64.0f;

struct ChunkTextures {
    CMapChunk* m_chunk;
    uint32_t m_layerCount;
    SMLayer m_layers[4];
    float m_weights[4][4096];
    uint8_t m_mcal[3 * 4096];
};

Brush s_brush = { 15.0f, 0.5f, 0.9f, 255.0f };

static std::string s_texture;
static std::map<uint32_t, ChunkTextures*> s_chunks;
static std::vector<ChunkTextures*> s_retired;
static std::vector<uint32_t> s_pendingRebuild;
static std::vector<std::string> s_tilesets;
static bool s_tilesetsBuilt;
static bool s_stroking;

void ListArchives(int32_t (*callback)(const char* fileName, void* param), void* param) {
    uint32_t count = 0;
    SArchive* const* archives = ClientGetArchiveHandles(count);

    for (uint32_t i = 0; archives && i < count; i++) {
        if (archives[i]) {
            SFile::List(archives[i], callback, param);
        }
    }
}

void SetTexture(const char* name) {
    // CMapArea::LoadTerrainTexture copies the name into a 256 byte buffer and needs an extension.
    s_texture = name && strlen(name) < 240 && strrchr(name, '.') ? name : "";
}

const char* GetTexture() {
    return s_texture.c_str();
}

static uint32_t ChunkKey(int32_t row, int32_t col) {
    return (static_cast<uint32_t>(row) << 10) | static_cast<uint32_t>(col);
}

static bool BigAlpha() {
    return (CMap::header.m_flags & 0x4) != 0;
}

static CMapArea* AreaOf(CMapChunk* chunk) {
    return static_cast<CMapArea*>(chunk->m_parentLinkList.Head()->m_ref);
}

static void DecodeLayer(CMapChunk* chunk, const SMLayer& layer, uint8_t* out) {
    memset(out, 0, 4096);

    if (!(layer.m_flags & 0x100) || !chunk->m_additionalShadowmap) {
        return;
    }

    const uint8_t* src = chunk->m_additionalShadowmap + layer.m_offsetInMCAL;

    if (BigAlpha()) {
        if (layer.m_flags & 0x200) {
            uint32_t written = 0;

            while (written < 4096) {
                uint8_t header = *src++;
                uint32_t count = header & 0x7F;

                for (uint32_t i = 0; i < count && written < 4096; i++) {
                    out[written++] = (header & 0x80) ? *src : src[i];
                }

                src += (header & 0x80) ? 1 : count;
            }
        } else {
            memcpy(out, src, 4096);
        }

        return;
    }

    for (uint32_t i = 0; i < 2048; i++) {
        out[2 * i] = static_cast<uint8_t>((src[i] & 0x0F) * 17);
        out[2 * i + 1] = static_cast<uint8_t>((src[i] >> 4) * 17);
    }
}

static void Encode(ChunkTextures* state) {
    bool bigAlpha = BigAlpha();
    uint32_t layerSize = bigAlpha ? 4096 : 2048;

    for (uint32_t layer = 0; layer < state->m_layerCount; layer++) {
        SMLayer& info = state->m_layers[layer];
        info.m_flags &= ~0x300u;

        if (!layer) {
            info.m_offsetInMCAL = 0;
            continue;
        }

        info.m_flags |= 0x100;
        info.m_offsetInMCAL = (layer - 1) * layerSize;
        uint8_t* dst = &state->m_mcal[info.m_offsetInMCAL];
        const float* weights = state->m_weights[layer];

        if (bigAlpha) {
            for (uint32_t i = 0; i < 4096; i++) {
                dst[i] = static_cast<uint8_t>(std::min(std::max(lroundf(weights[i]), 0L), 255L));
            }
        } else {
            for (uint32_t i = 0; i < 2048; i++) {
                uint32_t low = static_cast<uint32_t>(std::min(std::max(lroundf(weights[2 * i] / 17.0f), 0L), 15L));
                uint32_t high = static_cast<uint32_t>(std::min(std::max(lroundf(weights[2 * i + 1] / 17.0f), 0L), 15L));
                dst[i] = static_cast<uint8_t>(low | (high << 4));
            }
        }
    }

    state->m_chunk->m_header->m_nLayers = state->m_layerCount;
    state->m_chunk->m_layers = state->m_layers;
    state->m_chunk->m_additionalShadowmap = state->m_mcal;
}

static ChunkTextures* GetState(int32_t row, int32_t col) {
    CMapChunk* chunk = TerrainEdit::GetChunk(row, col);

    if (!chunk || !chunk->m_header) {
        return nullptr;
    }

    uint32_t key = ChunkKey(row, col);
    auto found = s_chunks.find(key);
    ChunkTextures* state = found != s_chunks.end() ? found->second : nullptr;

    if (state && state->m_chunk == chunk && chunk->m_layers == state->m_layers) {
        return state;
    }

    if (!state) {
        state = new ChunkTextures();
        s_chunks[key] = state;
    }

    state->m_chunk = chunk;
    state->m_layerCount = std::min<uint32_t>(chunk->m_header->m_nLayers, 4);
    memset(state->m_weights, 0, sizeof(state->m_weights));

    for (uint32_t i = 0; i < state->m_layerCount; i++) {
        state->m_layers[i] = chunk->m_layers[i];
    }

    uint8_t alpha[4096];

    for (uint32_t i = 0; i < 4096; i++) {
        state->m_weights[0][i] = 255.0f;
    }

    for (uint32_t layer = 1; layer < state->m_layerCount; layer++) {
        DecodeLayer(chunk, state->m_layers[layer], alpha);

        for (uint32_t i = 0; i < 4096; i++) {
            state->m_weights[layer][i] = alpha[i];
            state->m_weights[0][i] -= alpha[i];
        }
    }

    for (uint32_t i = 0; i < 4096; i++) {
        state->m_weights[0][i] = std::max(state->m_weights[0][i], 0.0f);
    }

    Encode(state);
    return state;
}

static void QueueRebuild(int32_t row, int32_t col) {
    uint32_t key = ChunkKey(row & ~1, col & ~1);

    if (std::find(s_pendingRebuild.begin(), s_pendingRebuild.end(), key) == s_pendingRebuild.end()) {
        s_pendingRebuild.push_back(key);
    }
}

void RequestRebuild(int32_t row, int32_t col) {
    QueueRebuild(row, col);
}

void FlushRenderRebuilds() {
    for (uint32_t key : s_pendingRebuild) {
        int32_t row = static_cast<int32_t>(key >> 10);
        int32_t col = static_cast<int32_t>(key & 0x3FF);

        for (int32_t dr = 0; dr < 2; dr++) {
            for (int32_t dc = 0; dc < 2; dc++) {
                CMapChunk* chunk = TerrainEdit::GetChunk(row + dr, col + dc);

                if (chunk && chunk->m_renderChunk) {
                    CMap::FreeRenderChunk(chunk->m_renderChunk);
                }
            }
        }
    }

    s_pendingRebuild.clear();
}

static int32_t FindAreaTexture(CMapArea* area, const char* name, bool add) {
    for (uint32_t i = 0; i < area->m_textures.Count(); i++) {
        const char* existing = area->m_textures.m_data[i].m_textureName;

        if (existing && !SStrCmpI(existing, name, STORM_MAX_STR)) {
            return static_cast<int32_t>(i);
        }
    }

    if (!add) {
        return -1;
    }

    size_t length = SStrLen(name) + 1;
    char* copy = static_cast<char*>(SMemAlloc(static_cast<uint32_t>(length), __FILE__, __LINE__, 0));
    SStrCopy(copy, name, static_cast<uint32_t>(length));

    CMapAreaTexture* texture = area->m_textures.New();
    texture->m_textureName = copy;
    texture->m_texture = CMap::LoadTexture(copy);
    return static_cast<int32_t>(area->m_textures.Count() - 1);
}

static void RemoveLayer(ChunkTextures* state, uint32_t layer) {
    for (uint32_t i = layer; i + 1 < state->m_layerCount; i++) {
        state->m_layers[i] = state->m_layers[i + 1];
        memcpy(state->m_weights[i], state->m_weights[i + 1], sizeof(state->m_weights[i]));
    }

    state->m_layerCount--;
}

static bool EraseUnusedLayers(ChunkTextures* state) {
    bool erased = false;

    for (uint32_t layer = state->m_layerCount; layer-- > 0;) {
        if (state->m_layerCount < 2) {
            break;
        }

        bool visible = false;

        for (uint32_t i = 0; i < 4096; i++) {
            if (state->m_weights[layer][i] > 0.0f) {
                visible = true;
                break;
            }
        }

        if (!visible) {
            RemoveLayer(state, layer);
            erased = true;
        }
    }

    return erased;
}

static int32_t LayerForTexture(ChunkTextures* state, uint32_t textureId, float target) {
    for (uint32_t i = 0; i < state->m_layerCount; i++) {
        if (state->m_layers[i].m_textureId == textureId) {
            return static_cast<int32_t>(i);
        }
    }

    if (target == 0.0f) {
        return -1;
    }

    if (state->m_layerCount == 4 && !EraseUnusedLayers(state)) {
        return -1;
    }

    uint32_t layer = state->m_layerCount++;
    state->m_layers[layer].m_textureId = textureId;
    state->m_layers[layer].m_flags = 0;
    state->m_layers[layer].m_offsetInMCAL = 0;
    state->m_layers[layer].m_offectId = 0;

    for (uint32_t i = 0; i < 4096; i++) {
        state->m_weights[layer][i] = layer ? 0.0f : 255.0f;
    }

    return static_cast<int32_t>(layer);
}

static float BrushValue(float dist) {
    float radius = s_brush.m_radius;
    float inner = s_brush.m_hardness * radius;
    float outer = radius - inner;

    if (dist > radius) {
        return 0.0f;
    }

    if (dist < inner || outer <= 0.0f) {
        return 1.0f;
    }

    return 1.0f - (dist - inner) / outer;
}

static float DistanceToTexel(float px, float py, float minX, float minY, float size) {
    float dx = std::max(std::max(minX - px, 0.0f), px - (minX + size));
    float dy = std::max(std::max(minY - py, 0.0f), py - (minY + size));
    return sqrtf(dx * dx + dy * dy);
}

static bool PaintChunk(int32_t row, int32_t col, const C3Vector& pos, float strength, float pressure) {
    CMapChunk* chunk = TerrainEdit::GetChunk(row, col);

    if (!chunk) {
        return false;
    }

    float top = chunk->m_topLeftCoords.x;
    float left = chunk->m_topLeftCoords.y;
    float nearX = std::min(std::max(pos.x, top - s_chunkSize), top);
    float nearY = std::min(std::max(pos.y, left - s_chunkSize), left);

    if (sqrtf((nearX - pos.x) * (nearX - pos.x) + (nearY - pos.y) * (nearY - pos.y)) > s_brush.m_radius) {
        return false;
    }

    CMapArea* area = AreaOf(chunk);
    int32_t textureId = FindAreaTexture(area, s_texture.c_str(), strength > 0.0f);

    if (textureId < 0) {
        return false;
    }

    EditorUndo::RecordTexture(row, col);
    ChunkTextures* state = GetState(row, col);

    if (!state) {
        return false;
    }

    int32_t layer = LayerForTexture(state, static_cast<uint32_t>(textureId), strength);

    if (layer < 0 || state->m_layerCount == 1) {
        Encode(state);
        QueueRebuild(row, col);
        return state->m_layerCount == 1;
    }

    bool changed = false;

    for (int32_t j = 0; j < 64; j++) {
        float texelMaxX = top - j * s_texelSize;

        for (int32_t i = 0; i < 64; i++) {
            float texelMaxY = left - i * s_texelSize;
            float dist = DistanceToTexel(pos.x, pos.y, texelMaxX - s_texelSize, texelMaxY - s_texelSize, s_texelSize);

            if (dist > s_brush.m_radius) {
                continue;
            }

            uint32_t offset = static_cast<uint32_t>(i + 64 * j);
            double alpha[4] = { 0.0, 0.0, 0.0, 0.0 };
            double total = 0.0;

            for (uint32_t n = 0; n < 4; n++) {
                alpha[n] = n < state->m_layerCount ? state->m_weights[n][offset] : 0.0;
                total += alpha[n];
            }

            double current = alpha[layer];
            double others = total - current;
            double change = (strength - current) * pressure * BrushValue(dist);

            if (change < 0.0 && current + change < 1.0) {
                change = -current;
            }

            if (fabs(current - strength) < 0.0001) {
                continue;
            }

            if (others < 1.0) {
                if (change > 0.0) {
                    for (uint32_t n = 0; n < state->m_layerCount; n++) {
                        alpha[n] = static_cast<int32_t>(n) == layer ? 255.0 : 0.0;
                    }
                } else {
                    bool applied = false;

                    for (uint32_t n = 0; n < state->m_layerCount; n++) {
                        if (static_cast<int32_t>(n) == layer) {
                            alpha[n] += change;
                        } else if (!applied) {
                            alpha[n] -= change;
                            applied = true;
                        } else {
                            alpha[layer] += alpha[n];
                            alpha[n] = 0.0;
                        }
                    }
                }
            } else {
                for (uint32_t n = 0; n < state->m_layerCount; n++) {
                    if (static_cast<int32_t>(n) == layer) {
                        alpha[n] += change;
                    } else {
                        alpha[n] -= change * alpha[n] / others;

                        if (alpha[n] < 1.0) {
                            alpha[layer] += alpha[n];
                            alpha[n] = 0.0;
                        }
                    }
                }
            }

            double final = 0.0;

            for (uint32_t n = 0; n < 4; n++) {
                final += alpha[n];
            }

            if (fabs(final - 255.0) > 0.001 && final > 0.0) {
                for (uint32_t n = 0; n < 4; n++) {
                    alpha[n] = alpha[n] * 255.0 / final;
                }
            }

            for (uint32_t n = 0; n < state->m_layerCount; n++) {
                state->m_weights[n][offset] = static_cast<float>(alpha[n]);
            }

            changed = true;
        }
    }

    if (!changed) {
        return false;
    }

    EraseUnusedLayers(state);
    Encode(state);
    QueueRebuild(row, col);
    return true;
}

void Paint(const C3Vector& pos, float elapsedSec) {
    if (s_texture.empty() || elapsedSec <= 0.0f) {
        return;
    }

    if (!s_stroking) {
        s_stroking = true;
        EditorUndo::BeginStep();
    }

    float radius = s_brush.m_radius;
    float strength = s_brush.m_opacity;
    float pressure = 1.0f - powf(1.0f - s_brush.m_pressure, elapsedSec * 10.0f);
    int32_t minRow = TerrainEdit::ChunkCoord(pos.x + radius);
    int32_t maxRow = TerrainEdit::ChunkCoord(pos.x - radius);
    int32_t minCol = TerrainEdit::ChunkCoord(pos.y + radius);
    int32_t maxCol = TerrainEdit::ChunkCoord(pos.y - radius);

    for (int32_t row = minRow; row <= maxRow; row++) {
        for (int32_t col = minCol; col <= maxCol; col++) {
            PaintChunk(row, col, pos, strength, pressure);
        }
    }

    FlushRenderRebuilds();
}

void EndStroke() {
    if (!s_stroking) {
        return;
    }

    s_stroking = false;
    EditorUndo::EndStep();
}

bool GetSaveData(int32_t row, int32_t col, const SMLayer** layers, uint32_t* layerCount, const uint8_t** alpha, uint32_t* alphaSize) {
    auto found = s_chunks.find(ChunkKey(row, col));

    if (found == s_chunks.end() || !found->second) {
        return false;
    }

    ChunkTextures* state = found->second;
    *layers = state->m_layers;
    *layerCount = state->m_layerCount;
    *alpha = state->m_mcal;
    *alphaSize = state->m_layerCount > 1 ? (state->m_layerCount - 1) * (BigAlpha() ? 4096 : 2048) : 0;
    return true;
}

void Rebind() {
    for (auto& entry : s_chunks) {
        ChunkTextures* state = entry.second;
        int32_t row = static_cast<int32_t>(entry.first >> 10);
        int32_t col = static_cast<int32_t>(entry.first & 0x3FF);
        CMapChunk* chunk = TerrainEdit::GetChunk(row, col);

        if (!state || !chunk || !chunk->m_header || chunk->m_layers == state->m_layers) {
            continue;
        }

        state->m_chunk = chunk;
        chunk->m_header->m_nLayers = state->m_layerCount;
        chunk->m_layers = state->m_layers;
        chunk->m_additionalShadowmap = state->m_mcal;
        QueueRebuild(row, col);
    }
}

void ResetArea(int32_t areaX, int32_t areaY) {
    for (auto it = s_chunks.begin(); it != s_chunks.end();) {
        int32_t row = static_cast<int32_t>(it->first >> 10);
        int32_t col = static_cast<int32_t>(it->first & 0x3FF);

        if ((row >> 4) == areaY && (col >> 4) == areaX) {
            delete it->second;
            it = s_chunks.erase(it);
        } else {
            ++it;
        }
    }
}

void FreeRetired() {
    for (ChunkTextures* state : s_retired) {
        delete state;
    }

    s_retired.clear();
}

void Reset() {
    FreeRetired();

    // Chunks of the dying map may still point at these, so they are freed on the next update.
    for (auto& entry : s_chunks) {
        s_retired.push_back(entry.second);
    }

    s_chunks.clear();
    s_pendingRebuild.clear();
    s_stroking = false;
}

uint32_t GetChunkTextures(const C3Vector& pos, const char** names, uint32_t maxNames) {
    CMapChunk* chunk = TerrainEdit::GetChunk(TerrainEdit::ChunkCoord(pos.x), TerrainEdit::ChunkCoord(pos.y));

    if (!chunk || !chunk->m_header) {
        return 0;
    }

    CMapArea* area = AreaOf(chunk);
    uint32_t count = std::min<uint32_t>(std::min<uint32_t>(chunk->m_header->m_nLayers, 4), maxNames);

    for (uint32_t i = 0; i < count; i++) {
        uint32_t id = chunk->m_layers[i].m_textureId;
        names[i] = id < area->m_textures.Count() ? area->m_textures.m_data[id].m_textureName : "";
    }

    return count;
}

bool PickTexture(const C3Vector& pos) {
    int32_t row = TerrainEdit::ChunkCoord(pos.x);
    int32_t col = TerrainEdit::ChunkCoord(pos.y);
    ChunkTextures* state = GetState(row, col);

    if (!state || !state->m_layerCount) {
        return false;
    }

    int32_t j = std::min(std::max(static_cast<int32_t>((state->m_chunk->m_topLeftCoords.x - pos.x) / s_texelSize), 0), 63);
    int32_t i = std::min(std::max(static_cast<int32_t>((state->m_chunk->m_topLeftCoords.y - pos.y) / s_texelSize), 0), 63);
    uint32_t best = 0;

    for (uint32_t layer = 1; layer < state->m_layerCount; layer++) {
        if (state->m_weights[layer][i + 64 * j] > state->m_weights[best][i + 64 * j]) {
            best = layer;
        }
    }

    CMapArea* area = AreaOf(state->m_chunk);
    uint32_t id = state->m_layers[best].m_textureId;

    if (id >= area->m_textures.Count()) {
        return false;
    }

    SetTexture(area->m_textures.m_data[id].m_textureName);
    return true;
}

static int32_t CollectTileset(const char* fileName, void* param) {
    if (SStrCmpI(fileName, "tileset\\", 8) || SStrLen(fileName) < 12) {
        return 1;
    }

    const char* extension = fileName + SStrLen(fileName) - 4;

    if (SStrCmpI(extension, ".blp", STORM_MAX_STR)) {
        return 1;
    }

    size_t length = SStrLen(fileName);

    if (length > 6 && (!SStrCmpI(fileName + length - 6, "_s.blp", STORM_MAX_STR) || !SStrCmpI(fileName + length - 6, "_h.blp", STORM_MAX_STR))) {
        return 1;
    }

    static_cast<std::vector<std::string>*>(param)->push_back(fileName);
    return 1;
}

static void BuildTilesets() {
    if (s_tilesetsBuilt) {
        return;
    }

    s_tilesetsBuilt = true;

    ListArchives(CollectTileset, &s_tilesets);

    for (std::string& name : s_tilesets) {
        for (char& c : name) {
            c = c == '/' ? '\\' : c;
        }
    }

    std::sort(s_tilesets.begin(), s_tilesets.end(), [](const std::string& a, const std::string& b) {
        return SStrCmpI(a.c_str(), b.c_str(), STORM_MAX_STR) < 0;
    });

    s_tilesets.erase(std::unique(s_tilesets.begin(), s_tilesets.end(), [](const std::string& a, const std::string& b) {
        return !SStrCmpI(a.c_str(), b.c_str(), STORM_MAX_STR);
    }), s_tilesets.end());
}

uint32_t GetTilesetCount() {
    BuildTilesets();
    return static_cast<uint32_t>(s_tilesets.size());
}

const char* GetTileset(uint32_t index) {
    BuildTilesets();
    return index < s_tilesets.size() ? s_tilesets[index].c_str() : nullptr;
}

bool Capture(int32_t row, int32_t col, EditorUndo::TextureSnapshot& snapshot) {
    ChunkTextures* state = GetState(row, col);

    if (!state) {
        return false;
    }

    snapshot.m_row = row;
    snapshot.m_col = col;
    snapshot.m_layerCount = state->m_layerCount;

    for (uint32_t layer = 0; layer < 4; layer++) {
        snapshot.m_layers[layer] = layer < state->m_layerCount ? state->m_layers[layer] : SMLayer();
    }

    for (uint32_t layer = 1; layer < 4; layer++) {
        for (uint32_t i = 0; i < 4096; i++) {
            float weight = layer < state->m_layerCount ? state->m_weights[layer][i] : 0.0f;
            snapshot.m_alpha[layer - 1][i] = static_cast<uint8_t>(std::min(std::max(lroundf(weight), 0L), 255L));
        }
    }

    return true;
}

void Restore(const EditorUndo::TextureSnapshot& snapshot) {
    ChunkTextures* state = GetState(snapshot.m_row, snapshot.m_col);

    if (!state) {
        return;
    }

    state->m_layerCount = snapshot.m_layerCount;

    for (uint32_t layer = 0; layer < 4; layer++) {
        state->m_layers[layer] = snapshot.m_layers[layer];
    }

    for (uint32_t i = 0; i < 4096; i++) {
        float base = 255.0f;

        for (uint32_t layer = 1; layer < 4; layer++) {
            state->m_weights[layer][i] = snapshot.m_alpha[layer - 1][i];

            if (layer < state->m_layerCount) {
                base -= snapshot.m_alpha[layer - 1][i];
            }
        }

        state->m_weights[0][i] = std::max(base, 0.0f);
    }

    Encode(state);
    QueueRebuild(snapshot.m_row, snapshot.m_col);
    FlushRenderRebuilds();
}

}
