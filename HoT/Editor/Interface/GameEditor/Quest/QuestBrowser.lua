-- Quest browser on the left of the main area and the new quest window.

GameEditorQuest = GameEditorQuest or {};
local Q = GameEditorQuest;
local Common = GameEditorObject;

local LEFT_OFFSET = 56;
local BROWSER_WIDTH = 320;

local root, searchBox, list, status;

-- Opening ----------------------------------------------------------------------------------------------------

function Q.OpenQuest(id)
	id = tonumber(id);

	if ( not id or id <= 0 ) then
		return;
	end

	if ( id == Q.id ) then
		Q.Reload();
		return;
	end

	Q.Editor.ConfirmDiscard(function()
		Q.Open(id);

		if ( GameEditor_GetWorkspace() ~= "quest" ) then
			GameEditor_SetWorkspace("quest");
		end
	end);

	-- While the discard question is open the list keeps marking the quest that is still open.
	Q.RefreshBrowser(true);
end

-- Browser ----------------------------------------------------------------------------------------------------

local function LevelText(meta)
	if ( not meta ) then
		return "";
	end

	local level = tonumber(meta.level) or 0;
	return level > 0 and tostring(level) or "";
end

-- Every quest, recently opened ones first while nothing is typed.
local function Items()
	local text = strtrim(searchBox and searchBox:GetText() or "");
	local all = GameEditor_TemplateList("quest");

	if ( text ~= "" ) then
		return GameEditor_FilterList(all, text);
	end

	local items, seen = {}, {};

	for _, id in ipairs(Q.recent) do
		if ( not seen[id] ) then
			seen[id] = true;
			table.insert(items, { value = id, text = Q.NameOf(id) or "", tag = "recent" });
		end
	end

	if ( #items == 0 ) then
		return all;
	end

	for _, item in ipairs(all) do
		if ( not seen[item.value] ) then
			table.insert(items, item);
		end
	end

	return items;
end

function Q.RefreshBrowser(keepSelection)
	if ( not list ) then
		return;
	end

	local items = Items();
	NoggitListBox_SetItems(list, items, keepSelection);
	list.selected = nil;

	for index, item in ipairs(items) do
		if ( item.value == Q.id ) then
			list.selected = index;
			break;
		end
	end

	NoggitListBox_Refresh(list);

	local all, loading = GameEditor_TemplateList("quest");

	if ( loading ) then
		status:SetText(#all > 0 and format("%d of %d, still loading", #items, #all) or "Loading the quest list from the server");
	elseif ( #items == #all ) then
		status:SetText(format("%d quests", #all));
	else
		status:SetText(format("%d of %d", #items, #all));
	end
end

local function FilterBrowser()
	Q.RefreshBrowser();
end

GameEditor_OnTemplateList("quest", function()
	Q.RefreshBrowser(true);
end);

function Q.FocusSearch()
	if ( searchBox ) then
		searchBox:SetFocus();
		searchBox:HighlightText();
	end
end

local function BuildMain(parent)
	root = CreateFrame("Frame", "GameEditorQuestBrowser", parent, "NoggitBoxTemplate");
	NoggitBox_Set(root, NOGGIT_COLOR_BASE, NOGGIT_COLOR_MID);
	root:SetPoint("TOPLEFT", parent, "TOPLEFT", LEFT_OFFSET, -6);
	root:SetPoint("BOTTOMLEFT", parent, "BOTTOMLEFT", LEFT_OFFSET, 6);
	root:SetWidth(BROWSER_WIDTH);
	root:EnableMouse(true);
	root:EnableMouseWheel(true);

	local title = Common.Label(root, "Quests", "NoggitFont16", NOGGIT_COLOR_WHITE);
	title:SetPoint("TOPLEFT", root, "TOPLEFT", 10, -8);

	local new = Common.Button(root, "New quest", 80, function() Q.ShowNewDialog(); end, "Create a quest with a new ID (Ctrl+N)");
	new:SetPoint("TOPRIGHT", root, "TOPRIGHT", -8, -6);

	local searchButton = Common.Button(root, "Reload", 64, function()
		GameEditor_ReloadTemplateList("quest");
		Q.RefreshBrowser(true);
	end, "Ask the server for the quest list again");
	searchButton:SetPoint("TOPRIGHT", root, "TOPRIGHT", -8, -34);

	searchBox = NoggitLineEdit_Create(root, 200);
	searchBox:SetHeight(22);
	searchBox:SetPoint("TOPLEFT", root, "TOPLEFT", 8, -34);
	searchBox:SetPoint("RIGHT", searchButton, "LEFT", -4, 0);
	searchBox:SetAutoFocus(false);

	local hint = Common.Label(searchBox, "Title or quest ID", nil, NOGGIT_COLOR_DISABLED);
	hint:SetPoint("LEFT", searchBox, "LEFT", 6, 0);

	searchBox:SetScript("OnEditFocusGained", function() hint:Hide(); end);
	searchBox:SetScript("OnEditFocusLost", function(self)
		if ( self:GetText() == "" ) then
			hint:Show();
		end
	end);
	searchBox:SetScript("OnEnterPressed", function(self)
		self:ClearFocus();
		local text = strtrim(self:GetText() or "");
		local number = tonumber(text);

		if ( number ) then
			Q.OpenQuest(number);
		end
	end);
	searchBox:SetScript("OnEscapePressed", function(self)
		self:SetText("");
		self:ClearFocus();
	end);
	searchBox:SetScript("OnTextChanged", function()
		GameEditor_Debounce(FilterBrowser, #GameEditor_TemplateList("quest"));
	end);

	status = Common.Label(root, "", nil, NOGGIT_COLOR_DISABLED);
	status:SetPoint("BOTTOMLEFT", root, "BOTTOMLEFT", 10, 8);
	status:SetPoint("BOTTOMRIGHT", root, "BOTTOMRIGHT", -10, 8);

	list = NoggitListBox_Create(root, 20, function(row)
		row.entry = NoggitUI_FontString(row, "");
		row.entry:SetPoint("LEFT", row, "LEFT", 4, 0);
		row.entry:SetWidth(48);
		NoggitText_SetColor(row.entry, NOGGIT_COLOR_DISABLED);
		row.level = NoggitUI_FontString(row, "", nil, "RIGHT");
		row.level:SetPoint("RIGHT", row, "RIGHT", -4, 0);
		row.level:SetWidth(46);
		NoggitText_SetColor(row.level, NOGGIT_COLOR_DISABLED);
		row.name = NoggitUI_FontString(row, "");
		row.name:SetPoint("LEFT", row, "LEFT", 54, 0);
		row.name:SetPoint("RIGHT", row.level, "LEFT", -4, 0);
	end, function(row, item)
		local name = item.row and item.row.name or Q.NameOf(item.value) or item.text;
		local level = LevelText(Q.MetaOf(item.value));
		row.entry:SetText(tostring(item.value));
		row.name:SetText(name and name ~= "" and name or "|cff7f7f7ftitle unknown|r");
		NoggitText_SetColor(row.name, item.value == Q.id and NOGGIT_COLOR_GOLD or NOGGIT_COLOR_TEXT);

		if ( level ~= "" ) then
			row.level:SetText("["..level.."]");
		else
			row.level:SetText(item.tag == "recent" and "recent" or "");
		end
	end);
	list:SetPoint("TOPLEFT", searchBox, "BOTTOMLEFT", 0, -6);
	list:SetPoint("RIGHT", root, "RIGHT", -8, 0);
	list:SetPoint("BOTTOM", status, "TOP", 0, 6);
	list.onSelect = function(item)
		Q.OpenQuest(item.value);
	end;

	Q.RefreshBrowser();
	return root;
end

Q.BuildMain = BuildMain;

Q.On("OPENED", function() Q.RefreshBrowser(true); end);

Q.On("ROWS", function(name)
	if ( name == "template" ) then
		Q.RefreshBrowser(true);
	end
end);

-- New quest window ---------------------------------------------------------------------------------------------

local newWindow;

local function SetNewStatus(text, color)
	newWindow.status:SetText(text or "");
	NoggitText_SetColor(newWindow.status, color or NOGGIT_COLOR_DISABLED);
end

local function Create(id)
	local copy = newWindow.copy and Q.Editor.Snapshot() or nil;
	local seed = copy or { template = { LogTitle = "New quest" } };
	local copiedFrom = Q.id;

	local function Run()
		GameEditor_HideWindow(newWindow);
		Q.Open(id, seed);
		GameEditor_Notify(copiedFrom and copy and format("Quest %d copied to %d, Save writes it", copiedFrom, id) or format("New quest %d, Save writes it", id), "info");
	end

	if ( copy ) then
		Run();
	else
		Q.Editor.ConfirmDiscard(Run);
	end
end

local function CheckAndCreate()
	local id = tonumber(strtrim(newWindow.idBox:GetText() or ""));

	if ( not id or id <= 0 or id > 4294967295 ) then
		SetNewStatus("Type a quest ID first", NOGGIT_COLOR_GOLD);
		return;
	end

	if ( newWindow.copy and not Q.Editor.Snapshot() ) then
		SetNewStatus("No quest is open to copy", NOGGIT_COLOR_GOLD);
		return;
	end

	SetNewStatus("Checking the ID on the server...");
	newWindow.checking = true;

	Q.FindFreeId(id, function(free)
		newWindow.checking = false;

		if ( free == id ) then
			Create(id);
		elseif ( free ) then
			newWindow.idBox:SetText(tostring(free));
			SetNewStatus(format("Quest %d already exists, %d is free. Press Create to use it.", id, free), NOGGIT_COLOR_GOLD);
		else
			SetNewStatus(format("Quest %d already exists and no free ID was found close to it", id), NOGGIT_COLOR_GOLD);
		end
	end);
end

local function FindFree()
	local start = tonumber(strtrim(newWindow.idBox:GetText() or "")) or (Q.maxSeen + 1);
	SetNewStatus("Looking for a free ID...");

	Q.FindFreeId(start, function(free)
		if ( free ) then
			newWindow.idBox:SetText(tostring(free));
			SetNewStatus(format("%d is free", free));
		else
			SetNewStatus("No free ID found close to it, type another one", NOGGIT_COLOR_GOLD);
		end
	end);
end

local function CreateNewWindow()
	newWindow = GameEditor_CreateWindow("GameEditorQuestNewWindow", "New quest", 380, 196);
	local content = newWindow.content;

	local label = Common.Label(content, "Quest ID");
	label:SetPoint("TOPLEFT", content, "TOPLEFT", 2, -4);

	newWindow.idBox = NoggitLineEdit_Create(content, 120);
	newWindow.idBox:SetHeight(22);
	newWindow.idBox:SetPoint("TOPLEFT", content, "TOPLEFT", 80, 0);
	newWindow.idBox:SetAutoFocus(false);
	newWindow.idBox:SetScript("OnEnterPressed", function(self)
		self:ClearFocus();
		CheckAndCreate();
	end);
	newWindow.idBox:SetScript("OnEscapePressed", function(self) self:ClearFocus(); end);

	local find = Common.Button(content, "Find free", 76, FindFree, "Walk up from the typed ID until the server has no quest with it");
	find:SetPoint("LEFT", newWindow.idBox, "RIGHT", 6, 0);

	local group = {};
	newWindow.empty = NoggitCheckBox_Create(content, "Start from an empty quest", true, function() newWindow.copy = false; end, group);
	newWindow.empty:SetPoint("TOPLEFT", content, "TOPLEFT", 0, -32);
	newWindow.copyBox = NoggitCheckBox_Create(content, "Copy the open quest", false, function() newWindow.copy = true; end, group);
	newWindow.copyBox:SetPoint("TOPLEFT", content, "TOPLEFT", 0, -56);

	local note = Common.Paragraph(content, "A copy takes the template, addon and texts, not the givers.");
	note:SetPoint("TOPLEFT", content, "TOPLEFT", 2, -82);
	note:SetPoint("TOPRIGHT", content, "TOPRIGHT", -2, -82);

	newWindow.status = Common.Label(content, "", nil, NOGGIT_COLOR_DISABLED);
	newWindow.status:SetPoint("BOTTOMLEFT", content, "BOTTOMLEFT", 2, 30);
	newWindow.status:SetPoint("BOTTOMRIGHT", content, "BOTTOMRIGHT", -2, 30);

	local cancel = Common.Button(content, "Cancel", 70, function() GameEditor_HideWindow(newWindow); end);
	cancel:SetPoint("BOTTOMRIGHT", content, "BOTTOMRIGHT", 0, 0);
	local create = Common.Button(content, "Create", 70, CheckAndCreate, "Open the new quest in the editor, nothing is written until you save");
	create:SetPoint("RIGHT", cancel, "LEFT", -4, 0);
end

-- Q.ShowNewDialog(copy) asks for the ID of a new quest, copy preselects copying the open quest.
function Q.ShowNewDialog(copy)
	if ( not newWindow ) then
		CreateNewWindow();
	end

	local canCopy = Q.Editor.Snapshot() ~= nil;
	newWindow.copy = copy and canCopy or false;
	Common.SetEnabled(newWindow.copyBox, canCopy);
	NoggitCheckBox_SetChecked(newWindow.empty, not newWindow.copy);
	NoggitCheckBox_SetChecked(newWindow.copyBox, newWindow.copy);
	newWindow:SetTitle(newWindow.copy and format("Copy quest %d", Q.id) or "New quest");

	newWindow.idBox:SetText(Q.maxSeen > 0 and tostring(Q.maxSeen + 1) or "");
	SetNewStatus(Q.maxSeen > 0 and "One above the highest quest ID seen so far, Create checks it" or "Type an ID, Find free walks up to an unused one");
	GameEditor_ShowWindow(newWindow);
	newWindow.idBox:SetFocus();
	newWindow.idBox:HighlightText();
end
