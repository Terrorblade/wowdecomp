local ZEROPOINT = 17066.666;
local TILESIZE = 533.33333;

local timeSpeed = 0;
local timeOfDay = 720;
local fpsElapsed = 0;
local fpsFrames = 0;
local fpsText = "FPS: 0 - Average frame time: 0ms";
local activeToolKey;
local panels = {};

local MAP_TOOLS = {
	{ key = "map.raise", name = "Raise / Lower", icon = 0xF89C, mode = 1, create = NoggitTools_CreateRaiseLower },
	{ key = "map.flatten", name = "Flatten | Blur", icon = 0xF89D, mode = 2, create = NoggitTools_CreateFlattenBlur },
	{ key = "map.texture", name = "Texture Painter", icon = 0xF89E, mode = 3, create = NoggitTools_CreateTexturePainter },
	{ key = "map.holes", name = "Hole Cutter", icon = 0xF89F, mode = 5, create = NoggitTools_CreateHoleCutter },
	{ key = "map.area", name = "Area Designator", icon = 0xF8A0, mode = 6, create = NoggitTools_CreateAreaDesignator },
	{ key = "map.impass", name = "Impass Designator", icon = 0xF8A1, mode = 7, noPanel = true },
	{ key = "map.vertex", name = "Vertex Painter", icon = 0xF8A3, mode = 8, create = NoggitTools_CreateVertexPainter },
	{ key = "map.object", name = "Object Editor", icon = 0xF8A4, mode = 4, create = NoggitTools_CreateObjectEditor },
};

local VIEW_ITEMS = {
	{ text = "Doodads", icon = 0xF8AB, flag = "doodads" },
	{ text = "WMOs", icon = 0xF8A9, flag = "wmo" },
	{ text = "Terrain", icon = 0xF8AE, flag = "terrain" },
	{ text = "Water", icon = 0xF8B3, flag = "water" },
	{ text = "Lines", icon = 0xF8AF, flag = "lines" },
	{ text = "Hole lines", icon = 0xF8B7, flag = "holelines" },
	{ text = "Wireframe", icon = 0xF8B0, flag = "wireframe" },
	{ text = "Animations", icon = 0xF8B8, flag = "animations" },
	{ text = "Fog", icon = 0xF8B2, flag = "fog" },
	{ text = "Models with box", icon = 0xF8AC, flag = "modelboxes" },
};

-- F12 opens and closes the game editor, so fog sits on Shift-F12.
local HOTKEY_FLAGS = {
	["F1"] = "doodads",
	["F3"] = "terrain",
	["F4"] = "water",
	["F6"] = "wmo",
	["F7"] = "lines",
	["F10"] = "wireframe",
	["F11"] = "animations",
	["SHIFT-F1"] = "holelines",
	["SHIFT-F12"] = "fog",
};

local function ToolIs(key)
	return activeToolKey == key;
end

local function ToggleViewFlag(flag)
	EditorSetViewFlag(flag, not EditorGetViewFlag(flag));
end

local function FlagChecked(flag)
	return function()
		return EditorGetViewFlag(flag);
	end;
end

local function FlagToggle(flag)
	return function()
		ToggleViewFlag(flag);
	end;
end

local function Header(text)
	return { text = text, disabled = true };
end

local function ScaleCameraSpeed(factor)
	local _, _, _, _, _, speed = EditorGetCamera();
	EditorSetCameraSpeed((speed or 40) * factor);
end

local function ChangeTimeSpeed(delta)
	if ( timeSpeed == 0 and delta > 0 ) then
		timeOfDay = EditorGetTimeOfDay();
	end

	timeSpeed = max(0, timeSpeed + delta);
end

local function ResetTime()
	timeSpeed = 0;
	EditorClearTimeOfDay();
end

-- Returns false when the key does not apply to the current tool, so the shell can use it.
local function ToolAction(toolKey, action)
	return function()
		if ( not ToolIs(toolKey) ) then
			return false;
		end

		action();
	end;
end

local function ObjectAction(action)
	return ToolAction("map.object", action);
end

local function ResetRotation()
	EditorSetObjectTransform(nil, nil, nil, 0, 0, 0, nil);
end

local function PasteAtCursor()
	EditorPasteObjects(true);
end

function MapEditor_Save(mode)
	local saved, err = EditorSaveTiles(mode);

	if ( not saved ) then
		NoggitMessageBox_Critical(err or "Saving failed");
	else
		GameEditor_Notify(format("Saved %d file(s)", saved), saved > 0 and "ok" or "info");
	end
end

local function BuildMenus()
	local separator = { separator = true };

	return {
		{
			title = "Editor",
			{ text = "Save current tile", shortcut = "Ctrl+Shift+S", onClick = function() MapEditor_Save("current"); end },
			{ text = "Save changed tiles", shortcut = "Ctrl+S", onClick = function() MapEditor_Save("changed"); end },
			{ text = "Save all tiles", shortcut = "Ctrl+Shift+A", onClick = function() MapEditor_Save("all"); end },
		},
		{
			title = "Edit",
			Header("Selected object"),
			separator,
			{ text = "Copy", shortcut = "Ctrl+C", onClick = EditorCopySelection },
			{ text = "Paste at cursor", shortcut = "Ctrl+V", onClick = PasteAtCursor },
			{ text = "Duplicate", shortcut = "Ctrl+B", onClick = EditorDuplicateSelection },
			{ text = "Delete", shortcut = "Del", onClick = EditorDeleteSelection },
			{ text = "Reset rotation", shortcut = "Ctrl+R", onClick = ResetRotation },
			{ text = "Set to ground", shortcut = "PgDown", onClick = EditorObjectSnapToGround },
			{ text = "Clear selection", onClick = EditorClearSelection },
			separator,
			Header("State"),
			separator,
			{ text = "Undo", shortcut = "Ctrl+Z", onClick = EditorUndo },
			{ text = "Redo", shortcut = "Ctrl+Shift+Z", onClick = EditorRedo },
		},
		{
			title = "Assist",
			Header("Current ADT"),
			separator,
			{ text = "Remove all holes", shortcut = "T", onClick = function() EditorSetTileHoles(false); end },
			{ text = "Remove all ground", shortcut = "Alt+T", onClick = function() EditorSetTileHoles(true); end },
			{ text = "Set area id on tile", shortcut = "F", onClick = EditorSetTileArea },
		},
		{
			title = "View",
			Header("Drawing"),
			separator,
			{ text = "Doodads", shortcut = "F1", checked = FlagChecked("doodads"), onClick = FlagToggle("doodads") },
			{ text = "Terrain", shortcut = "F3", checked = FlagChecked("terrain"), onClick = FlagToggle("terrain") },
			{ text = "Water", shortcut = "F4", checked = FlagChecked("water"), onClick = FlagToggle("water") },
			{ text = "WMOs", shortcut = "F6", checked = FlagChecked("wmo"), onClick = FlagToggle("wmo") },
			{ text = "Lines", shortcut = "F7", checked = FlagChecked("lines"), onClick = FlagToggle("lines") },
			{ text = "Wireframe", shortcut = "F10", checked = FlagChecked("wireframe"), onClick = FlagToggle("wireframe") },
			{ text = "Toggle Animation", shortcut = "F11", checked = FlagChecked("animations"), onClick = FlagToggle("animations") },
			{ text = "Draw fog", shortcut = "Shift+F12", checked = FlagChecked("fog"), onClick = FlagToggle("fog") },
			{ text = "Hole lines", shortcut = "Shift+F1", checked = FlagChecked("holelines"), onClick = FlagToggle("holelines") },
			{ text = "Models with box", checked = FlagChecked("modelboxes"), onClick = FlagToggle("modelboxes") },
			{ text = "Detail doodads", checked = FlagChecked("detaildoodads"), onClick = FlagToggle("detaildoodads") },
			{ text = "Draw Sky", checked = FlagChecked("sky"), onClick = FlagToggle("sky") },
			{ text = "Show ADT borders", checked = FlagChecked("adtborders"), onClick = FlagToggle("adtborders") },
			separator,
			{ text = "Increase time speed", shortcut = "N", onClick = function() ChangeTimeSpeed(90); end },
			{ text = "Decrease time speed", shortcut = "B", onClick = function() ChangeTimeSpeed(-90); end },
			{ text = "Pause time", shortcut = "J", onClick = function() timeSpeed = 0; end },
			{ text = "Use game time", onClick = ResetTime },
			{ text = "Decrease camera speed", shortcut = "O", onClick = function() ScaleCameraSpeed(0.5); end },
			{ text = "Increase camera speed", shortcut = "P", onClick = function() ScaleCameraSpeed(2); end },
			separator,
			{ text = "Texture palette", onClick = MapEditor_ShowTexturePalette },
			{ text = "Asset browser", onClick = MapEditor_ShowAssetBrowser },
			{ text = "Pos/Rotation Editor", onClick = MapEditor_ShowRotationEditor },
		},
	};
end

local HOTKEYS = {
	["CTRL-S"] = function() MapEditor_Save("changed"); end,
	["CTRL-SHIFT-S"] = function() MapEditor_Save("current"); end,
	["CTRL-SHIFT-A"] = function() MapEditor_Save("all"); end,
	["CTRL-Z"] = EditorUndo,
	["CTRL-SHIFT-Z"] = EditorRedo,
	["CTRL-Y"] = EditorRedo,
	["CTRL-R"] = ObjectAction(ResetRotation),
	["CTRL-C"] = ObjectAction(EditorCopySelection),
	["CTRL-V"] = ObjectAction(PasteAtCursor),
	["CTRL-B"] = ObjectAction(EditorDuplicateSelection),
	["DELETE"] = ObjectAction(EditorDeleteSelection),
	["PAGEDOWN"] = ObjectAction(EditorObjectSnapToGround),
	["T"] = ToolAction("map.holes", function() EditorSetTileHoles(false); end),
	["ALT-T"] = ToolAction("map.holes", function() EditorSetTileHoles(true); end),
	["F"] = ToolAction("map.area", EditorSetTileArea),
	["N"] = function() ChangeTimeSpeed(90); end,
	["B"] = function() ChangeTimeSpeed(-90); end,
	["J"] = function() timeSpeed = 0; end,
	["O"] = function() ScaleCameraSpeed(0.5); end,
	["P"] = function() ScaleCameraSpeed(2); end,
};

local function OnHotkey(key)
	local handler = HOTKEYS[key];

	if ( handler ) then
		return handler() ~= false;
	end

	local flag = HOTKEY_FLAGS[key];

	if ( flag ) then
		ToggleViewFlag(flag);
		return true;
	end

	return false;
end

local function BuildStatus()
	if ( not EditorIsMapLoaded() ) then
		return { "No map loaded" };
	end

	local x, y, z = EditorGetCamera();

	if ( not x ) then
		return {};
	end

	local clientX = ZEROPOINT - y;
	local clientZ = ZEROPOINT - x;
	local minutes = floor(EditorGetTimeOfDay());
	local _, chunks, areas, _, _, _, objects, renderedObjects = EditorGetStats();

	return {
		format("tile: %d %d, client: (%.2f, %.2f, %.2f), server: (%.2f, %.2f, %.2f)", floor(clientX / TILESIZE), floor(clientZ / TILESIZE), clientX, clientZ, z, x, y, z),
		EditorGetAreaName(x, y),
		format("Time: %d:%02d", floor(minutes / 60), mod(minutes, 60)),
		fpsText,
		format("Loaded tiles: %d, Rendered chunks: %d     Loaded objects: %d, Rendered objects: %d", areas, chunks, objects, renderedObjects),
	};
end

local function OnUpdate(elapsed)
	local panel = activeToolKey and panels[activeToolKey];

	if ( panel and panel:IsShown() ) then
		NoggitTools_Sync(panel);
	end

	if ( timeSpeed > 0 ) then
		timeOfDay = mod(timeOfDay + elapsed * timeSpeed / 2, 1440);
		EditorSetTimeOfDay(timeOfDay);
	end

	fpsElapsed = fpsElapsed + elapsed;
	fpsFrames = fpsFrames + 1;

	if ( fpsElapsed > 1 ) then
		local average = fpsElapsed / fpsFrames;
		fpsText = format("FPS: %d - Average frame time: %sms", floor(1 / average), tostring(floor(average * 1000000 + 0.5) / 1000));
		fpsElapsed = 0;
		fpsFrames = 0;
	end
end

local function BuildTools()
	local tools = {};

	for _, tool in ipairs(MAP_TOOLS) do
		local entry = { key = tool.key, name = tool.name, icon = tool.icon, noPanel = tool.noPanel };

		if ( tool.create ) then
			local create = tool.create;
			local key = tool.key;
			entry.build = function(parent)
				local panel = create(parent);
				panels[key] = panel;
				panel:Show();
				return panel;
			end;
		end

		table.insert(tools, entry);
	end

	return tools;
end

local function BuildViewItems()
	local items = {};

	for _, item in ipairs(VIEW_ITEMS) do
		table.insert(items, { text = item.text, icon = item.icon, checked = FlagChecked(item.flag), onClick = FlagToggle(item.flag) });
	end

	return items;
end

GameEditor_RegisterWorkspace({
	key = "map",
	title = "Map",
	order = 1,
	tools = BuildTools(),
	menus = BuildMenus,
	viewItems = BuildViewItems(),
	onActivate = function()
		timeSpeed = 0;
		timeOfDay = EditorGetTimeOfDay() or 720;
	end,
	onDeactivate = function()
		timeSpeed = 0;
		MapEditor_HideWindows();
	end,
	onUpdate = OnUpdate,
	onHotkey = OnHotkey,
	status = BuildStatus,
	onToolSelected = function(toolKey)
		activeToolKey = toolKey;
	end,
});
