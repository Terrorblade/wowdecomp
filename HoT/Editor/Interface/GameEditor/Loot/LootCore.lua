-- Loot tables through the world tables: loading, edits, saving and the item names and icons.

GameEditorLoot = GameEditorLoot or {};
local L = GameEditorLoot;

-- Same order as EditorLootTable on the server, the loot searches send these indices.
L.TABLES = {
	[0] = { table = "creature_loot_template", title = "Creature loot", short = "loot", source = "creature", field = "lootid" },
	[1] = { table = "pickpocketing_loot_template", title = "Pickpocket loot", short = "pickpocket", source = "creature", field = "pickpocketloot" },
	[2] = { table = "skinning_loot_template", title = "Skinning loot", short = "skinning", source = "creature", field = "skinloot" },
	[3] = { table = "gameobject_loot_template", title = "Gameobject loot", short = "object", source = "gameobject", field = "Data1" },
	[4] = { table = "item_loot_template", title = "Item contents", short = "contents", source = "item" },
	[5] = { table = "disenchant_loot_template", title = "Disenchant loot", short = "disenchant", source = "item", field = "DisenchantID" },
	[6] = { table = "prospecting_loot_template", title = "Prospecting loot", short = "prospecting", source = "item" },
	[7] = { table = "milling_loot_template", title = "Milling loot", short = "milling", source = "item" },
	[8] = { table = "fishing_loot_template", title = "Fishing loot", short = "fishing", source = "fishing" },
	[9] = { table = "spell_loot_template", title = "Spell loot", short = "spell", source = "spell" },
	[10] = { table = "mail_loot_template", title = "Mail loot", short = "mail", source = "mail" },
	[11] = { table = "reference_loot_template", title = "Reference loot", short = "reference", source = "reference" },
};

L.REFERENCE = 11;
L.REFERENCE_TABLE = "reference_loot_template";

L.SOURCES = {
	creature = { title = "Creature", tables = { 0, 1, 2 } },
	gameobject = { title = "Gameobject", tables = { 3 } },
	item = { title = "Item", tables = { 4, 5, 6, 7 } },
	fishing = { title = "Fishing area", tables = { 8 } },
	spell = { title = "Spell", tables = { 9 } },
	mail = { title = "Mail template", tables = { 10 } },
	reference = { title = "Reference", tables = { 11 } },
};

L.SOURCE_ORDER = { "creature", "gameobject", "item", "fishing", "spell", "mail", "reference" };

L.byTable = {};

for index, info in pairs(L.TABLES) do
	info.index = index;
	L.byTable[info.table] = info;
end

-- Columns the editor knows, the rest of a row is kept as the server sent it.
L.DEFAULTS = {
	Item = 0, Reference = 0, Chance = 100, QuestRequired = 0, LootMode = 1, GroupId = 0, MinCount = 1, MaxCount = 1,
	Comment = "", RequiredDungeonLevel = 0, MapId = 0, MagicFindChanceCoef = 0, MagicFindAmountCoef = 0,
};

L.FALLBACK_COLUMNS = {
	"Item", "Reference", "Chance", "QuestRequired", "LootMode", "GroupId", "MinCount", "MaxCount",
	"Comment", "RequiredDungeonLevel", "MapId", "MagicFindChanceCoef", "MagicFindAmountCoef",
};

L.REFERENCE_ICON = "Interface\\Icons\\INV_Misc_Bag_10";
L.UNKNOWN_ICON = "Interface\\Icons\\INV_Misc_QuestionMark";

local QUALITY_COLORS = {
	[0] = "ff9d9d9d", [1] = "ffffffff", [2] = "ff1eff00", [3] = "ff0070dd", [4] = "ffa335ee", [5] = "ffff8000", [6] = "ffe6cc80", [7] = "ffe6cc80",
};

local sets = {};
local listeners = {};
local nextUid = 0;

local function Num(value)
	return tonumber(value) or 0;
end

L.Num = Num;

function L.SetId(tableName, key)
	return string.lower(tableName)..":"..tostring(key);
end

function L.On(callback)
	table.insert(listeners, callback);
end

function L.Changed(set)
	for _, callback in ipairs(listeners) do
		pcall(callback, set);
	end
end

-- Items -------------------------------------------------------------------------------------------------------

local cacheTooltip = CreateFrame("GameTooltip", "GameEditorLootCacheTooltip", nil, "GameTooltipTemplate");
local cacheAsked = {};
local searchNames = {};

-- Asks the server for an item the loot query did not push, like a row typed in by hand.
function L.CacheItem(id)
	id = Num(id);

	if ( id <= 0 or cacheAsked[id] or GetItemInfo(id) ) then
		return;
	end

	cacheAsked[id] = true;
	cacheTooltip:SetOwner(UIParent, "ANCHOR_NONE");
	pcall(cacheTooltip.SetHyperlink, cacheTooltip, "item:"..id);
	cacheTooltip:Hide();
end

function L.RememberItem(id, name, quality)
	if ( id and name and name ~= "" ) then
		searchNames[id] = { name = name, quality = quality };
	end
end

-- L.ItemInfo(id) -> name, quality, icon, known
function L.ItemInfo(id)
	id = Num(id);

	if ( id <= 0 ) then
		return nil, 1, L.UNKNOWN_ICON, false;
	end

	local name, _, quality, _, _, _, _, _, _, texture = GetItemInfo(id);
	local icon = texture or (GetItemIcon and GetItemIcon(id));

	if ( name ) then
		return name, quality or 1, icon or L.UNKNOWN_ICON, true;
	end

	local remembered = searchNames[id];
	L.CacheItem(id);
	return remembered and remembered.name, remembered and remembered.quality or 1, icon or L.UNKNOWN_ICON, false;
end

function L.Colored(text, quality)
	return format("|c%s%s|r", QUALITY_COLORS[quality or 1] or QUALITY_COLORS[1], text or "");
end

function L.ItemText(id)
	local name, quality = L.ItemInfo(id);

	if ( name ) then
		return L.Colored(name, quality);
	end

	return format("|cff7f7f7fitem %d|r", Num(id));
end

-- Item name search for the pickers, the item search also fills the item cache.
local itemSearch;

function L.SearchItems(text)
	text = strtrim(text or "");
	local items = {};
	local number = tonumber(text);

	if ( number ) then
		table.insert(items, { value = number, text = L.ItemText(number) });
		return items;
	end

	if ( string.len(text) >= 3 and TemplateSearch_Query and (not itemSearch or itemSearch.text ~= text) ) then
		itemSearch = { text = text, requestId = TemplateSearch_Query("item", text, 300) };
	end

	if ( itemSearch and itemSearch.text == text and itemSearch.results ) then
		for _, item in ipairs(itemSearch.results) do
			table.insert(items, item);
		end
	end

	return items;
end

local function OnItemResults(requestId)
	local data = TemplateSearch_GetResults and TemplateSearch_GetResults("item");

	if ( not itemSearch or not data or itemSearch.requestId ~= requestId ) then
		return;
	end

	local results = {};

	for _, row in ipairs(data.rows or {}) do
		L.RememberItem(row.entry, row.name, row.type);
		table.insert(results, { value = row.entry, text = format("%s  (ilvl %d)", L.Colored(row.name, row.type), row.maxLevel or 0) });
	end

	itemSearch.results = results;

	if ( GameEditorPicker and GameEditorPicker:IsShown() ) then
		GameEditorPicker.pendingSearch = 0.1;
	end
end

-- Source names ------------------------------------------------------------------------------------------------

function L.SourceName(source, entry)
	entry = Num(entry);

	if ( source == "creature" or source == "gameobject" ) then
		return GameEditorObject and GameEditorObject.NameOf(source, entry);
	elseif ( source == "item" ) then
		return (L.ItemInfo(entry));
	elseif ( source == "spell" ) then
		return GetSpellInfo(entry);
	elseif ( source == "fishing" ) then
		return GameEditor_ResolveRef and GameEditor_ResolveRef("dbc:AreaTable", entry);
	end
end

-- Sets --------------------------------------------------------------------------------------------------------

local function NewUid()
	nextUid = nextUid + 1;
	return nextUid;
end

local function CopyRow(row)
	local copy = {};

	for key, value in pairs(row) do
		copy[key] = value;
	end

	return copy;
end

L.CopyRow = CopyRow;

function L.NewRow(values)
	local row = {};

	for key, value in pairs(L.DEFAULTS) do
		row[key] = value;
	end

	for key, value in pairs(values or {}) do
		row[key] = value;
	end

	row._uid = NewUid();
	row._new = true;
	return row;
end

function L.IsReference(row)
	return Num(row and row.Reference) > 0;
end

local COMPARE = { "Item", "Reference", "Chance", "QuestRequired", "LootMode", "GroupId", "MinCount", "MaxCount", "Comment", "RequiredDungeonLevel", "MapId", "MagicFindChanceCoef", "MagicFindAmountCoef" };

local function SameValue(a, b)
	if ( type(a) == "number" or type(b) == "number" ) then
		local x, y = tonumber(a), tonumber(b);

		if ( not x or not y ) then
			return x == y;
		end

		return math.abs(x - y) <= 0.00001 * max(1, math.abs(x));
	end

	return tostring(a or "") == tostring(b or "");
end

function L.RowChanged(set, row)
	local original = set.originalByUid and set.originalByUid[row._uid];

	if ( not original ) then
		return true;
	end

	for _, key in ipairs(COMPARE) do
		if ( not SameValue(row[key], original[key]) ) then
			return true;
		end
	end

	return false;
end

function L.IsDirty(set)
	if ( not set or not set.loaded ) then
		return false;
	end

	if ( #set.rows ~= #set.original ) then
		return true;
	end

	for index, row in ipairs(set.rows) do
		if ( row._uid ~= set.original[index]._uid or L.RowChanged(set, row) ) then
			return true;
		end
	end

	return false;
end

function L.Get(tableName, key)
	return sets[L.SetId(tableName, key)];
end

function L.DirtySets()
	local list = {};

	for _, set in pairs(sets) do
		if ( L.IsDirty(set) ) then
			table.insert(list, set);
		end
	end

	return list;
end

local function SortRows(rows)
	table.sort(rows, function(a, b)
		local ra, rb = L.IsReference(a) and 1 or 0, L.IsReference(b) and 1 or 0;

		if ( ra ~= rb ) then
			return ra < rb;
		end

		if ( Num(a.GroupId) ~= Num(b.GroupId) ) then
			return Num(a.GroupId) < Num(b.GroupId);
		end

		return Num(a.Item) < Num(b.Item);
	end);
end

-- L.Load(table, key, force) asks the server for the rows unless they are here already, edits stay unless force.
function L.Load(tableName, key, force)
	key = Num(key);

	if ( key <= 0 or not WorldTable_Query ) then
		return nil;
	end

	local id = L.SetId(tableName, key);
	local set = sets[id];

	if ( not set ) then
		set = { table = tableName, key = key, rows = {}, original = {}, originalByUid = {} };
		sets[id] = set;
	end

	if ( set.loading or (set.loaded and not force) ) then
		return set;
	end

	set.loading = true;
	set.error = nil;
	WorldTable_Query(tableName, key);
	return set;
end

function L.Reload(set)
	if ( set ) then
		set.keepEdits = false;
		L.Load(set.table, set.key, true);
		L.Changed(set);
	end
end

local function OnRows(tableName, key, ok, message)
	local set = sets[L.SetId(tableName, key)];

	if ( not set ) then
		return;
	end

	set.loading = false;

	if ( ok ~= 1 and ok ~= true ) then
		set.error = (message and message ~= "") and message or "the server could not read the rows";
		GameEditor_Notify(format("%s %d: %s", tableName, set.key, set.error), "error");
		L.Changed(set);
		return;
	end

	local data = WorldTable_GetRows(tableName, key);
	local original = {};

	set.columns = {};
	set.keyColumn = data and data.keyColumn or "Entry";

	for _, column in ipairs(data and data.columns or {}) do
		if ( string.lower(column.name) ~= string.lower(set.keyColumn) ) then
			table.insert(set.columns, { name = column.name, type = column.type });
		end
	end

	for _, row in ipairs(data and data.rows or {}) do
		local copy = CopyRow(row);
		copy[set.keyColumn] = nil;
		copy._uid = NewUid();
		table.insert(original, copy);
	end

	SortRows(original);

	local keep = set.keepEdits and set.loaded and L.IsDirty(set);
	set.keepEdits = nil;
	set.original = original;
	set.originalByUid = {};

	for _, row in ipairs(original) do
		set.originalByUid[row._uid] = row;
	end

	if ( not keep ) then
		set.rows = {};

		for index, row in ipairs(original) do
			set.rows[index] = CopyRow(row);
		end
	end

	set.loaded = true;
	L.Changed(set);
end

function L.Revert(set)
	if ( not set or not set.loaded ) then
		return;
	end

	set.rows = {};

	for index, row in ipairs(set.original) do
		set.rows[index] = CopyRow(row);
	end

	L.Changed(set);
end

function L.AddRow(set, values)
	if ( not set or not set.loaded ) then
		return nil;
	end

	local row = L.NewRow(values);
	table.insert(set.rows, row);

	if ( Num(row.Item) > 0 and not L.IsReference(row) ) then
		L.CacheItem(row.Item);
	end

	L.Changed(set);
	return row;
end

function L.RemoveRow(set, row)
	for index, other in ipairs(set and set.rows or {}) do
		if ( other == row ) then
			table.remove(set.rows, index);
			L.Changed(set);
			return true;
		end
	end

	return false;
end

function L.SetValue(set, row, key, value)
	row[key] = value;

	if ( key == "Item" and not L.IsReference(row) ) then
		L.CacheItem(value);
	end

	L.Changed(set);
end

-- Same checks the server runs while loading, a row it would skip is refused here.
function L.Validate(set)
	local seen = {};

	for _, row in ipairs(set.rows) do
		local isReference = L.IsReference(row);
		local item = Num(row.Item);
		local label = isReference and format("Reference %d", Num(row.Reference)) or format("Item %d", item);

		if ( isReference and item == 0 ) then
			row.Item = Num(row.Reference);
			item = row.Item;
		end

		if ( item <= 0 ) then
			return false, "A row has no item, set one or delete the row";
		end

		if ( seen[item] ) then
			return false, format("Item column %d is used twice, every row of a loot id needs its own Item value", item);
		end

		seen[item] = true;

		local chance = Num(row.Chance);
		local group = Num(row.GroupId);
		local minCount = Num(row.MinCount);
		local maxCount = Num(row.MaxCount);

		if ( chance < 0 or chance > 100 ) then
			return false, label..": chance must be 0 to 100";
		end

		if ( group < 0 or group > 127 ) then
			return false, label..": group must be 0 to 127";
		end

		if ( minCount < 1 or minCount > 255 or maxCount < 0 or maxCount > 255 ) then
			return false, label..": min count must be 1 to 255, max count 0 to 255";
		end

		if ( Num(row.LootMode) < 0 or Num(row.LootMode) > 65535 ) then
			return false, label..": loot mode must be 0 to 65535";
		end

		if ( isReference ) then
			if ( chance <= 0 ) then
				return false, label..": a reference needs a chance above 0";
			end

			if ( Num(row.Reference) == Num(set.key) and set.table == L.REFERENCE_TABLE ) then
				return false, label..": a reference can not pull itself";
			end
		else
			if ( chance == 0 and group == 0 ) then
				return false, label..": chance 0 only works inside a group, it then shares the group chance";
			end

			if ( maxCount < minCount ) then
				return false, label..": max count is below min count";
			end
		end
	end

	return true;
end

function L.Save(set)
	if ( not L.IsDirty(set) or set.saving ) then
		return false;
	end

	local ok, message = L.Validate(set);

	if ( not ok ) then
		GameEditor_Notify(format("%s %d: %s", set.table, set.key, message), "error");
		return false;
	end

	local columns = {};

	if ( set.columns and #set.columns > 0 ) then
		for _, column in ipairs(set.columns) do
			table.insert(columns, { name = column.name, type = column.type });
		end
	else
		for _, name in ipairs(L.FALLBACK_COLUMNS) do
			table.insert(columns, name);
		end
	end

	local rows = {};

	for _, row in ipairs(set.rows) do
		local out = {};

		for key, value in pairs(row) do
			if ( type(key) == "string" and string.sub(key, 1, 1) ~= "_" ) then
				out[key] = value;
			end
		end

		if ( not L.IsReference(out) ) then
			out.Reference = 0;
		end

		table.insert(rows, out);
	end

	if ( not WorldTable_Save(set.table, set.key, columns, rows) ) then
		GameEditor_Notify("The loot rows could not be sent", "error");
		return false;
	end

	set.saving = true;
	L.Changed(set);
	return true;
end

function L.SaveAll(list)
	local count = 0;

	for _, set in ipairs(list or L.DirtySets()) do
		if ( L.Save(set) ) then
			count = count + 1;
		end
	end

	return count;
end

local function OnResult(ok, tableName, key, message)
	local set = sets[L.SetId(tableName, key)];

	if ( not set ) then
		return;
	end

	set.saving = false;

	if ( ok == 1 or ok == true ) then
		GameEditor_Notify(format("%s %d saved, new loot rolls use it", tableName, set.key), "info");
		L.Load(set.table, set.key, true);
	else
		GameEditor_Notify(format("%s %d: %s", tableName, set.key, message or "save failed"), "error");
	end

	L.Changed(set);
end

-- Group chance summary, explicit chances add up and the equal chanced rows share what is left.
function L.GroupSummary(set)
	local groups = {};

	for _, row in ipairs(set and set.rows or {}) do
		local group = Num(row.GroupId);

		if ( group > 0 and not L.IsReference(row) ) then
			local info = groups[group];

			if ( not info ) then
				info = { group = group, explicit = 0, equal = 0, count = 0 };
				groups[group] = info;
			end

			info.count = info.count + 1;

			if ( Num(row.Chance) > 0 ) then
				info.explicit = info.explicit + Num(row.Chance);
			else
				info.equal = info.equal + 1;
			end
		end
	end

	local list = {};

	for _, info in pairs(groups) do
		table.insert(list, info);
	end

	table.sort(list, function(a, b) return a.group < b.group; end);
	return list;
end

-- Where a reference is used, callback(hits) with { table, entry, name, text } for every loot row that pulls it.
local useSearch;

function L.FindReferenceUse(referenceId, callback)
	if ( not TemplateSearch_Query ) then
		return false;
	end

	useSearch = { id = Num(referenceId), callback = callback, requestId = TemplateSearch_Query("lootuse", tostring(Num(referenceId)), 500) };
	return useSearch.requestId ~= nil;
end

local function OnUseResults(requestId)
	local data = TemplateSearch_GetResults and TemplateSearch_GetResults("lootuse");

	if ( not useSearch or not data or useSearch.requestId ~= requestId ) then
		return;
	end

	local search = useSearch;
	useSearch = nil;
	local hits = {};

	for _, row in ipairs(data.rows or {}) do
		local info = L.TABLES[row.type or -1];

		if ( info ) then
			table.insert(hits, { table = info.table, info = info, entry = row.entry, name = row.name or "", text = row.subName or "" });
		end
	end

	search.callback(hits, data.truncated);
end

-- Source search for the browser, callback(rows, truncated) with the raw hits.
local sourceSearch;

function L.SearchSources(text, callback)
	if ( not TemplateSearch_Query ) then
		return false;
	end

	sourceSearch = { callback = callback, requestId = TemplateSearch_Query("loot", text, 600) };
	return sourceSearch.requestId ~= nil;
end

local function OnSourceResults(requestId)
	local data = TemplateSearch_GetResults and TemplateSearch_GetResults("loot");

	if ( not sourceSearch or not data or sourceSearch.requestId ~= requestId ) then
		return;
	end

	local search = sourceSearch;
	sourceSearch = nil;

	for _, row in ipairs(data.rows or {}) do
		local info = L.TABLES[row.type or -1];

		if ( info and (info.source == "creature" or info.source == "gameobject") and GameEditorObject ) then
			GameEditorObject.RememberName(info.source, row.entry, row.name);
		elseif ( info and info.source == "item" ) then
			L.RememberItem(row.entry, row.name, 1);
		end
	end

	search.callback(data.rows or {}, data.truncated);
end

local events = CreateFrame("Frame", "GameEditorLootEvents");
events:RegisterEvent("WORLD_TABLE_ROWS");
events:RegisterEvent("WORLD_TABLE_RESULT");
events:RegisterEvent("EDITOR_TEMPLATE_RESULTS");
events:SetScript("OnEvent", function(_, event, ...)
	if ( event == "EDITOR_TEMPLATE_RESULTS" ) then
		local kind, requestId = ...;
		requestId = tonumber(requestId);

		if ( kind == "item" ) then
			OnItemResults(requestId);
		elseif ( kind == "lootuse" ) then
			OnUseResults(requestId);
		elseif ( kind == "loot" ) then
			OnSourceResults(requestId);
		end

		return;
	end

	if ( event == "WORLD_TABLE_ROWS" ) then
		local tableName, key, ok, message = ...;

		if ( L.byTable[string.lower(tostring(tableName or ""))] ) then
			OnRows(string.lower(tableName), key, ok, message);
		end
	else
		local ok, tableName, key, message = ...;

		if ( L.byTable[string.lower(tostring(tableName or ""))] ) then
			OnResult(ok, string.lower(tableName), key, message);
		end
	end
end);
