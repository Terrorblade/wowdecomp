-- Models workspace, copies a model, edits its textures, particles, ribbons and attachment points and adds it to the display DBCs.

local ME = GameEditorModel;
local Common = GameEditorObject;

local PREVIEW_HEIGHT = 220;
local TAB_NAMES = { "Textures", "Particles", "Ribbons", "Attach", "Save" };
local TAB_TEXTURES, TAB_PARTICLES, TAB_RIBBONS, TAB_ATTACHMENTS, TAB_SAVE = 1, 2, 3, 4, 5;

local Num = ME.Num;

local frame;
local pathEdit;
local idEdit;
local docCombo;
local closeButton;
local docState;
local previewHolder;
local tabs;
local tabAnchor;
local container;
local pages = {};
local empty;
local statusText;
local saveButton;
local pendingSelection;

local function IsActive()
	return GameEditor_GetWorkspace() == "model";
end

-- Opening ------------------------------------------------------------------------------------------------------------

local function OpenTyped()
	local text = ME.CleanPath(pathEdit:GetText());

	if ( ME.Open(text) ) then
		pathEdit:ClearFocus();
	end
end

local function TypedId()
	local id = tonumber(strtrim(idEdit:GetText() or ""));

	if ( not id or id <= 0 ) then
		ME.Notice("Type a display id first", "info");
	end

	return id;
end

local modelExists = {};

-- DBC rows can name models the client does not have, those are left out of the browse list.
local function ModelExists(path)
	local key = string.lower(ME.CleanPath(path or ""));

	if ( key == "" ) then
		return false;
	end

	if ( modelExists[key] == nil ) then
		modelExists[key] = ME.FileExists(ME.ToM2(path)) and true or false;
	end

	return modelExists[key];
end

local existsWatcher = CreateFrame("Frame");
existsWatcher:RegisterEvent("MODEL_EDITOR_SAVED");
local modelList;
existsWatcher:RegisterEvent("DBC_RECORD_UPDATED");
existsWatcher:SetScript("OnEvent", function()
	modelExists = {};
	modelList = nil;
end);

-- Every model path the display DBCs name and the client has, one row per path.
local function ModelList()
	if ( modelList ) then
		return modelList;
	end

	modelList = {};
	local seen = {};

	local function Add(prefix, id, path, source)
		local key = string.lower(ME.CleanPath(path or ""));

		if ( key ~= "" and not seen[key] and ModelExists(path) ) then
			seen[key] = true;
			table.insert(modelList, { value = prefix..id, text = path, path = path, source = source });
		end
	end

	for _, item in ipairs(GameEditor_DbcList("CreatureModelData")) do
		Add("M", item.value, item.text, { kind = "creature", modelDataId = item.value });
	end

	for _, item in ipairs(GameEditor_DbcList("GameObjectDisplayInfo")) do
		Add("G", item.value, item.text, { kind = "gameobject", displayId = item.value });
	end

	return modelList;
end

local function BrowseModels()
	GameEditor_PickValue("Models in the display DBCs", ModelList, nil, function(_, item)
		if ( item and item.path ) then
			ME.Open(item.path, item.source);
		end
	end);
end

local function ShowRecent()
	local items = {};

	for index, path in ipairs(ME.recent) do
		table.insert(items, { value = index, text = path, path = path });
	end

	if ( #items == 0 ) then
		ME.Notice("No recent models yet", "info");
		return;
	end

	GameEditor_PickValue("Recent models", items, nil, function(_, item)
		if ( item ) then
			ME.Open(item.path);
		end
	end);
end

local function CreatureDisplays(data)
	local ids = {};
	local seen = {};

	local function Add(id)
		id = Num(id);

		if ( id > 0 and not seen[id] ) then
			seen[id] = true;
			table.insert(ids, id);
		end
	end

	if ( data.spawn ) then
		Add(data.spawn.modelid);
	end

	if ( #ids == 0 and data.template ) then
		for index = 1, 4 do
			Add(data.template["modelid"..index]);
		end
	end

	return ids;
end

-- Opens the display of the selected creature or gameobject, the data is asked for when it is not loaded.
local function OpenFromSelection()
	local kind, guid, entry, name = GameEditor_GetSelection();

	if ( kind ~= "creature" and kind ~= "gameobject" ) then
		ME.Notice("Select a creature or a gameobject in the world first", "info");
		return;
	end

	local source = { entry = tonumber(entry), entryKind = kind };

	if ( kind == "creature" ) then
		local data = ME.Quiet("CreatureEditor_GetData", "selection");

		if ( type(data) ~= "table" or data.guid ~= guid ) then
			pendingSelection = guid;
			ME.Call("CreatureEditor_Request", guid);
			GameEditor_Notify("Loading the creature, the model opens when its data arrives", "info");
			return;
		end

		local ids = CreatureDisplays(data);

		if ( #ids == 0 ) then
			ME.Notice("The creature has no display id in its spawn or template");
		elseif ( #ids == 1 ) then
			ME.OpenCreatureDisplay(ids[1], source);
		else
			local items = {};

			for _, id in ipairs(ids) do
				local display = ME.Row("CreatureDisplayInfo", id);
				local model = display and ME.Row("CreatureModelData", display.m_modelID);
				table.insert(items, { value = id, text = model and model.m_modelName or "unknown model" });
			end

			GameEditor_PickValue(format("Display of %s", name or "the creature"), items, nil, function(value)
				ME.OpenCreatureDisplay(value, source);
			end);
		end

		return;
	end

	local data = ME.Quiet("GobEditor_GetData", "selection");

	if ( type(data) ~= "table" or data.guid ~= guid ) then
		pendingSelection = guid;
		ME.Call("GobEditor_Request", guid);
		GameEditor_Notify("Loading the gameobject, the model opens when its data arrives", "info");
		return;
	end

	local displayId = Num(data.displayId);

	if ( displayId <= 0 and data.template ) then
		displayId = Num(data.template.displayId);
	end

	if ( displayId <= 0 ) then
		ME.Notice("The gameobject has no display id");
		return;
	end

	ME.OpenGameObjectDisplay(displayId, source);
end

table.insert(ME.selectionHandlers, function(event, arg1)
	if ( not pendingSelection ) then
		return;
	end

	if ( tonumber(arg1) == 1 ) then
		return;
	end

	local _, guid = GameEditor_GetSelection();

	if ( guid ~= pendingSelection ) then
		pendingSelection = nil;
		return;
	end

	pendingSelection = nil;
	OpenFromSelection();
end);

-- Refresh -------------------------------------------------------------------------------------------------------------

local function DocItems()
	local items = {};

	for _, doc in ipairs(ME.order) do
		local entry = ME.docs[doc];
		local info = ME.Info(doc);

		if ( entry ) then
			table.insert(items, { value = doc, text = format("%s%s", (info and info.dirty) and "* " or "", entry.path or ME.DocName(doc)) });
		end
	end

	return items;
end

local function RefreshDocBar()
	local items = DocItems();
	docCombo.items = #items > 0 and items or { { value = nil, text = "No model open" } };
	local selected = 1;

	for index, item in ipairs(items) do
		if ( item.value == ME.current ) then
			selected = index;
		end
	end

	NoggitComboBox_SetSelected(docCombo, selected, true);
	ME.SetEnabled(closeButton, ME.current ~= nil);

	local info = ME.current and ME.Info(ME.current);

	if ( info ) then
		docState:SetText(info.dirty and "|cffffcc00unsaved|r" or "saved");
	else
		docState:SetText("");
	end
end

local function RefreshStatus()
	local info = ME.current and ME.Info(ME.current);

	if ( not info ) then
		statusText:SetText("Open a model to edit it");
		NoggitText_SetColor(statusText, NOGGIT_COLOR_DISABLED);
	elseif ( info.dirty ) then
		statusText:SetText(format("Unsaved changes, revision %s", tostring(info.revision or "?")));
		NoggitText_SetColor(statusText, NOGGIT_COLOR_GOLD);
	else
		statusText:SetText(info.modern and format("Modern model, version %s", tostring(info.version or "?")) or "No unsaved changes");
		NoggitText_SetColor(statusText, NOGGIT_COLOR_DISABLED);
	end

	ME.SetEnabled(saveButton, info ~= nil);
end

local function RefreshPages()
	if ( not frame ) then
		return;
	end

	RefreshDocBar();
	RefreshStatus();

	if ( not ME.current ) then
		container:Hide();
		empty:Set({
			title = "No model open",
			text = "Type a game path above, open a display by its id, or take the model of the selected creature or gameobject.",
			primary = "From selection", onPrimary = OpenFromSelection,
			secondary = "Recent models", onSecondary = ShowRecent,
			third = "Browse models", onThird = BrowseModels,
			help = "Edits stay in the client until you save. Save as copy writes your own model loose into the game folder, then Add to DBC on the Save tab gives it a display id.",
		});
		return;
	end

	empty:Hide();
	container:Show();

	local page = pages[tabs.current];

	if ( page and page.Refresh ) then
		page:Refresh();
	end
end

ME.OnChanged(function()
	if ( frame and frame:IsShown() ) then
		RefreshPages();
	end
end);

local function ShowPage(index)
	for key, page in pairs(pages) do
		if ( key == index ) then
			page:Show();
		else
			page:Hide();
		end
	end

	RefreshPages();
end

function ME.OnPreviewLayout(floating)
	if ( not previewHolder ) then
		return;
	end

	tabAnchor:ClearAllPoints();

	if ( floating ) then
		previewHolder:Hide();
		tabAnchor:SetPoint("TOPLEFT", docCombo, "BOTTOMLEFT", 0, -6);
	else
		previewHolder:Show();
		tabAnchor:SetPoint("TOPLEFT", previewHolder, "BOTTOMLEFT", 0, -6);
	end
end

-- Actions -----------------------------------------------------------------------------------------------------------------

local function CloseCurrent()
	local doc = ME.current;

	if ( not doc ) then
		return;
	end

	local function Close()
		ME.HideTrackWindow();
		ME.Close(doc);
	end

	if ( ME.IsDirty(doc) ) then
		NoggitMessageBox_Show("Close model", format("%s has unsaved changes. Close it and lose them?", ME.DocName(doc)), {
			{ text = "Close", onClick = Close },
			{ text = "Cancel" },
		});
	else
		Close();
	end
end

local function Save()
	if ( not ME.current ) then
		ME.Notice("Open a model first", "info");
		return;
	end

	if ( not ME.SaveInPlace() ) then
		tabs:Select(TAB_SAVE);
	end
end

-- Adds the skins, portraits and NPC bakes of every CreatureDisplayInfo row using the model to textures, returns how many rows there were.
local function DisplayTextures(entry, textures)
	local modelIds = ME.FindByModelName("CreatureModelData", entry.path);
	local displayIds = {};
	local count = 0;

	if ( entry.source and entry.source.modelDataId ) then
		table.insert(modelIds, entry.source.modelDataId);
	end

	if ( entry.source and entry.source.kind == "creature" and entry.source.displayId ) then
		displayIds[Num(entry.source.displayId)] = true;
	end

	for _, modelId in ipairs(modelIds) do
		local rows = ME.Quiet("DbcEditor_Filter", "CreatureDisplayInfo", { text = "", where = { { columns = { "m_modelID" }, value = modelId } } }, 5000);

		for _, row in ipairs(type(rows) == "table" and rows or {}) do
			displayIds[Num(row.id)] = true;
		end
	end

	for id in pairs(displayIds) do
		local display = ME.Row("CreatureDisplayInfo", id);
		local modelData = display and ME.Row("CreatureModelData", display.m_modelID);
		local folder = ME.Folder(ME.CleanPath(modelData and modelData.m_modelName or entry.path));

		if ( display ) then
			count = count + 1;

			for index = 0, 2 do
				local name = display[format("m_textureVariation[%d]", index)];

				if ( name and name ~= "" ) then
					table.insert(textures, (folder ~= "" and (folder.."\\") or "")..name..".blp");
				end
			end

			if ( (display.m_portraitTextureName or "") ~= "" ) then
				table.insert(textures, "Interface\\Icons\\"..display.m_portraitTextureName);
			end

			local extra = ME.Row("CreatureDisplayInfoExtra", display.m_extendedDisplayInfoID);

			if ( extra and (extra.m_bakeName or "") ~= "" ) then
				table.insert(textures, "Textures\\BakedNpcTextures\\"..extra.m_bakeName);
			end
		end
	end

	return count;
end

-- Writes the model as edited, its skins, anims and every texture (plus a png of each) under <game folder>\Export\.
local function Export()
	local entry = ME.Current();

	if ( not entry ) then
		ME.Notice("Open a model first", "info");
		return;
	end

	local textures = {};

	for _, path in pairs(ME.PreviewTextures(entry)) do
		table.insert(textures, path);
	end

	local displays = DisplayTextures(entry, textures);
	local ok, message = ME.Do("ModelEditor_Export", entry.doc, { textures = textures, png = true });

	if ( ok ) then
		GameEditor_Notify(tostring(message or "Exported")..(displays > 0 and format(" Textures of %d creature displays included.", displays) or ""), "ok");
	end
end

local function ReloadPreview()
	if ( ME.current ) then
		ME.Invalidate(ME.current);
	end

	ME.PreviewReload(true);
	ME.Changed();
end

-- Build ---------------------------------------------------------------------------------------------------------------------

local function Build(parent)
	frame = CreateFrame("Frame", "GameEditorModelPanel", parent);

	local openRow = CreateFrame("Frame", NoggitUI_NewName("GameEditorModelOpen"), frame);
	openRow:SetHeight(22);
	openRow:SetPoint("TOPLEFT", frame, "TOPLEFT", 6, -6);
	openRow:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -6, -6);

	local browse = Common.Button(openRow, "Browse", 60, BrowseModels, "Search the model names of CreatureModelData and GameObjectDisplayInfo");
	browse:SetPoint("RIGHT", openRow, "RIGHT", 0, 0);
	local open = Common.Button(openRow, "Open", 50, OpenTyped, "Open the typed game path, .mdx and .mdl open the .m2");
	open:SetPoint("RIGHT", browse, "LEFT", -4, 0);

	pathEdit = NoggitLineEdit_Create(openRow, 300);
	pathEdit:SetHeight(22);
	pathEdit:SetMaxLetters(260);
	pathEdit:SetPoint("LEFT", openRow, "LEFT", 0, 0);
	pathEdit:SetPoint("RIGHT", open, "LEFT", -4, 0);
	pathEdit:SetScript("OnEnterPressed", OpenTyped);
	pathEdit:SetScript("OnEscapePressed", function(self) self:ClearFocus(); end);
	Common.Tooltip(pathEdit, "Game path of a model, for example Creature\\Murloc\\Murloc.m2");

	local idRow = CreateFrame("Frame", NoggitUI_NewName("GameEditorModelOpenId"), frame);
	idRow:SetHeight(22);
	idRow:SetPoint("TOPLEFT", openRow, "BOTTOMLEFT", 0, -4);
	idRow:SetPoint("TOPRIGHT", openRow, "BOTTOMRIGHT", 0, -4);

	local idLabel = Common.Label(idRow, "Display id");
	idLabel:SetPoint("LEFT", idRow, "LEFT", 0, 0);
	idEdit = NoggitLineEdit_Create(idRow, 70);
	idEdit:SetHeight(22);
	idEdit:SetMaxLetters(10);
	idEdit:SetPoint("LEFT", idLabel, "RIGHT", 6, 0);
	idEdit:SetScript("OnEnterPressed", function(self)
		local id = TypedId();

		if ( id and ME.OpenCreatureDisplay(id) ) then
			self:ClearFocus();
		end
	end);
	Common.Tooltip(idEdit, "CreatureDisplayInfo or GameObjectDisplayInfo id, Enter opens it as a creature display");

	local creature = Common.Button(idRow, "Creature", 66, function()
		local id = TypedId();

		if ( id ) then
			ME.OpenCreatureDisplay(id);
		end
	end, "Open the model of this CreatureDisplayInfo id through CreatureModelData");
	creature:SetPoint("LEFT", idEdit, "RIGHT", 4, 0);

	local object = Common.Button(idRow, "Object", 58, function()
		local id = TypedId();

		if ( id ) then
			ME.OpenGameObjectDisplay(id);
		end
	end, "Open the model of this GameObjectDisplayInfo id");
	object:SetPoint("LEFT", creature, "RIGHT", 4, 0);

	local recent = Common.Button(idRow, "Recent", 60, ShowRecent, "Models you opened or saved lately");
	recent:SetPoint("RIGHT", idRow, "RIGHT", 0, 0);
	local fromSelection = Common.Button(idRow, "From selection", 104, OpenFromSelection, "Open the display of the selected creature or gameobject");
	fromSelection:SetPoint("RIGHT", recent, "LEFT", -4, 0);

	closeButton = Common.Button(frame, "Close", 56, CloseCurrent, "Close the model, unsaved edits are lost");
	closeButton:SetPoint("TOPRIGHT", idRow, "BOTTOMRIGHT", 0, -6);
	docState = Common.Label(frame, "", nil, NOGGIT_COLOR_DISABLED);
	docState:SetPoint("RIGHT", closeButton, "LEFT", -6, 0);
	docState:SetWidth(56);
	docState:SetJustifyH("RIGHT");

	docCombo = NoggitComboBox_Create(frame, 300, { { value = nil, text = "No model open" } }, 1, function(_, _, item)
		if ( item and item.value and item.value ~= ME.current ) then
			ME.HideTrackWindow();
			ME.SetCurrent(item.value);
		end
	end);
	docCombo:SetPoint("TOPLEFT", idRow, "BOTTOMLEFT", 0, -6);
	docCombo:SetPoint("RIGHT", docState, "LEFT", -6, 0);

	previewHolder = CreateFrame("Frame", NoggitUI_NewName("GameEditorModelPreviewHolder"), frame);
	previewHolder:SetHeight(PREVIEW_HEIGHT);
	previewHolder:SetPoint("TOPLEFT", docCombo, "BOTTOMLEFT", 0, -6);
	previewHolder:SetPoint("RIGHT", frame, "RIGHT", -6, 0);
	ME.CreatePreview(previewHolder);

	tabAnchor = CreateFrame("Frame", NoggitUI_NewName("GameEditorModelTabAnchor"), frame);
	tabAnchor:SetWidth(1);
	tabAnchor:SetHeight(1);
	tabAnchor:SetPoint("TOPLEFT", previewHolder, "BOTTOMLEFT", 0, -6);

	local bar = CreateFrame("Frame", NoggitUI_NewName("GameEditorModelBar"), frame);
	bar:SetHeight(24);
	bar:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 6, 6);
	bar:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -6, 6);

	saveButton = Common.Button(bar, "Save", 70, Save, "Save the model over its own file, loose custom models only (Ctrl+S)");
	saveButton:SetPoint("RIGHT", bar, "RIGHT", 0, 0);
	local saveAs = Common.Button(bar, "Save as...", 80, function() tabs:Select(TAB_SAVE); end, "Save a copy and add it to the DBCs on the Save tab");
	saveAs:SetPoint("RIGHT", saveButton, "LEFT", -4, 0);
	local export = Common.Button(bar, "Export", 60, Export, "Write the model, its skins, anims and textures with a png of each under <game folder>\\Export\\, edited pngs show up in Import image");
	export:SetPoint("RIGHT", saveAs, "LEFT", -4, 0);
	statusText = Common.Label(bar, "", nil, NOGGIT_COLOR_DISABLED);
	statusText:SetPoint("LEFT", bar, "LEFT", 2, 0);
	statusText:SetPoint("RIGHT", export, "LEFT", -4, 0);

	tabs = Common.CreateTabs(frame, TAB_NAMES, ShowPage);
	tabs.tabs[1]:SetPoint("TOPLEFT", tabAnchor, "TOPLEFT", 0, 0);

	container = CreateFrame("Frame", NoggitUI_NewName("GameEditorModelPages"), frame);
	container:SetPoint("TOPLEFT", tabs.tabs[1], "BOTTOMLEFT", 0, -6);
	container:SetPoint("RIGHT", frame, "RIGHT", -6, 0);
	container:SetPoint("BOTTOM", bar, "TOP", 0, 6);

	empty = Common.CreateEmptyState(frame);
	empty:ClearAllPoints();
	empty:SetPoint("TOPLEFT", tabs.tabs[1], "BOTTOMLEFT", -6, -6);
	empty:SetPoint("BOTTOMRIGHT", bar, "TOPRIGHT", 6, 6);

	pages[TAB_TEXTURES] = ME.BuildTexturePage(container);
	pages[TAB_PARTICLES] = ME.BuildParticlePage(container);
	pages[TAB_RIBBONS] = ME.BuildRibbonPage(container);
	pages[TAB_ATTACHMENTS] = ME.BuildAttachmentPage(container);
	pages[TAB_SAVE] = ME.BuildSavePage(container);

	frame:SetScript("OnShow", function()
		ME.SyncDocs();
		ShowPage(tabs.current);
	end);

	ME.SyncDocs();
	ShowPage(tabs.current);
	ME.PreviewReload(false);
	return frame;
end

-- Workspace -------------------------------------------------------------------------------------------------------------------

local separator = { separator = true };

local function BuildMenus()
	return {
		{
			title = "Model",
			{ text = "Open from selection", onClick = OpenFromSelection },
			{ text = "Browse models...", onClick = BrowseModels },
			{ text = "Recent models...", onClick = ShowRecent },
			separator,
			{ text = "Save", shortcut = "Ctrl+S", onClick = Save },
			{ text = "Save as copy...", onClick = function() tabs:Select(TAB_SAVE); end },
			{ text = "Close model", onClick = CloseCurrent },
			separator,
			{ text = "Export model and textures", onClick = Export },
			{ text = "Import image...", onClick = function()
				local entry = ME.Current();
				ME.ShowImport(entry and entry.sel.texture);
			end },
			{ text = "Reload the preview", shortcut = "F5", onClick = ReloadPreview },
			{ text = "Preview in a window", checked = function() return ME.IsPreviewFloating(); end, onClick = function() ME.SetPreviewFloating(not ME.IsPreviewFloating()); end },
		},
	};
end

local KEYS = {
	{ "Ctrl+S", "Save the model over its own file, loose custom models only" },
	{ "F5", "Reload the model and its preview" },
	{ "Drag in the preview", "Turn the model, Shift or right drag moves it" },
	{ "Wheel in the preview", "Zoom" },
};

local HOTKEYS = {
	["CTRL-S"] = Save,
	["F5"] = ReloadPreview,
};

local function BuildStatus()
	local cells = {};
	local info = ME.current and ME.Info(ME.current);

	if ( not info ) then
		table.insert(cells, "No model open");
		return cells;
	end

	table.insert(cells, format("Model %s%s", ME.DocName(ME.current), info.modern and " (MD21)" or ""));

	if ( #ME.order > 1 ) then
		table.insert(cells, format("%d models open", #ME.order));
	end

	if ( info.dirty ) then
		table.insert(cells, "|cffffcc00unsaved changes|r");
	end

	return cells;
end

GameEditor_RegisterWorkspace({
	key = "model",
	title = "Models",
	category = "Visuals",
	order = 9,
	dockWidth = 520,
	tools = {
		{ key = "select", name = "Models", icon = 0xF8B8, build = Build },
	},
	menus = BuildMenus,
	keys = KEYS,
	onActivate = function()
		ME.SyncDocs();
		ME.Changed();
	end,
	onDeactivate = function()
		ME.HideTrackWindow();
		ME.HideColorPicker();
	end,
	onHotkey = function(key)
		local handler = HOTKEYS[key];

		if ( handler ) then
			handler();
			return true;
		end

		return false;
	end,
	status = BuildStatus,
});
