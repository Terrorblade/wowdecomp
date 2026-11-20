local PALETTE_COLUMNS = 5;
local PALETTE_ROWS = 6;
local PALETTE_THUMB = 64;
local PALETTE_SPACING = 4;
local PALETTE_MARGIN = 8;

-- Sizes a game editor window so its content area is exactly width x height.
local function CreateMapWindow(name, title, width, height)
	local window = GameEditor_CreateWindow(name, title, width, height);
	local content = window.content;
	window:SetWidth(width + window.chromeWidth);
	window:SetHeight(height + window.chromeHeight);
	GameEditor_SetWindowWorkspace(window, "map");
	return window, content;
end

local function SetContentHeight(window, height)
	window:SetHeight(height + window.chromeHeight);
end

local tilesets;
local filtered = {};
local palettePage = 1;
local paletteButtons = {};
local paletteWindow;
local paletteFrame;

local function LoadTilesets()
	if ( tilesets ) then
		return;
	end

	tilesets = {};

	for i = 1, EditorGetTilesetCount() do
		tilesets[i] = EditorGetTileset(i);
	end
end

local function ApplyFilter(text)
	LoadTilesets();
	filtered = {};
	text = string.lower(text or "");

	for _, name in ipairs(tilesets) do
		if ( text == "" or string.find(string.lower(name), text, 1, true) ) then
			table.insert(filtered, name);
		end
	end

	palettePage = 1;
end

local function RefreshPalette()
	if ( not paletteFrame ) then
		return;
	end

	local perPage = PALETTE_COLUMNS * PALETTE_ROWS;
	local pages = max(1, math.ceil(#filtered / perPage));
	palettePage = min(max(palettePage, 1), pages);
	local current = EditorGetTexture();

	for i, button in ipairs(paletteButtons) do
		local name = filtered[(palettePage - 1) * perPage + i];
		button.texture = name;

		if ( name ) then
			button.image:SetTexture(name);
			button:Show();
		else
			button:Hide();
		end

		button.selected = name ~= nil and current ~= nil and string.lower(name) == string.lower(current);
		NoggitTextureButton_Refresh(button);
	end

	paletteFrame.pageText:SetText(format("Page %d / %d   (%d textures)", palettePage, pages, #filtered));
end

function MapEditor_CreateTexturePalette()
	if ( paletteWindow ) then
		return paletteWindow;
	end

	local width = PALETTE_MARGIN * 2 + PALETTE_COLUMNS * (PALETTE_THUMB + PALETTE_SPACING) - PALETTE_SPACING;
	local window, frame = CreateMapWindow("EditorTexturePalette", "Texture Palette", width, 500);
	paletteWindow = window;
	paletteFrame = frame;

	local filterLabel = NoggitUI_FontString(frame, "Filter:");
	filterLabel:SetPoint("TOPLEFT", frame, "TOPLEFT", PALETTE_MARGIN, -PALETTE_MARGIN - 4);

	local filter = NoggitLineEdit_Create(frame, width - PALETTE_MARGIN * 2 - 44);
	filter:SetPoint("TOPLEFT", frame, "TOPLEFT", PALETTE_MARGIN + 44, -PALETTE_MARGIN);
	filter:SetScript("OnTextChanged", function(self)
		ApplyFilter(self:GetText());
		RefreshPalette();
	end);

	local top = -PALETTE_MARGIN - 22 - 8;

	for row = 0, PALETTE_ROWS - 1 do
		for column = 0, PALETTE_COLUMNS - 1 do
			local button = NoggitTextureButton_Create(frame, PALETTE_THUMB);
			button:SetPoint("TOPLEFT", frame, "TOPLEFT", PALETTE_MARGIN + column * (PALETTE_THUMB + PALETTE_SPACING), top - row * (PALETTE_THUMB + PALETTE_SPACING));
			button:SetScript("OnClick", function(self)
				if ( self.texture ) then
					EditorSetTexture(self.texture);
					RefreshPalette();
				end
			end);
			button:HookScript("OnEnter", function(self)
				frame.hoverText:SetText(self.texture and (self.texture:gsub("^[Tt][Ii][Ll][Ee][Ss][Ee][Tt]\\", "")) or "");
			end);
			table.insert(paletteButtons, button);
		end
	end

	local bottom = top - PALETTE_ROWS * (PALETTE_THUMB + PALETTE_SPACING) - 4;

	frame.hoverText = NoggitUI_FontString(frame, "", "NoggitFontDisabled");
	frame.hoverText:SetWidth(width - PALETTE_MARGIN * 2);
	frame.hoverText:SetPoint("TOPLEFT", frame, "TOPLEFT", PALETTE_MARGIN, bottom);
	bottom = bottom - 20;

	local previous = NoggitPushButton_Create(frame, "<", 32, function()
		palettePage = palettePage - 1;
		RefreshPalette();
	end);
	previous:SetPoint("TOPLEFT", frame, "TOPLEFT", PALETTE_MARGIN, bottom);

	local nextButton = NoggitPushButton_Create(frame, ">", 32, function()
		palettePage = palettePage + 1;
		RefreshPalette();
	end);
	nextButton:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -PALETTE_MARGIN, bottom);

	frame.pageText = NoggitUI_FontString(frame, "", "NoggitFont", "CENTER");
	frame.pageText:SetPoint("LEFT", previous, "RIGHT", 4, 0);
	frame.pageText:SetPoint("RIGHT", nextButton, "LEFT", -4, 0);

	frame:EnableMouseWheel(true);
	frame:SetScript("OnMouseWheel", function(self, delta)
		palettePage = palettePage - delta;
		RefreshPalette();
	end);

	SetContentHeight(window, -bottom + 23 + PALETTE_MARGIN);
	frame.filter = filter;
	return window;
end

function MapEditor_RefreshTexturePalette()
	if ( #filtered == 0 and not tilesets ) then
		ApplyFilter("");
	end

	RefreshPalette();
end

local rotationWindow;
local rotationFields = {};

function MapEditor_CreateRotationEditor()
	if ( rotationWindow ) then
		return rotationWindow;
	end

	local window, frame = CreateMapWindow("EditorRotationEditor", "Pos/Rotation Editor", 230, 400);
	rotationWindow = window;

	local y = -10;

	local function Section(text)
		local label = NoggitUI_FontString(frame, text, "NoggitFontBold");
		label:SetPoint("TOPLEFT", frame, "TOPLEFT", 10, y);
		y = y - 20;
	end

	local function Field(label, key, argument, decimals)
		if ( label ~= "" ) then
			local text = NoggitUI_FontString(frame, label);
			text:SetPoint("TOPLEFT", frame, "TOPLEFT", 14, y - 4);
		end

		local spin = NoggitSpinBox_Create(frame, 180, -100000, 100000, key == "scale" and 0.1 or 1, decimals, function(_, value)
			local args = { nil, nil, nil, nil, nil, nil, nil };
			args[argument] = value;
			EditorSetObjectTransform(args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
		end);
		spin:SetPoint("TOPLEFT", frame, "TOPLEFT", 36, y);
		rotationFields[key] = { spin = spin, argument = argument };
		y = y - 26;
	end

	Section("Tilt");
	Field("X", "tiltX", 4, 2);
	Field("Z", "tiltZ", 6, 2);
	Section("Rotation");
	Field("", "rotation", 5, 2);
	Section("Position");
	Field("X", "positionX", 1, 3);
	Field("Z", "positionZ", 3, 3);
	Field("H", "positionH", 2, 3);
	Section("Scale");
	Field("", "scale", 7, 3);

	local warning = NoggitUI_FontString(frame, "Multi selection warning:\n- rotation and scale only\n  change when pressing enter\n- scaling is multiplicative", "NoggitFontDisabled");
	warning:SetPoint("TOPLEFT", frame, "TOPLEFT", 10, y - 4);
	warning:SetWidth(210);
	y = y - 64;

	SetContentHeight(window, -y + 6);
	frame:SetScript("OnUpdate", function()
		local values = { EditorGetObjectTransform() };
		local has = values[1] ~= nil;

		for _, field in pairs(rotationFields) do
			local spin = field.spin;
			spin:SetEnabledState(has);

			if ( has and not _G[spin:GetName().."Edit"]:HasFocus() and math.abs(spin.value - values[field.argument]) > 0.0005 ) then
				NoggitSpinBox_SetValue(spin, values[field.argument], true);
			end
		end
	end);

	return window;
end

local assetModels;
local assetFilter = "";
local assetShowWmo = true;
local assetShowM2 = true;
local assetList;
local assetWindow;
local assetFrame;

local function LoadAssetModels()
	if ( assetModels ) then
		return;
	end

	assetModels = {};

	for i = 1, EditorGetModelCount() do
		assetModels[i] = EditorGetModel(i);
	end
end

local function RefreshAssetList()
	LoadAssetModels();
	local items = {};
	local filter = string.lower(assetFilter);

	for _, name in ipairs(assetModels) do
		local lower = string.lower(name);
		local isWmo = string.sub(lower, -4) == ".wmo";

		if ( ((isWmo and assetShowWmo) or (not isWmo and assetShowM2)) and (filter == "" or string.find(lower, filter, 1, true)) ) then
			table.insert(items, name);
		end
	end

	NoggitListBox_SetItems(assetList, items);
	assetFrame.countText:SetText(format("%d files", #items));
end

function MapEditor_CreateAssetBrowser()
	if ( assetWindow ) then
		return assetWindow;
	end

	local width = 440;
	local window, frame = CreateMapWindow("EditorAssetBrowser", "Asset browser", width, 560);
	assetWindow = window;
	assetFrame = frame;

	local search = NoggitLineEdit_Create(frame, width - 16 - 80);
	search:SetPoint("TOPLEFT", frame, "TOPLEFT", 8, -8);
	search:SetScript("OnEnterPressed", function(self)
		assetFilter = self:GetText() or "";
		RefreshAssetList();
		self:ClearFocus();
	end);

	local searchButton = NoggitPushButton_Create(frame, "Search", 74, function()
		assetFilter = search:GetText() or "";
		RefreshAssetList();
	end);
	searchButton:SetPoint("LEFT", search, "RIGHT", 6, 0);

	assetList = NoggitListBox_Create(frame, 20, function(row)
		row.text = NoggitUI_FontString(row, "");
		row.text:SetPoint("LEFT", row, "LEFT", 6, 0);
		row.text:SetPoint("RIGHT", row, "RIGHT", -4, 0);
		row.text:SetHeight(14);
	end, function(row, name)
		row.text:SetText(name);
	end);
	assetList:SetPoint("TOPLEFT", search, "BOTTOMLEFT", 0, -6);
	assetList:SetPoint("TOPRIGHT", searchButton, "BOTTOMRIGHT", 0, -6);
	assetList:SetPoint("BOTTOM", frame, "BOTTOM", 0, 84);
	assetList.onSelect = function(name)
		if ( EditorSetClipboardModel(name) ) then
			frame.statusText:SetText("Copied to clipboard. Ctrl+V pastes at the cursor.");
		else
			frame.statusText:SetText("Could not read "..name);
		end
	end;

	local modeLabel = NoggitUI_FontString(frame, "Browse Mode");
	modeLabel:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 8, 62);
	local mode = NoggitComboBox_Create(frame, 160, { { text = "Listfile" } }, 1);
	mode:SetPoint("LEFT", modeLabel, "RIGHT", 8, 0);

	local wmo = NoggitCheckBox_Create(frame, "WMOs (*.wmo)", true, function(_, checked)
		assetShowWmo = checked;
		RefreshAssetList();
	end);
	wmo:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 8, 34);

	local m2 = NoggitCheckBox_Create(frame, "Models (*.m2)", true, function(_, checked)
		assetShowM2 = checked;
		RefreshAssetList();
	end);
	m2:SetPoint("LEFT", wmo, "RIGHT", 16, 0);

	frame.countText = NoggitUI_FontString(frame, "", "NoggitFontDisabled", "RIGHT");
	frame.countText:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -8, 38);

	frame.statusText = NoggitUI_FontString(frame, "Select a model, then Ctrl+V in the Object Editor pastes it.", "NoggitFontDisabled");
	frame.statusText:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 8, 10);
	frame.statusText:SetWidth(width - 16);

	return window;
end

function MapEditor_RefreshAssetBrowser()
	if ( not assetModels ) then
		RefreshAssetList();
	end
end

function MapEditor_ShowTexturePalette()
	local window = MapEditor_CreateTexturePalette();
	MapEditor_RefreshTexturePalette();
	GameEditor_ShowWindow(window);
end

function MapEditor_ShowAssetBrowser()
	local window = MapEditor_CreateAssetBrowser();
	MapEditor_RefreshAssetBrowser();
	GameEditor_ShowWindow(window);
end

function MapEditor_ShowRotationEditor()
	GameEditor_ShowWindow(MapEditor_CreateRotationEditor());
end
