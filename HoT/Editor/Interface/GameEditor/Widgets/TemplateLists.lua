-- Every creature, gameobject, item, quest and loot source the server knows, fetched once per session and filtered locally.

local LIST_LIMIT = 65535;
local TIMEOUT = 30;

local lists = {};
local listeners = {};

local function Format(kind, row)
	local text = row.name or "";

	if ( kind == "item" ) then
		text = format("%s  (ilvl %d)", text, row.maxLevel or 0);
	elseif ( kind ~= "loot" and row.subName and row.subName ~= "" ) then
		text = format("%s <%s>", text, row.subName);
	end

	return text;
end

local function Notify(kind)
	for _, listener in ipairs(listeners[kind] or {}) do
		local ok, err = pcall(listener, kind);

		if ( not ok ) then
			GameEditor_Print("Template list "..kind..": "..tostring(err), 1, 0.3, 0.3);
		end
	end

	GameEditor_RefreshPicker();
end

local function Request(kind)
	local list = lists[kind];

	if ( not TemplateSearch_Query ) then
		list.failed = true;
		return;
	end

	local ok, requestId = pcall(TemplateSearch_Query, kind, "", LIST_LIMIT);
	list.requestId = ok and tonumber(requestId) or nil;
	list.expires = GetTime() + TIMEOUT;
	list.failed = list.requestId == nil;
end

-- GameEditor_TemplateList(kind) returns the items { value, text, row } and true while the server is still sending them.
function GameEditor_TemplateList(kind)
	local list = lists[kind];

	if ( not list ) then
		list = { items = {}, byEntry = {} };
		lists[kind] = list;
		Request(kind);
	end

	return list.items, list.requestId ~= nil;
end

-- The search row of an entry { entry, name, subName, type, displayId, minLevel, maxLevel }, nil until the list arrived.
function GameEditor_TemplateRow(kind, entry)
	local list = lists[kind];
	local item = list and list.byEntry[tonumber(entry) or 0];
	return item and item.row or nil;
end

function GameEditor_TemplateName(kind, entry)
	local row = GameEditor_TemplateRow(kind, entry);
	return row and row.name or nil;
end

function GameEditor_TemplateListLoaded(kind)
	return lists[kind] ~= nil and lists[kind].loaded == true;
end

-- Asks the server again, for when templates changed outside the editor.
function GameEditor_ReloadTemplateList(kind)
	if ( lists[kind] ) then
		Request(kind);
	end
end

-- Keeps a loaded list current after the editor creates or renames a template, fields go into the search row.
function GameEditor_TemplateListPut(kind, entry, name, fields)
	local list = lists[kind];
	entry = tonumber(entry);

	if ( not list or not entry or entry <= 0 or not name or name == "" ) then
		return;
	end

	local item = list.byEntry[entry];

	if ( not item ) then
		if ( not list.loaded ) then
			return;
		end

		item = { value = entry, row = { entry = entry, subName = "", type = 0, displayId = 0, minLevel = 0, maxLevel = 0 } };
		list.byEntry[entry] = item;

		local index = #list.items + 1;

		for i, other in ipairs(list.items) do
			if ( other.value > entry ) then
				index = i;
				break;
			end
		end

		table.insert(list.items, index, item);
	end

	item.row.name = name;

	for key, value in pairs(fields or {}) do
		item.row[key] = value;
	end

	item.text = Format(kind, item.row);
	item.lower = nil;
end

-- listener(kind) runs when a list finished loading.
function GameEditor_OnTemplateList(kind, listener)
	listeners[kind] = listeners[kind] or {};
	table.insert(listeners[kind], listener);
end

for _, kind in ipairs({ "creature", "gameobject", "item", "quest" }) do
	GameEditor_RefLists[kind] = function()
		return GameEditor_TemplateList(kind);
	end;
end

local function OnResults(kind, requestId)
	local list = lists[kind];

	if ( not list or not list.requestId or list.requestId ~= tonumber(requestId) ) then
		return;
	end

	local data = TemplateSearch_GetResults and TemplateSearch_GetResults(kind);
	list.requestId = nil;

	if ( not data ) then
		return;
	end

	local items, byEntry = {}, {};

	for _, row in ipairs(data.rows or {}) do
		local item = { value = row.entry, text = Format(kind, row), row = row };
		table.insert(items, item);
		byEntry[row.entry] = byEntry[row.entry] or item;
	end

	list.items, list.byEntry = items, byEntry;
	list.loaded = true;

	if ( #items == 0 and kind ~= "loot" ) then
		GameEditor_Print(format("The server sent an empty %s list, it may need the list mode of the template search", kind), 1, 0.6, 0.2);
	end

	Notify(kind);
end

local frame = CreateFrame("Frame", "GameEditorTemplateLists");
frame:RegisterEvent("EDITOR_TEMPLATE_RESULTS");
frame:SetScript("OnEvent", function(_, _, kind, requestId)
	OnResults(kind, requestId);
end);
frame:SetScript("OnUpdate", function()
	local now = GetTime();

	for kind, list in pairs(lists) do
		if ( list.requestId and now > list.expires ) then
			list.requestId = nil;
			list.failed = true;
			GameEditor_Print(format("The server did not send the %s list, it may need the list mode of the template search", kind), 1, 0.6, 0.2);
			Notify(kind);
		end
	end
end);
