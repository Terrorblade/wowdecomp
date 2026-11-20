-- Objects workspace, select a gameobject in the world and edit its rows, spawn and rotation.

GameEditorGameObject = GameEditorGameObject or {};
local GameObject = GameEditorGameObject;
local Common = GameEditorObject;
local Inspector = GameObject.Inspector;

local cache;
local templateCache;

-- Entry of the template the inspector edits without a spawn, nil while it follows the selection.
GameObject.templateEntry = nil;

-- A reloaded UI starts out of template mode, the client keeps the mode across reloads.
if ( GobEditor_SetTemplateMode ) then
	GobEditor_SetTemplateMode(0);
end

function GameObject.GetSelectedGuid()
	local kind, guid = GameEditor_GetSelection();

	if ( kind == "gameobject" ) then
		return guid;
	end
end

-- Snapshot of the selected gameobject, nil while it is not loaded yet.
function GameObject.GetSelectionData()
	local guid = GameObject.GetSelectedGuid();

	if ( not guid ) then
		return nil;
	end

	if ( not cache or cache.guid ~= guid ) then
		cache = GobEditor_GetData("selection");
	end

	if ( cache and cache.guid == guid ) then
		return cache;
	end
end

-- Snapshot of the template being edited without a spawn, nil while it is not loaded yet.
function GameObject.GetTemplateData()
	local entry = GameObject.templateEntry;

	if ( not entry ) then
		return nil;
	end

	if ( not templateCache or templateCache.entry ~= entry ) then
		templateCache = GobEditor_GetData("template");

		if ( templateCache ) then
			templateCache.isTemplate = true;
		end
	end

	if ( templateCache and templateCache.entry == entry ) then
		return templateCache;
	end
end

-- What the inspector edits, the template in template mode and the selection otherwise.
function GameObject.GetData()
	if ( GameObject.templateEntry ) then
		return GameObject.GetTemplateData();
	end

	return GameObject.GetSelectionData();
end

-- Edits the template rows of the entry, no gameobject has to be spawned or selected.
function GameObject.EditTemplate(entry)
	entry = tonumber(entry);

	if ( not entry or entry <= 0 ) then
		return;
	end

	GameObject.templateEntry = entry;
	templateCache = nil;
	GobEditor_SetTemplateMode(entry);
	GobEditor_RequestTemplate(entry);

	if ( GameEditor_GetWorkspace() ~= "gameobject" ) then
		GameEditor_SetWorkspace("gameobject", "select");
	end

	Inspector.Refresh();
end

function GameObject.CloseTemplate()
	if ( not GameObject.templateEntry ) then
		GameEditor_Notify("No template is open", "info");
		return;
	end

	GameObject.templateEntry = nil;
	templateCache = nil;
	GobEditor_SetTemplateMode(0);
	Inspector.Refresh();
end

Common.templateEditors.gameobject = GameObject.EditTemplate;

local function FindTemplate()
	Common.ShowTemplateBrowser("gameobject");
end

GameObject.FindTemplate = FindTemplate;

function GameObject.Request()
	if ( GameObject.templateEntry ) then
		GobEditor_RequestTemplate(GameObject.templateEntry);
		return;
	end

	local guid = GameObject.GetSelectedGuid();

	if ( guid ) then
		GobEditor_Request(guid);
	else
		GameEditor_Notify("Select a gameobject first", "info");
	end
end

function GameObject.Reload()
	if ( not GameObject.templateEntry and not GameObject.GetSelectedGuid() ) then
		GameEditor_Notify("Select a gameobject first", "error");
		return;
	end

	GameObject.Request();
	GameEditor_Notify(GameObject.templateEntry and format("Reloading template %d from the server", GameObject.templateEntry) or "Reloading the gameobject from the server", "info");
end

function GameObject.CommitLive()
	if ( not GobEditor_Commit() ) then
		GameEditor_Notify("Nothing sent, select a gameobject in the world first", "error");
	end
end

function GameObject.NotifyUpright(ok)
	if ( ok ) then
		GameEditor_Notify("Pitch and roll cleared on your client only, Save move saves it", "info");
	else
		GameEditor_Notify("Select a gameobject first", "error");
	end
end

local function WithSpawn(action)
	return function()
		local data = GameObject.GetData();

		if ( GameObject.templateEntry ) then
			GameEditor_Notify("A template has no spawn, close it and select a gameobject in the world", "info");
		elseif ( not data ) then
			GameEditor_Notify("Select a gameobject first", "info");
		elseif ( not data.spawn ) then
			GameEditor_Notify("This gameobject is not a database spawn", "info");
		else
			action(data);
		end
	end;
end

local Respawn = WithSpawn(function() GobEditor_SetFields(1, { respawn = 1 }); end);
local MoveSpawnToMe = WithSpawn(function() GobEditor_SetFields(1, { ["position.save"] = 1 }); end);

GameObject.TeleportToSpawn = WithSpawn(function(data)
	local spawn = data.spawn;
	GameEditor_RunCommand(format(".go xyz %.3f %.3f %.3f %d", tonumber(spawn.position_x) or 0, tonumber(spawn.position_y) or 0, tonumber(spawn.position_z) or 0, tonumber(spawn.map) or 0));
end);

local function IsActive()
	return GameEditor_GetWorkspace() == "gameobject";
end

local events = CreateFrame("Frame", "GameEditorGameObjectEvents");
events:RegisterEvent("HOT_GOB_EDITOR_DATA");
events:RegisterEvent("GAME_EDITOR_SELECTION_CHANGED");
events:RegisterEvent("GAME_EDITOR_GIZMO_CHANGED");
events:SetScript("OnEvent", function(_, event, arg1)
	if ( event == "HOT_GOB_EDITOR_DATA" and tonumber(arg1) == 1 ) then
		templateCache = GobEditor_GetData("template");

		if ( templateCache ) then
			templateCache.isTemplate = true;
			Common.RememberName("gameobject", templateCache.entry, templateCache.name);
		end

		Inspector.Refresh();
	elseif ( event == "HOT_GOB_EDITOR_DATA" ) then
		cache = GobEditor_GetData("selection");

		if ( cache ) then
			Common.RememberName("gameobject", cache.entry, cache.name);
		end

		Inspector.Refresh();
	elseif ( event == "GAME_EDITOR_SELECTION_CHANGED" ) then
		if ( arg1 == "gameobject" and GameObject.templateEntry ) then
			GameObject.CloseTemplate();
		end

		Inspector.Refresh();
	elseif ( event == "GAME_EDITOR_GIZMO_CHANGED" and IsActive() ) then
		GameEditor_RefreshViewToolbar();
	end
end);

local function GizmoIs(mode)
	return function() return GameEditor_GetGizmo() == mode; end;
end

local function SetGizmo(mode)
	return function()
		if ( not GameObject.GetSelectedGuid() ) then
			GameEditor_Notify("Select a gameobject first", "info");
			return;
		end

		GameEditor_SetGizmo(mode);
	end;
end

local function ResetRotation()
	local guid = GameObject.GetSelectedGuid();
	local qx, qy, qz, qw = GetSelectedGobRotation();

	if ( not guid or not qx ) then
		return false;
	end

	local yaw = GameObject.ToEuler(qx, qy, qz, qw);
	local x, y, z, w = GameObject.ToQuaternion(yaw, 0, 0);

	return GameEditor_SetSelectionRotation(x, y, z, w);
end

local separator = { separator = true };

local function BuildMenus()
	return {
		{
			title = "Object",
			{ text = "Selection", disabled = true },
			separator,
			{ text = "Spawn gameobject...", shortcut = "Ctrl+N", onClick = function() Common.ShowSpawnWindow("gameobject"); end },
			{ text = "Find template...", shortcut = "Ctrl+F", onClick = FindTemplate },
			{ text = "Close template", onClick = GameObject.CloseTemplate },
			{ text = "Duplicate", shortcut = "Ctrl+D", onClick = function() Common.Duplicate("gameobject"); end },
			{ text = "Delete spawn", shortcut = "Del", onClick = function() Common.ConfirmDelete("gameobject"); end },
			separator,
			{ text = "Save all changes", shortcut = "Ctrl+S", onClick = function() Inspector.SaveAll(); end },
			{ text = "Revert all changes", onClick = function() Inspector.RevertAll(); end },
			{ text = "Reload from server", shortcut = "F5", onClick = GameObject.Reload },
			{ text = "Respawn", shortcut = "Ctrl+R", onClick = Respawn },
			separator,
			{ text = "Drop to ground", shortcut = "G", onClick = function() Common.DropToGround("gameobject"); end },
			{ text = "Clear pitch and roll", shortcut = "Ctrl+U", onClick = function() GameObject.NotifyUpright(ResetRotation()); end },
			{ text = "Save live position and rotation", onClick = GameObject.CommitLive },
			{ text = "Move spawn to my character", onClick = MoveSpawnToMe },
			{ text = "Teleport me to the spawn", onClick = GameObject.TeleportToSpawn },
		},
	};
end

local VIEW_ITEMS = {
	{ text = "No gizmo", icon = 0xF8DB, checked = GizmoIs("off"), onClick = SetGizmo("off") },
	{ text = "Move gizmo", icon = 0xF8C9, checked = GizmoIs("move"), onClick = SetGizmo("move") },
	{ text = "Rotate gizmo", icon = 0xF8CA, checked = GizmoIs("rotate"), onClick = SetGizmo("rotate") },
	separator,
	{ text = "Spawn gameobject (Ctrl+N)", icon = 0xF8DA, onClick = function() Common.ShowSpawnWindow("gameobject"); end },
};

local KEYS = {
	{ "Ctrl+S", "Save every changed tab of the gameobject" },
	{ "F5", "Reload the gameobject from the server" },
	{ "Ctrl+N", "Spawn a gameobject" },
	{ "Ctrl+F", "Find a gameobject template by name or entry and edit it without a spawn" },
	{ "Ctrl+D", "Spawn a copy next to the selection" },
	{ "Ctrl+R", "Respawn the selected gameobject" },
	{ "Ctrl+U", "Clear pitch and roll of the selection" },
	{ "Delete", "Delete the spawn, asks first" },
};

local HOTKEYS = {
	["CTRL-S"] = function() Inspector.SaveAll(); end,
	["F5"] = GameObject.Reload,
	["CTRL-N"] = function() Common.ShowSpawnWindow("gameobject"); end,
	["CTRL-F"] = FindTemplate,
	["CTRL-D"] = function() Common.Duplicate("gameobject"); end,
	["CTRL-R"] = Respawn,
	["CTRL-U"] = ResetRotation,
	["DELETE"] = function()
		if ( GameObject.templateEntry ) then
			return false;
		end

		Common.ConfirmDelete("gameobject");
	end,
};

local function OnHotkey(key)
	local handler = HOTKEYS[key];

	if ( handler ) then
		return handler() ~= false;
	end

	return false;
end

local function BuildStatus()
	local kind, _, entry, name = GameEditor_GetSelection();

	if ( GameObject.templateEntry ) then
		local data = GameObject.GetTemplateData();
		local cells = { format("Template %s (%d)", data and data.name or "loading", GameObject.templateEntry), "No spawn, edits go to the template rows" };

		if ( Inspector.HasChanges() ) then
			table.insert(cells, "|cffffcc00Unsaved changes|r");
		end

		return cells;
	end

	if ( kind ~= "gameobject" ) then
		return { "No gameobject selected", "Click a gameobject to select it, Ctrl+N spawns one, Ctrl+F finds a template" };
	end

	local cells = { format("%s (%d)", name or "?", entry or 0), "Gizmo: "..(GameEditor_GetGizmo() or "off") };

	if ( GameEditor_IsDragging() ) then
		table.insert(cells, "Dragging, Escape cancels, Save move sends it");
	else
		table.insert(cells, "Drag the arrows to move, Ctrl+click gizmo, Ctrl+wheel turns, G ground");
	end

	if ( Inspector.HasChanges() ) then
		table.insert(cells, "|cffffcc00Unsaved changes|r");
	end

	return cells;
end

GameEditor_RegisterWorkspace({
	key = "gameobject",
	title = "Objects",
	order = 3,
	dockWidth = 440,
	tools = {
		{ key = "select", name = "Select and move", icon = 0xF8DB, build = Inspector.Build },
		{ key = "move", name = "Move gizmo", icon = 0xF8C9, build = Inspector.Build, world = true },
		{ key = "rotate", name = "Rotate gizmo", icon = 0xF8CA, build = Inspector.Build, world = true },
	},
	menus = BuildMenus,
	viewItems = VIEW_ITEMS,
	keys = KEYS,
	onActivate = function()
		if ( GameObject.templateEntry and not GameObject.GetTemplateData() ) then
			GameObject.Request();
		elseif ( not GameObject.templateEntry and GameObject.GetSelectedGuid() and not GameObject.GetData() ) then
			GameObject.Request();
		end

		Inspector.Refresh();
	end,
	onUpdate = function(elapsed)
		Inspector.Update(elapsed);
	end,
	onHotkey = OnHotkey,
	status = BuildStatus,
	onToolSelected = function()
		GameEditor_RefreshViewToolbar();
	end,
});
