-- Displays workspace data, CreatureDisplayInfo, CreatureDisplayInfoExtra, CreatureModelData and GameObjectDisplayInfo rows, creature_model_info and a model preview.

GameEditorDisplay = GameEditorDisplay or {};
local DE = GameEditorDisplay;
local ME = GameEditorModel;
local Common = GameEditorObject;
local Data = GameEditorData;
local I = GameEditorItem;

DE.KINDS = {
	creature = { table = "CreatureDisplayInfo", title = "Creature display" },
	extra = { table = "CreatureDisplayInfoExtra", title = "Character display" },
	model = { table = "CreatureModelData", title = "Creature model" },
	object = { table = "GameObjectDisplayInfo", title = "Object display" },
};

DE.store = GameEditor_CreateDbcDocs("Display editor", { CreatureDisplayInfo = true, CreatureDisplayInfoExtra = true, CreatureModelData = true, GameObjectDisplayInfo = true });
DE.ids = DE.ids or {};

local Num = ME.Num;
DE.Num = Num;

DE.MODEL_FLAGS = {};

for bit = 0, 31 do
	table.insert(DE.MODEL_FLAGS, { bit = bit, name = format("Flag 0x%X", 2 ^ bit) });
end

-- The 11 npc item slots in ITEM_SLOT order, types are the inventory types an item worn there has.
DE.NPC_ITEM_SLOTS = {
	{ label = "Head", types = { 1 } },
	{ label = "Shoulders", types = { 3 } },
	{ label = "Shirt", types = { 4 } },
	{ label = "Chest", types = { 5, 20 } },
	{ label = "Waist", types = { 6 } },
	{ label = "Legs", types = { 7 } },
	{ label = "Feet", types = { 8 } },
	{ label = "Wrists", types = { 9 } },
	{ label = "Hands", types = { 10 } },
	{ label = "Tabard", types = { 19 } },
	{ label = "Back", types = { 16 } },
};

DE.SEXES = { [0] = "Male", [1] = "Female" };

local LOOK_TIP = "Index into the CharSections of the race, the barber shop number minus one";
local BAKE_TIP = "Texture in Textures\\BakedNpcTextures that replaces the skin, looks and armor textures. Clear it, or tick Ignore bake, to see look and item changes";

-- Column specs of the forms, kind is int, float, text, path, ref, combo, item or flags, anything not listed is an int or float.
DE.COLUMNS = {
	CreatureDisplayInfo = {
		m_modelID = { kind = "ref", ref = "dbc:CreatureModelData", label = "Model data", tip = "CreatureModelData row with the M2 path, Go opens it" },
		m_creatureModelScale = { label = "Scale", tip = "Multiplies the scale of the model data" },
		m_creatureModelAlpha = { label = "Alpha", tip = "0 to 255" },
		["m_textureVariation[0]"] = { kind = "text", label = "Skin 1", tip = "Texture name next to the M2, no folder and no extension" },
		["m_textureVariation[1]"] = { kind = "text", label = "Skin 2", tip = "Texture name next to the M2, no folder and no extension" },
		["m_textureVariation[2]"] = { kind = "text", label = "Skin 3", tip = "Texture name next to the M2, no folder and no extension" },
		m_portraitTextureName = { kind = "text", label = "Portrait texture" },
		m_extendedDisplayInfoID = { kind = "ref", ref = "dbc:CreatureDisplayInfoExtra", label = "Extra display info", tip = "CreatureDisplayInfoExtra row for character style models" },
		m_soundID = { label = "Sound data", tip = "CreatureSoundData id, overrides the one of the model data" },
		m_npcsoundID = { label = "NPC sounds", tip = "NPCSounds id, greeting and farewell sounds" },
		m_sizeClass = { label = "Size class" },
		m_bloodID = { label = "Blood" },
		m_particleColorID = { label = "Particle color" },
		m_creatureGeosetData = { label = "Geoset data", tip = "Geoset group values packed 4 bits each" },
		m_objectEffectPackageID = { label = "Object effect package" },
	},
	CreatureDisplayInfoExtra = {
		m_displayRaceID = { kind = "ref", ref = "dbc:ChrRaces", label = "Race", tip = "ChrRaces row, picks the character model" },
		m_displaySexID = { kind = "combo", items = ME.EnumItems(DE.SEXES, 0, 1), label = "Sex" },
		m_skinID = { label = "Skin color", low = 0, high = 255, tip = LOOK_TIP },
		m_faceID = { label = "Face", low = 0, high = 255, tip = LOOK_TIP },
		m_hairStyleID = { label = "Hair style", low = 0, high = 255, tip = LOOK_TIP },
		m_hairColorID = { label = "Hair color", low = 0, high = 255, tip = LOOK_TIP },
		m_facialHairID = { label = "Facial hair", low = 0, high = 255, tip = LOOK_TIP },
		m_flags = { kind = "flags", flags = DE.MODEL_FLAGS, header = "Other" },
		m_bakeName = { kind = "text", label = "Baked texture", tip = BAKE_TIP },
	},
	CreatureModelData = {
		m_modelName = { kind = "path", label = "Model", tip = "M2 path, .mdx is fine" },
		m_flags = { kind = "flags", flags = DE.MODEL_FLAGS },
		m_modelScale = { label = "Scale" },
		m_soundID = { label = "Sound data", tip = "CreatureSoundData id" },
		m_footprintTextureID = { label = "Footprint texture" },
		m_foleyMaterialID = { label = "Foley material" },
		m_collisionWidth = { label = "Collision width" },
		m_collisionHeight = { label = "Collision height" },
		m_mountHeight = { label = "Mount height", tip = "Height of the rider above the ground" },
	},
	GameObjectDisplayInfo = {
		m_modelName = { kind = "path", label = "Model", tip = "M2 or WMO path" },
		["m_geoBoxMin[0]"] = { label = "Box min X" },
		["m_geoBoxMin[1]"] = { label = "Box min Y" },
		["m_geoBoxMin[2]"] = { label = "Box min Z" },
		["m_geoBoxMax[0]"] = { label = "Box max X" },
		["m_geoBoxMax[1]"] = { label = "Box max Y" },
		["m_geoBoxMax[2]"] = { label = "Box max Z" },
		m_objectEffectPackageID = { label = "Object effect package" },
	},
};

for slot, info in ipairs(DE.NPC_ITEM_SLOTS) do
	DE.COLUMNS.CreatureDisplayInfoExtra["m_npcitemDisplay["..(slot - 1).."]"] = { kind = "item", slot = slot - 1, label = info.label, header = slot == 1 and "Items" or nil };
end

for slot = 0, 9 do
	DE.COLUMNS.GameObjectDisplayInfo["m_sound["..slot.."]"] = { kind = "ref", ref = "dbc:SoundEntries", label = "Sound "..(slot + 1) };
end

Data.Enums.CreatureGender = { [0] = "Male", [1] = "Female", [2] = "None" };

Data.Fields.creature_model_info = {
	DisplayID = { label = "Display", type = "int", ref = "dbc:CreatureDisplayInfo" },
	BoundingRadius = { label = "Bounding radius", tooltip = "Radius the server uses for melee range and collision", type = "float" },
	CombatReach = { label = "Combat reach", tooltip = "Extra melee reach of the creature", type = "float" },
	Gender = { label = "Gender", type = "int", enum = "CreatureGender" },
	DisplayID_Other_Gender = { label = "Other gender display", tooltip = "Picked at random instead of this display half of the time, 0 for none", type = "int", ref = "dbc:CreatureDisplayInfo" },
};
Data.Columns.creature_model_info = { "DisplayID", "BoundingRadius", "CombatReach", "Gender", "DisplayID_Other_Gender" };

function DE.Label(column)
	local name = string.gsub(column, "^m_", "");
	name = string.gsub(name, "%[(%d+)%]", function(index) return " "..(tonumber(index) + 1); end);
	name = string.gsub(name, "(%l)(%u)", "%1 %2");
	return string.upper(string.sub(name, 1, 1))..string.lower(string.sub(name, 2));
end

-- Npc item slots ------------------------------------------------------------------------------------------

local slotLists, slotNames = {}, {};

local function InventoryTypeOf(row)
	local _, _, inventoryType = string.find(tostring(row.subName or ""), "(%-?%d+)$");
	return tonumber(inventoryType);
end

-- Items worn in the slot as picker items whose value is the item display id, plus the item name of each display.
local function SlotList(slot)
	if ( slotLists[slot] ) then
		return slotLists[slot], false, slotNames[slot];
	end

	local items, loading = GameEditor_TemplateList("item");
	local wanted = {};

	for _, inventoryType in ipairs(DE.NPC_ITEM_SLOTS[slot + 1].types) do
		wanted[inventoryType] = true;
	end

	local list, names = {}, {};

	for _, entry in ipairs(items) do
		local row = entry.row;
		local displayId = row and Num(row.displayId) or 0;

		if ( displayId > 0 and wanted[InventoryTypeOf(row) or -1] ) then
			local name = I and I.Colored and I.Colored(row.name, row.type) or (row.name or "");
			table.insert(list, { value = displayId, text = format("%s  |cff808080item %d|r", name, row.entry) });
			names[displayId] = names[displayId] or name;
		end
	end

	if ( not loading ) then
		slotLists[slot], slotNames[slot] = list, names;
	end

	return list, loading, names;
end

GameEditor_OnTemplateList("item", function()
	slotLists, slotNames = {}, {};
end);

function DE.PickSlotItem(slot, current, onPick)
	GameEditor_PickValue(DE.NPC_ITEM_SLOTS[slot + 1].label.." items", function()
		local list, loading = SlotList(slot);
		return list, loading;
	end, current, onPick, nil, { anyNumber = true });
end

-- An item that uses the display, else what the ItemDisplayInfo row shows.
function DE.ItemDisplayText(slot, value)
	value = Num(value);

	if ( value <= 0 ) then
		return "";
	end

	local _, _, names = SlotList(slot);

	if ( names and names[value] ) then
		return names[value];
	end

	local row = ME.Row("ItemDisplayInfo", value);

	if ( not row ) then
		return "|cffff6060no ItemDisplayInfo row|r";
	end

	local text = row["m_inventoryIcon[0]"] or "";
	return text ~= "" and text or (row["m_modelName[0]"] or "exists");
end

-- An item display id with a picker of the items worn in the slot.
local function ItemSlotRow(form, key, label, slot)
	local item = form:Number(key, label, 0, 2147483647, 1, 0, "ItemDisplayInfo id, ... picks it from the items worn in this slot");
	local pick = Common.Button(item.frame, "...", 26, function()
		if ( form.data ) then
			DE.PickSlotItem(slot, Num(form.data[key]), function(value) form:Set(key, value); end);
		end
	end, "Pick an item worn in this slot, its display id goes in");
	pick:SetPoint("LEFT", item.spin, "RIGHT", 4, 0);
	local clear = Common.Button(item.frame, "x", 22, function()
		if ( form.data ) then
			form:Set(key, 0);
		end
	end, "Take the item off");
	clear:SetPoint("LEFT", pick, "RIGHT", 4, 0);

	item.hint:ClearAllPoints();
	item.hint:SetPoint("LEFT", clear, "RIGHT", 8, 0);
	item.hint:SetPoint("RIGHT", item.frame, "RIGHT", 0, 0);
	item.hintFunc = function(value) return DE.ItemDisplayText(slot, value); end;

	local refresh = item.refresh;
	item.refresh = function(data, info)
		refresh(data, info);
		ME.SetEnabled(pick, data ~= nil);
		ME.SetEnabled(clear, data ~= nil and Num(data[key]) ~= 0);
	end;
	return item;
end

-- Every stored column of the table as a form row, the specs above pick the widget.
function DE.AddColumns(form, tableName)
	local specs = DE.COLUMNS[tableName] or {};
	local schema = ME.Quiet("DbcEditor_GetSchema", tableName);

	for _, column in ipairs(type(schema) == "table" and schema or {}) do
		local name = column.name;
		local spec = specs[name] or {};
		local label = spec.label or DE.Label(name);
		local kind = spec.kind or (column.type == "float" and "float") or (column.type == "string" and "text") or "int";

		if ( not column.stored or name == "m_id" ) then
			kind = nil;
		end

		if ( kind and spec.header ) then
			form:Header(spec.header);
		end

		if ( kind == "ref" ) then
			form:Ref(name, label, spec.ref, spec.tip);
		elseif ( kind == "text" ) then
			form:Text(name, label, spec.tip);
		elseif ( kind == "path" ) then
			form:Path(name, label, spec.tip);
		elseif ( kind == "combo" ) then
			form:Combo(name, label, spec.items, spec.tip);
		elseif ( kind == "item" ) then
			ItemSlotRow(form, name, label, spec.slot);
		elseif ( kind == "flags" ) then
			form:Flags(name, label, spec.flags, 32, spec.tip);
		elseif ( kind == "float" ) then
			form:Float(name, label, spec.tip);
		elseif ( kind == "int" ) then
			form:Int(name, label, spec.low or (column.type == "uint" and 0 or -2147483648), spec.high or (column.type == "uint" and 4294967295 or 2147483647), spec.tip);
		end
	end
end

-- What a row shows: the model path, the display to skin it with and its scale.
function DE.PreviewOf(kind, values)
	if ( kind == "creature" ) then
		local modelData = ME.Row("CreatureModelData", values.m_modelID);
		local scale = Num(values.m_creatureModelScale) > 0 and Num(values.m_creatureModelScale) or 1;
		return modelData and modelData.m_modelName or "", values.m_id, scale * (modelData and Num(modelData.m_modelScale) > 0 and Num(modelData.m_modelScale) or 1);
	elseif ( kind == "model" ) then
		return values.m_modelName or "", nil, Num(values.m_modelScale) > 0 and Num(values.m_modelScale) or 1;
	end

	return values.m_modelName or "", nil, 1;
end

-- Character displays ----------------------------------------------------------------------------------------

local raceNames = {};

function DE.RaceName(race)
	race = Num(race);

	if ( raceNames[race] == nil ) then
		local row = ME.Row("ChrRaces", race);
		local name = row and row[GameEditor_DbcNameColumn("ChrRaces") or "m_name[0]"];
		raceNames[race] = name and name ~= "" and name or false;
	end

	return raceNames[race] or format("race %d", race);
end

function DE.ExtraText(values)
	local text = format("%s %s", DE.RaceName(values.m_displayRaceID), DE.SEXES[Num(values.m_displaySexID)] or "?");

	if ( values.m_bakeName and values.m_bakeName ~= "" ) then
		text = text.."  "..values.m_bakeName;
	end

	return text;
end

local EXTRA_KEYS = { "m_displayRaceID", "m_displaySexID", "m_skinID", "m_faceID", "m_hairStyleID", "m_hairColorID", "m_facialHairID", "m_bakeName" };

for slot = 0, 10 do
	table.insert(EXTRA_KEYS, "m_npcitemDisplay["..slot.."]");
end

-- Changes whenever anything the dressed preview shows changes.
function DE.ExtraKey(id, values, ignoreBake)
	local parts = { tostring(id), ignoreBake and "1" or "0" };

	for _, key in ipairs(EXTRA_KEYS) do
		table.insert(parts, tostring(values[key] or ""));
	end

	return table.concat(parts, ":");
end

-- Preview -------------------------------------------------------------------------------------------------------

local ZOOM_STEP = 1.15;
local ROTATE_SPEED = 0.012;
local DEFAULT_PITCH = 0.25;
local RETRY_DELAY = 0.1;
local RETRY_LIMIT = 100;

local Preview = {};
Preview.__index = Preview;

function Preview:Try(method, ...)
	if ( self.model[method] ) then
		return pcall(self.model[method], self.model, ...);
	end

	return false;
end

function Preview:ApplyCamera()
	local view = self.view;

	if ( self.path and not ME.Quiet("ModelEditor_SetPreviewView", self.model, view.yaw, view.pitch, view.zoom, view.panX, view.panY) and self.retries < RETRY_LIMIT ) then
		self.retries = self.retries + 1;
		self.reapplyIn = RETRY_DELAY;
	end
end

function Preview:ApplyView()
	self:Try("SetPosition", 0, 0, 0);
	self:Try("SetFacing", 0);
	self:Try("SetModelScale", self.scale or 1);
	self:Try("SetLight", 1, 0, -0.5, 0.4, -0.75, 0.6, 1, 1, 1, 0.9, 1, 1, 1);

	if ( self.skin ) then
		ME.Quiet("ModelEditor_SetPreviewSkin", self.model, self.skin);
	end

	self:ApplyCamera();
	self:Try("SetSequence", 0);
end

-- Shows path skinned with the CreatureDisplayInfo skin, the same values again keep the camera.
-- Back to a plain model after a dressed character.
function Preview:Undress()
	if ( self.extra ) then
		ME.Quiet("ModelEditor_SetPreviewExtra", self.model, 0);
		self.extra, self.extraKey, self.path = nil, nil, nil;
	end
end

function Preview:Show(path, skin, scale, message)
	path = path or "";
	self:Undress();

	if ( path == "" ) then
		self:Try("ClearModel");
		self.path = nil;
		self.message:SetText(message or "No model");
		return;
	end

	if ( path == self.path and skin == self.skin and scale == self.scale and not self.dirty ) then
		return;
	end

	local changedModel = path ~= self.path;
	self.skin, self.scale, self.dirty = skin, scale, false;

	if ( changedModel ) then
		local ok, err = self:Try("SetModel", ME.ToM2 and ME.ToM2(path) or path);

		if ( not ok ) then
			self.message:SetText("Preview failed: "..tostring(err or "no model frame"));
			return;
		end

		self.path = path;
		self.retries = 0;
	end

	self.message:SetText("");
	self:ApplyView();
	self.reapplyIn = 0.3;
end

-- The race model dressed by a CreatureDisplayInfoExtra row, key from DE.ExtraKey skips rebuilding the same look.
function Preview:ShowExtra(id, key, ignoreBake, scale)
	if ( self.extra and key == self.extraKey and not self.dirty ) then
		return;
	end

	local ok, path = ME.Quiet("ModelEditor_SetPreviewExtra", self.model, id, ignoreBake);

	if ( not ok ) then
		self:Undress();
		self:Try("ClearModel");
		self.path, self.extra, self.extraKey, self.dirty = nil, true, key, false;
		self.message:SetText("No character model for this race and sex");
		return;
	end

	if ( path ~= self.path ) then
		self.retries = 0;
	end

	self.path, self.skin, self.scale, self.dirty = path, nil, scale or 1, false;
	self.extra, self.extraKey = true, key;
	self.message:SetText("");
	self:ApplyView();
	self.reapplyIn = 0.3;
end

-- Skins and DBC rows changed under the same path, the next Show reloads them.
function Preview:Invalidate()
	self.dirty = true;
end

function Preview:ResetCamera()
	self.view = { yaw = 0, pitch = DEFAULT_PITCH, zoom = 1, panX = 0, panY = 0 };
	self.retries = 0;
	self:ApplyView();
end

function DE.CreatePreview(holder)
	local preview = setmetatable({ view = { yaw = 0, pitch = DEFAULT_PITCH, zoom = 1, panX = 0, panY = 0 }, retries = 0 }, Preview);

	local panel = CreateFrame("Frame", NoggitUI_NewName("GameEditorDisplayPreview"), holder, "NoggitBoxTemplate");
	NoggitBox_Set(panel, { 0.07, 0.07, 0.08, 1 }, NOGGIT_COLOR_MID);
	panel:SetAllPoints(holder);

	local model = CreateFrame("Model", NoggitUI_NewName("GameEditorDisplayPreviewModel"), panel);
	model:SetPoint("TOPLEFT", panel, "TOPLEFT", 2, -2);
	model:SetPoint("BOTTOMRIGHT", panel, "BOTTOMRIGHT", -2, 2);
	model:EnableMouse(true);
	model:EnableMouseWheel(true);
	preview.model = model;

	model:SetScript("OnMouseDown", function(self, button)
		local x, y = NoggitUI_CursorIn(self);
		local view = preview.view;
		preview.drag = { button = button, x = x, y = y, yaw = view.yaw, pitch = view.pitch, panX = view.panX, panY = view.panY, pan = button ~= "LeftButton" or IsShiftKeyDown() };
	end);
	model:SetScript("OnMouseUp", function() preview.drag = nil; end);
	model:SetScript("OnMouseWheel", function(_, delta)
		preview.view.zoom = max(0.02, min(50, preview.view.zoom * (delta > 0 and 1 / ZOOM_STEP or ZOOM_STEP)));
		preview:ApplyCamera();
	end);
	model:SetScript("OnUpdate", function(self, elapsed)
		local drag = preview.drag;

		if ( preview.extra and preview.path ) then
			ME.Quiet("ModelEditor_UpdatePreviewExtra", self);
		end

		if ( drag ) then
			if ( not EditorIsMouseButtonDown(drag.button) ) then
				preview.drag = nil;
			else
				local x, y = NoggitUI_CursorIn(self);
				local dx, dy = x - drag.x, y - drag.y;
				local view = preview.view;

				if ( drag.pan ) then
					local unit = 2.4 * view.zoom / max(1, self:GetHeight());
					view.panX = drag.panX - dx * unit;
					view.panY = drag.panY - dy * unit;
				else
					view.yaw = drag.yaw - dx * ROTATE_SPEED;
					view.pitch = max(-1.5, min(1.5, drag.pitch + dy * ROTATE_SPEED));
				end

				preview:ApplyCamera();
			end
		end

		if ( preview.reapplyIn ) then
			preview.reapplyIn = preview.reapplyIn - elapsed;

			if ( preview.reapplyIn <= 0 ) then
				preview.reapplyIn = nil;
				preview:ApplyView();
			end
		end
	end);
	Common.Tooltip(model, "Drag to orbit, Shift or right drag to pan, wheel to zoom");

	preview.message = Common.Label(panel, "", nil, NOGGIT_COLOR_DISABLED);
	preview.message:SetPoint("TOPLEFT", panel, "TOPLEFT", 8, -8);
	preview.message:SetPoint("TOPRIGHT", panel, "TOPRIGHT", -8, -8);

	local reset = Common.Button(panel, "Reset", 50, function() preview:ResetCamera(); end, "Reset the turn, zoom and offset");
	reset:SetHeight(20);
	reset:SetPoint("BOTTOMRIGHT", panel, "BOTTOMRIGHT", -4, 4);
	reset:SetFrameLevel(model:GetFrameLevel() + 2);

	panel:SetScript("OnShow", function() preview.reapplyIn = 0.3; end);
	return preview;
end
