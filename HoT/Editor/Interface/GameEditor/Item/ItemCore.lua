-- Item editor state: the open item template, item search, world table row sets and the generator name caches.

GameEditorItem = GameEditorItem or {};
local I = GameEditorItem;
local Data = GameEditorData or {};

local RECENT_MAX = 20;
local PROBE_MAX = 200;

-- The server sends every row of a listable table for this key, -2^63 is exact as a Lua number.
I.LIST_KEY = -9223372036854775808;

-- Whole tables are queried and saved with this key.
I.WHOLE_KEY = 0;

I.TEMPLATE = "item_template";
I.LOCALE = "item_template_locale";

I.QUALITY_COLORS = {
	[0] = "ff9d9d9d", [1] = "ffffffff", [2] = "ff1eff00", [3] = "ff0070dd", [4] = "ffa335ee", [5] = "ffff8000", [6] = "ffe6cc80", [7] = "ffe6cc80",
};

I.entry = nil;
I.names = {};
I.meta = {};
I.recent = {};
I.maxSeen = 0;

-- Small helpers -------------------------------------------------------------------------------------------

local function IsOk(value)
	return value == 1 or value == true;
end

function I.Copy(source)
	local copy = {};

	for key, value in pairs(source or {}) do
		copy[key] = value;
	end

	return copy;
end

function I.Colored(name, quality)
	local color = I.QUALITY_COLORS[tonumber(quality) or 1] or I.QUALITY_COLORS[1];
	return format("|c%s%s|r", color, name or "");
end

function I.EnumText(enumName, value)
	local enum = Data.Enums and Data.Enums[enumName];
	return enum and enum[tonumber(value) or value];
end

function I.SubClassEnum(class)
	return "ItemSubClass_"..tostring(tonumber(class) or 0);
end

-- Short "Epic Plate Chest" style text of an item.
function I.KindText(class, subclass, inventoryType)
	local parts = {};
	local sub = I.EnumText(I.SubClassEnum(class), subclass);
	local slot = I.EnumText("InventoryType", inventoryType);

	if ( sub ) then
		table.insert(parts, sub);
	else
		table.insert(parts, I.EnumText("ItemClass", class) or format("class %s", tostring(class)));
	end

	if ( slot and (tonumber(inventoryType) or 0) ~= 0 ) then
		table.insert(parts, slot);
	end

	return table.concat(parts, ", ");
end

-- Events ----------------------------------------------------------------------------------------------------

local listeners = {};

function I.On(event, func)
	listeners[event] = listeners[event] or {};
	table.insert(listeners[event], func);
end

function I.Fire(event, ...)
	for _, func in ipairs(listeners[event] or {}) do
		local ok, err = pcall(func, ...);

		if ( not ok ) then
			GameEditor_Print("Item editor: "..tostring(err), 1, 0.3, 0.3);
		end
	end
end

-- Names ------------------------------------------------------------------------------------------------------

function I.Remember(entry, name, meta)
	entry = tonumber(entry);

	if ( not entry or entry <= 0 ) then
		return;
	end

	if ( name and name ~= "" ) then
		I.names[entry] = name;
		GameEditor_TemplateListPut("item", entry, name, meta and { type = meta.quality, maxLevel = meta.itemLevel, minLevel = meta.requiredLevel, displayId = meta.displayId });
	end

	if ( meta ) then
		I.meta[entry] = meta;
	end

	if ( entry > I.maxSeen ) then
		I.maxSeen = entry;
	end
end

function I.NameOf(entry)
	entry = tonumber(entry) or 0;
	return I.names[entry] or GameEditor_TemplateName("item", entry) or (GetItemInfo(entry));
end

function I.RememberRecent(entry)
	for index = #I.recent, 1, -1 do
		if ( I.recent[index] == entry ) then
			table.remove(I.recent, index);
		end
	end

	table.insert(I.recent, 1, entry);

	while ( #I.recent > RECENT_MAX ) do
		table.remove(I.recent);
	end
end

local function MetaOfRow(row)
	return {
		quality = tonumber(row.Quality) or 1,
		class = tonumber(row.class) or 0,
		subclass = tonumber(row.subclass) or 0,
		inventoryType = tonumber(row.InventoryType) or 0,
		itemLevel = tonumber(row.ItemLevel) or 0,
		requiredLevel = tonumber(row.RequiredLevel) or 0,
		displayId = tonumber(row.displayid) or 0,
	};
end

-- Search hits carry "class subclass inventoryType" in the sub name slot.
local function MetaOfHit(row)
	local _, _, class, subclass, inventoryType = string.find(tostring(row.subName or ""), "^(%-?%d+) (%-?%d+) (%-?%d+)$");

	return {
		quality = tonumber(row.type) or 1,
		class = tonumber(class),
		subclass = tonumber(subclass),
		inventoryType = tonumber(inventoryType),
		itemLevel = tonumber(row.maxLevel) or 0,
		requiredLevel = tonumber(row.minLevel) or 0,
		displayId = tonumber(row.displayId) or 0,
	};
end

-- Item search ------------------------------------------------------------------------------------------------

function I.MetaOf(entry)
	entry = tonumber(entry) or 0;
	local row = not I.meta[entry] and GameEditor_TemplateRow("item", entry);
	return I.meta[entry] or (row and MetaOfHit(row)) or nil;
end

GameEditor_OnTemplateList("item", function()
	local items = GameEditor_TemplateList("item");
	local last = items[#items];

	if ( last and last.value > I.maxSeen ) then
		I.maxSeen = last.value;
	end

	I.Fire("NAMES");
end);

-- World table row sets ---------------------------------------------------------------------------------------

function I.Query(tableName, key)
	if ( not WorldTable_Query ) then
		GameEditor_Notify("Item editing needs WorldTable_Query in this client build", "error");
		return false;
	end

	local ok, sent = pcall(WorldTable_Query, tableName, key);
	return ok and sent and true or false;
end

-- The last rows the server sent for the table and key, or nil.
function I.GetSet(tableName, key)
	if ( not WorldTable_GetRows ) then
		return nil;
	end

	local ok, data = pcall(WorldTable_GetRows, tableName, key);
	return ok and type(data) == "table" and data or nil;
end

-- Rows hold exactly the columns given, missing values become 0 or an empty string.
function I.Save(tableName, key, columns, rows)
	if ( not WorldTable_Save ) then
		GameEditor_Notify("Saving needs WorldTable_Save in this client build", "error");
		return false;
	end

	local list = {};
	local clean = {};

	for _, column in ipairs(columns or {}) do
		table.insert(list, { name = column.name, type = column.type });
	end

	for _, row in ipairs(rows or {}) do
		local out = {};

		for _, column in ipairs(list) do
			local value = row[column.name];

			if ( value == nil ) then
				value = column.type == "string" and "" or 0;
			elseif ( column.type == "string" ) then
				value = tostring(value);
			else
				value = tonumber(value) or 0;
			end

			out[column.name] = value;
		end

		table.insert(clean, out);
	end

	local ok, sent = pcall(WorldTable_Save, tableName, key, list, clean);

	if ( not ok or not sent ) then
		GameEditor_Notify(format("Could not send %s: %s", tableName, tostring(ok and "bad rows" or sent)), "error");
		return false;
	end

	return true;
end

-- The open item template ----------------------------------------------------------------------------------------

local function ResetTemplate()
	I.template = { rows = {}, columns = {}, loaded = false, loading = false, saving = false };
end

ResetTemplate();

function I.IsLoaded()
	return I.template.loaded;
end

function I.RowExists()
	return I.template.loaded and #I.template.rows > 0;
end

function I.IsNew()
	return I.entry ~= nil and I.template.loaded and #I.template.rows == 0;
end

function I.IsSaving()
	return I.template.saving;
end

function I.IsLoading()
	return I.template.loading;
end

-- Values a brand new item starts with, the table defaults of item_template.
I.NEW_ITEM = {
	name = "New item", Quality = 1, class = 15, subclass = 0, SoundOverrideSubclass = -1, BuyCount = 1, stackable = 1,
	AllowableClass = -1, AllowableRace = -1, delay = 1000, RangedModRange = 0, RequiredDisenchantSkill = -1,
	spellcooldown_1 = -1, spellcategorycooldown_1 = -1, spellcooldown_2 = -1, spellcategorycooldown_2 = -1,
	spellcooldown_3 = -1, spellcategorycooldown_3 = -1, spellcooldown_4 = -1, spellcategorycooldown_4 = -1,
	spellcooldown_5 = -1, spellcategorycooldown_5 = -1, ScalingStatValue = 0, ItemLevel = 1,
};

function I.Defaults()
	local row = {};

	for _, column in ipairs(I.template.columns) do
		row[column.name] = column.type == "string" and "" or 0;
	end

	for column, value in pairs(I.NEW_ITEM) do
		row[column] = value;
	end

	row.entry = I.entry;
	return row;
end

function I.Row()
	if ( I.template.rows[1] ) then
		return I.Copy(I.template.rows[1]);
	end

	return I.Defaults();
end

function I.QueryTemplate()
	if ( not I.entry ) then
		return;
	end

	I.template.loading = true;
	I.template.error = nil;
	I.Query(I.TEMPLATE, I.entry);
	I.Fire("STATE");
end

-- Writes the whole template row, the server reloads just this entry and sends it to every player.
function I.SaveTemplate(values)
	if ( not I.entry or not I.template.loaded ) then
		GameEditor_Notify("The item is not loaded yet", "info");
		return false;
	end

	local row = I.Copy(values);
	row.entry = I.entry;

	if ( not I.Save(I.TEMPLATE, I.entry, I.template.columns, { row }) ) then
		return false;
	end

	I.template.saving = true;
	I.Fire("STATE");
	return true;
end

-- I.Open(entry, seed) loads the template, seed holds the values a new item starts with.
function I.Open(entry, seed)
	entry = tonumber(entry);

	if ( not entry or entry <= 0 ) then
		return;
	end

	I.entry = entry;
	I.seed = seed;
	ResetTemplate();
	I.RememberRecent(entry);
	I.Remember(entry);
	I.Fire("OPENED", entry);
	I.QueryTemplate();
end

function I.Reload()
	if ( not I.entry ) then
		GameEditor_Notify("Open an item first", "info");
		return;
	end

	I.QueryTemplate();
	I.Fire("RELOAD");
end

function I.Close()
	I.entry = nil;
	I.seed = nil;
	ResetTemplate();
	I.Fire("OPENED", nil);
end

function I.TakeSeed()
	local seed = I.seed;
	I.seed = nil;
	return seed;
end

-- Asks the client to fetch the item into its cache so the tooltip and icon work.
local cacheTooltip = CreateFrame("GameTooltip", "GameEditorItemCacheTooltip", nil, "GameTooltipTemplate");
local cacheAsked = {};

function I.CacheItem(entry, force)
	entry = tonumber(entry) or 0;

	if ( entry <= 0 or (cacheAsked[entry] and not force) ) then
		return;
	end

	cacheAsked[entry] = true;
	cacheTooltip:SetOwner(UIParent, "ANCHOR_NONE");
	pcall(cacheTooltip.SetHyperlink, cacheTooltip, "item:"..entry);
	cacheTooltip:Hide();
end

-- Free entry search ---------------------------------------------------------------------------------------------

local probe;

-- I.FindFreeEntry(start, callback) walks up from start until item_template has no row, callback(entry) or callback(nil).
function I.FindFreeEntry(start, callback)
	start = tonumber(start);

	if ( not start or start <= 0 or not WorldTable_Query ) then
		callback(nil);
		return;
	end

	while ( I.names[start] ) do
		start = start + 1;
	end

	probe = { entry = start, tries = 0, callback = callback };
	I.Query(I.TEMPLATE, start);
end

local function OnProbeRows(key)
	if ( not probe or probe.entry ~= key ) then
		return;
	end

	local data = I.GetSet(I.TEMPLATE, key);
	local exists = data and data.rows and #data.rows > 0;

	if ( not exists ) then
		local callback = probe.callback;
		probe = nil;
		callback(key);
		return;
	end

	I.Remember(key, data.rows[1].name, MetaOfRow(data.rows[1]));
	probe.tries = probe.tries + 1;

	if ( probe.tries >= PROBE_MAX ) then
		local callback = probe.callback;
		probe = nil;
		callback(nil);
		return;
	end

	probe.entry = key + 1;

	while ( I.names[probe.entry] ) do
		probe.entry = probe.entry + 1;
	end

	I.Query(I.TEMPLATE, probe.entry);
end

-- Generator name caches and refs ----------------------------------------------------------------------------------

I.statGroups = {};
I.uniques = {};

local listsAsked = false;

local LIST_TABLES = {
	item_generator_stat_group = function(rows)
		I.statGroups = {};

		for _, row in ipairs(rows) do
			I.statGroups[tonumber(row.id) or 0] = row.name or "";
		end
	end,
	item_generator_unique = function(rows)
		I.uniques = {};

		for _, row in ipairs(rows) do
			I.uniques[tonumber(row.id) or 0] = (row.name and row.name ~= "") and row.name or (row.comment or "");
		end
	end,
};

-- Loads the stat group and unique item lists once so the ref columns show names.
function I.EnsureLists(force)
	if ( listsAsked and not force ) then
		return;
	end

	listsAsked = true;

	for tableName in pairs(LIST_TABLES) do
		I.Query(tableName, I.LIST_KEY);
	end
end

local function CacheSearch(cache, text, extra)
	text = strtrim(text or "");
	local items = {};
	local number = tonumber(text);

	for value, label in pairs(extra or {}) do
		table.insert(items, { value = value, text = label });
	end

	if ( number ) then
		table.insert(items, { value = number, text = cache[number] or "" });
		return items;
	end

	local lower = string.lower(text);

	for id, name in pairs(cache) do
		if ( lower == "" or string.find(string.lower(name), lower, 1, true) ) then
			table.insert(items, { value = id, text = name });
		end
	end

	table.sort(items, function(a, b) return a.value < b.value; end);
	return items;
end

GameEditor_RefResolvers = GameEditor_RefResolvers or {};
GameEditor_RefSearchers = GameEditor_RefSearchers or {};

GameEditor_RefResolvers["itemgen_statgroup"] = function(value)
	value = tonumber(value) or 0;

	if ( value < 0 ) then
		return "any or random";
	end

	I.EnsureLists();
	return I.statGroups[value];
end;

GameEditor_RefSearchers["itemgen_statgroup"] = function(text)
	I.EnsureLists();
	return CacheSearch(I.statGroups, text, { [-1] = "any or random" });
end;

GameEditor_RefResolvers["itemgen_unique"] = function(value)
	I.EnsureLists();
	local name = I.uniques[tonumber(value) or 0];
	return (name and name ~= "") and name or (name and "exists") or nil;
end;

GameEditor_RefSearchers["itemgen_unique"] = function(text)
	I.EnsureLists();
	return CacheSearch(I.uniques, text);
end;

-- Server replies ------------------------------------------------------------------------------------------------

local function OnRows(tableName, key, ok, message)
	key = tonumber(key);
	local lower = string.lower(tostring(tableName or ""));

	if ( lower == I.TEMPLATE ) then
		OnProbeRows(key);

		if ( I.entry and key == I.entry ) then
			local state = I.template;
			state.loading = false;

			if ( ok ~= nil and not IsOk(ok) ) then
				state.error = (message and message ~= "") and message or "the server could not read the rows";
				GameEditor_Notify(I.TEMPLATE..": "..state.error, "error");
			else
				local data = I.GetSet(I.TEMPLATE, key);

				if ( data ) then
					state.rows = data.rows or {};
					state.columns = data.columns or {};
					state.loaded = true;
					state.error = nil;

					if ( state.rows[1] ) then
						I.Remember(key, state.rows[1].name, MetaOfRow(state.rows[1]));
					end

					I.CacheItem(key, true);
					I.Fire("TEMPLATE");
				else
					state.error = "the server sent no rows";
				end
			end

			I.Fire("STATE");
		end
	end

	if ( LIST_TABLES[lower] and key == I.LIST_KEY and IsOk(ok) ) then
		local data = I.GetSet(lower, key);

		if ( data ) then
			LIST_TABLES[lower](data.rows or {});
			I.Fire("NAMES");
		end
	end

	I.Fire("ROWS", lower, key, IsOk(ok), message);
end

local function OnResult(ok, tableName, key, message)
	key = tonumber(key);
	local lower = string.lower(tostring(tableName or ""));

	if ( lower == I.TEMPLATE and I.entry and key == I.entry ) then
		I.template.saving = false;

		if ( IsOk(ok) ) then
			I.QueryTemplate();
		end

		I.Fire("STATE");
	end

	-- A saved stat group or unique item changes the names the refs show.
	if ( IsOk(ok) and LIST_TABLES[lower] and listsAsked ) then
		I.Query(lower, I.LIST_KEY);
	end

	I.Fire("RESULT", lower, key, IsOk(ok), message);
end

local events = CreateFrame("Frame", "GameEditorItemEvents");
events:RegisterEvent("WORLD_TABLE_ROWS");
events:RegisterEvent("WORLD_TABLE_RESULT");
events:SetScript("OnEvent", function(_, event, ...)
	if ( event == "WORLD_TABLE_ROWS" ) then
		OnRows(...);
	elseif ( event == "WORLD_TABLE_RESULT" ) then
		OnResult(...);
	end
end);

local elapsedSinceCheck = 0;

events:SetScript("OnUpdate", function(_, elapsed)
	elapsedSinceCheck = elapsedSinceCheck + elapsed;

	if ( elapsedSinceCheck < 0.5 ) then
		return;
	end

	elapsedSinceCheck = 0;

	-- The client cache answers later, the header waits for the open item.
	if ( I.entry and I.waitingInfo ~= I.entry and not GetItemInfo(I.entry) ) then
		I.waitingInfo = I.entry;
	elseif ( I.waitingInfo and GetItemInfo(I.waitingInfo) ) then
		I.waitingInfo = nil;
		I.Fire("ITEM_INFO");
	end
end);
