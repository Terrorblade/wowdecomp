local PANEL_WIDTH = 265;
local PANEL_MARGIN = 9;
local GROUP_PADDING = 12;
local ROW_HEIGHT = 22;

local function NewPanel(parent)
	local panel = CreateFrame("Frame", NoggitUI_NewName("NoggitToolPanel"), parent);
	panel:SetPoint("TOPLEFT", parent, "TOPLEFT", 0, 0);
	panel:SetWidth(PANEL_WIDTH);
	panel:SetHeight(10);
	panel:Hide();
	panel.innerWidth = PANEL_WIDTH - PANEL_MARGIN * 2;
	panel.items = {};
	panel.sync = {};
	return panel;
end

local function RelayoutPanel(panel)
	local y = -PANEL_MARGIN;

	for _, item in ipairs(panel.items) do
		if ( item.widget:IsShown() ) then
			item.widget:ClearAllPoints();
			item.widget:SetPoint("TOPLEFT", panel, "TOPLEFT", PANEL_MARGIN + (item.x or 0), y);
			y = y - item.height - (item.spacing or 6);
		end
	end

	panel:SetHeight(-y);
end

local function PlaceInPanel(panel, widget, height, spacing, x)
	table.insert(panel.items, { widget = widget, height = height, spacing = spacing, x = x });
	RelayoutPanel(panel);
end

local function NewGroup(parent, width, title, checkable, checked, onToggle)
	local group = NoggitGroupBox_Create(parent, width, title, checkable, checked, onToggle);
	group.cursor = group.contentTop - 6;
	group.innerWidth = width - GROUP_PADDING * 2;
	return group;
end

local function PlaceInGroup(group, widget, height, x)
	widget:SetPoint("TOPLEFT", group, "TOPLEFT", GROUP_PADDING + (x or 0), group.cursor);
	NoggitGroupBox_Add(group, widget);
	return height;
end

local function AdvanceGroup(group, height, spacing)
	group.cursor = group.cursor - height - (spacing or 6);
end

local function FinishGroup(group)
	group:SetHeight(-group.cursor + 4);
	NoggitGroupBox_Finish(group);
	return -group.cursor + 4;
end

local function AddGroup(panel, group)
	PlaceInPanel(panel, group, FinishGroup(group));
end

local function RadioGrid(group, labels, current, onSelect, disabled)
	local radios = {};
	local column = group.innerWidth / 2;

	for i, label in ipairs(labels) do
		local radio = NoggitCheckBox_Create(group, label, i == current, function()
			onSelect(i);
		end, radios);

		if ( disabled and disabled[i] ) then
			radio:Disable();
		end

		PlaceInGroup(group, radio, ROW_HEIGHT, mod(i - 1, 2) * column);

		if ( mod(i, 2) == 0 or i == #labels ) then
			AdvanceGroup(group, ROW_HEIGHT, 2);
		end
	end

	return radios;
end

local function KeybindRow(panel, modifierGlyph, text)
	local row = CreateFrame("Frame", NoggitUI_NewName("NoggitKeybindRow"), panel);
	row:SetWidth(panel.innerWidth);
	row:SetHeight(25);

	local modifier = NoggitUI_FontString(row, NoggitUI_Utf8(modifierGlyph), "NoggitIconFontSmall", "CENTER");
	modifier:SetPoint("LEFT", row, "LEFT", 0, 0);

	local plus = NoggitUI_FontString(row, "+");
	plus:SetPoint("LEFT", modifier, "RIGHT", 2, 0);

	local mouse = NoggitUI_FontString(row, NoggitUI_Utf8(0xF864), "NoggitIconFontSmall", "CENTER");
	mouse:SetPoint("LEFT", plus, "RIGHT", 2, 0);

	local label = NoggitUI_FontString(row, text);
	label:SetPoint("LEFT", mouse, "RIGHT", 4, 0);

	PlaceInPanel(panel, row, 25, 0);
end

local function Expander(panel, title, buildContent, expanded, enabled)
	local header = CreateFrame("Button", NoggitUI_NewName("NoggitExpander"), panel);
	header:SetWidth(panel.innerWidth);
	header:SetHeight(ROW_HEIGHT);

	local arrow = header:CreateTexture(nil, "ARTWORK");
	arrow:SetPoint("LEFT", header, "LEFT", 4, 0);

	local label = NoggitUI_FontString(header, title, enabled and "NoggitFont" or "NoggitFontDisabled");
	label:SetPoint("LEFT", header, "LEFT", 20, 0);

	local line = NoggitUI_SolidTexture(header, "BORDER", NOGGIT_COLOR_MID);
	line:SetHeight(1);
	line:SetPoint("BOTTOMLEFT", header, "BOTTOMLEFT", 0, 0);
	line:SetPoint("BOTTOMRIGHT", header, "BOTTOMRIGHT", 0, 0);

	PlaceInPanel(panel, header, ROW_HEIGHT);

	local content;

	if ( buildContent ) then
		content = CreateFrame("Frame", NoggitUI_NewName("NoggitExpanderContent"), panel);
		content:SetWidth(panel.innerWidth);
		content:SetHeight(buildContent(content, panel.innerWidth));
		PlaceInPanel(panel, content, content:GetHeight());
	end

	local function Refresh()
		if ( header.expanded ) then
			arrow:SetTexture("Interface\\GameEditor\\Textures\\icon_branch_open.tga");
			arrow:SetTexCoord(0, 0.75, 0, 1);
			arrow:SetWidth(12);
			arrow:SetHeight(8);
		else
			arrow:SetTexture("Interface\\GameEditor\\Textures\\icon_branch_closed.tga");
			arrow:SetTexCoord(0, 1, 0, 0.75);
			arrow:SetWidth(8);
			arrow:SetHeight(12);
		end

		if ( content ) then
			if ( header.expanded ) then
				content:Show();
			else
				content:Hide();
			end
		end

		RelayoutPanel(panel);
	end

	header.expanded = expanded and content ~= nil;

	if ( enabled and content ) then
		header:SetScript("OnClick", function()
			header.expanded = not header.expanded;
			Refresh();
		end);
	end

	Refresh();
	return header, content;
end

local function SnapCheckBoxes(group)
	local m2 = NoggitCheckBox_Create(group, "Snap M2 objects", true);
	m2:Disable();
	m2.tooltip = "Object snapping is not available yet";
	PlaceInGroup(group, m2, 20);
	AdvanceGroup(group, 20, 2);

	local wmo = NoggitCheckBox_Create(group, "Snap WMO objects", true);
	wmo:Disable();
	wmo.tooltip = "Object snapping is not available yet";
	PlaceInGroup(group, wmo, 20);
	AdvanceGroup(group, 20, 2);
end

local function SyncSlider(panel, slider, getter)
	table.insert(panel.sync, function()
		local value = getter();

		if ( value and math.abs(slider:GetValue() - value) > 0.0001 and not _G[slider.spin:GetName().."Edit"]:HasFocus() ) then
			slider:SetValue(value);
		end
	end);
end

local function ExtSlider(panel, group, prefix, minValue, maxValue, step, decimals, getter, setter, width)
	local slider = NoggitExtendedSlider_Create(group, width or group.innerWidth, prefix, minValue, maxValue, step, decimals, function(_, value)
		setter(value);
	end);
	slider:SetValue(getter());
	PlaceInGroup(group, slider, 46);
	AdvanceGroup(group, 46, 4);
	SyncSlider(panel, slider, getter);
	return slider;
end

local function Button(parent, text, width, onClick, enabled)
	local button = NoggitPushButton_Create(parent, text, width, onClick);

	if ( not enabled ) then
		button:Disable();
	end

	return button;
end

local function RaiseLowerValue(index)
	return select(index, EditorGetRaiseLower());
end

local function FlattenBlurValue(index)
	return select(index, EditorGetFlattenBlur());
end

local function TextureBrushValue(index)
	return select(index, EditorGetTextureBrush());
end

function NoggitTools_CreateRaiseLower(parent)
	local panel = NewPanel(parent);

	local typeGroup = NewGroup(panel, panel.innerWidth, "Type");
	RadioGrid(typeGroup, { "Flat", "Linear", "Smooth", "Polynomial", "Trigonom", "Quadratic", "Gaussian", "Vertex", "Script" }, RaiseLowerValue(1), function(index)
		EditorSetRaiseLower("type", index);
	end, { [8] = true, [9] = true });
	AddGroup(panel, typeGroup);

	local settings = NewGroup(panel, panel.innerWidth, "Settings");
	ExtSlider(panel, settings, "Radius:", 0, 1000, 1, 2, function() return RaiseLowerValue(2); end, function(value) EditorSetRaiseLower("radius", value); end);
	ExtSlider(panel, settings, "Inner Radius:", 0, 1, 0.05, 2, function() return RaiseLowerValue(3); end, function(value) EditorSetRaiseLower("innerRadius", value); end);
	ExtSlider(panel, settings, "Speed:", 0, 1000, 1, 2, function() return RaiseLowerValue(4); end, function(value) EditorSetRaiseLower("speed", value); end);
	SnapCheckBoxes(settings);
	AddGroup(panel, settings);

	Expander(panel, "Custom Brush", nil, false, false);
	return panel;
end

function NoggitTools_CreateFlattenBlur(parent)
	local panel = NewPanel(parent);

	KeybindRow(panel, 0xF86D, "Flatten Terrain");
	KeybindRow(panel, 0xF86C, "Blur Terrain");

	local typeGroup = NewGroup(panel, panel.innerWidth, "Type");
	RadioGrid(typeGroup, { "Flat", "Linear", "Smooth", "Origin" }, FlattenBlurValue(1), function(index)
		EditorSetFlattenBlur("type", index);
	end);
	AddGroup(panel, typeGroup);

	local settings = NewGroup(panel, panel.innerWidth, "Settings");
	ExtSlider(panel, settings, "Radius:", 0, 1000, 1, 2, function() return FlattenBlurValue(2); end, function(value) EditorSetFlattenBlur("radius", value); end);
	ExtSlider(panel, settings, "Speed:", 0, 10, 0.1, 2, function() return FlattenBlurValue(3); end, function(value) EditorSetFlattenBlur("speed", value); end);
	SnapCheckBoxes(settings);
	AddGroup(panel, settings);

	local mode = NewGroup(panel, panel.innerWidth, "Flatten/Blur");
	local raise = NoggitCheckBox_Create(mode, "Raise", FlattenBlurValue(4), function(_, checked) EditorSetFlattenBlur("raise", checked); end);
	raise.tooltip = "Raise the terrain when using the tool";
	PlaceInGroup(mode, raise, 20);
	local lower = NoggitCheckBox_Create(mode, "Lower", FlattenBlurValue(5), function(_, checked) EditorSetFlattenBlur("lower", checked); end);
	lower.tooltip = "Lower the terrain when using the tool";
	PlaceInGroup(mode, lower, 20, mode.innerWidth / 2);
	AdvanceGroup(mode, 20, 2);
	AddGroup(panel, mode);

	local flattenOnly = NewGroup(panel, panel.innerWidth, "Flatten only");
	local innerWidth = flattenOnly.innerWidth;

	local angled = NewGroup(flattenOnly, innerWidth, "Angled mode", true, FlattenBlurValue(6), function(_, checked)
		EditorSetFlattenBlur("angled", checked);
	end);

	local orientationLabel = NoggitUI_FontString(angled, "Orientation : 0");
	PlaceInGroup(angled, orientationLabel, 16);
	AdvanceGroup(angled, 16, 2);
	local orientation = NoggitSlider_Create(angled, angled.innerWidth, 0, 360, 1, function(_, value)
		EditorSetFlattenBlur("orientation", value);
		orientationLabel:SetText(format("Orientation : %d", value));
	end);
	PlaceInGroup(angled, orientation, 16);
	AdvanceGroup(angled, 16, 8);

	local angleLabel = NoggitUI_FontString(angled, "Angle : 0");
	PlaceInGroup(angled, angleLabel, 16);
	AdvanceGroup(angled, 16, 2);
	local angle = NoggitSlider_Create(angled, angled.innerWidth, 0, 89, 1, function(_, value)
		EditorSetFlattenBlur("angle", value);
		angleLabel:SetText(format("Angle : %d", value));
	end);
	PlaceInGroup(angled, angle, 16);
	AdvanceGroup(angled, 16, 6);
	local angledHeight = FinishGroup(angled);
	PlaceInGroup(flattenOnly, angled, angledHeight);
	AdvanceGroup(flattenOnly, angledHeight, 8);

	local lock = NewGroup(flattenOnly, innerWidth, "Lock mode", true, FlattenBlurValue(9), function(_, checked)
		EditorSetFlattenBlur("lock", checked);
	end);

	local function LockRow(label, minValue, maxValue, getter, setter)
		local text = NoggitUI_FontString(lock, label);
		text:SetPoint("TOPLEFT", lock, "TOPLEFT", GROUP_PADDING, lock.cursor - 4);
		local spin = NoggitSpinBox_Create(lock, lock.innerWidth - 24, minValue, maxValue, 1, 3, function(_, value)
			setter(value);
		end);
		NoggitSpinBox_SetValue(spin, getter(), true);
		PlaceInGroup(lock, spin, 22, 24);
		AdvanceGroup(lock, 22, 4);
		return spin;
	end

	LockRow("X:", 0, 34133, function() return 17066.666 - FlattenBlurValue(11); end, function(value) EditorSetFlattenBlur("lockY", 17066.666 - value); end);
	LockRow("Z:", 0, 34133, function() return 17066.666 - FlattenBlurValue(10); end, function(value) EditorSetFlattenBlur("lockX", 17066.666 - value); end);
	LockRow("H:", -100000, 100000, function() return FlattenBlurValue(12); end, function(value) EditorSetFlattenBlur("lockH", value); end);
	local lockHeight = FinishGroup(lock);
	PlaceInGroup(flattenOnly, lock, lockHeight);
	AdvanceGroup(flattenOnly, lockHeight, 4);
	AddGroup(panel, flattenOnly);

	return panel;
end

function NoggitTools_CreateTexturePainter(parent)
	local panel = NewPanel(parent);

	local preview = NoggitTextureButton_Create(panel, 225);
	preview:SetScript("OnClick", function() MapEditor_ShowTexturePalette(); end);
	PlaceInPanel(panel, preview, 225, 2, (panel.innerWidth - 225) / 2);

	local nameHolder = CreateFrame("Frame", NoggitUI_NewName("NoggitTextureName"), panel);
	nameHolder:SetWidth(panel.innerWidth);
	nameHolder:SetHeight(16);
	local previewName = NoggitUI_FontString(nameHolder, "No texture selected", "NoggitFontDisabled", "CENTER");
	previewName:SetWidth(panel.innerWidth);
	previewName:SetPoint("CENTER", nameHolder, "CENTER", 0, 0);
	PlaceInPanel(panel, nameHolder, 16, 8);

	table.insert(panel.sync, function()
		local texture = EditorGetTexture();

		if ( texture ~= panel.texture ) then
			panel.texture = texture;

			if ( texture ) then
				preview.image:SetTexture(texture);
				preview.image:Show();
				previewName:SetText(texture:match("([^\\]+)$") or texture);
				previewName:SetFontObject(NoggitFont);
			else
				preview.image:Hide();
				previewName:SetText("No texture selected");
				previewName:SetFontObject(NoggitFontDisabled);
			end
		end
	end);

	local tabHolder = CreateFrame("Frame", NoggitUI_NewName("NoggitTabHolder"), panel);
	tabHolder:SetWidth(panel.innerWidth);
	tabHolder:SetHeight(24);
	local tabs = NoggitTabs_Create(tabHolder, { "Paint", "Swap", "Anim" }, 1, nil, { [2] = true, [3] = true });
	tabs[1]:SetPoint("TOPLEFT", tabHolder, "TOPLEFT", 0, 0);
	PlaceInPanel(panel, tabHolder, 24, 0);

	local page = CreateFrame("Frame", NoggitUI_NewName("NoggitTabPage"), panel);
	page:SetWidth(panel.innerWidth);
	local pageBorder = NoggitUI_SolidTexture(page, "BORDER", NOGGIT_COLOR_MID);
	pageBorder:SetHeight(1);
	pageBorder:SetPoint("TOPLEFT", page, "TOPLEFT", 0, 0);
	pageBorder:SetPoint("TOPRIGHT", page, "TOPRIGHT", 0, 0);

	local leftWidth = panel.innerWidth - 60;
	local y = -8;

	local function Labeled(text, minValue, maxValue, step, getter, setter)
		local label = NoggitUI_FontString(page, text);
		label:SetPoint("TOPLEFT", page, "TOPLEFT", 4, y);
		y = y - 18;
		local slider = NoggitExtendedSlider_Create(page, leftWidth - 8, "", minValue, maxValue, step, 2, function(_, value) setter(value); end);
		slider:SetPoint("TOPLEFT", page, "TOPLEFT", 4, y);
		slider:SetValue(getter());
		y = y - 50;
		table.insert(panel.sync, function()
			local value = getter();

			if ( math.abs(slider:GetValue() - value) > 0.0001 and not _G[slider.spin:GetName().."Edit"]:HasFocus() ) then
				slider:SetValue(value);
			end
		end);
	end

	Labeled("Hardness:", 0, 1, 0.05, function() return TextureBrushValue(2); end, function(value) EditorSetTextureBrush("hardness", value); end);
	Labeled("Radius:", 0, 1000, 1, function() return TextureBrushValue(1); end, function(value) EditorSetTextureBrush("radius", value); end);
	Labeled("Pressure:", 0, 1, 0.05, function() return TextureBrushValue(3); end, function(value) EditorSetTextureBrush("pressure", value); end);

	local opacityLabel = NoggitUI_FontString(page, "Opacity:");
	opacityLabel:SetPoint("TOPLEFT", page, "TOPLEFT", leftWidth + 4, -8);

	local opacitySpin = NoggitSpinBox_Create(page, 52, 0, 255, 5, 0);
	local opacitySlider = NoggitVerticalSlider_Create(page, 150, 0, 255, 1, function(_, value)
		local opacity = 255 - value;
		EditorSetTextureBrush("opacity", opacity);
		NoggitSpinBox_SetValue(opacitySpin, opacity, true);
	end);
	opacitySlider:SetPoint("TOPLEFT", page, "TOPLEFT", leftWidth + 12, -30);
	opacitySpin:SetPoint("TOPLEFT", page, "TOPLEFT", leftWidth + 4, -186);
	opacitySpin.onChange = function(_, value)
		EditorSetTextureBrush("opacity", value);
		NoggitVerticalSlider_SetValueQuiet(opacitySlider, 255 - value);
	end;
	NoggitVerticalSlider_SetValueQuiet(opacitySlider, 255 - TextureBrushValue(4));
	NoggitSpinBox_SetValue(opacitySpin, TextureBrushValue(4), true);

	table.insert(panel.sync, function()
		local opacity = TextureBrushValue(4);

		if ( math.abs(opacitySpin.value - opacity) > 0.001 and not _G[opacitySpin:GetName().."Edit"]:HasFocus() ) then
			NoggitSpinBox_SetValue(opacitySpin, opacity, true);
			NoggitVerticalSlider_SetValueQuiet(opacitySlider, 255 - opacity);
		end
	end);

	y = math.min(y, -214) - 6;

	local spray = NewGroup(page, panel.innerWidth - 8, "Spray", true, false);
	spray:SetPoint("TOPLEFT", page, "TOPLEFT", 4, y);
	local sprayNote = NoggitUI_FontString(spray, "Spray painting is not available yet", "NoggitFontDisabled");
	PlaceInGroup(spray, sprayNote, 16);
	AdvanceGroup(spray, 16, 4);
	y = y - FinishGroup(spray) - 8;
	spray.toggle:Disable();

	local quick = Button(page, "Quick Palette", panel.innerWidth - 8, function() MapEditor_ShowTexturePalette(); end, true);
	quick:SetPoint("TOPLEFT", page, "TOPLEFT", 4, y);
	y = y - 23 - 8;

	local hint = NoggitUI_FontString(page, "Shift + LMB paints, Ctrl + LMB picks the texture under the cursor.", "NoggitFontDisabled");
	hint:SetWidth(panel.innerWidth - 8);
	hint:SetPoint("TOPLEFT", page, "TOPLEFT", 4, y);
	y = y - 32;

	page:SetHeight(-y);
	PlaceInPanel(panel, page, -y, 6);

	Expander(panel, "Custom Brush", nil, false, false);
	return panel;
end

function NoggitTools_CreateObjectEditor(parent)
	local panel = NewPanel(parent);

	local radiusGroup = NewGroup(panel, panel.innerWidth, "Selection Brush Radius");
	local radiusSlider = NoggitSlider_Create(radiusGroup, radiusGroup.innerWidth, 0, 100, 1);
	radiusSlider:Disable();
	PlaceInGroup(radiusGroup, radiusSlider, 16);
	AdvanceGroup(radiusGroup, 16, 6);
	local radiusSpin = NoggitSpinBox_Create(radiusGroup, radiusGroup.innerWidth, 0, 100, 1, 2);
	radiusSpin:SetEnabledState(false);
	PlaceInGroup(radiusGroup, radiusSpin, 22);
	AdvanceGroup(radiusGroup, 22, 4);
	AddGroup(panel, radiusGroup);

	PlaceInPanel(panel, Button(panel, "Asset browser", panel.innerWidth, function() MapEditor_ShowAssetBrowser(); end, true), 23);
	PlaceInPanel(panel, Button(panel, "Object palette", panel.innerWidth, nil, false), 23);

	local wmoGroup = NewGroup(panel, panel.innerWidth, "Selected WMO Options");
	local function InfoRow(label)
		local text = NoggitUI_FontString(wmoGroup, label, "NoggitFontDisabled");
		PlaceInGroup(wmoGroup, text, 18);
		local value = NoggitUI_FontString(wmoGroup, "-", "NoggitFontDisabled");
		PlaceInGroup(wmoGroup, value, 18, 90);
		AdvanceGroup(wmoGroup, 18, 4);
		return value;
	end
	InfoRow("Doodad Set:");
	InfoRow("Name Set:");
	InfoRow("Low Res Model:");
	AddGroup(panel, wmoGroup);

	local clipboard = NewGroup(panel, panel.innerWidth, "Clipboard");
	local clipboardText = NoggitUI_FontString(clipboard, "Empty (0 objects copied)");
	PlaceInGroup(clipboard, clipboardText, 18);
	AdvanceGroup(clipboard, 18, 4);
	AddGroup(panel, clipboard);

	local selectionHolder = CreateFrame("Frame", NoggitUI_NewName("NoggitSelectionInfo"), panel);
	selectionHolder:SetWidth(panel.innerWidth);
	selectionHolder:SetHeight(32);
	local selectionInfo = NoggitUI_FontString(selectionHolder, "", "NoggitFontDisabled");
	selectionInfo:SetWidth(panel.innerWidth);
	selectionInfo:SetHeight(32);
	selectionInfo:SetJustifyV("TOP");
	selectionInfo:SetPoint("TOPLEFT", selectionHolder, "TOPLEFT", 0, 0);
	PlaceInPanel(panel, selectionHolder, 32);

	table.insert(panel.sync, function()
		local count = EditorGetSelectionCount();
		local text;

		if ( count == 0 ) then
			text = "Nothing selected";
		elseif ( count == 1 ) then
			local kind, uid, file = EditorGetSelection(1);
			local label = kind == "wmo" and "WMO" or (kind == "wmodoodad" and "WMO M2 (all placements)" or "M2");
			text = format("%s: %d\n%s", label, uid, file:match("([^\\]+)$") or file);
		else
			text = format("%d objects selected", count);
		end

		selectionInfo:SetText(text);
	end);

	Expander(panel, "Copy options", nil, false, false);
	Expander(panel, "Paste Options", nil, false, false);

	Expander(panel, "Movement Options", function(content, width)
		local function Option(group, text, key, enabled)
			local box = NoggitCheckBox_Create(group, text, key and EditorGetObjectSetting(key) or false, function(_, checked)
				if ( key ) then
					EditorSetObjectSetting(key, checked);
				end
			end);

			if ( not enabled ) then
				box:Disable();
			end

			PlaceInGroup(group, box, 20);
			AdvanceGroup(group, 20, 2);
		end

		local single = NewGroup(content, width, "Single Selection Movement");
		Option(single, "Rotate following cursor", nil, false);
		Option(single, "Smooth follow rotation", nil, false);
		Option(single, "Random rot/tilt/scale on rotate", nil, false);
		Option(single, "Mouse move follow cursor on the ground", "followCursor", true);
		Option(single, "Mouse move snaps to objects", nil, false);
		Option(single, "Select M2s inside WMOs", "pickWmoDoodads", true);
		single:SetPoint("TOPLEFT", content, "TOPLEFT", 0, 0);
		local singleHeight = FinishGroup(single);

		local multi = NewGroup(content, width, "Multi Selection Movement");
		Option(multi, "Mouse move snap models to the ground", "snapMultiToGround", true);
		Option(multi, "Rotate around pivot point", nil, false);
		multi:SetPoint("TOPLEFT", content, "TOPLEFT", 0, -singleHeight - 6);
		local multiHeight = FinishGroup(multi);

		return singleHeight + 6 + multiHeight;
	end, true, true);

	PlaceInPanel(panel, Button(panel, "Pos/Rotation Editor", panel.innerWidth, function() MapEditor_ShowRotationEditor(); end, true), 23);
	PlaceInPanel(panel, Button(panel, "Clear Hidden Models List", panel.innerWidth, nil, false), 23);
	PlaceInPanel(panel, Button(panel, "Clear Selection Groups List", panel.innerWidth, nil, false), 23);
	Expander(panel, "Import (old, use Asset Browser)", nil, false, false);

	return panel;
end

local function ChunkToolValue(index)
	return select(index, EditorGetChunkTool());
end

local function FormSpinRow(panel, label, minValue, maxValue, step, decimals, getter, setter)
	local row = CreateFrame("Frame", NoggitUI_NewName("NoggitFormRow"), panel);
	row:SetWidth(panel.innerWidth);
	row:SetHeight(24);

	local text = NoggitUI_FontString(row, label);
	text:SetPoint("LEFT", row, "LEFT", 0, 0);

	local spin = NoggitSpinBox_Create(row, 110, minValue, maxValue, step, decimals, function(_, value)
		setter(value);
	end);
	spin:SetPoint("RIGHT", row, "RIGHT", 0, 0);
	NoggitSpinBox_SetValue(spin, getter(), true);
	PlaceInPanel(panel, row, 24, 4);

	table.insert(panel.sync, function()
		local value = getter();

		if ( value and math.abs(spin.value - value) > 0.0001 and not _G[spin:GetName().."Edit"]:HasFocus() ) then
			NoggitSpinBox_SetValue(spin, value, true);
		end
	end);

	return spin;
end

local function FormSlider(panel, minValue, maxValue, step, getter, setter)
	local slider = NoggitSlider_Create(panel, panel.innerWidth, minValue, maxValue, step, function(_, value)
		setter(value);
	end);
	NoggitSlider_SetValueQuiet(slider, getter());
	PlaceInPanel(panel, slider, 18, 8);

	table.insert(panel.sync, function()
		local value = getter();

		if ( value and math.abs(slider:GetValue() - min(max(value, minValue), maxValue)) > 0.5 ) then
			NoggitSlider_SetValueQuiet(slider, value);
		end
	end);

	return slider;
end

function NoggitTools_CreateHoleCutter(parent)
	local panel = NewPanel(parent);
	local getter = function() return ChunkToolValue(1); end;
	local setter = function(value) EditorSetChunkTool("holeRadius", value); end;
	FormSpinRow(panel, "Radius:", 0, 1000, 1, 2, getter, setter);
	FormSlider(panel, 0, 250, 1, getter, setter);
	return panel;
end

local areaItems = {};
local areaFilter = "";
local areaMapId;
local areaList;
local areaHeader;

local function AreaDepth(area, byId)
	local depth = 0;
	local parent = byId[area.parent];

	while ( parent and depth < 8 ) do
		depth = depth + 1;
		parent = byId[parent.parent];
	end

	return depth;
end

local function BuildAreaItems()
	local areas = EditorGetAreas();
	local byId = {};
	local children = {};
	local roots = {};

	for _, area in ipairs(areas) do
		byId[area.id] = area;
	end

	for _, area in ipairs(areas) do
		if ( area.parent ~= 0 and byId[area.parent] and area.parent ~= area.id ) then
			children[area.parent] = children[area.parent] or {};
			table.insert(children[area.parent], area);
		else
			table.insert(roots, area);
		end
	end

	local filter = string.lower(areaFilter);
	local items = {};

	local function Matches(area)
		return filter == "" or string.find(string.lower(area.id.."-"..area.name), filter, 1, true) ~= nil;
	end

	local function AnyChildMatches(area)
		for _, child in ipairs(children[area.id] or {}) do
			if ( Matches(child) or AnyChildMatches(child) ) then
				return true;
			end
		end

		return false;
	end

	local function Add(area, depth, forced)
		local visible = forced or Matches(area) or AnyChildMatches(area);

		if ( not visible ) then
			return;
		end

		table.insert(items, { id = area.id, text = area.id.."-"..area.name, depth = depth });

		for _, child in ipairs(children[area.id] or {}) do
			Add(child, depth + 1, forced or (filter ~= "" and Matches(area)));
		end
	end

	for _, area in ipairs(roots) do
		Add(area, 0, false);
	end

	areaItems = items;
end

local function RefreshAreaList()
	BuildAreaItems();
	NoggitListBox_SetItems(areaList, areaItems, true);
	local selected = ChunkToolValue(3);
	areaList.selected = nil;

	for index, item in ipairs(areaItems) do
		if ( item.id == selected ) then
			areaList.selected = index;
		end
	end

	NoggitListBox_Refresh(areaList);
end

function MapEditor_OnAreaPicked()
	local selected = ChunkToolValue(3);

	for index, item in ipairs(areaItems) do
		if ( item.id == selected ) then
			areaList.selected = index;
			local visible = areaList.visibleCount;

			if ( index <= areaList.offset or index > areaList.offset + visible ) then
				areaList.offset = max(0, index - floor(visible / 2));
			end
		end
	end

	NoggitListBox_Refresh(areaList);
end

function NoggitTools_CreateAreaDesignator(parent)
	local panel = NewPanel(parent);
	local getter = function() return ChunkToolValue(2); end;
	local setter = function(value) EditorSetChunkTool("areaRadius", value); end;
	FormSpinRow(panel, "Brush Radius:", 0, 1000, 1, 2, getter, setter);
	FormSlider(panel, 0, 1000, 1, getter, setter);

	local filterRow = CreateFrame("Frame", NoggitUI_NewName("NoggitFormRow"), panel);
	filterRow:SetWidth(panel.innerWidth);
	filterRow:SetHeight(22);
	local filterLabel = NoggitUI_FontString(filterRow, "Filter :");
	filterLabel:SetPoint("LEFT", filterRow, "LEFT", 0, 0);
	local filter = NoggitLineEdit_Create(filterRow, panel.innerWidth - 60);
	filter:SetPoint("RIGHT", filterRow, "RIGHT", 0, 0);
	filter:SetScript("OnTextChanged", function(self)
		areaFilter = self:GetText() or "";
		RefreshAreaList();
	end);
	PlaceInPanel(panel, filterRow, 22, 6);

	areaHeader = CreateFrame("Frame", NoggitUI_NewName("NoggitAreaHeader"), panel, "NoggitBoxTemplate");
	areaHeader:SetWidth(panel.innerWidth);
	areaHeader:SetHeight(22);
	NoggitBox_Set(areaHeader, NOGGIT_COLOR_MID, nil);
	areaHeader.text = NoggitUI_FontString(areaHeader, "");
	areaHeader.text:SetPoint("LEFT", areaHeader, "LEFT", 6, 0);
	PlaceInPanel(panel, areaHeader, 22, 0);

	areaList = NoggitListBox_Create(panel, 20, function(row)
		row.arrow = row:CreateTexture(nil, "ARTWORK");
		row.arrow:SetWidth(8);
		row.arrow:SetHeight(8);
		row.text = NoggitUI_FontString(row, "");
	end, function(row, item)
		row.text:ClearAllPoints();
		row.text:SetPoint("LEFT", row, "LEFT", 16 + item.depth * 14, 0);
		row.text:SetText(item.text);
		row.arrow:Hide();
	end);
	areaList:SetWidth(panel.innerWidth);
	areaList:SetHeight(330);
	areaList.onSelect = function(item)
		EditorSetChunkTool("areaId", item.id);
	end;
	PlaceInPanel(panel, areaList, 330, 6);

	local buttons = {
		{ "Edit selected Area", 0xF013 },
		{ "Add a new Zone(parent Area)", 0xF067 },
		{ "Add a new Subzone(selected as Parent)", 0xF067 },
	};

	for _, definition in ipairs(buttons) do
		local button = Button(panel, "    "..definition[1], panel.innerWidth, nil, false);
		button.tooltip = "AreaTable editing is not available yet";
		local glyph = button:CreateFontString(nil, "ARTWORK", "NoggitAwesomeFont");
		glyph:SetText(NoggitUI_Utf8(definition[2]));
		NoggitText_SetColor(glyph, NOGGIT_COLOR_DISABLED);
		glyph:SetPoint("RIGHT", button:GetFontString(), "LEFT", -2, 0);
		PlaceInPanel(panel, button, 23, 4);
	end

	table.insert(panel.sync, function()
		local directory, mapId = EditorGetMapInfo();

		if ( mapId ~= areaMapId ) then
			areaMapId = mapId;
			areaHeader.text:SetText(format("%d-%s", mapId or 0, directory or ""));
			RefreshAreaList();
		end
	end);

	return panel;
end

local function RgbToHsv(r, g, b)
	local maxValue = max(r, g, b);
	local minValue = min(r, g, b);
	local delta = maxValue - minValue;
	local hue = 0;

	if ( delta > 0 ) then
		if ( maxValue == r ) then
			hue = 60 * mod((g - b) / delta, 6);
		elseif ( maxValue == g ) then
			hue = 60 * ((b - r) / delta + 2);
		else
			hue = 60 * ((r - g) / delta + 4);
		end
	end

	if ( hue < 0 ) then
		hue = hue + 360;
	end

	local saturation = maxValue > 0 and delta / maxValue or 0;
	return floor(hue + 0.5), floor(saturation * 255 + 0.5), floor(maxValue * 255 + 0.5);
end

local function HsvToRgb(hue, saturation, value)
	local s = saturation / 255;
	local v = value / 255;
	local c = v * s;
	local x = c * (1 - math.abs(mod(hue / 60, 2) - 1));
	local m = v - c;
	local r, g, b;

	if ( hue < 60 ) then
		r, g, b = c, x, 0;
	elseif ( hue < 120 ) then
		r, g, b = x, c, 0;
	elseif ( hue < 180 ) then
		r, g, b = 0, c, x;
	elseif ( hue < 240 ) then
		r, g, b = 0, x, c;
	elseif ( hue < 300 ) then
		r, g, b = x, 0, c;
	else
		r, g, b = c, 0, x;
	end

	return r + m, g + m, b + m;
end

local vertexHsv = { 0, 0, 255 };
local vertexPalette = {};
local paletteButtons = {};
local colorSwatch;

local function SetVertexColor(r, g, b)
	EditorSetChunkTool("colorR", r);
	EditorSetChunkTool("colorG", g);
	EditorSetChunkTool("colorB", b);
end

local function RefreshSwatch()
	local r, g, b = select(6, EditorGetChunkTool());

	if ( colorSwatch ) then
		colorSwatch.color:SetTexture(r, g, b, 1);
	end
end

local function RefreshPalette()
	for index, button in ipairs(paletteButtons) do
		local color = vertexPalette[index];

		if ( color ) then
			button.color:SetTexture(color[1], color[2], color[3], 1);
			button:Show();
		else
			button:Hide();
		end
	end
end

function MapEditor_OnColorPicked()
	local r, g, b = select(6, EditorGetChunkTool());
	vertexHsv[1], vertexHsv[2], vertexHsv[3] = RgbToHsv(r, g, b);
	RefreshSwatch();
end

function MapEditor_AddVertexColorToPalette()
	local r, g, b = select(6, EditorGetChunkTool());

	if ( #vertexPalette < #paletteButtons ) then
		table.insert(vertexPalette, { r, g, b });
		RefreshPalette();
	end
end

function NoggitTools_CreateVertexPainter(parent)
	local panel = NewPanel(parent);

	local settings = NewGroup(panel, panel.innerWidth, "Settings");
	ExtSlider(panel, settings, "Radius:", 0, 10000, 1, 2, function() return ChunkToolValue(4); end, function(value) EditorSetChunkTool("vertexRadius", value); end);
	ExtSlider(panel, settings, "Speed:", 0, 10, 1, 2, function() return ChunkToolValue(5); end, function(value) EditorSetChunkTool("vertexSpeed", value); end);
	AddGroup(panel, settings);

	local colorRow = CreateFrame("Frame", NoggitUI_NewName("NoggitFormRow"), panel);
	colorRow:SetWidth(panel.innerWidth);
	colorRow:SetHeight(24);
	local colorLabel = NoggitUI_FontString(colorRow, "Color:");
	colorLabel:SetPoint("LEFT", colorRow, "LEFT", 0, 0);
	colorSwatch = CreateFrame("Frame", NoggitUI_NewName("NoggitColorSwatch"), colorRow, "NoggitBoxTemplate");
	colorSwatch:SetWidth(110);
	colorSwatch:SetHeight(22);
	colorSwatch:SetPoint("RIGHT", colorRow, "RIGHT", 0, 0);
	NoggitBox_Set(colorSwatch, nil, NOGGIT_COLOR_HOVER);
	colorSwatch.color = colorSwatch:CreateTexture(nil, "ARTWORK");
	colorSwatch.color:SetPoint("TOPLEFT", colorSwatch, "TOPLEFT", 2, -2);
	colorSwatch.color:SetPoint("BOTTOMRIGHT", colorSwatch, "BOTTOMRIGHT", -2, 2);
	PlaceInPanel(panel, colorRow, 24, 6);

	local function HsvGetter(index)
		return function() return vertexHsv[index]; end;
	end

	local function HsvSetter(index)
		return function(value)
			vertexHsv[index] = floor(value + 0.5);
			SetVertexColor(HsvToRgb(vertexHsv[1], vertexHsv[2], vertexHsv[3]));
			RefreshSwatch();
		end;
	end

	FormSpinRow(panel, "Hue:", 0, 359, 1, 0, HsvGetter(1), HsvSetter(1));
	FormSlider(panel, 0, 359, 1, HsvGetter(1), HsvSetter(1));
	FormSpinRow(panel, "Saturation:", 0, 255, 1, 0, HsvGetter(2), HsvSetter(2));
	FormSlider(panel, 0, 255, 1, HsvGetter(2), HsvSetter(2));
	FormSpinRow(panel, "Value:", 0, 255, 1, 0, HsvGetter(3), HsvSetter(3));
	FormSlider(panel, 0, 255, 1, HsvGetter(3), HsvSetter(3));

	local imageColors = NoggitCheckBox_Create(panel, "Use image colors", true);
	imageColors:Disable();
	PlaceInPanel(panel, imageColors, 20, 6);

	local palette = CreateFrame("Frame", NoggitUI_NewName("NoggitColorPalette"), panel, "NoggitBoxTemplate");
	palette:SetWidth(panel.innerWidth);
	palette:SetHeight(58);
	NoggitBox_Set(palette, NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID);
	local columns = floor((panel.innerWidth - 8) / 24);

	for i = 1, columns * 2 - 1 do
		local button = CreateFrame("Button", NoggitUI_NewName("NoggitPaletteColor"), palette, "NoggitBoxTemplate");
		button:SetWidth(20);
		button:SetHeight(20);
		button:SetPoint("TOPLEFT", palette, "TOPLEFT", 4 + mod(i - 1, columns) * 24, -4 - floor((i - 1) / columns) * 26);
		NoggitBox_Set(button, nil, NOGGIT_COLOR_HOVER);
		button.color = button:CreateTexture(nil, "ARTWORK");
		button.color:SetPoint("TOPLEFT", button, "TOPLEFT", 1, -1);
		button.color:SetPoint("BOTTOMRIGHT", button, "BOTTOMRIGHT", -1, 1);
		button.index = i;
		button:SetScript("OnClick", function(self)
			local color = vertexPalette[self.index];

			if ( color ) then
				SetVertexColor(color[1], color[2], color[3]);
				MapEditor_OnColorPicked();
			end
		end);
		button:Hide();
		table.insert(paletteButtons, button);
	end

	local add = NoggitPushButton_Create(palette, "+", 20, MapEditor_AddVertexColorToPalette);
	add:SetHeight(20);
	add:SetPoint("BOTTOMRIGHT", palette, "BOTTOMRIGHT", -4, 4);
	PlaceInPanel(panel, palette, 58, 6);

	local info = NoggitUI_FontString(panel, "Drag&Drop colors to select.");
	PlaceInPanel(panel, info, 16, 6);

	Expander(panel, "Custom Brush", nil, false, false);

	MapEditor_OnColorPicked();
	return panel;
end

local function FlightToolValue(index)
	return select(index, EditorGetFlightTool());
end

local function FlightInfoText()
	local x, y = EditorGetCursor();

	if ( not x ) then
		x, y = EditorGetCamera();
	end

	if ( not x ) then
		return "";
	end

	local tileX, tileY, hasChunk, lowerAt, upperAt, vertexLower, vertexUpper = EditorGetFlightBounds(x, y);

	if ( not tileX ) then
		return "No tile loaded here";
	elseif ( not hasChunk ) then
		return format("Tile %d_%d has no MFBO yet", tileX, tileY);
	end

	return format("Tile %d_%d\nFloor here: %.1f  ceiling: %.1f\nNearest vertex: floor %d, ceiling %d", tileX, tileY, lowerAt, upperAt, vertexLower, vertexUpper);
end

function NoggitTools_CreateFlightBounds(parent)
	local panel = NewPanel(parent);

	local layerGroup = NewGroup(panel, panel.innerWidth, "Plane");
	local layerRadios = RadioGrid(layerGroup, { "Floor (void)", "Ceiling" }, FlightToolValue(1), function(index)
		EditorSetFlightTool("layer", index);
	end);
	AddGroup(panel, layerGroup);

	table.insert(panel.sync, function()
		local layer = FlightToolValue(1);

		for i, radio in ipairs(layerRadios) do
			if ( radio.checked ~= (i == layer) ) then
				NoggitCheckBox_SetChecked(radio, i == layer);
			end
		end
	end);

	FormSpinRow(panel, "Height:", -32768, 32767, 1, 0, function() return FlightToolValue(2); end, function(value) EditorSetFlightTool("height", value); end);

	local fromCursor = NoggitCheckBox_Create(panel, "Use cursor height", FlightToolValue(4), function(_, checked)
		EditorSetFlightTool("fromCursor", checked);
	end);
	PlaceInPanel(panel, fromCursor, ROW_HEIGHT, 2);

	table.insert(panel.sync, function()
		local checked = FlightToolValue(4) and true or false;

		if ( fromCursor.checked ~= checked ) then
			NoggitCheckBox_SetChecked(fromCursor, checked);
		end
	end);

	FormSpinRow(panel, "Below cursor by:", -1000, 1000, 1, 1, function() return FlightToolValue(5); end, function(value) EditorSetFlightTool("offset", value); end);

	local radiusGetter = function() return FlightToolValue(3); end;
	local radiusSetter = function(value) EditorSetFlightTool("radius", value); end;
	FormSpinRow(panel, "Brush Radius:", 0, 2000, 1, 0, radiusGetter, radiusSetter);
	FormSlider(panel, 0, 2000, 1, radiusGetter, radiusSetter);

	local showUpper = NoggitCheckBox_Create(panel, "Show ceiling", FlightToolValue(6), function(_, checked)
		EditorSetFlightTool("showUpper", checked);
	end);
	PlaceInPanel(panel, showUpper, ROW_HEIGHT, 2);

	KeybindRow(panel, 0xF86D, "Set vertices in brush");
	KeybindRow(panel, 0xF86C, "Pick nearest vertex height");

	PlaceInPanel(panel, Button(panel, "Fill current tile", panel.innerWidth, EditorFillFlightTile, true), 23);
	PlaceInPanel(panel, Button(panel, "Fill all loaded tiles", panel.innerWidth, function()
		NoggitMessageBox_Show("Fill loaded tiles", "Set this plane to the height on every loaded tile?", {
			{ text = "Fill", onClick = EditorFillFlightLoaded },
			{ text = "Cancel" },
		});
	end, true), 23);

	local info = NoggitUI_FontString(panel, "");
	info:SetWidth(panel.innerWidth);
	info:SetHeight(48);
	PlaceInPanel(panel, info, 48, 4);

	table.insert(panel.sync, function()
		info:SetText(FlightInfoText());
	end);

	return panel;
end

function NoggitTools_Sync(panel)
	for _, sync in ipairs(panel.sync) do
		sync();
	end
end
