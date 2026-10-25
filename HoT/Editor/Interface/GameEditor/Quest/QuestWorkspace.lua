-- Quests workspace: browser in the main area, the quest editor in the dock, menus, keys and status.

GameEditorQuest = GameEditorQuest or {};
local Q = GameEditorQuest;
local Editor = Q.Editor;

local TEMPLATE, ADDON, TEXTS, GIVERS = 1, 2, 3, 4;

local function NewQuest()
	Q.ShowNewDialog();
end

local function CopyQuest()
	if ( not Q.id ) then
		GameEditor_Notify("Open a quest to copy first", "info");
		return;
	end

	Q.ShowNewDialog(true);
end

local function OpenById()
	GameEditor_PickRef("Open quest", "quest", Q.id, function(value) Q.OpenQuest(value); end, { anyNumber = true });
end

local function GoTo(index)
	return function()
		GameEditor_SelectTool("select");
		Editor.SelectTab(index);
	end;
end

local separator = { separator = true };

local function BuildMenus()
	return {
		{
			title = "Quest",
			{ text = "New quest...", shortcut = "Ctrl+N", onClick = NewQuest },
			{ text = "Copy to a new ID...", shortcut = "Ctrl+D", onClick = CopyQuest },
			{ text = "Open by title or ID...", shortcut = "Ctrl+O", onClick = OpenById },
			{ text = "Find in the quest list", shortcut = "Ctrl+F", onClick = function() Q.FocusSearch(); end },
			separator,
			{ text = "Save all changes", shortcut = "Ctrl+S", onClick = function() Editor.SaveAll(); end },
			{ text = "Revert all changes", onClick = function() Editor.RevertAll(); end },
			{ text = "Reload from server", shortcut = "F5", onClick = function() Q.Reload(); end },
			{ text = "Close the quest", onClick = function() Editor.ConfirmDiscard(Q.Close); end },
		},
		{
			title = "Go to",
			{ text = "Template", onClick = GoTo(TEMPLATE) },
			{ text = "Addon", onClick = GoTo(ADDON) },
			{ text = "Texts", onClick = GoTo(TEXTS) },
			{ text = "Givers", onClick = GoTo(GIVERS) },
		},
	};
end

local KEYS = {
	{ "Ctrl+S", "Save every changed tab of the quest" },
	{ "F5", "Load the quest again from the server, unsaved edits stay" },
	{ "Ctrl+N", "New quest with a new ID" },
	{ "Ctrl+D", "Copy the open quest to a new ID" },
	{ "Ctrl+O", "Open a quest by title or ID" },
	{ "Ctrl+F", "Search the quest list" },
};

local HOTKEYS = {
	["CTRL-S"] = function() Editor.SaveAll(); end,
	["F5"] = function() Q.Reload(); end,
	["CTRL-N"] = NewQuest,
	["CTRL-D"] = CopyQuest,
	["CTRL-O"] = OpenById,
	["CTRL-F"] = function() Q.FocusSearch(); end,
};

local function Status()
	if ( not Q.id ) then
		return { "No quest open", "Search the list on the left, Ctrl+N creates a quest" };
	end

	local cells = { format("Quest %d: %s", Q.id, Q.NameOf(Q.id) or "?") };

	if ( Q.IsSaving() ) then
		table.insert(cells, "saving");
	elseif ( Q.IsNew() ) then
		table.insert(cells, "new, not saved");
	elseif ( Editor.HasChanges() ) then
		table.insert(cells, "|cffffcc00Unsaved changes|r");
	else
		table.insert(cells, "saved");
	end

	return cells;
end

GameEditor_RegisterWorkspace({
	key = "quest",
	title = "Quests",
	order = 5,
	dockWidth = 560,
	tools = {
		{ key = "select", name = "Quest editor", icon = 0xF8A4, build = Editor.Build },
	},
	menus = BuildMenus,
	keys = KEYS,
	buildMain = Q.BuildMain,
	onActivate = function()
		Q.RefreshBrowser(true);
		Editor.Refresh();
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
