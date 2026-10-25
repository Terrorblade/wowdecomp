-- Pieces shared by the creature and gameobject workspaces, loaded before both.

GameEditorObject = GameEditorObject or {};
local Common = GameEditorObject;

local Data = GameEditorData or {};

local RECENT_MAX = 15;

Common.names = { creature = {}, gameobject = {} };
Common.recent = { creature = {}, gameobject = {} };
Common.edited = { creature = {}, gameobject = {} };
Common.templateEditors = Common.templateEditors or {};

local NOUN = { creature = "creature", gameobject = "gameobject" };

local LOOKUP_LINK = {
	creature = "|Hcreature_entry:(%d+)|h%[(.-)%]|h",
	gameobject = "|Hgameobject_entry:(%d+)|h%[(.-)%]|h",
};

-- Small helpers -------------------------------------------------------------------------------------------

function Common.Label(parent, text, font, color)
	local label = NoggitUI_FontString(parent, text or "", font);

	if ( color ) then
		NoggitText_SetColor(label, color);
	end

	return label;
end

-- Multi line text, anchor it with TOPLEFT and TOPRIGHT so it wraps.
function Common.Paragraph(parent, text, color)
	local label = Common.Label(parent, text, nil, color or NOGGIT_COLOR_DISABLED);
	label:SetJustifyV("TOP");
	return label;
end

function Common.Button(parent, text, width, onClick, tooltip)
	local button = NoggitPushButton_Create(parent, text, width, onClick);
	button:SetHeight(22);

	if ( tooltip ) then
		button:SetScript("OnEnter", function(self)
			NoggitButton_SetHover(self, true);
			NoggitTooltip_Show(self, tooltip);
		end);
		button:SetScript("OnLeave", function(self)
			NoggitButton_SetHover(self, false);
			NoggitTooltip_Hide();
		end);
	end

	return button;
end

function Common.SetEnabled(widget, enabled)
	if ( enabled ) then
		widget:Enable();
	else
		widget:Disable();
	end
end

function Common.Tooltip(frame, text)
	frame:SetScript("OnEnter", function(self) NoggitTooltip_Show(self, text); end);
	frame:SetScript("OnLeave", NoggitTooltip_Hide);
end

function Common.Float(value, decimals)
	return format("%."..(decimals or 2).."f", tonumber(value) or 0);
end

function Common.Same(a, b)
	if ( type(a) == "number" or type(b) == "number" ) then
		local x, y = tonumber(a), tonumber(b);

		if ( not x or not y ) then
			return x == y;
		end

		return math.abs(x - y) <= 0.0001 * max(1, math.abs(x));
	end

	return tostring(a or "") == tostring(b or "");
end

function Common.Copy(source)
	local copy = {};

	for key, value in pairs(source or {}) do
		copy[key] = value;
	end

	return copy;
end

function Common.EnumText(enumName, value)
	local enum = Data.Enums and Data.Enums[enumName];
	local name = enum and enum[tonumber(value) or value];

	if ( name ) then
		return format("%s (%s)", tostring(name), tostring(value));
	end

	return tostring(value or 0);
end

function Common.FlagsText(flagsName, value)
	local flags = Data.Flags and Data.Flags[flagsName];
	local number = tonumber(value) or 0;
	local names = {};

	for _, flag in ipairs(flags or {}) do
		if ( GameEditor_HasBit(number, flag.bit) ) then
			table.insert(names, flag.alias or flag.name);
		end
	end

	if ( #names == 0 ) then
		return number == 0 and "none" or format("0x%X", number);
	end

	return table.concat(names, ", ");
end

function Common.SpellText(spellId)
	local name, rank = GetSpellInfo(spellId);

	if ( not name ) then
		return "unknown spell";
	end

	if ( rank and rank ~= "" ) then
		return name.." ("..rank..")";
	end

	return name;
end

function Common.CanSearchSpells()
	return GameEditor_CanSearchRef("spell") and true or false;
end

-- Grid values from the server, edits survive when the server value of that column did not change.
function Common.LoadGrid(grid, values, keepEdits)
	values = values or {};
	local edits = keepEdits and grid:GetChanges() or nil;
	local previous = grid.original or {};
	grid:SetValues(values);

	if ( edits ) then
		for column, value in pairs(edits) do
			if ( Common.Same(values[column], previous[column]) ) then
				grid:SetValue(column, value, true);
			end
		end
	end
end

-- Names ------------------------------------------------------------------------------------------------------

function Common.RememberName(kind, entry, name)
	entry = tonumber(entry);

	if ( entry and entry > 0 and name and name ~= "" and Common.names[kind] ) then
		Common.names[kind][entry] = name;
		GameEditor_TemplateListPut(kind, entry, name);
	end
end

function Common.NameOf(kind, entry)
	entry = tonumber(entry) or 0;
	return Common.names[kind] and Common.names[kind][entry] or GameEditor_TemplateName(kind, entry);
end

local function OnSystemMessage(message)
	if ( type(message) ~= "string" ) then
		return;
	end

	for kind, pattern in pairs(LOOKUP_LINK) do
		for entry, name in string.gmatch(message, pattern) do
			entry = tonumber(entry);
			Common.RememberName(kind, entry, name);
		end
	end
end

GameEditor_RefResolvers = GameEditor_RefResolvers or {};
GameEditor_RefSearchers = GameEditor_RefSearchers or {};

for kind in pairs(NOUN) do
	if ( not GameEditor_RefResolvers[kind] ) then
		GameEditor_RefResolvers[kind] = function(value)
			return Common.NameOf(kind, value);
		end;
	end

	GameEditor_RefLists[kind] = function()
		return GameEditor_TemplateList(kind);
	end;
end

-- Spawning and deleting ------------------------------------------------------------------------------------

local function Remember(kind, entry, store)
	local recent = (store or Common.recent)[kind];

	for index = #recent, 1, -1 do
		if ( recent[index] == entry ) then
			table.remove(recent, index);
		end
	end

	table.insert(recent, 1, entry);

	while ( #recent > RECENT_MAX ) do
		table.remove(recent);
	end
end

function Common.Spawn(kind, entry, atCharacter)
	entry = tonumber(entry);

	if ( not entry or entry <= 0 ) then
		GameEditor_Notify("Enter the entry to spawn first", "info");
		return false;
	end

	local ok;

	if ( atCharacter ) then
		local x, y, z, o = CreatureEditor_GetPlayerPosition();

		if ( not x ) then
			GameEditor_Notify("Your character position is not available", "error");
			return false;
		end

		ok = GameEditor_Spawn(kind, entry, x, y, z, o);
	else
		ok = GameEditor_Spawn(kind, entry);
	end

	if ( ok ) then
		Remember(kind, entry);
	else
		GameEditor_Notify("Spawning failed, check your GM level", "error");
	end

	return ok;
end

-- Spawns another copy of the selection next to it.
function Common.Duplicate(kind)
	local selected, _, entry, _, x, y, z, o = GameEditor_GetSelection();

	if ( selected ~= kind or not x or not entry or entry == 0 ) then
		GameEditor_Notify("Select a "..NOUN[kind].." to duplicate first", "info");
		return;
	end

	local side = (o or 0) + math.pi / 2;

	if ( GameEditor_Spawn(kind, entry, x + math.cos(side) * 2, y + math.sin(side) * 2, z, o) ) then
		Remember(kind, entry);
	end
end

function Common.ConfirmDelete(kind)
	local selected, guid, entry, name = GameEditor_GetSelection();

	if ( selected ~= kind or not guid ) then
		GameEditor_Notify("Select a "..NOUN[kind].." to delete first", "info");
		return;
	end

	NoggitMessageBox_Show("Delete spawn", format("Delete the %s %s (entry %s) from the world and from the database?\n\nThis can not be undone.", NOUN[kind], name or "?", tostring(entry or "?")), {
		{ text = "Delete", onClick = function() GameEditor_DeleteSpawn(kind, guid); end },
		{ text = "Cancel" },
	});
end

-- Selects the creature or gameobject the character is targeting.
function Common.SelectTarget()
	local guid = CreatureEditor_GetTargetGuid();

	if ( not guid or not GameEditor_Select(guid) ) then
		GameEditor_Notify("Target a creature with your character first", "info");
	end
end

-- Opens the template of the entry in its workspace, no spawn needed.
function Common.EditTemplate(kind, entry)
	entry = tonumber(entry);

	if ( not entry or entry <= 0 ) then
		GameEditor_Notify("Enter the entry to edit first", "info");
		return false;
	end

	local editor = Common.templateEditors[kind];

	if ( not editor ) then
		return false;
	end

	Remember(kind, entry, Common.edited);
	editor(entry);
	return true;
end

-- Template browser, spawns or edits the templates found by name or entry -----------------------------------

local spawnWindow;

local function SpawnItems()
	local window = spawnWindow;
	local kind = window.kind;
	local text = strtrim(window.search:GetText() or "");
	local all = GameEditor_TemplateList(kind);

	if ( text ~= "" ) then
		return GameEditor_FilterList(all, text);
	end

	-- Recent entries lead the full list.
	local items, seen = {}, {};
	local recent = window.editMode and Common.edited[kind] or Common.recent[kind];

	for _, entry in ipairs(recent) do
		if ( not seen[entry] ) then
			seen[entry] = true;
			table.insert(items, { value = entry, text = Common.NameOf(kind, entry) or "", tag = window.editMode and "edited" or "spawned" });
		end
	end

	if ( #items == 0 ) then
		return all;
	end

	for _, item in ipairs(all) do
		if ( not seen[item.value] ) then
			table.insert(items, item);
		end
	end

	return items;
end

local function SpawnRefresh(keepSelection)
	if ( not spawnWindow ) then
		return;
	end

	local items = SpawnItems();
	NoggitListBox_SetItems(spawnWindow.list, items, keepSelection);

	if ( not spawnWindow.list.selected and #items > 0 ) then
		spawnWindow.list.selected = 1;
		NoggitListBox_Refresh(spawnWindow.list);
	end

	local all, loading = GameEditor_TemplateList(spawnWindow.kind);

	if ( loading ) then
		spawnWindow.status:SetText(#all > 0 and format("%d of %d, still loading", #items, #all) or "Loading the list from the server");
	elseif ( #items == #all ) then
		spawnWindow.status:SetText(format("%d templates", #all));
	else
		spawnWindow.status:SetText(format("%d of %d", #items, #all));
	end
end

local function SpawnFilter()
	SpawnRefresh();
end

for kind in pairs(NOUN) do
	GameEditor_OnTemplateList(kind, function()
		if ( spawnWindow and spawnWindow.kind == kind and spawnWindow:IsShown() ) then
			SpawnRefresh(true);
		end
	end);
end

local function SelectedEntry()
	local list = spawnWindow.list;
	local item = list.items[list.selected or 0];
	return item and item.value or tonumber(strtrim(spawnWindow.search:GetText() or ""));
end

local function SpawnSelected()
	Common.Spawn(spawnWindow.kind, SelectedEntry(), spawnWindow.atCharacter);
end

local function EditSelected()
	Common.EditTemplate(spawnWindow.kind, SelectedEntry());
end

local function UseEntry(entry)
	if ( spawnWindow.editMode ) then
		Common.EditTemplate(spawnWindow.kind, entry);
	else
		Common.Spawn(spawnWindow.kind, entry, spawnWindow.atCharacter);
	end
end

local HINT_SPAWN = "Type part of a name or an entry to filter the list, Enter spawns the selected row or the typed entry. Double click a row to spawn it.";
local HINT_EDIT = "Type part of a name or an entry to filter the list, Enter edits the selected row or the typed entry. Double click a row to edit it, no spawn is needed.";

local function SetEditMode(editMode)
	spawnWindow.editMode = editMode and true or false;
	NoggitCheckBox_SetChecked(spawnWindow.spawnMode, not spawnWindow.editMode);
	NoggitCheckBox_SetChecked(spawnWindow.editModeBox, spawnWindow.editMode);
	spawnWindow.hint:SetText(spawnWindow.editMode and HINT_EDIT or HINT_SPAWN);

	local noun = spawnWindow.kind == "gameobject" and "gameobject" or "creature";
	spawnWindow:SetTitle(spawnWindow.editMode and format("Find %s template", noun) or format("Spawn %s", noun));
	SpawnRefresh(true);
end

local function CreateSpawnWindow()
	spawnWindow = GameEditor_CreateWindow("GameEditorSpawnWindow", "Spawn", 400, 480, true);
	local content = spawnWindow.content;

	local searchButton = Common.Button(content, "Reload", 70, function()
		GameEditor_ReloadTemplateList(spawnWindow.kind);
		SpawnRefresh(true);
	end, "Ask the server for the template list again");
	searchButton:SetPoint("TOPRIGHT", content, "TOPRIGHT", 0, 0);

	spawnWindow.search = NoggitLineEdit_Create(content, 200);
	spawnWindow.search:SetHeight(22);
	spawnWindow.search:SetPoint("TOPLEFT", content, "TOPLEFT", 0, 0);
	spawnWindow.search:SetPoint("RIGHT", searchButton, "LEFT", -4, 0);
	spawnWindow.search:SetScript("OnEnterPressed", function(self)
		self:ClearFocus();
		local text = strtrim(self:GetText() or "");

		if ( tonumber(text) ) then
			UseEntry(tonumber(text));
		else
			SpawnRefresh();

			if ( SelectedEntry() ) then
				UseEntry(SelectedEntry());
			end
		end
	end);
	spawnWindow.search:SetScript("OnTextChanged", function()
		GameEditor_Debounce(SpawnFilter, #GameEditor_TemplateList(spawnWindow.kind));
	end);

	local hint = Common.Paragraph(content, HINT_SPAWN);
	hint:SetPoint("TOPLEFT", spawnWindow.search, "BOTTOMLEFT", 0, -6);
	hint:SetPoint("TOPRIGHT", searchButton, "BOTTOMRIGHT", 0, -6);
	spawnWindow.hint = hint;

	local bottom = CreateFrame("Frame", "GameEditorSpawnWindowBottom", content);
	bottom:SetHeight(76);
	bottom:SetPoint("BOTTOMLEFT", content, "BOTTOMLEFT", 0, 0);
	bottom:SetPoint("BOTTOMRIGHT", content, "BOTTOMRIGHT", 0, 0);

	local group = {};
	spawnWindow.camera = NoggitCheckBox_Create(bottom, "In front of the camera", true, function() spawnWindow.atCharacter = false; end, group);
	spawnWindow.camera:SetPoint("TOPLEFT", bottom, "TOPLEFT", 0, 0);
	spawnWindow.character = NoggitCheckBox_Create(bottom, "At my character", false, function() spawnWindow.atCharacter = true; end, group);
	spawnWindow.character:SetPoint("LEFT", spawnWindow.camera, "RIGHT", 16, 0);

	local modeGroup = {};
	spawnWindow.spawnMode = NoggitCheckBox_Create(bottom, "Double click spawns", true, function() SetEditMode(false); end, modeGroup);
	spawnWindow.spawnMode:SetPoint("TOPLEFT", spawnWindow.camera, "BOTTOMLEFT", 0, -4);
	spawnWindow.editModeBox = NoggitCheckBox_Create(bottom, "Double click edits the template", false, function() SetEditMode(true); end, modeGroup);
	spawnWindow.editModeBox:SetPoint("LEFT", spawnWindow.spawnMode, "RIGHT", 16, 0);

	spawnWindow.status = Common.Label(bottom, "", nil, NOGGIT_COLOR_DISABLED);
	spawnWindow.status:SetPoint("BOTTOMLEFT", bottom, "BOTTOMLEFT", 2, 6);

	local spawnButton = Common.Button(bottom, "Spawn", 70, SpawnSelected, "Spawn the selected entry, it is saved to the database and becomes the selection");
	spawnButton:SetPoint("BOTTOMRIGHT", bottom, "BOTTOMRIGHT", 0, 0);

	local editButton = Common.Button(bottom, "Edit template", 100, EditSelected, "Edit the template rows of the selected entry in the inspector, no spawn is needed");
	editButton:SetPoint("RIGHT", spawnButton, "LEFT", -4, 0);

	spawnWindow.list = NoggitListBox_Create(content, 20, function(row)
		row.entry = NoggitUI_FontString(row, "");
		row.entry:SetPoint("LEFT", row, "LEFT", 6, 0);
		row.entry:SetWidth(64);
		NoggitText_SetColor(row.entry, NOGGIT_COLOR_DISABLED);
		row.name = NoggitUI_FontString(row, "");
		row.name:SetPoint("LEFT", row, "LEFT", 74, 0);
		row.name:SetPoint("RIGHT", row, "RIGHT", -60, 0);
		row.tag = NoggitUI_FontString(row, "", nil, "RIGHT");
		row.tag:SetPoint("RIGHT", row, "RIGHT", -4, 0);
		NoggitText_SetColor(row.tag, NOGGIT_COLOR_DISABLED);
	end, function(row, item)
		row.entry:SetText(tostring(item.value));
		row.name:SetText(item.text ~= "" and item.text or "|cff7f7f7fname unknown|r");
		row.tag:SetText(item.tag or "");
	end);
	spawnWindow.list:SetPoint("TOPLEFT", hint, "BOTTOMLEFT", 0, -6);
	spawnWindow.list:SetPoint("BOTTOMRIGHT", bottom, "TOPRIGHT", 0, 6);
	spawnWindow.list.onDoubleClick = function(item)
		UseEntry(item.value);
	end;
end

local function ShowTemplateWindow(kind, editMode)
	if ( not spawnWindow ) then
		CreateSpawnWindow();
	end

	if ( spawnWindow.kind ~= kind ) then
		spawnWindow.kind = kind;
		spawnWindow.search:SetText("");
	end

	GameEditor_ShowWindow(spawnWindow);
	SetEditMode(editMode);
	spawnWindow.search:SetFocus();
end

function Common.ShowSpawnWindow(kind)
	ShowTemplateWindow(kind, false);
end

-- The same window with double click and Enter editing the template instead of spawning.
function Common.ShowTemplateBrowser(kind)
	ShowTemplateWindow(kind, true);
end

-- Layout pieces ----------------------------------------------------------------------------------------------

-- Header with the object name, an info line, a live position line and buttons on the right.
function Common.CreateHeader(parent)
	local header = CreateFrame("Frame", NoggitUI_NewName("GameEditorObjectHeader"), parent, "NoggitBoxTemplate");
	NoggitBox_Set(header, NOGGIT_COLOR_DARK, nil);
	header:SetHeight(80);
	header:SetPoint("TOPLEFT", parent, "TOPLEFT", 0, 0);
	header:SetPoint("TOPRIGHT", parent, "TOPRIGHT", 0, 0);
	header.buttons = {};

	header.name = Common.Label(header, "", "NoggitFont16", NOGGIT_COLOR_WHITE);
	header.name:SetPoint("TOPLEFT", header, "TOPLEFT", 8, -6);
	header.name:SetHeight(20);

	header.info = Common.Label(header, "", nil, NOGGIT_COLOR_TEXT);
	header.info:SetPoint("TOPLEFT", header, "TOPLEFT", 8, -28);
	header.info:SetPoint("TOPRIGHT", header, "TOPRIGHT", -8, -28);

	header.position = Common.Label(header, "", nil, NOGGIT_COLOR_DISABLED);
	header.position:SetPoint("TOPLEFT", header, "TOPLEFT", 8, -44);
	header.position:SetPoint("TOPRIGHT", header, "TOPRIGHT", -8, -44);

	header.unsaved = Common.Label(header, "", nil, NOGGIT_COLOR_GOLD);
	header.unsaved:SetPoint("TOPLEFT", header, "TOPLEFT", 8, -60);
	header.unsaved:SetPoint("TOPRIGHT", header, "TOPRIGHT", -8, -60);

	header.AddButton = function(self, text, width, onClick, tooltip)
		local button = Common.Button(self, text, width, onClick, tooltip);
		button:SetHeight(20);
		local previous = self.buttons[#self.buttons];

		if ( previous ) then
			button:SetPoint("RIGHT", previous, "LEFT", -4, 0);
		else
			button:SetPoint("TOPRIGHT", self, "TOPRIGHT", -6, -6);
		end

		table.insert(self.buttons, button);
		self.name:SetPoint("TOPRIGHT", button, "TOPLEFT", -6, 0);
		return button;
	end;

	return header;
end

-- Moves only exist on this client until Save move sends them.
function Common.HasUnsavedMove()
	return GameEditor_HasUnsavedMove and GameEditor_HasUnsavedMove() and true or false;
end

function Common.UnsavedMoveCount()
	return GameEditor_GetUnsavedMoveCount and GameEditor_GetUnsavedMoveCount() or 0;
end

function Common.SaveAllMoves()
	return GameEditor_SaveAllMoves and GameEditor_SaveAllMoves() or 0;
end

function Common.AddMoveButtons(header, onChange)
	header.saveMove = Common.Button(header, "Save move", 76, function() GameEditor_SaveMove(); end,
		"Send the new position to the server, until then the move only exists on your client (Ctrl+S)");
	header.saveMove:SetHeight(18);
	header.saveMove:SetPoint("TOPRIGHT", header, "TOPRIGHT", -6, -57);

	header.revertMove = Common.Button(header, "Revert move", 84, function() GameEditor_RevertMove(); end,
		"Put it back where it stood before you moved it");
	header.revertMove:SetHeight(18);
	header.revertMove:SetPoint("RIGHT", header.saveMove, "LEFT", -4, 0);

	header.UpdateMove = function(self)
		self.unsaved:ClearAllPoints();
		self.unsaved:SetPoint("TOPLEFT", self, "TOPLEFT", 8, -60);

		if ( Common.HasUnsavedMove() and not self.hideMove ) then
			self.saveMove:Show();
			self.revertMove:Show();
			self.unsaved:SetPoint("TOPRIGHT", self.revertMove, "TOPLEFT", -6, -3);
		else
			self.saveMove:Hide();
			self.revertMove:Hide();
			self.unsaved:SetPoint("TOPRIGHT", self, "TOPRIGHT", -8, -60);
		end
	end;

	local events = CreateFrame("Frame", nil, header);
	events:RegisterEvent("GAME_EDITOR_MOVES_CHANGED");
	events:RegisterEvent("GAME_EDITOR_SELECTION_CHANGED");
	events:SetScript("OnEvent", function()
		header:UpdateMove();

		if ( onChange ) then
			onChange();
		end
	end);

	header:UpdateMove();
end

-- Tab strip with a dot on tabs that hold unsaved changes.
function Common.CreateTabs(parent, names, onSelect)
	local strip = { current = 1 };

	strip.tabs = NoggitTabs_Create(parent, names, 1, function(index)
		strip.current = index;
		onSelect(index);
	end);

	for _, tab in ipairs(strip.tabs) do
		tab.dot = NoggitUI_SolidTexture(tab, "OVERLAY", NOGGIT_COLOR_GOLD);
		tab.dot:SetWidth(6);
		tab.dot:SetHeight(6);
		tab.dot:SetPoint("TOPRIGHT", tab, "TOPRIGHT", -4, -6);
		tab.dot:Hide();
	end

	strip.Select = function(self, index)
		for _, tab in ipairs(self.tabs) do
			tab.selected = tab.index == index;
			NoggitTab_Refresh(tab);
		end

		self.current = index;
		onSelect(index);
	end;

	strip.SetDirty = function(self, index, dirty)
		local tab = self.tabs[index];

		if ( tab and dirty ) then
			tab.dot:Show();
		elseif ( tab ) then
			tab.dot:Hide();
		end
	end;

	return strip;
end

-- A property grid with a Save and Revert bar. options: table, groups, columns, fields, readOnly, hidden, top,
-- labelWidth, onSave(changes, count), onDirty(page), saveTooltip, unavailableText.
function Common.CreateGridPage(parent, options)
	local page = CreateFrame("Frame", NoggitUI_NewName("GameEditorObjectPage"), parent);
	page.options = options;

	local bar = CreateFrame("Frame", NoggitUI_NewName("GameEditorObjectPageBar"), page);
	bar:SetHeight(24);
	bar:SetPoint("BOTTOMLEFT", page, "BOTTOMLEFT", 0, 0);
	bar:SetPoint("BOTTOMRIGHT", page, "BOTTOMRIGHT", 0, 0);
	page.bar = bar;

	page.save = Common.Button(bar, "Save", 80, function() page:Save(); end, options.saveTooltip or "Save the changed fields to the server (Ctrl+S saves every page)");
	page.save:SetPoint("RIGHT", bar, "RIGHT", 0, 0);
	page.revert = Common.Button(bar, "Revert", 70, function() page:Revert(); end, "Throw away the unsaved changes on this page");
	page.revert:SetPoint("RIGHT", page.save, "LEFT", -4, 0);
	page.status = Common.Label(bar, "", nil, NOGGIT_COLOR_DISABLED);
	page.status:SetPoint("LEFT", bar, "LEFT", 2, 0);
	page.status:SetPoint("RIGHT", page.revert, "LEFT", -4, 0);

	page.grid = GameEditor_CreatePropertyGrid(page, {
		table = options.table,
		groups = options.groups,
		columns = options.columns,
		fields = options.fields,
		readOnly = options.readOnly,
		hidden = options.hidden,
		labelWidth = options.labelWidth or 160,
		filter = options.filter ~= false,
		onChange = function()
			page:UpdateState();
		end,
	});
	page.grid.frame:SetPoint("TOPLEFT", page, "TOPLEFT", 0, -(options.top or 0));
	page.grid.frame:SetPoint("BOTTOMRIGHT", bar, "TOPRIGHT", 0, 4);

	page.unavailable = Common.Paragraph(page, "");
	page.unavailable:SetPoint("TOPLEFT", page, "TOPLEFT", 4, -(options.top or 0) - 8);
	page.unavailable:SetPoint("TOPRIGHT", page, "TOPRIGHT", -4, -(options.top or 0) - 8);
	page.unavailable:Hide();

	page.available = true;

	page.HasChanges = function(self)
		return self.available and self.grid:HasChanges();
	end;

	page.UpdateState = function(self)
		local _, count = self.grid:GetChanges();

		if ( count > 0 ) then
			self.status:SetText(format("%d unsaved change%s", count, count == 1 and "" or "s"));
			NoggitText_SetColor(self.status, NOGGIT_COLOR_GOLD);
		else
			self.status:SetText(self.hint or "No changes");
			NoggitText_SetColor(self.status, NOGGIT_COLOR_DISABLED);
		end

		Common.SetEnabled(self.save, count > 0 and self.available);
		Common.SetEnabled(self.revert, count > 0);

		if ( options.onDirty ) then
			options.onDirty(self);
		end
	end;

	page.Load = function(self, values, keepEdits)
		Common.LoadGrid(self.grid, values, keepEdits);
		self:UpdateState();
	end;

	page.Save = function(self)
		local changes, count = self.grid:GetChanges();

		if ( count == 0 or not self.available ) then
			return false;
		end

		if ( options.onSave ) then
			options.onSave(changes, count);
		end

		return true;
	end;

	page.Revert = function(self)
		self.grid:Revert();
		self:UpdateState();
	end;

	page.SetAvailable = function(self, available, text)
		self.available = available and true or false;

		if ( available ) then
			self.grid.frame:Show();
			self.bar:Show();
			self.unavailable:Hide();
		else
			self.grid.frame:Hide();
			self.bar:Hide();
			self.unavailable:SetText(text or "");
			self.unavailable:Show();
		end

		self:UpdateState();
	end;

	page:UpdateState();
	return page;
end

-- Live X, Y, Z and facing of the selection, changes nudge it locally until Save move.
function Common.CreateTransformBox(parent)
	local box = CreateFrame("Frame", NoggitUI_NewName("GameEditorTransformBox"), parent);
	box:SetHeight(40);
	box.spins = {};

	local specs = {
		{ key = "x", label = "X", min = -20000, max = 20000, step = 0.5, decimals = 2, threshold = 0.006 },
		{ key = "y", label = "Y", min = -20000, max = 20000, step = 0.5, decimals = 2, threshold = 0.006 },
		{ key = "z", label = "Z", min = -5000, max = 5000, step = 0.25, decimals = 2, threshold = 0.006 },
		{ key = "o", label = "Facing (degrees)", min = -360, max = 720, step = 5, decimals = 1, threshold = 0.06 },
	};

	for index, spec in ipairs(specs) do
		local key = spec.key;
		local threshold = spec.threshold;
		local spin = NoggitSpinBox_Create(box, 96, spec.min, spec.max, spec.step, spec.decimals, function(_, value)
			local live = box.live;

			if ( not live ) then
				return;
			end

			local delta = value - live[key];

			if ( math.abs(delta) < threshold ) then
				return;
			end

			if ( key == "x" ) then
				GameEditor_NudgeSelection(delta, 0, 0);
			elseif ( key == "y" ) then
				GameEditor_NudgeSelection(0, delta, 0);
			elseif ( key == "z" ) then
				GameEditor_NudgeSelection(0, 0, delta);
			else
				GameEditor_NudgeSelection(0, 0, 0, delta);
			end
		end);
		spin:SetPoint("TOPLEFT", box, "TOPLEFT", (index - 1) * 102, -16);
		box.spins[key] = spin;

		local label = Common.Label(box, spec.label, nil, NOGGIT_COLOR_DISABLED);
		label:SetPoint("BOTTOMLEFT", spin, "TOPLEFT", 2, 2);
	end

	box.Sync = function(self)
		local _, _, _, _, x, y, z, o = GameEditor_GetSelection();

		if ( not x ) then
			self.live = nil;
			return;
		end

		local live = self.live or {};
		live.x = x;
		live.y = y;
		live.z = z;
		live.o = math.deg(o or 0) % 360;
		self.live = live;

		for key, spin in pairs(self.spins) do
			if ( not _G[spin:GetName().."Edit"]:HasFocus() ) then
				NoggitSpinBox_SetValue(spin, live[key], true);
			end
		end
	end;

	return box;
end

-- Empty state shown when the workspace has nothing to edit.
function Common.CreateEmptyState(parent)
	local frame = CreateFrame("Frame", NoggitUI_NewName("GameEditorObjectEmpty"), parent);
	frame:SetAllPoints(parent);

	frame.title = Common.Label(frame, "", "NoggitFont16", NOGGIT_COLOR_WHITE);
	frame.title:SetPoint("TOPLEFT", frame, "TOPLEFT", 12, -14);
	frame.title:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -12, -14);

	frame.text = Common.Paragraph(frame, "", NOGGIT_COLOR_TEXT);
	frame.text:SetPoint("TOPLEFT", frame, "TOPLEFT", 12, -44);
	frame.text:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -12, -44);

	frame.primary = Common.Button(frame, "", 150, function()
		if ( frame.onPrimary ) then
			frame.onPrimary();
		end
	end);
	frame.primary:SetPoint("TOPLEFT", frame.text, "BOTTOMLEFT", 0, -12);

	frame.secondary = Common.Button(frame, "", 150, function()
		if ( frame.onSecondary ) then
			frame.onSecondary();
		end
	end);
	frame.secondary:SetPoint("LEFT", frame.primary, "RIGHT", 6, 0);

	frame.third = Common.Button(frame, "", 150, function()
		if ( frame.onThird ) then
			frame.onThird();
		end
	end);
	frame.third:SetPoint("LEFT", frame.secondary, "RIGHT", 6, 0);

	frame.help = Common.Paragraph(frame, "");
	frame.help:SetPoint("TOPLEFT", frame.primary, "BOTTOMLEFT", 0, -16);
	frame.help:SetPoint("TOPRIGHT", frame.text, "BOTTOMRIGHT", 0, -50);

	frame.Set = function(self, state)
		self.title:SetText(state.title or "");
		self.text:SetText(state.text or "");
		self.help:SetText(state.help or "");
		self.onPrimary = state.onPrimary;
		self.onSecondary = state.onSecondary;
		self.onThird = state.onThird;

		if ( state.primary ) then
			self.primary:SetText(state.primary);
			self.primary:SetWidth(max(110, self.primary:GetFontString():GetStringWidth() + 24));
			self.primary:Show();
		else
			self.primary:Hide();
		end

		if ( state.secondary ) then
			self.secondary:SetText(state.secondary);
			self.secondary:SetWidth(max(110, self.secondary:GetFontString():GetStringWidth() + 24));
			self.secondary:Show();
		else
			self.secondary:Hide();
		end

		if ( state.third ) then
			self.third:SetText(state.third);
			self.third:SetWidth(max(110, self.third:GetFontString():GetStringWidth() + 24));
			self.third:Show();
		else
			self.third:Hide();
		end

		self:Show();
	end;

	return frame;
end

-- Selection follows the workspace ----------------------------------------------------------------------------

local WORKSPACE_TOOLS = {
	creature = { select = true, move = true, rotate = true, waypoint = true },
	gameobject = { select = true, move = true, rotate = true },
};

local watcher = CreateFrame("Frame", "GameEditorObjectWatcher");
watcher:RegisterEvent("GAME_EDITOR_SELECTION_CHANGED");
watcher:RegisterEvent("CHAT_MSG_SYSTEM");
watcher:RegisterEvent("GAME_EDITOR_MODE_CHANGED");
watcher:SetScript("OnEvent", function(_, event, kind, ...)
	if ( event == "GAME_EDITOR_MODE_CHANGED" ) then
		local count = tonumber(kind) == 0 and Common.UnsavedMoveCount() or 0;

		if ( count > 0 and DEFAULT_CHAT_FRAME ) then
			DEFAULT_CHAT_FRAME:AddMessage(format("|cffffd100Game editor:|r %d move%s not saved yet, open the editor and press Save move or Ctrl+S.", count, count == 1 and " is" or "s are"));
		end

		return;
	end

	if ( event == "CHAT_MSG_SYSTEM" ) then
		OnSystemMessage(kind);
		return;
	end

	local workspace = GameEditor_GetWorkspace and GameEditor_GetWorkspace();

	if ( not WORKSPACE_TOOLS[kind or ""] or kind == workspace ) then
		return;
	end

	-- The spell workspace uses the selection as its preview target.
	if ( workspace ~= "map" and not WORKSPACE_TOOLS[workspace or ""] ) then
		return;
	end

	local tool = GameEditor_GetActiveTool();

	if ( not WORKSPACE_TOOLS[kind][tool or ""] ) then
		tool = "select";
	end

	GameEditor_SetWorkspace(kind, tool);
end);

-- Tool panels can be the same frame for several tools, this builds it once.
function Common.SharedBuilder(build)
	local frame;

	return function(parent)
		if ( not frame ) then
			frame = build(parent);
		end

		return frame;
	end;
end
