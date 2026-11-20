-- Item editor, the dock panel with the item_template tabs and the locale texts of the open item.

GameEditorItem = GameEditorItem or {};
local I = GameEditorItem;
local Common = GameEditorObject;
local Data = GameEditorData or {};

local Editor = {};
I.Editor = Editor;

local TABS = Data.ItemTemplateTabs or {};
local TAB_NAMES = {};

for index, tab in ipairs(TABS) do
	TAB_NAMES[index] = tab.title;
end

table.insert(TAB_NAMES, "Locale");
local LOCALE = #TAB_NAMES;

local LOCALES = { "koKR", "frFR", "deDE", "zhCN", "zhTW", "esES", "esMX", "ruRU" };

local frame, body, empty, header, tabs, icon;
local pages = {};
local localeEditor;
local loadedEntry;
local RefreshHeader;

I.HELP = table.concat({
	"- Search items by name or entry in the list on the left, click one to open it.",
	"- Every tab edits the same item_template row, Save on any tab or Ctrl+S writes all of them.",
	"- Saving reloads just this item on the server and sends it to every player, the tooltip updates on its own.",
	"- The Locale tab holds the translated names, it saves on its own.",
	"- The Generator tool on the left edits the item_generator tables of the random item generator.",
}, "\n");

-- Subclass names follow the class of the row.
local subclassField = I.Copy(Data.Fields and Data.Fields.item_template and Data.Fields.item_template.subclass or { label = "Subclass", type = "int" });
local templateFields = { subclass = subclassField };

local function FollowClass(class)
	subclassField.enum = I.SubClassEnum(class);
end

-- Template pages ------------------------------------------------------------------------------------------------

local function TemplateChanges()
	local changes = {};
	local count = 0;

	for index = 1, #TABS do
		local page = pages[index];

		if ( page ) then
			local pageChanges, pageCount = page.grid:GetChanges();

			for column, value in pairs(pageChanges) do
				changes[column] = value;
			end

			count = count + pageCount;
		end
	end

	return changes, count;
end

-- Values of every tab on top of the server row.
local function CurrentValues()
	local values = I.Row();

	for index = 1, #TABS do
		local page = pages[index];

		if ( page ) then
			for column, value in pairs(page.grid:GetValues()) do
				if ( page.grid.rowsByColumn[column] ) then
					values[column] = value;
				end
			end
		end
	end

	return values;
end

local function SaveTemplate()
	local _, count = TemplateChanges();

	if ( count == 0 and not I.IsNew() ) then
		return false;
	end

	return I.SaveTemplate(CurrentValues());
end

local function UpdateDirty()
	if ( not frame or not tabs ) then
		return;
	end

	local names = {};

	for index = 1, #TAB_NAMES do
		local dirty;

		if ( index == LOCALE ) then
			dirty = localeEditor and localeEditor:HasChanges() or false;
		else
			dirty = pages[index] and pages[index]:HasChanges() or false;
		end

		tabs:SetDirty(index, dirty);

		if ( dirty ) then
			table.insert(names, TAB_NAMES[index]);
		end
	end

	if ( I.IsSaving() ) then
		header.unsaved:SetText("Saving...");
	elseif ( #names > 0 ) then
		header.unsaved:SetText("Unsaved: "..table.concat(names, ", ").."   Ctrl+S saves them all");
	elseif ( I.IsNew() ) then
		header.unsaved:SetText("New item, Save writes it");
	else
		header.unsaved:SetText("");
	end

	if ( body:IsShown() and I.IsLoaded() ) then
		RefreshHeader();
	end
end

local function BuildTemplatePage(container, index)
	local tab = TABS[index];
	local page = Common.CreateGridPage(container, {
		table = I.TEMPLATE,
		groups = tab.groups,
		fields = templateFields,
		readOnly = { entry = true },
		onSave = function() SaveTemplate(); end,
		onDirty = UpdateDirty,
		saveTooltip = "Save the item, every tab writes together. The server reloads this item and sends it to every player (Ctrl+S)",
	});

	local baseChange = page.grid.onChange;

	page.grid.onChange = function(grid, column, value, previous)
		if ( column == "class" ) then
			FollowClass(value);
			grid:Refresh();
		end

		baseChange(grid, column, value, previous);

		-- A field shown on two tabs stays the same on both.
		for other = 1, #TABS do
			local otherPage = pages[other];

			if ( otherPage and otherPage ~= page and otherPage.grid.rowsByColumn[column] ) then
				otherPage.grid:SetValue(column, value, true);

				if ( column == "class" ) then
					otherPage.grid:Refresh();
				end

				otherPage:UpdateState();
			end
		end
	end;

	-- Save stays on for a new item even before an edit.
	local baseUpdate = page.UpdateState;

	page.UpdateState = function(self)
		baseUpdate(self);

		if ( I.IsNew() and self.available ) then
			Common.SetEnabled(self.save, true);
			self.status:SetText("New item, Save writes it");
			NoggitText_SetColor(self.status, NOGGIT_COLOR_GOLD);
		end
	end;

	page.LoadItem = function(self, values, keepEdits, seed)
		Common.LoadGrid(self.grid, values, keepEdits);

		for column, value in pairs(seed or {}) do
			if ( column ~= "entry" and self.grid.rowsByColumn[column] ) then
				self.grid:SetValue(column, value, true);
			end
		end

		self:UpdateState();
	end;

	return page;
end

local function LoadTemplatePages()
	if ( not I.IsLoaded() ) then
		return;
	end

	local values = I.Row();
	local keepEdits = loadedEntry == I.entry;
	local seed = not I.RowExists() and I.TakeSeed() or nil;
	loadedEntry = I.entry;

	FollowClass(seed and seed.class or values.class);

	for index = 1, #TABS do
		pages[index]:LoadItem(values, keepEdits, seed);
	end

	UpdateDirty();
end

-- Locale page ---------------------------------------------------------------------------------------------------

local function BuildLocalePage(container)
	local page = CreateFrame("Frame", "GameEditorItemLocalePage", container);

	localeEditor = I.CreateRowEditor(page, {
		table = I.LOCALE,
		title = "Translations",
		groups = Data.Groups.item_template_locale,
		readOnly = { ID = true },
		listWidth = 170,
		labelWidth = 120,
		unique = { { "locale" } },
		summary = function(values)
			local locale = tostring(values.locale or "");
			local name = tostring(values.Name or "");
			return format("%s  %s", locale ~= "" and locale or "?", name);
		end,
		sort = function(a, b) return tostring(a.locale) < tostring(b.locale); end,
		newValues = function(editor)
			local used = {};

			for _, wrapper in ipairs(editor.wrappers) do
				used[tostring(wrapper.values.locale)] = true;
			end

			for _, locale in ipairs(LOCALES) do
				if ( not used[locale] ) then
					return { locale = locale, Name = I.NameOf(I.entry) or "" };
				end
			end

			return { locale = "" };
		end,
		validate = function(editor)
			for _, wrapper in ipairs(editor.wrappers) do
				local locale = tostring(wrapper.values.locale or "");
				local known = false;

				for _, name in ipairs(LOCALES) do
					known = known or name == locale;
				end

				if ( not known ) then
					return format("Locale %s is not one of %s", locale ~= "" and locale or "(empty)", table.concat(LOCALES, ", "));
				end
			end
		end,
		onDirty = function() UpdateDirty(); end,
	});
	localeEditor:SetAllPoints(page);

	page.HasChanges = function() return localeEditor:HasChanges(); end;
	page.Save = function() return localeEditor:Save(); end;
	page.Revert = function() localeEditor:Revert(); end;

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
	local values = pages[1] and pages[1].grid:GetValues() or I.Row();
	local name = tostring(values.name or "");
	local quality = tonumber(values.Quality) or 1;

	header.name:SetText(I.Colored(name ~= "" and name or "(no name)", quality));

	local parts = { format("Entry %d", I.entry or 0), format("ilvl %s", tostring(values.ItemLevel or "?")) };

	if ( (tonumber(values.RequiredLevel) or 0) > 0 ) then
		table.insert(parts, format("req %s", tostring(values.RequiredLevel)));
	end

	table.insert(parts, I.KindText(values.class, values.subclass, values.InventoryType));
	header.info:SetText(table.concat(parts, "    "));

	local state;

	if ( I.IsNew() ) then
		state = "|cffffcc00New item, nothing is on the server yet|r";
	elseif ( I.IsLoading() ) then
		state = "Loading...";
	else
		state = format("Display %d    %s", tonumber(values.displayid) or 0, Common.EnumText("ItemBonding", values.bonding));
	end

	header.position:SetText(state);

	local texture = I.entry and GetItemIcon and GetItemIcon(I.entry);
	icon.texture:SetTexture(texture or "Interface\\Icons\\INV_Misc_QuestionMark");
end

local function SetEmpty(state)
	body:Hide();
	empty:Set(state);
end

function Editor.Refresh()
	if ( not frame ) then
		return;
	end

	if ( not I.entry ) then
		SetEmpty({
			title = "No item open",
			text = "Search an item by name or entry in the list on the left and click it, or start a new item.",
			primary = "New item...", onPrimary = function() I.ShowNewDialog(); end,
			secondary = "Find an item", onSecondary = function() I.FocusSearch(); end,
			third = "Item generator", onThird = function() GameEditor_SelectTool("generator"); end,
			help = I.HELP,
		});
		return;
	end

	local state = I.template;

	if ( not state.loaded ) then
		SetEmpty({
			title = format("Loading item %d...", I.entry),
			text = state.error and ("The server answered: "..state.error) or "Waiting for the server. Editing needs GM level 3 on the server.",
			primary = "Ask again", onPrimary = function() I.Reload(); end,
			help = I.HELP,
		});
		return;
	end

	empty:Hide();
	body:Show();
	RefreshHeader();
	UpdateDirty();
end

local function CreateIcon(parent)
	local button = CreateFrame("Button", "GameEditorItemEditorIcon", parent);
	button:SetWidth(34);
	button:SetHeight(34);
	button:SetPoint("TOPLEFT", parent, "TOPLEFT", 8, -8);
	button.texture = button:CreateTexture(nil, "ARTWORK");
	button.texture:SetAllPoints(button);
	button:SetScript("OnEnter", function(self)
		if ( not I.entry ) then
			return;
		end

		GameTooltip:SetOwner(self, "ANCHOR_RIGHT");
		GameTooltip:SetHyperlink("item:"..I.entry);
		GameTooltip:Show();
	end);
	button:SetScript("OnLeave", function() GameTooltip:Hide(); end);
	return button;
end

local function Build(parent)
	frame = CreateFrame("Frame", "GameEditorItemEditor", parent);

	empty = Common.CreateEmptyState(frame);

	body = CreateFrame("Frame", "GameEditorItemEditorBody", frame);
	body:SetAllPoints(frame);

	header = Common.CreateHeader(body);
	header:AddButton("Save all", 64, function() Editor.SaveAll(); end, "Save every changed tab (Ctrl+S)");
	header:AddButton("Reload", 56, function() I.Reload(); end, "Load the item again from the server, unsaved edits stay (F5)");
	header:AddButton("Copy", 50, function() I.ShowNewDialog(true); end, "Copy this item to a new entry (Ctrl+D)");

	icon = CreateIcon(header);

	for _, label in ipairs({ header.name, header.info, header.position, header.unsaved }) do
		local point, relative, relativePoint, x, y = label:GetPoint(1);
		label:SetPoint(point, relative, relativePoint, x + 40, y);
	end

	local hover = CreateFrame("Frame", nil, header);
	hover:SetPoint("TOPLEFT", header.name, "TOPLEFT", 0, 0);
	hover:SetPoint("BOTTOMRIGHT", header.name, "BOTTOMRIGHT", 0, 0);
	hover:EnableMouse(true);
	hover:SetScript("OnEnter", icon:GetScript("OnEnter"));
	hover:SetScript("OnLeave", icon:GetScript("OnLeave"));

	local container = CreateFrame("Frame", "GameEditorItemEditorPages", body);
	container:SetPoint("TOPLEFT", header, "BOTTOMLEFT", 6, -30);
	container:SetPoint("BOTTOMRIGHT", body, "BOTTOMRIGHT", -6, 6);

	tabs = Common.CreateTabs(body, TAB_NAMES, function(index)
		ShowPage(index);
	end);
	tabs.tabs[1]:SetPoint("TOPLEFT", header, "BOTTOMLEFT", 6, -4);

	for index = 1, #TABS do
		pages[index] = BuildTemplatePage(container, index);
	end

	pages[LOCALE] = BuildLocalePage(container);

	for _, page in pairs(pages) do
		page:SetAllPoints(container);
	end

	ShowPage(tabs.current);
	LoadTemplatePages();

	if ( I.entry ) then
		localeEditor:SetKey(I.entry);
	end

	frame:SetScript("OnShow", function() Editor.Refresh(); end);
	Editor.Refresh();
	return frame;
end

Editor.Build = Common.SharedBuilder(Build);

function Editor.HasChanges()
	if ( not frame ) then
		return false;
	end

	local _, count = TemplateChanges();
	return count > 0 or localeEditor:HasChanges();
end

function Editor.SaveAll()
	if ( not I.entry ) then
		GameEditor_Notify("Open an item first", "info");
		return;
	end

	if ( not frame ) then
		GameEditor_Notify(format("Nothing to save, item %d has no changes", I.entry), "info");
		return;
	end

	local saved = 0;

	if ( SaveTemplate() ) then
		saved = saved + 1;
	end

	if ( localeEditor:HasChanges() and localeEditor:Save() ) then
		saved = saved + 1;
	end

	if ( saved == 0 and Editor.HasChanges() ) then
		GameEditor_Notify(format("Item %d has changes but nothing was sent, wait for it to finish loading", I.entry), "error");
	elseif ( saved == 0 ) then
		GameEditor_Notify(format("Nothing to save, item %d has no changes", I.entry), "info");
	end
end

function Editor.RevertAll()
	if ( not Editor.HasChanges() ) then
		GameEditor_Notify(I.entry and format("Nothing to revert, item %d has no changes", I.entry) or "Nothing to revert, no item is open", "info");
		return;
	end

	for index = 1, #TABS do
		pages[index]:Revert();
	end

	FollowClass(pages[1].grid:GetValue("class"));

	for index = 1, #TABS do
		pages[index].grid:Refresh();
	end

	if ( localeEditor:HasChanges() ) then
		localeEditor:Revert();
	end

	UpdateDirty();
end

function Editor.SelectTab(index)
	if ( tabs ) then
		tabs:Select(index);
	end
end

-- Values of the open item, a new item copies them.
function Editor.Snapshot()
	if ( not frame or not I.entry or not I.IsLoaded() ) then
		return nil;
	end

	return CurrentValues();
end

-- Runs action right away, or after asking when the open item has unsaved changes.
function Editor.ConfirmDiscard(action)
	if ( not Editor.HasChanges() ) then
		action();
		return;
	end

	NoggitMessageBox_Show("Unsaved changes", format("Item %d has unsaved changes. Drop them?", I.entry or 0), {
		{ text = "Drop changes", onClick = action },
		{ text = "Cancel" },
	});
end

-- Event hooks ----------------------------------------------------------------------------------------------------

I.On("TEMPLATE", function()
	if ( frame ) then
		LoadTemplatePages();
		Editor.Refresh();
	end
end);

I.On("OPENED", function(entry)
	if ( not frame ) then
		return;
	end

	loadedEntry = nil;
	localeEditor:SetKey(entry);
	Editor.Refresh();
end);

I.On("RELOAD", function()
	if ( frame and I.entry ) then
		localeEditor:Reload();
	end
end);

I.On("STATE", function()
	if ( frame and body:IsShown() ) then
		for index = 1, #TABS do
			pages[index]:UpdateState();
		end

		RefreshHeader();
		UpdateDirty();
	end
end);

local function RefreshNames()
	if ( not frame or not body:IsVisible() ) then
		return;
	end

	RefreshHeader();

	for index = 1, #TABS do
		if ( pages[index]:IsShown() ) then
			pages[index].grid:Refresh();
		end
	end
end

I.On("NAMES", RefreshNames);
I.On("ITEM_INFO", RefreshNames);
