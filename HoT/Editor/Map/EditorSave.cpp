#include "HoT/Editor/Map/EditorSave.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#if defined(_WIN32)
#include <windows.h>
#else
#include <filesystem>
#include "Engine/Source/Os/Linux/OsPath.h"
#endif
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "WoW/Source/WorldClient/Map.h"
#include "WoW/Source/WorldClient/MapArea.h"
#include "WoW/Source/WorldClient/MapChunk.h"
#include "WoW/Source/WorldClient/Types.h"
#include "HoT/Editor/Map/ChunkEdit.h"
#include "HoT/Editor/Map/EditorUndo.h"
#include "Engine/Source/Base/Handle.h"
#include "WoW/Source/WorldClient/MapDoodadDef.h"
#include "WoW/Source/WorldClient/MapObjDef.h"
#include "HoT/Editor/Map/TerrainEdit.h"
#include "HoT/Editor/Map/ObjectEdit.h"
#include "HoT/Editor/Map/TextureEdit.h"
#include "HoT/Editor/Map/Runtime.h"
#include "HoT/Streaming/LooseFiles.h"

namespace HoT::Editor::Map::EditorSave {

static const uint32_t TOKEN_MVER = 0x4D564552;
static const uint32_t TOKEN_MHDR = 0x4D484452;
static const uint32_t TOKEN_MCIN = 0x4D43494E;
static const uint32_t TOKEN_MTEX = 0x4D544558;
static const uint32_t TOKEN_MMDX = 0x4D4D4458;
static const uint32_t TOKEN_MMID = 0x4D4D4944;
static const uint32_t TOKEN_MWMO = 0x4D574D4F;
static const uint32_t TOKEN_MWID = 0x4D574944;
static const uint32_t TOKEN_MDDF = 0x4D444446;
static const uint32_t TOKEN_MODF = 0x4D4F4446;
static const uint32_t TOKEN_MFBO = 0x4D46424F;
static const uint32_t TOKEN_MH2O = 0x4D48324F;
static const uint32_t TOKEN_MTXF = 0x4D545846;
static const uint32_t TOKEN_MCNK = 0x4D434E4B;
static const uint32_t TOKEN_MCVT = 0x4D435654;
static const uint32_t TOKEN_MCCV = 0x4D434356;
static const uint32_t TOKEN_MCNR = 0x4D434E52;
static const uint32_t TOKEN_MCLY = 0x4D434C59;
static const uint32_t TOKEN_MCRF = 0x4D435246;
static const uint32_t TOKEN_MCSH = 0x4D435348;
static const uint32_t TOKEN_MCAL = 0x4D43414C;
static const uint32_t TOKEN_MCLQ = 0x4D434C51;
static const uint32_t TOKEN_MCSE = 0x4D435345;

static const uint32_t MCNR_SIZE = 435;
static const uint32_t MCNR_PADDED = 448;

static uint32_t ReadU32(const uint8_t* data) {
    uint32_t value;
    memcpy(&value, data, 4);
    return value;
}

static void PutU32(std::vector<uint8_t>& out, uint32_t value) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
    out.insert(out.end(), bytes, bytes + 4);
}

static void SetU32(std::vector<uint8_t>& out, size_t offset, uint32_t value) {
    memcpy(&out[offset], &value, 4);
}

static size_t BeginChunk(std::vector<uint8_t>& out, uint32_t token) {
    size_t start = out.size();
    PutU32(out, token);
    PutU32(out, 0);
    return start;
}

static void EndChunk(std::vector<uint8_t>& out, size_t start) {
    SetU32(out, start + 4, static_cast<uint32_t>(out.size() - start - 8));
}

static void PutChunk(std::vector<uint8_t>& out, uint32_t token, const uint8_t* data, uint32_t size, uint32_t sizeField) {
    PutU32(out, token);
    PutU32(out, sizeField);

    if (size) {
        out.insert(out.end(), data, data + size);
    }
}

static const uint8_t* SubChunk(const uint8_t* mcnk, uint32_t offset, uint32_t token, uint32_t end) {
    if (!offset || offset + 8 > end || ReadU32(mcnk + offset) != token) {
        return nullptr;
    }

    return mcnk + offset;
}

struct ObjectTables {
    bool m_active = false;
    std::vector<std::string> m_doodadNames;
    std::vector<std::string> m_mapObjNames;
    std::vector<SMDoodadDef> m_doodads;
    std::vector<SMMapObjDef> m_mapObjs;
    std::vector<int32_t> m_doodadRemap;
    std::vector<int32_t> m_mapObjRemap;
    std::vector<std::vector<uint32_t>> m_addedDoodadRefs;
    std::vector<std::vector<uint32_t>> m_addedMapObjRefs;
};

static void ReadNames(const uint8_t* blob, uint32_t blobSize, const uint8_t* offsets, uint32_t offsetsSize, std::vector<std::string>& names) {
    for (uint32_t i = 0; i + 4 <= offsetsSize; i += 4) {
        uint32_t offset = ReadU32(offsets + i);
        std::string name;

        for (uint32_t k = offset; blob && k < blobSize && blob[k]; k++) {
            name += static_cast<char>(blob[k]);
        }

        names.push_back(name);
    }
}

static uint32_t NameIndex(std::vector<std::string>& names, const std::string& name) {
    for (uint32_t i = 0; i < names.size(); i++) {
        if (_stricmp(names[i].c_str(), name.c_str()) == 0) {
            return i;
        }
    }

    names.push_back(name);
    return static_cast<uint32_t>(names.size() - 1);
}

static uint32_t ChunkIndexOf(const C3Vector& filePos) {
    int32_t row = static_cast<int32_t>(filePos.z / 33.333332f) & 15;
    int32_t col = static_cast<int32_t>(filePos.x / 33.333332f) & 15;
    return static_cast<uint32_t>(col + 16 * row);
}

static void BuildObjectTables(const uint8_t* buffer, uint32_t size, const ObjectChanges* changes, ObjectTables& tables) {
    tables.m_addedDoodadRefs.assign(256, std::vector<uint32_t>());
    tables.m_addedMapObjRefs.assign(256, std::vector<uint32_t>());

    if (!changes || (changes->m_removedDoodads.empty() && changes->m_removedMapObjs.empty() && changes->m_added.empty())) {
        return;
    }

    tables.m_active = true;
    const uint8_t* chunks[6] = {};
    uint32_t sizes[6] = {};
    const uint32_t tokens[6] = { TOKEN_MMDX, TOKEN_MMID, TOKEN_MWMO, TOKEN_MWID, TOKEN_MDDF, TOKEN_MODF };

    for (uint32_t offset = 0; offset + 8 <= size;) {
        uint32_t token = ReadU32(buffer + offset);
        uint32_t chunkSize = ReadU32(buffer + offset + 4);

        for (int32_t i = 0; i < 6; i++) {
            if (token == tokens[i]) {
                chunks[i] = buffer + offset + 8;
                sizes[i] = chunkSize;
            }
        }

        if (token == TOKEN_MCNK) {
            break;
        }

        offset += 8 + chunkSize;
    }

    ReadNames(chunks[0], sizes[0], chunks[1], sizes[1], tables.m_doodadNames);
    ReadNames(chunks[2], sizes[2], chunks[3], sizes[3], tables.m_mapObjNames);

    for (uint32_t i = 0; i * sizeof(SMDoodadDef) < sizes[4]; i++) {
        SMDoodadDef def;
        memcpy(&def, chunks[4] + i * sizeof(SMDoodadDef), sizeof(def));
        bool removed = std::find(changes->m_removedDoodads.begin(), changes->m_removedDoodads.end(), def.m_uniqueId) != changes->m_removedDoodads.end();
        tables.m_doodadRemap.push_back(removed ? -1 : static_cast<int32_t>(tables.m_doodads.size()));

        if (!removed) {
            tables.m_doodads.push_back(def);
        }
    }

    for (uint32_t i = 0; i * sizeof(SMMapObjDef) < sizes[5]; i++) {
        SMMapObjDef def;
        memcpy(&def, chunks[5] + i * sizeof(SMMapObjDef), sizeof(def));
        bool removed = std::find(changes->m_removedMapObjs.begin(), changes->m_removedMapObjs.end(), def.m_uniqueId) != changes->m_removedMapObjs.end();
        tables.m_mapObjRemap.push_back(removed ? -1 : static_cast<int32_t>(tables.m_mapObjs.size()));

        if (!removed) {
            tables.m_mapObjs.push_back(def);
        }
    }

    for (const AddedObject& added : changes->m_added) {
        if (added.m_kind == ObjectEdit::KIND_DOODAD) {
            SMDoodadDef def = added.m_doodad;
            def.m_nameId = NameIndex(tables.m_doodadNames, added.m_fileName);
            tables.m_addedDoodadRefs[ChunkIndexOf(def.m_position)].push_back(static_cast<uint32_t>(tables.m_doodads.size()));
            tables.m_doodads.push_back(def);
        } else {
            SMMapObjDef def = added.m_mapObj;
            def.m_nameId = NameIndex(tables.m_mapObjNames, added.m_fileName);
            tables.m_addedMapObjRefs[ChunkIndexOf(def.m_position)].push_back(static_cast<uint32_t>(tables.m_mapObjs.size()));
            tables.m_mapObjs.push_back(def);
        }
    }
}

static size_t PutNames(std::vector<uint8_t>& out, uint32_t blobToken, uint32_t offsetToken, const std::vector<std::string>& names) {
    std::vector<uint32_t> offsets;
    size_t start = BeginChunk(out, blobToken);

    for (const std::string& name : names) {
        offsets.push_back(static_cast<uint32_t>(out.size() - start - 8));
        out.insert(out.end(), name.begin(), name.end());
        out.push_back(0);
    }

    EndChunk(out, start);
    size_t idStart = BeginChunk(out, offsetToken);

    for (uint32_t offset : offsets) {
        PutU32(out, offset);
    }

    EndChunk(out, idStart);
    return idStart;
}

static bool BuildChunk(CMapArea* area, uint32_t index, const uint8_t* mcnk, uint32_t mcnkSize, std::vector<uint8_t>& out, const ObjectTables& tables) {
    uint32_t end = mcnkSize + 8;

    if (ReadU32(mcnk) != TOKEN_MCNK || mcnkSize < sizeof(SMChunk)) {
        return false;
    }

    SMChunk header;
    memcpy(&header, mcnk + 8, sizeof(header));

    const uint8_t* mcvt = SubChunk(mcnk, header.m_ofsHeight, TOKEN_MCVT, end);
    const uint8_t* mccv = SubChunk(mcnk, header.m_ofsMCCV, TOKEN_MCCV, end);
    const uint8_t* mcnr = SubChunk(mcnk, header.m_ofsNormal, TOKEN_MCNR, end);
    const uint8_t* mcly = SubChunk(mcnk, header.m_ofsLayer, TOKEN_MCLY, end);
    const uint8_t* mcrf = SubChunk(mcnk, header.m_ofsRefs, TOKEN_MCRF, end);
    const uint8_t* mcsh = SubChunk(mcnk, header.m_ofsShadow, TOKEN_MCSH, end);
    const uint8_t* mcal = SubChunk(mcnk, header.m_ofsAlpha, TOKEN_MCAL, end);
    const uint8_t* mclq = SubChunk(mcnk, header.m_ofsLiquid, TOKEN_MCLQ, end);
    const uint8_t* mcse = SubChunk(mcnk, header.m_ofsSndEmitters, TOKEN_MCSE, end);

    if (!mcvt || !mcnr) {
        return false;
    }

    int32_t row = area->m_index.y * 16 + static_cast<int32_t>(index / 16);
    int32_t col = area->m_index.x * 16 + static_cast<int32_t>(index % 16);
    const SMLayer* layers = nullptr;
    uint32_t layerCount = 0;
    const uint8_t* alpha = nullptr;
    uint32_t alphaSize = 0;

    if (!TextureEdit::GetSaveData(row, col, &layers, &layerCount, &alpha, &alphaSize)) {
        layers = mcly ? reinterpret_cast<const SMLayer*>(mcly + 8) : nullptr;
        layerCount = mcly ? ReadU32(mcly + 4) / sizeof(SMLayer) : 0;
        alpha = mcal ? mcal + 8 : nullptr;
        alphaSize = mcal && header.m_sizeAlpha >= 8 ? header.m_sizeAlpha - 8 : 0;
    }

    size_t start = BeginChunk(out, TOKEN_MCNK);
    size_t headerOffset = out.size();
    out.resize(out.size() + sizeof(SMChunk));

    auto relative = [&]() {
        return static_cast<uint32_t>(out.size() - start);
    };

    header.m_ofsHeight = relative();
    PutChunk(out, TOKEN_MCVT, mcvt + 8, ReadU32(mcvt + 4), ReadU32(mcvt + 4));

    const uint32_t* addedColors = mccv ? nullptr : ChunkEdit::GetAddedVertexColors(row, col);

    if (mccv) {
        header.m_ofsMCCV = relative();
        PutChunk(out, TOKEN_MCCV, mccv + 8, ReadU32(mccv + 4), ReadU32(mccv + 4));
    } else if (addedColors && (header.m_flags & 0x40)) {
        header.m_ofsMCCV = relative();
        PutChunk(out, TOKEN_MCCV, reinterpret_cast<const uint8_t*>(addedColors), 145 * 4, 145 * 4);
    } else {
        header.m_ofsMCCV = 0;
        header.m_flags &= ~0x40u;
    }

    header.m_ofsNormal = relative();
    PutChunk(out, TOKEN_MCNR, mcnr + 8, MCNR_PADDED, MCNR_SIZE);

    header.m_ofsLayer = relative();
    header.m_nLayers = layerCount;
    PutChunk(out, TOKEN_MCLY, reinterpret_cast<const uint8_t*>(layers), layerCount * sizeof(SMLayer), layerCount * sizeof(SMLayer));

    if (tables.m_active) {
        std::vector<uint32_t> doodadRefs;
        std::vector<uint32_t> mapObjRefs;
        uint32_t refCount = mcrf ? ReadU32(mcrf + 4) / 4 : 0;

        for (uint32_t i = 0; i < refCount; i++) {
            uint32_t ref = ReadU32(mcrf + 8 + i * 4);

            if (i < header.m_nDoodadRefs) {
                if (ref < tables.m_doodadRemap.size() && tables.m_doodadRemap[ref] >= 0) {
                    doodadRefs.push_back(static_cast<uint32_t>(tables.m_doodadRemap[ref]));
                }
            } else if (i < header.m_nDoodadRefs + header.m_nMapObjRefs) {
                if (ref < tables.m_mapObjRemap.size() && tables.m_mapObjRemap[ref] >= 0) {
                    mapObjRefs.push_back(static_cast<uint32_t>(tables.m_mapObjRemap[ref]));
                }
            }
        }

        doodadRefs.insert(doodadRefs.end(), tables.m_addedDoodadRefs[index].begin(), tables.m_addedDoodadRefs[index].end());
        mapObjRefs.insert(mapObjRefs.end(), tables.m_addedMapObjRefs[index].begin(), tables.m_addedMapObjRefs[index].end());
        header.m_nDoodadRefs = static_cast<uint32_t>(doodadRefs.size());
        header.m_nMapObjRefs = static_cast<uint32_t>(mapObjRefs.size());
        header.m_ofsRefs = relative();
        size_t refStart = BeginChunk(out, TOKEN_MCRF);

        for (uint32_t ref : doodadRefs) {
            PutU32(out, ref);
        }

        for (uint32_t ref : mapObjRefs) {
            PutU32(out, ref);
        }

        EndChunk(out, refStart);
    } else if (mcrf) {
        header.m_ofsRefs = relative();
        PutChunk(out, TOKEN_MCRF, mcrf + 8, ReadU32(mcrf + 4), ReadU32(mcrf + 4));
    } else {
        header.m_ofsRefs = 0;
    }

    if (mcsh) {
        header.m_ofsShadow = relative();
        PutChunk(out, TOKEN_MCSH, mcsh + 8, header.m_sizeShadow, header.m_sizeShadow);
    } else {
        header.m_ofsShadow = 0;
    }

    header.m_ofsAlpha = relative();
    header.m_sizeAlpha = alphaSize + 8;
    PutChunk(out, TOKEN_MCAL, alpha, alphaSize, alphaSize);

    if (mclq) {
        uint32_t liquidSize = header.m_sizeLiquid >= 8 ? header.m_sizeLiquid - 8 : 0;
        header.m_ofsLiquid = relative();
        PutChunk(out, TOKEN_MCLQ, mclq + 8, liquidSize, 0);
    } else {
        header.m_ofsLiquid = 0;
    }

    if (mcse) {
        header.m_ofsSndEmitters = relative();
        PutChunk(out, TOKEN_MCSE, mcse + 8, ReadU32(mcse + 4), ReadU32(mcse + 4));
    } else {
        header.m_ofsSndEmitters = 0;
    }

    memcpy(&out[headerOffset], &header, sizeof(header));
    EndChunk(out, start);
    return true;
}

bool BuildTile(CMapArea* area, std::vector<uint8_t>& out, const ObjectChanges* changes) {
    if (!area || area->m_asyncObject || !area->m_fileBuffer || !area->m_header) {
        return false;
    }

    const uint8_t* buffer = static_cast<const uint8_t*>(area->m_fileBuffer);
    uint32_t size = static_cast<uint32_t>(area->m_fileSize);
    out.clear();
    out.reserve(size + 0x10000);

    size_t mhdrData = 0;
    size_t mcinData = 0;
    uint32_t chunkIndex = 0;
    std::vector<uint32_t> chunkOffsets(256, 0);
    std::vector<uint32_t> chunkSizes(256, 0);
    uint32_t offset = 0;
    ObjectTables tables;
    BuildObjectTables(buffer, size, changes, tables);

    while (offset + 8 <= size) {
        uint32_t token = ReadU32(buffer + offset);
        uint32_t chunkSize = ReadU32(buffer + offset + 4);

        if (offset + 8 + chunkSize > size) {
            return false;
        }

        const uint8_t* data = buffer + offset + 8;
        size_t chunkStart = out.size();

        if (tables.m_active && (token == TOKEN_MMID || token == TOKEN_MWID)) {
            offset += 8 + chunkSize;
            continue;
        } else if (tables.m_active && token == TOKEN_MMDX) {
            size_t idStart = PutNames(out, TOKEN_MMDX, TOKEN_MMID, tables.m_doodadNames);

            if (mhdrData) {
                SetU32(out, mhdrData + offsetof(SMAreaHeader, m_mmid), static_cast<uint32_t>(idStart - mhdrData));
            }
        } else if (tables.m_active && token == TOKEN_MWMO) {
            size_t idStart = PutNames(out, TOKEN_MWMO, TOKEN_MWID, tables.m_mapObjNames);

            if (mhdrData) {
                SetU32(out, mhdrData + offsetof(SMAreaHeader, m_mwid), static_cast<uint32_t>(idStart - mhdrData));
            }
        } else if (tables.m_active && token == TOKEN_MDDF) {
            PutChunk(out, TOKEN_MDDF, reinterpret_cast<const uint8_t*>(tables.m_doodads.data()), static_cast<uint32_t>(tables.m_doodads.size() * sizeof(SMDoodadDef)), static_cast<uint32_t>(tables.m_doodads.size() * sizeof(SMDoodadDef)));
        } else if (tables.m_active && token == TOKEN_MODF) {
            PutChunk(out, TOKEN_MODF, reinterpret_cast<const uint8_t*>(tables.m_mapObjs.data()), static_cast<uint32_t>(tables.m_mapObjs.size() * sizeof(SMMapObjDef)), static_cast<uint32_t>(tables.m_mapObjs.size() * sizeof(SMMapObjDef)));
        } else if (token == TOKEN_MTEX) {
            size_t start = BeginChunk(out, TOKEN_MTEX);

            for (uint32_t i = 0; i < area->m_textures.Count(); i++) {
                const char* name = area->m_textures.m_data[i].m_textureName;
                name = name ? name : "";
                out.insert(out.end(), name, name + strlen(name) + 1);
            }

            EndChunk(out, start);
        } else if (token == TOKEN_MTXF) {
            size_t start = BeginChunk(out, TOKEN_MTXF);
            uint32_t count = area->m_textures.Count();

            for (uint32_t i = 0; i < count; i++) {
                PutU32(out, i * 4 + 4 <= chunkSize ? ReadU32(data + i * 4) : 0);
            }

            EndChunk(out, start);
        } else if (token == TOKEN_MCNK) {
            if (chunkIndex >= 256 || !BuildChunk(area, chunkIndex, buffer + offset, chunkSize, out, tables)) {
                return false;
            }

            chunkOffsets[chunkIndex] = static_cast<uint32_t>(chunkStart);
            chunkSizes[chunkIndex] = static_cast<uint32_t>(out.size() - chunkStart);
            chunkIndex++;
        } else {
            PutChunk(out, token, data, chunkSize, chunkSize);

            if (token == TOKEN_MHDR) {
                mhdrData = chunkStart + 8;
            } else if (token == TOKEN_MCIN) {
                mcinData = chunkStart + 8;
            }
        }

        if (mhdrData) {
            uint32_t relative = static_cast<uint32_t>(chunkStart - mhdrData);
            size_t field = 0;

            switch (token) {
                case TOKEN_MCIN: field = offsetof(SMAreaHeader, m_mcin); break;
                case TOKEN_MTEX: field = offsetof(SMAreaHeader, m_mtex); break;
                case TOKEN_MMDX: field = offsetof(SMAreaHeader, m_mmdx); break;
                case TOKEN_MMID: field = offsetof(SMAreaHeader, m_mmid); break;
                case TOKEN_MWMO: field = offsetof(SMAreaHeader, m_mwmo); break;
                case TOKEN_MWID: field = offsetof(SMAreaHeader, m_mwid); break;
                case TOKEN_MDDF: field = offsetof(SMAreaHeader, m_mddf); break;
                case TOKEN_MODF: field = offsetof(SMAreaHeader, m_modf); break;
                case TOKEN_MFBO: field = offsetof(SMAreaHeader, m_mfbo); break;
                case TOKEN_MH2O: field = offsetof(SMAreaHeader, m_mh2o); break;
                case TOKEN_MTXF: field = offsetof(SMAreaHeader, m_mtxf); break;
                default: field = static_cast<size_t>(-1); break;
            }

            if (field != static_cast<size_t>(-1) && token != TOKEN_MHDR) {
                SetU32(out, mhdrData + field, relative);
            }
        }

        offset += 8 + chunkSize;
    }

    if (chunkIndex != 256 || !mhdrData || !mcinData) {
        return false;
    }

    for (uint32_t i = 0; i < 256; i++) {
        SMChunkInfo info;
        memcpy(&info, &out[mcinData + i * sizeof(SMChunkInfo)], sizeof(info));
        info.m_offset = chunkOffsets[i];
        info.m_size = chunkSizes[i];
        info.m_flags = 0;
        memset(info.unused_000C, 0, sizeof(info.unused_000C));
        memcpy(&out[mcinData + i * sizeof(SMChunkInfo)], &info, sizeof(info));
    }

    return true;
}

bool ReloadArea(CMapArea* area, const std::vector<uint8_t>& data) {
    if (!area || area->m_asyncObject || !area->m_fileBuffer || data.empty()) {
        return false;
    }

    for (CMapBaseObjLink* link = area->m_chunkLinkList.Head(); link;) {
        CMapChunk* chunk = static_cast<CMapChunk*>(link->m_owner);
        CMapBaseObjLink* next = area->m_chunkLinkList.Next(link);
        area->m_mapChunks[16 * chunk->m_aIndex.y + chunk->m_aIndex.x] = nullptr;
        CMap::FreeBaseObjLink(link);
        chunk->Purge();
        CMap::FreeChunk(chunk);
        link = next;
    }

    for (uint32_t i = 0; i < area->m_textures.m_count; i++) {
        if (area->m_textures.m_data[i].m_texture) {
            HandleClose(reinterpret_cast<HOBJECT>(area->m_textures.m_data[i].m_texture));
        }
    }

    area->m_textures.m_count = 0;

    if (area->m_liquid) {
        SMemFree(area->m_liquid, __FILE__, __LINE__, 0);
        area->m_liquid = nullptr;
    }

    area->m_flyingBbox = nullptr;
    area->m_textureFlags = nullptr;
    CMapArea::FreeAsyncLoadBuffer(area->m_fileBuffer, area->m_fileSize);
    area->m_fileSize = static_cast<int32_t>(data.size());
    area->m_fileBuffer = CMapArea::AllocAsyncLoadBuffer(area->m_fileSize);
    memcpy(area->m_fileBuffer, data.data(), data.size());
    area->ProcessIffChunks();

    TextureEdit::ResetArea(area->m_index.x, area->m_index.y);
    ChunkEdit::ResetArea(area->m_index.x, area->m_index.y);
    return true;
}

bool ApplyObjectChanges(CMapArea* area, const ObjectChanges& changes) {
    std::vector<uint8_t> data;

    if (!BuildTile(area, data, &changes)) {
        return false;
    }

    if (!ReloadArea(area, data)) {
        return false;
    }

    EditorUndo::MarkArea(static_cast<uint32_t>(64 * area->m_index.y + area->m_index.x));
    return true;
}

#if defined(_WIN32)
static std::wstring Widen(const std::string& text) {
    if (text.empty()) {
        return std::wstring();
    }

    int32_t length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int32_t>(text.size()), nullptr, 0);
    std::wstring result(length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int32_t>(text.size()), &result[0], length);
    return result;
}

static std::wstring ExeDirectory() {
    wchar_t buffer[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    std::wstring path(buffer, length);
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

static void CreateDirectories(const std::wstring& path) {
    for (size_t i = 3; i <= path.size(); i++) {
        if (i == path.size() || path[i] == L'\\') {
            CreateDirectoryW(path.substr(0, i).c_str(), nullptr);
        }
    }
}

static bool WriteBytes(const std::wstring& path, const void* data, size_t size) {
    FILE* file = _wfopen(path.c_str(), L"wb");

    if (!file) {
        return false;
    }

    bool ok = fwrite(data, 1, size, file) == size;
    fclose(file);
    return ok;
}

static void BackupOriginal(const std::wstring& path, const char* gamePath) {
    std::wstring backup = path + L".bak";

    if (GetFileAttributesW(backup.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return;
    }

    void* data = nullptr;
    uint32_t size = 0;

    if (!SFile::LoadFile(gamePath, &data, &size, 0, nullptr) || !data) {
        return;
    }

    WriteBytes(backup, data, size);
    SFile::Unload(data);
}
#else
// Game-relative paths resolve against the working directory (the game folder) with on-disk case, like the Linux
// OsFile does, instead of against the exe directory.
static std::filesystem::path GameFilePath(const std::string& relative) {
    char resolved[1024];
    OsLinuxResolvePath(relative.c_str(), resolved, sizeof(resolved), true);
    return std::filesystem::path(resolved);
}

static void CreateDirectories(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::create_directories(path, error);
}

static bool WriteBytes(const std::filesystem::path& path, const void* data, size_t size) {
    FILE* file = fopen(path.c_str(), "wb");

    if (!file) {
        return false;
    }

    bool ok = fwrite(data, 1, size, file) == size;
    fclose(file);
    return ok;
}

static void BackupOriginal(const std::filesystem::path& path, const char* gamePath) {
    std::filesystem::path backup = path;
    backup += ".bak";
    std::error_code error;

    if (std::filesystem::exists(backup, error)) {
        return;
    }

    void* data = nullptr;
    uint32_t size = 0;

    if (!SFile::LoadFile(gamePath, &data, &size, 0, nullptr) || !data) {
        return;
    }

    WriteBytes(backup, data, size);
    SFile::Unload(data);
}
#endif

static bool SaveArea(CMapArea* area) {
    std::vector<uint8_t> data;

    if (!BuildTile(area, data)) {
        return false;
    }

    char gamePath[512];
    snprintf(gamePath, sizeof(gamePath), "%s\\%s_%d_%d.adt", CMap::mapPath, CMap::mapName, area->m_index.x, area->m_index.y);
    std::string relative = gamePath;
    std::replace(relative.begin(), relative.end(), '/', '\\');

#if defined(_WIN32)
    std::wstring path = ExeDirectory() + L"\\" + Widen(relative);
    CreateDirectories(path.substr(0, path.find_last_of(L'\\')));
#else
    std::filesystem::path path = GameFilePath(relative);
    CreateDirectories(path.parent_path());
#endif
    BackupOriginal(path, relative.c_str());
    return WriteBytes(path, data.data(), data.size());
}

static void AddArea(std::vector<CMapArea*>& areas, CMapArea* area) {
    if (area && !area->m_asyncObject && area->m_fileBuffer && std::find(areas.begin(), areas.end(), area) == areas.end()) {
        areas.push_back(area);
    }
}

int32_t Save(MODE mode, std::string& error) {
    if (!CMap::bActive) {
        error = "No map is loaded.";
        return -1;
    }

    std::vector<CMapArea*> areas;

    if (mode == SAVE_ALL) {
        for (uint32_t i = 0; i < 4096; i++) {
            AddArea(areas, CMap::m_areaTable[i]);
        }
    } else if (mode == SAVE_CURRENT) {
        C3Vector camera = Runtime::CameraPosition();
        int32_t x = static_cast<int32_t>((17066.666f - camera.y) / 533.33333f);
        int32_t y = static_cast<int32_t>((17066.666f - camera.x) / 533.33333f);

        if (x >= 0 && x < 64 && y >= 0 && y < 64) {
            AddArea(areas, CMap::m_areaTable[64 * y + x]);
        }
    } else {
        for (uint32_t index : EditorUndo::GetDirtyAreas()) {
            AddArea(areas, CMap::m_areaTable[index]);
        }
    }

    int32_t saved = 0;

    for (CMapArea* area : areas) {
        if (!SaveArea(area)) {
            error = "Failed to save " + std::string(CMap::mapName) + "_" + std::to_string(area->m_index.x) + "_" + std::to_string(area->m_index.y) + ".adt";
            break;
        }

        saved++;
    }

    if (saved) {
        Streaming::LooseFiles::Invalidate();
        SFile::RebuildHash();
    }

    if (!error.empty()) {
        return -1;
    }

    if (mode != SAVE_CURRENT) {
        EditorUndo::ClearModified();
    }

    return saved;
}

}
