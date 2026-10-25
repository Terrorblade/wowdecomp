-- Lights workspace, edits Light.dbc and its params, bands and skyboxes with live preview in the world.

local LE = GameEditorLight;
local Common = GameEditorObject;

local W = 420;
local LABEL_WIDTH = 140;
local UPDATE_TIME = 0.5;

local TAB_NAMES = { "Light", "Params", "Colors", "Fog", "Skybox" };
local TAB_LIGHT, TAB_PARAMS, TAB_COLORS, TAB_FOG, TAB_SKYBOX = 1, 2, 3, 4, 5;

-- World x, y, z, falloff start and end and the Light column each one lives in.
local WORLD_COLUMN = { "m_gameCoords[2]", "m_gameCoords[0]", "m_gameCoords[1]", "m_gameFalloffStart", "m_gameFalloffEnd" };
local WORLD_RAW = { 3, 1, 2, 4, 5 };

local frame;
local list;
local tabs;
local pages = {};
local header;
local timeText;
local timeSlider;
local serverTime;
local statusText;
local saveButton;
local revertButton;

local state = {};
local items = {};
local selected;
local slot = 0;
local intBand = 0;
local floatBand = 0;
local skyboxId;
local showOthers = true;
local previewSlot = false;
local timeOverride = false;
local frozenTime;
local timeEdit;
local lastMap;
local updateElapsed = 0;
local skyboxItems;

local function IsActive()
	return GameEditor_GetWorkspace() == "light";
end

local function Num(value)
	return tonumber(value) or 0;
end

local function LightDoc()
	return selected and LE.Doc("Light", selected);
end

local function IsDefaultValues(values)
	return Num(values[LE.COORDS[1]]) == 0 and Num(values[LE.COORDS[2]]) == 0 and Num(values[LE.COORDS[3]]) == 0;
end

local function World(values)
	return LightEditor_DbcToWorld(Num(values[LE.COORDS[1]]), Num(values[LE.COORDS[2]]), Num(values[LE.COORDS[3]]), Num(values.m_gameFalloffStart), Num(values.m_gameFalloffEnd));
end

local function ParamsId()
	local doc = LightDoc();
	return doc and Num(doc.values[LE.SlotColumn(slot)]) or 0;
end

local function ParamsDoc()
	return LE.Doc("LightParams", ParamsId());
end

local function SkyboxShown()
	if ( skyboxId ) then
		return skyboxId;
	end

	local params = ParamsDoc();
	return params and Num(params.values.m_lightSkyboxID) or 0;
end

local function PreviewTime()
	if ( timeOverride and frozenTime ) then
		return frozenTime;
	end

	return Num(state.time);
end

local function UpdateDisplay()
	if ( LightEditor_SetDisplay ) then
		LightEditor_SetDisplay(IsActive(), selected, showOthers);
	end
end

local function ApplyPreviewSlot()
	if ( not LightEditor_SetPreviewSlot ) then
		return;
	end

	if ( previewSlot and IsActive() ) then
		LightEditor_SetPreviewSlot(slot);
	else
		LightEditor_SetPreviewSlot(nil);
	end
end

-- Camera position while the free camera flies, the character otherwise.
local function HerePosition()
	if ( GameEditor_IsFreeCamEnabled and GameEditor_IsFreeCamEnabled() and EditorGetCamera ) then
		local x, y, z = EditorGetCamera();
		return x, y, z;
	end

	return LightEditor_GetPlayerPosition();
end

-- Light list -------------------------------------------------------------------------------------------------

local function FindItemIndex(id)
	for index, item in ipairs(items) do
		if ( item.id == id ) then
			return index;
		end
	end
end

local function RefreshList(resort)
	if ( not list or not LightEditor_GetLights ) then
		return;
	end

	local lights = LightEditor_GetLights() or {};

	if ( resort or #lights ~= #items ) then
		table.sort(lights, function(a, b)
			if ( a.isDefault ~= b.isDefault ) then
				return a.isDefault and true or false;
			end

			return a.distance < b.distance;
		end);
		items = lights;
		NoggitListBox_SetItems(list, items, true);
	else
		local byId = {};

		for _, light in ipairs(lights) do
			byId[light.id] = light;
		end

		for index, item in ipairs(items) do
			if ( byId[item.id] ) then
				items[index] = byId[item.id];
			end
		end

		list.items = items;
	end

	list.selected = FindItemIndex(selected);
	NoggitListBox_Refresh(list);
end

local function ActiveText()
	local parts = {};

	for _, active in ipairs(state.active or {}) do
		table.insert(parts, format("%d (%d%%)", active.id, floor(active.weight * 100 + 0.5)));
	end

	if ( #parts == 0 ) then
		return format("Only the map default light %d", Num(state.defaultId));
	end

	return "In "..table.concat(parts, ", ");
end

local function RefreshHeader()
	if ( header ) then
		header:SetText(format("Map %d, %d lights. %s", Num(state.mapId), #items, ActiveText()));
	end
end

-- Pages --------------------------------------------------------------------------------------------------------

local function NewPage(container)
	local page = CreateFrame("Frame", NoggitUI_NewName("GameEditorLightPage"), container);
	page:SetAllPoints(container);
	page.scroll, page.child = GameEditor_CreateScrollFrame(page);
	page.scroll:SetAllPoints(page);
	page:Hide();
	return page;
end

local function Label(parent, text, y, x, color)
	local label = Common.Label(parent, text, nil, color);
	label:SetPoint("TOPLEFT", parent, "TOPLEFT", x or 0, y - 4);
	return label;
end

local function Spin(parent, y, x, width, low, high, step, decimals, onChange)
	local spin = NoggitSpinBox_Create(parent, width, low, high, step, decimals, function(_, value)
		onChange(value);
	end);
	spin:SetPoint("TOPLEFT", parent, "TOPLEFT", x, y);
	return spin;
end

local function SetSpin(spin, value, enabled)
	if ( not _G[spin:GetName().."Edit"]:HasFocus() ) then
		NoggitSpinBox_SetValue(spin, Num(value), true);
	end

	spin:SetEnabledState(enabled ~= false);
end

local function Check(parent, text, y, x, onChange, tooltip)
	local box = NoggitCheckBox_Create(parent, text, false, function(_, checked) onChange(checked); end);
	box:SetPoint("TOPLEFT", parent, "TOPLEFT", x or 0, y);

	if ( tooltip ) then
		local enter = box:GetScript("OnEnter");
		local leave = box:GetScript("OnLeave");
		box:SetScript("OnEnter", function(self)
			if ( enter ) then
				enter(self);
			end

			NoggitTooltip_Show(self, tooltip);
		end);
		box:SetScript("OnLeave", function(self)
			if ( leave ) then
				leave(self);
			end

			NoggitTooltip_Hide();
		end);
	end

	return box;
end

local function SetCheckEnabled(box, enabled)
	if ( enabled ) then
		box:Enable();
	else
		box:Disable();
	end

	NoggitCheckBox_Refresh(box);
end

local function SetWorld(doc, index, value)
	local world = { World(doc.values) };
	world[index] = value;
	local raw = { LightEditor_WorldToDbc(world[1], world[2], world[3], world[4], world[5]) };
	LE.Set(doc, WORLD_COLUMN[index], raw[WORLD_RAW[index]]);
end

local function ParamsUsage(paramsId)
	local users = {};

	for _, item in ipairs(items) do
		for s = 0, LE.SLOTS - 1 do
			if ( Num(item.params[s + 1]) == paramsId ) then
				table.insert(users, format("%d %s", item.id, LE.SLOT_NAMES[s]));
			end
		end
	end

	return users;
end

local function CopyParamsForSlot()
	local doc = LightDoc();

	if ( not doc ) then
		return;
	end

	local source = ParamsId();
	local id = LE.CopyParams(source);
	LE.Set(doc, LE.SlotColumn(slot), id);
	GameEditor_Notify(format("Params %d copied to %d with its bands, slot %s of light %d uses the copy", source, id, LE.SLOT_NAMES[slot], doc.id), "ok");
end

local function PickParams(doc, s)
	local seen = {};
	local pickItems = { { value = 0, text = "None" } };

	for _, item in ipairs(items) do
		for i = 0, LE.SLOTS - 1 do
			local id = Num(item.params[i + 1]);

			if ( id > 0 and not seen[id] ) then
				seen[id] = true;
				table.insert(pickItems, { value = id, text = table.concat(ParamsUsage(id), ", ") });
			end
		end
	end

	table.sort(pickItems, function(a, b) return a.value < b.value; end);

	GameEditor_PickValue(format("Params for %s", LE.SLOT_NAMES[s]), pickItems, Num(doc.values[LE.SlotColumn(s)]), function(value)
		LE.Set(doc, LE.SlotColumn(s), Num(value));
	end);
end

local function BuildLightPage(container)
	local page = NewPage(container);
	local c = page.child;
	c:SetHeight(480);

	page.title = Common.Label(c, "", "NoggitFontBold");
	page.title:SetPoint("TOPLEFT", c, "TOPLEFT", 0, 0);
	page.note = Common.Label(c, "", nil, NOGGIT_COLOR_DISABLED);
	page.note:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -16);
	page.note:SetWidth(W);

	local function OnDoc(func)
		return function(value)
			local doc = LightDoc();

			if ( doc ) then
				func(doc, value);
			end
		end;
	end

	Label(c, "Map", -36);
	page.map = Spin(c, -36, LABEL_WIDTH, 100, 0, 100000, 1, 0, OnDoc(function(doc, value) LE.Set(doc, "m_continentID", value); end));

	local names = { "Position X", "Position Y", "Position Z", "Falloff start (yd)", "Falloff end (yd)" };
	page.world = {};

	for i = 1, 5 do
		local y = -62 - (i - 1) * 26;
		Label(c, names[i], y);
		page.world[i] = Spin(c, y, LABEL_WIDTH, 120, i <= 3 and -100000 or 0, 100000, 1, 2, OnDoc(function(doc, value) SetWorld(doc, i, value); end));
	end

	page.raw = Common.Label(c, "", nil, NOGGIT_COLOR_DISABLED);
	page.raw:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -196);
	page.raw:SetWidth(W);

	page.others = Check(c, "Show the other lights of this map", -214, 0, function(checked)
		showOthers = checked;
		UpdateDisplay();
	end, "Small markers for every other light within 2000 yards");

	local slotsTitle = Common.Label(c, "Params slots", "NoggitFontBold");
	slotsTitle:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -244);

	page.slots = {};

	for s = 0, LE.SLOTS - 1 do
		local y = -264 - s * 26;
		local row = {};
		row.label = Label(c, format("%d %s", s, LE.SLOT_NAMES[s]), y);
		row.spin = Spin(c, y, LABEL_WIDTH, 84, 0, 1000000, 1, 0, OnDoc(function(doc, value) LE.Set(doc, LE.SlotColumn(s), value); end));
		row.pick = Common.Button(c, "Pick", 46, function()
			local doc = LightDoc();

			if ( doc ) then
				PickParams(doc, s);
			end
		end, LE.SLOT_TIPS[s]);
		row.pick:SetPoint("TOPLEFT", c, "TOPLEFT", LABEL_WIDTH + 90, y);
		row.edit = Common.Button(c, "Edit", 46, function()
			slot = s;
			ApplyPreviewSlot();
			tabs:Select(TAB_PARAMS);
		end, "Edit this params set in the Params, Colors and Fog tabs");
		row.edit:SetPoint("LEFT", row.pick, "RIGHT", 4, 0);
		row.copy = Common.Button(c, "Copy", 46, function()
			slot = s;
			CopyParamsForSlot();
		end, "Give this slot its own copy of the params set and its bands, other lights keep the old one");
		row.copy:SetPoint("LEFT", row.edit, "RIGHT", 4, 0);
		page.slots[s] = row;
	end

	page.Refresh = function(self)
		local doc = LightDoc();

		if ( not doc ) then
			self.title:SetText("No light selected");
			self.note:SetText("Pick a light in the list or make a new one.");
			self.raw:SetText("");

			for i = 1, 5 do
				self.world[i]:SetEnabledState(false);
			end

			self.map:SetEnabledState(false);

			for s = 0, LE.SLOTS - 1 do
				self.slots[s].spin:SetEnabledState(false);
				Common.SetEnabled(self.slots[s].pick, false);
				Common.SetEnabled(self.slots[s].edit, false);
				Common.SetEnabled(self.slots[s].copy, false);
			end

			return;
		end

		local values = doc.values;
		local isDefault = IsDefaultValues(values);
		self.title:SetText(format("Light %d%s", doc.id, doc.isNew and " (new)" or ""));

		if ( isDefault ) then
			self.note:SetText("Map default light. It has no position and every area light blends over it.");
		elseif ( doc.remoteChanged ) then
			self.note:SetText("Changed elsewhere since you opened it.");
		else
			self.note:SetText("Position and falloff in world yards, saved in DBC units.");
		end

		SetSpin(self.map, values.m_continentID);
		local world = { World(values) };

		for i = 1, 5 do
			SetSpin(self.world[i], world[i], not isDefault);
		end

		self.raw:SetText(format("DBC: %.1f, %.1f, %.1f, falloff %.1f to %.1f", Num(values[LE.COORDS[1]]), Num(values[LE.COORDS[2]]), Num(values[LE.COORDS[3]]), Num(values.m_gameFalloffStart), Num(values.m_gameFalloffEnd)));
		NoggitCheckBox_SetChecked(self.others, showOthers);

		for s = 0, LE.SLOTS - 1 do
			local row = self.slots[s];
			SetSpin(row.spin, values[LE.SlotColumn(s)]);
			Common.SetEnabled(row.pick, true);
			Common.SetEnabled(row.edit, Num(values[LE.SlotColumn(s)]) > 0);
			Common.SetEnabled(row.copy, true);
			NoggitText_SetColor(row.label, s == slot and NOGGIT_COLOR_GOLD or NOGGIT_COLOR_TEXT);
		end
	end;

	return page;
end

local function SkyboxPath(id)
	local values = Num(id) > 0 and LE.Values("LightSkybox", id);
	return values and values.m_name or "";
end

local function SkyboxPickItems()
	if ( not skyboxItems ) then
		skyboxItems = { { value = 0, text = "None" } };

		for id = 1, LE.MaxId("LightSkybox") do
			local values = LE.Values("LightSkybox", id);

			if ( values ) then
				table.insert(skyboxItems, { value = id, text = values.m_name or "" });
			end
		end
	end

	return skyboxItems;
end

local function SlotComboItems()
	local doc = LightDoc();
	local comboItems = {};

	for s = 0, LE.SLOTS - 1 do
		local id = doc and Num(doc.values[LE.SlotColumn(s)]) or 0;
		table.insert(comboItems, { text = format("%d %s (%d)", s, LE.SLOT_NAMES[s], id) });
	end

	return comboItems;
end

local function BuildParamsPage(container)
	local page = NewPage(container);
	local c = page.child;
	c:SetHeight(360);

	local function OnParams(column)
		return function(value)
			local doc = ParamsDoc();

			if ( doc ) then
				LE.Set(doc, column, value);
			end
		end;
	end

	Label(c, "Slot", 0);
	page.slot = NoggitComboBox_Create(c, 240, SlotComboItems(), slot + 1, function(_, index)
		slot = index - 1;
		ApplyPreviewSlot();
		LE.Changed();
	end);
	page.slot:SetPoint("TOPLEFT", c, "TOPLEFT", LABEL_WIDTH - 60, 0);

	page.preview = Check(c, "Preview this slot", -28, 0, function(checked)
		previewSlot = checked;
		ApplyPreviewSlot();
	end, "Shows this slot of the map default light everywhere the way a screen effect does, area lights are left out while it is on");

	page.copy = Common.Button(c, "Make a copy", 100, CopyParamsForSlot, "Give the slot of the selected light its own copy of this params set and its bands");
	page.copy:SetPoint("TOPRIGHT", c, "TOPRIGHT", 0, -28);

	page.title = Common.Label(c, "", "NoggitFontBold");
	page.title:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -58);
	page.usage = Common.Label(c, "", nil, NOGGIT_COLOR_DISABLED);
	page.usage:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -74);
	page.usage:SetWidth(W);

	page.highlight = Check(c, "Highlight sky", -94, 0, function(checked)
		OnParams("m_highlightSky")(checked and 1 or 0);
	end, "Brightens the sky around the sun at dawn and dusk");

	Label(c, "Skybox", -122);
	page.skybox = Spin(c, -122, LABEL_WIDTH, 84, 0, 1000000, 1, 0, OnParams("m_lightSkyboxID"));
	page.skyboxPick = Common.Button(c, "Pick", 46, function()
		local doc = ParamsDoc();

		if ( doc ) then
			GameEditor_PickValue("Skybox", SkyboxPickItems(), Num(doc.values.m_lightSkyboxID), function(value)
				LE.Set(doc, "m_lightSkyboxID", Num(value));
			end);
		end
	end, "Pick a LightSkybox row");
	page.skyboxPick:SetPoint("TOPLEFT", c, "TOPLEFT", LABEL_WIDTH + 90, -122);
	page.skyboxEdit = Common.Button(c, "Edit", 46, function()
		skyboxId = nil;
		tabs:Select(TAB_SKYBOX);
	end, "Edit this skybox in the Skybox tab");
	page.skyboxEdit:SetPoint("LEFT", page.skyboxPick, "RIGHT", 4, 0);
	page.skyboxPath = Common.Label(c, "", nil, NOGGIT_COLOR_DISABLED);
	page.skyboxPath:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -148);
	page.skyboxPath:SetWidth(W);

	local fields = {
		{ "Cloud type", "m_cloudTypeID", 0, 100000, 1, 0 },
		{ "Glow", "m_glow", 0, 10, 0.05, 3 },
		{ "Water shallow alpha", "m_waterShallowAlpha", 0, 1, 0.05, 3 },
		{ "Water deep alpha", "m_waterDeepAlpha", 0, 1, 0.05, 3 },
		{ "Ocean shallow alpha", "m_oceanShallowAlpha", 0, 1, 0.05, 3 },
		{ "Ocean deep alpha", "m_oceanDeepAlpha", 0, 1, 0.05, 3 },
	};

	page.fields = {};

	for i, field in ipairs(fields) do
		local y = -170 - (i - 1) * 26;
		Label(c, field[1], y);
		page.fields[i] = { column = field[2], spin = Spin(c, y, LABEL_WIDTH, 100, field[3], field[4], field[5], field[6], OnParams(field[2])) };
	end

	page.Refresh = function(self)
		self.slot.items = SlotComboItems();
		NoggitComboBox_SetSelected(self.slot, slot + 1, true);
		NoggitCheckBox_SetChecked(self.preview, previewSlot);

		local paramsId = ParamsId();
		local doc = ParamsDoc();
		Common.SetEnabled(self.copy, LightDoc() ~= nil);

		if ( not doc ) then
			self.title:SetText(paramsId > 0 and format("Params %d does not exist", paramsId) or "This slot has no params");
			self.usage:SetText(LightDoc() and "Pick a params id on the Light tab or make a copy." or "No light selected.");
			SetCheckEnabled(self.highlight, false);
			self.skybox:SetEnabledState(false);
			Common.SetEnabled(self.skyboxPick, false);
			Common.SetEnabled(self.skyboxEdit, false);
			self.skyboxPath:SetText("");

			for _, field in ipairs(self.fields) do
				field.spin:SetEnabledState(false);
			end

			return;
		end

		local values = doc.values;
		self.title:SetText(format("Params %d%s", doc.id, doc.isNew and " (new)" or ""));
		local users = ParamsUsage(doc.id);
		self.usage:SetText(#users > 1 and ("Shared, edits change light "..table.concat(users, ", ")) or "Not shared with other lights of this map");

		SetCheckEnabled(self.highlight, true);
		NoggitCheckBox_SetChecked(self.highlight, Num(values.m_highlightSky) ~= 0);
		SetSpin(self.skybox, values.m_lightSkyboxID);
		Common.SetEnabled(self.skyboxPick, true);
		Common.SetEnabled(self.skyboxEdit, Num(values.m_lightSkyboxID) > 0);
		self.skyboxPath:SetText(Num(values.m_lightSkyboxID) > 0 and SkyboxPath(values.m_lightSkyboxID) or "No skybox model");

		for _, field in ipairs(self.fields) do
			SetSpin(field.spin, values[field.column]);
		end
	end;

	return page;
end

local function BandPage(container, isColor)
	local page = NewPage(container);
	local c = page.child;
	local tableName = isColor and "LightIntBand" or "LightFloatBand";
	local count = isColor and LE.INT_BANDS or LE.FLOAT_BANDS;
	local names = isColor and LE.INT_BAND_NAMES or LE.FLOAT_BAND_NAMES;
	local rowHeight = 18;

	local function Selected()
		return isColor and intBand or floatBand;
	end

	page.title = Common.Label(c, "", "NoggitFontBold");
	page.title:SetPoint("TOPLEFT", c, "TOPLEFT", 0, 0);

	page.rows = {};

	for band = 0, count - 1 do
		local y = -20 - band * rowHeight;
		local row = {};
		row.label = Label(c, names[band], y + 2);
		row.strip = LE.CreateStrip(c, W - LABEL_WIDTH, 14);
		row.strip:SetPoint("TOPLEFT", c, "TOPLEFT", LABEL_WIDTH, y);
		row.strip:SetScript("OnClick", function()
			if ( isColor ) then
				intBand = band;
			else
				floatBand = band;
			end

			LE.Changed();
		end);

		if ( not isColor ) then
			Common.Tooltip(row.strip, LE.FLOAT_BAND_TIPS[band]);
		end

		page.rows[band] = row;
	end

	local top = 20 + count * rowHeight + 10;
	page.editor = LE.CreateKeyEditor(c, {
		width = W,
		isColor = isColor,
		getTime = PreviewTime,
		onCreate = function()
			return LE.BandDoc(tableName, ParamsId(), Selected(), true);
		end,
	});
	page.editor:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -top);
	c:SetHeight(top + page.editor:GetHeight());

	page.Refresh = function(self)
		local paramsId = ParamsId();
		local params = ParamsDoc();
		self.title:SetText(params and format("Params %d, slot %d %s", paramsId, slot, LE.SLOT_NAMES[slot]) or "No params set for this slot");

		for band = 0, count - 1 do
			local row = self.rows[band];
			local values = params and LE.Values(tableName, isColor and LE.IntBandId(paramsId, band) or LE.FloatBandId(paramsId, band));
			local keys = values and LE.GetKeys(values) or {};
			row.strip:SetStops(isColor and LE.ColorStops(keys) or LE.FloatStops(keys), false);
			row.strip:SetTime(PreviewTime());
			NoggitText_SetColor(row.label, band == Selected() and NOGGIT_COLOR_GOLD or NOGGIT_COLOR_TEXT);
		end

		if ( params ) then
			local doc = LE.BandDoc(tableName, paramsId, Selected(), false);
			local name = names[Selected()];

			if ( not isColor and Selected() == 0 ) then
				name = name.." (DBC units, 36 per yard)";
			end

			self.editor.onCreate = function()
				return LE.BandDoc(tableName, paramsId, Selected(), true);
			end;
			self.editor:SetBand(doc, name, "The band row does not exist yet, adding a key creates it.");
		else
			self.editor.onCreate = nil;
			self.editor:SetBand(nil, "", "Pick a light and a slot with a params set first.");
		end

		self.editor:SetTime(PreviewTime());
	end;

	page.UpdateTime = function(self)
		for band = 0, count - 1 do
			self.rows[band].strip:SetTime(PreviewTime());
		end

		self.editor:SetTime(PreviewTime());
	end;

	return page;
end

local function BuildSkyboxPage(container)
	local page = NewPage(container);
	local c = page.child;
	c:SetHeight(260);

	local function OnSkybox(column)
		return function(value)
			local doc = LE.Doc("LightSkybox", SkyboxShown());

			if ( doc ) then
				LE.Set(doc, column, value);
			end
		end;
	end

	Label(c, "Skybox", 0);
	page.id = Spin(c, 0, LABEL_WIDTH, 84, 0, 1000000, 1, 0, function(value)
		skyboxId = value;
		LE.Changed();
	end);
	page.pick = Common.Button(c, "Pick", 46, function()
		GameEditor_PickValue("Skybox", SkyboxPickItems(), SkyboxShown(), function(value)
			skyboxId = Num(value);
			LE.Changed();
		end);
	end, "Show another LightSkybox row");
	page.pick:SetPoint("TOPLEFT", c, "TOPLEFT", LABEL_WIDTH + 90, 0);
	page.new = Common.Button(c, "New", 46, function()
		local id = LE.MaxId("LightSkybox") + 1;
		local values = LE.Values("LightSkybox", SkyboxShown()) or { m_name = "", m_flags = 0 };
		LE.CreateDoc("LightSkybox", id, values);
		skyboxId = id;
		skyboxItems = nil;
		LE.Changed();
	end, "Make a new skybox row from this one");
	page.new:SetPoint("LEFT", page.pick, "RIGHT", 4, 0);

	page.title = Common.Label(c, "", "NoggitFontBold");
	page.title:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -30);

	Label(c, "Model", -50);
	page.path = NoggitLineEdit_Create(c, W - LABEL_WIDTH);
	page.path:SetPoint("TOPLEFT", c, "TOPLEFT", LABEL_WIDTH, -50);
	page.path:SetHeight(22);
	page.path:SetScript("OnEditFocusLost", function(self)
		local doc = LE.Doc("LightSkybox", SkyboxShown());

		if ( doc and self:GetText() ~= (doc.values.m_name or "") ) then
			LE.Set(doc, "m_name", self:GetText());
			skyboxItems = nil;
		end
	end);
	page.path:SetScript("OnEnterPressed", function(self) self:ClearFocus(); end);

	Label(c, "Flags", -78);
	page.flags = Spin(c, -78, LABEL_WIDTH, 84, 0, 2147483647, 1, 0, OnSkybox("m_flags"));

	local function FlagBox(bit, text, y, tooltip)
		return Check(c, text, y, 0, function(checked)
			local doc = LE.Doc("LightSkybox", SkyboxShown());

			if ( doc ) then
				local flags = Num(doc.values.m_flags);
				local has = mod(floor(flags / bit), 2) == 1;

				if ( checked and not has ) then
					flags = flags + bit;
				elseif ( not checked and has ) then
					flags = flags - bit;
				end

				LE.Set(doc, "m_flags", flags);
			end
		end, tooltip);
	end

	page.flag1 = FlagBox(1, "Animation follows the time of day", -106, "Flag 1, the model animation is set from the game time");
	page.flag2 = FlagBox(2, "Keep the sky behind it", -130, "Flag 2, the sky dome and other skyboxes still draw under this one");

	page.use = Common.Button(c, "Use for the params", 140, function()
		local params = ParamsDoc();

		if ( params ) then
			LE.Set(params, "m_lightSkyboxID", SkyboxShown());
		end
	end, "Set this skybox on the params set of the selected slot");
	page.use:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -160);

	page.note = Common.Paragraph(c, "A new model path shows at once. Flag changes show after the skybox model loads again, for example on the next map load.");
	page.note:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -192);
	page.note:SetWidth(W);

	page.Refresh = function(self)
		local id = SkyboxShown();
		local doc = id > 0 and LE.Doc("LightSkybox", id);
		SetSpin(self.id, id);
		Common.SetEnabled(self.use, ParamsDoc() ~= nil and doc and true or false);

		if ( not doc ) then
			self.title:SetText(id > 0 and format("Skybox %d does not exist", id) or "No skybox, pick one or make a new one");
			self.path:SetText("");
			self.path:EnableMouse(false);
			self.flags:SetEnabledState(false);
			SetCheckEnabled(self.flag1, false);
			SetCheckEnabled(self.flag2, false);
			return;
		end

		self.title:SetText(format("Skybox %d%s", doc.id, doc.isNew and " (new)" or ""));
		self.path:EnableMouse(true);

		if ( not self.path:HasFocus() ) then
			self.path:SetText(doc.values.m_name or "");
		end

		SetSpin(self.flags, doc.values.m_flags);
		local flags = Num(doc.values.m_flags);
		SetCheckEnabled(self.flag1, true);
		SetCheckEnabled(self.flag2, true);
		NoggitCheckBox_SetChecked(self.flag1, mod(flags, 2) == 1);
		NoggitCheckBox_SetChecked(self.flag2, mod(floor(flags / 2), 2) == 1);
	end;

	return page;
end

-- Refresh ------------------------------------------------------------------------------------------------------

local function AnyBandDirty(tableName, paramsId, count)
	if ( paramsId <= 0 ) then
		return false;
	end

	for band = 0, count - 1 do
		local id = tableName == "LightIntBand" and LE.IntBandId(paramsId, band) or LE.FloatBandId(paramsId, band);

		if ( LE.IsDirty(LE.FindDoc(tableName, id)) ) then
			return true;
		end
	end

	return false;
end

local function RefreshDirty()
	if ( not tabs ) then
		return;
	end

	local paramsId = ParamsId();
	tabs:SetDirty(TAB_LIGHT, LE.IsDirty(selected and LE.FindDoc("Light", selected)));
	tabs:SetDirty(TAB_PARAMS, LE.IsDirty(LE.FindDoc("LightParams", paramsId)));
	tabs:SetDirty(TAB_COLORS, AnyBandDirty("LightIntBand", paramsId, LE.INT_BANDS));
	tabs:SetDirty(TAB_FOG, AnyBandDirty("LightFloatBand", paramsId, LE.FLOAT_BANDS));
	tabs:SetDirty(TAB_SKYBOX, LE.IsDirty(LE.FindDoc("LightSkybox", SkyboxShown())));

	local count = LE.DirtyCount();

	if ( count > 0 ) then
		statusText:SetText(format("%d unsaved row%s", count, count == 1 and "" or "s"));
		NoggitText_SetColor(statusText, NOGGIT_COLOR_GOLD);
	else
		statusText:SetText("No changes");
		NoggitText_SetColor(statusText, NOGGIT_COLOR_DISABLED);
	end

	Common.SetEnabled(saveButton, count > 0);
	Common.SetEnabled(revertButton, count > 0);
end

local function RefreshPages()
	if ( not frame ) then
		return;
	end

	local page = pages[tabs.current];

	if ( page and page.Refresh ) then
		page:Refresh();
	end

	RefreshDirty();
end

local function ShowPage(index)
	for i, page in pairs(pages) do
		if ( i == index ) then
			page:Show();
			page.scroll.UpdateRange();
		else
			page:Hide();
		end
	end

	RefreshPages();
end

local function Select(id)
	selected = id;
	skyboxId = nil;
	UpdateDisplay();
	RefreshList(false);
	RefreshPages();
end

LE.OnChanged(function()
	if ( frame and frame:IsShown() ) then
		RefreshList(false);
		RefreshPages();
	end
end);

-- Actions ------------------------------------------------------------------------------------------------------

local function GoToLight()
	local doc = LightDoc();

	if ( not doc ) then
		GameEditor_Notify("Select a light first", "info");
		return;
	end

	if ( IsDefaultValues(doc.values) ) then
		GameEditor_Notify("The map default light has no position", "info");
		return;
	end

	local x, y, z = World(doc.values);

	if ( GameEditor_IsFreeCamEnabled and GameEditor_IsFreeCamEnabled() and EditorSetCamera ) then
		EditorSetCamera(x, y, z + 5);
	else
		GameEditor_RunCommand(format(".go xyz %.3f %.3f %.3f %d", x, y, z, Num(doc.values.m_continentID)));
	end
end

local function MoveHere()
	local doc = LightDoc();

	if ( not doc ) then
		GameEditor_Notify("Select a light first", "info");
		return;
	end

	if ( IsDefaultValues(doc.values) ) then
		GameEditor_Notify("The map default light has no position, make a new light instead", "info");
		return;
	end

	local x, y, z = HerePosition();

	if ( not x ) then
		return;
	end

	local _, _, _, falloffStart, falloffEnd = World(doc.values);
	local rx, ry, rz = LightEditor_WorldToDbc(x, y, z, falloffStart, falloffEnd);
	LE.SetMany(doc, { [LE.COORDS[1]] = rx, [LE.COORDS[2]] = ry, [LE.COORDS[3]] = rz });
end

local function NewLight()
	local x, y, z = HerePosition();

	if ( not x ) then
		GameEditor_Notify("Enter the world first", "info");
		return;
	end

	local base = (selected and LE.Values("Light", selected)) or (Num(state.defaultId) > 0 and LE.Values("Light", state.defaultId));
	local values = LE.Copy(base or {});
	local falloffStart, falloffEnd = 40, 80;

	if ( base and not IsDefaultValues(base) ) then
		local _, _, _, baseStart, baseEnd = World(base);

		if ( baseEnd > 0 ) then
			falloffStart, falloffEnd = baseStart, baseEnd;
		end
	end

	local rx, ry, rz, rs, re = LightEditor_WorldToDbc(x, y, z, falloffStart, falloffEnd);
	values.m_continentID = Num(state.mapId);
	values[LE.COORDS[1]] = rx;
	values[LE.COORDS[2]] = ry;
	values[LE.COORDS[3]] = rz;
	values.m_gameFalloffStart = rs;
	values.m_gameFalloffEnd = re;

	for s = 0, LE.SLOTS - 1 do
		values[LE.SlotColumn(s)] = Num(values[LE.SlotColumn(s)]);
	end

	local source = Num(values[LE.SlotColumn(0)]);
	values[LE.SlotColumn(0)] = LE.CopyParams(source);

	local id = LE.MaxId("Light") + 1;
	LE.CreateDoc("Light", id, values);
	slot = 0;
	GameEditor_Notify(format("Light %d made at your position with params %d copied from %d, Save sends it", id, values[LE.SlotColumn(0)], source), "ok");
	RefreshList(true);
	Select(id);
	tabs:Select(TAB_LIGHT);
end

local function Reload()
	skyboxItems = nil;
	RefreshList(true);
	RefreshPages();
end

local function SaveAll()
	LE.HideColorPicker();
	LE.SaveAll();
end

local function RevertAll()
	LE.HideColorPicker();
	local count = LE.RevertAll();
	local light = selected and LightEditor_GetLight(selected);

	if ( selected and (not light or light.map ~= state.mapId) ) then
		Select(nil);
	end

	GameEditor_Notify(count > 0 and format("Reverted %d row%s, unsaved new lights are hidden until you log out", count, count == 1 and "" or "s") or "Nothing to revert", "info");
end

-- Time -----------------------------------------------------------------------------------------------------------

local UpdateTime;

-- Freezes the world at that time of day, half minutes from midnight.
local function SetPreviewTime(time)
	time = mod(mod(floor((tonumber(time) or 0) + 0.5), LE.DAY) + LE.DAY, LE.DAY);
	timeOverride = true;
	frozenTime = time;
	LightEditor_SetTime(time);
	UpdateTime();
end

local function UseServerTime()
	timeOverride = false;
	frozenTime = nil;
	LightEditor_ClearTime();
	UpdateTime();
end

local function SetFrozen(frozen)
	if ( frozen ) then
		SetPreviewTime(PreviewTime());
	else
		UseServerTime();
	end
end

local function ToggleFrozen()
	SetFrozen(not timeOverride);
end

local function StepTime(delta)
	return function()
		SetPreviewTime(PreviewTime() + delta);
	end;
end

local function TimePreset(time)
	return function()
		SetPreviewTime(time);
	end;
end

UpdateTime = function()
	if ( not timeSlider ) then
		return;
	end

	local time = PreviewTime();
	NoggitSlider_SetValueQuiet(timeSlider, time);
	NoggitCheckBox_SetChecked(serverTime, timeOverride);

	if ( not timeEdit:HasFocus() ) then
		timeEdit:SetText(LE.FormatTime(time));
	end

	timeText:SetText(timeOverride and "|cffffcc00Frozen|r" or "Time");

	for _, page in pairs(pages) do
		if ( page.UpdateTime and page:IsShown() ) then
			page:UpdateTime();
		end
	end
end

-- Build --------------------------------------------------------------------------------------------------------

local function Build(parent)
	frame = CreateFrame("Frame", "GameEditorLightPanel", parent);

	header = Common.Label(frame, "", nil, NOGGIT_COLOR_DISABLED);
	header:SetPoint("TOPLEFT", frame, "TOPLEFT", 6, -6);
	header:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -6, -6);

	list = NoggitListBox_Create(frame, 20, function(row)
		row.idText = NoggitUI_FontString(row, "");
		row.idText:SetPoint("LEFT", row, "LEFT", 6, 0);
		row.idText:SetWidth(60);
		row.info = NoggitUI_FontString(row, "");
		row.info:SetPoint("LEFT", row, "LEFT", 70, 0);
		row.info:SetWidth(240);
		NoggitText_SetColor(row.info, NOGGIT_COLOR_DISABLED);
		row.state = NoggitUI_FontString(row, "", nil, "RIGHT");
		row.state:SetPoint("RIGHT", row, "RIGHT", -4, 0);
		row.state:SetWidth(80);
	end, function(row, item)
		local doc = LE.FindDoc("Light", item.id);
		row.idText:SetText(tostring(item.id)..((doc and LE.IsDirty(doc)) and " *" or ""));

		if ( item.isDefault ) then
			row.info:SetText("map default");
		else
			row.info:SetText(format("%.0f yd away, falloff %.0f to %.0f", item.distance, item.falloffStart, item.falloffEnd));
		end

		if ( item.weight > 0 ) then
			row.state:SetText(format("|cff60ff60in %d%%|r", floor(item.weight * 100 + 0.5)));
		elseif ( item.isDefault and item.id == Num(state.defaultId) ) then
			row.state:SetText("|cff60ff60base|r");
		else
			row.state:SetText("");
		end
	end);
	list:SetPoint("TOPLEFT", frame, "TOPLEFT", 6, -24);
	list:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -6, -24);
	list:SetHeight(164);
	list.onSelect = function(item)
		Select(item.id);
	end;
	list.onDoubleClick = function()
		GoToLight();
	end;

	local buttons = CreateFrame("Frame", NoggitUI_NewName("GameEditorLightButtons"), frame);
	buttons:SetHeight(22);
	buttons:SetPoint("TOPLEFT", list, "BOTTOMLEFT", 0, -4);
	buttons:SetPoint("TOPRIGHT", list, "BOTTOMRIGHT", 0, -4);

	local goTo = Common.Button(buttons, "Go to", 60, GoToLight, "Move the free camera to the light, or teleport there with .go xyz (double click a row does the same)");
	goTo:SetPoint("LEFT", buttons, "LEFT", 0, 0);
	local moveHere = Common.Button(buttons, "Move here", 80, MoveHere, "Put the light at the free camera, or at your character when the free camera is off");
	moveHere:SetPoint("LEFT", goTo, "RIGHT", 4, 0);
	local new = Common.Button(buttons, "New light", 80, NewLight, "Copy the selected light to your position with its own copy of the clear params and bands");
	new:SetPoint("LEFT", moveHere, "RIGHT", 4, 0);
	local reload = Common.Button(buttons, "Refresh", 70, Reload, "Sort the list by distance again");
	reload:SetPoint("RIGHT", buttons, "RIGHT", 0, 0);

	local timeRow = CreateFrame("Frame", NoggitUI_NewName("GameEditorLightTime"), frame);
	timeRow:SetHeight(22);
	timeRow:SetPoint("TOPLEFT", buttons, "BOTTOMLEFT", 0, -6);
	timeRow:SetPoint("TOPRIGHT", buttons, "BOTTOMRIGHT", 0, -6);

	timeText = Common.Label(timeRow, "Time");
	timeText:SetPoint("LEFT", timeRow, "LEFT", 0, 0);
	timeText:SetWidth(42);

	timeEdit = NoggitLineEdit_Create(timeRow, 62);
	timeEdit:SetHeight(20);
	timeEdit:SetPoint("LEFT", timeText, "RIGHT", 2, 0);
	timeEdit:SetScript("OnEnterPressed", function(self)
		self:ClearFocus();
		local time = LE.ParseTime(self:GetText());

		if ( time ) then
			SetPreviewTime(time);
		else
			GameEditor_Notify("Type the time as HH:MM, for example 06:30", "info");
			UpdateTime();
		end
	end);
	timeEdit:SetScript("OnEscapePressed", function(self)
		self:ClearFocus();
		UpdateTime();
	end);
	Common.Tooltip(timeEdit, "Type a time of day as HH:MM and press Enter to freeze the world at it");

	serverTime = NoggitCheckBox_Create(timeRow, "Freeze", timeOverride, function(_, checked)
		SetFrozen(checked);
	end);
	serverTime:SetPoint("RIGHT", timeRow, "RIGHT", 0, 0);
	Common.Tooltip(serverTime, "Hold the time of day still so the lights do not change while you edit (Ctrl+T), unchecked follows the server time");

	local later = Common.Button(timeRow, "+", 22, function() SetPreviewTime(PreviewTime() + (IsShiftKeyDown() and 120 or 30)); end, "Freeze 15 minutes later, Shift+click for one hour");
	later:SetHeight(20);
	later:SetPoint("RIGHT", serverTime, "LEFT", -6, 0);

	local earlier = Common.Button(timeRow, "-", 22, function() SetPreviewTime(PreviewTime() - (IsShiftKeyDown() and 120 or 30)); end, "Freeze 15 minutes earlier, Shift+click for one hour");
	earlier:SetHeight(20);
	earlier:SetPoint("RIGHT", later, "LEFT", -2, 0);

	timeSlider = NoggitSlider_Create(timeRow, 200, 0, LE.DAY - 1, 1, function(_, value)
		SetPreviewTime(value);
	end);
	timeSlider:SetPoint("LEFT", timeEdit, "RIGHT", 6, 0);
	timeSlider:SetPoint("RIGHT", earlier, "LEFT", -6, 0);

	local bar = CreateFrame("Frame", NoggitUI_NewName("GameEditorLightBar"), frame);
	bar:SetHeight(24);
	bar:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 6, 6);
	bar:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -6, 6);

	saveButton = Common.Button(bar, "Save", 80, SaveAll, "Send every changed Light, LightParams, band and skybox row to the server (Ctrl+S)");
	saveButton:SetPoint("RIGHT", bar, "RIGHT", 0, 0);
	revertButton = Common.Button(bar, "Revert", 70, RevertAll, "Throw away every unsaved change and restore the live rows");
	revertButton:SetPoint("RIGHT", saveButton, "LEFT", -4, 0);
	statusText = Common.Label(bar, "", nil, NOGGIT_COLOR_DISABLED);
	statusText:SetPoint("LEFT", bar, "LEFT", 2, 0);
	statusText:SetPoint("RIGHT", revertButton, "LEFT", -4, 0);

	local container = CreateFrame("Frame", NoggitUI_NewName("GameEditorLightPages"), frame);
	container:SetPoint("TOPLEFT", timeRow, "BOTTOMLEFT", 0, -30);
	container:SetPoint("BOTTOMRIGHT", bar, "TOPRIGHT", 0, 6);

	tabs = Common.CreateTabs(frame, TAB_NAMES, ShowPage);
	tabs.tabs[1]:SetPoint("TOPLEFT", timeRow, "BOTTOMLEFT", 0, -6);

	pages[TAB_LIGHT] = BuildLightPage(container);
	pages[TAB_PARAMS] = BuildParamsPage(container);
	pages[TAB_COLORS] = BandPage(container, true);
	pages[TAB_FOG] = BandPage(container, false);
	pages[TAB_SKYBOX] = BuildSkyboxPage(container);

	frame:SetScript("OnShow", function()
		RefreshList(true);
		ShowPage(tabs.current);
	end);

	if ( LightEditor_GetState ) then
		state = LightEditor_GetState() or {};
	end

	RefreshList(true);
	ShowPage(tabs.current);
	return frame;
end

-- Workspace ---------------------------------------------------------------------------------------------------

local function OnUpdate(elapsed)
	updateElapsed = updateElapsed + elapsed;

	if ( updateElapsed < UPDATE_TIME or not LightEditor_GetState ) then
		return;
	end

	updateElapsed = 0;
	state = LightEditor_GetState() or {};

	if ( state.mapId ~= lastMap ) then
		lastMap = state.mapId;

		if ( selected ) then
			local light = LightEditor_GetLight(selected);

			if ( not light or light.map ~= state.mapId ) then
				Select(nil);
			end
		end

		RefreshList(true);
	elseif ( list ) then
		RefreshList(false);
	end

	if ( timeOverride and not state.frozen ) then
		timeOverride = false;
		frozenTime = nil;
	end

	RefreshHeader();
	UpdateTime();
end

local separator = { separator = true };

local function BuildMenus()
	return {
		{
			title = "Light",
			{ text = "New light", shortcut = "Ctrl+N", onClick = NewLight },
			{ text = "Go to the light", onClick = GoToLight },
			{ text = "Move the light here", onClick = MoveHere },
			separator,
			{ text = "Save all changes", shortcut = "Ctrl+S", onClick = SaveAll },
			{ text = "Revert all changes", onClick = RevertAll },
			{ text = "Refresh the list", shortcut = "F5", onClick = Reload },
		},
		{
			title = "Time",
			{ text = "Freeze time", shortcut = "Ctrl+T", checked = function() return timeOverride; end, onClick = ToggleFrozen },
			{ text = "Use server time", onClick = UseServerTime },
			separator,
			{ text = "Midnight (00:00)", onClick = TimePreset(0) },
			{ text = "Dawn (06:00)", onClick = TimePreset(720) },
			{ text = "Noon (12:00)", onClick = TimePreset(1440) },
			{ text = "Dusk (18:00)", onClick = TimePreset(2160) },
			separator,
			{ text = "15 minutes later", onClick = StepTime(30) },
			{ text = "15 minutes earlier", onClick = StepTime(-30) },
		},
	};
end

local KEYS = {
	{ "Ctrl+S", "Save every changed light row" },
	{ "Ctrl+N", "New light at the camera or your character" },
	{ "F5", "Sort the light list by distance again" },
	{ "Double click", "Go to the light in the list" },
	{ "Ctrl+T", "Freeze or unfreeze the time of day" },
};

local HOTKEYS = {
	["CTRL-T"] = ToggleFrozen,
	["CTRL-S"] = SaveAll,
	["CTRL-N"] = NewLight,
	["F5"] = Reload,
};

local function BuildStatus()
	local cells = {};
	local doc = LightDoc();
	table.insert(cells, doc and format("Light %d, slot %d %s", doc.id, slot, LE.SLOT_NAMES[slot]) or "No light selected");
	table.insert(cells, ActiveText());
	table.insert(cells, "Time "..LE.FormatTime(PreviewTime())..(timeOverride and " (frozen)" or ""));

	local count = LE.DirtyCount();

	if ( count > 0 ) then
		table.insert(cells, format("|cffffcc00%d unsaved row%s|r", count, count == 1 and "" or "s"));
	end

	return cells;
end

GameEditor_RegisterWorkspace({
	key = "light",
	title = "Lights",
	category = "Visuals",
	order = 11,
	dockWidth = 470,
	tools = {
		{ key = "select", name = "Lights", icon = 0xF8A3, build = Build },
	},
	menus = BuildMenus,
	keys = KEYS,
	onActivate = function()
		if ( LightEditor_GetState ) then
			state = LightEditor_GetState() or {};
			timeOverride = state.frozen and true or false;
			frozenTime = timeOverride and Num(state.time) or nil;
			UpdateTime();
		end

		UpdateDisplay();
		ApplyPreviewSlot();
		RefreshList(true);
		RefreshPages();
	end,
	onDeactivate = function()
		LE.HideColorPicker();

		if ( LightEditor_SetDisplay ) then
			LightEditor_SetDisplay(false);
		end

		if ( LightEditor_SetPreviewSlot ) then
			LightEditor_SetPreviewSlot(nil);
		end
	end,
	onUpdate = OnUpdate,
	onHotkey = function(key)
		local handler = HOTKEYS[key];

		if ( handler ) then
			return handler() ~= false;
		end

		return false;
	end,
	status = BuildStatus,
});
