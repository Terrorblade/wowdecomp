-- Quest editor, the dock panel with the Template, Addon, Texts and Givers tabs of the open quest.

GameEditorQuest = GameEditorQuest or {};
local Q = GameEditorQuest;
local Common = GameEditorObject;

local Editor = {};
Q.Editor = Editor;

local TAB_NAMES = { "Template", "Addon", "Texts", "Givers" };
local TEMPLATE, ADDON, TEXTS, GIVERS = 1, 2, 3, 4;
local MAIL_COLUMN = "RewardMailSenderEntry";
local TEMPLATE_TEXT_COLUMNS = { "QuestDescription", "LogDescription", "QuestCompletionLog" };

local frame, body, empty, header, tabs;
local pages = {};
local RefreshHeader;

Q.HELP = table.concat({
	"- Search quests by title or ID in the list on the left, click one to open it.",
	"- Each tab saves its own tables, Ctrl+S saves every changed tab and F5 loads the quest again.",
	"- The Texts tab edits the long texts with line breaks, $B is a line break in quest texts.",
	"- Givers are the creatures and objects that start and end the quest, Selected adds what you clicked in the world.",
	"- Saving reloads the quests on the server and sends the new quest data to every player.",
}, "\n");

-- Multi line text box ---------------------------------------------------------------------------------------

local function CreateTextArea(parent, onChange)
	local box = CreateFrame("Frame", NoggitUI_NewName("GameEditorQuestTextArea"), parent, "NoggitBoxTemplate");
	NoggitBox_Set(box, NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID);
	box:EnableMouse(true);

	local scroll = CreateFrame("ScrollFrame", NoggitUI_NewName("GameEditorQuestTextScroll"), box);
	scroll:SetPoint("TOPLEFT", box, "TOPLEFT", 6, -5);
	scroll:SetPoint("BOTTOMRIGHT", box, "BOTTOMRIGHT", -6, 5);
	scroll:EnableMouseWheel(true);

	local edit = CreateFrame("EditBox", NoggitUI_NewName("GameEditorQuestTextEdit"), scroll);
	edit:SetMultiLine(true);
	edit:SetAutoFocus(false);
	edit:SetFontObject(NoggitFont);
	edit:SetTextColor(NOGGIT_COLOR_TEXT[1], NOGGIT_COLOR_TEXT[2], NOGGIT_COLOR_TEXT[3]);
	edit:SetWidth(200);
	edit:SetHeight(20);
	edit:SetMaxLetters(0);
	scroll:SetScrollChild(edit);

	local function Clamp(value)
		local range = scroll:GetVerticalScrollRange() or 0;
		scroll:SetVerticalScroll(max(0, min(range, value)));
	end

	scroll:SetScript("OnSizeChanged", function(self, width)
		edit:SetWidth(max(20, width or self:GetWidth()));
	end);
	scroll:SetScript("OnMouseWheel", function(self, delta)
		Clamp((self:GetVerticalScroll() or 0) - delta * 30);
	end);
	edit:SetScript("OnCursorChanged", function(_, _, y, _, height)
		local offset = scroll:GetVerticalScroll() or 0;
		local visible = scroll:GetHeight() or 0;
		y = -(y or 0);

		if ( y < offset ) then
			Clamp(y);
		elseif ( y + (height or 0) > offset + visible ) then
			Clamp(y + (height or 0) - visible);
		end
	end);
	-- SetText also runs OnTextChanged a frame later, only typing and pasting count as edits.
	edit:SetScript("OnTextChanged", function(self, userInput)
		scroll:UpdateScrollChildRect();

		if ( not userInput ) then
			return;
		end

		local text = self:GetText();

		if ( text == box.shownText ) then
			return;
		end

		box.shownText = text;

		if ( box.onChange ) then
			box.onChange(text);
		end
	end);
	edit:SetScript("OnEscapePressed", function(self) self:ClearFocus(); end);
	edit:SetScript("OnEditFocusGained", function() NoggitBox_Set(box, NOGGIT_COLOR_DARK, NOGGIT_COLOR_ACCENT); end);
	edit:SetScript("OnEditFocusLost", function() NoggitBox_Set(box, NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID); end);
	box:SetScript("OnMouseDown", function() edit:SetFocus(); end);

	box.edit = edit;
	box.onChange = onChange;
	box.SetValue = function(self, text)
		text = tostring(text or "");
		self.shownText = text;

		if ( edit:GetText() ~= text ) then
			edit:SetText(text);
		end
	end;
	return box;
end

-- Loading rows into pages -----------------------------------------------------------------------------------

-- Server values into a grid, a new row gets the seed values as edits so Save writes them.
local function LoadGrid(page, grid, name, values, seed)
	local keepEdits = page.questIds and page.questIds[name] == Q.id;
	Common.LoadGrid(grid, values, keepEdits);
	page.questIds = page.questIds or {};
	page.questIds[name] = Q.id;

	if ( seed ) then
		local key = Q.TABLES[name].key;

		for column, value in pairs(seed) do
			if ( column ~= key and grid.rowsByColumn[column] ) then
				grid:SetValue(column, value, true);
			end
		end
	end
end

local function UpdateDirty()
	if ( not frame or not tabs ) then
		return;
	end

	local names = {};

	for index = 1, #TAB_NAMES do
		local page = pages[index];
		local dirty = page and page:HasChanges() or false;
		tabs:SetDirty(index, dirty);

		if ( dirty ) then
			table.insert(names, TAB_NAMES[index]);
		end
	end

	if ( Q.IsSaving() ) then
		header.unsaved:SetText("Saving...");
	elseif ( #names > 0 ) then
		header.unsaved:SetText("Unsaved: "..table.concat(names, ", ").."   Ctrl+S saves them all");
	else
		header.unsaved:SetText("");
	end

	if ( pages[TEXTS] and pages[TEXTS].RefreshTexts ) then
		pages[TEXTS]:RefreshTexts();
	end

	if ( body:IsShown() and Q.IsLoaded("template") ) then
		RefreshHeader();
	end
end

-- Template and Addon pages ----------------------------------------------------------------------------------

local function BuildTemplatePage(container)
	local page;
	page = Common.CreateGridPage(container, {
		table = "quest_template",
		readOnly = { ID = true },
		onSave = function()
			Q.SaveRow("template", page.grid:GetValues());
		end,
		onDirty = UpdateDirty,
		saveTooltip = "Save quest_template, the server reloads its quests and sends the quest to every player (Ctrl+S saves every tab)",
	});

	page.LoadQuest = function(self)
		if ( Q.IsLoaded("template") ) then
			LoadGrid(self, self.grid, "template", Q.Row("template"), not Q.RowExists("template") and Q.TakeSeed("template"));
			self:UpdateState();
		end
	end;

	return page;
end

local function BuildAddonPage(container)
	local page;
	page = Common.CreateGridPage(container, {
		table = "quest_template_addon",
		readOnly = { ID = true },
		onSave = function(changes)
			local values = page.grid:GetValues();
			local addonChanged = false;

			for column in pairs(changes) do
				if ( column ~= MAIL_COLUMN ) then
					addonChanged = true;
				end
			end

			if ( addonChanged ) then
				Q.SaveRow("addon", values);
			end

			if ( changes[MAIL_COLUMN] ~= nil ) then
				if ( (tonumber(values[MAIL_COLUMN]) or 0) ~= 0 ) then
					Q.SaveRow("mail", values);
				elseif ( Q.RowExists("mail") ) then
					Q.DeleteRows("mail");
				end
			end
		end,
		onDirty = UpdateDirty,
		saveTooltip = "Save quest_template_addon and the mail sender in quest_mail_sender (Ctrl+S saves every tab)",
	});

	page.LoadQuest = function(self)
		if ( not Q.IsLoaded("addon") or not Q.IsLoaded("mail") ) then
			return;
		end

		local values = Q.Row("addon");
		values[MAIL_COLUMN] = tonumber(Q.Row("mail")[MAIL_COLUMN]) or 0;

		local seed = not Q.RowExists("addon") and Q.TakeSeed("addon") or nil;
		local mailSeed = not Q.RowExists("mail") and Q.TakeSeed("mail") or nil;

		if ( mailSeed ) then
			seed = seed or {};
			seed[MAIL_COLUMN] = mailSeed[MAIL_COLUMN];
		end

		LoadGrid(self, self.grid, "addon", values, seed);
		self:UpdateState();
	end;

	return page;
end

-- Texts page ------------------------------------------------------------------------------------------------

local TEXT_GRIDS = {
	{ name = "details", table = "quest_details" },
	{ name = "request", table = "quest_request_items", hidden = { CompletionText = true } },
	{ name = "offer", table = "quest_offer_reward", hidden = { RewardText = true } },
};

local function TemplateGrid()
	return pages[TEMPLATE] and pages[TEMPLATE].grid;
end

local function TemplateTextsChanged()
	local grid = TemplateGrid();

	if ( not grid ) then
		return false;
	end

	local changes = grid:GetChanges();

	for _, column in ipairs(TEMPLATE_TEXT_COLUMNS) do
		if ( changes[column] ~= nil ) then
			return true;
		end
	end

	return false;
end

local function BuildTextsPage(container)
	local page = CreateFrame("Frame", "GameEditorQuestTextsPage", container);
	page.grids = {};
	page.areas = {};
	page.subpages = {};

	local bar = CreateFrame("Frame", "GameEditorQuestTextsBar", page);
	bar:SetHeight(24);
	bar:SetPoint("BOTTOMLEFT", page, "BOTTOMLEFT", 0, 0);
	bar:SetPoint("BOTTOMRIGHT", page, "BOTTOMRIGHT", 0, 0);

	page.save = Common.Button(bar, "Save", 80, function() page:Save(true); end,
		"Save quest_details, quest_request_items and quest_offer_reward, the quest texts of quest_template are saved with them (Ctrl+S saves every tab)");
	page.save:SetPoint("RIGHT", bar, "RIGHT", 0, 0);
	page.revert = Common.Button(bar, "Revert", 70, function() page:Revert(); end, "Throw away the unsaved changes on this page");
	page.revert:SetPoint("RIGHT", page.save, "LEFT", -4, 0);
	page.status = Common.Label(bar, "", nil, NOGGIT_COLOR_DISABLED);
	page.status:SetPoint("LEFT", bar, "LEFT", 2, 0);
	page.status:SetPoint("RIGHT", page.revert, "LEFT", -4, 0);

	local holder = CreateFrame("Frame", "GameEditorQuestTextsHolder", page);
	holder:SetPoint("TOPLEFT", page, "TOPLEFT", 0, -28);
	holder:SetPoint("BOTTOMRIGHT", bar, "TOPRIGHT", 0, 4);

	local function ShowSubpage(index)
		for key, subpage in ipairs(page.subpages) do
			if ( key == index ) then
				subpage:Show();
			else
				subpage:Hide();
			end
		end
	end

	local subtabs = NoggitTabs_Create(page, { "Accept", "Progress", "Completion" }, 1, ShowSubpage);
	subtabs[1]:SetPoint("TOPLEFT", page, "TOPLEFT", 0, 0);

	for _, spec in ipairs(TEXT_GRIDS) do
		local subpage = CreateFrame("Frame", NoggitUI_NewName("GameEditorQuestTextsSub"), holder);
		subpage:SetAllPoints(holder);
		table.insert(page.subpages, subpage);

		page.grids[spec.name] = GameEditor_CreatePropertyGrid(subpage, {
			table = spec.table,
			hidden = spec.hidden,
			labelWidth = 150,
			filter = false,
			onChange = function()
				page:UpdateState();
			end,
		});
	end

	-- binding = { grid function, column }, the area writes into that grid so dirty tracking stays in one place.
	local function AddArea(subpage, title, tooltip, top, height, gridFunc, column)
		local label = Common.Label(subpage, title, "NoggitFontBold");
		label:SetPoint("TOPLEFT", subpage, "TOPLEFT", 2, top);

		local area = CreateTextArea(subpage, function(text)
			local grid = gridFunc();

			if ( grid ) then
				grid:SetValue(column, text);
			end
		end);
		area:SetPoint("TOPLEFT", subpage, "TOPLEFT", 0, top - 18);
		area:SetPoint("TOPRIGHT", subpage, "TOPRIGHT", 0, top - 18);
		area:SetHeight(height);
		area:SetScript("OnEnter", function(self) NoggitTooltip_Show(self, tooltip); end);
		area:SetScript("OnLeave", NoggitTooltip_Hide);
		area.gridFunc = gridFunc;
		area.column = column;
		table.insert(page.areas, area);
		return top - 18 - height - 8;
	end

	local function PlaceGrid(subpage, grid, top, title)
		local label = Common.Label(subpage, title, "NoggitFontBold");
		label:SetPoint("TOPLEFT", subpage, "TOPLEFT", 2, top);
		grid.frame:SetPoint("TOPLEFT", subpage, "TOPLEFT", 0, top - 18);
		grid.frame:SetPoint("BOTTOMRIGHT", subpage, "BOTTOMRIGHT", 0, 0);
	end

	local codes = "$B is a line break, $N the player name, $R the race, $C the class and $Gmale:female; picks by gender.";

	local accept = page.subpages[1];
	local y = AddArea(accept, "Quest text", "quest_template.QuestDescription, what the giver says when offering the quest. "..codes, 0, 150, TemplateGrid, "QuestDescription");
	y = AddArea(accept, "Objectives text", "quest_template.LogDescription, the objective summary in the quest log. "..codes, y, 60, TemplateGrid, "LogDescription");
	PlaceGrid(accept, page.grids.details, y, "Giver emotes (quest_details)");

	local progress = page.subpages[2];
	y = AddArea(progress, "Progress text", "quest_request_items.CompletionText, what the ender says while the quest is not turned in. "..codes, 0, 150, function() return page.grids.request; end, "CompletionText");
	PlaceGrid(progress, page.grids.request, y, "Ender emotes (quest_request_items)");

	local completion = page.subpages[3];
	y = AddArea(completion, "Reward text", "quest_offer_reward.RewardText, what the ender says when the quest is turned in. "..codes, 0, 150, function() return page.grids.offer; end, "RewardText");
	y = AddArea(completion, "Completion log text", "quest_template.QuestCompletionLog, the quest log line once the objectives are done.", y, 40, TemplateGrid, "QuestCompletionLog");
	PlaceGrid(completion, page.grids.offer, y, "Reward emotes (quest_offer_reward)");

	ShowSubpage(1);

	page.RefreshTexts = function(self)
		for _, area in ipairs(self.areas) do
			local grid = area.gridFunc();

			if ( grid and not area.edit:HasFocus() ) then
				area:SetValue(grid:GetValue(area.column));
			end
		end
	end;

	page.HasOwnChanges = function(self)
		for _, grid in pairs(self.grids) do
			if ( grid:HasChanges() ) then
				return true;
			end
		end

		return false;
	end;

	page.HasChanges = function(self)
		return self:HasOwnChanges() or TemplateTextsChanged();
	end;

	page.UpdateState = function(self)
		local count = 0;

		for _, grid in pairs(self.grids) do
			local _, changed = grid:GetChanges();
			count = count + changed;
		end

		if ( TemplateTextsChanged() ) then
			self.status:SetText(count > 0 and format("%d unsaved change%s and quest texts", count, count == 1 and "" or "s") or "Unsaved quest texts");
			NoggitText_SetColor(self.status, NOGGIT_COLOR_GOLD);
		elseif ( count > 0 ) then
			self.status:SetText(format("%d unsaved change%s", count, count == 1 and "" or "s"));
			NoggitText_SetColor(self.status, NOGGIT_COLOR_GOLD);
		else
			self.status:SetText("No changes");
			NoggitText_SetColor(self.status, NOGGIT_COLOR_DISABLED);
		end

		local dirty = self:HasChanges();
		Common.SetEnabled(self.save, dirty);
		Common.SetEnabled(self.revert, dirty);
		UpdateDirty();
	end;

	-- withTemplate also saves quest_template when its texts changed, Save all saves the template on its own.
	page.Save = function(self, withTemplate)
		local saved = false;

		if ( withTemplate and TemplateTextsChanged() and pages[TEMPLATE]:Save() ) then
			saved = true;
		end

		for _, spec in ipairs(TEXT_GRIDS) do
			local grid = self.grids[spec.name];

			if ( grid:HasChanges() and Q.SaveRow(spec.name, grid:GetValues()) ) then
				saved = true;
			end
		end

		return saved;
	end;

	page.Revert = function(self)
		for _, grid in pairs(self.grids) do
			grid:Revert();
		end

		local grid = TemplateGrid();

		if ( grid ) then
			for _, column in ipairs(TEMPLATE_TEXT_COLUMNS) do
				grid:SetValue(column, grid.original[column], true);
			end

			pages[TEMPLATE]:UpdateState();
		end

		self:RefreshTexts();
		self:UpdateState();
	end;

	page.LoadQuest = function(self, name)
		for _, spec in ipairs(TEXT_GRIDS) do
			if ( (not name or name == spec.name) and Q.IsLoaded(spec.name) ) then
				LoadGrid(self, self.grids[spec.name], spec.name, Q.Row(spec.name), not Q.RowExists(spec.name) and Q.TakeSeed(spec.name));
			end
		end

		self:RefreshTexts();
		self:UpdateState();
	end;

	page:SetScript("OnShow", function(self) self:RefreshTexts(); end);
	return page;
end

-- Givers page -----------------------------------------------------------------------------------------------

local GIVERS = {
	{ name = "creatureStart", title = "Creatures that start it", tooltip = "creature_queststarter, the creature needs the quest giver npc flag" },
	{ name = "creatureEnd", title = "Creatures that end it", tooltip = "creature_questender, the creature needs the quest giver npc flag" },
	{ name = "objectStart", title = "Objects that start it", tooltip = "gameobject_queststarter, the object has to be a quest giver type" },
	{ name = "objectEnd", title = "Objects that end it", tooltip = "gameobject_questender, the object has to be a quest giver type" },
	{ name = "trigger", title = "Area triggers", tooltip = "areatrigger_involvedrelation, walking into the trigger completes the exploration objective. A trigger belongs to one quest only" },
};

local function SameIds(a, b)
	if ( #a ~= #b ) then
		return false;
	end

	local set = {};

	for _, id in ipairs(a) do
		set[id] = (set[id] or 0) + 1;
	end

	for _, id in ipairs(b) do
		if ( not set[id] or set[id] == 0 ) then
			return false;
		end

		set[id] = set[id] - 1;
	end

	return true;
end

local function GiverName(kind, id)
	local name = GameEditor_ResolveRef(kind, id);

	if ( not name and (kind == "creature" or kind == "gameobject") ) then
		Q.QueueLookup(kind, id);
	end

	return name;
end

local function CreateGiverList(parent, spec, onChange)
	local info = Q.TABLES[spec.name];
	local block = CreateFrame("Frame", NoggitUI_NewName("GameEditorQuestGivers"), parent);
	block.name = spec.name;
	block.kind = info.kind;
	block.ids = {};
	block.snapshot = {};

	local title = Common.Label(block, spec.title, "NoggitFontBold");
	title:SetPoint("TOPLEFT", block, "TOPLEFT", 2, 0);
	title:SetPoint("TOPRIGHT", block, "TOPRIGHT", -2, 0);
	title:SetHeight(18);
	local hover = CreateFrame("Frame", nil, block);
	hover:SetAllPoints(title);
	hover:EnableMouse(true);
	Common.Tooltip(hover, spec.tooltip);

	local function Changed()
		block:Refresh();
		onChange();
	end

	local function Add(id)
		id = tonumber(id);

		if ( not id or id <= 0 ) then
			return;
		end

		for _, existing in ipairs(block.ids) do
			if ( existing == id ) then
				GameEditor_Notify(format("%d is already in the list", id), "info");
				return;
			end
		end

		table.insert(block.ids, id);
		Changed();
	end

	local add = Common.Button(block, "Add...", 54, function()
		if ( not Q.id ) then
			return;
		end

		GameEditor_PickRef(spec.title, block.kind, nil, function(value) Add(value); end, { anyNumber = true });
	end, "Pick one by name or type its entry");
	add:SetHeight(20);
	add:SetPoint("BOTTOMLEFT", block, "BOTTOMLEFT", 0, 0);

	local previous = add;

	if ( block.kind == "creature" or block.kind == "gameobject" ) then
		local selected = Common.Button(block, "Selected", 66, function()
			local kind, _, entry, name = GameEditor_GetSelection();

			if ( kind ~= block.kind or not entry or entry == 0 ) then
				GameEditor_Notify(format("Click a %s in the world first", block.kind), "info");
				return;
			end

			Common.RememberName(kind, entry, name);
			Add(entry);
		end, "Add the "..block.kind.." selected in the world");
		selected:SetHeight(20);
		selected:SetPoint("LEFT", add, "RIGHT", 4, 0);
		previous = selected;
	end

	local remove = Common.Button(block, "Remove", 62, function()
		local index = block.list.selected;

		if ( index and block.ids[index] ) then
			table.remove(block.ids, index);
			block.list.selected = nil;
			Changed();
		end
	end, "Remove the selected row, Save writes it");
	remove:SetHeight(20);
	remove:SetPoint("LEFT", previous, "RIGHT", 4, 0);

	block.list = NoggitListBox_Create(block, 18, function(row)
		row.entry = NoggitUI_FontString(row, "");
		row.entry:SetPoint("LEFT", row, "LEFT", 4, 0);
		row.entry:SetWidth(56);
		NoggitText_SetColor(row.entry, NOGGIT_COLOR_DISABLED);
		row.name = NoggitUI_FontString(row, "");
		row.name:SetPoint("LEFT", row, "LEFT", 62, 0);
		row.name:SetPoint("RIGHT", row, "RIGHT", -4, 0);
	end, function(row, item)
		row.entry:SetText(tostring(item.value));
		row.name:SetText(item.text ~= "" and item.text or "|cff7f7f7fname unknown|r");
		NoggitText_SetColor(row.name, item.added and NOGGIT_COLOR_GOLD or NOGGIT_COLOR_TEXT);
	end);
	block.list:SetPoint("TOPLEFT", title, "BOTTOMLEFT", -2, -2);
	block.list:SetPoint("BOTTOMRIGHT", block, "BOTTOMRIGHT", 0, 24);
	block.list.onDoubleClick = function(item)
		if ( block.kind == "creature" or block.kind == "gameobject" ) then
			GameEditor_Notify(format("%s %d: %s", block.kind, item.value, item.text ~= "" and item.text or "name unknown"), "info");
		end
	end;

	block.emptyText = Common.Label(block.list, "None", nil, NOGGIT_COLOR_DISABLED);
	block.emptyText:SetPoint("CENTER", block.list, "CENTER", 0, 0);

	block.Refresh = function(self)
		local inSnapshot = {};

		for _, id in ipairs(self.snapshot) do
			inSnapshot[id] = true;
		end

		local items = {};

		for _, id in ipairs(self.ids) do
			table.insert(items, { value = id, text = GiverName(self.kind, id) or "", added = not inSnapshot[id] });
		end

		NoggitListBox_SetItems(self.list, items, true);

		if ( #items == 0 ) then
			self.emptyText:Show();
		else
			self.emptyText:Hide();
		end
	end;

	block.HasChanges = function(self)
		return not SameIds(self.ids, self.snapshot);
	end;

	-- Unsaved additions and removals survive a reload, a clean list follows the server.
	block.Load = function(self, sameQuest)
		local fresh = Q.ListIds(self.name);

		if ( not sameQuest or not self:HasChanges() ) then
			self.ids = Q.Copy(fresh);
		end

		self.snapshot = fresh;
		self:Refresh();
	end;

	block.Revert = function(self)
		self.ids = Q.Copy(self.snapshot);
		self:Refresh();
	end;

	block.Save = function(self)
		if ( not self:HasChanges() ) then
			return false;
		end

		return Q.SaveList(self.name, self.ids);
	end;

	return block;
end

local function BuildGiversPage(container)
	local page = CreateFrame("Frame", "GameEditorQuestGiversPage", container);
	page.blocks = {};

	local bar = CreateFrame("Frame", "GameEditorQuestGiversBar", page);
	bar:SetHeight(24);
	bar:SetPoint("BOTTOMLEFT", page, "BOTTOMLEFT", 0, 0);
	bar:SetPoint("BOTTOMRIGHT", page, "BOTTOMRIGHT", 0, 0);

	page.save = Common.Button(bar, "Save", 80, function() page:Save(); end, "Replace the changed giver lists on the server (Ctrl+S saves every tab)");
	page.save:SetPoint("RIGHT", bar, "RIGHT", 0, 0);
	page.revert = Common.Button(bar, "Revert", 70, function() page:Revert(); end, "Throw away the unsaved changes on this page");
	page.revert:SetPoint("RIGHT", page.save, "LEFT", -4, 0);
	page.status = Common.Label(bar, "", nil, NOGGIT_COLOR_DISABLED);
	page.status:SetPoint("LEFT", bar, "LEFT", 2, 0);
	page.status:SetPoint("RIGHT", page.revert, "LEFT", -4, 0);

	local area = CreateFrame("Frame", "GameEditorQuestGiversArea", page);
	area:SetPoint("TOPLEFT", page, "TOPLEFT", 0, 0);
	area:SetPoint("BOTTOMRIGHT", bar, "TOPRIGHT", 0, 6);

	for _, spec in ipairs(GIVERS) do
		table.insert(page.blocks, CreateGiverList(area, spec, function() page:UpdateState(); end));
	end

	page.note = Common.Paragraph(area, "Creatures need the quest giver flag in npcflag and objects the quest giver type to show the quest. New givers show up after the creature or object respawns.");

	local function Layout()
		local width = area:GetWidth() or 0;
		local height = area:GetHeight() or 0;

		if ( width < 10 or height < 10 ) then
			return;
		end

		local columnWidth = floor((width - 8) / 2);
		local rowHeight = floor((height - 16) / 3);

		for index, block in ipairs(page.blocks) do
			local column = (index - 1) % 2;
			local row = floor((index - 1) / 2);
			block:ClearAllPoints();
			block:SetPoint("TOPLEFT", area, "TOPLEFT", column * (columnWidth + 8), -row * (rowHeight + 8));
			block:SetWidth(columnWidth);
			block:SetHeight(rowHeight);
		end

		page.note:ClearAllPoints();
		page.note:SetPoint("TOPLEFT", area, "TOPLEFT", columnWidth + 12, -2 * (rowHeight + 8) - 4);
		page.note:SetPoint("TOPRIGHT", area, "TOPRIGHT", -4, -2 * (rowHeight + 8) - 4);
	end

	area:SetScript("OnSizeChanged", Layout);

	page.HasChanges = function(self)
		for _, block in ipairs(self.blocks) do
			if ( block:HasChanges() ) then
				return true;
			end
		end

		return false;
	end;

	page.UpdateState = function(self)
		local changed = {};

		for index, block in ipairs(self.blocks) do
			if ( block:HasChanges() ) then
				table.insert(changed, GIVERS[index].title);
			end
		end

		if ( #changed > 0 ) then
			self.status:SetText("Unsaved: "..table.concat(changed, ", "));
			NoggitText_SetColor(self.status, NOGGIT_COLOR_GOLD);
		else
			self.status:SetText("No changes");
			NoggitText_SetColor(self.status, NOGGIT_COLOR_DISABLED);
		end

		Common.SetEnabled(self.save, #changed > 0);
		Common.SetEnabled(self.revert, #changed > 0);
		UpdateDirty();
	end;

	page.Save = function(self)
		local saved = false;

		for _, block in ipairs(self.blocks) do
			if ( block:Save() ) then
				saved = true;
			end
		end

		return saved;
	end;

	page.Revert = function(self)
		for _, block in ipairs(self.blocks) do
			block:Revert();
		end

		self:UpdateState();
	end;

	page.LoadQuest = function(self, name)
		self.questIds = self.questIds or {};

		for _, block in ipairs(self.blocks) do
			if ( (not name or name == block.name) and Q.IsLoaded(block.name) ) then
				block:Load(self.questIds[block.name] == Q.id);
				self.questIds[block.name] = Q.id;
			end
		end

		self:UpdateState();
	end;

	page.RefreshNames = function(self)
		for _, block in ipairs(self.blocks) do
			block:Refresh();
		end
	end;

	page:SetScript("OnShow", Layout);
	return page;
end

-- Panel ------------------------------------------------------------------------------------------------------

local function ShowPage(index)
	for key, page in pairs(pages) do
		if ( key == index ) then
			page:Show();
		else
			page:Hide();
		end
	end
end

RefreshHeader = function()
	local id = Q.id;
	local row = Q.IsLoaded("template") and Q.Row("template") or {};
	local values = pages[TEMPLATE] and pages[TEMPLATE].grid:GetValues() or row;
	local title = tostring(values.LogTitle or "");

	header.name:SetText(title ~= "" and title or "(no title)");

	local parts = { format("ID %d", id or 0), format("Level %s", tostring(values.QuestLevel or "?")) };

	if ( (tonumber(values.MinLevel) or 0) > 0 ) then
		table.insert(parts, format("min %s", tostring(values.MinLevel)));
	end

	local info = tonumber(values.QuestInfoID) or 0;

	if ( info ~= 0 ) then
		table.insert(parts, Common.EnumText("QuestInfo", info));
	end

	header.info:SetText(table.concat(parts, "    "));

	local state;

	if ( Q.IsNew() ) then
		state = "|cffffcc00New quest, Save on the Template tab creates it|r";
	elseif ( Q.IsLoading() ) then
		state = "Loading...";
	else
		state = Q.SortText(values.QuestSortID);
	end

	header.position:SetText(state);
end

local function SetEmpty(state)
	body:Hide();
	empty:Set(state);
end

function Editor.Refresh()
	if ( not frame ) then
		return;
	end

	if ( not Q.id ) then
		SetEmpty({
			title = "No quest open",
			text = "Search a quest by title or ID in the list on the left and click it, or start a new quest.",
			primary = "New quest...", onPrimary = function() Q.ShowNewDialog(); end,
			secondary = "Find a quest", onSecondary = function() Q.FocusSearch(); end,
			help = Q.HELP,
		});
		return;
	end

	local state = Q.state.template;

	if ( not state.loaded ) then
		SetEmpty({
			title = format("Loading quest %d...", Q.id),
			text = state.error and ("The server answered: "..state.error) or "Waiting for the server. Editing needs GM level 3 on the server.",
			primary = "Ask again", onPrimary = function() Q.Reload(); end,
			help = Q.HELP,
		});
		return;
	end

	empty:Hide();
	body:Show();
	RefreshHeader();
	UpdateDirty();
end

local function Build(parent)
	frame = CreateFrame("Frame", "GameEditorQuestEditor", parent);

	empty = Common.CreateEmptyState(frame);

	body = CreateFrame("Frame", "GameEditorQuestEditorBody", frame);
	body:SetAllPoints(frame);

	header = Common.CreateHeader(body);
	header:AddButton("Save all", 64, function() Editor.SaveAll(); end, "Save every changed tab (Ctrl+S)");
	header:AddButton("Reload", 56, function() Q.Reload(); end, "Load the quest again from the server, unsaved edits stay (F5)");
	header:AddButton("Copy", 50, function() Q.ShowNewDialog(true); end, "Copy this quest to a new ID (Ctrl+D)");

	local container = CreateFrame("Frame", "GameEditorQuestEditorPages", body);
	container:SetPoint("TOPLEFT", header, "BOTTOMLEFT", 6, -30);
	container:SetPoint("BOTTOMRIGHT", body, "BOTTOMRIGHT", -6, 6);

	tabs = Common.CreateTabs(body, TAB_NAMES, function(index)
		ShowPage(index);
	end);
	tabs.tabs[1]:SetPoint("TOPLEFT", header, "BOTTOMLEFT", 6, -4);

	pages[TEMPLATE] = BuildTemplatePage(container);
	pages[ADDON] = BuildAddonPage(container);
	pages[TEXTS] = BuildTextsPage(container);
	pages[GIVERS] = BuildGiversPage(container);

	for _, page in pairs(pages) do
		page:SetAllPoints(container);
	end

	ShowPage(tabs.current);

	for _, page in pairs(pages) do
		page:LoadQuest();
	end

	frame:SetScript("OnShow", function() Editor.Refresh(); end);
	Editor.Refresh();
	return frame;
end

Editor.Build = Common.SharedBuilder(Build);

function Editor.HasChanges()
	for _, page in pairs(pages) do
		if ( page:HasChanges() ) then
			return true;
		end
	end

	return false;
end

function Editor.SaveAll()
	if ( not Q.id ) then
		GameEditor_Notify("Open a quest first", "info");
		return;
	end

	local saved = 0;

	if ( pages[TEMPLATE] and pages[TEMPLATE]:HasChanges() and pages[TEMPLATE]:Save() ) then
		saved = saved + 1;
	end

	if ( pages[ADDON] and pages[ADDON]:HasChanges() and pages[ADDON]:Save() ) then
		saved = saved + 1;
	end

	if ( pages[TEXTS] and pages[TEXTS]:HasOwnChanges() and pages[TEXTS]:Save(false) ) then
		saved = saved + 1;
	end

	if ( pages[GIVERS] and pages[GIVERS]:HasChanges() and pages[GIVERS]:Save() ) then
		saved = saved + 1;
	end

	if ( saved == 0 ) then
		GameEditor_Notify("Nothing to save", "info");
	end
end

function Editor.RevertAll()
	for _, page in pairs(pages) do
		page:Revert();
	end

	UpdateDirty();
end

function Editor.SelectTab(index)
	if ( tabs ) then
		tabs:Select(index);
	end
end

-- Values of the open quest per table, a new quest copies them.
function Editor.Snapshot()
	if ( not frame or not Q.id or not Q.IsLoaded("template") ) then
		return nil;
	end

	local snapshot = { template = Q.Copy(pages[TEMPLATE].grid:GetValues()) };
	local addon = Q.Copy(pages[ADDON].grid:GetValues());
	snapshot.mail = { [MAIL_COLUMN] = addon[MAIL_COLUMN] };
	addon[MAIL_COLUMN] = nil;
	snapshot.addon = addon;

	for _, spec in ipairs(TEXT_GRIDS) do
		snapshot[spec.name] = Q.Copy(pages[TEXTS].grids[spec.name]:GetValues());
	end

	return snapshot;
end

-- Runs action right away, or after asking when the open quest has unsaved changes.
function Editor.ConfirmDiscard(action)
	if ( not Editor.HasChanges() ) then
		action();
		return;
	end

	NoggitMessageBox_Show("Unsaved changes", format("Quest %d has unsaved changes. Drop them?", Q.id or 0), {
		{ text = "Drop changes", onClick = action },
		{ text = "Cancel" },
	});
end

-- Event hooks ----------------------------------------------------------------------------------------------------

local PAGE_OF = {
	template = TEMPLATE, addon = ADDON, mail = ADDON,
	details = TEXTS, request = TEXTS, offer = TEXTS,
	creatureStart = GIVERS, creatureEnd = GIVERS, objectStart = GIVERS, objectEnd = GIVERS, trigger = GIVERS,
};

Q.On("ROWS", function(name)
	if ( not frame ) then
		return;
	end

	local page = pages[PAGE_OF[name]];

	if ( page ) then
		page:LoadQuest(name);
	end

	if ( name == "template" ) then
		pages[TEXTS]:RefreshTexts();
	end

	Editor.Refresh();
end);

Q.On("OPENED", function()
	if ( frame ) then
		Editor.Refresh();
	end
end);

Q.On("STATE", function()
	if ( frame and body:IsShown() ) then
		RefreshHeader();
		UpdateDirty();
	end
end);

Q.On("NAMES", function()
	if ( not frame or not body:IsVisible() ) then
		return;
	end

	if ( pages[GIVERS]:IsShown() ) then
		pages[GIVERS]:RefreshNames();
	end

	for _, index in ipairs({ TEMPLATE, ADDON }) do
		if ( pages[index]:IsShown() ) then
			pages[index].grid:Refresh();
		end
	end
end);
