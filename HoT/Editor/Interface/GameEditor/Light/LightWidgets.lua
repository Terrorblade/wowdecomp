-- Widgets of the lights workspace: day strips, the color picker and the band key editor.

local LE = GameEditorLight;
local Common = GameEditorObject;

-- Day strip ------------------------------------------------------------------------------------------------

-- Stops are { time, r, g, b } sorted by time with 0..1 channels, the last segment wraps past midnight like the client.
local function StripSegments(stops)
	local segments = {};
	local count = #stops;

	if ( count == 0 ) then
		return segments;
	end

	if ( count == 1 ) then
		local s = stops[1];
		table.insert(segments, { 0, LE.DAY, s[2], s[3], s[4], s[2], s[3], s[4] });
		return segments;
	end

	for i = 1, count - 1 do
		local a, b = stops[i], stops[i + 1];
		table.insert(segments, { a[1], b[1], a[2], a[3], a[4], b[2], b[3], b[4] });
	end

	local last, first = stops[count], stops[1];
	local span = first[1] + LE.DAY - last[1];
	local t = span > 0 and (LE.DAY - last[1]) / span or 0;
	local mr = last[2] + (first[2] - last[2]) * t;
	local mg = last[3] + (first[3] - last[3]) * t;
	local mb = last[4] + (first[4] - last[4]) * t;
	table.insert(segments, { last[1], LE.DAY, last[2], last[3], last[4], mr, mg, mb });
	table.insert(segments, { 0, first[1], mr, mg, mb, first[2], first[3], first[4] });
	return segments;
end

function LE.CreateStrip(parent, width, height)
	local strip = CreateFrame("Button", NoggitUI_NewName("GameEditorLightStrip"), parent, "NoggitBoxTemplate");
	strip:SetWidth(width);
	strip:SetHeight(height);
	NoggitBox_Set(strip, NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID);
	strip.textures = {};
	strip.ticks = {};

	strip.marker = NoggitUI_SolidTexture(strip, "OVERLAY", NOGGIT_COLOR_WHITE);
	strip.marker:SetWidth(1);
	strip.marker:SetPoint("TOP", strip, "TOP", 0, -1);
	strip.marker:SetPoint("BOTTOM", strip, "BOTTOM", 0, 1);

	strip.SetStops = function(self, stops, showTicks)
		local inner = self:GetWidth() - 2;
		local segments = StripSegments(stops);
		local used = 0;

		for _, segment in ipairs(segments) do
			if ( segment[2] > segment[1] ) then
				used = used + 1;
				local texture = self.textures[used];

				if ( not texture ) then
					texture = self:CreateTexture(nil, "ARTWORK");
					self.textures[used] = texture;
				end

				local x = segment[1] / LE.DAY * inner;
				texture:ClearAllPoints();
				texture:SetPoint("TOPLEFT", self, "TOPLEFT", 1 + x, -1);
				texture:SetPoint("BOTTOMLEFT", self, "BOTTOMLEFT", 1 + x, 1);
				texture:SetWidth(max(1, (segment[2] - segment[1]) / LE.DAY * inner));
				texture:SetTexture(1, 1, 1, 1);
				texture:SetGradient("HORIZONTAL", segment[3], segment[4], segment[5], segment[6], segment[7], segment[8]);
				texture:Show();
			end
		end

		for i = used + 1, #self.textures do
			self.textures[i]:Hide();
		end

		local ticks = showTicks and stops or {};

		for i, stop in ipairs(ticks) do
			local tick = self.ticks[i];

			if ( not tick ) then
				tick = NoggitUI_SolidTexture(self, "OVERLAY", NOGGIT_COLOR_GOLD);
				tick:SetWidth(2);
				tick:SetHeight(5);
				self.ticks[i] = tick;
			end

			tick:ClearAllPoints();
			tick:SetPoint("BOTTOMLEFT", self, "BOTTOMLEFT", stop[1] / LE.DAY * inner, 1);
			tick:Show();
		end

		for i = #ticks + 1, #self.ticks do
			self.ticks[i]:Hide();
		end
	end;

	strip.SetTime = function(self, time)
		if ( time ) then
			self.marker:ClearAllPoints();
			self.marker:SetPoint("TOP", self, "TOPLEFT", 1 + time / LE.DAY * (self:GetWidth() - 2), -1);
			self.marker:SetPoint("BOTTOM", self, "BOTTOMLEFT", 1 + time / LE.DAY * (self:GetWidth() - 2), 1);
			self.marker:Show();
		else
			self.marker:Hide();
		end
	end;

	-- Half minutes under the cursor.
	strip.TimeAtCursor = function(self)
		local x = NoggitUI_CursorIn(self);
		local fraction = (x - self:GetLeft() - 1) / max(1, self:GetWidth() - 2);
		return max(0, min(LE.DAY - 1, floor(fraction * LE.DAY + 0.5)));
	end;

	return strip;
end

-- Stops of a band for a strip, colors as they are and numbers as gray between low and high.
function LE.ColorStops(keys)
	local stops = {};

	for _, key in ipairs(keys) do
		local r, g, b = LE.SplitColor(key.value);
		table.insert(stops, { key.time, r / 255, g / 255, b / 255 });
	end

	return stops;
end

function LE.FloatStops(keys)
	local low, high;

	for _, key in ipairs(keys) do
		low = low and min(low, key.value) or key.value;
		high = high and max(high, key.value) or key.value;
	end

	local stops = {};

	for _, key in ipairs(keys) do
		local gray = (high and high > low) and 0.15 + 0.85 * (key.value - low) / (high - low) or 0.6;
		table.insert(stops, { key.time, gray, gray, gray });
	end

	return stops, low, high;
end

-- Color picker ---------------------------------------------------------------------------------------------

local picker;

local function PickerColor()
	return picker.channels[1]:GetValue(), picker.channels[2]:GetValue(), picker.channels[3]:GetValue();
end

local function PickerRefresh(fromHex)
	local r, g, b = PickerColor();
	picker.swatch.color:SetTexture(r / 255, g / 255, b / 255, 1);

	if ( not fromHex ) then
		picker.hex:SetText(format("%02X%02X%02X", r, g, b));
	end
end

local function PickerChanged()
	PickerRefresh();

	if ( picker.onChange ) then
		picker.onChange(PickerColor());
	end
end

local function CreatePicker()
	picker = GameEditor_CreateWindow("GameEditorLightColorPicker", "Color", 300, 250, false);
	local content = picker.content;
	picker.channels = {};

	local names = { "Red:", "Green:", "Blue:" };

	for i = 1, 3 do
		local slider = NoggitExtendedSlider_Create(content, 288, names[i], 0, 255, 1, 0, PickerChanged);
		slider:SetPoint("TOPLEFT", content, "TOPLEFT", 0, -(i - 1) * 48);
		picker.channels[i] = slider;
	end

	picker.swatch = CreateFrame("Frame", NoggitUI_NewName("GameEditorLightSwatch"), content, "NoggitBoxTemplate");
	picker.swatch:SetWidth(80);
	picker.swatch:SetHeight(22);
	picker.swatch:SetPoint("TOPLEFT", content, "TOPLEFT", 0, -148);
	NoggitBox_Set(picker.swatch, nil, NOGGIT_COLOR_HOVER);
	picker.swatch.color = picker.swatch:CreateTexture(nil, "ARTWORK");
	picker.swatch.color:SetPoint("TOPLEFT", picker.swatch, "TOPLEFT", 2, -2);
	picker.swatch.color:SetPoint("BOTTOMRIGHT", picker.swatch, "BOTTOMRIGHT", -2, 2);

	picker.hex = NoggitLineEdit_Create(content, 90);
	picker.hex:SetPoint("LEFT", picker.swatch, "RIGHT", 8, 0);
	picker.hex:SetScript("OnEnterPressed", function(self)
		self:ClearFocus();
		local text = string.match(self:GetText() or "", "^%s*#?(%x%x%x%x%x%x)%s*$");

		if ( text ) then
			local value = tonumber(text, 16);
			local r, g, b = LE.SplitColor(value);
			picker.channels[1]:SetValue(r);
			picker.channels[2]:SetValue(g);
			picker.channels[3]:SetValue(b);
			PickerChanged();
		else
			PickerRefresh();
		end
	end);

	local ok = Common.Button(content, "OK", 70, function()
		picker.initial = nil;
		GameEditor_HideWindow(picker);
	end);
	ok:SetPoint("BOTTOMRIGHT", content, "BOTTOMRIGHT", 0, 0);

	local cancel = Common.Button(content, "Cancel", 70, function()
		GameEditor_HideWindow(picker);
	end, "Put the color back the way it was");
	cancel:SetPoint("RIGHT", ok, "LEFT", -6, 0);

	-- Closing without OK puts the first color back.
	picker.onClose = function()
		local initial = picker.initial;
		picker.initial = nil;

		if ( initial and picker.onChange ) then
			picker.onChange(initial[1], initial[2], initial[3]);
		end
	end;
end

-- LE.PickColor(r, g, b, onChange) calls onChange(r, g, b) on every move, 0..255 channels.
function LE.PickColor(r, g, b, onChange, title)
	if ( not picker ) then
		CreatePicker();
	end

	if ( picker:IsShown() ) then
		picker.initial = nil;
		GameEditor_HideWindow(picker);
	end

	picker.onChange = nil;
	picker.initial = { r, g, b };
	picker:SetTitle(title or "Color");
	picker.channels[1]:SetValue(r);
	picker.channels[2]:SetValue(g);
	picker.channels[3]:SetValue(b);
	PickerRefresh();
	picker.onChange = onChange;
	GameEditor_ShowWindow(picker);
end

function LE.HideColorPicker()
	if ( picker and picker:IsShown() ) then
		picker.initial = nil;
		GameEditor_HideWindow(picker);
	end
end

-- Band key editor ------------------------------------------------------------------------------------------

local ROW_HEIGHT = 24;

local function CreateKeyRow(editor, index)
	local row = CreateFrame("Frame", NoggitUI_NewName("GameEditorLightKey"), editor);
	row:SetHeight(ROW_HEIGHT);
	row:SetPoint("TOPLEFT", editor, "TOPLEFT", 0, -60 - (index - 1) * ROW_HEIGHT);
	row:SetPoint("TOPRIGHT", editor, "TOPRIGHT", 0, -60 - (index - 1) * ROW_HEIGHT);
	row.index = index;

	row.number = Common.Label(row, tostring(index), nil, NOGGIT_COLOR_DISABLED);
	row.number:SetPoint("LEFT", row, "LEFT", 0, 0);
	row.number:SetWidth(20);

	row.time = NoggitLineEdit_Create(row, 80);
	row.time:SetPoint("LEFT", row, "LEFT", 22, 0);
	row.time:SetHeight(22);

	local function CommitTime(self)
		local time = LE.ParseTime(self:GetText());
		local keys = editor:Keys();
		local key = keys[row.index];

		if ( time and key and time ~= key.time ) then
			key.time = time;
			editor:Store(keys);
		else
			editor:Refresh();
		end
	end

	row.time:SetScript("OnEnterPressed", function(self)
		self:ClearFocus();
	end);
	row.time:SetScript("OnEditFocusLost", CommitTime);
	Common.Tooltip(row.time, "Time of day as HH:MM, add :30 for the half minute");

	if ( editor.isColor ) then
		row.swatch = CreateFrame("Button", NoggitUI_NewName("GameEditorLightKeySwatch"), row, "NoggitBoxTemplate");
		row.swatch:SetWidth(120);
		row.swatch:SetHeight(22);
		row.swatch:SetPoint("LEFT", row.time, "RIGHT", 6, 0);
		NoggitBox_Set(row.swatch, nil, NOGGIT_COLOR_HOVER);
		row.swatch.color = row.swatch:CreateTexture(nil, "ARTWORK");
		row.swatch.color:SetPoint("TOPLEFT", row.swatch, "TOPLEFT", 2, -2);
		row.swatch.color:SetPoint("BOTTOMRIGHT", row.swatch, "BOTTOMRIGHT", -2, 2);
		row.swatch.text = NoggitUI_FontString(row.swatch, "", nil, "CENTER");
		row.swatch.text:SetPoint("CENTER", row.swatch, "CENTER", 0, 0);
		row.swatch:SetScript("OnClick", function()
			local keys = editor:Keys();
			local key = keys[row.index];

			if ( not key ) then
				return;
			end

			local r, g, b = LE.SplitColor(key.value);
			local doc, index = editor.doc, row.index;

			LE.PickColor(r, g, b, function(nr, ng, nb)
				if ( editor.doc ~= doc ) then
					return;
				end

				local current = editor:Keys();

				if ( current[index] ) then
					current[index].value = LE.JoinColor(nr, ng, nb);
					editor:Store(current, true);
				end
			end, format("%s, key %d", editor.title or "Color", index));
		end);
	else
		row.value = NoggitSpinBox_Create(row, 120, -1000000, 1000000, 0.01, 3, function(_, value)
			local keys = editor:Keys();
			local key = keys[row.index];

			if ( key and not LE.Same(key.value, value) ) then
				key.value = value;
				editor:Store(keys, true);
			end
		end);
		row.value:SetPoint("LEFT", row.time, "RIGHT", 6, 0);
	end

	row.remove = Common.Button(row, "Remove", 64, function()
		local keys = editor:Keys();

		if ( keys[row.index] ) then
			table.remove(keys, row.index);
			editor:Store(keys);
		end
	end, "Remove this key");
	row.remove:SetPoint("RIGHT", row, "RIGHT", 0, 0);

	return row;
end

-- options: isColor, width. editor:SetBand(doc or nil, title, emptyText), editor:Refresh(), editor.getTime() for new keys.
function LE.CreateKeyEditor(parent, options)
	local editor = CreateFrame("Frame", NoggitUI_NewName("GameEditorLightKeys"), parent);
	editor:SetWidth(options.width);
	editor:SetHeight(60 + LE.MAX_KEYS * ROW_HEIGHT);
	editor.isColor = options.isColor;
	editor.getTime = options.getTime;
	editor.onCreate = options.onCreate;

	editor.titleText = Common.Label(editor, "", "NoggitFontBold");
	editor.titleText:SetPoint("TOPLEFT", editor, "TOPLEFT", 0, 0);

	editor.countText = Common.Label(editor, "", nil, NOGGIT_COLOR_DISABLED);
	editor.countText:SetPoint("TOPRIGHT", editor, "TOPRIGHT", 0, -2);

	editor.strip = LE.CreateStrip(editor, options.width, 18);
	editor.strip:SetPoint("TOPLEFT", editor, "TOPLEFT", 0, -18);
	Common.Tooltip(editor.strip, "Click to add a key at that time");
	editor.strip:SetScript("OnClick", function(self)
		editor:AddKey(self:TimeAtCursor());
	end);

	editor.add = Common.Button(editor, "Add key", 80, function()
		editor:AddKey(editor.getTime and editor.getTime() or 0);
	end, "Add a key at the preview time with the value the band has there");
	editor.add:SetPoint("TOPLEFT", editor.strip, "BOTTOMLEFT", 0, -4);

	editor.empty = Common.Paragraph(editor, "");
	editor.empty:SetPoint("TOPLEFT", editor.add, "BOTTOMLEFT", 0, -8);
	editor.empty:SetPoint("RIGHT", editor, "RIGHT", 0, 0);

	editor.rows = {};

	for i = 1, LE.MAX_KEYS do
		editor.rows[i] = CreateKeyRow(editor, i);
	end

	editor.Keys = function(self)
		return self.doc and LE.GetKeys(self.doc.values) or {};
	end;

	editor.Store = function(self, keys, keepPicker)
		if ( not self.doc ) then
			return;
		end

		if ( not keepPicker ) then
			LE.HideColorPicker();
		end

		LE.SetKeys(self.doc, keys);
	end;

	editor.AddKey = function(self, time)
		if ( not self.doc and self.onCreate ) then
			self.doc = self.onCreate();
		end

		if ( not self.doc ) then
			GameEditor_Notify("Pick a light and a slot with a params set first", "info");
			return;
		end

		local keys = self:Keys();

		if ( #keys >= LE.MAX_KEYS ) then
			GameEditor_Notify("A band holds at most 16 keys", "info");
			return;
		end

		for _, key in ipairs(keys) do
			if ( key.time == time ) then
				GameEditor_Notify("There is a key at that time already", "info");
				return;
			end
		end

		local value;

		if ( self.isColor ) then
			value = #keys > 0 and LE.JoinColor(LE.InterpColor(keys, time)) or 0;
		else
			value = LE.InterpFloat(keys, time);
		end

		table.insert(keys, { time = time, value = value });
		self:Store(keys);
	end;

	editor.SetBand = function(self, doc, title, emptyText)
		if ( self.doc ~= doc ) then
			LE.HideColorPicker();
		end

		self.doc = doc;
		self.title = title;
		self.emptyText = emptyText;
		self:Refresh();
	end;

	editor.Refresh = function(self)
		self.titleText:SetText(self.title or "");
		local keys = self:Keys();
		self.countText:SetText(format("%d of %d keys", #keys, LE.MAX_KEYS));

		if ( self.isColor ) then
			self.strip:SetStops(LE.ColorStops(keys), true);
		else
			self.strip:SetStops(LE.FloatStops(keys), true);
		end

		if ( self.doc or self.onCreate ) then
			self.empty:SetText(#keys == 0 and (self.doc and "No keys, the band is black or zero all day. Add a key to start." or (self.emptyText or "")) or "");
			Common.SetEnabled(self.add, #keys < LE.MAX_KEYS);
		else
			self.empty:SetText(self.emptyText or "");
			Common.SetEnabled(self.add, false);
		end

		for i, row in ipairs(self.rows) do
			local key = keys[i];

			if ( key ) then
				row:Show();

				if ( not row.time:HasFocus() ) then
					row.time:SetText(LE.FormatTime(key.time));
				end

				if ( self.isColor ) then
					local r, g, b = LE.SplitColor(key.value);
					row.swatch.color:SetTexture(r / 255, g / 255, b / 255, 1);
					row.swatch.text:SetText(format("%02X%02X%02X", r, g, b));
					NoggitText_SetColor(row.swatch.text, (r + g + b) > 380 and { 0, 0, 0, 1 } or NOGGIT_COLOR_WHITE);
				else
					NoggitSpinBox_SetValue(row.value, key.value, true);
				end
			else
				row:Hide();
			end
		end
	end;

	editor.SetTime = function(self, time)
		self.strip:SetTime(time);
	end;

	return editor;
end
