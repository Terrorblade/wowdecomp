-- Layout pieces in the style of the Personal Spell Editor: labelled field columns, checkbox flag lists, sub tabs and pick fields.

local SE = GameEditorSpell;
local Data = GameEditorData or {};

local FLAG_ROW = 20;

-- Spell field definitions with the labels the Personal Spell Editor uses. spec is a list of { column, label }.
function SE.LabeledFields(spec, tableName)
	local fields = Data.Fields and Data.Fields[tableName or SE.TABLE] or {};
	local result = {};
	local columns = {};

	for _, entry in ipairs(spec) do
		local column, label = entry[1], entry[2];
		local field = {};

		for key, value in pairs(fields[column] or {}) do
			field[key] = value;
		end

		field.label = label or field.label;
		result[column] = field;
		table.insert(columns, column);
	end

	return result, columns;
end

-- One column of label and value rows bound to the open spell.
function SE.FieldColumn(parent, spec, options)
	options = options or {};
	local fields, columns = SE.LabeledFields(spec);
	local grid = SE.CreateSpellGrid(parent, { { columns = columns } }, {
		fields = fields,
		labelWidth = options.labelWidth or 150,
		editWidth = options.editWidth or 70,
		visible = options.visible,
	});
	return grid;
end

-- Puts frames next to each other across parent. panes is a list of { frame, width = fixed or weight = share }.
function SE.SideBySide(parent, panes, gap, top, bottom)
	gap = gap or 12;

	local function Layout()
		local width = parent:GetWidth() or 0;
		local fixed, weight = 0, 0;

		for _, pane in ipairs(panes) do
			if ( pane.width ) then
				fixed = fixed + pane.width;
			else
				weight = weight + (pane.weight or 1);
			end
		end

		local free = max(0, width - fixed - gap * (#panes - 1));

		-- The gaps stay, weighted panes give way first, then fixed ones from the right, a pane under 24 collapses.
		local over = max(0, fixed + gap * (#panes - 1) - width);
		local widths = {};

		for index = #panes, 1, -1 do
			local pane = panes[index];

			if ( pane.width ) then
				local cut = min(over, pane.width);
				widths[index] = pane.width - cut;
				over = over - cut;
			else
				widths[index] = floor(free * (pane.weight or 1) / max(1, weight));
			end
		end

		local x = 0;

		for index, pane in ipairs(panes) do
			local paneWidth = widths[index];
			pane.frame:ClearAllPoints();
			pane.frame:SetPoint("TOPLEFT", parent, "TOPLEFT", x, top or 0);
			pane.frame:SetPoint("BOTTOMLEFT", parent, "BOTTOMLEFT", x, bottom or 0);
			pane.frame:SetWidth(max(1, paneWidth));

			if ( width > 0 and paneWidth < 24 ) then
				if ( pane.frame:IsShown() ) then
					pane.collapsed = true;
					pane.frame:Hide();
				end
			elseif ( pane.collapsed ) then
				pane.collapsed = nil;
				pane.frame:Show();
			end

			x = x + paneWidth + (pane.collapsed and floor(gap / 2) or gap);
		end
	end

	parent:HookScript("OnSizeChanged", Layout);
	Layout();
	return Layout;
end

-- Sub tabs with one page frame each, the pages fill parent below the tab row.
function SE.CreateTabPages(parent, names, top, onSelect)
	top = top or 0;
	local holder = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellSubTabs"), parent);
	holder:SetHeight(24);
	holder:SetPoint("TOPLEFT", parent, "TOPLEFT", 0, top);
	holder:SetPoint("TOPRIGHT", parent, "TOPRIGHT", 0, top);

	local pages = { selected = 1, holder = holder };

	for index = 1, #names do
		local page = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellSubPage"), parent);
		page:SetPoint("TOPLEFT", parent, "TOPLEFT", 0, top - 30);
		page:SetPoint("BOTTOMRIGHT", parent, "BOTTOMRIGHT", 0, 0);
		page:Hide();
		pages[index] = page;
	end

	pages.Select = function(self, index)
		self.selected = index;

		for other = 1, #names do
			if ( other ~= index ) then
				self[other]:Hide();
			end
		end

		for _, tab in ipairs(self.tabs) do
			tab.selected = tab.index == index;
			NoggitTab_Refresh(tab);
		end

		self[index]:Show();

		if ( onSelect ) then
			onSelect(index);
		end
	end;

	pages.tabs = NoggitTabs_Create(holder, names, 1, function(index) pages:Select(index); end);
	pages.tabs[1]:SetPoint("BOTTOMLEFT", holder, "BOTTOMLEFT", 0, 0);
	pages[1]:Show();
	return pages;
end

-- Flag lists ----------------------------------------------------------------------------------------------

local function FlagsOf(word, get)
	if ( word.flags ) then
		return word.flags;
	end

	local field = Data.Fields and Data.Fields[word.table or SE.TABLE] and Data.Fields[word.table or SE.TABLE][word.column] or {};

	if ( field.flagsBy ) then
		return field.flagsBy.prefix..tostring(SE.Number(get(field.flagsBy.column)));
	end

	return field.flags;
end

-- Flags of a word with a key that changes when the list does. word.list(get) gives computed flags, like class mask names.
local function FlagListOf(word, get)
	if ( word.list ) then
		return word.list(get) or {}, "list:"..tostring(word.key and word.key(get) or "");
	end

	local name = FlagsOf(word, get);
	return Data.Flags and Data.Flags[name or ""] or {}, tostring(name);
end

-- Checkbox list over the bits of one or more flag columns.
--   options.words: { { column, flags = name, offset = bit offset in the shown numbers, list = fn(get), key = fn(get), watch = column } }
--   flags default to the field's flag set, list and key give computed flags and watch is a column the list follows
--   options.title, options.tooltip, options.allBits (unnamed bits too), options.showHex, options.emptyText
--   options.get(column), options.set(column, value), options.original(column) default to the open spell
function SE.CreateFlagList(parent, options)
	local frame = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellFlagList"), parent);
	local get = options.get or function(column) return SE.GetValue(column); end;
	local set = options.set or function(column, value) SE.SetValue(column, value, frame); end;
	local original = options.original or function(column) return SE.cur and SE.cur.original[column]; end;
	local top = 0;

	if ( options.title ) then
		frame.title = SE.Label(frame, options.title, "NoggitFontBold", NOGGIT_COLOR_WHITE);
		frame.title:SetPoint("TOPLEFT", frame, "TOPLEFT", 2, 0);

		if ( options.tooltip ) then
			local hit = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellFlagTitle"), frame);
			hit:SetAllPoints(frame.title);
			hit:EnableMouse(true);
			SE.Tooltip(hit, options.tooltip);
		end

		frame.count = SE.Label(frame, "", "NoggitFont10", NOGGIT_COLOR_DISABLED, "RIGHT");
		frame.count:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -2, -2);
		top = -18;
	end

	local box = SE.Box(frame, "GameEditorSpellFlagBox", NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID);
	box:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, top);
	box:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", 0, 0);
	local scroll, child = GameEditor_CreateScrollFrame(box);
	scroll:SetPoint("TOPLEFT", box, "TOPLEFT", 4, -4);
	scroll:SetPoint("BOTTOMRIGHT", box, "BOTTOMRIGHT", -4, 4);
	frame.scroll = scroll;
	frame.child = child;
	frame.rows = {};
	frame.filter = "";
	frame.onlySet = false;
	frame.empty = SE.Label(child, options.emptyText or "No flags for this value.", nil, NOGGIT_COLOR_DISABLED);
	frame.empty:SetPoint("TOPLEFT", child, "TOPLEFT", 4, -4);
	frame.empty:Hide();

	local function CreateRow()
		local row = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellFlagRow"), child);
		row:SetHeight(FLAG_ROW);
		row.bg = row:CreateTexture(nil, "BACKGROUND");
		row.bg:SetAllPoints(row);
		row.bg:SetTexture(0, 0, 0, 0);
		row.box = NoggitCheckBox_Create(row, "", false, function(_, checked)
			local value = SE.Number(get(row.word.column));
			set(row.word.column, GameEditor_SetBit(value, row.bit, checked));
			frame:Refresh();
		end);
		row.box:SetPoint("LEFT", row, "LEFT", 2, 0);
		row.label = _G[row.box:GetName().."Label"];
		row.label:SetJustifyH("LEFT");
		row.label:SetHeight(16);
		table.insert(frame.rows, row);
		return row;
	end

	-- Rows come from the flag sets, which can follow another column like the item subclass list.
	frame.Rebuild = function(self)
		local key = {};

		for _, word in ipairs(options.words) do
			local _, wordKey = FlagListOf(word, get);
			table.insert(key, wordKey);
		end

		key = table.concat(key, ",");

		if ( self.builtFor == key ) then
			return;
		end

		self.builtFor = key;
		local used = 0;

		for _, word in ipairs(options.words) do
			local byBit = {};

			for _, flag in ipairs((FlagListOf(word, get))) do
				byBit[flag.bit] = flag;
			end

			for bit = 0, 31 do
				local flag = byBit[bit];

				if ( flag or options.allBits ) then
					flag = flag or { bit = bit, name = "Unused bit "..(bit + (word.offset or 0)) };
					used = used + 1;
					local row = self.rows[used] or CreateRow();
					row.word = word;
					row.bit = bit;
					local title = flag.alias or flag.name or ("Bit "..bit);
					local subtitle = (flag.alias and flag.name and flag.name ~= flag.alias) and flag.name or nil;
					row.label:SetText(title);
					row.box.tooltip = format("%s\n|cff8f8f8f%s bit %d  0x%X  %s|r%s%s", title, word.column, bit, 2 ^ bit, flag.const or "",
						subtitle and ("\n\n"..subtitle) or "", flag.tooltip and ("\n\n"..flag.tooltip) or "");
					row.search = SE.Lower(table.concat({ title, flag.name or "", flag.const or "", flag.tooltip or "", "bit "..bit }, " "));
					row.inUse = true;
				end
			end
		end

		for index = used + 1, #self.rows do
			self.rows[index].inUse = false;
			self.rows[index]:Hide();
		end
	end;

	frame.Layout = function(self)
		local y = 0;
		local width = max(40, (self.child:GetWidth() or 100) - 10);
		local shown = 0;

		for _, row in ipairs(self.rows) do
			local show = row.inUse;

			if ( show and self.filter ~= "" and not string.find(row.search, self.filter, 1, true) ) then
				show = false;
			end

			if ( show and self.onlySet and not GameEditor_HasBit(SE.Number(get(row.word.column)), row.bit) ) then
				show = false;
			end

			if ( show ) then
				row:ClearAllPoints();
				row:SetPoint("TOPLEFT", self.child, "TOPLEFT", 0, -y);
				row:SetPoint("TOPRIGHT", self.child, "TOPRIGHT", 0, -y);
				row.label:SetWidth(width - 24);
				row.box:SetWidth(width);
				row:Show();
				y = y + FLAG_ROW;
				shown = shown + 1;
			else
				row:Hide();
			end
		end

		if ( shown == 0 ) then
			self.empty:Show();
			y = 20;
		else
			self.empty:Hide();
		end

		self.child:SetHeight(max(10, y));
		self.scroll.UpdateRange();
	end;

	frame.Refresh = function(self)
		self:Rebuild();
		local count = 0;

		for _, row in ipairs(self.rows) do
			if ( row.inUse ) then
				local value = SE.Number(get(row.word.column));
				local isSet = GameEditor_HasBit(value, row.bit);
				NoggitCheckBox_SetChecked(row.box, isSet);

				if ( isSet ) then
					count = count + 1;
				end

				if ( isSet ~= GameEditor_HasBit(SE.Number(original(row.word.column)), row.bit) ) then
					row.bg:SetTexture(NOGGIT_COLOR_ACCENT[1], NOGGIT_COLOR_ACCENT[2], NOGGIT_COLOR_ACCENT[3], 0.22);
				else
					row.bg:SetTexture(0, 0, 0, 0);
				end
			end
		end

		self.setCount = count;

		if ( self.count ) then
			local text = count > 0 and format("%d set", count) or "";

			if ( options.showHex and options.words[1] ) then
				local value = SE.Number(get(options.words[1].column));
				text = format("%s  0x%08X", text, value < 0 and value + 4294967296 or value);
			end

			self.count:SetText(text);
		end

		self.stamp = SE.stamp;
		self:Layout();
	end;

	frame.SetFilter = function(self, text, onlySet)
		self.filter = SE.Lower(strtrim(text or ""));
		self.onlySet = onlySet and true or false;
		self:Layout();
	end;

	local watched = {};

	for _, word in ipairs(options.words) do
		watched[word.column] = true;
		local field = Data.Fields and Data.Fields[word.table or SE.TABLE] and Data.Fields[word.table or SE.TABLE][word.column];

		if ( field and field.flagsBy ) then
			watched[field.flagsBy.column] = true;
		end

		if ( word.watch ) then
			watched[word.watch] = true;
		end
	end

	if ( not options.get ) then
		SE.On("VALUE_CHANGED", function(column, source)
			if ( source ~= frame and frame:IsVisible() and watched[column] ) then
				frame:Refresh();
			end
		end);
		local function RefreshVisible()
			if ( frame:IsVisible() ) then
				frame:Refresh();
			end
		end

		SE.On("SPELL_LOADED", RefreshVisible);
		SE.On("SPELL_SAVED", RefreshVisible);
	end

	child:SetScript("OnSizeChanged", function() frame:Layout(); end);
	frame:SetScript("OnShow", function(self)
		if ( self.stamp ~= SE.stamp or options.get ) then
			self:Refresh();
		else
			self:Layout();
		end
	end);
	frame.stamp = -1;
	return frame;
end

-- Pick fields ---------------------------------------------------------------------------------------------

-- A box showing the picked value, a click opens the searchable picker and right click clears it.
-- options.items() or options.search(text) feed the picker, options.text(value) names the value.
function SE.CreatePickField(parent, width, title, options, onChange)
	local field = CreateFrame("Button", NoggitUI_NewName("GameEditorSpellPickField"), parent, "NoggitBoxTemplate");
	field:SetWidth(width);
	field:SetHeight(22);
	field:RegisterForClicks("LeftButtonUp", "RightButtonUp");
	NoggitBox_Set(field, NOGGIT_COLOR_MID, NOGGIT_COLOR_HOVER);
	field.text = SE.Label(field, "");
	field.text:SetPoint("LEFT", field, "LEFT", 8, 0);
	field.text:SetPoint("RIGHT", field, "RIGHT", -22, 0);

	field.clear = SE.GlyphButton(field, 0xF00D, "Clear", function()
		field:SetValue(nil);

		if ( onChange ) then
			onChange(nil);
		end
	end, 18);
	field.clear:SetPoint("RIGHT", field, "RIGHT", -2, 0);

	field.SetValue = function(self, value)
		self.value = value;

		if ( value == nil ) then
			self.text:SetText("|cff8f8f8fAny|r");
			self.clear:Hide();
		else
			local name = options.text and options.text(value);
			self.text:SetText(format("%s%s", tostring(value), (name and name ~= "") and ("  "..name) or ""));
			self.clear:Show();
		end
	end;

	field:SetScript("OnClick", function(self, button)
		if ( button == "RightButton" ) then
			self.clear:Click();
			return;
		end

		GameEditor_PickValue(title, options.items and options.items() or nil, self.value, function(value)
			self:SetValue(tonumber(value) or value);

			if ( onChange ) then
				onChange(self.value);
			end
		end, options.search);
	end);
	field:SetScript("OnEnter", function(self)
		NoggitBox_Set(self, NOGGIT_COLOR_HOVER, NOGGIT_COLOR_HOVER);
		NoggitTooltip_Show(self, title.."\n|cff8f8f8fClick to pick, right click clears|r");
	end);
	field:SetScript("OnLeave", function(self)
		NoggitBox_Set(self, NOGGIT_COLOR_MID, NOGGIT_COLOR_HOVER);
		NoggitTooltip_Hide();
	end);

	field:SetValue(nil);
	return field;
end

-- Label on the left, control on the right, for the filter rows of the Select Spell tab.
function SE.LabeledControl(parent, label, control, labelWidth)
	local text = SE.Label(parent, label);
	text:SetPoint("RIGHT", control, "LEFT", -8, 0);
	text:SetWidth((labelWidth or 130) - 8);
	text:SetJustifyH("LEFT");
	return text;
end
