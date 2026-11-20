-- Item generator view: every item_generator table, whole tables as row lists and keyed tables per key.

GameEditorItem = GameEditorItem or {};
local I = GameEditorItem;
local Common = GameEditorObject;
local Data = GameEditorData or {};

local NAV_WIDTH = 210;
local KEY_LIST_WIDTH = 210;

local function Name(enumName, value)
	return I.EnumText(enumName, value) or tostring(value);
end

local function Rate(value)
	return Common.Float(value, 3);
end

local function SortBy(...)
	local columns = { ... };

	return function(a, b)
		for _, column in ipairs(columns) do
			local x, y = tonumber(a[column]) or 0, tonumber(b[column]) or 0;

			if ( x ~= y ) then
				return x < y;
			end
		end

		return false;
	end;
end

local function NextIndex(editor, column, filter)
	local highest = -1;

	for _, wrapper in ipairs(editor.wrappers) do
		if ( not filter or filter(wrapper.values) ) then
			highest = max(highest, tonumber(wrapper.values[column]) or 0);
		end
	end

	return highest + 1;
end

local function SubclassText(class, subclass)
	if ( (tonumber(subclass) or 0) < 0 ) then
		return "any";
	end

	return I.EnumText(I.SubClassEnum(class), subclass) or tostring(subclass);
end

-- Subclass names follow the class column of the row being edited.
local function SubclassFollower(classColumn, subclassColumn, tableName)
	local base = Data.Fields[tableName] and Data.Fields[tableName][subclassColumn] or { label = "Subclass", type = "int" };
	local field = I.Copy(base);
	local fields = { [subclassColumn] = field };

	local function Follow(editor)
		local class = editor.current and tonumber(editor.current.values[classColumn]) or 4;
		field.enum = (class and class >= 0) and I.SubClassEnum(class) or nil;
		editor.grid:Refresh();
	end

	return fields, Follow;
end

-- Sections ---------------------------------------------------------------------------------------------------

local itemTypeFields, itemTypeFollow = SubclassFollower("itemClass", "subClass", "item_generator_item_types");
local uniqueFields, uniqueFollow = SubclassFollower("itemClass", "subClass", "item_generator_unique");

-- The value of a stat list row is a stat type, of the socket list a socket color.
local statValueField = I.Copy(Data.Fields.item_generator_stat_group_stats.value);
local statFields = { value = statValueField };

local function StatFollow(editor)
	local kind = editor.current and tonumber(editor.current.values.type) or 0;
	statValueField.enum = kind == 2 and "ItemSocketColor" or "ItemModType";
	editor.grid:Refresh();
end

local INVENTORY_KEYS = {};

for value in pairs(Data.Enums.InventoryType or {}) do
	if ( value > 0 ) then
		table.insert(INVENTORY_KEYS, value);
	end
end

table.sort(INVENTORY_KEYS);

local NAME_KEYS = { 1, 2, 3, 4, 5, 6, 7, 8 };

local SECTIONS = {
	{ header = "Rates and slots" },
	{
		title = "Quality rates", table = "item_generator_quality_rate", whole = true,
		help = "Stat budget multiplier per quality.",
		summary = function(v) return format("%s  %s", Name("ItemQuality", v.quality), Rate(v.rate)); end,
		sort = SortBy("quality"), unique = { { "quality" } }, defaults = { rate = 1 },
	},
	{
		title = "Stat rates", table = "item_generator_stat_rate", whole = true,
		help = "Points of a stat per budget point. A stat without a row uses 1.",
		summary = function(v) return format("%s  %s", Name("ItemModType", v.stat), Rate(v.rate)); end,
		sort = SortBy("stat"), unique = { { "stat" } }, defaults = { rate = 1 },
	},
	{
		title = "Slot rates", table = "item_generator_slot_rate", whole = true,
		help = "Stat budget multiplier per inventory type.",
		summary = function(v) return format("%s  %s", Name("InventoryType", v.slot), Rate(v.rate)); end,
		sort = SortBy("slot"), unique = { { "slot" } }, defaults = { rate = 1 },
	},
	{
		title = "Stat slots", table = "item_generator_stat_slots", whole = true,
		help = "How many primary and secondary stats an item of each quality rolls. Missing qualities roll 1 primary and 0 secondary.",
		summary = function(v) return format("%s  %d + %d", Name("ItemQuality", v.quality), tonumber(v.primary_slots) or 0, tonumber(v.secondary_slots) or 0); end,
		sort = SortBy("quality"), unique = { { "quality" } }, defaults = { primary_slots = 1 },
	},
	{ header = "Item rolls" },
	{
		title = "Item types", table = "item_generator_item_types", whole = true,
		help = "Class, subclass and slot combinations a random type base can roll, with their sheath, material and range.",
		summary = function(v) return format("%s, %s", SubclassText(v.itemClass, v.subClass), Name("InventoryType", v.inventoryType)); end,
		sort = SortBy("itemClass", "subClass", "inventoryType"), unique = { { "itemClass", "subClass", "inventoryType" } },
		defaults = { itemClass = 4 }, fields = itemTypeFields, follow = itemTypeFollow, followColumn = "itemClass",
	},
	{
		title = "Spells", table = "item_generator_spells", whole = true,
		help = "Spells a generated item can roll. -1 in a filter column means any.",
		summary = function(v)
			local name = GetSpellInfo(tonumber(v.spellId) or 0);
			return format("%d  %s  (%s to %s)", tonumber(v.spellId) or 0, name or "?", Name("ItemQuality", v.minQuality), Name("ItemQuality", v.maxQuality));
		end,
		sort = SortBy("spellId", "minQuality"),
		defaults = { minQuality = 2, maxQuality = 5, itemClass = -1, subClass = -1, inventoryType = -1, statGroup = -1, minItemLevel = -1, maxItemLevel = -1, SpellTrigger = 1, SpellCooldown = -1, SpellCategoryCooldown = -1 },
	},
	{
		title = "Names", table = "item_generator_names", keys = NAME_KEYS, keyText = function(key) return Name("ItemGenNameList", key); end,
		help = "Words the name generator picks from, one list at a time.",
		summary = function(v)
			local class = tonumber(v.itemtype) or -1;
			local kind = class < 0 and "any" or SubclassText(class, v.subclass);
			return format("%s  |cff7f7f7f%s|r", tostring(v.string or ""), kind);
		end,
		sort = function(a, b) return tostring(a.string) < tostring(b.string); end,
		defaults = { itemtype = -1, subclass = -1 },
	},
	{
		title = "Displays", table = "item_generator_displays", keys = INVENTORY_KEYS, keyText = function(key) return Name("InventoryType", key); end,
		help = "Display ids a generated item of this slot can look like, picked by quality and subclass.",
		summary = function(v) return format("%d  %s  %s", tonumber(v.displayId) or 0, Name("ItemQuality", v.quality), SubclassText(v.classType, v.subclassType)); end,
		sort = SortBy("quality", "subclassType", "displayId"),
		defaults = { quality = 2, classType = 4 },
	},
	{ header = "Stat groups" },
	{
		title = "Stat groups", table = "item_generator_stat_group", parent = true,
		help = "Stat pools an item rolls from. 17, 18 and 19 are internal pools the generator needs. Deleting a group leaves its stats rows.",
		keyName = function(v) return tostring(v.name or ""); end,
		defaults = { flags = 1, weight = 1, trinketGroup = -1, staminaMod = 1, armorClothMod = 1, armorLeatherMod = 1, armorMailMod = 1, armorPlateMod = 1 },
		children = {
			{
				title = "Stats", table = "item_generator_stat_group_stats",
				summary = function(v)
					local kind = tonumber(v.type) or 0;
					local value = kind == 2 and Name("ItemSocketColor", v.value) or Name("ItemModType", v.value);
					return format("%s %d  %s", kind == 0 and "P" or (kind == 1 and "S" or "Socket"), tonumber(v.idx) or 0, value);
				end,
				sort = SortBy("type", "idx"), unique = { { "type", "idx" } },
				fields = statFields, follow = StatFollow, followColumn = "type",
				newValues = function(editor)
					local kind = editor.current and tonumber(editor.current.values.type) or 0;
					return { type = kind, idx = NextIndex(editor, "idx", function(v) return (tonumber(v.type) or 0) == kind; end) };
				end,
				duplicate = function(editor, values)
					values.idx = NextIndex(editor, "idx", function(v) return (tonumber(v.type) or 0) == (tonumber(values.type) or 0); end);
				end,
			},
		},
	},
	{
		title = "Stat group mask", table = "item_generator_stat_group_mask", whole = true,
		help = "Names of the stat group mask bits. Spell and legendary filters test these bits.",
		summary = function(v) return format("%d  %s", tonumber(v.bit) or 0, tostring(v.name or "")); end,
		sort = SortBy("bit"), unique = { { "bit" } },
	},
	{ header = "Unique and legendary" },
	{
		title = "Unique items", table = "item_generator_unique", parent = true,
		help = "Authored items an item_template points at with uniqueId. -1 keeps the base value or lets the generator derive it.",
		keyName = function(v) return (v.name and v.name ~= "") and tostring(v.name) or tostring(v.comment or ""); end,
		fields = uniqueFields, follow = uniqueFollow, followColumn = "itemClass",
		defaults = {
			quality = 4, itemClass = 4, bonding = -1, requiredLevel = -1, itemLimitCategory = -1, statGroup = -1,
			armorMin = -1, armorMax = -1, blockMin = -1, blockMax = -1, delayMin = -1, delayMax = -1,
			dmgMinLow = -1, dmgMinHigh = -1, dmgMaxLow = -1, dmgMaxHigh = -1, dmg2MinLow = -1, dmg2MinHigh = -1, dmg2MaxLow = -1, dmg2MaxHigh = -1,
			socketColor1 = -1, socketColor2 = -1, socketColor3 = -1, socketBonus = -1,
		},
		children = {
			{
				title = "Stats", table = "item_generator_unique_stat",
				summary = function(v) return format("%d  %s  %d to %d", tonumber(v.idx) or 0, Name("ItemModType", v.statType), tonumber(v.minValue) or 0, tonumber(v.maxValue) or 0); end,
				sort = SortBy("idx"), unique = { { "idx" } },
				newValues = function(editor) return { idx = NextIndex(editor, "idx") }; end,
				duplicate = function(editor, values) values.idx = NextIndex(editor, "idx"); end,
			},
			{
				title = "Spells", table = "item_generator_unique_spell",
				summary = function(v)
					local name = GetSpellInfo(tonumber(v.spellId) or 0);
					return format("%d  %d %s", tonumber(v.idx) or 0, tonumber(v.spellId) or 0, name or "");
				end,
				sort = SortBy("idx"), unique = { { "idx" } },
				defaults = { spellTrigger = 1, spellCooldown = -1, spellCategoryCooldown = -1 },
				newValues = function(editor) return { idx = NextIndex(editor, "idx") }; end,
				duplicate = function(editor, values) values.idx = NextIndex(editor, "idx"); end,
				validate = function(editor)
					for _, wrapper in ipairs(editor.wrappers) do
						local idx = tonumber(wrapper.values.idx) or 0;

						if ( idx < 0 or idx > 4 ) then
							return "Spell slots go from 0 to 4";
						end
					end
				end,
			},
		},
	},
	{
		title = "Legendaries", table = "item_generator_legendary_template", parent = true,
		help = "Legendary effects. The spell set is shared by every legendary with the same set number.",
		keyName = function(v) return tostring(v.Comment or v.nameOverride or ""); end,
		defaults = { itemClass = -1, itemSubClass = -1, itemInventoryType = -1, itemStatGroup = -1, statGroupOverride = -1, primaryStatModifier = 1, secondaryStatModifier = 1 },
		preview = I.CreateLegendaryPreview,
		children = {
			{
				title = "Spell set", table = "item_generator_legendary_spell_entry", keyFrom = "legendarySpellTemplateId",
				summary = function(v)
					local name = GetSpellInfo(tonumber(v.SpellId) or 0);
					return format("%d  %d %s", tonumber(v.spellIndex) or 0, tonumber(v.SpellId) or 0, name or "");
				end,
				sort = SortBy("spellIndex"), unique = { { "spellIndex" } },
				defaults = { SpellTrigger = 1, SpellCooldown = -1, SpellCategoryCooldown = -1 },
				newValues = function(editor) return { spellIndex = NextIndex(editor, "spellIndex") }; end,
				duplicate = function(editor, values) values.spellIndex = NextIndex(editor, "spellIndex"); end,
			},
		},
	},
	{ header = "Labels" },
	{
		title = "Item sets", table = "item_generator_itemsets", whole = true,
		help = "Item set rules. The server does not load this table yet. A whole table save needs at least one row.",
		summary = function(v) return format("%d  %s", tonumber(v.setid) or 0, tostring(v.comment or "")); end,
		sort = SortBy("setid"),
		defaults = { class = -1, subclass = -1, inventorytype = -1, statgroup = -1 },
	},
	{
		title = "Quality labels", table = "item_generator_quality_type", whole = true,
		help = "Reference names only, nothing on the server reads them.",
		summary = function(v) return format("%d  %s", tonumber(v.id) or 0, tostring(v.name or "")); end,
		sort = SortBy("id"), unique = { { "id" } },
	},
	{
		title = "Slot labels", table = "item_generator_slot_type", whole = true,
		help = "Reference names only, nothing on the server reads them.",
		summary = function(v) return format("%d  %s", tonumber(v.id) or 0, tostring(v.name or "")); end,
		sort = SortBy("id"), unique = { { "id" } },
	},
	{
		title = "Stat labels", table = "item_generator_stat_type", whole = true,
		help = "Reference names only, nothing on the server reads them.",
		summary = function(v) return format("%d  %s", tonumber(v.id) or 0, tostring(v.name or "")); end,
		sort = SortBy("id"), unique = { { "id" } },
	},
};

local frame, nav, content;
local current;

-- Every row editor the view has built, for dirty checks and Save all.
local function Editors(section)
	local list = {};

	if ( section.editor ) then
		table.insert(list, section.editor);
	end

	for _, child in ipairs(section.childEditors or {}) do
		table.insert(list, child);
	end

	return list;
end

local function SectionDirty(section)
	for _, editor in ipairs(Editors(section)) do
		if ( editor:HasChanges() ) then
			return true;
		end
	end

	return false;
end

local function RefreshNav()
	if ( nav ) then
		NoggitListBox_Refresh(nav);
	end
end

local function RowSpec(section, extra)
	local spec = {
		table = section.table,
		title = section.title,
		groups = Data.Groups[section.table],
		fields = section.fields,
		summary = section.summary,
		sort = section.sort,
		unique = section.unique,
		defaults = section.defaults,
		newValues = section.newValues,
		duplicate = section.duplicate,
		validate = section.validate,
		whole = section.whole,
		keepRows = section.whole,
		labelWidth = 160,
		listWidth = 260,
		filterGrid = false,
		onDirty = RefreshNav,
	};

	if ( section.follow ) then
		spec.onChange = function(editor, column)
			if ( column == section.followColumn ) then
				section.follow(editor);
			end
		end;
		spec.onLoaded = section.follow;
	end

	for key, value in pairs(extra or {}) do
		spec[key] = value;
	end

	return spec;
end

local function FollowSelection(section, editor)
	if ( section.follow ) then
		local select = editor.Select;

		editor.Select = function(self, wrapper)
			select(self, wrapper);
			section.follow(self);
		end;
	end
end

local function HelpLabel(parent, section)
	local help = Common.Paragraph(parent, section.help or "");
	help:SetPoint("TOPLEFT", parent, "TOPLEFT", 2, 0);
	help:SetPoint("TOPRIGHT", parent, "TOPRIGHT", -2, 0);
	help:SetHeight(28);
	return help;
end

-- Whole table view ---------------------------------------------------------------------------------------------

local function BuildWhole(section)
	local view = CreateFrame("Frame", nil, content);
	view:SetAllPoints(content);

	local help = HelpLabel(view, section);

	section.editor = I.CreateRowEditor(view, RowSpec(section));
	section.editor:SetPoint("TOPLEFT", help, "BOTTOMLEFT", 0, -4);
	section.editor:SetPoint("BOTTOMRIGHT", view, "BOTTOMRIGHT", 0, 0);
	FollowSelection(section, section.editor);
	section.editor:SetKey(I.WHOLE_KEY);

	view.Reload = function() section.editor:Reload(); end;
	return view;
end

-- Keyed by a fixed list of keys --------------------------------------------------------------------------------

local function BuildKeyed(section)
	local view = CreateFrame("Frame", nil, content);
	view:SetAllPoints(content);

	local help = HelpLabel(view, section);

	local items = {};

	for _, key in ipairs(section.keys) do
		table.insert(items, { text = format("%d  %s", key, section.keyText(key)), value = key });
	end

	local label = Common.Label(view, "Key", nil, NOGGIT_COLOR_TEXT);
	label:SetPoint("TOPLEFT", help, "BOTTOMLEFT", 0, -8);

	section.editor = I.CreateRowEditor(view, RowSpec(section, { readOnly = { inventoryType = true, array_id = true } }));

	local combo;
	combo = NoggitComboBox_Create(view, 300, items, 1, function(index)
		local key = items[index].value;

		if ( key == section.editor.key ) then
			return;
		end

		section.editor:ConfirmDiscard(function()
			section.editor:SetKey(key);
		end);

		-- The combo shows the key that stays open until the question is answered.
		for other, item in ipairs(items) do
			if ( item.value == section.editor.key ) then
				NoggitComboBox_SetSelected(combo, other, true);
			end
		end
	end);
	combo:SetPoint("LEFT", label, "RIGHT", 8, 0);

	section.editor:SetPoint("TOPLEFT", help, "BOTTOMLEFT", 0, -36);
	section.editor:SetPoint("BOTTOMRIGHT", view, "BOTTOMRIGHT", 0, 0);
	FollowSelection(section, section.editor);
	section.editor:SetKey(items[1] and items[1].value);

	view.Reload = function() section.editor:Reload(); end;
	return view;
end

-- Parent table with child rows ------------------------------------------------------------------------------------

local idWindow;

local function AskId(title, suggestion, onAccept)
	if ( not idWindow ) then
		idWindow = GameEditor_CreateWindow("GameEditorItemGenIdWindow", "", 300, 110);
		local body = idWindow.content;

		local label = Common.Label(body, "ID");
		label:SetPoint("TOPLEFT", body, "TOPLEFT", 2, -4);

		idWindow.box = NoggitLineEdit_Create(body, 120);
		idWindow.box:SetHeight(22);
		idWindow.box:SetPoint("TOPLEFT", body, "TOPLEFT", 40, 0);
		idWindow.box:SetAutoFocus(false);

		local function Accept()
			local id = tonumber(strtrim(idWindow.box:GetText() or ""));

			if ( not id or id < 0 or id ~= floor(id) ) then
				GameEditor_Notify("Type a whole number of 0 or more", "info");
				return;
			end

			GameEditor_HideWindow(idWindow);
			idWindow.onAccept(id);
		end

		idWindow.box:SetScript("OnEnterPressed", function(self)
			self:ClearFocus();
			Accept();
		end);
		idWindow.box:SetScript("OnEscapePressed", function(self) self:ClearFocus(); end);

		local cancel = Common.Button(body, "Cancel", 70, function() GameEditor_HideWindow(idWindow); end);
		cancel:SetPoint("BOTTOMRIGHT", body, "BOTTOMRIGHT", 0, 0);
		local ok = Common.Button(body, "Create", 70, Accept, "Open the new row, nothing is written until you save");
		ok:SetPoint("RIGHT", cancel, "LEFT", -4, 0);
	end

	idWindow:SetTitle(title);
	idWindow.onAccept = onAccept;
	idWindow.box:SetText(tostring(suggestion or ""));
	GameEditor_ShowWindow(idWindow);
	idWindow.box:SetFocus();
	idWindow.box:HighlightText();
end

local function BuildParent(section)
	local view = CreateFrame("Frame", nil, content);
	view:SetAllPoints(content);

	local help = HelpLabel(view, section);
	local keys = {};
	local selected;

	local left = CreateFrame("Frame", nil, view);
	left:SetPoint("TOPLEFT", help, "BOTTOMLEFT", 0, -4);
	left:SetPoint("BOTTOMLEFT", view, "BOTTOMLEFT", 0, 0);
	left:SetWidth(KEY_LIST_WIDTH);

	local right = CreateFrame("Frame", nil, view);
	right:SetPoint("TOPLEFT", left, "TOPRIGHT", 8, 0);
	right:SetPoint("BOTTOMRIGHT", view, "BOTTOMRIGHT", 0, 0);

	local OnNew;
	local newButton = Common.Button(left, "New", 50, function() OnNew(); end, "Start a new row with an ID you choose");
	newButton:SetPoint("TOPLEFT", left, "TOPLEFT", 0, 0);
	local refresh = Common.Button(left, "Refresh", 64, function() I.Query(section.table, I.LIST_KEY); end, "Load the list again from the server");
	refresh:SetPoint("LEFT", newButton, "RIGHT", 4, 0);

	local filter = NoggitLineEdit_Create(left, KEY_LIST_WIDTH);
	filter:SetHeight(22);
	filter:SetPoint("TOPLEFT", newButton, "BOTTOMLEFT", 0, -4);
	filter:SetPoint("RIGHT", left, "RIGHT", 0, 0);
	filter:SetAutoFocus(false);
	filter:SetScript("OnEscapePressed", function(self)
		self:SetText("");
		self:ClearFocus();
	end);
	filter:SetScript("OnEnterPressed", function(self) self:ClearFocus(); end);

	local listStatus = Common.Label(left, "", nil, NOGGIT_COLOR_DISABLED);
	listStatus:SetPoint("BOTTOMLEFT", left, "BOTTOMLEFT", 2, 2);

	local list = NoggitListBox_Create(left, 20, function(row)
		row.id = NoggitUI_FontString(row, "");
		row.id:SetPoint("LEFT", row, "LEFT", 4, 0);
		row.id:SetWidth(40);
		NoggitText_SetColor(row.id, NOGGIT_COLOR_DISABLED);
		row.name = NoggitUI_FontString(row, "");
		row.name:SetPoint("LEFT", row, "LEFT", 46, 0);
		row.name:SetPoint("RIGHT", row, "RIGHT", -2, 0);
	end, function(row, item)
		row.id:SetText(tostring(item.value));
		row.name:SetText((item.value == selected and "|cffffd100" or "")..(item.text ~= "" and item.text or "|cff7f7f7f(no name)|r"));
	end);
	list:SetPoint("TOPLEFT", filter, "BOTTOMLEFT", 0, -4);
	list:SetPoint("RIGHT", left, "RIGHT", 0, 0);
	list:SetPoint("BOTTOM", listStatus, "TOP", 0, 4);

	local function RefreshKeys()
		local text = string.lower(strtrim(filter:GetText() or ""));
		local items = {};

		for _, item in ipairs(keys) do
			if ( text == "" or string.find(string.lower(item.value.."  "..item.text), text, 1, true) ) then
				table.insert(items, item);
			end
		end

		NoggitListBox_SetItems(list, items, true);
		list.selected = nil;

		for index, item in ipairs(items) do
			if ( item.value == selected ) then
				list.selected = index;
			end
		end

		NoggitListBox_Refresh(list);
		listStatus:SetText(format("%d row%s", #keys, #keys == 1 and "" or "s"));
	end

	filter:SetScript("OnTextChanged", RefreshKeys);

	local function LoadKeys()
		local data = I.GetSet(section.table, I.LIST_KEY);
		keys = {};

		for _, row in ipairs(data and data.rows or {}) do
			local id = tonumber(row[data.keyColumn or "id"]) or 0;
			table.insert(keys, { value = id, text = section.keyName(row), row = row });
		end

		table.sort(keys, function(a, b) return a.value < b.value; end);
		RefreshKeys();
	end

	-- Detail of the selected parent row, the children sit in tabs next to it.
	local names = { "Details" };

	for _, child in ipairs(section.children or {}) do
		table.insert(names, child.title);
	end

	local panels = {};
	local strip = Common.CreateTabs(right, names, function(index)
		for other, panel in ipairs(panels) do
			if ( other == index ) then
				panel:Show();
			else
				panel:Hide();
			end
		end
	end);
	strip.tabs[1]:SetPoint("TOPLEFT", right, "TOPLEFT", 0, 0);

	local holder = CreateFrame("Frame", nil, right);
	holder:SetPoint("TOPLEFT", right, "TOPLEFT", 0, -28);
	holder:SetPoint("BOTTOMRIGHT", right, "BOTTOMRIGHT", 0, 0);

	local emptyText = Common.Paragraph(right, "Pick a row on the left, or New to start one.");
	emptyText:SetPoint("TOPLEFT", holder, "TOPLEFT", 4, -8);
	emptyText:SetPoint("TOPRIGHT", holder, "TOPRIGHT", -4, -8);

	section.childEditors = {};

	-- The preview reads the open row and its first child set, unsaved edits included.
	local preview;

	if ( section.preview ) then
		preview = section.preview(right, function()
			local values = selected and section.editor and section.editor.current and section.editor.current.values;

			if ( not values ) then
				return nil;
			end

			local child = section.childEditors[1];
			local rows = {};

			if ( child and child.loaded ) then
				for _, wrapper in ipairs(child.wrappers) do
					table.insert(rows, wrapper.values);
				end

				if ( child.spec.sort ) then
					table.sort(rows, child.spec.sort);
				end
			end

			return values, rows, child and child.loaded and child.key or nil, (child and child.loading) or section.editor.loading;
		end);
		preview:SetPoint("TOPRIGHT", right, "TOPRIGHT", 0, 0);
		preview:SetPoint("BOTTOMRIGHT", right, "BOTTOMRIGHT", 0, 0);
		holder:ClearAllPoints();
		holder:SetPoint("TOPLEFT", right, "TOPLEFT", 0, -28);
		holder:SetPoint("BOTTOMRIGHT", preview, "BOTTOMLEFT", -8, 0);
	end

	local function UpdateTabs()
		if ( not section.editor ) then
			return;
		end

		strip:SetDirty(1, section.editor:HasChanges());

		for index, child in ipairs(section.childEditors) do
			strip:SetDirty(index + 1, child:HasChanges());
		end

		if ( preview ) then
			preview:Refresh();
		end

		RefreshNav();
	end

	local function ChildKey(spec, values)
		if ( spec.keyFrom ) then
			return tonumber(values and values[spec.keyFrom]) or 0;
		end

		return selected;
	end

	local detailSpec = RowSpec(section, {
		single = true,
		canDelete = true,
		readOnly = { id = true, Id = true },
		labelWidth = 170,
		onDirty = UpdateTabs,
		onLoaded = function(editor)
			if ( section.follow ) then
				section.follow(editor);
			end

			-- Children keyed on a column of the parent follow its saved value.
			local values = editor.current and editor.current.orig;

			for index, spec in ipairs(section.children or {}) do
				if ( spec.keyFrom and values ) then
					local key = ChildKey(spec, values);
					local child = section.childEditors[index];

					if ( child.key ~= key and not child:HasChanges() ) then
						child:SetKey(key);
						child:SetTitle(format("%s %d", spec.title, key));
					end
				end
			end
		end,
	});
	detailSpec.title = section.title;

	section.editor = I.CreateRowEditor(holder, detailSpec);
	section.editor:SetAllPoints(holder);
	FollowSelection(section, section.editor);
	table.insert(panels, section.editor);

	for _, spec in ipairs(section.children or {}) do
		local childSpec = RowSpec(spec, { readOnly = { groupId = true, uniqueId = true, legendaryIndex = true }, onDirty = UpdateTabs });
		local child = I.CreateRowEditor(holder, childSpec);
		child:SetAllPoints(holder);
		FollowSelection(spec, child);
		child:Hide();
		table.insert(section.childEditors, child);
		table.insert(panels, child);
	end

	local function Dirty()
		for _, editor in ipairs(Editors(section)) do
			if ( editor:HasChanges() ) then
				return true;
			end
		end

		return false;
	end

	local function Open(id, seed)
		selected = id;
		emptyText:Hide();
		section.editor:SetTitle(format("%s %d", section.title, id));
		section.editor:SetKey(id, seed);

		for index, spec in ipairs(section.children or {}) do
			local child = section.childEditors[index];

			if ( spec.keyFrom ) then
				child:SetKey(nil);
				child:SetTitle(spec.title);
			else
				child:SetKey(id);
				child:SetTitle(format("%s of %d", spec.title, id));
			end
		end

		strip:Select(strip.current or 1);
		RefreshKeys();

		if ( preview ) then
			preview:Refresh();
		end
	end

	local function ConfirmThen(action)
		if ( not Dirty() ) then
			action();
			return;
		end

		NoggitMessageBox_Show("Unsaved changes", format("%s %d has unsaved changes. Drop them?", section.title, selected or 0), {
			{ text = "Drop changes", onClick = action },
			{ text = "Cancel" },
		});
	end

	list.onSelect = function(item)
		if ( item.value ~= selected ) then
			ConfirmThen(function() Open(item.value); end);
			RefreshKeys();
		end
	end;

	OnNew = function()
		local highest = 0;

		for _, item in ipairs(keys) do
			highest = max(highest, item.value);
		end

		AskId("New row of "..section.title, highest + 1, function(id)
			for _, item in ipairs(keys) do
				if ( item.value == id ) then
					GameEditor_Notify(format("%d already exists, it is open now", id), "info");
					ConfirmThen(function() Open(id); end);
					return;
				end
			end

			ConfirmThen(function()
				local seed = I.Copy(section.defaults);
				seed[section.table == "item_generator_legendary_template" and "Id" or "id"] = id;
				Open(id, seed);
			end);
		end);
	end;

	for index = 2, #panels do
		panels[index]:Hide();
	end

	section.editor:Hide();
	emptyText:Show();

	-- The detail panel shows once a row is open.
	local baseSelect = strip.Select;

	strip.Select = function(self, index)
		baseSelect(self, index);

		if ( not selected ) then
			for _, panel in ipairs(panels) do
				panel:Hide();
			end
		end
	end;

	I.On("ROWS", function(tableName, key)
		if ( tableName == section.table and key == I.LIST_KEY ) then
			LoadKeys();
		end
	end);

	I.On("RESULT", function(tableName, key, ok)
		if ( ok and tableName == section.table and key ~= I.LIST_KEY ) then
			I.Query(section.table, I.LIST_KEY);
		end
	end);

	I.Query(section.table, I.LIST_KEY);
	LoadKeys();

	view.Reload = function()
		I.Query(section.table, I.LIST_KEY);

		for _, editor in ipairs(Editors(section)) do
			if ( editor.key ~= nil ) then
				editor:Reload();
			end
		end
	end;

	return view;
end

-- View ---------------------------------------------------------------------------------------------------------

local function ShowSection(section)
	if ( current and current.view ) then
		current.view:Hide();
	end

	current = section;

	if ( not section.view ) then
		if ( section.parent ) then
			section.view = BuildParent(section);
		elseif ( section.keys ) then
			section.view = BuildKeyed(section);
		else
			section.view = BuildWhole(section);
		end
	end

	section.view:Show();
	RefreshNav();
end

function I.BuildGenerator(parent, leftOffset)
	frame = CreateFrame("Frame", "GameEditorItemGenerator", parent, "NoggitBoxTemplate");
	NoggitBox_Set(frame, NOGGIT_COLOR_BASE, NOGGIT_COLOR_MID);
	frame:SetPoint("TOPLEFT", parent, "TOPLEFT", leftOffset or 56, -6);
	frame:SetPoint("BOTTOMRIGHT", parent, "BOTTOMRIGHT", -8, 6);
	frame:EnableMouse(true);
	frame:EnableMouseWheel(true);

	local title = Common.Label(frame, "Item generator", "NoggitFont16", NOGGIT_COLOR_WHITE);
	title:SetPoint("TOPLEFT", frame, "TOPLEFT", 10, -8);

	local items = {};

	for _, section in ipairs(SECTIONS) do
		table.insert(items, { section = section });
	end

	nav = NoggitListBox_Create(frame, 20, function(row)
		row.text = NoggitUI_FontString(row, "");
		row.text:SetPoint("LEFT", row, "LEFT", 4, 0);
		row.text:SetPoint("RIGHT", row, "RIGHT", -2, 0);
	end, function(row, item)
		if ( not item ) then
			row.text:SetText("");
			return;
		end

		local section = item.section;

		if ( section.header ) then
			row.text:SetText("|cff7f7f7f"..section.header.."|r");
		else
			local text = "   "..section.title;

			if ( SectionDirty(section) ) then
				text = text.." |cffffcc00*|r";
			end

			row.text:SetText(section == current and ("|cffffd100"..text.."|r") or text);
		end
	end);
	nav:SetPoint("TOPLEFT", frame, "TOPLEFT", 8, -32);
	nav:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 8, 8);
	nav:SetWidth(NAV_WIDTH);
	nav.onSelect = function(item)
		if ( item.section.header ) then
			RefreshNav();
			return;
		end

		ShowSection(item.section);
	end;

	content = CreateFrame("Frame", "GameEditorItemGeneratorContent", frame);
	content:SetPoint("TOPLEFT", nav, "TOPRIGHT", 10, 0);
	content:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -8, 8);

	NoggitListBox_SetItems(nav, items);
	nav.selected = 2;
	ShowSection(SECTIONS[2]);
	I.EnsureLists();
	return frame;
end

-- Generator wide actions for the workspace keys.
I.Generator = {};

function I.Generator.HasChanges()
	for _, section in ipairs(SECTIONS) do
		if ( not section.header and SectionDirty(section) ) then
			return true;
		end
	end

	return false;
end

function I.Generator.SaveAll()
	local saved = 0;

	for _, section in ipairs(SECTIONS) do
		if ( not section.header ) then
			for _, editor in ipairs(Editors(section)) do
				if ( editor:HasChanges() and editor:Save() ) then
					saved = saved + 1;
				end
			end
		end
	end

	if ( saved == 0 ) then
		GameEditor_Notify("Nothing to save", "info");
	end

	return saved;
end

function I.Generator.Reload()
	if ( not current or not current.view ) then
		return;
	end

	local section = current;

	if ( not SectionDirty(section) ) then
		section.view.Reload();
		return;
	end

	NoggitMessageBox_Show("Unsaved changes", format("%s has unsaved changes. Drop them and load again?", section.title), {
		{ text = "Drop changes", onClick = function() section.view.Reload(); end },
		{ text = "Cancel" },
	});
end

function I.Generator.DirtyCount()
	local count = 0;

	for _, section in ipairs(SECTIONS) do
		if ( not section.header and SectionDirty(section) ) then
			count = count + 1;
		end
	end

	return count;
end

function I.Generator.CurrentTitle()
	return current and current.title;
end
