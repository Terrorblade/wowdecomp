-- Loot workspace: source browser in the main area, the loot tree of the open source in the dock.

GameEditorLoot = GameEditorLoot or {};
local L = GameEditorLoot;

local LEFT_OFFSET = 56;
local BROWSER_WIDTH = 340;
local RECENT_MAX = 20;

local Num = L.Num;

local view;
local searchBox, filterCombo, list, status, tableCombo, idBox;
local recent = {};

L.current = L.current or { roots = {}, title = nil };

local function Common()
	return GameEditorObject;
end

-- Opening -------------------------------------------------------------------------------------------------------

local function SameRoots(a, b)
	if ( #a ~= #b ) then
		return false;
	end

	for index, root in ipairs(a) do
		if ( root.table ~= b[index].table or Num(root.key) ~= Num(b[index].key) ) then
			return false;
		end
	end

	return true;
end

local function Remember(roots, title)
	for index, entry in ipairs(recent) do
		if ( SameRoots(entry.roots, roots) ) then
			table.remove(recent, index);
			break;
		end
	end

	table.insert(recent, 1, { roots = roots, title = title });

	while ( #recent > RECENT_MAX ) do
		table.remove(recent);
	end
end

local function CopyRoots(roots)
	local copy = {};

	for index, root in ipairs(roots) do
		copy[index] = { table = root.table, key = Num(root.key), label = root.label };
	end

	return copy;
end

-- L.OpenRoots(roots, title) shows those loot ids in the Loot workspace.
function L.OpenRoots(roots, title)
	roots = CopyRoots(roots or {});

	if ( #roots == 0 ) then
		GameEditor_Notify("This source has no loot id", "info");
		return;
	end

	L.current = { roots = roots, title = title };
	Remember(roots, title);

	if ( GameEditor_GetWorkspace() ~= "loot" ) then
		GameEditor_SetWorkspace("loot", "select");
	else
		GameEditor_SelectTool("select");
	end

	if ( view ) then
		view:SetRoots(roots, title);
	end

	L.RefreshBrowser(true);
end

function L.OpenTable(index, key)
	local info = L.TABLES[index];
	key = Num(key);

	if ( not info or key <= 0 ) then
		return;
	end

	local name = L.SourceName(info.source, key);
	local title = format("%s %d%s", info.title, key, name and ("  "..name) or "");
	L.OpenRoots({ { table = info.table, key = key, label = info.title } }, title);
end

-- Browser --------------------------------------------------------------------------------------------------------

local FILTERS = { { text = "Every source", value = nil } };

for _, key in ipairs(L.SOURCE_ORDER) do
	table.insert(FILTERS, { text = L.SOURCES[key].title, value = key });
end

local function FilterValue()
	return filterCombo and FILTERS[filterCombo.selected or 1].value;
end

-- One browser line per source, a creature gets one line holding its loot, pickpocket and skinning ids.
local function GroupResults(rows)
	local groups = {};
	local ordered = {};

	for _, row in ipairs(rows or {}) do
		local info = L.TABLES[row.type or -1];

		if ( info ) then
			local id = info.source..":"..tostring(row.entry);
			local group = groups[id];

			if ( not group ) then
				group = { source = info.source, entry = row.entry, name = row.name or "", subName = row.subName or "", minLevel = row.minLevel, maxLevel = row.maxLevel, roots = {}, tags = {} };
				groups[id] = group;
				table.insert(ordered, group);
			end

			table.insert(group.roots, { table = info.table, key = row.displayId, label = info.title });
			table.insert(group.tags, info.short);
		end
	end

	return ordered;
end

local function GroupTitle(group)
	local source = L.SOURCES[group.source];
	local name = group.name ~= "" and group.name or (L.SourceName(group.source, group.entry) or "");
	return format("%s %d%s", source and source.title or group.source, group.entry, name ~= "" and ("  "..name) or "");
end

local grouped, groupedFrom, groupedCount;

-- Every source with loot as browser lines, rebuilt when the loot list changes.
local function AllSources()
	local rows, loading = GameEditor_TemplateList("loot");

	if ( not grouped or groupedFrom ~= rows or groupedCount ~= #rows ) then
		local hits = {};

		for index, item in ipairs(rows) do
			hits[index] = item.row;
		end

		grouped = {};

		for _, group in ipairs(GroupResults(hits)) do
			local title = GroupTitle(group);
			table.insert(grouped, { group = group, roots = group.roots, title = title, text = title, tag = table.concat(group.tags, ", ") });
		end

		groupedFrom, groupedCount = rows, #rows;
	end

	return grouped, loading;
end

-- Loot ids of one table that some source uses, for the open by id picker.
local function TableIds(index)
	local items, seen = {}, {};

	for _, item in ipairs((GameEditor_TemplateList("loot"))) do
		local row = item.row;

		if ( row.type == index and not seen[row.displayId] ) then
			seen[row.displayId] = true;
			table.insert(items, { value = row.displayId, text = row.name or "" });
		end
	end

	table.sort(items, function(a, b) return a.value < b.value; end);
	return items, select(2, GameEditor_TemplateList("loot"));
end

local function Items()
	local text = strtrim(searchBox and searchBox:GetText() or "");
	local filter = FilterValue();
	local all = AllSources();
	local items = {};

	if ( filter ) then
		for _, item in ipairs(all) do
			if ( item.group.source == filter ) then
				table.insert(items, item);
			end
		end

		all = items;
		items = {};
	end

	if ( text ~= "" ) then
		local number = tonumber(text);
		local found = GameEditor_FilterList(all, text);

		if ( not number or number <= 0 or (filter and filter ~= "mail" and filter ~= "reference") ) then
			return found;
		end

		-- Mail and reference ids have no source, a typed number offers them directly.
		for _, index in ipairs({ L.REFERENCE - 1, L.REFERENCE }) do
			local info = L.TABLES[index];
			table.insert(items, { roots = { { table = info.table, key = number, label = info.title } }, title = format("%s %d", info.title, number), tag = info.short });
		end

		for _, item in ipairs(found) do
			table.insert(items, item);
		end

		return items;
	end

	for _, entry in ipairs(recent) do
		table.insert(items, { recent = true, roots = entry.roots, title = entry.title or "", tag = "recent" });
	end

	if ( #items == 0 ) then
		return all;
	end

	for _, item in ipairs(all) do
		table.insert(items, item);
	end

	return items;
end

function L.RefreshBrowser(keepSelection)
	if ( not list ) then
		return;
	end

	local items = Items();
	NoggitListBox_SetItems(list, items, keepSelection);
	list.selected = nil;

	for index, item in ipairs(items) do
		if ( SameRoots(item.roots, L.current.roots) ) then
			list.selected = index;
			break;
		end
	end

	NoggitListBox_Refresh(list);

	local all, loading = AllSources();

	if ( loading ) then
		status:SetText(#all > 0 and format("%d of %d, still loading", #items, #all) or "Loading every source with loot from the server");
	else
		status:SetText(format("%d of %d sources with loot", #items, #all));
	end
end

local function FilterBrowser()
	L.RefreshBrowser();
end

GameEditor_OnTemplateList("loot", function()
	L.RefreshBrowser(true);
end);

function L.FocusSearch()
	if ( searchBox ) then
		searchBox:SetFocus();
		searchBox:HighlightText();
	end
end

local function BuildBrowser(parent)
	local frame = CreateFrame("Frame", "GameEditorLootBrowser", parent, "NoggitBoxTemplate");
	NoggitBox_Set(frame, NOGGIT_COLOR_BASE, NOGGIT_COLOR_MID);
	frame:SetPoint("TOPLEFT", parent, "TOPLEFT", LEFT_OFFSET, -6);
	frame:SetPoint("BOTTOMLEFT", parent, "BOTTOMLEFT", LEFT_OFFSET, 6);
	frame:SetWidth(BROWSER_WIDTH);
	frame:EnableMouse(true);
	frame:EnableMouseWheel(true);

	local title = Common().Label(frame, "Loot", "NoggitFont16", NOGGIT_COLOR_WHITE);
	title:SetPoint("TOPLEFT", frame, "TOPLEFT", 10, -8);

	filterCombo = NoggitComboBox_Create(frame, 130, FILTERS, 1, function() L.RefreshBrowser(); end);
	filterCombo:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -8, -6);

	local searchButton = Common().Button(frame, "Reload", 64, function()
		GameEditor_ReloadTemplateList("loot");
		L.RefreshBrowser(true);
	end, "Ask the server for every source with loot again");
	searchButton:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -8, -34);

	searchBox = NoggitLineEdit_Create(frame, 200);
	searchBox:SetHeight(22);
	searchBox:SetPoint("TOPLEFT", frame, "TOPLEFT", 8, -34);
	searchBox:SetPoint("RIGHT", searchButton, "LEFT", -4, 0);
	searchBox:SetAutoFocus(false);

	local hint = Common().Label(searchBox, "Name or id", nil, NOGGIT_COLOR_DISABLED);
	hint:SetPoint("LEFT", searchBox, "LEFT", 6, 0);

	searchBox:SetScript("OnEditFocusGained", function() hint:Hide(); end);
	searchBox:SetScript("OnEditFocusLost", function(self)
		if ( self:GetText() == "" ) then
			hint:Show();
		end
	end);
	searchBox:SetScript("OnEnterPressed", function(self)
		self:ClearFocus();
		local item = list.items[list.selected or 1];

		if ( item ) then
			L.OpenRoots(item.roots, item.title);
		end
	end);
	searchBox:SetScript("OnEscapePressed", function(self)
		self:SetText("");
		self:ClearFocus();
	end);
	searchBox:SetScript("OnTextChanged", function()
		GameEditor_Debounce(FilterBrowser, #AllSources());
	end);

	local openBar = CreateFrame("Frame", nil, frame);
	openBar:SetHeight(22);
	openBar:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 8, 8);
	openBar:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -8, 8);

	local tableItems = {};

	for index = 0, L.REFERENCE do
		table.insert(tableItems, { text = L.TABLES[index].title, value = index });
	end

	tableCombo = NoggitComboBox_Create(openBar, 150, tableItems, 1);
	tableCombo:SetPoint("LEFT", openBar, "LEFT", 0, 0);

	idBox = NoggitLineEdit_Create(openBar, 80);
	idBox:SetHeight(22);
	idBox:SetMaxLetters(10);
	idBox:SetAutoFocus(false);
	idBox:SetPoint("LEFT", tableCombo, "RIGHT", 4, 0);

	local function OpenTyped()
		local key = tonumber(idBox:GetText());

		if ( not key or key <= 0 ) then
			GameEditor_Notify("Type a loot id first", "info");
			return;
		end

		idBox:ClearFocus();
		L.OpenTable(tableItems[tableCombo.selected or 1].value, math.floor(key));
	end

	idBox:SetScript("OnEnterPressed", OpenTyped);
	idBox:SetScript("OnEscapePressed", function(self) self:ClearFocus(); end);

	local open = Common().Button(openBar, "Open", 56, OpenTyped, "Open that loot id of the table, an unused id starts empty and Save creates it");
	open:SetPoint("LEFT", idBox, "RIGHT", 4, 0);
	open:SetPoint("RIGHT", openBar, "RIGHT", 0, 0);

	local openLabel = Common().Label(frame, "Open a loot id directly", nil, NOGGIT_COLOR_DISABLED);
	openLabel:SetPoint("BOTTOMLEFT", openBar, "TOPLEFT", 2, 4);

	status = Common().Label(frame, "", nil, NOGGIT_COLOR_DISABLED);
	status:SetPoint("BOTTOMLEFT", openLabel, "TOPLEFT", 0, 8);
	status:SetPoint("RIGHT", frame, "RIGHT", -10, 0);

	list = NoggitListBox_Create(frame, 20, function(row)
		row.tag = NoggitUI_FontString(row, "", nil, "RIGHT");
		row.tag:SetPoint("RIGHT", row, "RIGHT", -4, 0);
		row.tag:SetWidth(120);
		NoggitText_SetColor(row.tag, NOGGIT_COLOR_DISABLED);
		row.name = NoggitUI_FontString(row, "");
		row.name:SetPoint("LEFT", row, "LEFT", 4, 0);
		row.name:SetPoint("RIGHT", row.tag, "LEFT", -4, 0);
		row.name:SetJustifyH("LEFT");
	end, function(row, item)
		local open = SameRoots(item.roots, L.current.roots);
		row.name:SetText((open and "> " or "")..(item.title or ""));
		row.tag:SetText(item.tag or "");
	end);
	list:SetPoint("TOPLEFT", searchBox, "BOTTOMLEFT", 0, -6);
	list:SetPoint("RIGHT", frame, "RIGHT", -8, 0);
	list:SetPoint("BOTTOM", status, "TOP", 0, 6);
	list.onSelect = function(item)
		L.OpenRoots(item.roots, item.title);
	end;

	L.RefreshBrowser();
	return frame;
end

function L.BuildMain(parent)
	local root = CreateFrame("Frame", "GameEditorLootMain", parent);
	root:SetAllPoints(parent);
	BuildBrowser(root);
	return root;
end

-- Dock -----------------------------------------------------------------------------------------------------------

local function BuildDock(parent)
	local frame = CreateFrame("Frame", "GameEditorLootDock", parent);

	view = L.CreateView(frame);
	view:SetPoint("TOPLEFT", frame, "TOPLEFT", 6, -6);
	view:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -6, 6);

	local empty = Common().Paragraph(view, "Search a creature, gameobject, item, fishing area or spell on the left, or open a loot id of any table directly. The tree shows the own loot of the source first and every reference it rolls below it.", NOGGIT_COLOR_TEXT);
	empty:SetPoint("TOPLEFT", view, "TOPLEFT", 2, -70);
	empty:SetPoint("TOPRIGHT", view, "TOPRIGHT", -2, -70);
	view.empty = empty;

	local base = view.Rebuild;
	view.Rebuild = function(self)
		base(self);

		if ( #self.roots == 0 ) then
			self.empty:Show();
		else
			self.empty:Hide();
		end
	end;

	view:SetRoots(L.current.roots, L.current.title or "No loot open");
	return frame;
end

-- Workspace ------------------------------------------------------------------------------------------------------

local function SaveAll()
	if ( not view or not view:Save() ) then
		GameEditor_Notify("Nothing to save", "info");
	end
end

local function Reload()
	if ( view ) then
		view:Reload();
	end
end

local function OpenById()
	if ( idBox ) then
		idBox:SetFocus();
		idBox:HighlightText();
	end
end

local separator = { separator = true };

local function BuildMenus()
	local openMenu = { title = "Open" };

	for index = 0, L.REFERENCE do
		local info = L.TABLES[index];
		table.insert(openMenu, { text = info.title.." id...", onClick = function()
			GameEditor_PickValue("Open "..string.lower(info.title), function() return TableIds(index); end, nil, function(value) L.OpenTable(index, value); end, nil, { anyNumber = true });
		end });
	end

	return {
		{
			title = "Loot",
			{ text = "Find a source", shortcut = "Ctrl+F", onClick = function() L.FocusSearch(); end },
			{ text = "Open a loot id", shortcut = "Ctrl+O", onClick = OpenById },
			separator,
			{ text = "Save all changes", shortcut = "Ctrl+S", onClick = SaveAll },
			{ text = "Revert all changes", onClick = function() if ( view ) then view:Revert(); end end },
			{ text = "Reload from server", shortcut = "F5", onClick = Reload },
		},
		openMenu,
	};
end

local KEYS = {
	{ "Ctrl+S", "Save every changed loot id of the open source" },
	{ "F5", "Load the open loot again from the server" },
	{ "Ctrl+F", "Search loot sources by name or id" },
	{ "Ctrl+O", "Open a loot id of a table directly" },
	{ "Del", "Delete the selected loot row" },
};

local HOTKEYS = {
	["CTRL-S"] = SaveAll,
	["F5"] = Reload,
	["CTRL-F"] = function() L.FocusSearch(); end,
	["CTRL-O"] = OpenById,
	["DELETE"] = function() return view and view:DeleteSelected(); end,
};

local function Status()
	if ( #L.current.roots == 0 ) then
		return { "No loot open", "Search a source on the left or open a loot id" };
	end

	local cells = { L.current.title or "Loot" };

	if ( view and view:HasChanges() ) then
		table.insert(cells, "|cffffcc00Unsaved changes|r");
	else
		table.insert(cells, "saved");
	end

	return cells;
end

GameEditor_RegisterWorkspace({
	key = "loot",
	title = "Loot",
	category = "Items",
	order = 8,
	dockWidth = 600,
	tools = {
		{ key = "select", name = "Loot editor", icon = 0xF8A4, build = BuildDock },
	},
	menus = BuildMenus,
	keys = KEYS,
	buildMain = L.BuildMain,
	onActivate = function()
		L.RefreshBrowser(true);

		if ( view ) then
			view:Rebuild();
		end
	end,
	onHotkey = function(key)
		local handler = HOTKEYS[key];

		if ( handler ) then
			return handler() ~= false;
		end

		return false;
	end,
	status = Status,
});
