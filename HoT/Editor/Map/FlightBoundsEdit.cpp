#include "HoT/Editor/Map/FlightBoundsEdit.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
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
#include "HoT/Editor/Map/EditorSave.h"
#include "HoT/Features/VoidFloors.h"

namespace HoT::Editor::Map::FlightBoundsEdit {

static const float s_origin = 17066.666f;
static const float s_tileSize = 533.33333f;
static const float s_step = 533.33333f / 2.0f;
static const uint32_t TOKEN_MHDR = 0x4D484452;
static const uint32_t TOKEN_MFBO = 0x4D46424F;
// A new chunk starts where a tile without MFBO already behaves: no void floor, no ceiling.
static const int16_t s_defaultLower = -500;
static const int16_t s_defaultUpper = 32767;
// The client fans 8 triangles around vertex 4, the same order CMap::CreateFlightBoundsFacets uses.
static const int32_t s_triangles[24] = { 3, 0, 4, 0, 1, 4, 1, 2, 4, 2, 5, 4, 5, 8, 4, 8, 7, 4, 7, 6, 4, 6, 3, 4 };

Settings s_settings = { LAYER_LOWER, 600, 0.0f, false, 10.0f, false };

static CMapArea* LoadedArea(int32_t areaX, int32_t areaY) {
    if (areaX < 0 || areaX > 63 || areaY < 0 || areaY > 63) {
        return nullptr;
    }

    CMapArea* area = CMap::m_areaTable[64 * areaY + areaX];
    return area && !area->m_asyncObject && area->m_fileBuffer && area->m_header ? area : nullptr;
}

static CMapArea* AreaAt(float x, float y) {
    int32_t areaX = static_cast<int32_t>(floorf((s_origin - y) / s_tileSize));
    int32_t areaY = static_cast<int32_t>(floorf((s_origin - x) / s_tileSize));
    return LoadedArea(areaX, areaY);
}

static C3Vector VertexPosition(CMapArea* area, int32_t index, float z) {
    return C3Vector(area->m_topLeft2.x - (index / 3) * s_step, area->m_topLeft2.y - (index % 3) * s_step, z);
}

static uint32_t ReadU32(const uint8_t* data) {
    uint32_t value;
    memcpy(&value, data, 4);
    return value;
}

static void PutU32(std::vector<uint8_t>& out, uint32_t value) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
    out.insert(out.end(), bytes, bytes + 4);
}

// Appends an MFBO chunk and reloads the tile, keeping its unsaved edits.
static bool EnsureChunk(CMapArea* area) {
    if (area->m_flyingBbox) {
        return true;
    }

    std::vector<uint8_t> data;

    if (!EditorSave::BuildTile(area, data)) {
        return false;
    }

    size_t mhdr = 0;

    for (size_t offset = 0; offset + 8 <= data.size(); offset += 8 + ReadU32(&data[offset + 4])) {
        if (ReadU32(&data[offset]) == TOKEN_MHDR) {
            mhdr = offset + 8;
            break;
        }
    }

    if (!mhdr) {
        return false;
    }

    size_t at = data.size();
    PutU32(data, TOKEN_MFBO);
    PutU32(data, 36);

    for (int32_t i = 0; i < 18; i++) {
        int16_t value = i < 9 ? s_defaultUpper : s_defaultLower;
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
        data.insert(data.end(), bytes, bytes + 2);
    }

    uint32_t flags = ReadU32(&data[mhdr + offsetof(SMAreaHeader, m_flags)]) | 0x1;
    uint32_t relative = static_cast<uint32_t>(at - mhdr);
    memcpy(&data[mhdr + offsetof(SMAreaHeader, m_flags)], &flags, 4);
    memcpy(&data[mhdr + offsetof(SMAreaHeader, m_mfbo)], &relative, 4);

    return EditorSave::ReloadArea(area, data) && area->m_flyingBbox;
}

static uint32_t AreaIndex(CMapArea* area) {
    return static_cast<uint32_t>(64 * area->m_index.y + area->m_index.x);
}

static void SetVertex(CMapArea* area, int32_t index, int32_t layer, int32_t height) {
    if (!EnsureChunk(area)) {
        return;
    }

    int16_t value = static_cast<int16_t>(std::clamp(height, -32768, 32767));
    int16_t& slot = area->m_flyingBbox[layer == LAYER_UPPER ? index : 9 + index];

    if (slot == value) {
        return;
    }

    EditorUndo::RecordFlight(AreaIndex(area));
    slot = value;
}

// Every loaded tile sharing global vertex (gx, gy), gx runs along world X and gy along world Y.
template <typename F>
static void ForEachShared(int32_t gx, int32_t gy, F func) {
    for (int32_t areaY = gx / 2 - 1; areaY <= gx / 2; areaY++) {
        int32_t row = gx - 2 * areaY;

        if (row < 0 || row > 2) {
            continue;
        }

        for (int32_t areaX = gy / 2 - 1; areaX <= gy / 2; areaX++) {
            int32_t col = gy - 2 * areaX;

            if (col < 0 || col > 2) {
                continue;
            }

            if (CMapArea* area = LoadedArea(areaX, areaY)) {
                func(area, row * 3 + col);
            }
        }
    }
}

static void NearestVertex(const C3Vector& pos, int32_t* gx, int32_t* gy) {
    *gx = static_cast<int32_t>(lroundf((s_origin - pos.x) / s_step));
    *gy = static_cast<int32_t>(lroundf((s_origin - pos.y) / s_step));
}

// Calls func(gx, gy) for the vertices the brush covers.
template <typename F>
static void ForEachBrushVertex(const C3Vector& cursor, F func) {
    int32_t gx;
    int32_t gy;
    NearestVertex(cursor, &gx, &gy);
    float radius = s_settings.m_radius;

    if (radius < 1.0f) {
        func(gx, gy);
        return;
    }

    int32_t reach = static_cast<int32_t>(ceilf(radius / s_step)) + 1;

    for (int32_t x = gx - reach; x <= gx + reach; x++) {
        for (int32_t y = gy - reach; y <= gy + reach; y++) {
            float dx = s_origin - x * s_step - cursor.x;
            float dy = s_origin - y * s_step - cursor.y;

            if (dx * dx + dy * dy <= radius * radius) {
                func(x, y);
            }
        }
    }
}

int32_t StrokeHeight(const C3Vector& cursor) {
    if (s_settings.m_fromCursor) {
        return static_cast<int32_t>(floorf(cursor.z - s_settings.m_offset));
    }

    return s_settings.m_height;
}

void Paint(const C3Vector& cursor) {
    int32_t height = StrokeHeight(cursor);

    ForEachBrushVertex(cursor, [&](int32_t gx, int32_t gy) {
        ForEachShared(gx, gy, [&](CMapArea* area, int32_t index) {
            SetVertex(area, index, s_settings.m_layer, height);
        });
    });
}

bool PickHeight(const C3Vector& cursor) {
    int32_t gx;
    int32_t gy;
    NearestVertex(cursor, &gx, &gy);
    bool picked = false;

    ForEachShared(gx, gy, [&](CMapArea* area, int32_t index) {
        if (!picked && area->m_flyingBbox) {
            s_settings.m_height = area->m_flyingBbox[s_settings.m_layer == LAYER_UPPER ? index : 9 + index];
            s_settings.m_fromCursor = false;
            picked = true;
        }
    });

    return picked;
}

static void FillArea(CMapArea* area, int32_t height) {
    for (int32_t i = 0; i < 9; i++) {
        SetVertex(area, i, s_settings.m_layer, height);
    }
}

void FillTile(const C3Vector& pos) {
    CMapArea* area = AreaAt(pos.x, pos.y);

    if (area) {
        FillArea(area, s_settings.m_height);
        EndStroke();
    }
}

void FillLoaded() {
    for (int32_t areaY = 0; areaY < 64; areaY++) {
        for (int32_t areaX = 0; areaX < 64; areaX++) {
            if (CMapArea* area = LoadedArea(areaX, areaY)) {
                FillArea(area, s_settings.m_height);
            }
        }
    }

    EndStroke();
}

// Height of one layer at pos, the client's triangle interpolation.
static float LayerHeight(CMapArea* area, const int16_t* heights, float x, float y) {
    float u = (area->m_topLeft2.x - x) / s_step;
    float v = (area->m_topLeft2.y - y) / s_step;

    for (int32_t tri = 0; tri < 24; tri += 3) {
        int32_t a = s_triangles[tri];
        int32_t b = s_triangles[tri + 1];
        int32_t c = s_triangles[tri + 2];
        float au = static_cast<float>(a / 3), av = static_cast<float>(a % 3);
        float bu = static_cast<float>(b / 3), bv = static_cast<float>(b % 3);
        float cu = static_cast<float>(c / 3), cv = static_cast<float>(c % 3);
        float det = (bv - cv) * (au - cu) + (cu - bu) * (av - cv);
        float wa = ((bv - cv) * (u - cu) + (cu - bu) * (v - cv)) / det;
        float wb = ((cv - av) * (u - cu) + (au - cu) * (v - cv)) / det;
        float wc = 1.0f - wa - wb;

        if (wa >= -0.0001f && wb >= -0.0001f && wc >= -0.0001f) {
            return wa * heights[a] + wb * heights[b] + wc * heights[c];
        }
    }

    return heights[4];
}

bool GetInfo(const C3Vector& pos, Info* info) {
    CMapArea* area = AreaAt(pos.x, pos.y);

    if (!area) {
        return false;
    }

    info->m_tileX = area->m_index.x;
    info->m_tileY = area->m_index.y;
    info->m_hasChunk = area->m_flyingBbox != nullptr;
    info->m_lowerAt = s_defaultLower;
    info->m_upperAt = s_defaultUpper;
    info->m_vertexLower = s_defaultLower;
    info->m_vertexUpper = s_defaultUpper;

    if (!info->m_hasChunk) {
        return true;
    }

    const int16_t* bounds = area->m_flyingBbox;
    info->m_upperAt = LayerHeight(area, bounds, pos.x, pos.y);
    info->m_lowerAt = LayerHeight(area, bounds + 9, pos.x, pos.y);

    int32_t gx;
    int32_t gy;
    NearestVertex(pos, &gx, &gy);
    int32_t row = std::clamp(gx - 2 * area->m_index.y, 0, 2);
    int32_t col = std::clamp(gy - 2 * area->m_index.x, 0, 2);
    info->m_vertexUpper = bounds[row * 3 + col];
    info->m_vertexLower = bounds[9 + row * 3 + col];
    return true;
}

void EndStroke() {
    if (EditorUndo::IsOpen()) {
        EditorUndo::EndStep();
    }
}

bool Capture(uint32_t areaIndex, EditorUndo::FlightSnapshot& snapshot) {
    CMapArea* area = LoadedArea(static_cast<int32_t>(areaIndex % 64), static_cast<int32_t>(areaIndex / 64));

    if (!area || !area->m_flyingBbox) {
        return false;
    }

    snapshot.m_area = areaIndex;
    memcpy(snapshot.m_heights, area->m_flyingBbox, sizeof(snapshot.m_heights));
    return true;
}

void Restore(const EditorUndo::FlightSnapshot& snapshot) {
    CMapArea* area = LoadedArea(static_cast<int32_t>(snapshot.m_area % 64), static_cast<int32_t>(snapshot.m_area / 64));

    if (area && EnsureChunk(area)) {
        memcpy(area->m_flyingBbox, snapshot.m_heights, sizeof(snapshot.m_heights));
    }
}

static std::vector<CGxVertexPC> s_fill;
static std::vector<CGxVertexPC> s_lines;

static void Flush(std::vector<CGxVertexPC>& vertices, EGxPrim prim, uint32_t unit) {
    const uint32_t batchSize = 6000 - 6000 % unit;

    for (size_t start = 0; start < vertices.size(); start += batchSize) {
        uint32_t count = static_cast<uint32_t>(std::min<size_t>(batchSize, vertices.size() - start));
        CGxBuf* buf = g_theGxDevicePtr->BufStream(GxPoolTarget_Vertex, sizeof(CGxVertexPC), count);
        CGxVertexPC* dst = reinterpret_cast<CGxVertexPC*>(g_theGxDevicePtr->BufLock(buf));
        memcpy(dst, &vertices[start], count * sizeof(CGxVertexPC));
        GxBufUnlock(buf, 0);
        GxPrimVertexPtr(buf, GxVBF_PC);

        CGxBatch batch;
        batch.m_primType = prim;
        batch.m_start = 0;
        batch.m_count = count;
        batch.m_minIndex = 0;
        batch.m_maxIndex = count - 1;
        g_theGxDevicePtr->Draw(&batch, 0);
    }

    vertices.clear();
}

static CImVector Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    CImVector color;
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = a;
    GxFormatColor(color);
    return color;
}

static void Push(std::vector<CGxVertexPC>& out, const C3Vector& p, CImVector color) {
    CGxVertexPC vertex;
    vertex.p = p;
    vertex.c = color;
    out.push_back(vertex);
}

static void AddPlane(CMapArea* area, const int16_t* heights, CImVector fill, CImVector line) {
    for (int32_t tri = 0; tri < 24; tri += 3) {
        C3Vector p[3];

        for (int32_t k = 0; k < 3; k++) {
            int32_t index = s_triangles[tri + k];
            p[k] = VertexPosition(area, index, heights[index]);
            Push(s_fill, p[k], fill);
        }

        // Outer edge and the spoke into the centre, each edge drawn once per tile.
        Push(s_lines, p[0], line);
        Push(s_lines, p[1], line);
        Push(s_lines, p[0], line);
        Push(s_lines, p[2], line);
    }
}

static void AddMarker(const C3Vector& p, float size, CImVector color) {
    Push(s_lines, C3Vector(p.x, p.y, p.z - size), color);
    Push(s_lines, C3Vector(p.x, p.y, p.z + size), color);
    Push(s_lines, C3Vector(p.x - size, p.y, p.z), color);
    Push(s_lines, C3Vector(p.x + size, p.y, p.z), color);
    Push(s_lines, C3Vector(p.x, p.y - size, p.z), color);
    Push(s_lines, C3Vector(p.x, p.y + size, p.z), color);
}

void DrawOverlay(const C3Vector& camPos, const C3Vector* cursor) {
    CImVector lowerFill = Color(0xFF, 0x40, 0x40, 0x30);
    CImVector lowerLine = Color(0xFF, 0x70, 0x70, 0xD0);
    CImVector upperFill = Color(0x40, 0x80, 0xFF, 0x28);
    CImVector upperLine = Color(0x70, 0xA0, 0xFF, 0xD0);
    CImVector target = Color(0xFF, 0xE0, 0x40, 0xFF);

    int32_t centerX = static_cast<int32_t>(floorf((s_origin - camPos.y) / s_tileSize));
    int32_t centerY = static_cast<int32_t>(floorf((s_origin - camPos.x) / s_tileSize));
    const int32_t range = 2;

    for (int32_t areaY = centerY - range; areaY <= centerY + range; areaY++) {
        for (int32_t areaX = centerX - range; areaX <= centerX + range; areaX++) {
            CMapArea* area = LoadedArea(areaX, areaY);

            if (!area || !area->m_flyingBbox) {
                continue;
            }

            const int16_t* bounds = area->m_flyingBbox;
            AddPlane(area, bounds + 9, lowerFill, lowerLine);

            if (s_settings.m_showUpper) {
                AddPlane(area, bounds, upperFill, upperLine);
            }
        }
    }

    // Client side void floors (HoT/Features/VoidFloors.cpp) as filled discs.
    CImVector discFill = Color(0xFF, 0x90, 0x30, 0x40);
    CImVector discLine = Color(0xFF, 0xB0, 0x50, 0xE0);
    float discX, discY, discRadius, discFloor;

    for (uint32_t i = 0; VoidFloors::GetCurrent(i, &discX, &discY, &discRadius, &discFloor); i++) {
        if (discRadius <= 0.0f) {
            // Whole map floor as a grid round the camera, excluded cells left as holes.
            const float cell = 8.0f;
            const int32_t reach = 60;
            float baseX = floorf(camPos.x / cell) * cell;
            float baseY = floorf(camPos.y / cell) * cell;

            for (int32_t gx = -reach; gx < reach; gx++) {
                for (int32_t gy = -reach; gy < reach; gy++) {
                    float x0 = baseX + gx * cell;
                    float y0 = baseY + gy * cell;

                    if (VoidFloors::IsExcluded(x0 + cell * 0.5f, y0 + cell * 0.5f)) {
                        continue;
                    }

                    C3Vector a(x0, y0, discFloor);
                    C3Vector b(x0 + cell, y0, discFloor);
                    C3Vector c(x0 + cell, y0 + cell, discFloor);
                    C3Vector d(x0, y0 + cell, discFloor);
                    Push(s_fill, a, discFill);
                    Push(s_fill, b, discFill);
                    Push(s_fill, c, discFill);
                    Push(s_fill, a, discFill);
                    Push(s_fill, c, discFill);
                    Push(s_fill, d, discFill);
                    // Grid lines every fourth cell, by world cell index so they do not crawl with the camera.
                    if ((static_cast<int32_t>(floorf(y0 / cell)) & 3) == 0) {
                        Push(s_lines, a, discLine);
                        Push(s_lines, b, discLine);
                    }

                    if ((static_cast<int32_t>(floorf(x0 / cell)) & 3) == 0) {
                        Push(s_lines, a, discLine);
                        Push(s_lines, d, discLine);
                    }
                }
            }

            // Exclusion outlines at the floor height.
            CImVector edge = Color(0x50, 0xFF, 0x70, 0xFF);
            float px, py, firstX, firstY, lastX, lastY;

            for (uint32_t n = 0; VoidFloors::GetExclusionPoint(n, 0, &firstX, &firstY); n++) {
                lastX = firstX;
                lastY = firstY;

                for (uint32_t p = 1; VoidFloors::GetExclusionPoint(n, p, &px, &py); p++) {
                    Push(s_lines, C3Vector(lastX, lastY, discFloor), edge);
                    Push(s_lines, C3Vector(px, py, discFloor), edge);
                    lastX = px;
                    lastY = py;
                }

                Push(s_lines, C3Vector(lastX, lastY, discFloor), edge);
                Push(s_lines, C3Vector(firstX, firstY, discFloor), edge);
            }

            continue;
        }

        const int32_t segments = 48;

        for (int32_t s = 0; s < segments; s++) {
            float a0 = s * 6.2831855f / segments;
            float a1 = (s + 1) * 6.2831855f / segments;
            C3Vector p0(discX + cosf(a0) * discRadius, discY + sinf(a0) * discRadius, discFloor);
            C3Vector p1(discX + cosf(a1) * discRadius, discY + sinf(a1) * discRadius, discFloor);
            Push(s_fill, C3Vector(discX, discY, discFloor), discFill);
            Push(s_fill, p0, discFill);
            Push(s_fill, p1, discFill);
            Push(s_lines, p0, discLine);
            Push(s_lines, p1, discLine);
        }
    }

    if (cursor) {
        int32_t layer = s_settings.m_layer;

        ForEachBrushVertex(*cursor, [&](int32_t gx, int32_t gy) {
            float x = s_origin - gx * s_step;
            float y = s_origin - gy * s_step;
            CMapArea* area = AreaAt(x - 1.0f, y - 1.0f);
            float z = static_cast<float>(StrokeHeight(*cursor));

            if (!area) {
                area = AreaAt(x + 1.0f, y + 1.0f);
            }

            if (area && area->m_flyingBbox) {
                int32_t row = std::clamp(gx - 2 * area->m_index.y, 0, 2);
                int32_t col = std::clamp(gy - 2 * area->m_index.x, 0, 2);
                z = area->m_flyingBbox[layer == LAYER_UPPER ? row * 3 + col : 9 + row * 3 + col];
            }

            AddMarker(C3Vector(x, y, z), 12.0f, target);
            Push(s_lines, C3Vector(x, y, z), target);
            Push(s_lines, C3Vector(x, y, static_cast<float>(StrokeHeight(*cursor))), target);
        });
    }

    if (s_fill.empty() && s_lines.empty()) {
        return;
    }

    g_theGxDevicePtr->RsPush();
    GxRsSet(GxRs_Fog, 0);
    GxRsSet(GxRs_Lighting, 0);
    GxRsSet(GxRs_VertexShader, nullptr);
    GxRsSet(GxRs_PixelShader, nullptr);
    GxRsSet(GxRs_Texture0, nullptr);
    GxRsSet(GxRs_DepthTest, 0);
    GxRsSet(GxRs_DepthWrite, 0);
    GxRsSet(GxRs_Culling, 0);
    GxRsSet(GxRs_BlendingMode, GxBlend_Alpha);

    C44Matrix world;
    world.Translate(C3Vector(-camPos.x, -camPos.y, -camPos.z));
    GxXformPush(GxXform_World, world);

    Flush(s_fill, GxPrim_Triangles, 3);
    Flush(s_lines, GxPrim_Lines, 2);

    GxXformPop(GxXform_World);
    g_theGxDevicePtr->RsPop();
}

}
