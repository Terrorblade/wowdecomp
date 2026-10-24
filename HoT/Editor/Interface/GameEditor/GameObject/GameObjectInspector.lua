-- Inspector of the selected gameobject, the dock panel of the select, move and rotate tools.

GameEditorGameObject = GameEditorGameObject or {};
local GameObject = GameEditorGameObject;
local Common = GameEditorObject;

local Inspector = {};
GameObject.Inspector = Inspector;

local TAB_NAMES = { "Template", "Spawn", "Addon", "Spawn Addon", "Loot" };
local TEMPLATE, SPAWN, TEMPLATE_ADDON, SPAWN_ADDON, LOOT = 1, 2, 3, 4, 5;

local SCOPE_TEMPLATE, SCOPE_SPAWN, SCOPE_TEMPLATE_ADDON, SCOPE_SPAWN_ADDON = 0, 1, 2, 3;

local frame, body, empty, header, tabs;
local pages = {};
local shownEntry, shownSpawn;
local updateElapsed = 0;

GameObject.HELP = table.concat({
	"- Left click a gameobject in the world to select it.",
	"- The move gizmo shows on the selection, drag an arrow to move it.",
	"- With the gizmo off, drag it along the ground. Shift+drag moves it up and down, Alt+drag keeps its height.",
	"- Ctrl+click the selection to switch between the move gizmo, the rotate gizmo and no gizmo. The rotate tool always shows the rings.",
	"- Ctrl+wheel turns it, arrow keys and Page Up/Down nudge it, [ and ] turn it, G drops it to the ground.",
	"- Moves and turns only happen on your client until you press Save move or Ctrl+S, Revert move puts it back.",
	"- Escape cancels a drag, hides the gizmo, then clears the selection.",
}, "\n");

-- Quaternion helpers, ZYX order like the server uses for gameobject rotation.
local function ToEuler(x, y, z, w)
	local roll = math.atan2(2 * (w * x + y * z), 1 - 2 * (x * x + y * y));
	local sinPitch = max(-1, min(1, 2 * (w * y - z * x)));
	local pitch = math.asin(sinPitch);
	local yaw = math.atan2(2 * (w * z + x * y), 1 - 2 * (y * y + z * z));
	return math.deg(yaw), math.deg(pitch), math.deg(roll);
end

local function ToQuaternion(yaw, pitch, roll)
	local cy, sy = math.cos(math.rad(yaw) / 2), math.sin(math.rad(yaw) / 2);
	local cp, sp = math.cos(math.rad(pitch) / 2), math.sin(math.rad(pitch) / 2);
	local cr, sr = math.cos(math.rad(roll) / 2), math.sin(math.rad(roll) / 2);
	local w = cr * cp * cy + sr * sp * sy;
	local x = sr * cp * cy - cr * sp * sy;
	local y = cr * sp * cy + sr * cp * sy;
	local z = cr * cp * sy - sr * sp * cy;
	return x, y, z, w;
end

GameObject.ToEuler = ToEuler;
GameObject.ToQuaternion = ToQuaternion;

-- Addon rows that do not exist yet have no column types in the snapshot, the client then sends every number
-- as an unsigned integer. Floats and negative numbers go as text so the server writes them unchanged.
local function SafeValues(values, rowExists)
	if ( rowExists ) then
		return values;
	end

	local safe = {};

	for column, value in pairs(values) do
		local number = tonumber(value);

		if ( number and (number < 0 or number ~= floor(number)) ) then
			safe[column] = tostring(number);
		else
			safe[column] = value;
		end
	end

	return safe;
end

local function UpdateDirty()
	if ( not frame ) then
		return;
	end

	local names = {};

	for index = 1, #TAB_NAMES do
		local page = pages[index];
		local dirty = page and page:HasChanges() or false;
		tabs:SetDirty(index, dirty);

		if ( dirty ) then
			table.insert(names, TAB_NAMES[index]);
		end
	end

	if ( Common.HasUnsavedMove() and not GameObject.templateEntry ) then
		table.insert(names, 1, "Position");
	end

	if ( #names > 0 ) then
		header.unsaved:SetText("Unsaved: "..table.concat(names, ", ").."   Ctrl+S saves them all");
	else
		header.unsaved:SetText("");
	end
end

local function ShowPage(index)
	for key, page in pairs(pages) do
		if ( key == index ) then
			page:Show();
		else
			page:Hide();
		end
	end
end

local function CreateRotationBox(parent)
	local box = CreateFrame("Frame", "GameEditorGameObjectRotation", parent);
	box:SetHeight(40);
	box.spins = {};
	box.pause = 0;

	local function Apply()
		local x, y, z, w = ToQuaternion(box.spins.yaw.value, box.spins.pitch.value, box.spins.roll.value);

		if ( GameEditor_SetSelectionRotation(x, y, z, w) ) then
			box.pause = 1;
		end
	end

	local specs = {
		{ key = "yaw", label = "Yaw (degrees)", min = -360, max = 360 },
		{ key = "pitch", label = "Pitch", min = -90, max = 90 },
		{ key = "roll", label = "Roll", min = -180, max = 180 },
	};

	for index, spec in ipairs(specs) do
		local spin = NoggitSpinBox_Create(box, 96, spec.min, spec.max, 5, 1, Apply);
		spin:SetPoint("TOPLEFT", box, "TOPLEFT", (index - 1) * 102, -16);
		box.spins[spec.key] = spin;

		local label = Common.Label(box, spec.label, nil, NOGGIT_COLOR_DISABLED);
		label:SetPoint("BOTTOMLEFT", spin, "TOPLEFT", 2, 2);
	end

	local reset = Common.Button(box, "Upright", 70, function()
		NoggitSpinBox_SetValue(box.spins.pitch, 0, true);
		NoggitSpinBox_SetValue(box.spins.roll, 0, true);
		Apply();
	end, "Clear pitch and roll, keep the yaw");
	reset:SetPoint("TOPLEFT", box, "TOPLEFT", 3 * 102 + 4, -16);

	box.Sync = function(self, elapsed)
		if ( self.pause > 0 ) then
			self.pause = self.pause - elapsed;
			return;
		end

		local x, y, z, w = GetSelectedGobRotation();

		if ( not x ) then
			return;
		end

		local yaw, pitch, roll = ToEuler(x, y, z, w);
		local values = { yaw = yaw, pitch = pitch, roll = roll };

		for key, spin in pairs(self.spins) do
			if ( not _G[spin:GetName().."Edit"]:HasFocus() and math.abs(spin.value - values[key]) > 0.05 ) then
				NoggitSpinBox_SetValue(spin, values[key], true);
			end
		end
	end;

	return box;
end

local function BuildSpawnPage(container)
	local page = Common.CreateGridPage(container, {
		table = "gameobject",
		readOnly = { guid = true },
		top = 136,
		onSave = function(changes) GobEditor_SetFields(SCOPE_SPAWN, changes); end,
		onDirty = UpdateDirty,
		saveTooltip = "Save the changed gameobject columns, the spawn respawns with them",
	});

	page.transform = Common.CreateTransformBox(page);
	page.transform:SetPoint("TOPLEFT", page, "TOPLEFT", 2, 0);
	page.transform:SetPoint("TOPRIGHT", page, "TOPRIGHT", 0, 0);

	page.rotation = CreateRotationBox(page);
	page.rotation:SetPoint("TOPLEFT", page, "TOPLEFT", 2, -42);
	page.rotation:SetPoint("TOPRIGHT", page, "TOPRIGHT", 0, -42);

	local actions = CreateFrame("Frame", "GameEditorGameObjectSpawnActions", page);
	actions:SetHeight(22);
	actions:SetPoint("TOPLEFT", page, "TOPLEFT", 0, -90);
	actions:SetPoint("TOPRIGHT", page, "TOPRIGHT", 0, -90);

	local ground = Common.Button(actions, "To ground", 74, function() GameEditor_SnapToGround(); end, "Drop the gameobject onto the ground below it (G)");
	ground:SetPoint("LEFT", actions, "LEFT", 0, 0);
	local toMe = Common.Button(actions, "Move to me", 84, function()
		GobEditor_SetFields(SCOPE_SPAWN, { ["position.save"] = 1 });
	end, "Save the position of your character as the spawn point, the rotation stays");
	toMe:SetPoint("LEFT", ground, "RIGHT", 4, 0);
	local goTo = Common.Button(actions, "Teleport me", 84, function() GameObject.TeleportToSpawn(); end, "Teleport your character to the stored spawn point");
	goTo:SetPoint("LEFT", toMe, "RIGHT", 4, 0);
	local respawn = Common.Button(actions, "Respawn", 70, function()
		GobEditor_SetFields(SCOPE_SPAWN, { respawn = 1 });
	end, "Despawn and load the spawn again from the database (Ctrl+R)");
	respawn:SetPoint("LEFT", goTo, "RIGHT", 4, 0);
	local commit = Common.Button(actions, "Save live", 70, function() GobEditor_Commit(); end, "Send the position and rotation you see right now to the server");
	commit:SetPoint("LEFT", respawn, "RIGHT", 4, 0);

	page.stored = Common.Label(page, "", nil, NOGGIT_COLOR_DISABLED);
	page.stored:SetPoint("TOPLEFT", page, "TOPLEFT", 2, -118);
	page.stored:SetPoint("TOPRIGHT", page, "TOPRIGHT", -2, -118);

	page.extra = { page.transform, page.rotation, actions, page.stored };
	return page;
end

local function BuildAddonPage(container, tableName, scope, keyColumn, defaults, note)
	local page;
	page = Common.CreateGridPage(container, {
		table = tableName,
		hidden = { [keyColumn] = true },
		top = 34,
		onSave = function(changes)
			local values = changes;

			if ( not page.exists ) then
				values = {};

				for column, value in pairs(page.grid:GetValues()) do
					if ( column ~= keyColumn ) then
						values[column] = value;
					end
				end
			end

			GobEditor_SetFields(scope, SafeValues(values, page.exists));
		end,
		onDirty = UpdateDirty,
		saveTooltip = "Save the "..tableName.." row, it is created when it does not exist yet",
	});

	page.state = Common.Paragraph(page, "", NOGGIT_COLOR_TEXT);
	page.state:SetPoint("TOPLEFT", page, "TOPLEFT", 2, -2);
	page.state:SetPoint("TOPRIGHT", page, "TOPRIGHT", -2, -2);

	page.LoadRow = function(self, row, keyText, keepEdits)
		self.exists = row ~= nil;
		local values = {};

		for _, column in ipairs(GameEditorData.Columns[tableName] or {}) do
			values[column] = defaults[column] or 0;
		end

		for column, value in pairs(row or {}) do
			values[column] = value;
		end

		self:Load(values, keepEdits);

		if ( self.exists ) then
			self.state:SetText(format("%s row of %s. %s", tableName, keyText, note));
		else
			self.state:SetText(format("No %s row for %s yet, change a value and Save creates it. %s", tableName, keyText, note));
		end
	end;

	return page;
end

local function Build(parent)
	frame = CreateFrame("Frame", "GameEditorGameObjectInspector", parent);

	empty = Common.CreateEmptyState(frame);

	body = CreateFrame("Frame", "GameEditorGameObjectInspectorBody", frame);
	body:SetAllPoints(frame);

	header = Common.CreateHeader(body);
	header:AddButton("Delete", 56, function() Common.ConfirmDelete("gameobject"); end, "Delete this spawn from the world and the database (Del)");
	header:AddButton("Reload", 56, function() GameObject.Request(); end, "Ask the server for a fresh snapshot (F5)");
	header:AddButton("To ground", 70, function() GameEditor_SnapToGround(); end, "Drop the gameobject onto the ground below it (G)");
	Common.AddMoveButtons(header, UpdateDirty);

	header.close = Common.Button(header, "Close", 56, function() GameObject.CloseTemplate(); end, "Stop editing the template and go back to the selection");
	header.close:SetHeight(20);
	header.close:SetPoint("TOPRIGHT", header, "TOPRIGHT", -6, -6);
	header.close:Hide();
	header.templateReload = Common.Button(header, "Reload", 56, function() GameObject.Request(); end, "Ask the server for a fresh copy of the template (F5)");
	header.templateReload:SetHeight(20);
	header.templateReload:SetPoint("RIGHT", header.close, "LEFT", -4, 0);
	header.templateReload:Hide();

	local container = CreateFrame("Frame", "GameEditorGameObjectInspectorPages", body);
	container:SetPoint("TOPLEFT", header, "BOTTOMLEFT", 6, -30);
	container:SetPoint("BOTTOMRIGHT", body, "BOTTOMRIGHT", -6, 6);

	tabs = Common.CreateTabs(body, TAB_NAMES, ShowPage);
	tabs.tabs[1]:SetPoint("TOPLEFT", header, "BOTTOMLEFT", 6, -4);

	pages[TEMPLATE] = Common.CreateGridPage(container, {
		table = "gameobject_template",
		readOnly = { entry = true },
		top = 20,
		onSave = function(changes) GobEditor_SetFields(SCOPE_TEMPLATE, changes); end,
		onDirty = UpdateDirty,
		saveTooltip = "Save the changed gameobject_template columns, every spawn of the entry respawns with them",
	});
	pages[TEMPLATE].note = Common.Label(pages[TEMPLATE], "", nil, NOGGIT_COLOR_DISABLED);
	pages[TEMPLATE].note:SetPoint("TOPLEFT", pages[TEMPLATE], "TOPLEFT", 2, -2);

	pages[SPAWN] = BuildSpawnPage(container);
	pages[TEMPLATE_ADDON] = BuildAddonPage(container, "gameobject_template_addon", SCOPE_TEMPLATE_ADDON, "entry", {},
		"Faction, flags, loot gold and art kits for every spawn of the entry.");
	pages[SPAWN_ADDON] = BuildAddonPage(container, "gameobject_addon", SCOPE_SPAWN_ADDON, "guid", { parent_rotation3 = 1 },
		"Parent rotation and invisibility of this spawn only.");
	pages[LOOT] = GameEditorLoot.CreateGameObjectPage(container, UpdateDirty);

	for _, page in pairs(pages) do
		page:SetAllPoints(container);
	end

	ShowPage(tabs.current);

	frame:SetScript("OnShow", function() Inspector.Refresh(); end);
	Inspector.Refresh();
	return frame;
end

Inspector.Build = Common.SharedBuilder(Build);

function Inspector.HasChanges()
	for _, page in pairs(pages) do
		if ( page:HasChanges() ) then
			return true;
		end
	end

	return false;
end

function Inspector.SaveAll()
	local saved = GameObject.templateEntry and 0 or Common.SaveAllMoves();

	for index = 1, #TAB_NAMES do
		local page = pages[index];

		if ( page and page:HasChanges() and page:Save() ) then
			saved = saved + 1;
		end
	end

	if ( saved == 0 ) then
		GameEditor_Notify("Nothing to save", "info");
	end
end

function Inspector.RevertAll()
	for _, page in pairs(pages) do
		page:Revert();
	end

	UpdateDirty();
end

local function SetEmpty(state)
	body:Hide();
	empty:Set(state);
end

local function TypeText(data)
	return Common.EnumText("GameObjectType", data.type);
end

-- Template mode swaps the selection buttons for Close and hides everything that moves a spawn.
local function SetTemplateLook(isTemplate)
	for _, button in ipairs(header.buttons) do
		if ( isTemplate ) then
			button:Hide();
		else
			button:Show();
		end
	end

	if ( isTemplate ) then
		header.close:Show();
		header.templateReload:Show();
		header.name:SetPoint("TOPRIGHT", header.templateReload, "TOPLEFT", -6, 0);
	else
		header.close:Hide();
		header.templateReload:Hide();
		header.name:SetPoint("TOPRIGHT", header.buttons[#header.buttons], "TOPLEFT", -6, 0);
	end

	header.hideMove = isTemplate;
	header:UpdateMove();
end

local ShowData;

function Inspector.Refresh()
	if ( not frame ) then
		return;
	end

	if ( GameObject.templateEntry ) then
		local data = GameObject.GetTemplateData();

		if ( not data ) then
			local entry = GameObject.templateEntry;
			SetEmpty({
				title = format("Loading %s...", Common.NameOf("gameobject", entry) or "gameobject template"),
				text = format("Waiting for the server to send gameobject template %d. Editing needs GM level 3 on the server.", entry),
				primary = "Ask again", onPrimary = function() GameObject.Request(); end,
				secondary = "Close", onSecondary = GameObject.CloseTemplate,
				help = GameObject.HELP,
			});
			return;
		end

		ShowData(data);
		return;
	end

	local kind, _, entry, name = GameEditor_GetSelection();

	if ( kind == "creature" ) then
		SetEmpty({
			title = "A creature is selected",
			text = "The Creatures workspace edits creatures. Click a gameobject to edit it here, or find a gameobject template to edit it without a spawn.",
			primary = "Open in Creatures", onPrimary = function() GameEditor_SetWorkspace("creature", "select"); end,
			secondary = "Find template...", onSecondary = GameObject.FindTemplate,
			help = GameObject.HELP,
		});
		return;
	end

	if ( kind ~= "gameobject" ) then
		SetEmpty({
			title = "No gameobject selected",
			text = "Select a gameobject to edit its template, spawn, rotation and addons. Find template edits a template by name or entry without a spawn.",
			primary = "Spawn gameobject...", onPrimary = function() Common.ShowSpawnWindow("gameobject"); end,
			secondary = "Find template...", onSecondary = GameObject.FindTemplate,
			help = GameObject.HELP,
		});
		return;
	end

	local data = GameObject.GetData();

	if ( not data ) then
		SetEmpty({
			title = format("Loading %s...", name or "gameobject"),
			text = format("Waiting for the server to send entry %s. Editing needs GM level 3 on the server.", tostring(entry or "?")),
			primary = "Ask again", onPrimary = function() GameObject.Request(); end,
			help = GameObject.HELP,
		});
		return;
	end

	ShowData(data);
end

ShowData = function(data)
	local isTemplate = data.isTemplate and true or false;

	empty:Hide();
	body:Show();
	SetTemplateLook(isTemplate);

	local spawnKey = isTemplate and ("template"..tostring(data.entry)) or ((data.spawnId or 0) > 0 and data.spawnId or ("temp"..tostring(data.guid)));
	local sameEntry = shownEntry == data.entry;
	local sameSpawn = shownSpawn == spawnKey;

	if ( shownSpawn and not sameSpawn and Inspector.HasChanges() ) then
		GameEditor_Notify("Unsaved gameobject changes were dropped", "info");
	end

	local rotation = data.rotation or {};
	header.name:SetText(data.name ~= "" and data.name or "Gameobject");

	if ( isTemplate ) then
		header.info:SetText(format("Template    Entry %d    %s    Display %d", data.entry or 0, TypeText(data), data.displayId or 0));
		header.position:SetText("Template mode, Close or a click on a gameobject goes back to the selection.");
	else
		header.info:SetText(format("Entry %d    Spawn %s    %s    Display %d    State %s", data.entry or 0, (data.spawnId or 0) > 0 and tostring(data.spawnId) or "none", TypeText(data), data.displayId or 0, Common.EnumText("GOState", data.state)));
	end

	pages[TEMPLATE]:Load(data.template, sameEntry);
	pages[TEMPLATE].note:SetText(format("Changes apply to every gameobject of entry %d.", data.entry or 0));

	local spawn = pages[SPAWN];

	if ( data.spawn ) then
		spawn:SetAvailable(true);
		spawn:Load(data.spawn, sameSpawn);
		spawn.stored:SetText(format("Stored: %s  %s  %s   Map %s   Rotation %s %s %s %s", Common.Float(data.spawn.position_x), Common.Float(data.spawn.position_y), Common.Float(data.spawn.position_z), tostring(data.spawn.map or "?"),
			Common.Float(rotation.x, 3), Common.Float(rotation.y, 3), Common.Float(rotation.z, 3), Common.Float(rotation.w, 3)));
	elseif ( isTemplate ) then
		spawn:SetAvailable(false, "A template has no spawn row. Close the template and select a gameobject in the world to edit its spawn.");
	else
		spawn:SetAvailable(false, "This gameobject is not a database spawn (a summon or a temporary object), so it has no gameobject row. Edit the template instead.");
	end

	for _, widget in ipairs(spawn.extra) do
		if ( data.spawn ) then
			widget:Show();
		else
			widget:Hide();
		end
	end

	pages[TEMPLATE_ADDON]:LoadRow(data.templateAddon, format("entry %d", data.entry or 0), sameEntry);

	if ( data.spawn ) then
		pages[SPAWN_ADDON]:SetAvailable(true);
		pages[SPAWN_ADDON]:LoadRow(data.spawnAddon, format("spawn %d", data.spawnId or 0), sameSpawn);
	elseif ( isTemplate ) then
		pages[SPAWN_ADDON]:SetAvailable(false, "A template has no spawn addon. Close the template and select a gameobject in the world to edit its gameobject_addon row.");
		pages[SPAWN_ADDON].state:SetText("");
	else
		pages[SPAWN_ADDON]:SetAvailable(false, "This gameobject is not a database spawn, so it can not have a gameobject_addon row.");
		pages[SPAWN_ADDON].state:SetText("");
	end

	pages[LOOT]:Load(data);

	shownEntry = data.entry;
	shownSpawn = spawnKey;
	UpdateDirty();
	Inspector.Update(1);
end

function Inspector.Update(elapsed)
	if ( not frame or not body:IsVisible() or GameObject.templateEntry ) then
		return;
	end

	updateElapsed = updateElapsed + elapsed;

	if ( updateElapsed < 0.1 ) then
		return;
	end

	local step = updateElapsed;
	updateElapsed = 0;

	local _, _, _, _, x, y, z, o = GameEditor_GetSelection();

	if ( x ) then
		local text = format("x %s   y %s   z %s   facing %s deg", Common.Float(x), Common.Float(y), Common.Float(z), Common.Float(math.deg(o or 0) % 360, 1));

		if ( GameEditor_IsDragging() ) then
			text = text.."   |cffffcc00dragging|r";
		end

		header.position:SetText(text);
	else
		header.position:SetText("");
	end

	local spawn = pages[SPAWN];

	if ( spawn:IsVisible() and spawn.available ) then
		spawn.transform:Sync();
		spawn.rotation:Sync(step);
	end
end
