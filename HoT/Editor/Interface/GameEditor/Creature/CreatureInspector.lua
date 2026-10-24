-- Inspector of the selected creature, the dock panel of the select, move and rotate tools.

GameEditorCreature = GameEditorCreature or {};
local Creature = GameEditorCreature;
local Common = GameEditorObject;

local Inspector = {};
Creature.Inspector = Inspector;

local TAB_NAMES = { "Template", "Spawn", "Addon", "Spawn Addon", "Equipment", "Vendor", "Loot", "Live" };
local TEMPLATE, SPAWN, TEMPLATE_ADDON, SPAWN_ADDON, EQUIPMENT, VENDOR, LOOT, LIVE = 1, 2, 3, 4, 5, 6, 7, 8;

local frame, body, empty, header, tabs;
local pages = {};
local shownEntry, shownSpawn;
local updateElapsed = 0;

Creature.HELP = table.concat({
	"- Left click a creature in the world to select it.",
	"- The move gizmo shows on the selection, drag an arrow to move it and the blue ring to turn it.",
	"- With the gizmo off, drag it along the ground. Shift+drag moves it up and down, Alt+drag keeps its height.",
	"- Mouse wheel while dragging raises or lowers it, hold Ctrl for small steps.",
	"- Ctrl+click the selection to switch between the move gizmo, the rotate gizmo and no gizmo.",
	"- Ctrl+wheel turns it, arrow keys and Page Up/Down nudge it, [ and ] turn it, G drops it to the ground.",
	"- Moves only happen on your client until you press Save move or Ctrl+S, Revert move puts it back.",
	"- Escape cancels a drag, hides the gizmo, then clears the selection.",
}, "\n");

local function UpdateDirty()
	if ( not frame ) then
		return;
	end

	local names = {};

	for index = 1, #TAB_NAMES do
		local page = pages[index];
		local dirty = page and page.HasChanges and page:HasChanges() or false;
		tabs:SetDirty(index, dirty);

		if ( dirty ) then
			table.insert(names, TAB_NAMES[index]);
		end
	end

	if ( Common.HasUnsavedMove() and not Creature.templateEntry ) then
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

local function BuildSpawnPage(container)
	local page = Common.CreateGridPage(container, {
		table = "creature",
		readOnly = { guid = true },
		top = 94,
		onSave = function(changes) CreatureEditor_SetFields(1, changes); end,
		onDirty = UpdateDirty,
		saveTooltip = "Save the changed creature columns, the spawn respawns with them",
	});

	page.transform = Common.CreateTransformBox(page);
	page.transform:SetPoint("TOPLEFT", page, "TOPLEFT", 2, 0);
	page.transform:SetPoint("TOPRIGHT", page, "TOPRIGHT", 0, 0);

	local actions = CreateFrame("Frame", NoggitUI_NewName("GameEditorCreatureSpawnActions"), page);
	actions:SetHeight(22);
	actions:SetPoint("TOPLEFT", page, "TOPLEFT", 0, -46);
	actions:SetPoint("TOPRIGHT", page, "TOPRIGHT", 0, -46);

	local ground = Common.Button(actions, "To ground", 74, function() GameEditor_SnapToGround(); end, "Drop the creature onto the ground below it (G)");
	ground:SetPoint("LEFT", actions, "LEFT", 0, 0);
	local toMe = Common.Button(actions, "Move to me", 84, function()
		CreatureEditor_SetFields(1, { ["position.save"] = 1 });
	end, "Save the position of your character as the spawn point");
	toMe:SetPoint("LEFT", ground, "RIGHT", 4, 0);
	local goTo = Common.Button(actions, "Teleport me", 84, function()
		CreatureEditor_SetFields(1, { ["position.tome"] = 1 });
	end, "Teleport your character to the stored spawn point");
	goTo:SetPoint("LEFT", toMe, "RIGHT", 4, 0);
	local respawn = Common.Button(actions, "Respawn", 70, function()
		CreatureEditor_SetFields(1, { respawn = 1 });
	end, "Despawn and load the spawn again from the database (Ctrl+R)");
	respawn:SetPoint("LEFT", goTo, "RIGHT", 4, 0);

	page.stored = Common.Label(page, "", nil, NOGGIT_COLOR_DISABLED);
	page.stored:SetPoint("TOPLEFT", page, "TOPLEFT", 2, -74);
	page.stored:SetPoint("TOPRIGHT", page, "TOPRIGHT", -2, -74);

	page.extra = { page.transform, actions, page.stored };
	return page;
end

local LIVE_ROWS = {
	{ "Level", function(data) return tostring(data.level or 0); end },
	{ "Default movement", function(data) return Common.EnumText("MovementType", data.defaultMovement); end },
	{ "Waypoint path", function(data) return (data.assignedPath or 0) > 0 and tostring(data.assignedPath) or "none"; end },
	{ "Stand state", function(data) return Common.EnumText("StandState", data.live.standState); end },
	{ "Anim tier", function(data) return Common.EnumText("AnimTier", data.live.animTier); end },
	{ "Sheath", function(data) return Common.EnumText("SheathState", data.live.sheathState); end },
	{ "Emote state", function(data) return Common.EnumText("Emote", data.live.emote); end },
	{ "Mount display", function(data) local mount = data.live.mount or 0; return mount > 0 and (tostring(mount).."  "..(GameEditor_ResolveRef("dbc:CreatureDisplayInfo", mount) or "")) or "none"; end },
	{ "Visibility flags", function(data) return Common.FlagsText("UnitVisFlags", data.live.visFlags); end },
	{ "PvP flags", function(data) return Common.FlagsText("PvPFlags", data.live.pvpFlags); end },
	{ "Equipment set", function(data) return tostring(data.equipmentId or 0); end },
};

local function BuildLivePage(container)
	local page = CreateFrame("Frame", "GameEditorCreatureLivePage", container);
	page.values = {};

	local intro = Common.Paragraph(page, "What the creature shows right now, including previews that are not saved.");
	intro:SetPoint("TOPLEFT", page, "TOPLEFT", 2, -2);
	intro:SetPoint("TOPRIGHT", page, "TOPRIGHT", -2, -2);
	page.parts = { intro };

	for index, row in ipairs(LIVE_ROWS) do
		local y = -24 - (index - 1) * 18;
		local key = Common.Label(page, row[1], nil, NOGGIT_COLOR_DISABLED);
		key:SetPoint("TOPLEFT", page, "TOPLEFT", 2, y);
		local value = Common.Label(page, "");
		value:SetPoint("TOPLEFT", page, "TOPLEFT", 130, y);
		value:SetPoint("TOPRIGHT", page, "TOPRIGHT", -2, y);
		value:SetHeight(16);
		page.values[index] = value;
		table.insert(page.parts, key);
		table.insert(page.parts, value);
	end

	local auraTop = -24 - #LIVE_ROWS * 18 - 6;
	local auraTitle = Common.Label(page, "Auras", "NoggitFontBold");
	auraTitle:SetPoint("TOPLEFT", page, "TOPLEFT", 2, auraTop);

	local bar = CreateFrame("Frame", "GameEditorCreatureLiveBar", page);
	bar:SetHeight(22);
	bar:SetPoint("BOTTOMLEFT", page, "BOTTOMLEFT", 0, 0);
	bar:SetPoint("BOTTOMRIGHT", page, "BOTTOMRIGHT", 0, 0);

	local toTemplate = Common.Button(bar, "Use for Addon", 104, function()
		tabs:Select(TEMPLATE_ADDON);
		pages[TEMPLATE_ADDON]:LoadLive();
	end, "Copy the live state into the template Addon tab");
	toTemplate:SetPoint("LEFT", bar, "LEFT", 0, 0);

	page.toSpawn = Common.Button(bar, "Use for Spawn Addon", 140, function()
		tabs:Select(SPAWN_ADDON);
		pages[SPAWN_ADDON]:LoadLive();
	end, "Copy the live state into the Spawn Addon tab");
	page.toSpawn:SetPoint("LEFT", toTemplate, "RIGHT", 4, 0);

	local reload = Common.Button(bar, "Reload", 64, function() Creature.Request(); end, "Ask the server for a fresh snapshot (F5)");
	reload:SetPoint("RIGHT", bar, "RIGHT", 0, 0);

	page.auras = NoggitListBox_Create(page, 20, function(row)
		row.id = NoggitUI_FontString(row, "");
		row.id:SetPoint("LEFT", row, "LEFT", 6, 0);
		row.id:SetWidth(60);
		NoggitText_SetColor(row.id, NOGGIT_COLOR_DISABLED);
		row.name = NoggitUI_FontString(row, "");
		row.name:SetPoint("LEFT", row, "LEFT", 70, 0);
		row.name:SetPoint("RIGHT", row, "RIGHT", -110, 0);
		row.tags = NoggitUI_FontString(row, "", nil, "RIGHT");
		row.tags:SetPoint("RIGHT", row, "RIGHT", -4, 0);
		NoggitText_SetColor(row.tags, NOGGIT_COLOR_DISABLED);
	end, function(row, item)
		row.id:SetText(tostring(item.spell));
		row.name:SetText(Common.SpellText(item.spell));
		local tags = {};

		if ( item.passive ) then
			table.insert(tags, "passive");
		end

		if ( item.selfCast ) then
			table.insert(tags, "own");
		end

		row.tags:SetText(table.concat(tags, ", "));
	end);
	page.auras:SetPoint("TOPLEFT", auraTitle, "BOTTOMLEFT", -2, -4);
	page.auras:SetPoint("BOTTOMRIGHT", bar, "TOPRIGHT", 0, 6);

	table.insert(page.parts, auraTitle);
	table.insert(page.parts, bar);
	table.insert(page.parts, page.auras);

	page.unavailable = Common.Paragraph(page, "");
	page.unavailable:SetPoint("TOPLEFT", page, "TOPLEFT", 4, -8);
	page.unavailable:SetPoint("TOPRIGHT", page, "TOPRIGHT", -4, -8);
	page.unavailable:Hide();

	page.SetAvailable = function(self, available, text)
		for _, part in ipairs(self.parts) do
			if ( available ) then
				part:Show();
			else
				part:Hide();
			end
		end

		if ( available ) then
			self.unavailable:Hide();
		else
			self.unavailable:SetText(text or "");
			self.unavailable:Show();
		end
	end;

	page.Load = function(self, data)
		for index, row in ipairs(LIVE_ROWS) do
			local ok, text = pcall(row[2], data);
			self.values[index]:SetText(ok and text or "?");
		end

		NoggitListBox_SetItems(self.auras, data.live and data.live.auras or {}, true);
		Common.SetEnabled(self.toSpawn, (data.spawnId or 0) > 0);
	end;

	return page;
end

local function Build(parent)
	frame = CreateFrame("Frame", "GameEditorCreatureInspector", parent);

	empty = Common.CreateEmptyState(frame);

	body = CreateFrame("Frame", "GameEditorCreatureInspectorBody", frame);
	body:SetAllPoints(frame);

	header = Common.CreateHeader(body);
	header:AddButton("Delete", 56, function() Common.ConfirmDelete("creature"); end, "Delete this spawn from the world and the database (Del)");
	header:AddButton("Reload", 56, function() Creature.Request(); end, "Ask the server for a fresh snapshot (F5)");
	header:AddButton("To ground", 70, function() GameEditor_SnapToGround(); end, "Drop the creature onto the ground below it (G)");
	Common.AddMoveButtons(header, UpdateDirty);

	header.close = Common.Button(header, "Close", 56, function() Creature.CloseTemplate(); end, "Stop editing the template and go back to the selection");
	header.close:SetHeight(20);
	header.close:SetPoint("TOPRIGHT", header, "TOPRIGHT", -6, -6);
	header.close:Hide();
	header.templateReload = Common.Button(header, "Reload", 56, function() Creature.Request(); end, "Ask the server for a fresh copy of the template (F5)");
	header.templateReload:SetHeight(20);
	header.templateReload:SetPoint("RIGHT", header.close, "LEFT", -4, 0);
	header.templateReload:Hide();

	local container = CreateFrame("Frame", "GameEditorCreatureInspectorPages", body);
	container:SetPoint("TOPLEFT", header, "BOTTOMLEFT", 6, -30);
	container:SetPoint("BOTTOMRIGHT", body, "BOTTOMRIGHT", -6, 6);

	tabs = Common.CreateTabs(body, TAB_NAMES, function(index)
		ShowPage(index);
	end);
	tabs.tabs[1]:SetPoint("TOPLEFT", header, "BOTTOMLEFT", 6, -4);

	pages[TEMPLATE] = Common.CreateGridPage(container, {
		table = "creature_template",
		readOnly = { entry = true },
		top = 20,
		onSave = function(changes) CreatureEditor_SetFields(0, changes); end,
		onDirty = UpdateDirty,
		saveTooltip = "Save the changed creature_template columns, every live creature of the entry updates",
	});
	pages[TEMPLATE].note = Common.Label(pages[TEMPLATE], "", nil, NOGGIT_COLOR_DISABLED);
	pages[TEMPLATE].note:SetPoint("TOPLEFT", pages[TEMPLATE], "TOPLEFT", 2, -2);

	pages[SPAWN] = BuildSpawnPage(container);
	pages[TEMPLATE_ADDON] = Creature.CreateAddonPage(container, 0, UpdateDirty);
	pages[SPAWN_ADDON] = Creature.CreateAddonPage(container, 1, UpdateDirty);
	pages[EQUIPMENT] = Creature.CreateEquipmentPage(container);
	pages[VENDOR] = Creature.CreateVendorPage(container, UpdateDirty);
	pages[LOOT] = GameEditorLoot.CreateCreaturePage(container, UpdateDirty);
	pages[LIVE] = BuildLivePage(container);

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
		if ( page.HasChanges and page:HasChanges() ) then
			return true;
		end
	end

	return false;
end

function Inspector.SaveAll()
	local saved = Creature.templateEntry and 0 or Common.SaveAllMoves();

	for index = 1, #TAB_NAMES do
		local page = pages[index];

		if ( page and page.HasChanges and page:HasChanges() and page:Save() ) then
			saved = saved + 1;
		end
	end

	if ( saved == 0 ) then
		GameEditor_Notify("Nothing to save", "info");
	end
end

function Inspector.RevertAll()
	for _, page in pairs(pages) do
		if ( page.Revert ) then
			page:Revert();
		end
	end

	UpdateDirty();
end

local function SetEmpty(state)
	body:Hide();
	empty:Set(state);
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

	if ( Creature.templateEntry ) then
		local data = Creature.GetTemplateData();

		if ( not data ) then
			local entry = Creature.templateEntry;
			SetEmpty({
				title = format("Loading %s...", Common.NameOf("creature", entry) or "creature template"),
				text = format("Waiting for the server to send creature template %d. Editing needs GM level 3 on the server.", entry),
				primary = "Ask again", onPrimary = function() Creature.Request(); end,
				secondary = "Close", onSecondary = Creature.CloseTemplate,
				help = Creature.HELP,
			});
			return;
		end

		ShowData(data);
		return;
	end

	local kind, guid, entry, name = GameEditor_GetSelection();

	if ( kind == "gameobject" ) then
		SetEmpty({
			title = "A gameobject is selected",
			text = "The Objects workspace edits gameobjects. Click a creature to edit it here, or find a creature template to edit it without a spawn.",
			primary = "Open in Objects", onPrimary = function() GameEditor_SetWorkspace("gameobject", "select"); end,
			secondary = "Find template...", onSecondary = Creature.FindTemplate,
			help = Creature.HELP,
		});
		return;
	end

	if ( kind ~= "creature" ) then
		SetEmpty({
			title = "No creature selected",
			text = "Select a creature to edit its template, spawn, addons, equipment and waypoints. Find template edits a template by name or entry without a spawn.",
			primary = "Spawn creature...", onPrimary = function() Common.ShowSpawnWindow("creature"); end,
			secondary = "Find template...", onSecondary = Creature.FindTemplate,
			third = "Select my target", onThird = Common.SelectTarget,
			help = Creature.HELP,
		});
		return;
	end

	local data = Creature.GetData();

	if ( not data ) then
		SetEmpty({
			title = format("Loading %s...", name or "creature"),
			text = format("Waiting for the server to send entry %s. Editing needs GM level 3 on the server.", tostring(entry or "?")),
			primary = "Ask again", onPrimary = function() Creature.Request(); end,
			help = Creature.HELP,
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
		GameEditor_Notify("Unsaved creature changes were dropped", "info");
	end

	header.name:SetText(data.name ~= "" and data.name or "Creature");

	if ( isTemplate ) then
		header.info:SetText(format("Template    Entry %d    No spawn selected", data.entry or 0));
		header.position:SetText("Template mode, Close or a click on a creature goes back to the selection.");
	else
		header.info:SetText(format("Entry %d    Spawn %s    Level %d    Guid %s", data.entry or 0, (data.spawnId or 0) > 0 and tostring(data.spawnId) or "none", data.level or 0, tostring(data.guid)));
	end

	pages[TEMPLATE]:Load(data.template, sameEntry);
	pages[TEMPLATE].note:SetText(format("Changes apply to every creature of entry %d.", data.entry or 0));

	local spawn = pages[SPAWN];

	if ( data.spawn ) then
		spawn:SetAvailable(true);
		spawn:Load(data.spawn, sameSpawn);
		spawn.stored:SetText(format("Stored spawn point: %s  %s  %s   Map %s", Common.Float(data.spawn.position_x), Common.Float(data.spawn.position_y), Common.Float(data.spawn.position_z), tostring(data.spawn.map or "?")));
	elseif ( isTemplate ) then
		spawn:SetAvailable(false, "A template has no spawn row. Close the template and select a creature in the world to edit its spawn.");
	else
		spawn:SetAvailable(false, "This creature is not a database spawn (a summon or a temporary creature), so it has no creature row and can not be moved or saved. Edit the template instead.");
	end

	for _, widget in ipairs(spawn.extra) do
		if ( data.spawn ) then
			widget:Show();
		else
			widget:Hide();
		end
	end

	pages[TEMPLATE_ADDON]:Load(data, sameEntry);
	pages[SPAWN_ADDON]:Load(data, sameSpawn);
	pages[EQUIPMENT]:Load(data);
	pages[VENDOR]:Load(data);
	pages[LOOT]:Load(data);

	if ( isTemplate ) then
		pages[LIVE]:SetAvailable(false, "A template has no live creature. Close the template and select a creature in the world to see what it shows right now.");
	else
		pages[LIVE]:SetAvailable(true);
		pages[LIVE]:Load(data);
	end

	shownEntry = data.entry;
	shownSpawn = spawnKey;
	UpdateDirty();
	Inspector.Update(1);
end

function Inspector.Update(elapsed)
	if ( not frame or not body:IsVisible() or Creature.templateEntry ) then
		return;
	end

	updateElapsed = updateElapsed + elapsed;

	if ( updateElapsed < 0.1 ) then
		return;
	end

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

	if ( pages[SPAWN]:IsVisible() ) then
		pages[SPAWN].transform:Sync();
	end
end

function Inspector.SelectTab(index)
	if ( tabs ) then
		tabs:Select(index);
	end
end
