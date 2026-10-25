-- Quest editor state: the open quest, its world table rows, quest search and the name caches.

GameEditorQuest = GameEditorQuest or {};
local Q = GameEditorQuest;
local Data = GameEditorData or {};

local RECENT_MAX = 20;
local LOOKUP_TIMEOUT = 5;
local PROBE_MAX = 200;

-- Single row tables keyed on the quest id, and the giver lists keyed on the quest column.
Q.TABLES = {
	template = { table = "quest_template", key = "ID", defaults = { QuestType = 2, QuestLevel = 1 } },
	addon = { table = "quest_template_addon", key = "ID", optional = true },
	mail = { table = "quest_mail_sender", key = "QuestId", optional = true },
	details = { table = "quest_details", key = "ID", optional = true },
	request = { table = "quest_request_items", key = "ID", optional = true },
	offer = { table = "quest_offer_reward", key = "ID", optional = true },
	creatureStart = { table = "creature_queststarter", key = "quest", list = true, kind = "creature" },
	creatureEnd = { table = "creature_questender", key = "quest", list = true, kind = "creature" },
	objectStart = { table = "gameobject_queststarter", key = "quest", list = true, kind = "gameobject" },
	objectEnd = { table = "gameobject_questender", key = "quest", list = true, kind = "gameobject" },
	trigger = { table = "areatrigger_involvedrelation", key = "quest", list = true, kind = "dbc:AreaTrigger" },
};

-- Save order matters, the template has to exist before the server accepts the rest of a new quest.
Q.SAVE_ORDER = { "template", "addon", "mail", "details", "request", "offer", "creatureStart", "creatureEnd", "objectStart", "objectEnd", "trigger" };

Q.id = nil;
Q.state = {};
Q.names = {};
Q.meta = {};
Q.recent = {};
Q.maxSeen = 0;

local byTable = {};

for name, info in pairs(Q.TABLES) do
	info.name = name;
	byTable[string.lower(info.table)] = info;
end

-- Small helpers -------------------------------------------------------------------------------------------

local function IsOk(value)
	return value == 1 or value == true;
end

function Q.Copy(source)
	local copy = {};

	for key, value in pairs(source or {}) do
		copy[key] = value;
	end

	return copy;
end

-- QuestSortID arrives unsigned in the display id slot of a search hit.
function Q.Signed(value)
	value = tonumber(value) or 0;

	if ( value >= 2147483648 ) then
		value = value - 4294967296;
	end

	return value;
end

function Q.SortText(value)
	value = tonumber(value) or 0;

	if ( value < 0 ) then
		local name = Data.Enums and Data.Enums.QuestSort and Data.Enums.QuestSort[-value];
		return name or format("sort %d", -value);
	end

	if ( value > 0 ) then
		return GameEditor_ResolveRef("dbc:AreaTable", value) or format("zone %d", value);
	end

	return "";
end

-- Events ----------------------------------------------------------------------------------------------------

local listeners = {};

function Q.On(event, func)
	listeners[event] = listeners[event] or {};
	table.insert(listeners[event], func);
end

function Q.Fire(event, ...)
	for _, func in ipairs(listeners[event] or {}) do
		local ok, err = pcall(func, ...);

		if ( not ok ) then
			GameEditor_Print("Quest editor: "..tostring(err), 1, 0.3, 0.3);
		end
	end
end

-- Names ------------------------------------------------------------------------------------------------------

function Q.Remember(id, title, meta)
	id = tonumber(id);

	if ( not id or id <= 0 ) then
		return;
	end

	if ( title and title ~= "" ) then
		Q.names[id] = title;
		GameEditor_TemplateListPut("quest", id, title, meta and { maxLevel = meta.level, minLevel = meta.minLevel, type = meta.info });
	end

	if ( meta ) then
		Q.meta[id] = meta;
	end

	if ( id > Q.maxSeen ) then
		Q.maxSeen = id;
	end
end

function Q.NameOf(id)
	id = tonumber(id) or 0;
	return Q.names[id] or GameEditor_TemplateName("quest", id);
end

-- Level and sort of a quest, from its rows or from the quest list.
function Q.MetaOf(id)
	id = tonumber(id) or 0;
	local row = not Q.meta[id] and GameEditor_TemplateRow("quest", id);
	return Q.meta[id] or (row and { level = row.maxLevel, minLevel = row.minLevel, info = row.type, sort = Q.Signed(row.displayId) }) or nil;
end

GameEditor_OnTemplateList("quest", function()
	local items = GameEditor_TemplateList("quest");
	local last = items[#items];

	if ( last and last.value > Q.maxSeen ) then
		Q.maxSeen = last.value;
	end

	Q.Fire("NAMES", "quest");
end);

function Q.RememberRecent(id)
	for index = #Q.recent, 1, -1 do
		if ( Q.recent[index] == id ) then
			table.remove(Q.recent, index);
		end
	end

	table.insert(Q.recent, 1, id);

	while ( #Q.recent > RECENT_MAX ) do
		table.remove(Q.recent);
	end
end

-- Name lookups go out one at a time per kind.
local lookups = { creature = {}, gameobject = {}, quest = {} };
local asked = { creature = {}, gameobject = {}, quest = {} };
local pendingLookup = {};

local function KnownName(kind, id)
	if ( kind == "quest" ) then
		return Q.NameOf(id);
	end

	return GameEditorObject and GameEditorObject.NameOf(kind, id);
end

local function PumpLookup(kind)
	if ( pendingLookup[kind] or not TemplateSearch_Query ) then
		return;
	end

	while ( #lookups[kind] > 0 ) do
		local id = table.remove(lookups[kind], 1);

		if ( not KnownName(kind, id) ) then
			local requestId = TemplateSearch_Query(kind, tostring(id), 5);

			if ( requestId ) then
				pendingLookup[kind] = { requestId = requestId, id = id, expires = GetTime() + LOOKUP_TIMEOUT };
			end

			return;
		end
	end
end

-- Asks the server once for the name of an entry nothing has named yet.
function Q.QueueLookup(kind, id)
	id = tonumber(id);

	if ( not id or id <= 0 or not asked[kind] or asked[kind][id] or KnownName(kind, id) ) then
		return;
	end

	asked[kind][id] = true;
	table.insert(lookups[kind], id);
	PumpLookup(kind);
end

local function OnTemplateResults(kind, requestId)
	local data = TemplateSearch_GetResults and TemplateSearch_GetResults(kind);

	if ( not data or not pendingLookup[kind] or pendingLookup[kind].requestId ~= requestId ) then
		return;
	end

	for _, row in ipairs(data.rows or {}) do
		if ( kind == "quest" ) then
			Q.Remember(row.entry, row.name, { level = row.maxLevel, minLevel = row.minLevel, info = row.type, sort = Q.Signed(row.displayId) });
		elseif ( GameEditorObject ) then
			GameEditorObject.RememberName(kind, row.entry, row.name);
		end
	end

	pendingLookup[kind] = nil;

	Q.Fire("NAMES", kind);
	PumpLookup(kind);
end

local function UpdateLookups()
	local now = GetTime();

	for kind, pending in pairs(pendingLookup) do
		if ( pending and now > pending.expires ) then
			pendingLookup[kind] = nil;
			PumpLookup(kind);
		end
	end
end

-- Ref pickers ------------------------------------------------------------------------------------------------

local combined, combinedFrom = nil, {};

-- Creatures as they are and gameobjects as negative entries, the way quest columns store them.
local function CreatureOrObjectList()
	local creatures, loadingCreatures = GameEditor_TemplateList("creature");
	local objects, loadingObjects = GameEditor_TemplateList("gameobject");

	if ( not combined or combinedFrom[1] ~= creatures or combinedFrom[2] ~= objects or combinedFrom[3] ~= #creatures + #objects ) then
		combined = {};

		for _, item in ipairs(creatures) do
			table.insert(combined, item);
		end

		for _, item in ipairs(objects) do
			table.insert(combined, { value = -item.value, text = "object: "..(item.text or "") });
		end

		combinedFrom = { creatures, objects, #creatures + #objects };
	end

	return combined, loadingCreatures or loadingObjects;
end

GameEditor_RefResolvers = GameEditor_RefResolvers or {};
GameEditor_RefSearchers = GameEditor_RefSearchers or {};

if ( not GameEditor_RefResolvers["quest"] ) then
	GameEditor_RefResolvers["quest"] = function(value)
		local id = math.abs(tonumber(value) or 0);
		local name = Q.NameOf(id);

		if ( not name ) then
			Q.QueueLookup("quest", id);
		end

		return name;
	end;
end

if ( not GameEditor_RefResolvers["creature_or_gameobject"] ) then
	GameEditor_RefResolvers["creature_or_gameobject"] = function(value)
		value = tonumber(value) or 0;
		local kind = value < 0 and "gameobject" or "creature";
		local name = GameEditor_ResolveRef(kind, math.abs(value));

		if ( not name ) then
			Q.QueueLookup(kind, math.abs(value));
		end

		return name and (value < 0 and ("object: "..name) or name) or (value < 0 and "gameobject" or "creature");
	end;
end

GameEditor_RefLists["creature_or_gameobject"] = CreatureOrObjectList;

if ( not GameEditor_RefResolvers["quest_sort"] ) then
	GameEditor_RefResolvers["quest_sort"] = function(value)
		return Q.SortText(value);
	end;
end

-- Quest sorts as negative values, then every zone.
GameEditor_RefLists["quest_sort"] = function()
	local items = {};

	for id, name in pairs(Data.Enums and Data.Enums.QuestSort or {}) do
		table.insert(items, { value = -id, text = "sort: "..name });
	end

	table.sort(items, function(a, b) return a.value > b.value; end);

	for _, item in ipairs(GameEditor_DbcList("AreaTable")) do
		table.insert(items, { value = item.value, text = "zone: "..item.text });
	end

	return items;
end;

-- World table state -------------------------------------------------------------------------------------------

local function ResetState()
	Q.state = {};

	for name in pairs(Q.TABLES) do
		Q.state[name] = { rows = {}, columns = {}, loaded = false, loading = false, saving = false };
	end
end

ResetState();

function Q.Query(name)
	local info = Q.TABLES[name];
	local state = Q.state[name];

	if ( not Q.id or not WorldTable_Query ) then
		return;
	end

	state.loading = true;
	state.error = nil;
	pcall(WorldTable_Query, info.table, Q.id);
end

function Q.QueryAll()
	for _, name in ipairs(Q.SAVE_ORDER) do
		Q.Query(name);
	end

	Q.Fire("STATE");
end

function Q.IsLoaded(name)
	return Q.state[name] and Q.state[name].loaded;
end

function Q.RowExists(name)
	local state = Q.state[name];
	return state and state.loaded and #state.rows > 0;
end

function Q.IsNew()
	return Q.id ~= nil and Q.IsLoaded("template") and not Q.RowExists("template");
end

-- A row of the table with every column the server reported, numbers 0 and strings empty.
function Q.Defaults(name)
	local info = Q.TABLES[name];
	local row = {};

	for _, column in ipairs(Q.state[name].columns) do
		row[column.name] = column.type == "string" and "" or 0;
	end

	for column, value in pairs(info.defaults or {}) do
		row[column] = value;
	end

	row[info.key] = Q.id;
	return row;
end

-- The row shown in an editor, the database row or the defaults when there is none.
function Q.Row(name)
	local state = Q.state[name];

	if ( state.rows[1] ) then
		return Q.Copy(state.rows[1]);
	end

	return Q.Defaults(name);
end

function Q.ListIds(name)
	local ids = {};

	for _, row in ipairs(Q.state[name].rows) do
		table.insert(ids, tonumber(row.id) or 0);
	end

	table.sort(ids);
	return ids;
end

function Q.IsSaving()
	for _, state in pairs(Q.state) do
		if ( state.saving ) then
			return true;
		end
	end

	return false;
end

function Q.IsLoading()
	for _, state in pairs(Q.state) do
		if ( state.loading ) then
			return true;
		end
	end

	return false;
end

local function Send(name, rows)
	local info = Q.TABLES[name];
	local state = Q.state[name];

	if ( not Q.id or not WorldTable_Save ) then
		GameEditor_Notify("Saving quests needs WorldTable_Save in this client build", "error");
		return false;
	end

	local ok, sent = pcall(WorldTable_Save, info.table, Q.id, rows);

	if ( not ok or not sent ) then
		GameEditor_Notify(format("Could not send %s: %s", info.table, tostring(ok and "bad rows" or sent)), "error");
		return false;
	end

	state.saving = true;
	Q.Fire("STATE");
	return true;
end

-- Writes one row made of the table columns, values the table does not have are left out.
function Q.SaveRow(name, values)
	local info = Q.TABLES[name];
	local state = Q.state[name];

	if ( not state.loaded ) then
		GameEditor_Notify(info.table.." is not loaded yet", "info");
		return false;
	end

	local row = {};

	for _, column in ipairs(state.columns) do
		local value = values[column.name];

		if ( value == nil ) then
			value = column.type == "string" and "" or 0;
		end

		row[column.name] = value;
	end

	row[info.key] = Q.id;
	return Send(name, { row });
end

function Q.DeleteRows(name)
	if ( not Q.state[name].loaded ) then
		return false;
	end

	return Send(name, {});
end

-- Replaces the giver list of a relation table with these ids.
function Q.SaveList(name, ids)
	local info = Q.TABLES[name];

	if ( not Q.state[name].loaded ) then
		GameEditor_Notify(info.table.." is not loaded yet", "info");
		return false;
	end

	local rows = {};

	for _, id in ipairs(ids) do
		table.insert(rows, { id = id, quest = Q.id });
	end

	return Send(name, rows);
end

-- Opening quests ------------------------------------------------------------------------------------------------

-- Q.Open(id, seed) loads every table of the quest, seed holds values a new quest starts with per table.
function Q.Open(id, seed)
	id = tonumber(id);

	if ( not id or id <= 0 ) then
		return;
	end

	Q.id = id;
	Q.seed = seed;
	ResetState();
	Q.RememberRecent(id);
	Q.Remember(id);
	Q.Fire("OPENED", id);
	Q.QueryAll();
end

function Q.Reload()
	if ( not Q.id ) then
		GameEditor_Notify("Open a quest first", "info");
		return;
	end

	Q.QueryAll();
end

function Q.Close()
	Q.id = nil;
	Q.seed = nil;
	ResetState();
	Q.Fire("OPENED", nil);
end

-- Seed values for a table of a new quest, used once.
function Q.TakeSeed(name)
	local seed = Q.seed and Q.seed[name];

	if ( seed ) then
		Q.seed[name] = nil;
	end

	return seed;
end

-- Free id search ------------------------------------------------------------------------------------------------

local probe;

-- Q.FindFreeId(start, callback) walks up from start until quest_template has no row, callback(id) or callback(nil).
function Q.FindFreeId(start, callback)
	start = tonumber(start);

	if ( not start or start <= 0 or not WorldTable_Query ) then
		callback(nil);
		return;
	end

	while ( Q.NameOf(start) ) do
		start = start + 1;
	end

	probe = { id = start, tries = 0, callback = callback };
	pcall(WorldTable_Query, "quest_template", start);
end

local function OnProbeRows(key)
	if ( not probe or probe.id ~= key ) then
		return;
	end

	local ok, data = pcall(WorldTable_GetRows, "quest_template", key);
	local exists = ok and type(data) == "table" and data.rows and #data.rows > 0;

	if ( not exists ) then
		local callback = probe.callback;
		probe = nil;
		callback(key);
		return;
	end

	Q.Remember(key, data.rows[1].LogTitle);
	probe.tries = probe.tries + 1;

	if ( probe.tries >= PROBE_MAX ) then
		local callback = probe.callback;
		probe = nil;
		callback(nil);
		return;
	end

	probe.id = key + 1;

	while ( Q.NameOf(probe.id) ) do
		probe.id = probe.id + 1;
	end

	pcall(WorldTable_Query, "quest_template", probe.id);
end

-- Server replies ------------------------------------------------------------------------------------------------

local function OnRows(tableName, key, ok, message)
	key = tonumber(key);
	local info = byTable[string.lower(tostring(tableName or ""))];

	if ( info and info.name == "template" ) then
		OnProbeRows(key);
	end

	if ( not info or not Q.id or key ~= Q.id ) then
		return;
	end

	local state = Q.state[info.name];
	state.loading = false;

	if ( ok ~= nil and not IsOk(ok) ) then
		state.error = (message and message ~= "") and message or "the server could not read the rows";
		GameEditor_Notify(info.table..": "..state.error, "error");
		Q.Fire("STATE");
		return;
	end

	local okRows, data = pcall(WorldTable_GetRows, info.table, Q.id);

	if ( not okRows or type(data) ~= "table" ) then
		state.error = "the server sent no rows";
		Q.Fire("STATE");
		return;
	end

	state.rows = data.rows or {};
	state.columns = data.columns or {};
	state.loaded = true;
	state.error = nil;

	if ( info.name == "template" and state.rows[1] ) then
		local row = state.rows[1];
		Q.Remember(Q.id, row.LogTitle, { level = row.QuestLevel, minLevel = row.MinLevel, info = row.QuestInfoID, sort = row.QuestSortID });
	end

	Q.Fire("ROWS", info.name);
	Q.Fire("STATE");
end

local function OnResult(ok, tableName, key, message)
	key = tonumber(key);
	local info = byTable[string.lower(tostring(tableName or ""))];

	if ( not info or not Q.id or key ~= Q.id ) then
		return;
	end

	Q.state[info.name].saving = false;

	if ( IsOk(ok) ) then
		Q.Query(info.name);
		Q.Fire("SAVED", info.name);
	end

	Q.Fire("STATE");
end

local events = CreateFrame("Frame", "GameEditorQuestEvents");
events:RegisterEvent("WORLD_TABLE_ROWS");
events:RegisterEvent("WORLD_TABLE_RESULT");
events:RegisterEvent("EDITOR_TEMPLATE_RESULTS");
events:SetScript("OnEvent", function(_, event, ...)
	if ( event == "WORLD_TABLE_ROWS" ) then
		OnRows(...);
	elseif ( event == "WORLD_TABLE_RESULT" ) then
		OnResult(...);
	elseif ( event == "EDITOR_TEMPLATE_RESULTS" ) then
		local kind, requestId = ...;

		if ( lookups[kind] ) then
			OnTemplateResults(kind, requestId);
		end
	end
end);

local elapsedSinceCheck = 0;

events:SetScript("OnUpdate", function(_, elapsed)
	elapsedSinceCheck = elapsedSinceCheck + elapsed;

	if ( elapsedSinceCheck > 0.5 ) then
		elapsedSinceCheck = 0;
		UpdateLookups();
	end
end);
