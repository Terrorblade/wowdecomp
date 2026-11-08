#include "HoT/Editor/Map/EditorScripts.h"
#include <cmath>
#include <cstdint>
#include <string>
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Os/Input.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/CRect.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "HoT/Core/Chat.h"
#include "HoT/Editor/FreeCam.h"
#include "HoT/Editor/Map/ChunkEdit.h"
#include "HoT/Editor/Map/EditorSave.h"
#include "HoT/Editor/Map/EditorUndo.h"
#include "HoT/Editor/Map/EditorWorld.h"
#include "HoT/Editor/Map/ObjectEdit.h"
#include "HoT/Editor/Map/Runtime.h"
#include "HoT/Editor/Map/TerrainEdit.h"
#include "HoT/Editor/Map/TextureEdit.h"
#include "HoT/Editor/Mode.h"
#include "HoT/Lua/Script.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/UIUtil/Camera.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include "WoW/Source/WorldClient/Map.h"
#include "WoW/Source/WorldClient/MapDoodadDef.h"
#include "WoW/Source/WorldClient/Types.h"
#include "WoW/Source/WorldClient/World.h"

namespace HoT::Editor::Map::EditorScripts {

struct EditorViewFlag {
    const char* m_name;
    uint32_t m_flag;
    bool m_inverted;
};

static const EditorViewFlag s_viewFlags[] = {
    { "doodads", Enable_Doodads, false },
    { "wmo", Enable_MapObjs, false },
    { "terrain", Enable_Chunks, false },
    { "water", Enable_Water, false },
    { "wireframe", Enable_ShowTris, false },
    { "animations", Enable_NoAnimation, true },
    { "sky", Enable_Sky, false },
    { "detaildoodads", Enable_DetailDoodads, false },
    { "modelboxes", Enable_AABoxes, false },
};

static float Clamp(float value, float minValue, float maxValue) {
    return value < minValue ? minValue : (value > maxValue ? maxValue : value);
}

static const EditorViewFlag* FindViewFlag(const char* name) {
    for (uint32_t i = 0; i < sizeof(s_viewFlags) / sizeof(s_viewFlags[0]); i++) {
        if (!SStrCmpI(name, s_viewFlags[i].m_name, STORM_MAX_STR)) {
            return &s_viewFlags[i];
        }
    }

    return nullptr;
}

static int32_t Script_EditorPrint(lua_State* L) {
    std::string text;
    int32_t count = lua_gettop(L);

    for (int32_t i = 1; i <= count; i++) {
        const char* value = lua_tolstring(L, i, nullptr);
        text += i > 1 ? " " : "";
        text += value ? value : (lua_isnil(L, i) ? "nil" : lua_typename(L, lua_type(L, i)));
    }

    lua_getfield(L, LUA_GLOBALSINDEX, "GameEditor_Print");

    if (lua_isfunction(L, -1)) {
        lua_pushstring(L, text.c_str());
        lua_pcall(L, 1, 0, 0);
    } else {
        lua_settop(L, count);
        Chat::Print("%s", text.c_str());
    }

    return 0;
}

static int32_t Script_EditorGetWindowSize(lua_State* L) {
    CRect window = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_theGxDevicePtr->CapsWindowSize(window);
    lua_pushnumber(L, window.maxX - window.minX);
    lua_pushnumber(L, window.maxY - window.minY);
    return 2;
}

static int32_t Script_EditorGetEditingMode(lua_State* L) {
    lua_pushnumber(L, TerrainEdit::s_mode + 1);
    return 1;
}

static int32_t Script_EditorSetEditingMode(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: EditorSetEditingMode(mode)");
        return 0;
    }

    int32_t mode = static_cast<int32_t>(lua_tonumber(L, 1)) - 1;

    if (mode >= 0 && mode < TerrainEdit::NUM_MODES) {
        Runtime::EndEdits();
        TerrainEdit::s_mode = static_cast<TerrainEdit::MODE>(mode);
    }

    return 0;
}

static int32_t Script_EditorGetRaiseLower(lua_State* L) {
    const TerrainEdit::RaiseLowerSettings& settings = TerrainEdit::s_raiseLower;
    lua_pushnumber(L, settings.m_type + 1);
    lua_pushnumber(L, settings.m_radius);
    lua_pushnumber(L, settings.m_innerRadius);
    lua_pushnumber(L, settings.m_speed);
    return 4;
}

static int32_t Script_EditorSetRaiseLower(lua_State* L) {
    if (!lua_isstring(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: EditorSetRaiseLower(\"key\", value)");
        return 0;
    }

    const char* key = lua_tolstring(L, 1, nullptr);
    float value = static_cast<float>(lua_tonumber(L, 2));
    TerrainEdit::RaiseLowerSettings& settings = TerrainEdit::s_raiseLower;

    if (!SStrCmpI(key, "type", STORM_MAX_STR)) {
        int32_t type = static_cast<int32_t>(value) - 1;

        if (type >= 0 && type < TerrainEdit::NUM_TERRAIN_TYPES) {
            settings.m_type = type;
        }
    } else if (!SStrCmpI(key, "radius", STORM_MAX_STR)) {
        settings.m_radius = Clamp(value, 0.0f, 1000.0f);
    } else if (!SStrCmpI(key, "innerRadius", STORM_MAX_STR)) {
        settings.m_innerRadius = Clamp(value, 0.0f, 1.0f);
    } else if (!SStrCmpI(key, "speed", STORM_MAX_STR)) {
        settings.m_speed = Clamp(value, 0.0f, 1000.0f);
    }

    return 0;
}

static int32_t Script_EditorGetFlattenBlur(lua_State* L) {
    const TerrainEdit::FlattenBlurSettings& settings = TerrainEdit::s_flattenBlur;
    lua_pushnumber(L, settings.m_type + 1);
    lua_pushnumber(L, settings.m_radius);
    lua_pushnumber(L, settings.m_speed);
    lua_pushboolean(L, settings.m_raise);
    lua_pushboolean(L, settings.m_lower);
    lua_pushboolean(L, settings.m_angled);
    lua_pushnumber(L, settings.m_angle);
    lua_pushnumber(L, settings.m_orientation);
    lua_pushboolean(L, settings.m_lock);
    lua_pushnumber(L, settings.m_lockPos.x);
    lua_pushnumber(L, settings.m_lockPos.y);
    lua_pushnumber(L, settings.m_lockPos.z);
    return 12;
}

static int32_t Script_EditorSetFlattenBlur(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: EditorSetFlattenBlur(\"key\", value)");
        return 0;
    }

    const char* key = lua_tolstring(L, 1, nullptr);
    float value = static_cast<float>(lua_tonumber(L, 2));
    bool enabled = lua_toboolean(L, 2) != 0;
    TerrainEdit::FlattenBlurSettings& settings = TerrainEdit::s_flattenBlur;

    if (!SStrCmpI(key, "type", STORM_MAX_STR)) {
        int32_t type = static_cast<int32_t>(value) - 1;

        if (type >= 0 && type < TerrainEdit::NUM_FLATTEN_TYPES) {
            settings.m_type = type;
        }
    } else if (!SStrCmpI(key, "radius", STORM_MAX_STR)) {
        settings.m_radius = Clamp(value, 0.0f, 1000.0f);
    } else if (!SStrCmpI(key, "speed", STORM_MAX_STR)) {
        settings.m_speed = Clamp(value, 0.0f, 10.0f);
    } else if (!SStrCmpI(key, "raise", STORM_MAX_STR)) {
        settings.m_raise = enabled;
    } else if (!SStrCmpI(key, "lower", STORM_MAX_STR)) {
        settings.m_lower = enabled;
    } else if (!SStrCmpI(key, "angled", STORM_MAX_STR)) {
        settings.m_angled = enabled;
    } else if (!SStrCmpI(key, "angle", STORM_MAX_STR)) {
        settings.m_angle = Clamp(value, 0.0f, 89.0f);
    } else if (!SStrCmpI(key, "orientation", STORM_MAX_STR)) {
        settings.m_orientation = fmodf(value, 360.0f);
    } else if (!SStrCmpI(key, "lock", STORM_MAX_STR)) {
        settings.m_lock = enabled;
    } else if (!SStrCmpI(key, "lockX", STORM_MAX_STR)) {
        settings.m_lockPos.x = value;
    } else if (!SStrCmpI(key, "lockY", STORM_MAX_STR)) {
        settings.m_lockPos.y = value;
    } else if (!SStrCmpI(key, "lockH", STORM_MAX_STR)) {
        settings.m_lockPos.z = value;
    }

    return 0;
}

static int32_t Script_EditorGetViewFlag(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: EditorGetViewFlag(\"flag\")");
        return 0;
    }

    const char* name = lua_tolstring(L, 1, nullptr);

    if (!SStrCmpI(name, "fog", STORM_MAX_STR)) {
        lua_pushboolean(L, EditorWorld::GetFog());
        return 1;
    }

    if (!SStrCmpI(name, "lines", STORM_MAX_STR)) {
        lua_pushboolean(L, ChunkEdit::s_view.m_lines);
        return 1;
    }

    if (!SStrCmpI(name, "holelines", STORM_MAX_STR)) {
        lua_pushboolean(L, ChunkEdit::s_view.m_holeLines);
        return 1;
    }

    if (!SStrCmpI(name, "adtborders", STORM_MAX_STR)) {
        lua_pushboolean(L, ChunkEdit::s_view.m_adtBorders);
        return 1;
    }

    const EditorViewFlag* flag = FindViewFlag(name);

    if (!flag) {
        return 0;
    }

    bool set = (CWorld::enables & flag->m_flag) != 0;
    lua_pushboolean(L, flag->m_inverted ? !set : set);
    return 1;
}

static int32_t Script_EditorSetViewFlag(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: EditorSetViewFlag(\"flag\", enabled)");
        return 0;
    }

    const char* name = lua_tolstring(L, 1, nullptr);
    bool enabled = lua_toboolean(L, 2) != 0;

    if (!SStrCmpI(name, "fog", STORM_MAX_STR)) {
        EditorWorld::SetFog(enabled);
        return 0;
    }

    if (!SStrCmpI(name, "lines", STORM_MAX_STR)) {
        ChunkEdit::s_view.m_lines = enabled;
        return 0;
    }

    if (!SStrCmpI(name, "holelines", STORM_MAX_STR)) {
        ChunkEdit::s_view.m_holeLines = enabled;
        return 0;
    }

    if (!SStrCmpI(name, "adtborders", STORM_MAX_STR)) {
        ChunkEdit::s_view.m_adtBorders = enabled;
        return 0;
    }

    const EditorViewFlag* flag = FindViewFlag(name);

    if (!flag) {
        return 0;
    }

    if (enabled != flag->m_inverted) {
        CWorld::enables |= flag->m_flag;
    } else {
        CWorld::enables &= ~flag->m_flag;
    }

    return 0;
}

static int32_t Script_EditorGetCursor(lua_State* L) {
    if (!Runtime::CursorValid()) {
        return 0;
    }

    const C3Vector& cursor = Runtime::Cursor();
    lua_pushnumber(L, cursor.x);
    lua_pushnumber(L, cursor.y);
    lua_pushnumber(L, cursor.z);
    return 3;
}

static int32_t Script_EditorGetCamera(lua_State* L) {
    C3Vector position = Runtime::CameraPosition();
    CGCamera* camera = CGWorldFrame::GetActiveCamera();
    C3Vector forward = camera ? camera->Forward() : C3Vector(1.0f, 0.0f, 0.0f);
    float yaw = atan2f(forward.y, forward.x);

    if (yaw < 0.0f) {
        yaw += 6.2831855f;
    }

    lua_pushnumber(L, position.x);
    lua_pushnumber(L, position.y);
    lua_pushnumber(L, position.z);
    lua_pushnumber(L, yaw);
    lua_pushnumber(L, asinf(Clamp(forward.z, -1.0f, 1.0f)));
    lua_pushnumber(L, FreeCam::GetSpeed());
    return 6;
}

// Only moves the camera, the free camera keeps its own look direction.
static int32_t Script_EditorSetCamera(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3)) {
        luaL_error(L, "Usage: EditorSetCamera(x, y, z [, yaw, pitch])");
        return 0;
    }

    FreeCam::SetPosition(C3Vector(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), static_cast<float>(lua_tonumber(L, 3))));
    return 0;
}

static int32_t Script_EditorSetCameraSpeed(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: EditorSetCameraSpeed(speed)");
        return 0;
    }

    FreeCam::SetSpeed(Clamp(static_cast<float>(lua_tonumber(L, 1)), 1.0f, 4000.0f));
    return 0;
}

static int32_t Script_EditorGetMapInfo(lua_State* L) {
    lua_pushstring(L, Runtime::MapDirectory());
    lua_pushnumber(L, Runtime::MapId());
    return 2;
}

static int32_t Script_EditorGetAreaName(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: EditorGetAreaName(x, y)");
        return 0;
    }

    AreaTableRec* area = g_areaTableDB.GetRecord(TerrainEdit::GetAreaId(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2))));

    if (!area) {
        lua_pushstring(L, "");
        return 1;
    }

    AreaTableRec* parent = area->m_parentAreaID ? g_areaTableDB.GetRecord(area->m_parentAreaID) : nullptr;

    if (parent && parent->m_areaName) {
        char name[512];
        SStrPrintf(name, sizeof(name), "%s - %s", parent->m_areaName, area->m_areaName ? area->m_areaName : "");
        lua_pushstring(L, name);
    } else {
        lua_pushstring(L, area->m_areaName ? area->m_areaName : "");
    }

    return 1;
}

static int32_t Script_EditorGetStats(lua_State* L) {
    uint32_t areasLoaded;
    uint32_t chunksDrawn;
    EditorWorld::Stats(areasLoaded, chunksDrawn);

    uint32_t doodads = 0;

    for (CMapDoodadDef* def = CMap::doodadDefHash.Head(); def; def = CMap::doodadDefHash.Next(def)) {
        doodads++;
    }

    lua_pushnumber(L, Runtime::Fps());
    lua_pushnumber(L, chunksDrawn);
    lua_pushnumber(L, areasLoaded);
    lua_pushnumber(L, EditorUndo::GetModifiedChunkCount());
    lua_pushnumber(L, EditorUndo::GetUndoCount());
    lua_pushnumber(L, EditorUndo::GetRedoCount());
    lua_pushnumber(L, doodads);
    lua_pushnumber(L, CWorldScene::nDoodadsRendered);
    lua_pushnumber(L, EditorUndo::GetModifiedObjectCount());
    return 9;
}

static int32_t Script_EditorUndo(lua_State* L) {
    Runtime::EndEdits();
    lua_pushboolean(L, EditorUndo::Undo());
    return 1;
}

static int32_t Script_EditorRedo(lua_State* L) {
    Runtime::EndEdits();
    lua_pushboolean(L, EditorUndo::Redo());
    return 1;
}

static int32_t Script_EditorGetTimeOfDay(lua_State* L) {
    lua_pushnumber(L, EditorWorld::TimeOfDay());
    return 1;
}

static int32_t Script_EditorSetTimeOfDay(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: EditorSetTimeOfDay(minutes)");
        return 0;
    }

    EditorWorld::SetTimeOfDay(static_cast<uint32_t>(Clamp(static_cast<float>(lua_tonumber(L, 1)), 0.0f, 1439.0f)));
    return 0;
}

static int32_t Script_EditorGetHeight(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: EditorGetHeight(x, y)");
        return 0;
    }

    float z;

    if (!TerrainEdit::GetHeight(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), &z)) {
        return 0;
    }

    lua_pushnumber(L, z);
    return 1;
}

static int32_t Script_EditorApplyBrush(lua_State* L) {
    if (!lua_isstring(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4)) {
        luaL_error(L, "Usage: EditorApplyBrush(\"raise|lower|flatten|blur\", x, y, seconds)");
        return 0;
    }

    static const char* s_operations[] = { "raise", "lower", "flatten", "blur" };
    const char* name = lua_tolstring(L, 1, nullptr);
    int32_t op = -1;

    for (int32_t i = 0; i < 4; i++) {
        if (!SStrCmpI(name, s_operations[i], STORM_MAX_STR)) {
            op = i;
        }
    }

    C3Vector pos(static_cast<float>(lua_tonumber(L, 2)), static_cast<float>(lua_tonumber(L, 3)), 0.0f);

    if (op < 0 || TerrainEdit::IsStroking() || !TerrainEdit::GetHeight(pos.x, pos.y, &pos.z)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    float seconds = static_cast<float>(lua_tonumber(L, 4));

    for (float done = 0.0f; done < seconds; done += 1.0f / 60.0f) {
        TerrainEdit::GetHeight(pos.x, pos.y, &pos.z);
        TerrainEdit::Apply(static_cast<TerrainEdit::OPERATION>(op), pos, 1.0f / 60.0f);
    }

    TerrainEdit::EndStroke();
    lua_pushboolean(L, 1);
    return 1;
}

static int32_t Script_EditorIsMouseButtonDown(lua_State* L) {
    bool down = false;

    for (MOUSEBUTTON button : { MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE }) {
        down = down || Mode::IsMouseButtonDown(button) || Mode::IsMouseButtonHeld(button);
    }

    lua_pushboolean(L, down);
    return 1;
}

static int32_t Script_EditorGetTextureBrush(lua_State* L) {
    lua_pushnumber(L, TextureEdit::s_brush.m_radius);
    lua_pushnumber(L, TextureEdit::s_brush.m_hardness);
    lua_pushnumber(L, TextureEdit::s_brush.m_pressure);
    lua_pushnumber(L, TextureEdit::s_brush.m_opacity);
    return 4;
}

static int32_t Script_EditorSetTextureBrush(lua_State* L) {
    if (!lua_isstring(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: EditorSetTextureBrush(\"key\", value)");
        return 0;
    }

    const char* key = lua_tolstring(L, 1, nullptr);
    float value = static_cast<float>(lua_tonumber(L, 2));

    if (!SStrCmpI(key, "radius", STORM_MAX_STR)) {
        TextureEdit::s_brush.m_radius = Clamp(value, 0.0f, 1000.0f);
    } else if (!SStrCmpI(key, "hardness", STORM_MAX_STR)) {
        TextureEdit::s_brush.m_hardness = Clamp(value, 0.0f, 1.0f);
    } else if (!SStrCmpI(key, "pressure", STORM_MAX_STR)) {
        TextureEdit::s_brush.m_pressure = Clamp(value, 0.0f, 1.0f);
    } else if (!SStrCmpI(key, "opacity", STORM_MAX_STR)) {
        TextureEdit::s_brush.m_opacity = Clamp(value, 0.0f, 255.0f);
    }

    return 0;
}

static int32_t Script_EditorGetTexture(lua_State* L) {
    const char* texture = TextureEdit::GetTexture();

    if (!texture || !*texture) {
        return 0;
    }

    lua_pushstring(L, texture);
    return 1;
}

static int32_t Script_EditorSetTexture(lua_State* L) {
    TextureEdit::SetTexture(lua_isstring(L, 1) ? lua_tolstring(L, 1, nullptr) : "");
    return 0;
}

static int32_t Script_EditorGetTilesetCount(lua_State* L) {
    lua_pushnumber(L, TextureEdit::GetTilesetCount());
    return 1;
}

static int32_t Script_EditorGetTileset(lua_State* L) {
    const char* name = TextureEdit::GetTileset(static_cast<uint32_t>(lua_tonumber(L, 1)) - 1);

    if (!name) {
        return 0;
    }

    lua_pushstring(L, name);
    return 1;
}

static int32_t Script_EditorGetCursorTextures(lua_State* L) {
    if (!Runtime::CursorValid()) {
        return 0;
    }

    const char* names[4];
    uint32_t count = TextureEdit::GetChunkTextures(Runtime::Cursor(), names, 4);

    for (uint32_t i = 0; i < count; i++) {
        lua_pushstring(L, names[i]);
    }

    return static_cast<int32_t>(count);
}

static const char* KindName(int32_t kind) {
    return kind == ObjectEdit::KIND_DOODAD ? "m2" : (kind == ObjectEdit::KIND_WMO ? "wmo" : "wmodoodad");
}

static int32_t Script_EditorGetSelectionCount(lua_State* L) {
    lua_pushnumber(L, ObjectEdit::GetSelectionCount());
    return 1;
}

static int32_t Script_EditorGetSelection(lua_State* L) {
    int32_t kind;
    uint32_t uniqueId;
    const char* fileName;

    if (!ObjectEdit::GetSelectionInfo(static_cast<uint32_t>(lua_tonumber(L, 1)) - 1, &kind, &uniqueId, &fileName)) {
        return 0;
    }

    lua_pushstring(L, KindName(kind));
    lua_pushnumber(L, uniqueId);
    lua_pushstring(L, fileName);
    return 3;
}

static int32_t Script_EditorClearSelection(lua_State* L) {
    ObjectEdit::ClearSelection();
    return 0;
}

static int32_t Script_EditorGetObjectTransform(lua_State* L) {
    C3Vector position;
    C3Vector rotation;
    float scale;

    if (!ObjectEdit::GetTransform(&position, &rotation, &scale)) {
        return 0;
    }

    lua_pushnumber(L, position.x);
    lua_pushnumber(L, position.y);
    lua_pushnumber(L, position.z);
    lua_pushnumber(L, rotation.x);
    lua_pushnumber(L, rotation.y);
    lua_pushnumber(L, rotation.z);
    lua_pushnumber(L, scale);
    return 7;
}

static int32_t Script_EditorSetObjectTransform(lua_State* L) {
    C3Vector position;
    C3Vector rotation;
    float scale;

    if (!ObjectEdit::GetTransform(&position, &rotation, &scale)) {
        return 0;
    }

    if (lua_isnumber(L, 1)) {
        position.x = static_cast<float>(lua_tonumber(L, 1));
    }

    if (lua_isnumber(L, 2)) {
        position.y = static_cast<float>(lua_tonumber(L, 2));
    }

    if (lua_isnumber(L, 3)) {
        position.z = static_cast<float>(lua_tonumber(L, 3));
    }

    if (lua_isnumber(L, 4)) {
        rotation.x = static_cast<float>(lua_tonumber(L, 4));
    }

    if (lua_isnumber(L, 5)) {
        rotation.y = static_cast<float>(lua_tonumber(L, 5));
    }

    if (lua_isnumber(L, 6)) {
        rotation.z = static_cast<float>(lua_tonumber(L, 6));
    }

    if (lua_isnumber(L, 7)) {
        scale = static_cast<float>(lua_tonumber(L, 7));
    }

    if (ObjectEdit::GetSelectionCount() == 1) {
        ObjectEdit::SetTransform(&position, &rotation, &scale);
    } else {
        ObjectEdit::SetTransform(lua_isnumber(L, 1) || lua_isnumber(L, 2) || lua_isnumber(L, 3) ? &position : nullptr, nullptr, nullptr);
    }

    return 0;
}

static int32_t Script_EditorObjectSnapToGround(lua_State* L) {
    ObjectEdit::SnapToGround();
    return 0;
}

static int32_t Script_EditorGetObjectSetting(lua_State* L) {
    const char* key = lua_tolstring(L, 1, nullptr);

    if (!key) {
        return 0;
    }

    if (!SStrCmpI(key, "followCursor", STORM_MAX_STR)) {
        lua_pushboolean(L, ObjectEdit::s_settings.m_followCursor);
        return 1;
    }

    if (!SStrCmpI(key, "snapMultiToGround", STORM_MAX_STR)) {
        lua_pushboolean(L, ObjectEdit::s_settings.m_snapMultiToGround);
        return 1;
    }

    if (!SStrCmpI(key, "pickWmoDoodads", STORM_MAX_STR)) {
        lua_pushboolean(L, ObjectEdit::s_settings.m_pickWmoDoodads);
        return 1;
    }

    return 0;
}

static int32_t Script_EditorSetObjectSetting(lua_State* L) {
    const char* key = lua_tolstring(L, 1, nullptr);
    bool value = lua_toboolean(L, 2) != 0;

    if (!key) {
        return 0;
    }

    if (!SStrCmpI(key, "followCursor", STORM_MAX_STR)) {
        ObjectEdit::s_settings.m_followCursor = value;
    } else if (!SStrCmpI(key, "snapMultiToGround", STORM_MAX_STR)) {
        ObjectEdit::s_settings.m_snapMultiToGround = value;
    } else if (!SStrCmpI(key, "pickWmoDoodads", STORM_MAX_STR)) {
        ObjectEdit::s_settings.m_pickWmoDoodads = value;
    }

    return 0;
}

static int32_t Script_EditorPickObject(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4) || !lua_isnumber(L, 5) || !lua_isnumber(L, 6)) {
        luaL_error(L, "Usage: EditorPickObject(x, y, z, dirX, dirY, dirZ [, add])");
        return 0;
    }

    C3Vector start(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), static_cast<float>(lua_tonumber(L, 3)));
    C3Vector dir(static_cast<float>(lua_tonumber(L, 4)), static_cast<float>(lua_tonumber(L, 5)), static_cast<float>(lua_tonumber(L, 6)));
    dir.Normalize();
    int32_t kind;
    uint32_t uniqueId;
    float distance;

    if (!Runtime::IsMapLoaded() || !ObjectEdit::Pick(start, dir, 2000.0f, &kind, &uniqueId, &distance)) {
        return 0;
    }

    ObjectEdit::Select(kind, uniqueId, lua_toboolean(L, 7) != 0);
    lua_pushstring(L, KindName(kind));
    lua_pushnumber(L, uniqueId);
    lua_pushnumber(L, distance);
    return 3;
}

static int32_t Script_EditorMoveSelection(lua_State* L) {
    ObjectEdit::MoveBy(C3Vector(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), static_cast<float>(lua_tonumber(L, 3))));
    return 0;
}

static int32_t Script_EditorPaintTexture(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3)) {
        luaL_error(L, "Usage: EditorPaintTexture(x, y, seconds)");
        return 0;
    }

    C3Vector pos(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), 0.0f);

    if (!TerrainEdit::GetHeight(pos.x, pos.y, &pos.z)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    float seconds = static_cast<float>(lua_tonumber(L, 3));

    for (float done = 0.0f; done < seconds; done += 1.0f / 60.0f) {
        TextureEdit::Paint(pos, 1.0f / 60.0f);
    }

    TextureEdit::EndStroke();
    lua_pushboolean(L, 1);
    return 1;
}

static int32_t Script_EditorGetTexturesAt(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: EditorGetTexturesAt(x, y)");
        return 0;
    }

    const char* names[4];
    uint32_t count = TextureEdit::GetChunkTextures(C3Vector(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), 0.0f), names, 4);

    for (uint32_t i = 0; i < count; i++) {
        lua_pushstring(L, names[i]);
    }

    return static_cast<int32_t>(count);
}

static int32_t Script_EditorGetChunkTool(lua_State* L) {
    const ChunkEdit::Settings& settings = ChunkEdit::s_settings;
    lua_pushnumber(L, settings.m_holeRadius);
    lua_pushnumber(L, settings.m_areaRadius);
    lua_pushnumber(L, settings.m_areaId);
    lua_pushnumber(L, settings.m_vertexRadius);
    lua_pushnumber(L, settings.m_vertexSpeed);
    lua_pushnumber(L, settings.m_color[0]);
    lua_pushnumber(L, settings.m_color[1]);
    lua_pushnumber(L, settings.m_color[2]);
    return 8;
}

static int32_t Script_EditorSetChunkTool(lua_State* L) {
    if (!lua_isstring(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: EditorSetChunkTool(\"key\", value)");
        return 0;
    }

    const char* key = lua_tolstring(L, 1, nullptr);
    float value = static_cast<float>(lua_tonumber(L, 2));
    ChunkEdit::Settings& settings = ChunkEdit::s_settings;

    if (!SStrCmpI(key, "holeRadius", STORM_MAX_STR)) {
        settings.m_holeRadius = Clamp(value, 0.0f, 250.0f);
    } else if (!SStrCmpI(key, "areaRadius", STORM_MAX_STR)) {
        settings.m_areaRadius = Clamp(value, 0.0f, 1000.0f);
    } else if (!SStrCmpI(key, "areaId", STORM_MAX_STR)) {
        settings.m_areaId = static_cast<int32_t>(value);
    } else if (!SStrCmpI(key, "vertexRadius", STORM_MAX_STR)) {
        settings.m_vertexRadius = Clamp(value, 0.0f, 10000.0f);
    } else if (!SStrCmpI(key, "vertexSpeed", STORM_MAX_STR)) {
        settings.m_vertexSpeed = Clamp(value, 0.0f, 10.0f);
    } else if (!SStrCmpI(key, "colorR", STORM_MAX_STR)) {
        settings.m_color[0] = Clamp(value, 0.0f, 1.0f);
    } else if (!SStrCmpI(key, "colorG", STORM_MAX_STR)) {
        settings.m_color[1] = Clamp(value, 0.0f, 1.0f);
    } else if (!SStrCmpI(key, "colorB", STORM_MAX_STR)) {
        settings.m_color[2] = Clamp(value, 0.0f, 1.0f);
    }

    return 0;
}

static int32_t Script_EditorGetAreas(lua_State* L) {
    int32_t mapId = static_cast<int32_t>(Runtime::MapId());
    lua_newtable(L);
    int32_t count = 0;

    for (int32_t i = 0; i < g_areaTableDB.m_numRecords; i++) {
        AreaTableRec* area = &g_areaTableDB.m_records[i];

        if (area->m_continentID != mapId) {
            continue;
        }

        lua_newtable(L);
        lua_pushnumber(L, area->m_id);
        lua_setfield(L, -2, "id");
        lua_pushnumber(L, area->m_parentAreaID);
        lua_setfield(L, -2, "parent");
        lua_pushstring(L, area->m_areaName ? area->m_areaName : "");
        lua_setfield(L, -2, "name");
        lua_rawseti(L, -2, ++count);
    }

    return 1;
}

static int32_t Script_EditorSetTileHoles(lua_State* L) {
    if (Runtime::IsMapLoaded()) {
        ChunkEdit::SetTileHoles(Runtime::CameraPosition(), lua_toboolean(L, 1) != 0);
    }

    return 0;
}

static int32_t Script_EditorSetTileArea(lua_State* L) {
    if (Runtime::IsMapLoaded()) {
        ChunkEdit::SetTileAreaId(Runtime::CameraPosition());
    }

    return 0;
}

static int32_t Script_EditorPaintVertex(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4)) {
        luaL_error(L, "Usage: EditorPaintVertex(x, y, z, seconds [, remove])");
        return 0;
    }

    C3Vector pos(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), static_cast<float>(lua_tonumber(L, 3)));
    ChunkEdit::PaintVertexColor(pos, static_cast<float>(lua_tonumber(L, 4)), !lua_toboolean(L, 5));
    ChunkEdit::EndStroke();
    return 0;
}

static int32_t Script_EditorSetHoles(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: EditorSetHoles(x, y, add [, wholeChunk])");
        return 0;
    }

    C3Vector pos(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), 0.0f);
    ChunkEdit::SetHoles(pos, lua_toboolean(L, 4) != 0, lua_toboolean(L, 3) != 0);
    ChunkEdit::EndStroke();
    return 0;
}

static int32_t Script_EditorSetImpass(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: EditorSetImpass(x, y, add)");
        return 0;
    }

    C3Vector pos(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), 0.0f);
    ChunkEdit::SetImpass(pos, lua_toboolean(L, 3) != 0);
    ChunkEdit::EndStroke();
    return 0;
}

static int32_t Script_EditorGetChunkInfo(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: EditorGetChunkInfo(x, y)");
        return 0;
    }

    uint32_t flags;
    uint32_t holes;
    uint32_t areaId;
    uint32_t color;

    if (!ChunkEdit::GetInfo(C3Vector(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), 0.0f), &flags, &holes, &areaId, &color)) {
        return 0;
    }

    lua_pushnumber(L, flags);
    lua_pushnumber(L, holes);
    lua_pushnumber(L, areaId);
    lua_pushnumber(L, color);
    return 4;
}

static int32_t Script_EditorDeleteSelection(lua_State* L) {
    ObjectEdit::DeleteSelection();
    return 0;
}

static int32_t Script_EditorCopySelection(lua_State* L) {
    ObjectEdit::CopySelection();
    return 0;
}

static int32_t Script_EditorPasteObjects(lua_State* L) {
    if (!Runtime::IsMapLoaded()) {
        return 0;
    }

    if (lua_isboolean(L, 1)) {
        if (lua_toboolean(L, 1) && Runtime::CursorValid()) {
            C3Vector cursor = Runtime::Cursor();
            ObjectEdit::Paste(&cursor);
        }
    } else if (lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3)) {
        C3Vector pos(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), static_cast<float>(lua_tonumber(L, 3)));
        ObjectEdit::Paste(&pos);
    } else {
        ObjectEdit::Paste(nullptr);
    }

    return 0;
}

static int32_t Script_EditorDuplicateSelection(lua_State* L) {
    ObjectEdit::Duplicate();
    return 0;
}

static int32_t Script_EditorHasClipboard(lua_State* L) {
    lua_pushboolean(L, ObjectEdit::HasClipboard());
    return 1;
}

static int32_t Script_EditorGetModelCount(lua_State* L) {
    lua_pushnumber(L, ObjectEdit::GetModelCount());
    return 1;
}

static int32_t Script_EditorGetModel(lua_State* L) {
    const char* name = lua_isnumber(L, 1) ? ObjectEdit::GetModel(static_cast<uint32_t>(lua_tonumber(L, 1)) - 1) : nullptr;

    if (!name) {
        return 0;
    }

    lua_pushstring(L, name);
    return 1;
}

static int32_t Script_EditorSetClipboardModel(lua_State* L) {
    lua_pushboolean(L, ObjectEdit::SetClipboardModel(lua_isstring(L, 1) ? lua_tolstring(L, 1, nullptr) : nullptr));
    return 1;
}

static int32_t Script_EditorQuit(lua_State* L) {
    Mode::SetActive(false);
    return 0;
}

static int32_t Script_EditorIsMapLoaded(lua_State* L) {
    lua_pushboolean(L, Runtime::IsMapLoaded());
    return 1;
}

static int32_t Script_EditorSaveTiles(lua_State* L) {
    const char* mode = lua_isstring(L, 1) ? lua_tolstring(L, 1, nullptr) : "";
    EditorSave::MODE saveMode = EditorSave::SAVE_CHANGED;

    if (!SStrCmpI(mode, "all", STORM_MAX_STR)) {
        saveMode = EditorSave::SAVE_ALL;
    } else if (!SStrCmpI(mode, "current", STORM_MAX_STR)) {
        saveMode = EditorSave::SAVE_CURRENT;
    }

    if (!Runtime::IsMapLoaded()) {
        lua_pushnil(L);
        lua_pushstring(L, "No map is loaded");
        return 2;
    }

    Runtime::EndEdits();
    std::string error;
    int32_t saved = EditorSave::Save(saveMode, error);

    if (saved < 0) {
        lua_pushnil(L);
        lua_pushstring(L, error.c_str());
        return 2;
    }

    lua_pushnumber(L, saved);
    return 1;
}

static int32_t Script_EditorClearTimeOfDay(lua_State* L) {
    EditorWorld::ClearTimeOfDay();
    return 0;
}

static int32_t Script_EditorGetCursorPosition(lua_State* L) {
    CRect window = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_theGxDevicePtr->CapsWindowSize(window);
    float height = window.maxY - window.minY;
    int32_t x = 0;
    int32_t y = 0;
    OsInputGetMousePosition(&x, &y);
    float scale = height > 0.0f ? 768.0f / height : 1.0f;
    lua_pushnumber(L, x * scale);
    lua_pushnumber(L, (height - y) * scale);
    return 2;
}

static int32_t Script_EditorGetTime(lua_State* L) {
    lua_pushnumber(L, OsGetAsyncTimeMs() / 1000.0);
    return 1;
}

template <lua_CFunction Function>
static int32_t Gated(lua_State* L) {
    if (!Runtime::HasAccess()) {
        return 0;
    }

    return Function(L);
}

void Register() {
    Lua::RegisterFunction("EditorPrint", Gated<Script_EditorPrint>);
    Lua::RegisterFunction("EditorGetWindowSize", Gated<Script_EditorGetWindowSize>);
    Lua::RegisterFunction("EditorGetEditingMode", Gated<Script_EditorGetEditingMode>);
    Lua::RegisterFunction("EditorSetEditingMode", Gated<Script_EditorSetEditingMode>);
    Lua::RegisterFunction("EditorGetRaiseLower", Gated<Script_EditorGetRaiseLower>);
    Lua::RegisterFunction("EditorSetRaiseLower", Gated<Script_EditorSetRaiseLower>);
    Lua::RegisterFunction("EditorGetFlattenBlur", Gated<Script_EditorGetFlattenBlur>);
    Lua::RegisterFunction("EditorSetFlattenBlur", Gated<Script_EditorSetFlattenBlur>);
    Lua::RegisterFunction("EditorGetViewFlag", Gated<Script_EditorGetViewFlag>);
    Lua::RegisterFunction("EditorSetViewFlag", Gated<Script_EditorSetViewFlag>);
    Lua::RegisterFunction("EditorGetCursor", Gated<Script_EditorGetCursor>);
    Lua::RegisterFunction("EditorGetCamera", Gated<Script_EditorGetCamera>);
    Lua::RegisterFunction("EditorSetCamera", Gated<Script_EditorSetCamera>);
    Lua::RegisterFunction("EditorSetCameraSpeed", Gated<Script_EditorSetCameraSpeed>);
    Lua::RegisterFunction("EditorGetMapInfo", Gated<Script_EditorGetMapInfo>);
    Lua::RegisterFunction("EditorGetAreaName", Gated<Script_EditorGetAreaName>);
    Lua::RegisterFunction("EditorGetStats", Gated<Script_EditorGetStats>);
    Lua::RegisterFunction("EditorUndo", Gated<Script_EditorUndo>);
    Lua::RegisterFunction("EditorRedo", Gated<Script_EditorRedo>);
    Lua::RegisterFunction("EditorGetTimeOfDay", Gated<Script_EditorGetTimeOfDay>);
    Lua::RegisterFunction("EditorSetTimeOfDay", Gated<Script_EditorSetTimeOfDay>);
    Lua::RegisterFunction("EditorGetHeight", Gated<Script_EditorGetHeight>);
    Lua::RegisterFunction("EditorApplyBrush", Gated<Script_EditorApplyBrush>);
    Lua::RegisterFunction("EditorIsMouseButtonDown", Gated<Script_EditorIsMouseButtonDown>);
    Lua::RegisterFunction("EditorGetTextureBrush", Gated<Script_EditorGetTextureBrush>);
    Lua::RegisterFunction("EditorSetTextureBrush", Gated<Script_EditorSetTextureBrush>);
    Lua::RegisterFunction("EditorGetTexture", Gated<Script_EditorGetTexture>);
    Lua::RegisterFunction("EditorSetTexture", Gated<Script_EditorSetTexture>);
    Lua::RegisterFunction("EditorGetTilesetCount", Gated<Script_EditorGetTilesetCount>);
    Lua::RegisterFunction("EditorGetTileset", Gated<Script_EditorGetTileset>);
    Lua::RegisterFunction("EditorGetCursorTextures", Gated<Script_EditorGetCursorTextures>);
    Lua::RegisterFunction("EditorGetSelectionCount", Gated<Script_EditorGetSelectionCount>);
    Lua::RegisterFunction("EditorGetSelection", Gated<Script_EditorGetSelection>);
    Lua::RegisterFunction("EditorClearSelection", Gated<Script_EditorClearSelection>);
    Lua::RegisterFunction("EditorGetObjectTransform", Gated<Script_EditorGetObjectTransform>);
    Lua::RegisterFunction("EditorSetObjectTransform", Gated<Script_EditorSetObjectTransform>);
    Lua::RegisterFunction("EditorObjectSnapToGround", Gated<Script_EditorObjectSnapToGround>);
    Lua::RegisterFunction("EditorGetObjectSetting", Gated<Script_EditorGetObjectSetting>);
    Lua::RegisterFunction("EditorSetObjectSetting", Gated<Script_EditorSetObjectSetting>);
    Lua::RegisterFunction("EditorPickObject", Gated<Script_EditorPickObject>);
    Lua::RegisterFunction("EditorMoveSelection", Gated<Script_EditorMoveSelection>);
    Lua::RegisterFunction("EditorPaintTexture", Gated<Script_EditorPaintTexture>);
    Lua::RegisterFunction("EditorGetTexturesAt", Gated<Script_EditorGetTexturesAt>);
    Lua::RegisterFunction("EditorGetChunkTool", Gated<Script_EditorGetChunkTool>);
    Lua::RegisterFunction("EditorSetChunkTool", Gated<Script_EditorSetChunkTool>);
    Lua::RegisterFunction("EditorGetAreas", Gated<Script_EditorGetAreas>);
    Lua::RegisterFunction("EditorSetTileHoles", Gated<Script_EditorSetTileHoles>);
    Lua::RegisterFunction("EditorSetTileArea", Gated<Script_EditorSetTileArea>);
    Lua::RegisterFunction("EditorPaintVertex", Gated<Script_EditorPaintVertex>);
    Lua::RegisterFunction("EditorSetHoles", Gated<Script_EditorSetHoles>);
    Lua::RegisterFunction("EditorSetImpass", Gated<Script_EditorSetImpass>);
    Lua::RegisterFunction("EditorGetChunkInfo", Gated<Script_EditorGetChunkInfo>);
    Lua::RegisterFunction("EditorDeleteSelection", Gated<Script_EditorDeleteSelection>);
    Lua::RegisterFunction("EditorCopySelection", Gated<Script_EditorCopySelection>);
    Lua::RegisterFunction("EditorPasteObjects", Gated<Script_EditorPasteObjects>);
    Lua::RegisterFunction("EditorDuplicateSelection", Gated<Script_EditorDuplicateSelection>);
    Lua::RegisterFunction("EditorHasClipboard", Gated<Script_EditorHasClipboard>);
    Lua::RegisterFunction("EditorGetModelCount", Gated<Script_EditorGetModelCount>);
    Lua::RegisterFunction("EditorGetModel", Gated<Script_EditorGetModel>);
    Lua::RegisterFunction("EditorSetClipboardModel", Gated<Script_EditorSetClipboardModel>);
    Lua::RegisterFunction("EditorQuit", Gated<Script_EditorQuit>);
    Lua::RegisterFunction("EditorIsMapLoaded", Gated<Script_EditorIsMapLoaded>);
    Lua::RegisterFunction("EditorSaveTiles", Gated<Script_EditorSaveTiles>);
    Lua::RegisterFunction("EditorClearTimeOfDay", Gated<Script_EditorClearTimeOfDay>);
    Lua::RegisterFunction("EditorGetCursorPosition", Gated<Script_EditorGetCursorPosition>);
    Lua::RegisterFunction("EditorGetTime", Gated<Script_EditorGetTime>);
}

}
