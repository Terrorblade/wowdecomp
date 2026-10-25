-- Property grid shared by the creature, gameobject and spell editors. Rows come from GameEditorData.Fields and Groups.

local ROW_HEIGHT = 24;
local HEADER_HEIGHT = 24;
local EDIT_WIDTH = 96;

local Data = GameEditorData or {};

-- Name lookups for ref columns, keyed by the ref token ("spell", "item", "dbc:SpellIcon"). Workspaces may add more.
GameEditor_RefResolvers = GameEditor_RefResolvers or {};

-- Searches for ref pickers, resolver(text) returns { { value =, text = } ... }.
GameEditor_RefSearchers = GameEditor_RefSearchers or {};

-- Jumps for ref columns, opener(value) shows the referenced row in the workspace that edits it.
GameEditor_RefOpeners = GameEditor_RefOpeners or {};

function GameEditor_CanOpenRef(ref)
	return ref ~= nil and GameEditor_RefOpeners[ref] ~= nil;
end

function GameEditor_OpenRef(ref, value)
	value = tonumber(value);
	local opener = ref and GameEditor_RefOpeners[ref];

	if ( not opener or not value or value == 0 ) then
		return false;
	end

	local ok, err = pcall(opener, value);

	if ( not ok ) then
		GameEditor_Notify(format("Could not open %s %d: %s", ref, value, tostring(err)), "error");
	end

	return ok;
end

GameEditor_RefResolvers["spell"] = GameEditor_RefResolvers["spell"] or function(value)
	local name, rank = GetSpellInfo(value);

	if ( name and rank and rank ~= "" ) then
		return name.." ("..rank..")";
	end

	return name;
end;

GameEditor_RefResolvers["item"] = GameEditor_RefResolvers["item"] or function(value)
	return (GetItemInfo(value));
end;

local function DbcNameColumn(tableName)
	if ( not DbcEditor_GetSchema ) then
		return;
	end

	local schema = DbcEditor_GetSchema(tableName);

	if ( type(schema) ~= "table" ) then
		return;
	end

	for _, column in ipairs(schema) do
		if ( column.isName ) then
			return column.name;
		end
	end

	for _, column in ipairs(schema) do
		if ( column.type == "string" ) then
			return column.name;
		end
	end
end

local dbcNameColumns = {};

local function ResolveDbc(tableName, value)
	if ( not DbcEditor_GetRow ) then
		return;
	end

	local row = DbcEditor_GetRow(tableName, value);

	if ( type(row) ~= "table" ) then
		return;
	end

	if ( dbcNameColumns[tableName] == nil ) then
		dbcNameColumns[tableName] = DbcNameColumn(tableName) or false;
	end

	local column = dbcNameColumns[tableName];

	if ( column and row[column] and row[column] ~= "" ) then
		return tostring(row[column]);
	end

	return "exists";
end

function GameEditor_ResolveRef(ref, value)
	value = tonumber(value);

	if ( not ref or not value or value == 0 ) then
		return nil;
	end

	local resolver = GameEditor_RefResolvers[ref];

	if ( resolver ) then
		local ok, text = pcall(resolver, value);
		return ok and text or nil;
	end

	local _, _, tableName = string.find(ref, "^dbc:(.+)$");

	if ( tableName ) then
		return ResolveDbc(tableName, value);
	end
end

function GameEditor_SearchRef(ref, text)
	local searcher = GameEditor_RefSearchers[ref];

	if ( searcher ) then
		local ok, items = pcall(searcher, text);
		return ok and items or {};
	end

	local _, _, tableName = string.find(ref or "", "^dbc:(.+)$");

	if ( tableName and DbcEditor_Find ) then
		local items = {};
		local found = DbcEditor_Find(tableName, text ~= "" and text or "", 200);

		for _, row in ipairs(found or {}) do
			table.insert(items, { value = row.id, text = row.name or "" });
		end

		return items;
	end
end

function GameEditor_CanSearchRef(ref)
	return ref and (GameEditor_RefSearchers[ref] ~= nil or (string.find(ref, "^dbc:") ~= nil and DbcEditor_Find ~= nil));
end

-- Tables with no name of their own borrow one from the table a column points at.
local DERIVED_REFS = {
	FactionTemplate = { column = "m_faction", target = "Faction" },
	GemProperties = { column = "m_enchantID", target = "SpellItemEnchantment" },
	Holidays = { column = "m_holidayNameID", target = "HolidayNames" },
};

local derivedLists = {};

local function DerivedList(tableName, info)
	if ( derivedLists[tableName] ) then
		return derivedLists[tableName];
	end

	local items = {};
	local maxId = min(DbcEditor_GetMaxId and DbcEditor_GetMaxId(tableName) or 0, 100000);

	for id = 1, maxId do
		local row = DbcEditor_GetRow(tableName, id);

		if ( type(row) == "table" ) then
			local target = tonumber(row[info.column]) or 0;
			local text = target ~= 0 and ResolveDbc(info.target, target) or nil;
			table.insert(items, { value = id, text = text and text ~= "exists" and text or "" });
		end
	end

	derivedLists[tableName] = items;
	return items;
end

local derivedWatcher = CreateFrame("Frame");
derivedWatcher:RegisterEvent("DBC_RECORD_UPDATED");
derivedWatcher:SetScript("OnEvent", function(_, _, tableName)
	derivedLists = {};
end);

for tableName, info in pairs(DERIVED_REFS) do
	local ref = "dbc:"..tableName;

	GameEditor_RefResolvers[ref] = GameEditor_RefResolvers[ref] or function(value)
		local row = DbcEditor_GetRow and DbcEditor_GetRow(tableName, value);

		if ( type(row) ~= "table" ) then
			return;
		end

		local target = tonumber(row[info.column]) or 0;
		local text = target ~= 0 and ResolveDbc(info.target, target) or nil;
		return text or "exists";
	end;

	GameEditor_RefSearchers[ref] = GameEditor_RefSearchers[ref] or function(text)
		if ( not DbcEditor_GetRow ) then
			return {};
		end

		text = string.lower(strtrim(text or ""));
		local number = tonumber(text);
		local result = {};

		for _, item in ipairs(DerivedList(tableName, info)) do
			if ( text == "" or item.value == number or string.find(string.lower(item.text), text, 1, true) ) then
				table.insert(result, item);

				if ( #result >= 200 ) then
					break;
				end
			end
		end

		return result;
	end;
end

-- Number parsing that understands 0x hex and keeps unsigned 32 bit values.
function GameEditor_ParseNumber(text)
	text = strtrim(tostring(text or ""));

	if ( text == "" ) then
		return 0;
	end

	local negative = false;

	if ( string.sub(text, 1, 1) == "-" ) then
		negative = true;
		text = string.sub(text, 2);
	end

	local value;

	if ( string.find(text, "^0[xX]%x+$") ) then
		value = tonumber(string.sub(text, 3), 16);
	else
		value = tonumber(text);
	end

	if ( not value ) then
		return nil;
	end

	return negative and -value or value;
end

local function HasBit(value, bit)
	value = value or 0;

	if ( value < 0 ) then
		value = value + 4294967296;
	end

	return mod(floor(value / 2 ^ bit), 2) == 1;
end

local function SetBit(value, bit, set)
	value = value or 0;
	local negative = value < 0;

	if ( negative ) then
		value = value + 4294967296;
	end

	local has = HasBit(value, bit);

	if ( set and not has ) then
		value = value + 2 ^ bit;
	elseif ( not set and has ) then
		value = value - 2 ^ bit;
	end

	if ( negative and value >= 2147483648 ) then
		value = value - 4294967296;
	end

	return value;
end

GameEditor_HasBit = HasBit;
GameEditor_SetBit = SetBit;

local function FormatNumber(value, kind)
	value = tonumber(value) or 0;

	if ( kind == "float" ) then
		local text = format("%.4f", value);
		text = string.gsub(text, "(%..-)0+$", "%1");
		text = string.gsub(text, "%.$", "");
		return text;
	end

	return format("%d", value);
end

local function FormatHex(value)
	value = tonumber(value) or 0;

	if ( value < 0 ) then
		value = value + 4294967296;
	end

	return format("0x%08X", value);
end

-- Shared picker window -----------------------------------------------------------------------------------

local picker;

local function PickerFilter()
	local text = string.lower(picker.search:GetText() or "");
	local items = {};

	if ( picker.searchFunc ) then
		items = picker.searchFunc(picker.search:GetText() or "") or {};
	else
		for _, item in ipairs(picker.items) do
			if ( text == "" or string.find(string.lower(item.text or ""), text, 1, true) or tostring(item.value) == text ) then
				table.insert(items, item);
			end
		end
	end

	NoggitListBox_SetItems(picker.list, items);

	for index, item in ipairs(items) do
		if ( item.value == picker.current ) then
			picker.list.selected = index;
			picker.list.offset = max(0, index - 3);
			NoggitListBox_Refresh(picker.list);
			break;
		end
	end
end

local function CreatePicker()
	picker = GameEditor_CreateWindow("GameEditorPicker", "Pick a value", 360, 420, true);
	local content = picker.content;

	picker.search = NoggitLineEdit_Create(content, 200);
	picker.search:SetPoint("TOPLEFT", content, "TOPLEFT", 0, 0);
	picker.search:SetPoint("TOPRIGHT", content, "TOPRIGHT", 0, 0);
	picker.search:SetHeight(22);
	picker.search:SetScript("OnTextChanged", function()
		if ( picker.searchFunc ) then
			picker.pendingSearch = 0.3;
		else
			PickerFilter();
		end
	end);
	picker.search:SetScript("OnEscapePressed", function(self) self:ClearFocus(); end);
	picker.search:SetScript("OnEnterPressed", function(self)
		self:ClearFocus();
		local item = picker.list.items[picker.list.selected or 1];

		if ( picker.searchFunc ) then
			PickerFilter();
		elseif ( item ) then
			picker.onPick(item.value, item);
			GameEditor_HideWindow(picker);
		end
	end);
	picker:SetScript("OnUpdate", function(self, elapsed)
		if ( self.pendingSearch ) then
			self.pendingSearch = self.pendingSearch - elapsed;

			if ( self.pendingSearch <= 0 ) then
				self.pendingSearch = nil;
				PickerFilter();
			end
		end
	end);

	picker.list = NoggitListBox_Create(content, 20, function(row)
		row.value = NoggitUI_FontString(row, "");
		row.value:SetPoint("LEFT", row, "LEFT", 6, 0);
		row.value:SetWidth(70);
		NoggitText_SetColor(row.value, NOGGIT_COLOR_DISABLED);
		row.text = NoggitUI_FontString(row, "");
		row.text:SetPoint("LEFT", row, "LEFT", 80, 0);
		row.text:SetPoint("RIGHT", row, "RIGHT", -4, 0);
	end, function(row, item)
		row.value:SetText(tostring(item.value));
		row.text:SetText(item.text or "");
	end);
	picker.list:SetPoint("TOPLEFT", picker.search, "BOTTOMLEFT", 0, -6);
	picker.list:SetPoint("BOTTOMRIGHT", content, "BOTTOMRIGHT", 0, 0);
	picker.list.onDoubleClick = function(item)
		picker.onPick(item.value, item);
		GameEditor_HideWindow(picker);
	end;
end

-- GameEditor_PickValue(title, items or nil, current, onPick, searchFunc) shows a searchable list.
function GameEditor_PickValue(title, items, current, onPick, searchFunc)
	if ( not picker ) then
		CreatePicker();
	end

	picker:SetTitle(title or "Pick a value");
	picker.items = items or {};
	picker.current = current;
	picker.onPick = onPick;
	picker.searchFunc = searchFunc;
	picker.search:SetText("");
	GameEditor_ShowWindow(picker);
	PickerFilter();
	picker.search:SetFocus();
end

function GameEditor_EnumItems(enumName)
	local enum = Data.Enums and Data.Enums[enumName];
	local items = {};

	if ( not enum ) then
		return items;
	end

	for value, name in pairs(enum) do
		table.insert(items, { value = value, text = tostring(name) });
	end

	table.sort(items, function(a, b)
		if ( type(a.value) == type(b.value) ) then
			return a.value < b.value;
		end

		return tostring(a.value) < tostring(b.value);
	end);

	return items;
end

-- Shared flags window ------------------------------------------------------------------------------------

local flagsWindow;

local function FlagsRefresh()
	local window = flagsWindow;
	local value = window.value or 0;
	window.valueText:SetText(format("%s  (%d)", FormatHex(value), value));

	for _, box in ipairs(window.boxes) do
		if ( box:IsShown() ) then
			NoggitCheckBox_SetChecked(box, HasBit(value, box.bit));
		end
	end
end

local function CreateFlagsWindow()
	flagsWindow = GameEditor_CreateWindow("GameEditorFlags", "Flags", 380, 480, true);
	local content = flagsWindow.content;

	flagsWindow.valueText = NoggitUI_FontString(content, "", "NoggitFontBold");
	flagsWindow.valueText:SetPoint("TOPLEFT", content, "TOPLEFT", 2, -2);

	local clear = NoggitPushButton_Create(content, "Clear", 60, function()
		flagsWindow.value = 0;
		flagsWindow.onChange(0);
		FlagsRefresh();
	end);
	clear:SetPoint("TOPRIGHT", content, "TOPRIGHT", 0, 0);

	local scroll, child = GameEditor_CreateScrollFrame(content);
	scroll:SetPoint("TOPLEFT", content, "TOPLEFT", 0, -28);
	scroll:SetPoint("BOTTOMRIGHT", content, "BOTTOMRIGHT", 0, 0);
	flagsWindow.scroll = scroll;
	flagsWindow.child = child;
	flagsWindow.boxes = {};
end

-- GameEditor_EditFlags(title, flags { {bit, name, tooltip} }, value, onChange) edits a bit mask live.
function GameEditor_EditFlags(title, flags, value, onChange, bitCount)
	if ( not flagsWindow ) then
		CreateFlagsWindow();
	end

	local window = flagsWindow;
	window:SetTitle(title or "Flags");
	window.value = tonumber(value) or 0;
	window.onChange = onChange;

	local byBit = {};

	for _, flag in ipairs(flags or {}) do
		byBit[flag.bit] = flag;
	end

	local count = bitCount or 32;

	for bit = 0, count - 1 do
		local box = window.boxes[bit + 1];

		if ( not box ) then
			box = NoggitCheckBox_Create(window.child, "", false, function(self, checked)
				window.value = SetBit(window.value, self.bit, checked);
				window.onChange(window.value);
				FlagsRefresh();
			end);
			box:SetPoint("TOPLEFT", window.child, "TOPLEFT", 0, -bit * 22);
			window.boxes[bit + 1] = box;
		end

		local flag = byBit[bit];
		box.bit = bit;
		local label = format("%2d  0x%X  %s", bit, 2 ^ bit, flag and (flag.alias or flag.name) or "unused");
		local labelText = _G[box:GetName().."Label"];
		labelText:SetText(label);
		box:SetWidth(labelText:GetStringWidth() + 26);

		box.tooltip = flag and flag.tooltip or nil;
		box:SetScript("OnEnter", function(self)
			if ( self.tooltip ) then
				NoggitTooltip_Show(self, self.tooltip);
			end
		end);
		box:SetScript("OnLeave", NoggitTooltip_Hide);
		box:Show();
	end

	for index = count + 1, #window.boxes do
		window.boxes[index]:Hide();
	end

	window.child:SetHeight(count * 22 + 4);
	GameEditor_ShowWindow(window);
	window.scroll.UpdateRange();
	FlagsRefresh();
end

-- Grid ---------------------------------------------------------------------------------------------------------

local Grid = {};
Grid.__index = Grid;

local function FieldFor(grid, column)
	local fields = Data.Fields and Data.Fields[grid.tableName];
	local field = (grid.fieldOverrides and grid.fieldOverrides[column]) or (fields and fields[column]) or {};
	return field;
end

-- The meaning of misc value and gameobject data columns depends on other columns of the row.
local function EffectiveField(grid, column)
	local field = FieldFor(grid, column);

	if ( field.miscValue and field.effect and Data.MiscValue ) then
		local effect = tonumber(grid.values["m_effect["..(field.effect - 1).."]"]) or 0;
		local aura = tonumber(grid.values["m_effectAura["..(field.effect - 1).."]"]) or 0;
		local info = (aura ~= 0 and Data.MiscValue.Aura and Data.MiscValue.Aura[aura]) or (Data.MiscValue.Effect and Data.MiscValue.Effect[effect]);
		local slot = field.miscValue;

		if ( info and info[slot] ) then
			local derived = {};

			for key, value in pairs(field) do
				derived[key] = value;
			end

			derived.label = field.label.." - "..info[slot];
			derived.enum = info[slot.."Enum"];
			derived.flags = info[slot.."Flags"];
			derived.ref = info[slot.."Ref"];

			if ( info.note ) then
				derived.tooltip = (field.tooltip and (field.tooltip.."\n") or "")..info.note;
			end

			return derived;
		end
	end

	if ( field.goData and grid.dataByType and Data.GameObjectData ) then
		local goType = tonumber(grid.values[grid.dataByType]) or 0;
		local info = Data.GameObjectData[goType] and Data.GameObjectData[goType][field.goData];

		if ( info ) then
			return {
				label = format("Data %d - %s", field.goData, info.label or info.name or ""),
				tooltip = info.tooltip,
				type = field.type,
				ref = info.ref,
				bool = info.bool,
			};
		end

		return { label = format("Data %d - unused", field.goData), type = field.type, unused = true };
	end

	if ( field.flagsBy ) then
		local by = grid.values[field.flagsBy.column];
		local derived = {};

		for key, value in pairs(field) do
			derived[key] = value;
		end

		derived.flags = field.flagsBy.prefix..tostring(by or "");
		return derived;
	end

	return field;
end

local function ClassMaskFlags(grid, field)
	local family = tonumber(grid.values[field.classMask.family]) or 0;
	local names = Data.ClassMask and Data.ClassMask[family];
	local flags = {};

	if ( names ) then
		local base = (field.classMask.word or 0) * 32;

		for bit = 0, 31 do
			local name = names[base + bit];

			if ( name ) then
				table.insert(flags, { bit = bit, name = type(name) == "table" and (name.name or "") or tostring(name), tooltip = type(name) == "table" and name.tooltip or nil });
			end
		end
	end

	return flags;
end

local function RowInfoText(grid, row)
	local field = row.field;
	local value = grid.values[row.column];

	if ( field.enum ) then
		local enum = Data.Enums and Data.Enums[field.enum];
		local name = enum and (enum[tonumber(value) or value] or enum[value]);
		return name and tostring(name) or (value ~= nil and value ~= "" and value ~= 0 and "unknown" or "");
	end

	if ( field.flags or field.classMask ) then
		local flags = field.classMask and ClassMaskFlags(grid, field) or (Data.Flags and Data.Flags[field.flags]) or {};
		local names = {};
		local number = tonumber(value) or 0;

		for _, flag in ipairs(flags) do
			if ( HasBit(number, flag.bit) ) then
				table.insert(names, flag.alias or flag.name);
			end
		end

		if ( #names == 0 ) then
			return number ~= 0 and FormatHex(number) or "";
		end

		return table.concat(names, ", ");
	end

	if ( field.bool ) then
		return (tonumber(value) or 0) ~= 0 and "yes" or "no";
	end

	if ( field.ref ) then
		return GameEditor_ResolveRef(field.ref, value) or "";
	end

	return "";
end

local function ValueText(grid, row)
	local value = grid.values[row.column];
	local kind = row.field.type;

	if ( kind == "string" ) then
		return value ~= nil and tostring(value) or "";
	end

	if ( (row.field.flags or row.field.classMask) and (tonumber(value) or 0) ~= 0 ) then
		return FormatHex(value);
	end

	return FormatNumber(value, kind);
end

local function Changed(grid, column)
	local original = grid.original[column];
	local current = grid.values[column];

	if ( original == nil and current == nil ) then
		return false;
	end

	if ( type(original) == "number" or type(current) == "number" ) then
		return tonumber(original) ~= tonumber(current);
	end

	return tostring(original or "") ~= tostring(current or "");
end

local function RefreshRow(grid, row)
	row.field = EffectiveField(grid, row.column);
	local field = row.field;
	row.label:SetText(field.label or row.column);

	if ( field.unused ) then
		NoggitText_SetColor(row.label, NOGGIT_COLOR_DISABLED);
	else
		NoggitText_SetColor(row.label, NOGGIT_COLOR_TEXT);
	end

	if ( not row.edit:HasFocus() ) then
		row.shownText = ValueText(grid, row);
		row.edit:SetText(row.shownText);
		row.edit:SetCursorPosition(0);
	end

	row.info:SetText(RowInfoText(grid, row));

	local pickable = field.enum or field.flags or field.classMask or (field.ref and GameEditor_CanSearchRef(field.ref)) or field.bool;

	if ( pickable and not grid.readOnly[row.column] ) then
		row.pick:Show();
	else
		row.pick:Hide();
	end

	row.info:ClearAllPoints();
	row.info:SetPoint("RIGHT", row, "RIGHT", -4, 0);

	if ( field.ref and GameEditor_CanOpenRef(field.ref) and (tonumber(grid.values[row.column]) or 0) ~= 0 ) then
		row.open:Show();
		row.info:SetPoint("LEFT", row.open, "RIGHT", 6, 0);
	else
		row.open:Hide();
		row.info:SetPoint("LEFT", row.pick, "RIGHT", 6, 0);
	end

	if ( grid.readOnly[row.column] ) then
		row.edit:EnableMouse(false);
		row.edit:SetTextColor(NOGGIT_COLOR_DISABLED[1], NOGGIT_COLOR_DISABLED[2], NOGGIT_COLOR_DISABLED[3]);
	else
		row.edit:EnableMouse(true);
		row.edit:SetTextColor(NOGGIT_COLOR_TEXT[1], NOGGIT_COLOR_TEXT[2], NOGGIT_COLOR_TEXT[3]);
	end

	if ( Changed(grid, row.column) ) then
		row.bg:SetTexture(NOGGIT_COLOR_ACCENT[1], NOGGIT_COLOR_ACCENT[2], NOGGIT_COLOR_ACCENT[3], 0.22);
	else
		row.bg:SetTexture(0, 0, 0, 0);
	end

	if ( field.type == "string" ) then
		row.edit:ClearAllPoints();
		row.edit:SetPoint("LEFT", row, "LEFT", grid.labelWidth, 0);
		row.edit:SetPoint("RIGHT", row, "RIGHT", (pickable and not grid.readOnly[row.column]) and -34 or -4, 0);
		row.info:Hide();
	else
		row.edit:ClearAllPoints();
		row.edit:SetPoint("LEFT", row, "LEFT", grid.labelWidth, 0);
		row.edit:SetWidth(grid.editWidth);
		row.info:Show();
	end
end

-- The shown text is a rounded or hex view of the value, leaving it untouched must not count as an edit.
local function CommitEdit(grid, row)
	local text = row.edit:GetText();
	local value;

	if ( text == row.shownText ) then
		RefreshRow(grid, row);
		return;
	end

	if ( row.field.type == "string" ) then
		value = text;
	elseif ( type(grid.values[row.column]) == "string" and strmatch(text, "^%s*%-?%d+%s*$") ) then
		-- 64 bit columns arrive as strings, keep them exact instead of rounding through a double.
		value = strtrim(text);
	else
		value = GameEditor_ParseNumber(text);

		if ( not value ) then
			RefreshRow(grid, row);
			return;
		end

		if ( row.field.type ~= "float" ) then
			value = floor(value + 0.5);
		end

		-- Flags show as unsigned hex, signed columns keep their negative form.
		if ( value >= 2147483648 and value < 4294967296 and (tonumber(grid.values[row.column]) or 0) < 0 ) then
			value = value - 4294967296;
		end
	end

	grid:SetValue(row.column, value);
end

local function OpenPicker(grid, row)
	local field = row.field;
	local column = row.column;
	local title = field.label or column;

	if ( field.bool ) then
		grid:SetValue(column, (tonumber(grid.values[column]) or 0) ~= 0 and 0 or 1);
		return;
	end

	if ( field.enum ) then
		GameEditor_PickValue(title, GameEditor_EnumItems(field.enum), grid.values[column], function(value)
			grid:SetValue(column, value);
		end);
		return;
	end

	if ( field.flags or field.classMask ) then
		local flags = field.classMask and ClassMaskFlags(grid, field) or (Data.Flags and Data.Flags[field.flags]) or {};
		GameEditor_EditFlags(title, flags, tonumber(grid.values[column]) or 0, function(value)
			grid:SetValue(column, value);
		end);
		return;
	end

	if ( field.ref ) then
		GameEditor_PickValue(title, nil, tonumber(grid.values[column]), function(value)
			grid:SetValue(column, value);
		end, function(text)
			return GameEditor_SearchRef(field.ref, text);
		end);
	end
end

local function CreateRow(grid, column)
	local row = CreateFrame("Frame", NoggitUI_NewName("GameEditorGridRow"), grid.child);
	row:SetHeight(ROW_HEIGHT);
	row:EnableMouse(true);
	row.column = column;
	row.bg = row:CreateTexture(nil, "BACKGROUND");
	row.bg:SetAllPoints(row);

	row.label = NoggitUI_FontString(row, column);
	row.label:SetPoint("LEFT", row, "LEFT", 8, 0);
	row.label:SetWidth(grid.labelWidth - 12);
	row.label:SetHeight(ROW_HEIGHT);

	row.edit = NoggitLineEdit_Create(row, grid.editWidth);
	row.edit:SetHeight(20);
	row.edit:SetAutoFocus(false);
	-- The line edit template stops at 128 letters, long strings would be cut on display.
	row.edit:SetMaxLetters(0);
	row.edit:SetScript("OnEnterPressed", function(self)
		self:ClearFocus();
	end);
	row.edit:SetScript("OnEscapePressed", function(self)
		self.cancel = true;
		self:ClearFocus();
	end);
	row.edit:SetScript("OnEditFocusLost", function(self)
		if ( self.cancel ) then
			self.cancel = nil;
			RefreshRow(grid, row);
			return;
		end

		CommitEdit(grid, row);
	end);
	row.edit:SetScript("OnTabPressed", function(self)
		self:ClearFocus();
		local next = grid:NextRow(row, IsShiftKeyDown() and -1 or 1);

		if ( next ) then
			next.edit:SetFocus();
			next.edit:HighlightText();
		end
	end);

	row.pick = NoggitPushButton_Create(row, "...", 26, function()
		OpenPicker(grid, row);
	end);
	row.pick:SetHeight(20);
	row.pick:SetPoint("LEFT", row.edit, "RIGHT", 3, 0);

	row.open = NoggitPushButton_Create(row, "Go", 30, function()
		GameEditor_OpenRef(row.field and row.field.ref, grid.values[column]);
	end);
	row.open:SetHeight(20);
	row.open:SetPoint("LEFT", row.pick, "RIGHT", 3, 0);
	row.open:SetScript("OnEnter", function(self)
		NoggitButton_SetHover(self, true);
		NoggitTooltip_Show(self, "Open it in the workspace that edits it");
	end);
	row.open:SetScript("OnLeave", function(self)
		NoggitButton_SetHover(self, false);
		NoggitTooltip_Hide();
	end);
	row.open:Hide();

	row.info = NoggitUI_FontString(row, "");
	row.info:SetPoint("LEFT", row.pick, "RIGHT", 6, 0);
	row.info:SetPoint("RIGHT", row, "RIGHT", -4, 0);
	row.info:SetHeight(ROW_HEIGHT);
	NoggitText_SetColor(row.info, NOGGIT_COLOR_DISABLED);

	local function ShowTooltip()
		local field = row.field or {};
		local text = (field.label or column).."\n|cff8f8f8f"..column.."|r";

		if ( field.tooltip ) then
			text = text.."\n\n"..field.tooltip;
		end

		local info = row.info:GetText();

		if ( info and info ~= "" ) then
			text = text.."\n\n|cffffcc00"..info.."|r";
		end

		NoggitTooltip_Show(row, text);
	end

	row:SetScript("OnEnter", ShowTooltip);
	row:SetScript("OnLeave", NoggitTooltip_Hide);
	row:SetScript("OnMouseUp", function(_, button)
		if ( button == "RightButton" and grid.onRowContext ) then
			grid.onRowContext(grid, column);
		end
	end);

	grid.rowsByColumn[column] = row;
	return row;
end

local function CreateHeader(grid, section)
	local header = CreateFrame("Button", NoggitUI_NewName("GameEditorGridHeader"), grid.child, "NoggitBoxTemplate");
	header:SetHeight(HEADER_HEIGHT);
	NoggitBox_Set(header, NOGGIT_COLOR_MID, nil);
	header.text = NoggitUI_FontString(header, section.title or "", "NoggitFontBold");
	header.text:SetPoint("LEFT", header, "LEFT", 24, 0);
	header.arrow = NoggitUI_CreateCaret(header);
	header.arrow:SetPoint("LEFT", header, "LEFT", 6, 0);
	header.count = NoggitUI_FontString(header, "");
	header.count:SetPoint("RIGHT", header, "RIGHT", -8, 0);
	NoggitText_SetColor(header.count, NOGGIT_COLOR_DISABLED);
	header:SetScript("OnClick", function()
		section.collapsed = not section.collapsed;
		grid:Layout();
	end);
	header:SetScript("OnEnter", function(self) NoggitBox_Set(self, NOGGIT_COLOR_HOVER, nil); end);
	header:SetScript("OnLeave", function(self) NoggitBox_Set(self, NOGGIT_COLOR_MID, nil); end);
	section.header = header;
end

function Grid:Layout()
	local y = 0;
	local filter = self.filter and string.lower(self.filter) or "";

	for _, section in ipairs(self.sections) do
		local visibleRows = {};
		local changed = 0;

		for _, row in ipairs(section.rows) do
			local field = row.field or FieldFor(self, row.column);
			local show = not self.hidden[row.column];

			if ( show and self.visible ) then
				show = self.visible(row.column, self.values) ~= false;
			end

			if ( show and filter ~= "" ) then
				local label = string.lower(field.label or "");
				show = string.find(label, filter, 1, true) or string.find(string.lower(row.column), filter, 1, true);
			end

			if ( Changed(self, row.column) ) then
				changed = changed + 1;
			end

			if ( show ) then
				table.insert(visibleRows, row);
			else
				row:Hide();
			end
		end

		local header = section.header;

		if ( #visibleRows == 0 or (section.when and not section.when(self.values)) ) then
			if ( header ) then
				header:Hide();
			end

			for _, row in ipairs(visibleRows) do
				row:Hide();
			end
		else
			if ( header ) then
				header:ClearAllPoints();
				header:SetPoint("TOPLEFT", self.child, "TOPLEFT", 0, -y);
				header:SetPoint("TOPRIGHT", self.child, "TOPRIGHT", 0, -y);
				header.arrow:SetOpen(not (section.collapsed and filter == ""));
				header.count:SetText(changed > 0 and format("%d changed", changed) or "");
				header:Show();
				y = y + HEADER_HEIGHT + 2;
			end

			local open = not section.collapsed or filter ~= "" or not header;

			for _, row in ipairs(visibleRows) do
				if ( open ) then
					row:ClearAllPoints();
					row:SetPoint("TOPLEFT", self.child, "TOPLEFT", 0, -y);
					row:SetPoint("TOPRIGHT", self.child, "TOPRIGHT", 0, -y);
					row:Show();
					y = y + ROW_HEIGHT;
				else
					row:Hide();
				end
			end

			y = y + 4;
		end
	end

	self.child:SetHeight(max(10, y));
	self.scroll.UpdateRange();
end

function Grid:Refresh()
	for _, row in pairs(self.rowsByColumn) do
		RefreshRow(self, row);
	end

	self:Layout();
end

-- Values are column -> value. The original copy is what changes are measured against.
function Grid:SetValues(values, keepOriginal)
	self.values = {};

	for key, value in pairs(values or {}) do
		self.values[key] = value;
	end

	if ( not keepOriginal ) then
		self.original = {};

		for key, value in pairs(values or {}) do
			self.original[key] = value;
		end
	end

	self:Refresh();
end

function Grid:GetValue(column)
	return self.values[column];
end

function Grid:GetValues()
	return self.values;
end

function Grid:SetValue(column, value, quiet)
	local previous = self.values[column];
	self.values[column] = value;

	local dependents = false;

	for _, row in pairs(self.rowsByColumn) do
		local field = FieldFor(self, row.column);

		if ( field.miscValue or field.goData or field.flagsBy or field.classMask ) then
			dependents = true;
			break;
		end
	end

	if ( dependents ) then
		self:Refresh();
	elseif ( self.rowsByColumn[column] ) then
		RefreshRow(self, self.rowsByColumn[column]);
		self:Layout();
	end

	if ( not quiet and self.onChange and previous ~= value ) then
		self.onChange(self, column, value, previous);
	end
end

function Grid:GetChanges()
	local changes = {};
	local count = 0;

	for column in pairs(self.rowsByColumn) do
		if ( Changed(self, column) ) then
			changes[column] = self.values[column];
			count = count + 1;
		end
	end

	return changes, count;
end

function Grid:HasChanges()
	local _, count = self:GetChanges();
	return count > 0;
end

-- After a successful save the current values become the new original.
function Grid:AcceptChanges()
	self.original = {};

	for key, value in pairs(self.values) do
		self.original[key] = value;
	end

	self:Refresh();
end

function Grid:Revert()
	self:SetValues(self.original);
end

function Grid:SetFilter(text)
	self.filter = text;
	self:Layout();
end

function Grid:SetReadOnly(column, readOnly)
	self.readOnly[column] = readOnly and true or nil;

	if ( self.rowsByColumn[column] ) then
		RefreshRow(self, self.rowsByColumn[column]);
	end
end

function Grid:SetHidden(column, hidden)
	self.hidden[column] = hidden and true or nil;
	self:Layout();
end

function Grid:NextRow(row, direction)
	local order = {};

	for _, section in ipairs(self.sections) do
		for _, candidate in ipairs(section.rows) do
			if ( candidate:IsShown() ) then
				table.insert(order, candidate);
			end
		end
	end

	for index, candidate in ipairs(order) do
		if ( candidate == row ) then
			return order[index + direction];
		end
	end
end

function Grid:Show()
	self.frame:Show();
end

function Grid:Hide()
	self.frame:Hide();
end

-- GameEditor_CreatePropertyGrid(parent, options) builds the grid for options.table.
--   options.groups: sections { {title, columns, collapsed, when = fn(values)} }, defaults to Groups[table] or one section of Columns[table]
--   options.columns: plain column list instead of groups, options.fields: column -> field overrides
--   options.readOnly / options.hidden: sets of columns, options.visible(column, values) hides rows
--   options.onChange(grid, column, value, previous), options.onRowContext(grid, column)
--   options.labelWidth, options.editWidth, options.filter (true adds a search box)
function GameEditor_CreatePropertyGrid(parent, options)
	options = options or {};
	local grid = setmetatable({}, Grid);
	grid.tableName = options.table;
	grid.fieldOverrides = options.fields;
	grid.readOnly = options.readOnly or {};
	grid.hidden = options.hidden or {};
	grid.visible = options.visible;
	grid.onChange = options.onChange;
	grid.onRowContext = options.onRowContext;
	grid.labelWidth = options.labelWidth or 170;
	grid.editWidth = options.editWidth or EDIT_WIDTH;
	grid.values = {};
	grid.original = {};
	grid.rowsByColumn = {};
	grid.sections = {};

	local frame = CreateFrame("Frame", NoggitUI_NewName("GameEditorGrid"), parent);
	grid.frame = frame;

	local top = 0;

	if ( options.filter ) then
		local search = NoggitLineEdit_Create(frame, 200);
		search:SetHeight(22);
		search:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, 0);
		search:SetPoint("TOPRIGHT", frame, "TOPRIGHT", 0, 0);
		search:SetAutoFocus(false);
		search:SetScript("OnTextChanged", function(self) grid:SetFilter(self:GetText()); end);
		search:SetScript("OnEscapePressed", function(self) self:SetText(""); self:ClearFocus(); end);
		search:SetScript("OnEnterPressed", function(self) self:ClearFocus(); end);
		local hint = NoggitUI_FontString(search, "Filter fields");
		hint:SetPoint("LEFT", search, "LEFT", 6, 0);
		NoggitText_SetColor(hint, NOGGIT_COLOR_DISABLED);
		search:SetScript("OnEditFocusGained", function() hint:Hide(); end);
		search:SetScript("OnEditFocusLost", function(self) if ( self:GetText() == "" ) then hint:Show(); end end);
		grid.search = search;
		top = 26;
	end

	local scroll, child = GameEditor_CreateScrollFrame(frame);
	scroll:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, -top);
	scroll:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", 0, 0);
	grid.scroll = scroll;
	grid.child = child;

	local groups = options.groups;

	if ( not groups and options.columns ) then
		groups = { { columns = options.columns } };
	end

	if ( not groups ) then
		groups = Data.Groups and Data.Groups[options.table];
	end

	if ( not groups ) then
		groups = { { columns = (Data.Columns and Data.Columns[options.table]) or {} } };
	end

	for _, group in ipairs(groups) do
		local section = { title = group.title, collapsed = group.collapsed, when = group.when, rows = {} };

		if ( group.dataByType ) then
			grid.dataByType = group.dataByType;
		end

		if ( group.title ) then
			CreateHeader(grid, section);
		end

		for _, column in ipairs(group.columns or {}) do
			table.insert(section.rows, CreateRow(grid, column));
		end

		table.insert(grid.sections, section);
	end

	frame:SetScript("OnSizeChanged", function() scroll.UpdateRange(); end);
	grid:Refresh();
	return grid;
end
