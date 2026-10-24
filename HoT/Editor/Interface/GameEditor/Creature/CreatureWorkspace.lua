-- Creatures workspace, select a creature in the world and edit its rows, spawn and waypoints.

GameEditorCreature = GameEditorCreature or {};
local Creature = GameEditorCreature;
local Common = GameEditorObject;
local Inspector = Creature.Inspector;
local Waypoints = Creature.Waypoints;

local cache;
local templateCache;
local activeTool;

-- Entry of the template the inspector edits without a spawn, nil while it follows the selection.
Creature.templateEntry = nil;

-- A reloaded UI starts out of template mode, the client keeps the mode across reloads.
if ( CreatureEditor_SetTemplateMode ) then
	CreatureEditor_SetTemplateMode(0);
end

function Creature.GetSelectedGuid()
	local kind, guid = GameEditor_GetSelection();

	if ( kind == "creature" ) then
		return guid;
	end
end

-- Snapshot of the selected creature, nil while it is not loaded yet.
function Creature.GetSelectionData()
	local guid = Creature.GetSelectedGuid();

	if ( not guid ) then
		return nil;
	end

	if ( not cache or cache.guid ~= guid ) then
		cache = CreatureEditor_GetData("selection");
	end

	if ( cache and cache.guid == guid ) then
		return cache;
	end
end

-- Snapshot of the template being edited without a spawn, nil while it is not loaded yet.
function Creature.GetTemplateData()
	local entry = Creature.templateEntry;

	if ( not entry ) then
		return nil;
	end

	if ( not templateCache or templateCache.entry ~= entry ) then
		templateCache = CreatureEditor_GetData("template");

		if ( templateCache ) then
			templateCache.isTemplate = true;
		end
	end

	if ( templateCache and templateCache.entry == entry ) then
		return templateCache;
	end
end

-- What the inspector edits, the template in template mode and the selection otherwise.
function Creature.GetData()
	if ( Creature.templateEntry ) then
		return Creature.GetTemplateData();
	end

	return Creature.GetSelectionData();
end

function Creature.IsTemplateMode()
	return Creature.templateEntry ~= nil;
end

-- Edits the template rows of the entry, no creature has to be spawned or selected.
function Creature.EditTemplate(entry)
	entry = tonumber(entry);

	if ( not entry or entry <= 0 ) then
		return;
	end

	Creature.templateEntry = entry;
	templateCache = nil;
	CreatureEditor_SetTemplateMode(entry);
	CreatureEditor_RequestTemplate(entry);

	if ( GameEditor_GetWorkspace() ~= "creature" or GameEditor_GetActiveTool() == "waypoint" ) then
		GameEditor_SetWorkspace("creature", "select");
	end

	Inspector.Refresh();
end

function Creature.CloseTemplate()
	if ( not Creature.templateEntry ) then
		return;
	end

	Creature.templateEntry = nil;
	templateCache = nil;
	CreatureEditor_SetTemplateMode(0);
	Inspector.Refresh();
end

Common.templateEditors.creature = Creature.EditTemplate;

local function FindTemplate()
	Common.ShowTemplateBrowser("creature");
end

Creature.FindTemplate = FindTemplate;

function Creature.Request()
	if ( Creature.templateEntry ) then
		CreatureEditor_RequestTemplate(Creature.templateEntry);
		return;
	end

	local guid = Creature.GetSelectedGuid();

	if ( not guid ) then
		GameEditor_Notify("Select a creature first", "info");
		return;
	end

	local path = Waypoints.GetViewPath();

	if ( path ) then
		CreatureEditor_Request(guid, path);
	else
		CreatureEditor_Request(guid);
	end
end

local function WithSpawn(action)
	return function()
		local data = Creature.GetData();

		if ( Creature.templateEntry ) then
			GameEditor_Notify("A template has no spawn, close it and select a creature in the world", "info");
		elseif ( not data ) then
			GameEditor_Notify("Select a creature first", "info");
		elseif ( not data.spawn ) then
			GameEditor_Notify("This creature is not a database spawn", "info");
		else
			action(data);
		end
	end;
end

local Respawn = WithSpawn(function() CreatureEditor_SetFields(1, { respawn = 1 }); end);
local MoveSpawnToMe = WithSpawn(function() CreatureEditor_SetFields(1, { ["position.save"] = 1 }); end);
local TeleportToSpawn = WithSpawn(function() CreatureEditor_SetFields(1, { ["position.tome"] = 1 }); end);

local function IsActive()
	return GameEditor_GetWorkspace() == "creature";
end

local events = CreateFrame("Frame", "GameEditorCreatureEvents");
events:RegisterEvent("HOT_CREATURE_EDITOR_DATA");
events:RegisterEvent("HOT_CREATURE_EDITOR_POINT_SELECTED");
events:RegisterEvent("GAME_EDITOR_SELECTION_CHANGED");
events:RegisterEvent("GAME_EDITOR_GIZMO_CHANGED");
events:SetScript("OnEvent", function(_, event, arg1)
	if ( event == "HOT_CREATURE_EDITOR_DATA" and tonumber(arg1) == 1 ) then
		templateCache = CreatureEditor_GetData("template");

		if ( templateCache ) then
			templateCache.isTemplate = true;
			Common.RememberName("creature", templateCache.entry, templateCache.name);
		end

		Inspector.Refresh();
	elseif ( event == "HOT_CREATURE_EDITOR_DATA" ) then
		cache = CreatureEditor_GetData("selection");

		if ( cache ) then
			Common.RememberName("creature", cache.entry, cache.name);
		end

		Waypoints.OnData();
		Inspector.Refresh();
	elseif ( event == "HOT_CREATURE_EDITOR_POINT_SELECTED" ) then
		Waypoints.OnPointSelected(tonumber(arg1) or 0);
	elseif ( event == "GAME_EDITOR_SELECTION_CHANGED" ) then
		if ( arg1 == "creature" and Creature.templateEntry ) then
			Creature.CloseTemplate();
		end

		Inspector.Refresh();
		Waypoints.Refresh();
	elseif ( event == "GAME_EDITOR_GIZMO_CHANGED" and IsActive() ) then
		GameEditor_RefreshViewToolbar();
	end
end);

local function GizmoIs(mode)
	return function() return GameEditor_GetGizmo() == mode; end;
end

local function SetGizmo(mode)
	return function()
		if ( not Creature.GetSelectedGuid() ) then
			GameEditor_Notify("Select a creature first", "info");
			return;
		end

		GameEditor_SetGizmo(mode);
	end;
end

local function ToggleShowPath()
	Waypoints.SetShowAlways(not Waypoints.IsShowingAlways());
end

local separator = { separator = true };

local function BuildMenus()
	return {
		{
			title = "Creature",
			{ text = "Selection", disabled = true },
			separator,
			{ text = "Spawn creature...", shortcut = "Ctrl+N", onClick = function() Common.ShowSpawnWindow("creature"); end },
			{ text = "Find template...", shortcut = "Ctrl+F", onClick = FindTemplate },
			{ text = "Close template", onClick = Creature.CloseTemplate },
			{ text = "Duplicate", shortcut = "Ctrl+D", onClick = function() Common.Duplicate("creature"); end },
			{ text = "Select my target", onClick = Common.SelectTarget },
			{ text = "Delete spawn", shortcut = "Del", onClick = function() Common.ConfirmDelete("creature"); end },
			separator,
			{ text = "Save all changes", shortcut = "Ctrl+S", onClick = function() Inspector.SaveAll(); end },
			{ text = "Revert all changes", onClick = function() Inspector.RevertAll(); end },
			{ text = "Reload from server", shortcut = "F5", onClick = Creature.Request },
			{ text = "Respawn", shortcut = "Ctrl+R", onClick = Respawn },
			separator,
			{ text = "Drop to ground", shortcut = "G", onClick = function() GameEditor_SnapToGround(); end },
			{ text = "Move spawn to my character", onClick = MoveSpawnToMe },
			{ text = "Teleport me to the spawn", onClick = TeleportToSpawn },
			separator,
			{ text = "Edit waypoints", onClick = function() GameEditor_SelectTool("waypoint"); end },
			{ text = "Show the path in every tool", checked = function() return Waypoints.IsShowingAlways(); end, onClick = ToggleShowPath },
		},
	};
end

local VIEW_ITEMS = {
	{ text = "No gizmo", icon = 0xF8DB, checked = GizmoIs("off"), onClick = SetGizmo("off") },
	{ text = "Move gizmo", icon = 0xF8C9, checked = GizmoIs("move"), onClick = SetGizmo("move") },
	{ text = "Rotate gizmo", icon = 0xF8CA, checked = GizmoIs("rotate"), onClick = SetGizmo("rotate") },
	separator,
	{ text = "Show the waypoint path in every tool", icon = 0xF8A1, checked = function() return Waypoints.IsShowingAlways(); end, onClick = ToggleShowPath },
	separator,
	{ text = "Spawn creature (Ctrl+N)", icon = 0xF8B8, onClick = function() Common.ShowSpawnWindow("creature"); end },
};

local KEYS = {
	{ "Ctrl+S", "Save every changed tab of the creature" },
	{ "F5", "Reload the creature from the server" },
	{ "Ctrl+N", "Spawn a creature" },
	{ "Ctrl+F", "Find a creature template by name or entry and edit it without a spawn" },
	{ "Ctrl+D", "Spawn a copy next to the selection" },
	{ "Ctrl+R", "Respawn the selected creature" },
	{ "Delete", "Delete the spawn (asks first), in the waypoint tool it deletes the selected node" },
	{ "Insert", "Waypoint tool, add a node under the mouse" },
	{ "Shift + click", "Waypoint tool, add a node on the ground" },
	{ "Drag a node", "Waypoint tool, move the node, saved on release" },
};

local HOTKEYS = {
	["CTRL-S"] = function() Inspector.SaveAll(); end,
	["F5"] = Creature.Request,
	["CTRL-N"] = function() Common.ShowSpawnWindow("creature"); end,
	["CTRL-F"] = FindTemplate,
	["CTRL-D"] = function() Common.Duplicate("creature"); end,
	["CTRL-R"] = Respawn,
	["DELETE"] = function()
		if ( activeTool == "waypoint" ) then
			return Waypoints.DeleteSelected();
		end

		if ( Creature.templateEntry ) then
			return false;
		end

		Common.ConfirmDelete("creature");
	end,
	["INSERT"] = function()
		if ( activeTool ~= "waypoint" ) then
			return false;
		end

		Waypoints.AddAtCursor();
	end,
};

local function OnHotkey(key)
	local handler = HOTKEYS[key];

	if ( handler ) then
		return handler() ~= false;
	end

	return false;
end

local GIZMO_NAMES = { off = "off", move = "move", rotate = "rotate" };

local function BuildStatus()
	local kind, _, entry, name = GameEditor_GetSelection();

	if ( Creature.templateEntry and activeTool ~= "waypoint" ) then
		local data = Creature.GetTemplateData();
		local cells = { format("Template %s (%d)", data and data.name or "loading", Creature.templateEntry), "No spawn, edits go to the template rows" };

		if ( Inspector.HasChanges() ) then
			table.insert(cells, "|cffffcc00Unsaved changes|r");
		end

		return cells;
	end

	if ( kind ~= "creature" ) then
		return { "No creature selected", "Click a creature to select it, Ctrl+N spawns one, Ctrl+F finds a template" };
	end

	local cells = { format("%s (%d)", name or "?", entry or 0), "Gizmo: "..(GIZMO_NAMES[GameEditor_GetGizmo()] or "off") };

	if ( GameEditor_IsDragging() ) then
		table.insert(cells, "Dragging, Escape cancels, Save move sends it");
	elseif ( activeTool == "waypoint" ) then
		table.insert(cells, Waypoints.StatusText() or "Loading");
		table.insert(cells, "Shift+click adds a node, drag moves one");
	else
		table.insert(cells, "Drag the arrows to move, the ring turns it, Ctrl+click gizmo, G ground");
	end

	if ( Inspector.HasChanges() ) then
		table.insert(cells, "|cffffcc00Unsaved changes|r");
	end

	return cells;
end

GameEditor_RegisterWorkspace({
	key = "creature",
	title = "Creatures",
	order = 2,
	dockWidth = 540,
	tools = {
		{ key = "select", name = "Select and move", icon = 0xF8DB, build = Inspector.Build },
		{ key = "move", name = "Move gizmo", icon = 0xF8C9, build = Inspector.Build },
		{ key = "rotate", name = "Rotate gizmo", icon = 0xF8CA, build = Inspector.Build },
		{ key = "waypoint", name = "Waypoints", icon = 0xF8D8, build = Waypoints.Build },
	},
	menus = BuildMenus,
	viewItems = VIEW_ITEMS,
	keys = KEYS,
	onActivate = function()
		if ( Creature.templateEntry and not Creature.GetTemplateData() ) then
			Creature.Request();
		elseif ( not Creature.templateEntry and Creature.GetSelectedGuid() and not Creature.GetData() ) then
			Creature.Request();
		end

		Inspector.Refresh();
		Waypoints.Refresh();
	end,
	onUpdate = function(elapsed)
		Inspector.Update(elapsed);
	end,
	onHotkey = OnHotkey,
	status = BuildStatus,
	onToolSelected = function(toolKey)
		activeTool = toolKey;
		GameEditor_RefreshViewToolbar();
	end,
});
