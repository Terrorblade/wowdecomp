-- Spell editor core: data access, the open spell documents, events and ref resolvers.

GameEditorSpell = GameEditorSpell or {};

local SE = GameEditorSpell;
local Data = GameEditorData or {};

SE.TABLE = "Spell";
SE.LOCALE_MASK = 16712190;
SE.MAX_RECENT = 40;

SE.SCHOOL_COLORS = {
	[0] = { 1, 1, 0.55 },
	[1] = { 1, 0.9, 0.5 },
	[2] = { 1, 0.5, 0 },
	[3] = { 0.3, 1, 0.3 },
	[4] = { 0.5, 1, 1 },
	[5] = { 0.6, 0.6, 1 },
	[6] = { 1, 0.5, 1 },
};

SE.SCHOOL_NAMES = { [0] = "Physical", "Holy", "Fire", "Nature", "Frost", "Shadow", "Arcane" };

SE.VISUAL_KIT_COLUMNS = {
	"m_precastKit", "m_castKit", "m_impactKit", "m_stateKit", "m_stateDoneKit", "m_channelKit", "m_casterImpactKit", "m_targetImpactKit",
	"m_missileTargetingKit", "m_instantAreaKit", "m_impactAreaKit", "m_persistentAreaKit",
};

SE.recent = SE.recent or {};
SE.stamp = 0;
SE.grids = {};
SE.editors = {};
SE.listeners = {};
SE.pendingSave = {};
SE.reservedIds = {};

-- Small helpers ------------------------------------------------------------------------------------------

function SE.Copy(source)
	local copy = {};

	for key, value in pairs(source or {}) do
		copy[key] = value;
	end

	return copy;
end

function SE.SameValue(a, b)
	if ( a == nil and b == nil ) then
		return true;
	end

	if ( type(a) == "number" or type(b) == "number" ) then
		return tonumber(a) == tonumber(b);
	end

	return tostring(a or "") == tostring(b or "");
end

function SE.IsOk(value)
	return value == 1 or value == true;
end

function SE.Number(value)
	return tonumber(value) or 0;
end

function SE.Lower(text)
	return string.lower(tostring(text or ""));
end

function SE.Trim(value)
	local text = format("%.2f", tonumber(value) or 0);
	text = string.gsub(text, "(%..-)0+$", "%1");
	text = string.gsub(text, "%.$", "");
	return text;
end

function SE.Color(color)
	return format("|cff%02x%02x%02x", floor(color[1] * 255), floor(color[2] * 255), floor(color[3] * 255));
end

-- "123 sec", "1.5 min", "2 hrs" from milliseconds.
function SE.FormatTime(ms)
	ms = tonumber(ms) or 0;
	local seconds = ms / 1000;

	if ( seconds < 60 ) then
		return SE.Trim(seconds).." sec";
	elseif ( seconds < 3600 ) then
		return SE.Trim(seconds / 60).." min";
	elseif ( seconds < 86400 ) then
		return SE.Trim(seconds / 3600).." hrs";
	end

	return SE.Trim(seconds / 86400).." days";
end

-- Unit GUIDs come as 0x hex strings, the editor C++ parses decimal.
function SE.GuidToDecimal(guid)
	if ( not guid or guid == "" ) then
		return nil;
	end

	if ( string.find(guid, "^%d+$") ) then
		return guid;
	end

	local hex = string.gsub(guid, "^0[xX]", "");
	local digits = { 0 };

	for index = 1, #hex do
		local carry = tonumber(string.sub(hex, index, index), 16);

		if ( not carry ) then
			return nil;
		end

		for position = 1, #digits do
			local value = digits[position] * 16 + carry;
			digits[position] = mod(value, 10);
			carry = floor(value / 10);
		end

		while ( carry > 0 ) do
			table.insert(digits, mod(carry, 10));
			carry = floor(carry / 10);
		end
	end

	local parts = {};

	for position = #digits, 1, -1 do
		table.insert(parts, digits[position]);
	end

	local text = string.gsub(table.concat(parts), "^0+", "");
	return text ~= "" and text or "0";
end

-- DBC access --------------------------------------------------------------------------------------------

local schemas = {};

local function NormalizeType(kind)
	if ( kind == 0 or kind == "uint" or kind == "uint32" ) then
		return "uint";
	elseif ( kind == 1 or kind == 4 or kind == 5 or kind == "int" or kind == "int32" or kind == "bool" ) then
		return "int";
	elseif ( kind == 2 or kind == "float" ) then
		return "float";
	elseif ( kind == 3 or kind == "string" ) then
		return "string";
	end

	return "int";
end

SE.NormalizeType = NormalizeType;

-- Column order and types of a DBC, from the C++ binding when it is there, otherwise from the generated data.
function SE.Schema(tableName)
	local schema = schemas[tableName];

	if ( schema ) then
		return schema;
	end

	schema = { columns = {}, types = {}, index = {} };

	local source;

	if ( DbcEditor_GetSchema ) then
		local ok, result = pcall(DbcEditor_GetSchema, tableName);

		if ( ok and type(result) == "table" and #result > 0 ) then
			source = result;
		end
	end

	if ( source ) then
		for index, column in ipairs(source) do
			local name = column.name or column[1];
			table.insert(schema.columns, name);
			schema.types[name] = NormalizeType(column.type or column[2]);
			schema.index[name] = index;
		end
	else
		local fields = Data.Fields and Data.Fields[tableName] or {};

		for index, name in ipairs(Data.Columns and Data.Columns[tableName] or {}) do
			table.insert(schema.columns, name);
			schema.types[name] = NormalizeType(fields[name] and fields[name].type or "int");
			schema.index[name] = index;
		end
	end

	if ( #schema.columns > 0 ) then
		schemas[tableName] = schema;
	end

	return schema;
end

-- Row as column -> value, accepts both the map and the 1-based array shape from the C++ side.
function SE.GetRow(tableName, id)
	id = tonumber(id);

	if ( not DbcEditor_GetRow or not id ) then
		return nil;
	end

	local ok, row = pcall(DbcEditor_GetRow, tableName, id);

	if ( not ok or type(row) ~= "table" ) then
		return nil;
	end

	if ( row[1] ~= nil and row.m_id == nil and row.id == nil ) then
		local map = {};
		local schema = SE.Schema(tableName);

		for index, value in ipairs(row) do
			local name = schema.columns[index];

			if ( name ) then
				map[name] = value;
			end
		end

		return map;
	end

	return row;
end

function SE.RowExists(tableName, id)
	return SE.GetRow(tableName, id) ~= nil;
end

function SE.Find(tableName, text, limit)
	if ( not DbcEditor_Find ) then
		return {};
	end

	local ok, result = pcall(DbcEditor_Find, tableName, text or "", limit or 200);

	if ( not ok or type(result) ~= "table" ) then
		return {};
	end

	local items = {};

	for _, row in ipairs(result) do
		local id = tonumber(row.id or row[1]);

		if ( id ) then
			table.insert(items, { id = id, name = row.name or row.text or row[2] or "" });
		end
	end

	return items;
end

-- Rows whose name holds text and that pass every where entry { columns = { ... }, value = v }, one column matching is enough.
function SE.Filter(tableName, text, where, limit)
	if ( not DbcEditor_Filter ) then
		return {};
	end

	local ok, result = pcall(DbcEditor_Filter, tableName, { text = text or "", where = where or {} }, limit);

	if ( not ok or type(result) ~= "table" ) then
		return {};
	end

	return result;
end

function SE.MaxId(tableName)
	local maxId = 0;

	if ( DbcEditor_GetMaxId ) then
		local ok, value = pcall(DbcEditor_GetMaxId, tableName);

		if ( ok ) then
			maxId = tonumber(value) or 0;
		end
	end

	return max(maxId, SE.reservedIds[tableName] or 0);
end

function SE.NextId(tableName)
	local id = SE.MaxId(tableName) + 1;
	SE.reservedIds[tableName] = id;
	return id;
end

-- A full row in schema order, missing columns become 0 or "", integers are rounded.
function SE.FullRow(tableName, values)
	local schema = SE.Schema(tableName);
	local row = {};

	for _, name in ipairs(schema.columns) do
		local kind = schema.types[name];
		local value = values[name];

		if ( kind == "string" ) then
			row[name] = value ~= nil and tostring(value) or "";
		elseif ( kind == "float" ) then
			row[name] = tonumber(value) or 0;
		else
			row[name] = floor((tonumber(value) or 0) + 0.5);
		end
	end

	return row;
end

function SE.EmptyRow(tableName, id)
	local row = SE.FullRow(tableName, {});
	row.m_id = id;
	return row;
end

function SE.DiffCount(values, original, columns)
	local count = 0;

	for _, column in ipairs(columns) do
		if ( not SE.SameValue(values[column], original[column]) ) then
			count = count + 1;
		end
	end

	return count;
end

-- Readable lookups for the small spell DBCs ------------------------------------------------------------

function SE.CastTimeInfo(id)
	local row = SE.GetRow("SpellCastTimes", id);

	if ( not row ) then
		return nil, nil;
	end

	local ms = SE.Number(row.m_base);
	local text = ms <= 0 and "Instant" or (SE.Trim(ms / 1000).." sec cast");
	local perLevel = SE.Number(row.m_perLevel);

	if ( perLevel ~= 0 ) then
		text = text..format(", %s ms per level, min %s", SE.Trim(perLevel), SE.Trim(SE.Number(row.m_minimum)));
	end

	return ms, text;
end

function SE.DurationInfo(id)
	local row = SE.GetRow("SpellDuration", id);

	if ( not row ) then
		return nil, nil;
	end

	local ms = SE.Number(row.m_duration);

	if ( ms < 0 ) then
		return -1, "Until cancelled";
	end

	local text = SE.FormatTime(ms);
	local maxMs = SE.Number(row.m_maxDuration);

	if ( SE.Number(row.m_durationPerLevel) ~= 0 or (maxMs > 0 and maxMs ~= ms) ) then
		text = text..format(" (+%s ms per level, max %s)", SE.Trim(SE.Number(row.m_durationPerLevel)), SE.FormatTime(maxMs));
	end

	return ms, text;
end

function SE.RangeInfo(id)
	local row = SE.GetRow("SpellRange", id);

	if ( not row ) then
		return nil;
	end

	local minHostile = SE.Number(row["m_rangeMin[0]"]);
	local maxHostile = SE.Number(row["m_rangeMax[0]"]);
	local minFriend = SE.Number(row["m_rangeMin[1]"]);
	local maxFriend = SE.Number(row["m_rangeMax[1]"]);
	local name = row["m_displayName[0]"] or row.Name or "";
	local yards;

	if ( minHostile > 0 ) then
		yards = format("%s-%s yd", SE.Trim(minHostile), SE.Trim(maxHostile));
	else
		yards = format("%s yd", SE.Trim(maxHostile));
	end

	if ( maxFriend ~= maxHostile or minFriend ~= minHostile ) then
		yards = yards..format(", friendly %s yd", SE.Trim(maxFriend));
	end

	local text = name ~= "" and (name.." - "..yards) or yards;
	return { min = minHostile, max = maxHostile, name = name, text = text, yards = yards };
end

function SE.RadiusInfo(id)
	local row = SE.GetRow("SpellRadius", id);

	if ( not row ) then
		return nil, nil;
	end

	local radius = SE.Number(row.m_radius);
	local text = SE.Trim(radius).." yd";

	if ( SE.Number(row.m_radiusPerLevel) ~= 0 ) then
		text = text..format(" (+%s per level, max %s)", SE.Trim(SE.Number(row.m_radiusPerLevel)), SE.Trim(SE.Number(row.m_radiusMax)));
	end

	return radius, text;
end

function SE.IconPath(iconId)
	local row = SE.GetRow("SpellIcon", iconId);
	local path = row and (row.m_textureFilename or row.name);

	if ( path and path ~= "" ) then
		return path;
	end

	return "Interface\\Icons\\INV_Misc_QuestionMark";
end

function SE.IconShortName(path)
	local _, _, short = string.find(path or "", "([^\\/]+)$");
	return short or path or "";
end

-- Icon of a spell row, or of a spell id when only the id is known.
function SE.SpellIcon(spellId, values)
	if ( values ) then
		return SE.IconPath(values.m_spellIconID);
	end

	local _, _, icon = GetSpellInfo(spellId);

	if ( icon ) then
		return icon;
	end

	local row = SE.GetRow(SE.TABLE, spellId);
	return SE.IconPath(row and row.m_spellIconID or 0);
end

function SE.SpellName(spellId)
	local name, rank = GetSpellInfo(spellId);

	if ( not name ) then
		local row = SE.GetRow(SE.TABLE, spellId);

		if ( row ) then
			name, rank = row["m_name[0]"], row["m_nameSubtext[0]"];
		end
	end

	return name, rank;
end

function SE.SchoolText(mask)
	mask = SE.Number(mask);
	local names = {};

	for bit = 0, 6 do
		if ( GameEditor_HasBit(mask, bit) ) then
			table.insert(names, SE.Color(SE.SCHOOL_COLORS[bit])..SE.SCHOOL_NAMES[bit].."|r");
		end
	end

	return #names > 0 and table.concat(names, ", ") or "No school";
end

function SE.EnumText(enumName, value)
	local enum = Data.Enums and Data.Enums[enumName];
	local name = enum and enum[tonumber(value) or value];
	return name and tostring(name) or tostring(value or 0);
end

-- Ref resolvers and searchers for the grids ----------------------------------------------------------

local listCache = {};

local function ListAll(tableName, describe)
	if ( listCache[tableName] ) then
		return listCache[tableName];
	end

	local items = {};

	for _, item in ipairs(GameEditor_DbcList(tableName)) do
		local row = SE.GetRow(tableName, item.value);

		if ( row ) then
			table.insert(items, { value = item.value, text = describe(row, item.value) or "" });
		end
	end

	listCache[tableName] = items;
	return items;
end

local function RegisterList(ref, tableName, describe)
	GameEditor_RefLists[ref] = function()
		return ListAll(tableName, describe);
	end;
end

function SE.InvalidateLists(tableName)
	if ( tableName ) then
		listCache[tableName] = nil;
	else
		listCache = {};
	end
end

local MISSING = "|cffff6060no such row|r";

function SE.RegisterResolvers()
	local resolvers = GameEditor_RefResolvers;

	resolvers["dbc:SpellCastTimes"] = function(value)
		local _, text = SE.CastTimeInfo(value);
		return text or MISSING;
	end;

	resolvers["dbc:SpellDuration"] = function(value)
		local _, text = SE.DurationInfo(value);
		return text or MISSING;
	end;

	resolvers["dbc:SpellRange"] = function(value)
		local info = SE.RangeInfo(value);
		return info and info.text or MISSING;
	end;

	resolvers["dbc:SpellRadius"] = function(value)
		local _, text = SE.RadiusInfo(value);
		return text or MISSING;
	end;

	resolvers["dbc:SpellIcon"] = function(value)
		local row = SE.GetRow("SpellIcon", value);
		return row and SE.IconShortName(row.m_textureFilename) or MISSING;
	end;

	resolvers["dbc:SpellVisual"] = function(value)
		local row = SE.GetRow("SpellVisual", value);

		if ( not row ) then
			return MISSING;
		end

		local kits = 0;

		for _, column in ipairs(SE.VISUAL_KIT_COLUMNS) do
			if ( SE.Number(row[column]) ~= 0 ) then
				kits = kits + 1;
			end
		end

		return format("%d kit%s%s", kits, kits == 1 and "" or "s", SE.Number(row.m_missileModel) ~= 0 and ", missile" or "");
	end;

	resolvers["dbc:SpellMissile"] = function(value)
		local row = SE.GetRow("SpellMissile", value);

		if ( not row ) then
			return MISSING;
		end

		return format("gravity %s, speed %s-%s", SE.Trim(SE.Number(row.m_gravity)), SE.Trim(SE.Number(row.m_defaultSpeedMin)), SE.Trim(SE.Number(row.m_defaultSpeedMax)));
	end;

	resolvers["dbc:SpellDescriptionVariables"] = function(value)
		local row = SE.GetRow("SpellDescriptionVariables", value);

		if ( not row ) then
			return MISSING;
		end

		local formula = string.gsub(tostring(row.m_variables or ""), "[\r\n]+", "  ");
		return string.sub(formula, 1, 80);
	end;

	RegisterList("dbc:SpellCastTimes", "SpellCastTimes", function(_, id)
		local _, text = SE.CastTimeInfo(id);
		return text;
	end);

	RegisterList("dbc:SpellDuration", "SpellDuration", function(_, id)
		local _, text = SE.DurationInfo(id);
		return text;
	end);

	RegisterList("dbc:SpellRange", "SpellRange", function(_, id)
		local info = SE.RangeInfo(id);
		return info and info.text;
	end);

	RegisterList("dbc:SpellRadius", "SpellRadius", function(_, id)
		local _, text = SE.RadiusInfo(id);
		return text;
	end);

	RegisterList("dbc:SpellIcon", "SpellIcon", function(row)
		return SE.IconShortName(row.m_textureFilename);
	end);
end

-- Events --------------------------------------------------------------------------------------------------

function SE.On(event, handler)
	SE.listeners[event] = SE.listeners[event] or {};
	table.insert(SE.listeners[event], handler);
end

function SE.Fire(event, ...)
	for _, handler in ipairs(SE.listeners[event] or {}) do
		local ok, err = pcall(handler, ...);

		if ( not ok ) then
			GameEditor_Print("Spell editor: "..event.." handler failed: "..tostring(err), 1, 0.3, 0.3);
		end
	end
end

local GAME_EVENTS = {
	"DBC_EDITOR_RESULT", "DBC_RECORD_UPDATED", "WORLD_TABLE_ROWS", "WORLD_TABLE_RESULT", "WORLD_SCRIPT_NAMES",
	"GAME_EDITOR_SELECTION_CHANGED", "PLAYER_TARGET_CHANGED",
};

function SE.InitEvents()
	if ( SE.eventFrame ) then
		return;
	end

	local frame = CreateFrame("Frame", "GameEditorSpellEvents", EditorParent);

	for _, event in ipairs(GAME_EVENTS) do
		frame:RegisterEvent(event);
	end

	frame:SetScript("OnEvent", function(_, event, ...)
		if ( event == "DBC_RECORD_UPDATED" ) then
			local tableName = ...;
			SE.InvalidateLists(tableName);
		end

		SE.Fire(event, ...);
	end);

	SE.eventFrame = frame;
end

-- Open spell documents -------------------------------------------------------------------------------

-- Every open spell is a document { id, values, original, exists, isNew, appliedLocal, remoteChanged, world }, SE.cur is the active one.
SE.docs = SE.docs or {};
SE.MAX_DOCS = 30;

local useCounter = 0;

function SE.Current()
	return SE.cur;
end

function SE.CurrentId()
	return SE.cur and SE.cur.id;
end

function SE.GetValue(column)
	return SE.cur and SE.cur.values[column];
end

function SE.Docs()
	return SE.docs;
end

function SE.FindDoc(id)
	id = tonumber(id);

	for index, doc in ipairs(SE.docs) do
		if ( doc.id == id ) then
			return doc, index;
		end
	end
end

function SE.DocIndex(doc)
	for index, existing in ipairs(SE.docs) do
		if ( existing == doc ) then
			return index;
		end
	end
end

function SE.RegisterGrid(grid)
	grid.seStamp = -1;
	table.insert(SE.grids, grid);
	SE.SyncGrid(grid);
	return grid;
end

-- Spell grids share the values and original tables of the open spell, they only need a refresh.
function SE.SyncGrid(grid, force)
	local cur = SE.cur;

	if ( not cur ) then
		return;
	end

	if ( grid.values ~= cur.values or grid.original ~= cur.original ) then
		grid.values = cur.values;
		grid.original = cur.original;
		force = true;
	end

	if ( force or grid.seStamp ~= SE.stamp ) then
		grid.seStamp = SE.stamp;
		grid:Refresh();
	end
end

function SE.RefreshVisibleGrids(source)
	for _, grid in ipairs(SE.grids) do
		if ( grid ~= source and grid.frame:IsVisible() ) then
			SE.SyncGrid(grid);
		end
	end
end

-- Called after any value of the open spell changed, source is the grid or widget that changed it.
function SE.Changed(column, source)
	SE.stamp = SE.stamp + 1;

	if ( source and source.seStamp ) then
		source.seStamp = SE.stamp;
	end

	SE.RefreshVisibleGrids(source);
	SE.Fire("VALUE_CHANGED", column, source);
	SE.QueueRefresh();
end

function SE.SetValue(column, value, source)
	if ( not SE.cur ) then
		return;
	end

	if ( SE.SameValue(SE.cur.values[column], value) and type(SE.cur.values[column]) == type(value) ) then
		return;
	end

	SE.cur.values[column] = value;
	SE.Changed(column, source);
end

function SE.QueueRefresh()
	SE.refreshQueued = true;
end

-- Runs once per frame from the main panel, keeps the header, sidebar and previews cheap while typing.
function SE.FlushRefresh()
	if ( SE.refreshQueued ) then
		SE.refreshQueued = false;
		SE.Fire("REFRESH");
	end
end

function SE.DocChangeCount(doc, columns)
	if ( not doc ) then
		return 0;
	end

	return SE.DiffCount(doc.values, doc.original, columns or SE.Schema(SE.TABLE).columns);
end

function SE.ChangeCount(columns)
	return SE.DocChangeCount(SE.cur, columns);
end

function SE.DocIsDirty(doc)
	return doc ~= nil and (doc.isNew or SE.DocChangeCount(doc) > 0);
end

function SE.IsDirty()
	return SE.DocIsDirty(SE.cur);
end

-- Spell row changes or unsaved server rows of a document.
function SE.DocHasUnsaved(doc)
	return SE.DocIsDirty(doc) or (SE.DocWorldDirty ~= nil and SE.DocWorldDirty(doc));
end

function SE.ChangedColumns()
	local list = {};
	local cur = SE.cur;

	if ( cur ) then
		for _, column in ipairs(SE.Schema(SE.TABLE).columns) do
			if ( not SE.SameValue(cur.values[column], cur.original[column]) ) then
				table.insert(list, column);
			end
		end
	end

	return list;
end

function SE.AddRecent(id)
	for index, existing in ipairs(SE.recent) do
		if ( existing == id ) then
			table.remove(SE.recent, index);
			break;
		end
	end

	table.insert(SE.recent, 1, id);

	while ( #SE.recent > SE.MAX_RECENT ) do
		table.remove(SE.recent);
	end

	SE.Fire("RECENT_CHANGED");
end

function SE.RemoveRecent(id)
	for index, existing in ipairs(SE.recent) do
		if ( existing == id ) then
			table.remove(SE.recent, index);
			break;
		end
	end

	SE.Fire("RECENT_CHANGED");
end

-- Commits a focused line edit into the spell it belongs to before the active spell changes.
local function ClearEditFocus()
	local focus = GetCurrentKeyBoardFocus and GetCurrentKeyBoardFocus();

	if ( focus and focus.ClearFocus ) then
		focus:ClearFocus();
	end
end

local function SetCurrent(doc)
	if ( doc ) then
		useCounter = useCounter + 1;
		doc.usedAt = useCounter;
	end

	SE.cur = doc;
	SE.stamp = SE.stamp + 1;
	SE.RefreshVisibleGrids();
	SE.Fire("SPELL_LOADED", doc);
	SE.QueueRefresh();
end

-- Swaps the values of a document, the active one reloads every view.
local function ReplaceValues(doc, values, original)
	doc.values = values;
	doc.original = original;

	if ( doc == SE.cur ) then
		SetCurrent(doc);
	else
		SE.stamp = SE.stamp + 1;
		SE.QueueRefresh();
	end
end

function SE.Activate(doc)
	if ( doc == SE.cur or (doc and not SE.DocIndex(doc)) ) then
		return;
	end

	ClearEditFocus();
	SetCurrent(doc);
end

-- Next or previous tab, wraps around.
function SE.CycleDoc(step)
	local count = #SE.docs;

	if ( count < 2 ) then
		return;
	end

	local index = SE.DocIndex(SE.cur) or 1;
	index = mod(index - 1 + (step or 1) + count, count) + 1;
	SE.Activate(SE.docs[index]);
end

-- Undoes ApplyLocal on the client, the C++ keeps the row from before the first apply.
function SE.RevertLocalRow(tableName, id, original)
	if ( DbcEditor_RevertLocal ) then
		local ok, reverted = pcall(DbcEditor_RevertLocal, tableName, id);

		if ( ok ) then
			return reverted;
		end
	end

	if ( original and DbcEditor_ApplyLocal ) then
		pcall(DbcEditor_ApplyLocal, tableName, id, SE.FullRow(tableName, original));
	end
end

function SE.DiscardSpellLocal(doc)
	doc = doc or SE.cur;

	if ( doc and doc.appliedLocal ) then
		SE.RevertLocalRow(SE.TABLE, doc.id, doc.exists and doc.original);
	end

	if ( doc ) then
		doc.appliedLocal = false;
	end
end

local function RemoveDoc(doc)
	local index = SE.DocIndex(doc);

	if ( not index ) then
		return;
	end

	if ( doc == SE.cur ) then
		ClearEditFocus();
	end

	SE.DiscardSpellLocal(doc);
	doc.closeAfterSave = nil;
	table.remove(SE.docs, index);

	if ( doc == SE.cur ) then
		SetCurrent(SE.docs[index] or SE.docs[index - 1]);
	else
		SE.QueueRefresh();
	end
end

-- Keeps the tab count bounded by closing the longest unused saved tabs.
local function TrimDocs(keep)
	while ( #SE.docs > SE.MAX_DOCS ) do
		local oldest;

		for _, doc in ipairs(SE.docs) do
			if ( doc ~= keep and doc ~= SE.cur and not SE.DocHasUnsaved(doc) and not SE.pendingSave[doc.id]
				and (not oldest or (doc.usedAt or 0) < (oldest.usedAt or 0)) ) then
				oldest = doc;
			end
		end

		if ( not oldest ) then
			return;
		end

		RemoveDoc(oldest);
	end
end

-- New documents open right of the active tab.
local function AddDoc(doc)
	doc.world = doc.world or {};
	local index = SE.DocIndex(SE.cur);
	table.insert(SE.docs, index and (index + 1) or (#SE.docs + 1), doc);
	ClearEditFocus();
	SetCurrent(doc);
	TrimDocs(doc);
end

-- Components with their own unsaved state (visual editors, world tables) register here so saving and closing can ask once.
function SE.RegisterEditor(editor)
	table.insert(SE.editors, editor);
end

function SE.DirtyEditors()
	local list = {};

	for _, editor in ipairs(SE.editors) do
		if ( editor:IsDirty() ) then
			table.insert(list, editor);
		end
	end

	return list;
end

-- True when anything in the spell editor has unsaved changes, in any tab.
function SE.AnyUnsaved()
	for _, doc in ipairs(SE.docs) do
		if ( SE.DocHasUnsaved(doc) ) then
			return true;
		end
	end

	return #SE.DirtyEditors() > 0;
end

function SE.DiscardLocal()
	SE.DiscardSpellLocal();

	for _, editor in ipairs(SE.editors) do
		if ( editor.DiscardLocal ) then
			editor:DiscardLocal();
		end
	end
end

-- Asks before throwing away unsaved work of the active spell, then runs proceed.
function SE.ConfirmDiscard(proceed, what)
	local spellDirty = SE.IsDirty();
	local others = SE.DirtyEditors();

	if ( not spellDirty and #others == 0 ) then
		proceed();
		return;
	end

	local parts = {};

	if ( spellDirty ) then
		if ( SE.cur.isNew ) then
			table.insert(parts, format("spell %d is new and not saved", SE.cur.id));
		else
			table.insert(parts, format("spell %d has %d unsaved field%s", SE.cur.id, SE.ChangeCount(), SE.ChangeCount() == 1 and "" or "s"));
		end
	end

	for _, editor in ipairs(others) do
		table.insert(parts, editor:DirtyText());
	end

	NoggitMessageBox_Show("Unsaved changes", format("Before %s: %s.", what or "switching", table.concat(parts, ", ")), {
		{ text = "Save all", onClick = function()
			SE.SaveAll();
			proceed();
		end },
		{ text = "Discard", onClick = function()
			SE.DiscardLocal();

			for _, editor in ipairs(others) do
				editor:Discard();
			end

			proceed();
		end },
		{ text = "Cancel" },
	});
end

local function OpenNew(id)
	local row = SE.GetRow(SE.TABLE, id);

	if ( not row ) then
		GameEditor_Notify(format("Spell %d does not exist. Spell > New spell creates one.", id), "error");
		return false;
	end

	AddDoc({ id = id, values = row, original = SE.Copy(row), exists = true });
	SE.AddRecent(id);
	return true;
end

-- Opens a spell in a new tab, or focuses the tab that already has it. Force reloads that tab from the database.
function SE.Open(id, force)
	id = tonumber(id);

	if ( not id or id <= 0 ) then
		return;
	end

	local doc = SE.FindDoc(id);

	if ( not doc ) then
		OpenNew(id);
		return;
	end

	if ( force and not doc.isNew ) then
		local row = SE.GetRow(SE.TABLE, id);

		if ( row ) then
			SE.DiscardSpellLocal(doc);
			doc.exists = true;
			doc.remoteChanged = false;

			if ( doc == SE.cur ) then
				ReplaceValues(doc, row, SE.Copy(row));
			else
				doc.values = row;
				doc.original = SE.Copy(row);
			end
		end
	end

	SE.Activate(doc);
	SE.AddRecent(id);
end

function SE.DefaultSpell(id)
	local row = SE.EmptyRow(SE.TABLE, id);
	row["m_name[0]"] = "New spell";
	row.m_castingTimeIndex = 1;
	row.m_rangeIndex = 1;
	row.m_spellIconID = 1;
	row.m_schoolMask = 1;
	row.m_stanceBarOrder = 0;
	row.m_nameFlags = SE.LOCALE_MASK;
	row.m_nameSubtextFlags = SE.LOCALE_MASK;
	row.m_descriptionFlags = SE.LOCALE_MASK;
	row.m_auraDescriptionFlags = SE.LOCALE_MASK;

	for index = 1, 3 do
		row["m_effectChainAmplitude["..(index - 1).."]"] = 1;
		row["m_effectBonusCoefficient["..(index - 1).."]"] = 1;
	end

	return row;
end

-- New spell in its own tab, from defaults or a copy of values.
function SE.NewSpell(fromValues)
	local id = SE.NextId(SE.TABLE);

	while ( SE.FindDoc(id) ) do
		id = id + 1;
		SE.reservedIds[SE.TABLE] = id;
	end

	local values;

	if ( fromValues ) then
		values = SE.FullRow(SE.TABLE, fromValues);
		values.m_id = id;
	else
		values = SE.DefaultSpell(id);
	end

	AddDoc({ id = id, values = values, original = SE.Copy(values), isNew = true, copiedFrom = fromValues and fromValues.ID });
	SE.AddRecent(id);
	GameEditor_Notify(format("Spell %d created locally, Save writes it to the dbc database.", id), "info");
end

function SE.CopySpell()
	if ( SE.cur ) then
		local values = SE.Copy(SE.cur.values);
		values.m_id = SE.cur.id;
		SE.NewSpell(values);
	end
end

function SE.SaveDoc(doc)
	if ( not doc ) then
		return false;
	end

	if ( not DbcEditor_Save ) then
		GameEditor_Notify("DbcEditor_Save is not available in this client build.", "error");
		return false;
	end

	local row = SE.FullRow(SE.TABLE, doc.values);
	row.m_id = doc.id;
	doc.saving = SE.Copy(row);
	SE.pendingSave[doc.id] = true;

	local ok, err = pcall(DbcEditor_Save, SE.TABLE, doc.id, row);

	if ( not ok ) then
		SE.pendingSave[doc.id] = nil;
		doc.saving = nil;
		GameEditor_Notify("Saving failed: "..tostring(err), "error");
	end

	SE.QueueRefresh();
	return ok;
end

function SE.Save()
	SE.SaveDoc(SE.cur);
end

-- Saves every tab, the active tab's server rows and visual rows, and the server rows kept by the other tabs.
function SE.SaveAll()
	for _, doc in ipairs(SE.docs) do
		if ( SE.DocIsDirty(doc) and not SE.pendingSave[doc.id] ) then
			SE.SaveDoc(doc);
		end

		if ( doc ~= SE.cur and SE.SaveDocWorld ) then
			SE.SaveDocWorld(doc);
		end
	end

	for _, editor in ipairs(SE.DirtyEditors()) do
		editor:Save();
	end
end

-- What would be lost when the documents close, one line per document.
local function UnsavedText(doc)
	local parts = {};
	local name = tostring(doc.values["m_name[0]"] or "");
	local title = name ~= "" and format("%d %s", doc.id, name) or tostring(doc.id);

	if ( doc.isNew ) then
		table.insert(parts, "new, not saved");
	else
		local changed = SE.DocChangeCount(doc);

		if ( changed > 0 ) then
			table.insert(parts, format("%d unsaved field%s", changed, changed == 1 and "" or "s"));
		end
	end

	for _, text in ipairs(SE.DocWorldDirtyParts and SE.DocWorldDirtyParts(doc) or {}) do
		table.insert(parts, text);
	end

	return format("Spell %s: %s", title, table.concat(parts, ", "));
end

local function SaveAndClose(doc)
	if ( SE.SaveDocWorld ) then
		SE.SaveDocWorld(doc);
	end

	if ( SE.DocIsDirty(doc) ) then
		doc.closeAfterSave = true;

		if ( not SE.pendingSave[doc.id] and not SE.SaveDoc(doc) ) then
			doc.closeAfterSave = nil;
		end
	else
		RemoveDoc(doc);
	end
end

-- Closes tabs, asks Save, Discard or Cancel once for the ones with unsaved changes. Saved tabs close when the server confirms.
function SE.CloseDocs(list, force)
	local dirty = {};

	for _, doc in ipairs(list or {}) do
		if ( SE.DocIndex(doc) ) then
			if ( not force and SE.DocHasUnsaved(doc) ) then
				table.insert(dirty, doc);
			else
				RemoveDoc(doc);
			end
		end
	end

	if ( #dirty == 0 ) then
		return;
	end

	local lines = {};

	for _, doc in ipairs(dirty) do
		table.insert(lines, UnsavedText(doc));
	end

	local title = #dirty == 1 and "Close spell" or format("Close %d spells", #dirty);
	NoggitMessageBox_Show(title, "These changes are not saved.\n\n"..table.concat(lines, "\n"), {
		{ text = "Save", onClick = function()
			for _, doc in ipairs(dirty) do
				SaveAndClose(doc);
			end
		end },
		{ text = "Discard", onClick = function()
			for _, doc in ipairs(dirty) do
				RemoveDoc(doc);
			end
		end },
		{ text = "Cancel" },
	});
end

function SE.CloseDoc(doc, force)
	doc = doc or SE.cur;

	if ( doc ) then
		SE.CloseDocs({ doc }, force);
	end
end

function SE.CloseOtherDocs(keep)
	local list = {};

	for _, doc in ipairs(SE.docs) do
		if ( doc ~= keep ) then
			table.insert(list, doc);
		end
	end

	SE.CloseDocs(list);
end

function SE.CloseSavedDocs()
	local list = {};

	for _, doc in ipairs(SE.docs) do
		if ( not SE.DocHasUnsaved(doc) and not SE.pendingSave[doc.id] ) then
			table.insert(list, doc);
		end
	end

	SE.CloseDocs(list);
end

function SE.ApplyLocal(quiet)
	local cur = SE.cur;

	if ( not cur ) then
		return false;
	end

	if ( not DbcEditor_ApplyLocal ) then
		GameEditor_Notify("DbcEditor_ApplyLocal is not available in this client build.", "error");
		return false;
	end

	local row = SE.FullRow(SE.TABLE, cur.values);
	row.m_id = cur.id;
	local ok, err = pcall(DbcEditor_ApplyLocal, SE.TABLE, cur.id, row);

	if ( not ok ) then
		GameEditor_Notify("Apply locally failed: "..tostring(err), "error");
		return false;
	end

	cur.appliedLocal = true;

	if ( not quiet ) then
		GameEditor_Notify(format("Spell %d applied to this client only. Save to keep it.", cur.id), "info");
	end

	SE.QueueRefresh();
	return true;
end

function SE.Revert()
	local cur = SE.cur;

	if ( not cur ) then
		return;
	end

	if ( cur.isNew ) then
		ReplaceValues(cur, SE.Copy(cur.original), cur.original);
		return;
	end

	SE.DiscardSpellLocal(cur);
	local row = SE.GetRow(SE.TABLE, cur.id) or SE.Copy(cur.original);
	cur.exists = true;
	cur.remoteChanged = false;
	ReplaceValues(cur, row, SE.Copy(row));
end

local function AnyOtherNew(doc)
	for _, other in ipairs(SE.docs) do
		if ( other ~= doc and other.isNew ) then
			return true;
		end
	end

	return false;
end

SE.On("DBC_EDITOR_RESULT", function(ok, tableName, id)
	id = tonumber(id);

	if ( SE.Lower(tableName) ~= "spell" or not id ) then
		return;
	end

	SE.pendingSave[id] = nil;
	local doc = SE.FindDoc(id);

	if ( not doc ) then
		SE.QueueRefresh();
		return;
	end

	if ( SE.IsOk(ok) ) then
		doc.original = doc.saving or SE.Copy(doc.values);
		doc.isNew = false;
		doc.exists = true;
		doc.appliedLocal = false;
		doc.remoteChanged = false;
		doc.saving = nil;

		if ( not AnyOtherNew(doc) ) then
			SE.reservedIds[SE.TABLE] = nil;
		end

		SE.stamp = SE.stamp + 1;

		if ( doc == SE.cur ) then
			SE.RefreshVisibleGrids();
		end

		SE.Fire("SPELL_SAVED", id, doc);

		if ( doc.closeAfterSave ) then
			doc.closeAfterSave = nil;

			if ( not SE.DocHasUnsaved(doc) ) then
				RemoveDoc(doc);
			end
		end
	else
		doc.closeAfterSave = nil;
	end

	doc.saving = nil;
	SE.QueueRefresh();
end);

SE.On("DBC_RECORD_UPDATED", function(tableName, id)
	id = tonumber(id);
	local doc = SE.Lower(tableName) == "spell" and SE.FindDoc(id);

	if ( not doc or SE.pendingSave[id] ) then
		SE.QueueRefresh();
		return;
	end

	doc.appliedLocal = false;

	if ( not SE.DocIsDirty(doc) ) then
		local row = SE.GetRow(SE.TABLE, id);

		if ( row ) then
			doc.exists = true;
			doc.remoteChanged = false;
			ReplaceValues(doc, row, SE.Copy(row));
			GameEditor_Notify(format("Spell %d was changed elsewhere and has been reloaded.", id), "info");
		end
	else
		doc.remoteChanged = true;
		GameEditor_Notify(format("Spell %d was changed elsewhere. Revert loads that version, Save overwrites it.", id), "info");
		SE.QueueRefresh();
	end
end);
