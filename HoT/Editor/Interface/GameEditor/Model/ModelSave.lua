-- Save page of the models workspace: save in place, save a copy and add the copy to the creature or gameobject DBCs.

local ME = GameEditorModel;
local Common = GameEditorObject;

local LABEL_WIDTH = ME.LABEL_WIDTH;
local Num = ME.Num;

local KIND_CREATURE, KIND_GAMEOBJECT = "creature", "gameobject";

local MODEL_DATA_DEFAULTS = {
	m_flags = 0,
	m_sizeClass = 1,
	m_modelScale = 1,
	m_collisionWidth = 0.5,
	m_collisionHeight = 2,
	m_mountHeight = 0,
	m_worldEffectScale = 1,
	m_attachedEffectScale = 1,
	m_missileCollisionRadius = 0.5,
};

local DISPLAY_DEFAULTS = {
	m_soundID = 0,
	m_extendedDisplayInfoID = 0,
	m_creatureModelScale = 1,
	m_creatureModelAlpha = 255,
	m_sizeClass = 0,
	m_bloodID = 0,
	m_npcsoundID = 0,
	m_particleColorID = 0,
	m_creatureGeosetData = 0,
	m_objectEffectPackageID = 0,
};

local page;
local state = { kind = KIND_CREATURE, pending = {}, created = {} };

local function Entry()
	return ME.Current();
end

local function Info()
	return ME.current and ME.Info(ME.current);
end

local function Key(tableName, id)
	return tableName..":"..tostring(id);
end

-- Saving the model -------------------------------------------------------------------------------------------------

function ME.SaveInPlace()
	local entry = Entry();

	if ( not entry ) then
		ME.Notice("Open a model first", "info");
		return false;
	end

	local info = Info();

	if ( info and not info.dirty and entry.loose ~= false ) then
		GameEditor_Notify(format("Nothing to save, %s has no unsaved changes", entry.path), "info");
		return true;
	end

	local ok, message = ME.Do("ModelEditor_Save", entry.doc, nil, { overwrite = true });

	if ( ok ) then
		entry.loose = true;
		GameEditor_Notify(message and tostring(message) ~= "" and tostring(message) or format("Saved %s", entry.path), "ok");
		ME.Edited(entry.doc);
	elseif ( message and tostring(message):find("not a loose file", 1, true) ) then
		entry.loose = false;
		ME.Changed(entry.doc);
	end

	return ok and true or false;
end

-- Writes the model loose into the game folder under its own path, so it overrides the archive copy.
function ME.SaveLooseOverride()
	local entry = Entry();

	if ( not entry ) then
		ME.Notice("Open a model first", "info");
		return false;
	end

	local ok, message = ME.Do("ModelEditor_Save", entry.doc, entry.path, { overwrite = true });

	if ( ok ) then
		entry.loose = true;
		ME.AddRecent(entry.path);
		GameEditor_Notify(message and tostring(message) ~= "" and tostring(message) or format("Saved %s as a loose override", entry.path), "ok");
		ME.Edited(entry.doc);
	end

	return ok and true or false;
end

local function DefaultCopyPath(path)
	local folder = ME.Folder(path);
	local base = ME.BaseName(path);

	if ( base == "" ) then
		return "";
	end

	return (folder ~= "" and (folder.."\\") or "")..base.."Custom.m2";
end

local function SaveCopy()
	local entry = Entry();

	if ( not entry ) then
		ME.Notice("Open a model first", "info");
		return;
	end

	local dest = ME.ToM2(ME.CleanPath(page.dest:GetText()));

	if ( dest == "" or ME.Extension(dest) ~= "m2" ) then
		ME.Notice("Type the destination as a game path ending in .m2", "info");
		return;
	end

	if ( ME.SamePath(dest, entry.path) ) then
		if ( entry.loose == false ) then
			ME.SaveLooseOverride();
		else
			ME.SaveInPlace();
		end

		return;
	end

	local folder = ME.CleanPath(page.textureFolder:GetText());
	local ok, message = ME.Do("ModelEditor_Save", entry.doc, dest, {
		overwrite = page.overwrite.checked and true or false,
		copyTextures = page.copyTextures.checked and true or false,
		textureFolder = folder ~= "" and folder or nil,
	});

	if ( ok ) then
		entry.path = dest;
		entry.savedCopy = dest;
		ME.AddRecent(dest);
		GameEditor_Notify(message and tostring(message) ~= "" and tostring(message) or format("Saved a copy to %s", dest), "ok");
		ME.Edited(entry.doc);
	end
end

-- DBC rows ----------------------------------------------------------------------------------------------------------

local function SendRow(tableName, id, values)
	local applied = ME.Do("DbcEditor_ApplyLocal", tableName, id, values);

	if ( not applied ) then
		return false, false;
	end

	local sent = ME.Do("DbcEditor_Save", tableName, id, values);
	state.pending[Key(tableName, id)] = sent and "sending" or "local only";
	return true, sent and true or false;
end

function ME.OnDbcResult(ok, tableName, id, message)
	local key = Key(tableName, tonumber(id));

	if ( state.pending[key] ) then
		state.pending[key] = (ok == true or ok == 1) and "saved" or ("failed"..(message and message ~= "" and (": "..message) or ""));
		ME.Changed();
	end
end

local function RowStatus(tableName, id)
	return state.pending[Key(tableName, id)] or "";
end

local function TextureVariationTypes(info)
	local used = {};

	for _, texture in ipairs(info and info.textures or {}) do
		local kind = Num(texture.type);

		if ( kind >= 11 and kind <= 13 ) then
			used[kind] = used[kind] or {};
			table.insert(used[kind], texture.index);
		end
	end

	return used;
end

local function CreateCreatureRows()
	local entry = Entry();
	local info = Info();

	if ( not entry or not info ) then
		ME.Notice("Open a model first", "info");
		return;
	end

	local modelId = floor(page.newModelData.value);
	local displayId = floor(page.newDisplay.value);

	if ( modelId <= 0 or displayId <= 0 ) then
		ME.Notice("Give both new rows an id above 0", "info");
		return;
	end

	if ( ME.Row("CreatureModelData", modelId) and state.created[Key("CreatureModelData", modelId)] == nil ) then
		ME.Notice(format("CreatureModelData %d already exists, pick a free id", modelId));
		return;
	end

	if ( ME.Row("CreatureDisplayInfo", displayId) and state.created[Key("CreatureDisplayInfo", displayId)] == nil ) then
		ME.Notice(format("CreatureDisplayInfo %d already exists, pick a free id", displayId));
		return;
	end

	local sourceModel = ME.Row("CreatureModelData", floor(page.sourceModelData.value));
	local model = ME.Copy(sourceModel or MODEL_DATA_DEFAULTS);
	model.m_id = modelId;
	model.m_modelName = ME.ToMdx(entry.path);

	local sourceDisplay = ME.Row("CreatureDisplayInfo", floor(page.sourceDisplay.value));
	local display = ME.Copy(sourceDisplay or DISPLAY_DEFAULTS);
	display.m_id = displayId;
	display.m_modelID = modelId;
	display.m_creatureModelScale = page.scale.value;

	for index = 1, 3 do
		display["m_textureVariation["..(index - 1).."]"] = strtrim(page.variations[index]:GetText() or "");
	end

	local modelApplied, modelSent = SendRow("CreatureModelData", modelId, model);
	local displayApplied, displaySent = false, false;

	if ( modelApplied ) then
		displayApplied, displaySent = SendRow("CreatureDisplayInfo", displayId, display);
	end

	if ( modelApplied and displayApplied ) then
		state.created[Key("CreatureModelData", modelId)] = true;
		state.created[Key("CreatureDisplayInfo", displayId)] = true;
		state.lastDisplay = displayId;
		state.lastKind = KIND_CREATURE;

		if ( modelSent and displaySent ) then
			GameEditor_Notify(format("Added CreatureModelData %d and CreatureDisplayInfo %d in the client, saving them to the database", modelId, displayId), "ok");
		else
			GameEditor_Notify(format("CreatureModelData %d and CreatureDisplayInfo %d are applied in the client only, not saved to the database", modelId, displayId), "error");
		end
	elseif ( modelApplied ) then
		GameEditor_Notify(format("Only CreatureModelData %d was added, CreatureDisplayInfo %d failed", modelId, displayId), "error");
	end

	ME.Changed();
end

local function CreateGameObjectRow()
	local entry = Entry();

	if ( not entry ) then
		ME.Notice("Open a model first", "info");
		return;
	end

	local displayId = floor(page.newGob.value);

	if ( displayId <= 0 ) then
		ME.Notice("Give the new row an id above 0", "info");
		return;
	end

	if ( ME.Row("GameObjectDisplayInfo", displayId) and state.created[Key("GameObjectDisplayInfo", displayId)] == nil ) then
		ME.Notice(format("GameObjectDisplayInfo %d already exists, pick a free id", displayId));
		return;
	end

	local source = ME.Row("GameObjectDisplayInfo", floor(page.sourceGob.value));
	local values = ME.Copy(source or {});
	values.m_id = displayId;
	values.m_modelName = ME.ToMdx(entry.path);

	local applied, sent = SendRow("GameObjectDisplayInfo", displayId, values);

	if ( applied ) then
		state.created[Key("GameObjectDisplayInfo", displayId)] = true;
		state.lastDisplay = displayId;
		state.lastKind = KIND_GAMEOBJECT;

		if ( sent ) then
			GameEditor_Notify(format("Added GameObjectDisplayInfo %d in the client, saving it to the database", displayId), "ok");
		else
			GameEditor_Notify(format("GameObjectDisplayInfo %d is applied in the client only, not saved to the database", displayId), "error");
		end
	end

	ME.Changed();
end

local function CreateRows()
	local info = Info();

	if ( info and info.dirty ) then
		ME.Notice("The model has unsaved changes, the new rows point at the file on disk. Save it first to see the edits in the world", "info");
	end

	local entry = Entry();

	if ( entry and not entry.savedCopy ) then
		ME.Notice("The rows point at the original model path, use Save as copy first for a model of your own", "info");
	end

	if ( state.kind == KIND_GAMEOBJECT ) then
		CreateGameObjectRow();
	else
		CreateCreatureRows();
	end
end

-- Use it --------------------------------------------------------------------------------------------------------------

local function UseDisplay()
	return state.lastDisplay or (state.kind == KIND_GAMEOBJECT and floor(page.newGob.value) or floor(page.newDisplay.value));
end

local function MorphTarget()
	local id = UseDisplay();

	if ( not UnitExists("target") ) then
		ME.Notice("Target a creature with your character first, .morph works on the target", "info");
		return;
	end

	GameEditor_RunCommand(format(".morph %d", id));
end

local function SetSpawnModel()
	local kind, guid = GameEditor_GetSelection();
	local Creature = GameEditorCreature;

	if ( kind ~= KIND_CREATURE ) then
		ME.Notice("Select a creature in the world first", "info");
		return;
	end

	if ( Creature and Creature.IsTemplateMode and Creature.IsTemplateMode() ) then
		ME.Notice("The creature workspace edits a template right now, close the template there first", "info");
		return;
	end

	local data = ME.Quiet("CreatureEditor_GetData", "selection");

	if ( type(data) ~= "table" or data.guid ~= guid ) then
		ME.Call("CreatureEditor_Request", guid);
		ME.Notice("The creature data is loading, press the button again in a moment", "info");
		return;
	end

	if ( not data.spawn ) then
		ME.Notice("This creature is not a database spawn", "info");
		return;
	end

	ME.Do("CreatureEditor_SetFields", 1, { modelid = UseDisplay() });
end

local function EditTemplate()
	local kind, _, entry = GameEditor_GetSelection();
	local wanted = state.kind == KIND_GAMEOBJECT and KIND_GAMEOBJECT or KIND_CREATURE;
	local entryId = (kind == wanted and tonumber(entry)) or (Entry() and Entry().source.entry);

	if ( not entryId or entryId <= 0 ) then
		ME.Notice(format("Select a %s in the world first", wanted), "info");
		return;
	end

	if ( Common.EditTemplate(wanted, entryId) ) then
		if ( wanted == KIND_GAMEOBJECT ) then
			GameEditor_Notify(format("Set displayId to %d on the template and save, spawned objects show it after a respawn", UseDisplay()), "info");
		else
			GameEditor_Notify(format("Set modelid1 to %d on the Template tab and save, clear modelid2 to 4 to always use it", UseDisplay()), "info");
		end
	end
end

-- Build ------------------------------------------------------------------------------------------------------------------

local function Label(parent, text, y, x, color)
	local label = Common.Label(parent, text, nil, color);
	label:SetPoint("TOPLEFT", parent, "TOPLEFT", x or 0, y - 4);
	return label;
end

local function Title(parent, text, y)
	local label = Common.Label(parent, text, "NoggitFontBold", NOGGIT_COLOR_GOLD);
	label:SetPoint("TOPLEFT", parent, "TOPLEFT", 0, y);
	return label;
end

local function Edit(parent, y, width, tooltip)
	local edit = NoggitLineEdit_Create(parent, width);
	edit:SetHeight(22);
	edit:SetMaxLetters(260);
	edit:SetPoint("TOPLEFT", parent, "TOPLEFT", LABEL_WIDTH, y);
	edit:SetScript("OnEnterPressed", function(self) self:ClearFocus(); end);
	Common.Tooltip(edit, tooltip);
	return edit;
end

local function IdSpin(parent, y, tooltip)
	local spin = ME.Spin(parent, 96, 0, 2147483647, 1, 0, function() ME.Changed(); end, tooltip);
	spin:SetPoint("TOPLEFT", parent, "TOPLEFT", LABEL_WIDTH, y);
	return spin;
end

local function FindModelData()
	local entry = Entry();

	if ( not entry ) then
		ME.Notice("Open a model first", "info");
		return;
	end

	local base = ME.BaseName(entry.source.originalPath or entry.path);
	local items = {};

	for _, row in ipairs(ME.Find("CreatureModelData", "m_modelName", base, 100)) do
		table.insert(items, { value = row.id, text = row.name or "" });
	end

	if ( #items == 0 ) then
		ME.Notice(format("No CreatureModelData row names a model called %s", base), "info");
		return;
	end

	GameEditor_PickValue("CreatureModelData to copy", items, floor(page.sourceModelData.value), function(value)
		NoggitSpinBox_SetValue(page.sourceModelData, Num(value), true);
		local displays = ME.Find("CreatureDisplayInfo", "m_modelID", Num(value), 1);

		if ( displays[1] ) then
			NoggitSpinBox_SetValue(page.sourceDisplay, displays[1].id, true);
			page:LoadVariations(displays[1].id);
		end

		ME.Changed();
	end);
end

-- Fills the copy sources and the next free ids once for each doc.
local function Prefill(entry)
	if ( not entry or page.prefilledFor == entry.doc ) then
		return;
	end

	page.prefilledFor = entry.doc;
	local source = entry.source;
	source.originalPath = source.originalPath or entry.path;

	local modelData = source.modelDataId or ME.FindByModelName("CreatureModelData", entry.path)[1] or 0;
	local display = source.kind == KIND_CREATURE and source.displayId or nil;

	if ( not display and modelData > 0 ) then
		local rows = ME.Find("CreatureDisplayInfo", "m_modelID", modelData, 1);
		display = rows[1] and rows[1].id;
	end

	local gob = source.kind == KIND_GAMEOBJECT and source.displayId or ME.FindByModelName("GameObjectDisplayInfo", entry.path)[1] or 0;

	if ( source.kind ) then
		state.kind = source.kind;
	elseif ( gob > 0 and modelData == 0 ) then
		state.kind = KIND_GAMEOBJECT;
	end

	NoggitSpinBox_SetValue(page.sourceModelData, modelData, true);
	NoggitSpinBox_SetValue(page.sourceDisplay, display or 0, true);
	NoggitSpinBox_SetValue(page.sourceGob, gob, true);
	NoggitSpinBox_SetValue(page.newModelData, ME.MaxId("CreatureModelData") + 1, true);
	NoggitSpinBox_SetValue(page.newDisplay, ME.MaxId("CreatureDisplayInfo") + 1, true);
	NoggitSpinBox_SetValue(page.newGob, ME.MaxId("GameObjectDisplayInfo") + 1, true);
	page:LoadVariations(display);
	page.dest:SetText(DefaultCopyPath(entry.path));
	page.textureFolder:SetText("");
	state.lastDisplay = nil;
end

function ME.BuildSavePage(container)
	page = CreateFrame("Frame", "GameEditorModelSavePage", container);
	page:SetAllPoints(container);
	page:Hide();
	page.scroll, page.child = GameEditor_CreateScrollFrame(page);
	page.scroll:SetAllPoints(page);
	local c = page.child;
	c:SetWidth(480);

	local y = 0;
	Title(c, "Model", y);
	page.info = Common.Paragraph(c, "", NOGGIT_COLOR_TEXT);
	page.info:SetPoint("TOPLEFT", c, "TOPLEFT", 0, y - 18);
	page.info:SetPoint("TOPRIGHT", c, "TOPRIGHT", 0, y - 18);
	page.info:SetHeight(76);

	y = y - 98;
	Title(c, "Save", y);
	page.save = Common.Button(c, "Save", 80, ME.SaveInPlace, "Write over the model file, only for loose models of your own in the game folder, a .bak is kept (Ctrl+S)");
	page.save:SetPoint("TOPLEFT", c, "TOPLEFT", LABEL_WIDTH, y - 20);
	page.override = Common.Button(c, "Save as loose override", 150, ME.SaveLooseOverride, "Write the model loose into the game folder under its own path, the client then loads it in place of the archive copy");
	page.override:SetPoint("LEFT", page.save, "RIGHT", 6, 0);
	page.saveNote = Common.Label(c, "", nil, NOGGIT_COLOR_DISABLED);
	page.saveNote:SetPoint("LEFT", page.override, "RIGHT", 8, 0);
	page.saveNote:SetPoint("RIGHT", c, "RIGHT", 0, 0);

	y = y - 50;
	Title(c, "Save as copy", y);
	y = y - 20;
	Label(c, "Destination", y);
	page.dest = Edit(c, y, 320, "Game path of the copy, the skins and anims are written next to it with the new name");
	y = y - 26;
	page.copyTextures = NoggitCheckBox_Create(c, "Copy the textures too", true);
	page.copyTextures:SetPoint("TOPLEFT", c, "TOPLEFT", LABEL_WIDTH, y);
	Common.Tooltip(page.copyTextures, "Copies every texture the model names into the texture folder and points the copy at them, replaceable types are left alone");
	page.overwrite = NoggitCheckBox_Create(c, "Overwrite", false);
	page.overwrite:SetPoint("LEFT", page.copyTextures, "RIGHT", 16, 0);
	Common.Tooltip(page.overwrite, "Write over files that are already there");
	y = y - 26;
	Label(c, "Texture folder", y);
	page.textureFolder = Edit(c, y, 320, "Folder for the copied textures, empty uses the folder of the copy");
	y = y - 28;
	page.saveCopy = Common.Button(c, "Save as copy", 110, SaveCopy, "Write the copy loose under the game folder, the open model then edits the copy");
	page.saveCopy:SetPoint("TOPLEFT", c, "TOPLEFT", LABEL_WIDTH, y);
	page.copyNote = Common.Paragraph(c, "");
	page.copyNote:SetPoint("TOPLEFT", c, "TOPLEFT", 0, y - 26);
	page.copyNote:SetPoint("TOPRIGHT", c, "TOPRIGHT", 0, y - 26);

	y = y - 64;
	Title(c, "Add to DBC", y);
	y = y - 22;
	local group = {};
	page.kindCreature = NoggitCheckBox_Create(c, "Creature display", true, function()
		state.kind = KIND_CREATURE;
		ME.Changed();
	end, group);
	page.kindCreature:SetPoint("TOPLEFT", c, "TOPLEFT", LABEL_WIDTH, y);
	page.kindGob = NoggitCheckBox_Create(c, "Gameobject display", false, function()
		state.kind = KIND_GAMEOBJECT;
		ME.Changed();
	end, group);
	page.kindGob:SetPoint("LEFT", page.kindCreature, "RIGHT", 16, 0);

	y = y - 28;
	local top = y;

	-- Creature rows.
	local creature = CreateFrame("Frame", NoggitUI_NewName("GameEditorModelSaveCreature"), c);
	creature:SetPoint("TOPLEFT", c, "TOPLEFT", 0, top);
	creature:SetPoint("TOPRIGHT", c, "TOPRIGHT", 0, top);
	creature:SetHeight(280);
	page.creature = creature;

	local cy = 0;
	Label(creature, "Copy model data", cy);
	page.sourceModelData = IdSpin(creature, cy, "CreatureModelData row whose collision, size and sounds the new row copies, 0 for defaults");
	page.findModel = Common.Button(creature, "Find", 50, FindModelData, "Look for CreatureModelData rows of this model name");
	page.findModel:SetPoint("LEFT", page.sourceModelData, "RIGHT", 6, 0);
	page.sourceModelText = Common.Label(creature, "", nil, NOGGIT_COLOR_DISABLED);
	page.sourceModelText:SetPoint("LEFT", page.findModel, "RIGHT", 6, 0);
	page.sourceModelText:SetPoint("RIGHT", creature, "RIGHT", 0, 0);

	cy = cy - 26;
	Label(creature, "Copy display", cy);
	page.sourceDisplay = IdSpin(creature, cy, "CreatureDisplayInfo row the new display copies, 0 for defaults");
	page.loadVariations = Common.Button(creature, "Use its skins", 96, function()
		local displayId = floor(page.sourceDisplay.value);

		if ( not ME.Row("CreatureDisplayInfo", displayId) ) then
			ME.Notice(format("CreatureDisplayInfo %d does not exist, pick a display to copy first", displayId), "info");
			return;
		end

		page:LoadVariations(displayId);
		ME.Changed();
	end, "Copy the texture variations of that display into the fields below");
	page.loadVariations:SetPoint("LEFT", page.sourceDisplay, "RIGHT", 6, 0);

	cy = cy - 26;
	Label(creature, "New model data id", cy);
	page.newModelData = IdSpin(creature, cy, "Id of the new CreatureModelData row, the next free one by default");
	page.modelStatus = Common.Label(creature, "", nil, NOGGIT_COLOR_DISABLED);
	page.modelStatus:SetPoint("LEFT", page.newModelData, "RIGHT", 6, 0);
	page.modelStatus:SetPoint("RIGHT", creature, "RIGHT", 0, 0);

	cy = cy - 26;
	Label(creature, "New display id", cy);
	page.newDisplay = IdSpin(creature, cy, "Id of the new CreatureDisplayInfo row, the next free one by default");
	page.displayStatus = Common.Label(creature, "", nil, NOGGIT_COLOR_DISABLED);
	page.displayStatus:SetPoint("LEFT", page.newDisplay, "RIGHT", 6, 0);
	page.displayStatus:SetPoint("RIGHT", creature, "RIGHT", 0, 0);

	cy = cy - 26;
	Label(creature, "Display scale", cy);
	page.scale = ME.Spin(creature, 96, 0.01, 100, 0.1, 3, function() end, "m_creatureModelScale of the new display");
	page.scale:SetPoint("TOPLEFT", creature, "TOPLEFT", LABEL_WIDTH, cy);
	NoggitSpinBox_SetValue(page.scale, 1, true);

	cy = cy - 30;
	page.variationNote = Common.Paragraph(creature, "Texture variations fill the textures of type 11, 12 and 13 of the model. Each is a file name without .blp in the model folder.");
	page.variationNote:SetPoint("TOPLEFT", creature, "TOPLEFT", 0, cy);
	page.variationNote:SetPoint("TOPRIGHT", creature, "TOPRIGHT", 0, cy);

	cy = cy - 32;
	page.variations = {};
	page.variationHints = {};

	for index = 1, 3 do
		Label(creature, format("Variation %d (type %d)", index, 10 + index), cy);
		local edit = NoggitLineEdit_Create(creature, 160);
		edit:SetHeight(22);
		edit:SetMaxLetters(100);
		edit:SetPoint("TOPLEFT", creature, "TOPLEFT", LABEL_WIDTH, cy);
		edit:SetScript("OnEnterPressed", function(self) self:ClearFocus(); end);
		edit:SetScript("OnEditFocusLost", function() ME.Changed(); end);
		Common.Tooltip(edit, format("m_textureVariation[%d], drawn on the texture of type %d", index - 1, 10 + index));
		page.variations[index] = edit;
		local hint = Common.Label(creature, "", nil, NOGGIT_COLOR_DISABLED);
		hint:SetPoint("LEFT", edit, "RIGHT", 6, 0);
		hint:SetPoint("RIGHT", creature, "RIGHT", 0, 0);
		page.variationHints[index] = hint;
		cy = cy - 26;
	end

	-- Gameobject row.
	local gob = CreateFrame("Frame", NoggitUI_NewName("GameEditorModelSaveGob"), c);
	gob:SetPoint("TOPLEFT", c, "TOPLEFT", 0, top);
	gob:SetPoint("TOPRIGHT", c, "TOPRIGHT", 0, top);
	gob:SetHeight(280);
	page.gob = gob;

	Label(gob, "Copy display", 0);
	page.sourceGob = IdSpin(gob, 0, "GameObjectDisplayInfo row whose sounds and bounds the new row copies, 0 for none");
	Label(gob, "New display id", -26);
	page.newGob = IdSpin(gob, -26, "Id of the new GameObjectDisplayInfo row, the next free one by default");
	page.gobStatus = Common.Label(gob, "", nil, NOGGIT_COLOR_DISABLED);
	page.gobStatus:SetPoint("LEFT", page.newGob, "RIGHT", 6, 0);
	page.gobStatus:SetPoint("RIGHT", gob, "RIGHT", 0, 0);
	page.gobNote = Common.Paragraph(gob, "Gameobjects take their skins from GameObjectArtKit, its texture variations fill the textures of type 11 to 13.");
	page.gobNote:SetPoint("TOPLEFT", gob, "TOPLEFT", 0, -56);
	page.gobNote:SetPoint("TOPRIGHT", gob, "TOPRIGHT", 0, -56);

	y = top + cy - 6;
	page.create = Common.Button(c, "Create rows", 110, CreateRows, "Add the rows to the client at once with DbcEditor_ApplyLocal and send them to the server with DbcEditor_Save");
	page.create:SetPoint("TOPLEFT", c, "TOPLEFT", LABEL_WIDTH, y);
	page.result = Common.Label(c, "", nil, NOGGIT_COLOR_TEXT);
	page.result:SetPoint("LEFT", page.create, "RIGHT", 8, 0);
	page.result:SetPoint("RIGHT", c, "RIGHT", 0, 0);

	y = y - 34;
	Title(c, "Use it", y);
	y = y - 22;
	page.morph = Common.Button(c, "Morph my target", 120, MorphTarget, "Runs .morph with the new display on the target of your character, it lasts until the creature resets");
	page.morph:SetPoint("TOPLEFT", c, "TOPLEFT", 0, y);
	page.spawnModel = Common.Button(c, "Set spawn model", 120, SetSpawnModel, "Saves the new display as creature.modelid of the selected spawn");
	page.spawnModel:SetPoint("LEFT", page.morph, "RIGHT", 4, 0);
	page.template = Common.Button(c, "Edit template", 110, EditTemplate, "Opens the template of the selection in its workspace to set the display there");
	page.template:SetPoint("LEFT", page.spawnModel, "RIGHT", 4, 0);
	page.useNote = Common.Paragraph(c, "");
	page.useNote:SetPoint("TOPLEFT", c, "TOPLEFT", 0, y - 28);
	page.useNote:SetPoint("TOPRIGHT", c, "TOPRIGHT", 0, y - 28);

	c:SetHeight(-y + 80);

	page.LoadVariations = function(self, displayId)
		local row = ME.Row("CreatureDisplayInfo", displayId);

		for index = 1, 3 do
			self.variations[index]:SetText(row and row["m_textureVariation["..(index - 1).."]"] or "");
		end

		if ( row and Num(row.m_creatureModelScale) > 0 ) then
			NoggitSpinBox_SetValue(self.scale, Num(row.m_creatureModelScale), true);
		end
	end;

	page.Refresh = function(self)
		local entry = Entry();
		local info = Info();
		Prefill(entry);

		if ( not entry or not info ) then
			self.info:SetText("No model open");
		else
			local lines = {
				entry.path,
				format("%s, version %s, %s bytes, %d bones, %d vertices, %d skin profiles", info.modern and "Modern MD21" or "Classic", tostring(info.version or "?"), tostring(info.fileSize or "?"), #(info.bones or {}) > 0 and #info.bones or Num(info.boneCount), Num(info.vertexCount), Num(info.skinProfiles)),
				format("%d textures, %d emitters, %d ribbons, %d sequences%s", #(info.textures or {}), #(info.particles or {}), #(info.ribbons or {}), #(info.sequences or {}), info.dirty and ", |cffffcc00unsaved changes|r" or ""),
			};

			if ( info.modern ) then
				local names = {};

				for _, chunk in ipairs(type(info.chunks) == "table" and info.chunks or {}) do
					table.insert(names, type(chunk) == "table" and tostring(chunk.id or chunk.name or chunk[1] or "?") or tostring(chunk));
				end

				local chunks = type(info.chunks) == "table" and table.concat(names, " ") or tostring(info.chunks or "");
				table.insert(lines, format("MD21 source version %s, chunks %s", tostring(info.sourceVersion or info.version or "?"), chunks ~= "" and chunks or "unknown"));
			end

			self.info:SetText(table.concat(lines, "\n"));
		end

		if ( info and info.modern ) then
			local shared = {};

			if ( info.skinsByFileDataId ) then
				table.insert(shared, "skins");
			end

			if ( info.animsByFileDataId ) then
				table.insert(shared, "anims");
			end

			if ( #shared > 0 ) then
				self.copyNote:SetText(format("This modern model finds its %s by file data id, a copy keeps using the original files.", table.concat(shared, " and ")));
			else
				self.copyNote:SetText("This modern model names its skins and anims by path, the copy gets its own files with the new name.");
			end
		else
			self.copyNote:SetText("The copy gets its own .skin and .anim files with the new name.");
		end

		if ( not entry ) then
			self.saveNote:SetText("");
		elseif ( entry.loose == false ) then
			self.saveNote:SetText("|cffffcc00Archive model, use loose override or Save as copy|r");
		elseif ( entry.loose ) then
			self.saveNote:SetText("Loose file, Save writes over it");
		else
			self.saveNote:SetText("Save works on loose files, archive models need the override or a copy");
		end

		local enabled = entry ~= nil;
		ME.SetEnabled(self.save, enabled);
		ME.SetEnabled(self.override, enabled);
		ME.SetEnabled(self.saveCopy, enabled);
		ME.SetEnabled(self.create, enabled);
		NoggitCheckBox_SetChecked(self.kindCreature, state.kind == KIND_CREATURE);
		NoggitCheckBox_SetChecked(self.kindGob, state.kind == KIND_GAMEOBJECT);

		if ( state.kind == KIND_GAMEOBJECT ) then
			self.creature:Hide();
			self.gob:Show();
		else
			self.gob:Hide();
			self.creature:Show();
		end

		local sourceModel = ME.Row("CreatureModelData", floor(self.sourceModelData.value));
		self.sourceModelText:SetText(sourceModel and (sourceModel.m_modelName or "") or "defaults");

		local modelId, displayId, gobId = floor(self.newModelData.value), floor(self.newDisplay.value), floor(self.newGob.value);
		local function Status(tableName, id)
			local status = RowStatus(tableName, id);

			if ( status ~= "" ) then
				return status;
			end

			return ME.Row(tableName, id) and "|cffff6060id taken|r" or "free";
		end

		self.modelStatus:SetText(Status("CreatureModelData", modelId));
		self.displayStatus:SetText(Status("CreatureDisplayInfo", displayId));
		self.gobStatus:SetText(Status("GameObjectDisplayInfo", gobId));

		local used = TextureVariationTypes(info);

		for index = 1, 3 do
			local textures = used[10 + index];
			local text = strtrim(self.variations[index]:GetText() or "");

			if ( textures ) then
				self.variationHints[index]:SetText(format("texture %s%s", table.concat(textures, ", "), text ~= "" and format(", %s.blp", text) or ""));
			else
				self.variationHints[index]:SetText(format("no type %d texture in the model", 10 + index));
			end
		end

		if ( state.lastDisplay ) then
			self.result:SetText(format("Display %d ready", state.lastDisplay));
		else
			self.result:SetText("");
		end

		local display = UseDisplay();

		if ( state.kind == KIND_GAMEOBJECT ) then
			self.morph:Hide();
			self.spawnModel:Hide();
			self.useNote:SetText(format("Gameobjects take the display from gameobject_template.displayId. Select an object and press Edit template, set displayId to %d and save, then respawn it.", display));
		else
			self.morph:Show();
			self.spawnModel:Show();
			self.useNote:SetText(format("Morph is a quick look on your target. Set spawn model saves %d on the selected spawn, Edit template opens the creature template to put it in modelid1. If the server checks creature_model_info, it needs a row for the new display too.", display));
		end

		self.child:SetWidth(max(380, (self.scroll:GetWidth() or 480) - 14));
		self.scroll.UpdateRange();
	end;

	return page;
end
