-- Displays workspace, one tool per table: creature displays with their creature_model_info row, character displays, creature model data and gameobject displays.

local DE = GameEditorDisplay;
local ME = GameEditorModel;
local Common = GameEditorObject;
local Data = GameEditorData;

local PREVIEW_HEIGHT = 220;
local FORM_WIDTH = 470;

local Num = DE.Num;
local store = DE.store;

local panels = {};
local pendingSelection;

local modelInfo = GameEditor_CreateWorldRow("creature_model_info", function()
	if ( panels.creature ) then
		panels.creature:Refresh();
	end
end);

local function Doc(kind)
	return DE.ids[kind] and store:Doc(DE.KINDS[kind].table, DE.ids[kind]);
end

local function DirtyCount()
	return #store:Dirty() + (modelInfo:IsDirty() and 1 or 0);
end

-- Opening ---------------------------------------------------------------------------------------------------

function DE.Open(kind, id)
	id = tonumber(id);
	local info = DE.KINDS[kind];

	if ( not id or not store:Doc(info.table, id) ) then
		GameEditor_Notify(format("%s %s does not exist", info.table, tostring(id)), "error");
		return false;
	end

	DE.ids[kind] = id;

	if ( kind == "creature" ) then
		modelInfo:Load(id);
	end

	if ( GameEditor_GetWorkspace() ~= "display" or GameEditor_GetActiveTool() ~= kind ) then
		GameEditor_SetWorkspace("display", kind);
	end

	if ( panels[kind] ) then
		panels[kind].idEdit:SetText(tostring(id));
		panels[kind].idEdit:ClearFocus();
	end

	store:Changed();
	return true;
end

local function DisplayText(id)
	local display = ME.Row("CreatureDisplayInfo", id);
	local modelData = display and ME.Row("CreatureModelData", display.m_modelID);
	return format("%d  %s", id, modelData and modelData.m_modelName or "no model");
end

local extraList;

local function ExtraList()
	if ( extraList ) then
		return extraList;
	end

	extraList = {};

	for _, item in ipairs(GameEditor_DbcList("CreatureDisplayInfoExtra")) do
		local row = ME.Row("CreatureDisplayInfoExtra", item.value);
		table.insert(extraList, { value = item.value, text = row and DE.ExtraText(row) or "" });
	end

	return extraList;
end

store:OnChanged(function() extraList = nil; end);

local LISTS = {
	creature = function() return GameEditor_RefList("dbc:CreatureDisplayInfo"); end,
	extra = ExtraList,
	model = function() return GameEditor_DbcList("CreatureModelData"); end,
	object = function() return GameEditor_DbcList("GameObjectDisplayInfo"); end,
};

local function Browse(kind)
	GameEditor_PickValue(DE.KINDS[kind].title.."s", LISTS[kind], DE.ids[kind], function(value)
		DE.Open(kind, value);
	end);
end

local function CreatureDisplays(data)
	local ids, seen = {}, {};
	local template = data.template or {};
	local candidates = { data.spawn and data.spawn.modelid, template.modelid1, template.modelid2, template.modelid3, template.modelid4 };

	for index = 1, 5 do
		local id = Num(candidates[index]);

		if ( id > 0 and not seen[id] ) then
			seen[id] = true;
			table.insert(ids, id);
		end
	end

	return ids;
end

-- Opens what the selected creature or gameobject shows, the data is asked for when it is not loaded.
local function FromSelection(kind)
	local selected, guid, _, name = GameEditor_GetSelection();
	local wantCreature = kind ~= "object";

	if ( selected ~= (wantCreature and "creature" or "gameobject") ) then
		GameEditor_Notify(wantCreature and "Select a creature in the world first" or "Select a gameobject in the world first", "info");
		return;
	end

	local data = ME.Quiet(wantCreature and "CreatureEditor_GetData" or "GobEditor_GetData", "selection");

	if ( type(data) ~= "table" or data.guid ~= guid ) then
		pendingSelection = { guid = guid, kind = kind };
		ME.Call(wantCreature and "CreatureEditor_Request" or "GobEditor_Request", guid);
		GameEditor_Notify("Loading the selection, it opens when its data arrives", "info");
		return;
	end

	if ( not wantCreature ) then
		local displayId = Num(data.displayId);

		if ( displayId <= 0 and data.template ) then
			displayId = Num(data.template.displayId);
		end

		DE.Open("object", displayId);
		return;
	end

	local function Use(displayId)
		local display = ME.Row("CreatureDisplayInfo", displayId);

		if ( kind == "model" ) then
			DE.Open("model", display and display.m_modelID);
		elseif ( kind == "extra" ) then
			DE.Open("extra", display and display.m_extendedDisplayInfoID);
		else
			DE.Open("creature", displayId);
		end
	end

	local ids = CreatureDisplays(data);

	if ( kind == "extra" ) then
		local dressed = {};

		for _, id in ipairs(ids) do
			local display = ME.Row("CreatureDisplayInfo", id);

			if ( display and Num(display.m_extendedDisplayInfoID) > 0 ) then
				table.insert(dressed, id);
			end
		end

		if ( #ids > 0 and #dressed == 0 ) then
			GameEditor_Notify("The creature has no character style display", "error");
			return;
		end

		ids = dressed;
	end

	if ( #ids == 0 ) then
		GameEditor_Notify("The creature has no display id in its spawn or template", "error");
	elseif ( #ids == 1 ) then
		Use(ids[1]);
	else
		local items = {};

		for _, id in ipairs(ids) do
			table.insert(items, { value = id, text = DisplayText(id) });
		end

		GameEditor_PickValue(format("Display of %s", name or "the creature"), items, nil, Use);
	end
end

table.insert(ME.selectionHandlers, function(_, arg1)
	if ( not pendingSelection or tonumber(arg1) == 1 ) then
		return;
	end

	local _, guid = GameEditor_GetSelection();
	local pending = pendingSelection;
	pendingSelection = nil;

	if ( guid == pending.guid ) then
		FromSelection(pending.kind);
	end
end);

-- Editing ---------------------------------------------------------------------------------------------------

local function NewRow(kind)
	local source = Doc(kind);
	local doc = store:Create(DE.KINDS[kind].table, source and source.values or {});
	DE.Open(kind, doc.id);
	GameEditor_Notify(format("%s %d made, it is not on the server until you save", DE.KINDS[kind].table, doc.id), "info");
end

local function OpenInModelEditor(kind)
	local doc = Doc(kind);

	if ( not doc ) then
		return;
	end

	if ( kind == "extra" ) then
		GameEditor_Notify("A character display has no model of its own, open a creature display that uses it", "info");
		return;
	end

	GameEditor_SetWorkspace("model");

	if ( kind == "creature" ) then
		ME.OpenCreatureDisplay(doc.id);
	elseif ( kind == "object" ) then
		ME.OpenGameObjectDisplay(doc.id);
	else
		ME.Open(doc.values.m_modelName, { kind = "creature", modelDataId = doc.id });
	end
end

local function DisplaysOfModel()
	local doc = Doc("model");

	if ( not doc ) then
		return;
	end

	local rows = ME.Quiet("DbcEditor_Filter", "CreatureDisplayInfo", { text = "", where = { { columns = { "m_modelID" }, value = doc.id } } }, 300);
	local items = {};

	for _, row in ipairs(type(rows) == "table" and rows or {}) do
		table.insert(items, { value = row.id, text = DisplayText(row.id) });
	end

	if ( #items == 0 ) then
		GameEditor_Notify(format("No creature display uses model data %d", doc.id), "info");
		return;
	end

	GameEditor_PickValue(format("Displays using model data %d", doc.id), items, nil, function(value)
		DE.Open("creature", value);
	end);
end

local function DisplaysOfExtra()
	local doc = Doc("extra");

	if ( not doc ) then
		return;
	end

	local rows = ME.Quiet("DbcEditor_Filter", "CreatureDisplayInfo", { text = "", where = { { columns = { "m_extendedDisplayInfoID" }, value = doc.id } } }, 300);
	local items = {};

	for _, row in ipairs(type(rows) == "table" and rows or {}) do
		table.insert(items, { value = row.id, text = DisplayText(row.id) });
	end

	if ( #items == 0 ) then
		GameEditor_Notify(format("No creature display uses character display %d, New display makes one", doc.id), "info");
		return;
	end

	GameEditor_PickValue(format("Displays using character display %d", doc.id), items, nil, function(value)
		DE.Open("creature", value);
	end);
end

-- A creature display for the character display, copied from the display ChrRaces gives the race and sex.
local function NewDisplayOfExtra()
	local doc = Doc("extra");

	if ( not doc ) then
		return;
	end

	local race = ME.Row("ChrRaces", doc.values.m_displayRaceID);
	local raceDisplayId = race and (Num(doc.values.m_displaySexID) == 1 and race.m_femaleDisplayID or race.m_maleDisplayID);
	local source = ME.Row("CreatureDisplayInfo", raceDisplayId);

	if ( not source ) then
		GameEditor_Notify("The race has no display for this sex in ChrRaces", "error");
		return;
	end

	local display = store:Create("CreatureDisplayInfo", source);
	store:SetMany(display, { m_extendedDisplayInfoID = doc.id });
	DE.Open("creature", display.id);
	GameEditor_Notify(format("CreatureDisplayInfo %d made for character display %d, it is not on the server until you save", display.id, doc.id), "info");
end

local function Save()
	local rows = #store:Dirty();

	if ( rows == 0 and not modelInfo:IsDirty() ) then
		GameEditor_Notify("Nothing to save", "info");
		return;
	end

	if ( rows > 0 ) then
		store:SaveAll();
	end

	modelInfo:Save();
end

local function Revert()
	if ( modelInfo:IsDirty() ) then
		modelInfo:Revert();
	end

	if ( #store:Dirty() > 0 ) then
		store:ConfirmRevert(function()
			for kind in pairs(DE.KINDS) do
				if ( DE.ids[kind] and not Doc(kind) ) then
					DE.ids[kind] = nil;
				end
			end
		end);
	end
end

-- Pages ---------------------------------------------------------------------------------------------------------

local function FormPage(panel, kind)
	local page = CreateFrame("Frame", NoggitUI_NewName("GameEditorDisplayPage"), panel.container);
	page:SetAllPoints(panel.container);
	page.scroll, page.child = GameEditor_CreateScrollFrame(page);
	page.scroll:SetAllPoints(page);
	page.child:SetWidth(FORM_WIDTH + 10);

	local form = ME.CreateForm(page.child, FORM_WIDTH, function(fields)
		local doc = Doc(kind);

		if ( doc ) then
			store:SetMany(doc, fields);
		end
	end);

	if ( kind == "model" ) then
		form:Note(function(data)
			local rows = data and ME.Quiet("DbcEditor_Filter", "CreatureDisplayInfo", { text = "", where = { { columns = { "m_modelID" }, value = data.m_id } } }, 300);
			local count = type(rows) == "table" and #rows or 0;
			return format("%d creature display%s use this model, Displays lists them.", count, count == 1 and "" or "s");
		end);
	end

	form:Header(DE.KINDS[kind].table);
	DE.AddColumns(form, DE.KINDS[kind].table);

	page.Refresh = function(self)
		local doc = Doc(kind);
		self.child:SetHeight(form:Refresh(doc and doc.values) + 10);
		self.child:SetWidth(max(380, (self.scroll:GetWidth() or 480) - 14));
		self.scroll.UpdateRange();
	end;

	return page;
end

local function ServerPage(panel)
	local page = CreateFrame("Frame", NoggitUI_NewName("GameEditorDisplayServerPage"), panel.container);
	page:SetAllPoints(panel.container);

	local intro = Common.Paragraph(page, "creature_model_info on the server: the size the server gives creatures with this display and the display of the other gender.");
	intro:SetPoint("TOPLEFT", page, "TOPLEFT", 2, -2);
	intro:SetPoint("TOPRIGHT", page, "TOPRIGHT", -2, -2);

	local bar = CreateFrame("Frame", NoggitUI_NewName("GameEditorDisplayServerBar"), page);
	bar:SetHeight(22);
	bar:SetPoint("TOPLEFT", page, "TOPLEFT", 0, -40);
	bar:SetPoint("TOPRIGHT", page, "TOPRIGHT", 0, -40);

	page.add = Common.Button(bar, "Add row", 70, function()
		modelInfo:Create({ BoundingRadius = 0.306, CombatReach = 1.5, Gender = 2 });
	end, "Give the display its own creature_model_info row");
	page.add:SetPoint("LEFT", bar, "LEFT", 0, 0);
	page.remove = Common.Button(bar, "Remove", 70, function() modelInfo:Remove(); end, "Delete the row when you save");
	page.remove:SetPoint("LEFT", page.add, "RIGHT", 4, 0);
	page.state = Common.Label(bar, "", nil, NOGGIT_COLOR_DISABLED);
	page.state:SetPoint("LEFT", page.remove, "RIGHT", 8, 0);
	page.state:SetPoint("RIGHT", bar, "RIGHT", 0, 0);

	local columns = {};

	for _, column in ipairs(Data.Columns.creature_model_info) do
		if ( column ~= "DisplayID" ) then
			table.insert(columns, column);
		end
	end

	page.grid = GameEditor_CreatePropertyGrid(page, {
		table = "creature_model_info",
		columns = columns,
		labelWidth = 160,
		onChange = function(_, column, value)
			modelInfo:Set(column, value);
		end,
	});
	page.grid.frame:SetPoint("TOPLEFT", bar, "BOTTOMLEFT", 0, -6);
	page.grid.frame:SetPoint("BOTTOMRIGHT", page, "BOTTOMRIGHT", 0, 0);

	page.Refresh = function(self)
		local exists = modelInfo:Exists();
		ME.SetEnabled(self.add, modelInfo.loaded and not exists);
		ME.SetEnabled(self.remove, exists);

		if ( modelInfo.error ) then
			self.state:SetText("|cffff6060"..modelInfo.error.."|r");
		elseif ( not modelInfo.loaded ) then
			self.state:SetText("Loading...");
		elseif ( not exists ) then
			self.state:SetText("No row, the server uses player sized defaults");
		else
			self.state:SetText(modelInfo:IsDirty() and "|cffffcc00Not saved|r" or "");
		end

		if ( exists ) then
			self.grid.frame:Show();
			self.grid:SetValues(modelInfo.values, modelInfo.original ~= nil and modelInfo:IsDirty());
		else
			self.grid.frame:Hide();
		end
	end;

	return page;
end

-- Panels ---------------------------------------------------------------------------------------------------------

local EMPTY_TEXT = {
	creature = "Type a CreatureDisplayInfo id, search by model name, or take the display of the selected creature.",
	extra = "Type a CreatureDisplayInfoExtra id, search by race or baked texture, or take the character display of the selected creature.",
	model = "Type a CreatureModelData id, search by model path, or take the model of the selected creature.",
	object = "Type a GameObjectDisplayInfo id, search by model path, or take the display of the selected gameobject.",
};

local function BuildPanel(parent, kind)
	local info = DE.KINDS[kind];
	local panel = CreateFrame("Frame", NoggitUI_NewName("GameEditorDisplayPanel"), parent);
	panel.kind = kind;

	local row = CreateFrame("Frame", NoggitUI_NewName("GameEditorDisplayRow"), panel);
	row:SetHeight(22);
	row:SetPoint("TOPLEFT", panel, "TOPLEFT", 6, -6);
	row:SetPoint("TOPRIGHT", panel, "TOPRIGHT", -6, -6);

	local label = Common.Label(row, "Id");
	label:SetPoint("LEFT", row, "LEFT", 0, 0);
	panel.idEdit = NoggitLineEdit_Create(row, 70);
	panel.idEdit:SetHeight(22);
	panel.idEdit:SetMaxLetters(10);
	panel.idEdit:SetPoint("LEFT", label, "RIGHT", 6, 0);
	panel.idEdit:SetScript("OnEnterPressed", function(self) DE.Open(kind, tonumber(strtrim(self:GetText() or ""))); end);
	panel.idEdit:SetScript("OnEscapePressed", function(self) self:ClearFocus(); end);
	Common.Tooltip(panel.idEdit, info.table.." id, Enter opens it");

	local browse = Common.Button(row, "Browse", 60, function() Browse(kind); end, kind == "extra" and "Search by id, race or baked texture" or "Search by id or model name");
	browse:SetPoint("LEFT", panel.idEdit, "RIGHT", 4, 0);
	local new = Common.Button(row, "New", 46, function() NewRow(kind); end, "Make a new row, a copy of the open one when there is one");
	new:SetPoint("LEFT", browse, "RIGHT", 4, 0);
	local fromSelection = Common.Button(row, "From selection", 104, function() FromSelection(kind); end, "Open what the selected creature or gameobject uses");
	fromSelection:SetPoint("RIGHT", row, "RIGHT", 0, 0);

	local previewHolder = CreateFrame("Frame", NoggitUI_NewName("GameEditorDisplayPreviewHolder"), panel);
	previewHolder:SetHeight(PREVIEW_HEIGHT);
	previewHolder:SetPoint("TOPLEFT", row, "BOTTOMLEFT", 0, -6);
	previewHolder:SetPoint("RIGHT", panel, "RIGHT", -6, 0);
	panel.preview = DE.CreatePreview(previewHolder);

	local actions = CreateFrame("Frame", NoggitUI_NewName("GameEditorDisplayActions"), panel);
	actions:SetHeight(22);
	actions:SetPoint("TOPLEFT", previewHolder, "BOTTOMLEFT", 0, -6);
	actions:SetPoint("TOPRIGHT", previewHolder, "BOTTOMRIGHT", 0, -6);

	local modelEditor = Common.Button(actions, "Model editor", 96, function() OpenInModelEditor(kind); end, "Open the model in the model editor for textures, particles and attachments");
	modelEditor:SetPoint("LEFT", actions, "LEFT", 0, 0);

	if ( kind == "extra" ) then
		modelEditor:Hide();
		local displays = Common.Button(actions, "Displays", 70, DisplaysOfExtra, "List the creature displays that use this character display");
		displays:SetPoint("LEFT", actions, "LEFT", 0, 0);
		local newDisplay = Common.Button(actions, "New display", 90, NewDisplayOfExtra, "Make a creature display of the race model that uses this character display");
		newDisplay:SetPoint("LEFT", displays, "RIGHT", 4, 0);
	end

	if ( kind == "creature" or kind == "extra" ) then
		panel.ignoreBake = NoggitCheckBox_Create(actions, "Ignore bake", DE.ignoreBake, function(_, checked)
			DE.ignoreBake = checked and true or false;
			store:Changed();
		end);
		panel.ignoreBake:SetPoint("RIGHT", actions, "RIGHT", 0, 0);
		Common.Tooltip(panel.ignoreBake, "Preview the looks and items instead of the baked texture, the game still uses the bake");
	end

	if ( kind == "creature" ) then
		local modelData = Common.Button(actions, "Model data", 84, function()
			local doc = Doc("creature");

			if ( doc ) then
				DE.Open("model", doc.values.m_modelID);
			end
		end, "Edit the CreatureModelData row of this display");
		modelData:SetPoint("LEFT", modelEditor, "RIGHT", 4, 0);
	elseif ( kind == "model" ) then
		local displays = Common.Button(actions, "Displays", 70, DisplaysOfModel, "List the creature displays that use this model");
		displays:SetPoint("LEFT", modelEditor, "RIGHT", 4, 0);
	end

	local bar = CreateFrame("Frame", NoggitUI_NewName("GameEditorDisplayBar"), panel);
	bar:SetHeight(24);
	bar:SetPoint("BOTTOMLEFT", panel, "BOTTOMLEFT", 6, 6);
	bar:SetPoint("BOTTOMRIGHT", panel, "BOTTOMRIGHT", -6, 6);

	panel.save = Common.Button(bar, "Save", 70, Save, "Send every changed display row to the server (Ctrl+S)");
	panel.save:SetPoint("RIGHT", bar, "RIGHT", 0, 0);
	panel.revert = Common.Button(bar, "Revert", 60, Revert, "Throw away the unsaved changes");
	panel.revert:SetPoint("RIGHT", panel.save, "LEFT", -4, 0);
	panel.status = Common.Label(bar, "", nil, NOGGIT_COLOR_DISABLED);
	panel.status:SetPoint("LEFT", bar, "LEFT", 2, 0);
	panel.status:SetPoint("RIGHT", panel.revert, "LEFT", -4, 0);

	local names = kind == "creature" and { "Display", "Server" } or { info.title };
	panel.pages = {};
	panel.tabs = Common.CreateTabs(panel, names, function() panel:Refresh(); end);
	panel.tabs.tabs[1]:SetPoint("TOPLEFT", actions, "BOTTOMLEFT", 0, -8);

	panel.container = CreateFrame("Frame", NoggitUI_NewName("GameEditorDisplayPages"), panel);
	panel.container:SetPoint("TOPLEFT", panel.tabs.tabs[1], "BOTTOMLEFT", 0, -6);
	panel.container:SetPoint("RIGHT", panel, "RIGHT", -6, 0);
	panel.container:SetPoint("BOTTOM", bar, "TOP", 0, 6);

	panel.pages[1] = FormPage(panel, kind);

	if ( kind == "creature" ) then
		panel.pages[2] = ServerPage(panel);
	end

	panel.empty = Common.CreateEmptyState(panel);
	panel.empty:ClearAllPoints();
	panel.empty:SetPoint("TOPLEFT", previewHolder, "BOTTOMLEFT", -6, -6);
	panel.empty:SetPoint("BOTTOMRIGHT", bar, "TOPRIGHT", 6, 6);

	panel.Refresh = function(self)
		if ( not self:IsShown() ) then
			return;
		end

		local doc = Doc(kind);
		local dirty = DirtyCount();
		ME.SetEnabled(self.save, dirty > 0);
		ME.SetEnabled(self.revert, dirty > 0);

		if ( dirty > 0 ) then
			self.status:SetText(format("%d unsaved row%s", dirty, dirty == 1 and "" or "s"));
			NoggitText_SetColor(self.status, NOGGIT_COLOR_GOLD);
		else
			self.status:SetText(doc and format("%s %d", info.table, doc.id) or "");
			NoggitText_SetColor(self.status, NOGGIT_COLOR_DISABLED);
		end

		if ( not doc ) then
			self.preview:Show(nil, nil, nil, "Nothing open");
			actions:Hide();
			self.container:Hide();

			for _, tab in ipairs(self.tabs.tabs) do
				tab:Hide();
			end

			self.empty:Set({
				title = "No "..string.lower(info.title).." open",
				text = EMPTY_TEXT[kind],
				primary = "From selection", onPrimary = function() FromSelection(kind); end,
				secondary = "Browse", onSecondary = function() Browse(kind); end,
				help = "Edits apply to this client at once and reach the server when you save.",
			});
			return;
		end

		self.empty:Hide();
		actions:Show();
		self.container:Show();

		for _, tab in ipairs(self.tabs.tabs) do
			tab:Show();
		end

		local extraId = kind == "extra" and doc.id or (kind == "creature" and Num(doc.values.m_extendedDisplayInfoID) or 0);
		local extra = extraId > 0 and (store:Find("CreatureDisplayInfoExtra", extraId) or {}).values or ME.Row("CreatureDisplayInfoExtra", extraId);

		if ( self.ignoreBake ) then
			NoggitCheckBox_SetChecked(self.ignoreBake, DE.ignoreBake);

			if ( extra ) then
				self.ignoreBake:Show();
			else
				self.ignoreBake:Hide();
			end
		end

		if ( extra ) then
			local scale = kind == "creature" and Num(doc.values.m_creatureModelScale) > 0 and Num(doc.values.m_creatureModelScale) or 1;
			self.preview:ShowExtra(extraId, DE.ExtraKey(extraId, extra, DE.ignoreBake), DE.ignoreBake, scale);
		else
			self.preview:Show(DE.PreviewOf(kind, doc.values));
		end

		for index, page in ipairs(self.pages) do
			if ( index == self.tabs.current ) then
				page:Show();
				page:Refresh();
			else
				page:Hide();
			end
		end
	end;

	panel:SetScript("OnShow", function(self) self:Refresh(); end);

	if ( DE.ids[kind] ) then
		panel.idEdit:SetText(tostring(DE.ids[kind]));
	end

	panels[kind] = panel;
	return panel;
end

store:OnChanged(function()
	for _, panel in pairs(panels) do
		panel.preview:Invalidate();
		panel:Refresh();
	end
end);

-- Refs -------------------------------------------------------------------------------------------------------------

GameEditor_RefOpeners["dbc:CreatureDisplayInfo"] = function(id) DE.Open("creature", id); end;
GameEditor_RefOpeners["dbc:CreatureDisplayInfoExtra"] = function(id) DE.Open("extra", id); end;
GameEditor_RefOpeners["dbc:CreatureModelData"] = function(id) DE.Open("model", id); end;
GameEditor_RefLists["dbc:CreatureDisplayInfoExtra"] = ExtraList;

GameEditor_RefResolvers["dbc:CreatureDisplayInfoExtra"] = function(value)
	local row = ME.Row("CreatureDisplayInfoExtra", value);
	return row and DE.ExtraText(row) or nil;
end;

GameEditor_OnTemplateList("item", function()
	if ( panels.extra ) then
		panels.extra:Refresh();
	end
end);
GameEditor_RefOpeners["dbc:GameObjectDisplayInfo"] = function(id) DE.Open("object", id); end;

GameEditor_RefResolvers["dbc:CreatureDisplayInfo"] = function(value)
	local display = ME.Row("CreatureDisplayInfo", value);
	local modelData = display and ME.Row("CreatureModelData", display.m_modelID);
	return display and (modelData and ME.FileName(modelData.m_modelName or "") or "no model") or nil;
end;

-- Workspace --------------------------------------------------------------------------------------------------------

local separator = { separator = true };

local function ActiveKind()
	local tool = GameEditor_GetActiveTool();
	return DE.KINDS[tool or ""] and tool or "creature";
end

local function BuildMenus()
	return {
		{
			title = "Display",
			{ text = "Open from selection", onClick = function() FromSelection(ActiveKind()); end },
			{ text = "Browse...", onClick = function() Browse(ActiveKind()); end },
			{ text = "New row", onClick = function() NewRow(ActiveKind()); end },
			separator,
			{ text = "Save", shortcut = "Ctrl+S", onClick = Save },
			{ text = "Revert", onClick = Revert },
			separator,
			{ text = "Open in the model editor", onClick = function() OpenInModelEditor(ActiveKind()); end },
			{ text = "New display of the character display", onClick = NewDisplayOfExtra },
		},
	};
end

local function BuildStatus()
	local kind = ActiveKind();
	local cells = { DE.ids[kind] and format("%s %d", DE.KINDS[kind].table, DE.ids[kind]) or "Nothing open" };
	local dirty = DirtyCount();

	if ( dirty > 0 ) then
		table.insert(cells, format("|cffffcc00%d unsaved|r", dirty));
	end

	return cells;
end

GameEditor_RegisterWorkspace({
	key = "display",
	title = "Displays",
	category = "Visuals",
	order = 10,
	dockWidth = 520,
	tools = {
		{ key = "creature", name = "Creature displays", icon = 0xF8DB, build = function(parent) return BuildPanel(parent, "creature"); end },
		{ key = "extra", name = "Character displays", icon = 0xF8DA, build = function(parent) return BuildPanel(parent, "extra"); end },
		{ key = "model", name = "Creature model data", icon = 0xF8B8, build = function(parent) return BuildPanel(parent, "model"); end },
		{ key = "object", name = "Gameobject displays", icon = 0xF8C0, build = function(parent) return BuildPanel(parent, "object"); end },
	},
	menus = BuildMenus,
	keys = {
		{ "Ctrl+S", "Save the changed display rows and creature_model_info" },
		{ "Drag in the preview", "Turn the model, Shift or right drag moves it" },
	},
	onHotkey = function(key)
		if ( key == "CTRL-S" ) then
			Save();
			return true;
		end

		return false;
	end,
	status = BuildStatus,
});
