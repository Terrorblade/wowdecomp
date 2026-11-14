#include "HoT/Editor/Map/TerrainEdit.h"
#include "HoT/Editor/Map/EditorUndo.h"
#include "HoT/Editor/Map/TextureEdit.h"
#include "HoT/Editor/Map/ChunkEdit.h"
#include "HoT/Editor/Map/FlightBoundsEdit.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include "Engine/Source/Gx/CGxBuf.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxVertexPC.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "WoW/Source/WorldClient/Map.h"
#include "WoW/Source/WorldClient/MapArea.h"
#include "WoW/Source/WorldClient/MapChunk.h"
#include "WoW/Source/WorldClient/MapRenderChunk.h"

namespace HoT::Editor::Map::TerrainEdit {

static const float s_mapOrigin = 17066.666f;
static const float s_chunkSize = 33.333332f;
static const float s_unitSize = 4.1666665f;

struct VertexEdit {
    CMapChunk* m_chunk;
    int32_t m_index;
    float m_height;
};

MODE s_mode = MODE_RAISE_LOWER;
RaiseLowerSettings s_raiseLower = { TERRAIN_LINEAR, 15.0f, 0.0f, 2.0f };
FlattenBlurSettings s_flattenBlur = { FLATTEN_LINEAR, 10.0f, 2.0f, true, true, false, 0.0f, 0.0f, false, C3Vector(0.0f, 0.0f, 0.0f) };

static bool s_stroking;
static std::vector<VertexEdit> s_edits;

static float Clamp(float value, float minValue, float maxValue) {
    return value < minValue ? minValue : (value > maxValue ? maxValue : value);
}

float GetRadius() {
    switch (s_mode) {
        case MODE_RAISE_LOWER:
            return s_raiseLower.m_radius;
        case MODE_FLATTEN_BLUR:
            return s_flattenBlur.m_radius;
        case MODE_TEXTURE_PAINT:
            return TextureEdit::s_brush.m_radius;
        case MODE_HOLES:
            return ChunkEdit::s_settings.m_holeRadius;
        case MODE_AREA:
            return ChunkEdit::s_settings.m_areaRadius;
        case MODE_VERTEX:
            return ChunkEdit::s_settings.m_vertexRadius;
        case MODE_FLIGHT:
            return FlightBoundsEdit::s_settings.m_radius;
        default:
            return 0.0f;
    }
}

void ChangeRadius(float delta) {
    if (s_mode == MODE_OBJECT || s_mode == MODE_IMPASS) {
        return;
    }

    if (s_mode == MODE_FLIGHT) {
        float& radius = FlightBoundsEdit::s_settings.m_radius;
        radius = Clamp(radius + delta * 10.0f, 0.0f, 2000.0f);
        return;
    }

    if (s_mode == MODE_HOLES || s_mode == MODE_AREA || s_mode == MODE_VERTEX) {
        float& value = s_mode == MODE_HOLES ? ChunkEdit::s_settings.m_holeRadius : (s_mode == MODE_AREA ? ChunkEdit::s_settings.m_areaRadius : ChunkEdit::s_settings.m_vertexRadius);
        value = Clamp(value + delta, 0.0f, s_mode == MODE_HOLES ? 250.0f : (s_mode == MODE_AREA ? 1000.0f : 10000.0f));
        return;
    }

    float& radius = s_mode == MODE_RAISE_LOWER ? s_raiseLower.m_radius : (s_mode == MODE_FLATTEN_BLUR ? s_flattenBlur.m_radius : TextureEdit::s_brush.m_radius);
    radius = Clamp(radius + delta, 0.0f, 1000.0f);
}

void ChangeInnerRadius(float delta) {
    s_raiseLower.m_innerRadius = Clamp(s_raiseLower.m_innerRadius + delta, 0.0f, 1.0f);
}

void ChangeSpeed(float delta) {
    if (s_mode == MODE_RAISE_LOWER) {
        s_raiseLower.m_speed = Clamp(s_raiseLower.m_speed + delta, 0.0f, 1000.0f);
    } else if (s_mode == MODE_FLATTEN_BLUR) {
        s_flattenBlur.m_speed = Clamp(s_flattenBlur.m_speed + delta, 0.0f, 10.0f);
    } else if (s_mode == MODE_TEXTURE_PAINT) {
        TextureEdit::s_brush.m_pressure = Clamp(TextureEdit::s_brush.m_pressure + delta * 0.1f, 0.0f, 1.0f);
    } else if (s_mode == MODE_VERTEX) {
        ChunkEdit::s_settings.m_vertexSpeed = Clamp(ChunkEdit::s_settings.m_vertexSpeed + delta, 0.0f, 10.0f);
    }
}

CMapChunk* GetChunk(int32_t row, int32_t col) {
    if (row < 0 || row > 1023 || col < 0 || col > 1023) {
        return nullptr;
    }

    CMapArea* area = CMap::m_areaTable[64 * (row >> 4) + (col >> 4)];

    if (!area || area->m_asyncObject) {
        return nullptr;
    }

    CMapChunk* chunk = area->m_mapChunks[(col & 15) + 16 * (row & 15)];
    return chunk && chunk->m_height && chunk->m_normals ? chunk : nullptr;
}

int32_t ChunkCoord(float coord) {
    return static_cast<int32_t>(floorf((s_mapOrigin - coord) / s_chunkSize));
}

static float TriangleHeight(float u, float v, float u0, float v0, float h0, float u1, float v1, float h1, float u2, float v2, float h2) {
    float det = (v1 - v2) * (u0 - u2) + (u2 - u1) * (v0 - v2);
    float w0 = ((v1 - v2) * (u - u2) + (u2 - u1) * (v - v2)) / det;
    float w1 = ((v2 - v0) * (u - u2) + (u0 - u2) * (v - v2)) / det;
    return w0 * h0 + w1 * h1 + (1.0f - w0 - w1) * h2;
}

bool GetHeight(float x, float y, float* z) {
    int32_t row = ChunkCoord(x);
    int32_t col = ChunkCoord(y);
    CMapChunk* chunk = GetChunk(row, col);

    if (!chunk) {
        return false;
    }

    float lx = (s_mapOrigin - row * s_chunkSize - x) / s_unitSize;
    float ly = (s_mapOrigin - col * s_chunkSize - y) / s_unitSize;
    int32_t i = std::min(std::max(static_cast<int32_t>(lx), 0), 7);
    int32_t j = std::min(std::max(static_cast<int32_t>(ly), 0), 7);
    float u = lx - i;
    float v = ly - j;

    const float* h = chunk->m_height;
    float a = h[i * 17 + j];
    float b = h[i * 17 + j + 1];
    float c = h[(i + 1) * 17 + j];
    float d = h[(i + 1) * 17 + j + 1];
    float m = h[i * 17 + 9 + j];
    float du = u - 0.5f;
    float dv = v - 0.5f;
    float local;

    if (fabsf(du) > fabsf(dv)) {
        local = du < 0.0f ? TriangleHeight(u, v, 0.0f, 0.0f, a, 0.0f, 1.0f, b, 0.5f, 0.5f, m) : TriangleHeight(u, v, 1.0f, 0.0f, c, 1.0f, 1.0f, d, 0.5f, 0.5f, m);
    } else {
        local = dv < 0.0f ? TriangleHeight(u, v, 0.0f, 0.0f, a, 1.0f, 0.0f, c, 0.5f, 0.5f, m) : TriangleHeight(u, v, 0.0f, 1.0f, b, 1.0f, 1.0f, d, 0.5f, 0.5f, m);
    }

    *z = local + chunk->m_topLeftCoords.z;
    return true;
}

int32_t GetAreaId(float x, float y) {
    CMapChunk* chunk = GetChunk(ChunkCoord(x), ChunkCoord(y));
    return chunk ? chunk->m_areaId : 0;
}

bool Raycast(const C3Vector& start, const C3Vector& dir, float maxDistance, C3Vector* hit) {
    const float step = 1.0f;
    float prevT = 0.0f;
    bool prevValid = false;

    for (float t = 0.0f; t <= maxDistance; t += step) {
        C3Vector p(start.x + dir.x * t, start.y + dir.y * t, start.z + dir.z * t);
        float h;

        if (!GetHeight(p.x, p.y, &h)) {
            prevValid = false;
            prevT = t;
            continue;
        }

        if (p.z <= h) {
            float lo = prevValid ? prevT : std::max(t - step, 0.0f);
            float hi = t;

            for (int32_t k = 0; k < 16; k++) {
                float mid = (lo + hi) * 0.5f;
                C3Vector q(start.x + dir.x * mid, start.y + dir.y * mid, start.z + dir.z * mid);
                float qh;

                if (GetHeight(q.x, q.y, &qh) && q.z <= qh) {
                    hi = mid;
                } else {
                    lo = mid;
                }
            }

            *hit = C3Vector(start.x + dir.x * hi, start.y + dir.y * hi, start.z + dir.z * hi);
            GetHeight(hit->x, hit->y, &hit->z);
            return true;
        }

        prevValid = true;
        prevT = t;
    }

    return false;
}

static bool RaiseLowerChange(float dx, float dy, float* dt) {
    float radius = s_raiseLower.m_radius;
    float inner = s_raiseLower.m_innerRadius;
    float dist = sqrtf(dx * dx + dy * dy);

    if (s_raiseLower.m_type == TERRAIN_QUADRA) {
        if (fabsf(dx) >= fabsf(radius / 2.0f) || fabsf(dy) >= fabsf(radius / 2.0f)) {
            return false;
        }

        *dt = *dt * (1.0f - dist * inner / radius);
        return true;
    }

    if (dist >= radius) {
        return false;
    }

    switch (s_raiseLower.m_type) {
        case TERRAIN_FLAT:
            break;
        case TERRAIN_LINEAR:
            *dt = *dt * (1.0f - dist * (1.0f - inner) / radius);
            break;
        case TERRAIN_SMOOTH:
            *dt = *dt / (1.0f + dist / radius);
            break;
        case TERRAIN_POLYNOM:
            *dt = *dt * ((dist / radius) * (dist / radius) + dist / radius + 1.0f);
            break;
        case TERRAIN_TRIGO:
            *dt = *dt * cosf(dist / radius);
            break;
        case TERRAIN_GAUSSIAN: {
            float d = dist < radius * inner ? inner : dist / radius;
            *dt = *dt * expf(-(d * d) / (2.0f * 0.39f * 0.39f));
            break;
        }
        default:
            return false;
    }

    return true;
}

static float FlattenBlend(float dist, float remain) {
    float radius = s_flattenBlur.m_radius;

    switch (s_flattenBlur.m_type) {
        case FLATTEN_FLAT:
            return remain;
        case FLATTEN_LINEAR:
            return remain * (1.0f - dist / radius);
        case FLATTEN_SMOOTH:
            return powf(remain, 1.0f + dist / radius);
        default:
            return remain;
    }
}

static void UpdateBounds(CMapChunk* chunk) {
    float minZ = chunk->m_height[0];
    float maxZ = chunk->m_height[0];

    for (int32_t i = 1; i < 145; i++) {
        minZ = std::min(minZ, chunk->m_height[i]);
        maxZ = std::max(maxZ, chunk->m_height[i]);
    }

    minZ += chunk->m_topLeftCoords.z;
    maxZ += chunk->m_topLeftCoords.z;
    chunk->m_bbox.b.z = std::min(chunk->m_bbox.b.z, minZ);
    chunk->m_bbox.t.z = std::max(chunk->m_bbox.t.z, maxZ);
    chunk->m_bbox2.b.z = std::min(chunk->m_bbox2.b.z, minZ);
    chunk->m_bbox2.t.z = std::max(chunk->m_bbox2.t.z, maxZ);

    C3Vector center((chunk->m_bbox.b.x + chunk->m_bbox.t.x) * 0.5f, (chunk->m_bbox.b.y + chunk->m_bbox.t.y) * 0.5f, (chunk->m_bbox.b.z + chunk->m_bbox.t.z) * 0.5f);
    C3Vector extent(chunk->m_bbox.t.x - center.x, chunk->m_bbox.t.y - center.y, chunk->m_bbox.t.z - center.z);
    chunk->m_sphere.c = center;
    chunk->m_sphere.r = sqrtf(extent.x * extent.x + extent.y * extent.y + extent.z * extent.z);

    CMapArea* area = CMap::m_areaTable[64 * (chunk->m_cOffset.y >> 4) + (chunk->m_cOffset.x >> 4)];

    if (area) {
        area->m_bounds.b.z = std::min(area->m_bounds.b.z, minZ);
        area->m_bounds.t.z = std::max(area->m_bounds.t.z, maxZ);
    }

    CMapRenderChunk* renderChunk = chunk->m_renderChunk;

    if (!renderChunk) {
        return;
    }

    CAaBox box = renderChunk->m_mapChunks[0]->m_bbox;

    if (renderChunk->m_mapChunks[1]) {
        box |= renderChunk->m_mapChunks[1]->m_bbox;
    }

    C3Vector diagonal(box.t.x - box.b.x, box.t.y - box.b.y, box.t.z - box.b.z);
    renderChunk->m_sphere.r = sqrtf(diagonal.x * diagonal.x + diagonal.y * diagonal.y + diagonal.z * diagonal.z) * 0.5f;
    renderChunk->m_sphere.c = C3Vector((box.t.x + box.b.x) * 0.5f, (box.t.y + box.b.y) * 0.5f, (box.t.z + box.b.z) * 0.5f);

    if (renderChunk->m_chunkBuf && renderChunk->m_chunkBuf->m_vertexBuf) {
        renderChunk->m_chunkBuf->m_vertexBuf->m_hasData = 0;
    }
}

static void UpdateNormals(CMapChunk* chunk) {
    const float e = s_unitSize * 0.5f;

    for (int32_t i = 0; i < 145; i++) {
        C3Vector pos;
        chunk->GetVertexPosition(i, &pos);

        float x0, x1, y0, y1;

        if (!GetHeight(pos.x - e, pos.y, &x0)) {
            x0 = pos.z;
        }

        if (!GetHeight(pos.x + e, pos.y, &x1)) {
            x1 = pos.z;
        }

        if (!GetHeight(pos.x, pos.y - e, &y0)) {
            y0 = pos.z;
        }

        if (!GetHeight(pos.x, pos.y + e, &y1)) {
            y1 = pos.z;
        }

        C3Vector n(x0 - x1, y0 - y1, 2.0f * e);
        float length = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
        n.x /= length;
        n.y /= length;
        n.z /= length;

        chunk->m_normals[3 * i + 0] = static_cast<int8_t>(lroundf(n.x * 127.0f));
        chunk->m_normals[3 * i + 1] = static_cast<int8_t>(lroundf(n.y * 127.0f));
        chunk->m_normals[3 * i + 2] = static_cast<int8_t>(lroundf(n.z * 127.0f));
    }
}

static void RefreshChunks(int32_t minRow, int32_t maxRow, int32_t minCol, int32_t maxCol) {
    for (int32_t row = minRow; row <= maxRow; row++) {
        for (int32_t col = minCol; col <= maxCol; col++) {
            CMapChunk* chunk = GetChunk(row, col);

            if (chunk) {
                UpdateNormals(chunk);
                UpdateBounds(chunk);
            }
        }
    }
}

static float BlurTarget(const C3Vector& pos, const C3Vector& vertex) {
    float radius = s_flattenBlur.m_radius;
    int32_t steps = std::max(static_cast<int32_t>(radius / s_unitSize), 1);
    float totalHeight = 0.0f;
    float totalWeight = 0.0f;

    for (int32_t j = -steps * 2; j <= steps * 2; j++) {
        float ty = pos.y + j * s_unitSize * 0.5f;

        for (int32_t k = -steps; k <= steps; k++) {
            float tx = pos.x + k * s_unitSize + (j % 2) * s_unitSize * 0.5f;
            float dx = tx - vertex.x;
            float dy = ty - vertex.y;
            float dist = sqrtf(dx * dx + dy * dy);

            if (dist > radius) {
                continue;
            }

            float h;

            if (GetHeight(tx, ty, &h)) {
                totalHeight += (1.0f - dist / radius) * h;
                totalWeight += 1.0f - dist / radius;
            }
        }
    }

    return totalWeight > 0.0f ? totalHeight / totalWeight : vertex.z;
}

void Apply(OPERATION op, const C3Vector& pos, float elapsedSec) {
    if (elapsedSec <= 0.0f) {
        return;
    }

    if (op == OP_BLUR && s_flattenBlur.m_type == FLATTEN_ORIGIN) {
        return;
    }

    if (!s_stroking) {
        s_stroking = true;
        EditorUndo::BeginStep();
    }

    bool raiseLower = op == OP_RAISE || op == OP_LOWER;
    float radius = raiseLower ? s_raiseLower.m_radius : s_flattenBlur.m_radius;

    if (radius <= 0.0f) {
        return;
    }

    int32_t minRow = ChunkCoord(pos.x + radius);
    int32_t maxRow = ChunkCoord(pos.x - radius);
    int32_t minCol = ChunkCoord(pos.y + radius);
    int32_t maxCol = ChunkCoord(pos.y - radius);
    float change = 7.5f * elapsedSec * s_raiseLower.m_speed * (op == OP_LOWER ? -1.0f : 1.0f);
    float remain = 1.0f - powf(0.5f, elapsedSec * s_flattenBlur.m_speed);
    C3Vector origin = s_flattenBlur.m_lock ? s_flattenBlur.m_lockPos : pos;
    float orientation = s_flattenBlur.m_orientation * 0.017453292f;
    float slope = s_flattenBlur.m_angled ? tanf(s_flattenBlur.m_angle * 0.017453292f) : 0.0f;

    s_edits.clear();

    for (int32_t row = minRow; row <= maxRow; row++) {
        for (int32_t col = minCol; col <= maxCol; col++) {
            CMapChunk* chunk = GetChunk(row, col);

            if (!chunk) {
                continue;
            }

            bool recorded = false;

            for (int32_t i = 0; i < 145; i++) {
                C3Vector vertex;
                chunk->GetVertexPosition(i, &vertex);
                float dx = vertex.x - pos.x;
                float dy = vertex.y - pos.y;
                float height = vertex.z;

                if (raiseLower) {
                    float dt = change;

                    if (!RaiseLowerChange(dx, dy, &dt)) {
                        continue;
                    }

                    height += dt;
                } else {
                    float dist = sqrtf(dx * dx + dy * dy);

                    if (dist >= radius) {
                        continue;
                    }

                    float target;

                    if (op == OP_FLATTEN) {
                        target = origin.z + ((vertex.x - origin.x) * cosf(orientation) + (vertex.y - origin.y) * sinf(orientation)) * slope;
                    } else {
                        target = BlurTarget(pos, vertex);
                    }

                    if ((!s_flattenBlur.m_lower && target < height) || (!s_flattenBlur.m_raise && target > height)) {
                        continue;
                    }

                    if (op == OP_FLATTEN && s_flattenBlur.m_type == FLATTEN_ORIGIN) {
                        height = origin.z;
                    } else {
                        height = height + (target - height) * FlattenBlend(dist, remain);
                    }
                }

                if (!recorded) {
                    EditorUndo::RecordHeight(row, col);
                    recorded = true;
                }

                VertexEdit edit = { chunk, i, height - chunk->m_topLeftCoords.z };
                s_edits.push_back(edit);
            }
        }
    }

    if (s_edits.empty()) {
        return;
    }

    for (const VertexEdit& edit : s_edits) {
        edit.m_chunk->m_height[edit.m_index] = edit.m_height;
    }

    RefreshChunks(minRow - 1, maxRow + 1, minCol - 1, maxCol + 1);
}

void EndStroke() {
    if (!s_stroking) {
        return;
    }

    s_stroking = false;
    EditorUndo::EndStep();
}

bool IsStroking() {
    return s_stroking;
}

bool CaptureHeights(int32_t row, int32_t col, EditorUndo::HeightSnapshot& snapshot) {
    CMapChunk* chunk = GetChunk(row, col);

    if (!chunk) {
        return false;
    }

    snapshot.m_row = row;
    snapshot.m_col = col;
    memcpy(snapshot.m_height, chunk->m_height, sizeof(snapshot.m_height));
    memcpy(snapshot.m_normals, chunk->m_normals, sizeof(snapshot.m_normals));
    return true;
}

void RestoreHeights(const EditorUndo::HeightSnapshot& snapshot) {
    CMapChunk* chunk = GetChunk(snapshot.m_row, snapshot.m_col);

    if (!chunk) {
        return;
    }

    memcpy(chunk->m_height, snapshot.m_height, sizeof(snapshot.m_height));
    memcpy(chunk->m_normals, snapshot.m_normals, sizeof(snapshot.m_normals));
    UpdateBounds(chunk);
}

static void DrawCircle(const C3Vector& pos, float radius, CImVector color) {
    const uint32_t segments = 64;
    CGxBuf* buf = g_theGxDevicePtr->BufStream(GxPoolTarget_Vertex, sizeof(CGxVertexPC), segments + 1);
    CGxVertexPC* vertices = reinterpret_cast<CGxVertexPC*>(g_theGxDevicePtr->BufLock(buf));
    GxFormatColor(color);

    for (uint32_t i = 0; i <= segments; i++) {
        float angle = i * 6.2831855f / segments;
        float x = pos.x + cosf(angle) * radius;
        float y = pos.y + sinf(angle) * radius;
        float z = pos.z;
        GetHeight(x, y, &z);
        vertices[i].p = C3Vector(x, y, z + 0.15f);
        vertices[i].c = color;
    }

    GxBufUnlock(buf, 0);
    GxPrimVertexPtr(buf, GxVBF_PC);

    CGxBatch batch;
    batch.m_primType = GxPrim_LineStrip;
    batch.m_start = 0;
    batch.m_count = segments + 1;
    batch.m_minIndex = 0;
    batch.m_maxIndex = segments;
    g_theGxDevicePtr->Draw(&batch, 0);
}

void DrawBrush(const C3Vector& pos, const C3Vector& camPos) {
    g_theGxDevicePtr->RsPush();
    GxRsSet(GxRs_Fog, 0);
    GxRsSet(GxRs_Lighting, 0);
    GxRsSet(GxRs_VertexShader, nullptr);
    GxRsSet(GxRs_PixelShader, nullptr);
    GxRsSet(GxRs_Texture0, nullptr);
    GxRsSet(GxRs_DepthTest, 0);
    GxRsSet(GxRs_DepthWrite, 0);

    C44Matrix world;
    world.Translate(C3Vector(-camPos.x, -camPos.y, -camPos.z));
    GxXformPush(GxXform_World, world);

    CImVector outer;
    outer.r = 0xFF;
    outer.g = 0xFF;
    outer.b = 0xFF;
    outer.a = 0xFF;

    float radius = GetRadius();

    if (radius > 0.0f) {
        DrawCircle(pos, radius, outer);
    }

    float innerRadius = s_mode == MODE_RAISE_LOWER ? s_raiseLower.m_innerRadius : (s_mode == MODE_TEXTURE_PAINT ? TextureEdit::s_brush.m_hardness : 0.0f);

    if (innerRadius > 0.0f) {
        CImVector inner;
        inner.r = 0x52;
        inner.g = 0x81;
        inner.b = 0xB9;
        inner.a = 0xFF;
        DrawCircle(pos, radius * innerRadius, inner);
    }

    GxXformPop(GxXform_World);
    g_theGxDevicePtr->RsPop();
}

void Reset() {
    s_stroking = false;
    s_edits.clear();
}

}
