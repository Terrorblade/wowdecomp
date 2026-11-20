NOGGIT_COLOR_BASE = { 0.149, 0.157, 0.176, 1 };
NOGGIT_COLOR_DARK = { 0.122, 0.125, 0.137, 1 };
NOGGIT_COLOR_MID = { 0.176, 0.184, 0.204, 1 };
NOGGIT_COLOR_HOVER = { 0.216, 0.231, 0.251, 1 };
NOGGIT_COLOR_ACCENT = { 0.322, 0.506, 0.725, 1 };
NOGGIT_COLOR_TEXT = { 0.843, 0.843, 0.843, 1 };
NOGGIT_COLOR_DISABLED = { 0.498, 0.498, 0.498, 1 };
NOGGIT_COLOR_WHITE = { 1, 1, 1, 1 };

local TEXTURE_PATH = "Interface\\GameEditor\\Textures\\";
local nameCounter = 0;

function NoggitUI_NewName(prefix)
	nameCounter = nameCounter + 1;
	return (prefix or "NoggitWidget")..nameCounter;
end

function NoggitUI_Utf8(codepoint)
	if ( codepoint < 0x80 ) then
		return string.char(codepoint);
	elseif ( codepoint < 0x800 ) then
		return string.char(0xC0 + floor(codepoint / 0x40), 0x80 + mod(codepoint, 0x40));
	elseif ( codepoint < 0x10000 ) then
		return string.char(0xE0 + floor(codepoint / 0x1000), 0x80 + mod(floor(codepoint / 0x40), 0x40), 0x80 + mod(codepoint, 0x40));
	end

	return string.char(0xF0 + floor(codepoint / 0x40000), 0x80 + mod(floor(codepoint / 0x1000), 0x40), 0x80 + mod(floor(codepoint / 0x40), 0x40), 0x80 + mod(codepoint, 0x40));
end

local function SetColor(texture, color)
	if ( color ) then
		texture:SetTexture(color[1], color[2], color[3], color[4] or 1);
	else
		texture:SetTexture(0, 0, 0, 0);
	end
end

function NoggitBox_Set(frame, background, border)
	local name = frame:GetName();
	SetColor(_G[name.."Bg"], background);
	SetColor(_G[name.."BorderTop"], border);
	SetColor(_G[name.."BorderBottom"], border);
	SetColor(_G[name.."BorderLeft"], border);
	SetColor(_G[name.."BorderRight"], border);
end

function NoggitText_SetColor(fontString, color)
	fontString:SetTextColor(color[1], color[2], color[3], color[4] or 1);
end

function NoggitUI_SolidTexture(frame, layer, color)
	local texture = frame:CreateTexture(nil, layer or "BACKGROUND");
	SetColor(texture, color);
	return texture;
end

function NoggitUI_FontString(parent, text, font, justify)
	local fontString = parent:CreateFontString(nil, "ARTWORK", font or "NoggitFont");
	fontString:SetJustifyH(justify or "LEFT");

	if ( text ) then
		fontString:SetText(text);
	end

	return fontString;
end

function NoggitButton_SetHover(self, hover)
	self.hover = hover;

	if ( not hover ) then
		self.pressed = false;
	end

	NoggitButton_Refresh(self);
end

function NoggitButton_SetPressed(self, pressed)
	self.pressed = pressed;
	NoggitButton_Refresh(self);
end

function NoggitButton_Refresh(self)
	if ( self.isToolButton ) then
		NoggitToolButton_Refresh(self);
		return;
	end

	local enabled = self:IsEnabled() == 1;
	local background = NOGGIT_COLOR_MID;

	if ( not enabled ) then
		background = NOGGIT_COLOR_DARK;
	elseif ( self.pressed ) then
		background = NOGGIT_COLOR_DARK;
	elseif ( self.hover ) then
		background = NOGGIT_COLOR_HOVER;
	end

	local tint, border = NoggitFeedback_Colors and NoggitFeedback_Colors(self);
	NoggitBox_Set(self, tint or background, border or NOGGIT_COLOR_HOVER);
end

function NoggitPushButton_OnLoad(self)
	self.hover = false;
	self.pressed = false;
	self:GetFontString():SetTextColor(NOGGIT_COLOR_TEXT[1], NOGGIT_COLOR_TEXT[2], NOGGIT_COLOR_TEXT[3]);
	self:SetDisabledFontObject(NoggitFontDisabled);
	NoggitButton_Refresh(self);

	if ( NoggitFeedback_Attach ) then
		NoggitFeedback_Attach(self);
	end
end

-- Icons the bundled Font Awesome subset does not have, drawn as white textures and tinted.
NOGGIT_GLYPH_TEXTURES = {
	[0xF002] = "search", [0xF007] = "user", [0xF021] = "refresh", [0xF04B] = "play", [0xF04D] = "stop",
	[0xF053] = "chevron_left", [0xF054] = "chevron_right", [0xF059] = "help", [0xF0C7] = "save",
	[0xF100] = "double_left", [0xF101] = "double_right", [0xF0D7] = "caret_down", [0xF0DA] = "caret_right",
};

function NoggitUI_GlyphTexture(codepoint)
	local name = NOGGIT_GLYPH_TEXTURES[codepoint];
	return name and ("Interface\\GameEditor\\Textures\\glyph_"..name) or nil;
end

-- Expand arrow for collapsible headers.
function NoggitUI_CreateCaret(parent, size)
	local caret = parent:CreateTexture(nil, "ARTWORK");
	caret:SetWidth(size or 12);
	caret:SetHeight(size or 12);
	caret:SetVertexColor(NOGGIT_COLOR_TEXT[1], NOGGIT_COLOR_TEXT[2], NOGGIT_COLOR_TEXT[3]);
	caret.SetOpen = function(self, open)
		self:SetTexture(NoggitUI_GlyphTexture(open and 0xF0D7 or 0xF0DA));
	end;
	caret:SetOpen(false);
	return caret;
end

function NoggitPushButton_Create(parent, text, width, onClick)
	local button = CreateFrame("Button", NoggitUI_NewName("NoggitPushButton"), parent, "NoggitPushButtonTemplate");
	button:SetText(text);
	button:SetWidth(width or max(80, button:GetFontString():GetStringWidth() + 24));
	button:SetScript("OnClick", onClick);
	return button;
end

function NoggitToolButton_OnLoad(self)
	self.isToolButton = true;
	self.hover = false;
	self.pressed = false;
	self.checked = false;
	NoggitToolButton_Refresh(self);

	if ( NoggitFeedback_Attach ) then
		NoggitFeedback_Attach(self);
	end
end

function NoggitToolButton_Refresh(self)
	local enabled = self:IsEnabled() == 1;
	local icon = _G[self:GetName().."Icon"];

	if ( not enabled ) then
		NoggitBox_Set(self, NOGGIT_COLOR_BASE, nil);
		icon:SetFontObject(NoggitIconFontDisabled);
		return;
	end

	icon:SetFontObject(NoggitIconFont);

	local tint, border = NoggitFeedback_Colors and NoggitFeedback_Colors(self);

	if ( tint ) then
		NoggitBox_Set(self, tint, border);
	elseif ( self.pressed or self.checked ) then
		NoggitBox_Set(self, NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID);
	elseif ( self.hover ) then
		NoggitBox_Set(self, NOGGIT_COLOR_MID, NOGGIT_COLOR_HOVER);
	else
		NoggitBox_Set(self, NOGGIT_COLOR_BASE, nil);
	end
end

function NoggitToolButton_SetChecked(self, checked)
	self.checked = checked and true or false;
	NoggitToolButton_Refresh(self);
end

function NoggitToolButton_Create(parent, codepoint, tooltip, onClick)
	local button = CreateFrame("Button", NoggitUI_NewName("NoggitToolButton"), parent, "NoggitToolButtonTemplate");
	_G[button:GetName().."Icon"]:SetText(NoggitUI_Utf8(codepoint));
	button.tooltip = tooltip;
	button:SetScript("OnClick", onClick);
	return button;
end

function NoggitCheckBox_OnLoad(self)
	self.checked = false;
	self.hover = false;
	NoggitCheckBox_Refresh(self);
end

function NoggitCheckBox_Refresh(self)
	local name = self:GetName();
	local enabled = self:IsEnabled() == 1;
	local kind = self.radioGroup and "radiobutton" or "checkbox";
	local state = self.checked and "checked" or "unchecked";
	local suffix = "";

	if ( not enabled ) then
		suffix = "_disabled";
	elseif ( self.hover ) then
		suffix = "_hover";
	end

	_G[name.."Indicator"]:SetTexture(TEXTURE_PATH.."icon_"..kind.."_"..state..suffix..".tga");

	local label = _G[name.."Label"];

	if ( not enabled ) then
		NoggitText_SetColor(label, NOGGIT_COLOR_DISABLED);
	elseif ( self.hover ) then
		NoggitText_SetColor(label, NOGGIT_COLOR_WHITE);
	else
		NoggitText_SetColor(label, NOGGIT_COLOR_TEXT);
	end
end

function NoggitCheckBox_SetChecked(self, checked)
	self.checked = checked and true or false;
	NoggitCheckBox_Refresh(self);
end

function NoggitCheckBox_OnClick(self)
	if ( self.radioGroup ) then
		if ( self.checked ) then
			return;
		end

		for _, other in ipairs(self.radioGroup) do
			NoggitCheckBox_SetChecked(other, other == self);
		end
	else
		NoggitCheckBox_SetChecked(self, not self.checked);
	end

	if ( self.onChange ) then
		self.onChange(self, self.checked);
	end
end

function NoggitCheckBox_Create(parent, text, checked, onChange, radioGroup)
	local box = CreateFrame("Button", NoggitUI_NewName("NoggitCheckBox"), parent, "NoggitCheckBoxTemplate");
	_G[box:GetName().."Label"]:SetText(text);
	box:SetWidth(_G[box:GetName().."Label"]:GetStringWidth() + 26);
	box.onChange = onChange;

	if ( radioGroup ) then
		box.radioGroup = radioGroup;
		table.insert(radioGroup, box);
	end

	NoggitCheckBox_SetChecked(box, checked);
	return box;
end

function NoggitSlider_OnLoad(self)
	self:EnableMouseWheel(true);
	self.minValue = 0;
	self.maxValue = 1;
	self.step = 0.01;
end

function NoggitSlider_UpdateFill(self)
	local fill = _G[self:GetName().."SubPage"];
	local range = self.maxValue - self.minValue;
	local fraction = range > 0 and (self:GetValue() - self.minValue) / range or 0;
	local width = max(1, (self:GetWidth() - 2) * fraction);
	fill:SetWidth(width);
	fill:SetGradient("HORIZONTAL", NOGGIT_COLOR_ACCENT[1], NOGGIT_COLOR_ACCENT[2], NOGGIT_COLOR_ACCENT[3], NOGGIT_COLOR_MID[1], NOGGIT_COLOR_MID[2], NOGGIT_COLOR_MID[3]);
end

function NoggitSlider_OnValueChanged(self, value)
	NoggitSlider_UpdateFill(self);

	if ( not self.updating and self.onChange ) then
		self.onChange(self, value);
	end
end

function NoggitSlider_OnMouseWheel(self, delta)
	self:SetValue(self:GetValue() + delta * self.step);
end

function NoggitSlider_SetRange(self, minValue, maxValue, step)
	self.minValue = minValue;
	self.maxValue = maxValue;
	self.step = step;
	self:SetMinMaxValues(minValue, maxValue);
	self:SetValueStep(step);
	NoggitSlider_UpdateFill(self);
end

function NoggitSlider_SetValueQuiet(self, value)
	self.updating = true;
	self:SetValue(value);
	self.updating = false;
	NoggitSlider_UpdateFill(self);
end

function NoggitSlider_Create(parent, width, minValue, maxValue, step, onChange)
	local slider = CreateFrame("Slider", NoggitUI_NewName("NoggitSlider"), parent, "NoggitSliderTemplate");
	slider:SetWidth(width);
	NoggitSlider_SetRange(slider, minValue, maxValue, step);
	slider.onChange = onChange;
	return slider;
end

function NoggitSpinBox_OnLoad(self)
	self.value = 0;
	self.minValue = 0;
	self.maxValue = 100;
	self.step = 1;
	self.decimals = 2;
	self:EnableMouseWheel(true);
	NoggitBox_Set(self, NOGGIT_COLOR_DARK, nil);
	_G[self:GetName().."Edit"]:SetTextInsets(0, 0, 0, 0);
end

function NoggitSpinBox_Refresh(self)
	_G[self:GetName().."Edit"]:SetText(format("%."..self.decimals.."f", self.value));
end

function NoggitSpinBox_SetValue(self, value, quiet)
	value = max(self.minValue, min(self.maxValue, value));
	local changed = value ~= self.value;
	self.value = value;
	NoggitSpinBox_Refresh(self);

	if ( changed and not quiet and self.onChange ) then
		self.onChange(self, value);
	end
end

function NoggitSpinBox_Commit(self)
	local value = tonumber(_G[self:GetName().."Edit"]:GetText());

	if ( value ) then
		NoggitSpinBox_SetValue(self, value);
	else
		NoggitSpinBox_Refresh(self);
	end
end

function NoggitSpinBox_Step(self, delta)
	NoggitSpinBox_SetValue(self, self.value + delta * self.step);
end

function NoggitSpinBox_Create(parent, width, minValue, maxValue, step, decimals, onChange)
	local box = CreateFrame("Frame", NoggitUI_NewName("NoggitSpinBox"), parent, "NoggitSpinBoxTemplate");
	box:SetWidth(width or 84);
	box.minValue = minValue;
	box.maxValue = maxValue;
	box.step = step;
	box.decimals = decimals;
	box.onChange = onChange;
	NoggitSpinBox_SetValue(box, minValue, true);
	box.SetEnabledState = function(self, enabled)
		local name = self:GetName();
		_G[name.."Edit"]:EnableMouse(enabled);
		_G[name.."Edit"]:SetTextColor(unpack(enabled and NOGGIT_COLOR_TEXT or NOGGIT_COLOR_DISABLED));

		if ( enabled ) then
			_G[name.."Up"]:Enable();
			_G[name.."Down"]:Enable();
		else
			_G[name.."Edit"]:ClearFocus();
			_G[name.."Up"]:Disable();
			_G[name.."Down"]:Disable();
		end
	end;
	return box;
end

function NoggitExtendedSlider_Create(parent, width, prefix, minValue, maxValue, step, decimals, onChange)
	local frame = CreateFrame("Frame", NoggitUI_NewName("NoggitExtendedSlider"), parent);
	frame:SetWidth(width);
	frame:SetHeight(46);

	local label = NoggitUI_FontString(frame, prefix);
	label:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, -5);

	local pen = CreateFrame("Button", NoggitUI_NewName("NoggitPenButton"), frame, "NoggitPushButtonTemplate");
	pen:SetWidth(23);
	pen:SetHeight(22);
	pen:SetPoint("TOPRIGHT", frame, "TOPRIGHT", 0, 0);
	pen:SetNormalFontObject(NoggitAwesomeFont);
	pen:SetHighlightFontObject(NoggitAwesomeFont);
	pen:SetDisabledFontObject(NoggitAwesomeFont);
	pen:SetText(NoggitUI_Utf8(0xF304));
	pen:Disable();

	local spin = NoggitSpinBox_Create(frame, 84, minValue, maxValue, step, decimals);
	spin:SetPoint("TOPRIGHT", pen, "TOPLEFT", -6, 0);

	local slider = NoggitSlider_Create(frame, width, minValue, maxValue, step);
	slider:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, -28);

	frame.spin = spin;
	frame.slider = slider;
	frame.onChange = onChange;

	spin.onChange = function(_, value)
		NoggitSlider_SetValueQuiet(slider, value);

		if ( frame.onChange ) then
			frame.onChange(frame, value);
		end
	end;

	slider.onChange = function(_, value)
		NoggitSpinBox_SetValue(spin, value, true);

		if ( frame.onChange ) then
			frame.onChange(frame, value);
		end
	end;

	frame.SetValue = function(self, value)
		NoggitSpinBox_SetValue(spin, value, true);
		NoggitSlider_SetValueQuiet(slider, value);
	end;

	frame.GetValue = function(self)
		return spin.value;
	end;

	frame.SetEnabledState = function(self, enabled)
		spin:SetEnabledState(enabled);
		NoggitText_SetColor(label, enabled and NOGGIT_COLOR_TEXT or NOGGIT_COLOR_DISABLED);

		if ( enabled ) then
			slider:Enable();
		else
			slider:Disable();
		end
	end;

	return frame;
end

function NoggitGroupBox_OnLoad(self)
	NoggitBox_Set(self, nil, NOGGIT_COLOR_MID);
end

function NoggitGroupBox_SetEnabled(self, enabled)
	self.enabledState = enabled;
	local title = _G[self:GetName().."Title"];
	title:SetFontObject(enabled and NoggitFontBold or NoggitFontBoldDisabled);

	for _, child in ipairs(self.children or {}) do
		if ( child.SetEnabledState ) then
			child:SetEnabledState(enabled);
		elseif ( child.Enable ) then
			if ( enabled ) then
				child:Enable();
			else
				child:Disable();
			end
		end
	end
end

function NoggitGroupBox_Create(parent, width, title, checkable, checked, onToggle)
	local box = CreateFrame("Frame", NoggitUI_NewName("NoggitGroupBox"), parent, "NoggitGroupBoxTemplate");
	box:SetWidth(width);
	box.children = {};

	local titleText = _G[box:GetName().."Title"];
	titleText:SetText(title);

	local line = _G[box:GetName().."TitleLine"];
	local lineWidth = titleText:GetStringWidth();

	if ( checkable ) then
		local toggle = CreateFrame("Button", NoggitUI_NewName("NoggitGroupToggle"), box, "NoggitCheckBoxTemplate");
		toggle:SetWidth(lineWidth + 22);
		toggle:SetHeight(18);
		toggle:SetPoint("TOP", box, "TOP", 0, -3);
		_G[toggle:GetName().."Indicator"]:SetWidth(14);
		_G[toggle:GetName().."Indicator"]:SetHeight(14);
		_G[toggle:GetName().."Label"]:SetText("");
		titleText:ClearAllPoints();
		titleText:SetPoint("LEFT", toggle, "LEFT", 22, 0);
		toggle.onChange = function(_, value)
			NoggitGroupBox_SetEnabled(box, value);

			if ( onToggle ) then
				onToggle(box, value);
			end
		end;
		NoggitCheckBox_SetChecked(toggle, checked);
		box.toggle = toggle;
		lineWidth = lineWidth + 22;
	end

	line:SetWidth(lineWidth);
	box.contentTop = -22;
	return box;
end

function NoggitGroupBox_Add(box, child)
	table.insert(box.children, child);
end

function NoggitGroupBox_Finish(box)
	if ( box.toggle ) then
		NoggitGroupBox_SetEnabled(box, box.toggle.checked);
	end
end

local tooltip;

function NoggitTooltip_Show(owner, text)
	if ( not text or text == "" ) then
		return;
	end

	if ( not tooltip ) then
		tooltip = CreateFrame("Frame", "NoggitTooltip", EditorParent, "NoggitTooltipTemplate");
		NoggitBox_Set(tooltip, NOGGIT_COLOR_BASE, NOGGIT_COLOR_HOVER);
	end

	NoggitTooltipText:SetWidth(0);
	NoggitTooltipText:SetText(text);
	local width = NoggitTooltipText:GetStringWidth();

	if ( width > 380 ) then
		width = 380;
	end

	NoggitTooltipText:SetWidth(width + 2);
	tooltip:SetWidth(width + 14);
	tooltip:SetHeight(max(22, NoggitTooltipText:GetHeight() + 10));
	tooltip:ClearAllPoints();
	tooltip:SetPoint("TOPLEFT", owner, "BOTTOMRIGHT", 2, -2);
	tooltip:SetClampedToScreen(true);
	tooltip:Show();
end

function NoggitTooltip_Hide()
	if ( tooltip ) then
		tooltip:Hide();
	end
end

function NoggitTab_Refresh(self)
	local indicator = _G[self:GetName().."Indicator"];
	local enabled = self:IsEnabled() == 1;

	if ( self.selected ) then
		NoggitBox_Set(self, NOGGIT_COLOR_BASE, nil);
		indicator:SetHeight(3);
		indicator:Show();
	elseif ( self.hover and enabled ) then
		NoggitBox_Set(self, NOGGIT_COLOR_HOVER, nil);
		indicator:SetHeight(1);
		indicator:Show();
	else
		NoggitBox_Set(self, NOGGIT_COLOR_MID, NOGGIT_COLOR_BASE);
		indicator:Hide();
	end
end

function NoggitTabs_Create(parent, names, selected, onSelect, disabled)
	local tabs = {};
	local previous;

	for index, name in ipairs(names) do
		local tab = CreateFrame("Button", NoggitUI_NewName("NoggitTab"), parent, "NoggitTabButtonTemplate");
		tab:SetText(name);
		tab:SetWidth(max(55, tab:GetFontString():GetStringWidth() + 20) + 2);

		if ( previous ) then
			tab:SetPoint("LEFT", previous, "RIGHT", 0, 0);
		end

		tab.index = index;
		tab.selected = index == selected;

		if ( disabled and disabled[index] ) then
			tab:Disable();
		end

		tab:SetScript("OnClick", function(self)
			for _, other in ipairs(tabs) do
				other.selected = other == self;
				NoggitTab_Refresh(other);
			end

			if ( onSelect ) then
				onSelect(self.index);
			end
		end);

		NoggitTab_Refresh(tab);
		tabs[index] = tab;
		previous = tab;
	end

	return tabs;
end

function NoggitTextureButton_Refresh(self)
	if ( self.selected ) then
		NoggitBox_Set(self, NOGGIT_COLOR_DARK, NOGGIT_COLOR_ACCENT);
	elseif ( self.hover ) then
		NoggitBox_Set(self, NOGGIT_COLOR_HOVER, NOGGIT_COLOR_HOVER);
	else
		NoggitBox_Set(self, NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID);
	end
end

function NoggitTextureButton_Create(parent, size)
	local button = CreateFrame("Button", NoggitUI_NewName("NoggitTextureButton"), parent, "NoggitTextureButtonTemplate");
	button:SetWidth(size);
	button:SetHeight(size);
	button.image = _G[button:GetName().."Image"];
	NoggitTextureButton_Refresh(button);
	return button;
end

function NoggitLineEdit_Create(parent, width)
	local edit = CreateFrame("EditBox", NoggitUI_NewName("NoggitLineEdit"), parent, "NoggitLineEditTemplate");
	edit:SetWidth(width);
	edit:SetTextColor(NOGGIT_COLOR_TEXT[1], NOGGIT_COLOR_TEXT[2], NOGGIT_COLOR_TEXT[3]);
	return edit;
end

function NoggitVerticalSlider_Create(parent, height, minValue, maxValue, step, onChange)
	local slider = CreateFrame("Slider", NoggitUI_NewName("NoggitVerticalSlider"), parent, "NoggitVerticalSliderTemplate");
	slider:SetHeight(height);
	slider:SetMinMaxValues(minValue, maxValue);
	slider:SetValueStep(step);
	slider.step = step;
	slider.onChange = onChange;
	return slider;
end

function NoggitVerticalSlider_SetValueQuiet(slider, value)
	slider.updating = true;
	slider:SetValue(value);
	slider.updating = false;
end
