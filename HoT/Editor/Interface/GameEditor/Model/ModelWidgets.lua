-- Widgets of the models workspace: value editors, the field form, track rows and the track key window.

local ME = GameEditorModel;
local Common = GameEditorObject;

local LABEL_WIDTH = 150;
local ROW = 26;
local BIG = 1000000;
local MAX_PART_KEYS = 4;
local MAX_RIBBON_TEXTURES = 4;

ME.LABEL_WIDTH = LABEL_WIDTH;
ME.ROW = ROW;

local Num = ME.Num;

local function EditOf(spin)
	return _G[spin:GetName().."Edit"];
end

function ME.SetSpin(spin, value, enabled)
	if ( not EditOf(spin):HasFocus() ) then
		NoggitSpinBox_SetValue(spin, Num(value), true);
	end

	spin:SetEnabledState(enabled ~= false);
end

function ME.SpinTip(spin, text)
	if ( text ) then
		Common.Tooltip(EditOf(spin), text);
	end
end

function ME.Spin(parent, width, low, high, step, decimals, onChange, tooltip)
	local spin = NoggitSpinBox_Create(parent, width, low, high, step, decimals, function(_, value)
		onChange(value);
	end);
	ME.SpinTip(spin, tooltip);
	return spin;
end

function ME.PickColor(r, g, b, onChange, title)
	local LE = GameEditorLight;

	if ( LE and LE.PickColor ) then
		LE.PickColor(floor(r + 0.5), floor(g + 0.5), floor(b + 0.5), onChange, title);
	else
		ME.Notice("The color picker comes with the lights workspace, it is not loaded");
	end
end

function ME.HideColorPicker()
	local LE = GameEditorLight;

	if ( LE and LE.HideColorPicker ) then
		LE.HideColorPicker();
	end
end

function ME.SetEnabled(widget, enabled)
	Common.SetEnabled(widget, enabled and true or false);
end

-- Value editors ---------------------------------------------------------------------------------------------

local KINDS = {
	float = { count = 1, low = -BIG, high = BIG, step = 0.1, decimals = 3, width = 90 },
	int = { count = 1, low = 0, high = 65535, step = 1, decimals = 0, width = 90 },
	byte = { count = 1, low = 0, high = 255, step = 1, decimals = 0, width = 90 },
	alpha = { count = 1, low = 0, high = 1, step = 0.05, decimals = 3, width = 90 },
	vec3 = { count = 3, comps = { "x", "y", "z" }, low = -BIG, high = BIG, step = 0.1, decimals = 3, width = 64 },
	vec2 = { count = 2, comps = { "x", "y" }, low = -BIG, high = BIG, step = 0.1, decimals = 3, width = 64 },
	fp69 = { count = 2, comps = { "x", "y" }, low = -63.99, high = 63.99, step = 0.05, decimals = 3, width = 64 },
	range = { count = 2, comps = { "min", "max" }, low = -BIG, high = BIG, step = 0.1, decimals = 3, width = 64 },
	color = { count = 3, comps = { "x", "y", "z" }, low = 0, high = 1, step = 0.05, decimals = 3, width = 58, swatch = 1 },
	pcolor = { count = 3, comps = { "x", "y", "z" }, low = 0, high = 255, step = 1, decimals = 0, width = 52, swatch = 255 },
};

ME.VALUE_KINDS = KINDS;

function ME.ZeroValue(kind)
	local spec = KINDS[kind] or KINDS.float;

	if ( spec.count == 1 ) then
		return kind == "alpha" and 1 or 0;
	end

	local values = {};

	for index = 1, spec.count do
		values[index] = spec.swatch and spec.swatch or 0;
	end

	return ME.MakeVector(values, spec.comps);
end

function ME.ValueText(kind, value)
	local spec = KINDS[kind] or KINDS.float;

	if ( spec.count == 1 ) then
		return format(spec.decimals > 0 and "%.3f" or "%d", Num(value));
	end

	local parts = {};

	for index = 1, spec.count do
		table.insert(parts, format(spec.decimals > 0 and "%.2f" or "%d", ME.Comp(value, index, spec.comps)));
	end

	return table.concat(parts, ", ");
end

-- A row of spins for one value of a kind, onChange(value) gets the whole value.
function ME.CreateValueEditor(parent, kind, onChange)
	local spec = KINDS[kind] or KINDS.float;
	local editor = CreateFrame("Frame", NoggitUI_NewName("GameEditorModelValue"), parent);
	editor:SetHeight(22);
	editor.kind = kind;
	editor.spins = {};
	editor.enabled = true;
	editor.value = ME.ZeroValue(kind);

	local x = 0;

	local function Changed(value)
		editor.value = value;
		editor:Refresh();

		if ( onChange ) then
			onChange(value);
		end
	end

	if ( spec.swatch ) then
		local swatch = CreateFrame("Button", NoggitUI_NewName("GameEditorModelSwatch"), editor, "NoggitBoxTemplate");
		swatch:SetWidth(22);
		swatch:SetHeight(22);
		swatch:SetPoint("LEFT", editor, "LEFT", 0, 0);
		NoggitBox_Set(swatch, nil, NOGGIT_COLOR_HOVER);
		swatch.color = swatch:CreateTexture(nil, "ARTWORK");
		swatch.color:SetPoint("TOPLEFT", swatch, "TOPLEFT", 2, -2);
		swatch.color:SetPoint("BOTTOMRIGHT", swatch, "BOTTOMRIGHT", -2, 2);
		swatch:SetScript("OnClick", function()
			if ( not editor.enabled ) then
				return;
			end

			local scale = 255 / spec.swatch;
			local value = editor.value;
			ME.PickColor(ME.Comp(value, 1) * scale, ME.Comp(value, 2) * scale, ME.Comp(value, 3) * scale, function(r, g, b)
				local back = spec.swatch / 255;
				Changed(ME.MakeVector({ r * back, g * back, b * back }, spec.comps));
			end, editor.title);
		end);
		Common.Tooltip(swatch, "Pick the color");
		editor.swatch = swatch;
		x = 26;
	end

	for index = 1, spec.count do
		local spin = NoggitSpinBox_Create(editor, spec.width, spec.low, spec.high, spec.step, spec.decimals, function(_, value)
			if ( spec.count == 1 ) then
				Changed(value);
				return;
			end

			local values = {};

			for other = 1, spec.count do
				values[other] = ME.Comp(editor.value, other, spec.comps);
			end

			values[index] = value;
			Changed(ME.MakeVector(values, spec.comps));
		end);
		spin:SetPoint("LEFT", editor, "LEFT", x, 0);
		x = x + spec.width + 4;
		editor.spins[index] = spin;
	end

	editor:SetWidth(x - 4);

	editor.Refresh = function(self)
		for index, spin in ipairs(self.spins) do
			ME.SetSpin(spin, spec.count == 1 and self.value or ME.Comp(self.value, index, spec.comps), self.enabled);
		end

		if ( self.swatch ) then
			local scale = 1 / spec.swatch;
			self.swatch.color:SetTexture(ME.Comp(self.value, 1) * scale, ME.Comp(self.value, 2) * scale, ME.Comp(self.value, 3) * scale, 1);
		end
	end;

	editor.SetValue = function(self, value)
		if ( value == nil ) then
			value = ME.ZeroValue(kind);
		end

		self.value = ME.Copy(value);
		self:Refresh();
	end;

	editor.SetEnabledState = function(self, enabled)
		self.enabled = enabled and true or false;
		self:Refresh();
	end;

	editor.SetTip = function(self, text)
		for _, spin in ipairs(self.spins) do
			ME.SpinTip(spin, text);
		end
	end;

	editor:Refresh();
	return editor;
end

-- Form ----------------------------------------------------------------------------------------------------------

local Form = {};
Form.__index = Form;

-- Rows of labelled fields bound to the keys of a data table, onSet(fields) gets the changed keys.
function ME.CreateForm(parent, width, onSet)
	return setmetatable({ parent = parent, width = width, items = {}, onSet = onSet, height = 0 }, Form);
end

function Form:Item(height)
	local frame = CreateFrame("Frame", NoggitUI_NewName("GameEditorModelRow"), self.parent);
	frame:SetHeight(height);
	frame:SetWidth(self.width);
	local item = { frame = frame, height = height };
	table.insert(self.items, item);
	return item;
end

function Form:Layout()
	local y = 0;

	for _, item in ipairs(self.items) do
		if ( item.hidden ) then
			item.frame:Hide();
		else
			item.frame:ClearAllPoints();
			item.frame:SetPoint("TOPLEFT", self.parent, "TOPLEFT", 0, -y);
			item.frame:SetHeight(item.height);
			item.frame:Show();
			y = y + item.height;
		end
	end

	self.height = y;
	return y;
end

function Form:Refresh(data, info)
	self.data = data;
	self.info = info;

	for _, item in ipairs(self.items) do
		item.hidden = (item.visible and not item.visible(data, info)) and true or false;

		if ( item.refresh and not item.hidden ) then
			item.refresh(data, info);
		end
	end

	return self:Layout();
end

function Form:Set(key, value)
	if ( self.data and self.onSet ) then
		self.onSet({ [key] = value });
	end
end

local function RowLabel(item, text)
	local label = Common.Label(item.frame, text);
	label:SetPoint("TOPLEFT", item.frame, "TOPLEFT", 0, -4);
	label:SetWidth(LABEL_WIDTH - 6);
	label:SetHeight(16);
	return label;
end

function Form:Header(text)
	local item = self:Item(28);
	local label = Common.Label(item.frame, text, "NoggitFontBold", NOGGIT_COLOR_GOLD);
	label:SetPoint("TOPLEFT", item.frame, "TOPLEFT", 0, -10);
	local line = NoggitSeparator_Create(item.frame);
	line:SetPoint("LEFT", label, "RIGHT", 6, 0);
	line:SetPoint("RIGHT", item.frame, "RIGHT", 0, 0);
	return item;
end

-- Text that follows the data, func(data, info) returns the text.
function Form:Note(func, height)
	local item = self:Item(height or 20);
	local label = Common.Paragraph(item.frame, "");
	label:SetPoint("TOPLEFT", item.frame, "TOPLEFT", 0, -3);
	label:SetPoint("TOPRIGHT", item.frame, "TOPRIGHT", 0, -3);
	item.refresh = function(data, info)
		label:SetText(type(func) == "function" and (func(data, info) or "") or tostring(func or ""));
	end;
	return item;
end

function Form:Number(key, label, low, high, step, decimals, tooltip)
	local item = self:Item(ROW);
	RowLabel(item, label);
	local spin = ME.Spin(item.frame, 96, low, high, step, decimals, function(value) self:Set(key, value); end, tooltip);
	spin:SetPoint("TOPLEFT", item.frame, "TOPLEFT", LABEL_WIDTH, 0);
	item.spin = spin;
	item.hint = Common.Label(item.frame, "", nil, NOGGIT_COLOR_DISABLED);
	item.hint:SetPoint("LEFT", spin, "RIGHT", 8, 0);
	item.hint:SetPoint("RIGHT", item.frame, "RIGHT", 0, 0);
	item.refresh = function(data, info)
		ME.SetSpin(spin, data and data[key], data ~= nil);

		if ( item.hintFunc ) then
			item.hint:SetText(data and item.hintFunc(data[key], data, info) or "");
		end
	end;
	return item;
end

function Form:Float(key, label, tooltip)
	return self:Number(key, label, -BIG, BIG, 0.1, 3, tooltip);
end

function Form:Int(key, label, low, high, tooltip)
	return self:Number(key, label, low or 0, high or 65535, 1, 0, tooltip);
end

-- A vector value, sub picks a table inside the value such as tumble.min.
function Form:Value(key, label, kind, tooltip, sub)
	local item = self:Item(ROW);
	RowLabel(item, label);
	local editor = ME.CreateValueEditor(item.frame, kind, function(value)
		if ( not self.data ) then
			return;
		end

		if ( sub ) then
			local whole = ME.Copy(self.data[key]) or {};
			whole[sub] = value;
			self:Set(key, whole);
		else
			self:Set(key, value);
		end
	end);
	editor:SetPoint("TOPLEFT", item.frame, "TOPLEFT", LABEL_WIDTH, 0);
	editor.title = label;
	editor:SetTip(tooltip);
	item.editor = editor;
	item.refresh = function(data)
		local value = data and data[key];

		if ( sub and type(value) == "table" ) then
			value = value[sub];
		end

		editor:SetValue(value);
		editor:SetEnabledState(data ~= nil);
	end;
	return item;
end

-- items is a list of { value, text } or a function(info) that builds it.
function Form:Combo(key, label, items, tooltip, width)
	local item = self:Item(ROW);
	RowLabel(item, label);
	local combo = NoggitComboBox_Create(item.frame, width or 220, type(items) == "table" and items or { { value = 0, text = "" } }, 1, function(_, _, entry)
		if ( entry and entry.value ~= nil ) then
			self:Set(key, entry.value);
		end
	end);
	combo:SetPoint("TOPLEFT", item.frame, "TOPLEFT", LABEL_WIDTH, 0);

	if ( tooltip ) then
		local enter = combo:GetScript("OnEnter");
		local leave = combo:GetScript("OnLeave");
		combo:SetScript("OnEnter", function(self)
			enter(self);
			NoggitTooltip_Show(self, tooltip);
		end);
		combo:SetScript("OnLeave", function(self)
			leave(self);
			NoggitTooltip_Hide();
		end);
	end

	item.combo = combo;
	item.refresh = function(data, info)
		if ( type(items) == "function" ) then
			combo.items = items(info, data) or {};
		end

		local value = data and data[key];
		local found;

		for index, entry in ipairs(combo.items) do
			if ( entry.value == value or (tonumber(value) and tonumber(entry.value) == tonumber(value)) ) then
				found = index;
				break;
			end
		end

		if ( found ) then
			NoggitComboBox_SetSelected(combo, found, true);
		else
			combo.text:SetText(data and format("%s (not in the list)", tostring(value)) or "");
		end

		combo:EnableMouse(data ~= nil);
	end;
	return item;
end

function ME.EnumItems(names, low, high)
	local items = {};

	for value = low, high do
		if ( names[value] ) then
			table.insert(items, { value = value, text = format("%d  %s", value, names[value]) });
		end
	end

	return items;
end

function ME.FlagsText(flags, value)
	local names = {};

	for _, flag in ipairs(flags) do
		if ( ME.HasBit(value, flag.bit) ) then
			table.insert(names, flag.name);
		end
	end

	local text = format("0x%X", Num(value));

	if ( #names > 0 ) then
		text = text.."  "..table.concat(names, ", ");
	end

	return text;
end

function Form:Flags(key, label, flags, bits, tooltip)
	local item = self:Item(ROW);
	RowLabel(item, label);
	local edit = Common.Button(item.frame, "Edit", 50, function()
		if ( self.data ) then
			GameEditor_EditFlags(label, flags, Num(self.data[key]), function(value)
				self:Set(key, value);
			end, bits or 32);
		end
	end, tooltip or "Toggle the bits one by one, every change applies at once");
	edit:SetPoint("TOPRIGHT", item.frame, "TOPRIGHT", 0, 0);
	local text = Common.Label(item.frame, "", nil, NOGGIT_COLOR_TEXT);
	text:SetPoint("TOPLEFT", item.frame, "TOPLEFT", LABEL_WIDTH, -4);
	text:SetPoint("RIGHT", edit, "LEFT", -6, 0);
	text:SetHeight(16);
	item.refresh = function(data)
		text:SetText(data and ME.FlagsText(flags, data[key]) or "");
		ME.SetEnabled(edit, data ~= nil);
	end;
	return item;
end

-- A game path typed into a line, Enter or leaving the field applies it.
function Form:Path(key, label, tooltip)
	local item = self:Item(ROW);
	RowLabel(item, label);
	local edit = NoggitLineEdit_Create(item.frame, self.width - LABEL_WIDTH);
	edit:SetHeight(22);
	edit:SetPoint("TOPLEFT", item.frame, "TOPLEFT", LABEL_WIDTH, 0);
	edit:SetMaxLetters(260);
	Common.Tooltip(edit, tooltip);

	local function Commit(box)
		if ( not self.data ) then
			return;
		end

		local text = ME.CleanPath(box:GetText());

		if ( text ~= (self.data[key] or "") ) then
			if ( text ~= "" ) then
				local exists, known = ME.FileExists(ME.ToM2(text));

				if ( known and not exists ) then
					ME.Notice(format("%s was not found, it is set anyway", text), "info");
				end
			end

			self:Set(key, text);
		end
	end

	edit:SetScript("OnEnterPressed", function(box) box:ClearFocus(); end);
	edit:SetScript("OnEditFocusLost", Commit);
	edit:SetScript("OnEscapePressed", function(box)
		box:SetText(self.data and self.data[key] or "");
		box:ClearFocus();
	end);
	item.refresh = function(data)
		if ( not edit:HasFocus() ) then
			edit:SetText(data and data[key] or "");
		end

		edit:EnableMouse(data ~= nil);
	end;
	return item;
end

-- Plain text, Enter or leaving the field applies it.
function Form:Text(key, label, tooltip)
	local item = self:Item(ROW);
	RowLabel(item, label);
	local edit = NoggitLineEdit_Create(item.frame, self.width - LABEL_WIDTH);
	edit:SetHeight(22);
	edit:SetPoint("TOPLEFT", item.frame, "TOPLEFT", LABEL_WIDTH, 0);
	edit:SetMaxLetters(0);
	Common.Tooltip(edit, tooltip);

	edit:SetScript("OnEnterPressed", function(box) box:ClearFocus(); end);
	edit:SetScript("OnEditFocusLost", function(box)
		if ( self.data and box:GetText() ~= tostring(self.data[key] or "") ) then
			self:Set(key, box:GetText());
		end
	end);
	edit:SetScript("OnEscapePressed", function(box)
		box:SetText(self.data and tostring(self.data[key] or "") or "");
		box:ClearFocus();
	end);
	item.refresh = function(data)
		if ( not edit:HasFocus() ) then
			edit:SetText(data and tostring(data[key] or "") or "");
		end

		edit:EnableMouse(data ~= nil);
	end;
	return item;
end

-- An id of a row somewhere else, the hint names it, ... searches and Go opens it where it is edited.
function Form:Ref(key, label, ref, tooltip, low, high)
	local item = self:Number(key, label, low or 0, high or 2147483647, 1, 0, tooltip);
	local anchor = item.spin;

	if ( GameEditor_CanSearchRef(ref) ) then
		local pick = Common.Button(item.frame, "...", 26, function()
			if ( self.data ) then
				GameEditor_PickValue(label, nil, Num(self.data[key]), function(value)
					self:Set(key, value);
				end, function(text)
					return GameEditor_SearchRef(ref, text);
				end);
			end
		end, "Search for one");
		pick:SetPoint("LEFT", anchor, "RIGHT", 4, 0);
		anchor = pick;
	end

	local go = Common.Button(item.frame, "Go", 32, function()
		if ( self.data ) then
			GameEditor_OpenRef(ref, self.data[key]);
		end
	end, "Open it in the workspace that edits it");
	go:SetPoint("LEFT", anchor, "RIGHT", 4, 0);

	item.hint:ClearAllPoints();
	item.hint:SetPoint("LEFT", go, "RIGHT", 8, 0);
	item.hint:SetPoint("RIGHT", item.frame, "RIGHT", 0, 0);
	item.hintFunc = function(value)
		return GameEditor_ResolveRef(ref, value) or "";
	end;

	local refresh = item.refresh;
	item.refresh = function(data, info)
		refresh(data, info);
		if ( GameEditor_CanOpenRef(ref) ) then
			go:Show();
		else
			go:Hide();
		end

		ME.SetEnabled(go, data ~= nil and Num(data[key]) ~= 0);
	end;
	return item;
end

-- Animated track: the value inline while it is constant, Keys opens the key window.
function Form:Track(key, label, kind, tooltip)
	local item = self:Item(ROW);
	RowLabel(item, label);

	local editor = ME.CreateValueEditor(item.frame, kind, function(value)
		if ( self.data ) then
			self:Set(key, ME.ConstantTrack(value, self.info, self.data[key]));
		end
	end);
	editor:SetPoint("TOPLEFT", item.frame, "TOPLEFT", LABEL_WIDTH, 0);
	editor.title = label;
	editor:SetTip((tooltip and (tooltip..". ") or "").."Changing it here makes the track one constant key in every sequence");

	local keys = Common.Button(item.frame, "Keys", 50, function()
		if ( not self.data ) then
			return;
		end

		local form = self;
		ME.EditTrack({
			title = label,
			kind = kind,
			get = function() return form.data and form.data[key]; end,
			set = function(track) form:Set(key, track); end,
		});
	end, "Edit the keys of each sequence");
	keys:SetHeight(20);
	keys:SetPoint("TOPRIGHT", item.frame, "TOPRIGHT", 0, -1);

	local summary = Common.Label(item.frame, "", nil, NOGGIT_COLOR_DISABLED);
	summary:SetPoint("TOPLEFT", item.frame, "TOPLEFT", LABEL_WIDTH + editor:GetWidth() + 8, -4);
	summary:SetPoint("RIGHT", keys, "LEFT", -4, 0);
	summary:SetHeight(16);

	local animated = Common.Label(item.frame, "", nil, NOGGIT_COLOR_GOLD);
	animated:SetPoint("TOPLEFT", item.frame, "TOPLEFT", LABEL_WIDTH, -4);
	animated:SetPoint("RIGHT", keys, "LEFT", -4, 0);
	animated:SetHeight(16);

	item.refresh = function(data)
		local track = data and data[key];
		local value, state = ME.TrackConstant(track);

		if ( state == "animated" ) then
			local count, sequences = ME.TrackKeyCount(track);
			editor:Hide();
			summary:SetText("");
			animated:SetText(format("Animated, %d keys in %d sequence%s%s", count, sequences, sequences == 1 and "" or "s", Num(track and track.globalSequence) >= 0 and (track.globalSequence ~= nil) and format(", global %d", track.globalSequence) or ""));
			animated:Show();
		else
			animated:Hide();
			editor:Show();
			editor:SetValue(value);
			editor:SetEnabledState(data ~= nil);
			summary:SetText(state == "empty" and "no keys" or "");
		end

		ME.SetEnabled(keys, data ~= nil);
	end;
	return item;
end

-- Particle lifetime track, a few keys with a time from 0 to 1 each.
-- A required track keeps at least one key, the C++ refuses an empty color, alpha or scale track.
function Form:PartTrack(key, label, kind, tooltip, required)
	local item = self:Item(24);
	RowLabel(item, label);

	local function Current()
		local track = self.data and self.data[key];
		return { times = ME.Copy(track and track.times or {}), values = ME.Copy(track and track.values or {}) };
	end

	local add = Common.Button(item.frame, "Add key", 64, function()
		local track = Current();

		if ( #track.times >= MAX_PART_KEYS ) then
			ME.Notice(format("This editor keeps at most %d keys", MAX_PART_KEYS), "info");
			return;
		end

		local last = #track.times;

		if ( last == 0 ) then
			table.insert(track.times, 0);
			table.insert(track.values, ME.ZeroValue(kind));
		elseif ( Num(track.times[last]) < 1 ) then
			table.insert(track.times, 1);
			table.insert(track.values, ME.Copy(track.values[last]));
		else
			local before = last > 1 and Num(track.times[last - 1]) or 0;
			table.insert(track.times, last, (before + Num(track.times[last])) / 2);
			table.insert(track.values, last, ME.Copy(track.values[last]));
		end

		self:Set(key, track);
	end, "Add a key, it goes in before the last key when that one is at 1");
	add:SetHeight(20);
	add:SetPoint("TOPRIGHT", item.frame, "TOPRIGHT", 0, -1);

	local remove = Common.Button(item.frame, "Remove key", 80, function()
		local track = Current();

		if ( required and #track.times <= 1 ) then
			ME.Notice(format("%s needs at least one key", label), "info");
			return;
		end

		if ( #track.times > 0 ) then
			table.remove(track.times);
			table.remove(track.values);
			self:Set(key, track);
		end
	end, required and "Remove the last key, one key always stays" or "Remove the last key");
	remove:SetHeight(20);
	remove:SetPoint("RIGHT", add, "LEFT", -4, 0);

	local empty = Common.Label(item.frame, "", nil, NOGGIT_COLOR_DISABLED);
	empty:SetPoint("TOPLEFT", item.frame, "TOPLEFT", LABEL_WIDTH, -4);
	empty:SetPoint("RIGHT", remove, "LEFT", -4, 0);

	if ( tooltip ) then
		Common.Tooltip(add, tooltip..", Add key puts in another key");
	end

	local rows = {};

	for index = 1, MAX_PART_KEYS do
		local row = CreateFrame("Frame", NoggitUI_NewName("GameEditorModelPartKey"), item.frame);
		row:SetHeight(24);
		row:SetPoint("TOPLEFT", item.frame, "TOPLEFT", 0, -24 * index);
		row:SetPoint("TOPRIGHT", item.frame, "TOPRIGHT", 0, -24 * index);
		row.label = Common.Label(row, format("Key %d at", index), nil, NOGGIT_COLOR_DISABLED);
		row.label:SetPoint("LEFT", row, "LEFT", 16, 0);
		row.time = ME.Spin(row, 70, 0, 1, 0.05, 3, function(value)
			local track = Current();

			if ( track.times[index] ) then
				local before = track.times[index - 1];
				local after = track.times[index + 1];

				if ( (before and value <= Num(before)) or (after and value >= Num(after)) ) then
					ME.Notice("Key times must go up from one key to the next", "info");
					self:Refresh(self.data, self.info);
					return;
				end

				track.times[index] = value;
				self:Set(key, track);
			end
		end, "Point in the particle life from 0 (born) to 1 (dies), each key later than the one before");
		row.time:SetPoint("LEFT", row, "LEFT", LABEL_WIDTH - 74, 0);
		row.value = ME.CreateValueEditor(row, kind, function(value)
			local track = Current();

			if ( track.times[index] ) then
				track.values[index] = value;
				self:Set(key, track);
			end
		end);
		row.value:SetPoint("LEFT", row, "LEFT", LABEL_WIDTH, 0);
		row.value.title = format("%s key %d", label, index);
		rows[index] = row;
	end

	item.refresh = function(data)
		local track = data and data[key];
		local times = track and track.times or {};
		local count = min(#times, MAX_PART_KEYS);
		empty:SetText(#times == 0 and "no keys" or (#times > MAX_PART_KEYS and format("%d keys, the first %d show", #times, MAX_PART_KEYS) or ""));

		for index, row in ipairs(rows) do
			if ( index <= count ) then
				row:Show();
				ME.SetSpin(row.time, times[index], true);
				row.value:SetValue(track.values and track.values[index]);
				row.value:SetEnabledState(true);
			else
				row:Hide();
			end
		end

		ME.SetEnabled(add, data ~= nil);
		ME.SetEnabled(remove, data ~= nil and #times > (required and 1 or 0));
		item.height = 24 + count * 24;
	end;
	return item;
end

-- Ribbon textures and materials, two lists that the client reads side by side.
function Form:RibbonTextures(textureKey, materialKey, label)
	local item = self:Item(24);
	RowLabel(item, label);

	local function Current()
		local data = self.data or {};
		return ME.Copy(data[textureKey] or {}), ME.Copy(data[materialKey] or {});
	end

	local function Store(textures, materials)
		if ( self.data and self.onSet ) then
			self.onSet({ [textureKey] = textures, [materialKey] = materials });
		end
	end

	local add = Common.Button(item.frame, "Add", 50, function()
		local textures, materials = Current();

		if ( #textures >= MAX_RIBBON_TEXTURES ) then
			ME.Notice(format("This editor keeps at most %d ribbon textures", MAX_RIBBON_TEXTURES), "info");
			return;
		end

		table.insert(textures, textures[#textures] or 0);
		table.insert(materials, materials[#materials] or 0);
		Store(textures, materials);
	end, "Add a texture and material pair");
	add:SetHeight(20);
	add:SetPoint("TOPRIGHT", item.frame, "TOPRIGHT", 0, -1);

	local remove = Common.Button(item.frame, "Remove", 64, function()
		local textures, materials = Current();

		if ( #textures > 1 ) then
			table.remove(textures);
			table.remove(materials);
			Store(textures, materials);
		end
	end, "Remove the last pair, a ribbon keeps at least one");
	remove:SetHeight(20);
	remove:SetPoint("RIGHT", add, "LEFT", -4, 0);

	local rows = {};

	for index = 1, MAX_RIBBON_TEXTURES do
		local row = CreateFrame("Frame", NoggitUI_NewName("GameEditorModelRibbonTexture"), item.frame);
		row:SetHeight(24);
		row:SetPoint("TOPLEFT", item.frame, "TOPLEFT", 0, -24 * index);
		row:SetPoint("TOPRIGHT", item.frame, "TOPRIGHT", 0, -24 * index);
		row.label = Common.Label(row, format("%d texture", index), nil, NOGGIT_COLOR_DISABLED);
		row.label:SetPoint("LEFT", row, "LEFT", 16, 0);
		row.texture = NoggitComboBox_Create(row, 180, { { value = 0, text = "" } }, 1, function(_, _, entry)
			local textures, materials = Current();

			if ( textures[index] ~= nil ) then
				textures[index] = entry.value;
				Store(textures, materials);
			end
		end);
		row.texture:SetPoint("LEFT", row, "LEFT", LABEL_WIDTH - 60, 0);
		row.materialLabel = Common.Label(row, "material", nil, NOGGIT_COLOR_DISABLED);
		row.materialLabel:SetPoint("LEFT", row.texture, "RIGHT", 8, 0);
		row.material = ME.Spin(row, 60, 0, 65535, 1, 0, function(value)
			local textures, materials = Current();

			if ( materials[index] ~= nil ) then
				materials[index] = value;
				Store(textures, materials);
			end
		end, "Material index, the Textures tab lists the materials");
		row.material:SetPoint("LEFT", row.materialLabel, "RIGHT", 6, 0);
		row.materialText = Common.Label(row, "", nil, NOGGIT_COLOR_DISABLED);
		row.materialText:SetPoint("LEFT", row.material, "RIGHT", 6, 0);
		row.materialText:SetPoint("RIGHT", row, "RIGHT", 0, 0);
		rows[index] = row;
	end

	item.refresh = function(data, info)
		local textures = data and data[textureKey] or {};
		local materials = data and data[materialKey] or {};
		local count = min(#textures, MAX_RIBBON_TEXTURES);
		local items = ME.TextureItems(info);

		for index, row in ipairs(rows) do
			if ( index <= count ) then
				row:Show();
				row.texture.items = items;
				local found;

				for itemIndex, entry in ipairs(items) do
					if ( entry.value == Num(textures[index]) ) then
						found = itemIndex;
					end
				end

				if ( found ) then
					NoggitComboBox_SetSelected(row.texture, found, true);
				else
					row.texture.text:SetText(format("%s (missing)", tostring(textures[index])));
				end

				ME.SetSpin(row.material, materials[index], true);
				local material = ME.MaterialByIndex(info, Num(materials[index]));
				row.materialText:SetText(material and (ME.BLEND_MODES[Num(material.blendMode)] or "") or "missing");
			else
				row:Hide();
			end
		end

		ME.SetEnabled(add, data ~= nil);
		ME.SetEnabled(remove, data ~= nil and #textures > 1);
		item.height = 24 + count * 24;
	end;
	return item;
end

function ME.TextureItems(info)
	local items = {};

	for _, texture in ipairs(info and info.textures or {}) do
		table.insert(items, { value = texture.index, text = ME.TextureText(info, texture.index) });
	end

	if ( #items == 0 ) then
		table.insert(items, { value = 0, text = "0 (no textures)" });
	end

	return items;
end

function ME.MaterialByIndex(info, index)
	for _, material in ipairs(info and info.materials or {}) do
		if ( material.index == index ) then
			return material;
		end
	end
end

-- Track window -----------------------------------------------------------------------------------------------------

local trackWindow;
local ROW_HEIGHT = 24;

local function Window()
	return trackWindow;
end

local function SortSequence(sequence)
	local keys = {};

	for index, time in ipairs(sequence.times or {}) do
		table.insert(keys, { time = Num(time), value = sequence.values and sequence.values[index] });
	end

	table.sort(keys, function(a, b) return a.time < b.time; end);
	sequence.times = {};
	sequence.values = {};

	for _, pair in ipairs(keys) do
		table.insert(sequence.times, pair.time);
		table.insert(sequence.values, pair.value);
	end
end

local function FindSequence(track, index)
	for position, sequence in ipairs(track.sequences or {}) do
		if ( sequence.sequence == index ) then
			return sequence, position;
		end
	end
end

local function EnsureSequence(track, index)
	track.sequences = track.sequences or {};
	local sequence = FindSequence(track, index);

	if ( not sequence ) then
		sequence = { sequence = index, times = {}, values = {} };
		table.insert(track.sequences, sequence);
		table.sort(track.sequences, function(a, b) return Num(a.sequence) < Num(b.sequence); end);
	end

	return sequence;
end

local function DropEmpty(track)
	for position = #(track.sequences or {}), 1, -1 do
		if ( #(track.sequences[position].times or {}) == 0 ) then
			table.remove(track.sequences, position);
		end
	end
end

local function TrackCopy()
	local window = Window();
	local track = window.options and window.options.get();
	track = ME.Copy(track) or {};
	track.interpolation = Num(track.interpolation);
	track.globalSequence = track.globalSequence == nil and -1 or Num(track.globalSequence);
	track.sequences = track.sequences or {};
	return track;
end

local function StoreTrack(track)
	local window = Window();

	if ( window.options ) then
		DropEmpty(track);
		window.options.set(track);
	end
end

local function TrackInfo()
	return ME.current and ME.Info(ME.current);
end

local function SequenceDuration(index)
	local sequence = ME.SequenceByIndex(TrackInfo(), index);
	return sequence and Num(sequence.duration) or 0;
end

local function RowEditor(row, kind)
	row.editors = row.editors or {};

	if ( not row.editors[kind] ) then
		local editor = ME.CreateValueEditor(row, kind, function(value)
			local track = TrackCopy();
			local sequence = EnsureSequence(track, Window().sequence);

			if ( sequence.times[row.index] ~= nil ) then
				sequence.values[row.index] = value;
				StoreTrack(track);
			end
		end);
		editor:SetPoint("LEFT", row.time, "RIGHT", 8, 0);
		row.editors[kind] = editor;
	end

	for other, editor in pairs(row.editors) do
		if ( other == kind ) then
			editor:Show();
		else
			editor:Hide();
		end
	end

	return row.editors[kind];
end

local function CreateTrackRow(window, index)
	local row = CreateFrame("Frame", NoggitUI_NewName("GameEditorModelTrackKey"), window.child);
	row:SetHeight(ROW_HEIGHT);
	row:SetPoint("TOPLEFT", window.child, "TOPLEFT", 0, -(index - 1) * ROW_HEIGHT);
	row:SetPoint("TOPRIGHT", window.child, "TOPRIGHT", 0, -(index - 1) * ROW_HEIGHT);
	row.index = index;

	row.number = Common.Label(row, tostring(index), nil, NOGGIT_COLOR_DISABLED);
	row.number:SetPoint("LEFT", row, "LEFT", 0, 0);
	row.number:SetWidth(22);

	row.time = ME.Spin(row, 80, 0, 100000000, 33, 0, function(value)
		local track = TrackCopy();
		local sequence = EnsureSequence(track, window.sequence);

		if ( sequence.times[index] ~= nil ) then
			sequence.times[index] = floor(value + 0.5);
			SortSequence(sequence);
			StoreTrack(track);
		end
	end, "Time of the key in milliseconds from the start of the sequence");
	row.time:SetPoint("LEFT", row, "LEFT", 24, 0);

	row.remove = Common.Button(row, "Remove", 64, function()
		local track = TrackCopy();
		local sequence = EnsureSequence(track, window.sequence);

		if ( sequence.times[index] ~= nil ) then
			table.remove(sequence.times, index);
			table.remove(sequence.values, index);
			StoreTrack(track);
		end
	end, "Remove this key");
	row.remove:SetHeight(20);
	row.remove:SetPoint("RIGHT", row, "RIGHT", -2, 0);

	return row;
end

local function ConstantEditor(window, kind)
	window.constEditors = window.constEditors or {};

	if ( not window.constEditors[kind] ) then
		local editor = ME.CreateValueEditor(window.constRow, kind, function()
			window.constTouched = true;
		end);
		editor:SetPoint("LEFT", window.constLabel, "RIGHT", 8, 0);
		window.constEditors[kind] = editor;
	end

	for other, editor in pairs(window.constEditors) do
		if ( other == kind ) then
			editor:Show();
		else
			editor:Hide();
		end
	end

	return window.constEditors[kind];
end

local function PickSequence()
	local window = Window();
	local info = TrackInfo();
	local track = window.options and window.options.get() or {};
	local items = {};

	for _, sequence in ipairs(info and info.sequences or {}) do
		local data = FindSequence(track, sequence.index);
		local keys = data and #(data.times or {}) or 0;
		local extra = keys > 0 and format(", %d keys", keys) or "";

		if ( sequence.external ) then
			extra = extra..", in a .anim file, not editable";
		end

		if ( ME.IsAlias(sequence) ) then
			extra = extra..", alias";
		end

		table.insert(items, { value = sequence.index, text = format("%s, %d ms%s", ME.SequenceText(sequence), Num(sequence.duration), extra) });
	end

	GameEditor_PickValue("Sequence", items, window.sequence, function(value)
		window.sequence = Num(value);
		ME.RefreshTrackWindow();
	end);
end

local function CreateTrackWindow()
	local window = GameEditor_CreateWindow("GameEditorModelTrackWindow", "Track", 500, 470, true);
	trackWindow = window;
	local c = window.content;
	window.rows = {};
	window.sequence = 0;

	local interpLabel = Common.Label(c, "Interpolation");
	interpLabel:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -4);
	local interpItems = ME.EnumItems(ME.INTERPOLATION, 0, 3);
	window.interp = NoggitComboBox_Create(c, 130, interpItems, 1, function(_, _, entry)
		local track = TrackCopy();
		track.interpolation = entry.value;
		StoreTrack(track);
	end);
	window.interp:SetPoint("TOPLEFT", c, "TOPLEFT", 100, 0);

	local globalLabel = Common.Label(c, "Global sequence");
	globalLabel:SetPoint("LEFT", window.interp, "RIGHT", 14, 0);
	window.global = ME.Spin(c, 70, -1, 65535, 1, 0, function(value)
		local track = TrackCopy();
		track.globalSequence = floor(value);
		StoreTrack(track);
	end, "-1 runs the keys with each sequence, a number runs them on that global timeline whatever plays");
	window.global:SetPoint("LEFT", globalLabel, "RIGHT", 8, 0);

	local seqLabel = Common.Label(c, "Sequence");
	seqLabel:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -32);
	window.seqButton = Common.Button(c, "", 300, PickSequence, "Pick the sequence whose keys show below");
	window.seqButton:SetPoint("TOPLEFT", c, "TOPLEFT", 100, -28);
	window.seqInfo = Common.Label(c, "", nil, NOGGIT_COLOR_DISABLED);
	window.seqInfo:SetPoint("LEFT", window.seqButton, "RIGHT", 8, 0);
	window.seqInfo:SetPoint("RIGHT", c, "RIGHT", 0, 0);

	window.add = Common.Button(c, "Add key", 80, function()
		local track = TrackCopy();
		local sequence = EnsureSequence(track, window.sequence);
		local count = #sequence.times;
		local time = count > 0 and min(sequence.times[count] + 100, max(sequence.times[count], SequenceDuration(window.sequence))) or 0;

		if ( count > 0 and time == sequence.times[count] ) then
			time = sequence.times[count] + 1;
		end

		table.insert(sequence.times, time);
		table.insert(sequence.values, count > 0 and ME.Copy(sequence.values[count]) or ME.ZeroValue(window.options.kind));
		SortSequence(sequence);
		StoreTrack(track);
	end, "Add a key after the last one with the same value");
	window.add:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -58);

	window.copyAll = Common.Button(c, "Copy to every sequence", 160, function()
		local track = TrackCopy();
		local source = FindSequence(track, window.sequence);

		if ( not source or #(source.times or {}) == 0 ) then
			ME.Notice("This sequence has no keys to copy", "info");
			return;
		end

		for _, sequence in ipairs(TrackInfo() and TrackInfo().sequences or {}) do
			if ( sequence.index ~= window.sequence and ME.IsEditableSequence(sequence) ) then
				local target = EnsureSequence(track, sequence.index);
				target.times = ME.Copy(source.times);
				target.values = ME.Copy(source.values);
			end
		end

		StoreTrack(track);
	end, "Give every other sequence the keys of this one, sequences in .anim files and aliases are skipped");
	window.copyAll:SetPoint("LEFT", window.add, "RIGHT", 6, 0);

	window.clear = Common.Button(c, "Clear sequence", 110, function()
		local track = TrackCopy();
		local _, position = FindSequence(track, window.sequence);

		if ( position ) then
			table.remove(track.sequences, position);
			StoreTrack(track);
		end
	end, "Remove every key of this sequence");
	window.clear:SetPoint("LEFT", window.copyAll, "RIGHT", 6, 0);

	window.constRow = CreateFrame("Frame", NoggitUI_NewName("GameEditorModelTrackConst"), c);
	window.constRow:SetHeight(24);
	window.constRow:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -86);
	window.constRow:SetPoint("TOPRIGHT", c, "TOPRIGHT", 0, -86);
	window.constLabel = Common.Label(window.constRow, "Constant");
	window.constLabel:SetPoint("LEFT", window.constRow, "LEFT", 0, 0);
	window.constApply = Common.Button(window.constRow, "Set constant", 100, function()
		local editor = ConstantEditor(window, window.options.kind);
		window.constTouched = false;
		StoreTrack(ME.ConstantTrack(editor.value, TrackInfo(), TrackCopy()));
	end, "Replace the whole track with one key at time 0 in every sequence, sequences in .anim files and aliases are skipped");
	window.constApply:SetHeight(20);
	window.constApply:SetPoint("RIGHT", window.constRow, "RIGHT", 0, 0);

	local header = Common.Label(c, "Key  Time (ms)      Value", nil, NOGGIT_COLOR_DISABLED);
	header:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -118);

	window.scroll, window.child = GameEditor_CreateScrollFrame(c);
	window.scroll:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -134);
	window.scroll:SetPoint("BOTTOMRIGHT", c, "BOTTOMRIGHT", 0, 0);

	window.empty = Common.Paragraph(window.child, "");
	window.empty:SetPoint("TOPLEFT", window.child, "TOPLEFT", 2, -4);
	window.empty:SetPoint("TOPRIGHT", window.child, "TOPRIGHT", -2, -4);

	window.onClose = function()
		window.options = nil;
	end;
end

function ME.RefreshTrackWindow()
	local window = trackWindow;

	if ( not window or not window:IsShown() or not window.options ) then
		return;
	end

	local options = window.options;
	local track = options.get();
	local info = TrackInfo();

	if ( type(track) ~= "table" ) then
		window.empty:SetText("The track is not available any more, pick the field again.");
		window.empty:Show();

		for _, row in ipairs(window.rows) do
			row:Hide();
		end

		return;
	end

	NoggitComboBox_SetSelected(window.interp, Num(track.interpolation) + 1, true);
	ME.SetSpin(window.global, track.globalSequence == nil and -1 or track.globalSequence, true);

	local sequenceInfo = ME.SequenceByIndex(info, window.sequence);
	window.seqButton:SetText(ME.SequenceText(sequenceInfo) or tostring(window.sequence));
	window.seqInfo:SetText(sequenceInfo and format("%d ms%s", Num(sequenceInfo.duration), sequenceInfo.external and ", .anim" or "") or "");

	local editable = not sequenceInfo or ME.IsEditableSequence(sequenceInfo);
	ME.SetEnabled(window.add, editable);
	ME.SetEnabled(window.copyAll, editable);
	ME.SetEnabled(window.clear, editable);

	local sequence = FindSequence(track, window.sequence);
	local times = sequence and sequence.times or {};
	local values = sequence and sequence.values or {};

	local constant = ConstantEditor(window, options.kind);
	local value = ME.TrackConstant(track);

	if ( not window.constTouched ) then
		constant:SetValue(value or values[1]);
	end

	for index = 1, #times do
		local row = window.rows[index];

		if ( not row ) then
			row = CreateTrackRow(window, index);
			window.rows[index] = row;
		end

		row:Show();
		ME.SetSpin(row.time, times[index], true);
		local editor = RowEditor(row, options.kind);
		editor.title = format("%s key %d", options.title or "Track", index);
		editor:SetValue(values[index]);
		editor:SetEnabledState(editable);
		row.time:SetEnabledState(editable);
		ME.SetEnabled(row.remove, editable);
	end

	for index = #times + 1, #window.rows do
		window.rows[index]:Hide();
	end

	if ( sequenceInfo and sequenceInfo.external ) then
		window.empty:SetText("This sequence lives in a .anim file. Its keys are not loaded and cannot be edited here, pick another sequence.");
		window.empty:Show();
	elseif ( sequenceInfo and ME.IsAlias(sequenceInfo) ) then
		window.empty:SetText("This sequence is an alias and plays the keys of another sequence, edit that one instead.");
		window.empty:Show();
	elseif ( #times == 0 ) then
		local count = ME.TrackKeyCount(track);
		window.empty:SetText(count > 0 and "This sequence has no keys, the track uses its default value while it plays. Add a key or copy keys from another sequence." or "The track has no keys at all. Set constant gives it one value, Add key starts keys for this sequence.");
		window.empty:Show();
	else
		window.empty:Hide();
	end

	window.child:SetHeight(max(40, #times * ROW_HEIGHT + 4));
	window.scroll.UpdateRange();
end

-- options: title, kind, get() returns the current track, set(track) replaces it.
function ME.EditTrack(options)
	if ( not trackWindow ) then
		CreateTrackWindow();
	end

	local window = trackWindow;
	window.options = options;
	window.constTouched = false;
	window:SetTitle(format("Track: %s", options.title or ""));

	local track = options.get() or {};
	local info = TrackInfo();
	local first;

	for _, sequence in ipairs(track.sequences or {}) do
		if ( #(sequence.times or {}) > 0 ) then
			first = sequence.sequence;
			break;
		end
	end

	if ( not first ) then
		local preview = ME.previewSequence and ME.SequenceByIndex(info, ME.previewSequence);

		if ( preview and ME.IsEditableSequence(preview) ) then
			first = preview.index;
		else
			for _, sequence in ipairs(info and info.sequences or {}) do
				if ( ME.IsEditableSequence(sequence) ) then
					first = sequence.index;
					break;
				end
			end
		end
	end

	window.sequence = first or 0;
	GameEditor_ShowWindow(window);
	ME.RefreshTrackWindow();
end

function ME.HideTrackWindow()
	if ( trackWindow and trackWindow:IsShown() ) then
		GameEditor_HideWindow(trackWindow);
	end
end

ME.OnChanged(function()
	ME.RefreshTrackWindow();
end);
