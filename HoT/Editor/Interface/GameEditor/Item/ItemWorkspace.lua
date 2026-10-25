-- Items workspace: the item browser or the generator tables in the main area, the item editor in the dock.

GameEditorItem = GameEditorItem or {};
local I = GameEditorItem;
local Editor = I.Editor;

local function InGenerator()
	return I.view == "generator";
end

local function NewItem()
	if ( InGenerator() ) then
		GameEditor_SelectTool("select");
	end

	I.ShowNewDialog();
end

local function CopyItem()
	if ( not I.entry ) then
		GameEditor_Notify("Open an item to copy first", "info");
		return;
	end

	I.ShowNewDialog(true);
end

local function OpenByName()
	GameEditor_PickRef("Open item", "item", I.entry, function(value) I.OpenItem(value); end, { anyNumber = true });
end

local function SaveAll()
	if ( InGenerator() ) then
		I.Generator.SaveAll();
	else
		Editor.SaveAll();
	end
end

local function Reload()
	if ( InGenerator() ) then
		I.Generator.Reload();
		return;
	end

	if ( not I.entry ) then
		GameEditor_Notify("Open an item first", "info");
		return;
	end

	Editor.ConfirmDiscard(function()
		Editor.RevertAll();
		I.Reload();
	end);
end

local function GoTo(index)
	return function()
		GameEditor_SelectTool("select");
		Editor.SelectTab(index);
	end;
end

local separator = { separator = true };

local function BuildMenus()
	local goTo = { title = "Go to" };

	for index, tab in ipairs(GameEditorData.ItemTemplateTabs or {}) do
		table.insert(goTo, { text = tab.title, onClick = GoTo(index) });
	end

	table.insert(goTo, { text = "Locale", onClick = GoTo(#goTo + 1) });
	table.insert(goTo, separator);
	table.insert(goTo, { text = "Item generator tables", onClick = function() GameEditor_SelectTool("generator"); end });

	return {
		{
			title = "Item",
			{ text = "New item...", shortcut = "Ctrl+N", onClick = NewItem },
			{ text = "Copy to a new entry...", shortcut = "Ctrl+D", onClick = CopyItem },
			{ text = "Open by name or entry...", shortcut = "Ctrl+O", onClick = OpenByName },
			{ text = "Find in the item list", shortcut = "Ctrl+F", onClick = function() I.FocusSearch(); end },
			separator,
			{ text = "Save all changes", shortcut = "Ctrl+S", onClick = SaveAll },
			{ text = "Revert all changes", onClick = function() Editor.RevertAll(); end },
			{ text = "Reload from server", shortcut = "F5", onClick = Reload },
			{ text = "Close the item", onClick = function() Editor.ConfirmDiscard(I.Close); end },
		},
		goTo,
	};
end

local KEYS = {
	{ "Ctrl+S", "Save every changed part, the item tabs or the generator tables" },
	{ "F5", "Load the item or the shown generator table again, asks before dropping edits" },
	{ "Ctrl+N", "New item with a new entry" },
	{ "Ctrl+D", "Copy the open item to a new entry" },
	{ "Ctrl+O", "Open an item by name or entry" },
	{ "Ctrl+F", "Search the item list" },
};

local HOTKEYS = {
	["CTRL-S"] = SaveAll,
	["F5"] = Reload,
	["CTRL-N"] = NewItem,
	["CTRL-D"] = CopyItem,
	["CTRL-O"] = OpenByName,
	["CTRL-F"] = function() I.FocusSearch(); end,
};

local function Status()
	if ( InGenerator() ) then
		local count = I.Generator.DirtyCount();
		local cells = { "Item generator: "..(I.Generator.CurrentTitle() or "") };
		table.insert(cells, count > 0 and format("|cffffcc00%d table%s with unsaved changes|r", count, count == 1 and "" or "s") or "saved");
		return cells;
	end

	if ( not I.entry ) then
		return { "No item open", "Search the list on the left, Ctrl+N creates an item" };
	end

	local cells = { format("Item %d: %s", I.entry, I.NameOf(I.entry) or "?") };

	if ( I.IsSaving() ) then
		table.insert(cells, "saving");
	elseif ( I.IsNew() ) then
		table.insert(cells, "new, not saved");
	elseif ( Editor.HasChanges() ) then
		table.insert(cells, "|cffffcc00Unsaved changes|r");
	else
		table.insert(cells, "saved");
	end

	if ( I.Generator and I.Generator.HasChanges() ) then
		table.insert(cells, "|cffffcc00Generator tables unsaved|r");
	end

	return cells;
end

GameEditor_RefOpeners["item"] = I.OpenItem;

GameEditor_RegisterWorkspace({
	key = "item",
	title = "Items",
	category = "Items",
	order = 6,
	dockWidth = 600,
	tools = {
		{ key = "select", name = "Item editor", icon = 0xF8A4, build = Editor.Build },
		{ key = "generator", name = "Item generator", icon = 0xF8B8, noPanel = true },
	},
	menus = BuildMenus,
	keys = KEYS,
	buildMain = I.BuildMain,
	onActivate = function()
		I.RefreshBrowser(true);
		Editor.Refresh();
	end,
	onToolSelected = function(key)
		I.ShowView(key == "generator" and "generator" or "items");
	end,
	onHotkey = function(key)
		local handler = HOTKEYS[key];

		if ( handler ) then
			handler();
			return true;
		end

		return false;
	end,
	status = Status,
});
