-- Select Spell tab: the spell list with the filters of the Personal Spell Editor, recent spells and the localized texts of the open spell.

local SE = GameEditorSpell;

local ROW_HEIGHT = 32;
local SEARCH_DELAY = 0.25;
local LIST_WIDTH = 340;
local FILTER_LABEL = 150;
local FILTER_ROW = 28;

local LOCALES = { "English", "Korean", "French", "Deutsch", "Chinese", "Taiwanese", "Spanish", "Mexican", "Russian" };

local function EffectColumns(prefix)
	return { prefix.."[0]", prefix.."[1]", prefix.."[2]" };
end

-- Laid out in the same two columns as the Personal Spell Editor, left column first.
local FILTERS = {
	{ key = "category", label = "Filter Category:", columns = { "m_category" }, ref = "dbc:SpellCategory", side = 2, row = 1 },
	{ key = "family", label = "Filter by Family:", columns = { "m_spellClassSet" }, enum = "SpellFamily", side = 2, row = 2 },
	{ key = "effect", label = "Filter by spell effect:", columns = EffectColumns("m_effect"), enum = "SpellEffect", side = 1, row = 3 },
	{ key = "aura", label = "Filter by spell aura:", columns = EffectColumns("m_effectAura"), enum = "SpellAura", side = 1, row = 4 },
	{ key = "targetA", label = "Filter by Target A:", columns = EffectColumns("m_implicitTargetA"), enum = "SpellImplicitTarget", side = 2, row = 3 },
	{ key = "targetB", label = "Filter by Target B:", columns = EffectColumns("m_implicitTargetB"), enum = "SpellImplicitTarget", side = 2, row = 4 },
};

local browser;
local texts;
local contextItem;
local allSpells;

local function SelectTab(index)
	for _, tab in ipairs(browser.tabs) do
		tab.selected = tab.index == index;
		NoggitTab_Refresh(tab);
	end
end

local function ShowRecent()
	local items = {};

	for _, id in ipairs(SE.recent) do
		table.insert(items, { id = id });
	end

	browser.mode = "recent";
	browser.items = items;
	NoggitListBox_SetItems(browser.list, items);
	browser.countText:SetText(#items > 0 and format("%d recent", #items) or "No recent spells yet");
	SelectTab(2);
	SE.SyncBrowserSelection();
end

local function Conditions()
	local where = {};

	for _, filter in ipairs(FILTERS) do
		local value = browser.filters[filter.key].value;

		if ( value ~= nil ) then
			table.insert(where, { columns = filter.columns, value = value });
		end
	end

	return where;
end

local function RunSearch()
	local text = strtrim(browser.name:GetText() or "");
	local where = Conditions();
	local number = tonumber(text);
	local rows;

	if ( #where == 0 and text == "" ) then
		allSpells = allSpells or SE.Filter(SE.TABLE, "", nil);
		rows = allSpells;
	else
		rows = SE.Filter(SE.TABLE, number and "" or text, where);
	end

	local items = {};

	-- A number in the name filter narrows to ids that start with it, the exact id first.
	if ( number ) then
		local prefix = tostring(floor(number));

		for _, row in ipairs(rows) do
			if ( row.id == number ) then
				table.insert(items, 1, { id = row.id, name = row.name });
			elseif ( string.sub(tostring(row.id), 1, string.len(prefix)) == prefix ) then
				table.insert(items, { id = row.id, name = row.name });
			end
		end
	else
		for index, row in ipairs(rows) do
			items[index] = { id = row.id, name = row.name };
		end
	end

	browser.mode = "search";
	browser.items = items;
	NoggitListBox_SetItems(browser.list, items);

	if ( #items == 0 ) then
		browser.countText:SetText("No spell matches");
	elseif ( #where == 0 and text == "" ) then
		browser.countText:SetText(format("%d spells", #items));
	else
		browser.countText:SetText(format("%d match%s", #items, #items == 1 and "" or "es"));
	end

	SelectTab(1);
	SE.SyncBrowserSelection();
end

function SE.SyncBrowserSelection()
	if ( not browser ) then
		return;
	end

	local id = SE.CurrentId();
	browser.list.selected = nil;

	for index, item in ipairs(browser.list.items) do
		if ( item.id == id ) then
			browser.list.selected = index;
			break;
		end
	end

	NoggitListBox_Refresh(browser.list);
end

function SE.FocusSearch()
	SE.SelectSection("select");

	if ( browser ) then
		browser.name:SetFocus();
		browser.name:HighlightText();
	end
end

local function RefreshRow(row, item, selected)
	local doc = SE.FindDoc(item.id);
	local name, rank, icon;

	if ( doc ) then
		name = doc.values["m_name[0]"];
		rank = doc.values["m_nameSubtext[0]"];
		icon = SE.IconPath(doc.values.m_spellIconID);
	else
		local infoName, infoRank, infoIcon = GetSpellInfo(item.id);
		name = item.name or infoName;
		rank = infoRank;
		icon = infoIcon;

		if ( not name or not icon ) then
			local values = SE.GetRow(SE.TABLE, item.id);

			if ( values ) then
				name = name or values["m_name[0]"];
				rank = rank or values["m_nameSubtext[0]"];
				icon = icon or SE.IconPath(values.m_spellIconID);
			end
		end
	end

	row.icon:SetTexture(icon or "Interface\\Icons\\INV_Misc_QuestionMark");
	row.name:SetText(format("|cff8f8f8f%d|r  %s", item.id, (name and name ~= "") and name or "|cff8f8f8f(no name)|r"));
	row.detail:SetText((rank and rank ~= "") and rank or "");

	if ( doc and SE.DocHasUnsaved(doc) ) then
		row.dirty:Show();
	else
		row.dirty:Hide();
	end

	-- Spells open in a tab are white, the active one is also selected.
	NoggitText_SetColor(row.name, (selected or doc) and NOGGIT_COLOR_WHITE or NOGGIT_COLOR_TEXT);
end

local function BuildRow(row)
	row.icon = row:CreateTexture(nil, "ARTWORK");
	row.icon:SetWidth(26);
	row.icon:SetHeight(26);
	row.icon:SetPoint("LEFT", row, "LEFT", 4, 0);
	row.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93);
	row.name = SE.Label(row, "");
	row.name:SetPoint("TOPLEFT", row, "TOPLEFT", 36, -3);
	row.name:SetPoint("RIGHT", row, "RIGHT", -14, 0);
	row.detail = SE.Label(row, "", "NoggitFont10", NOGGIT_COLOR_DISABLED);
	row.detail:SetPoint("BOTTOMLEFT", row, "BOTTOMLEFT", 36, 4);
	row.detail:SetPoint("RIGHT", row, "RIGHT", -14, 0);
	row.dirty = SE.Label(row, "*", "NoggitFontBold", NOGGIT_COLOR_GOLD);
	row.dirty:SetPoint("RIGHT", row, "RIGHT", -4, 0);
	row.dirty:Hide();
end

local CONTEXT_MENU = {
	{ text = "Open in a tab", onClick = function() if ( contextItem ) then SE.Open(contextItem.id); end end },
	{ text = "Close its tab", onClick = function()
		local doc = contextItem and SE.FindDoc(contextItem.id);

		if ( doc ) then
			SE.CloseDoc(doc);
		end
	end },
	{ text = "Insert a new spell copied from this one", onClick = function()
		if ( contextItem ) then
			local values = SE.GetRow(SE.TABLE, contextItem.id);

			if ( values ) then
				SE.NewSpell(values);
			end
		end
	end },
	{ text = "Preview on target", onClick = function()
		if ( contextItem and SpellPreview_Cast ) then
			SpellPreview_Cast(contextItem.id, SE.PreviewTargetGuid("auto"));
		end
	end },
	{ separator = true },
	{ text = "Remove from recent", onClick = function() if ( contextItem ) then SE.RemoveRecent(contextItem.id); end end },
};

-- Opens a spell picker with search, used by Spell > Open by id.
function SE.PickSpell(title, onPick)
	GameEditor_PickRef(title or "Open spell", "spell", SE.CurrentId(), function(value)
		onPick(tonumber(value));
	end, { anyNumber = true });
end

local function BuildList(parent)
	browser = CreateFrame("Frame", "GameEditorSpellBrowser", parent);
	browser.filters = {};

	local tabsHolder = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellBrowserTabs"), browser);
	tabsHolder:SetHeight(24);
	tabsHolder:SetPoint("TOPLEFT", browser, "TOPLEFT", 0, 0);
	tabsHolder:SetPoint("TOPRIGHT", browser, "TOPRIGHT", 0, 0);
	browser.tabs = NoggitTabs_Create(tabsHolder, { "Spells", "Recent" }, 1, function(index)
		if ( index == 2 ) then
			ShowRecent();
		else
			RunSearch();
		end
	end);
	browser.tabs[1]:SetPoint("BOTTOMLEFT", tabsHolder, "BOTTOMLEFT", 0, 0);

	browser.countText = SE.Label(tabsHolder, "", "NoggitFont10", NOGGIT_COLOR_DISABLED, "RIGHT");
	browser.countText:SetPoint("RIGHT", tabsHolder, "RIGHT", -2, 0);

	browser.list = NoggitListBox_Create(browser, ROW_HEIGHT, BuildRow, RefreshRow);
	browser.list:SetPoint("TOPLEFT", tabsHolder, "BOTTOMLEFT", 0, -4);
	browser.list:SetPoint("BOTTOMRIGHT", browser, "BOTTOMRIGHT", 0, 20);
	browser.list.onSelect = function(item)
		if ( GetMouseButtonClicked and GetMouseButtonClicked() == "RightButton" ) then
			return;
		end

		SE.Open(item.id);
	end;
	browser.list.onContext = function(item)
		contextItem = item;
		SE.SyncBrowserSelection();
		NoggitContextMenu_Show("GameEditorSpellBrowserContext", CONTEXT_MENU);
	end;

	browser.idText = SE.Label(browser, "", "NoggitFont10", NOGGIT_COLOR_DISABLED);
	browser.idText:SetPoint("BOTTOMLEFT", browser, "BOTTOMLEFT", 2, 4);
	browser.idText:SetPoint("RIGHT", browser, "RIGHT", -2, 0);

	browser:SetScript("OnUpdate", function(self, elapsed)
		if ( self.pending ) then
			self.pending = self.pending - elapsed;

			if ( self.pending <= 0 ) then
				self.pending = nil;
				RunSearch();
			end
		end
	end);
	browser:SetScript("OnShow", function()
		browser.idText:SetText(format("Highest spell id %d, right click a spell for more", SE.MaxId(SE.TABLE)));
	end);

	SE.On("RECENT_CHANGED", function()
		if ( browser.mode == "recent" ) then
			ShowRecent();
		end
	end);
	SE.On("SPELL_LOADED", SE.SyncBrowserSelection);
	SE.On("SPELL_SAVED", function()
		allSpells = nil;
	end);
	SE.On("REFRESH", function()
		NoggitListBox_Refresh(browser.list);
	end);

	return browser;
end

-- The filter rows of the Personal Spell Editor above the texts.
local function BuildFilters(parent)
	local frame = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellFilters"), parent);
	frame:SetHeight(4 * FILTER_ROW);
	frame.controls = {};

	local goTo = NoggitLineEdit_Create(frame, 100);
	goTo:SetHeight(22);
	goTo:SetAutoFocus(false);
	goTo:SetScript("OnEnterPressed", function(self)
		self:ClearFocus();
		local id = tonumber(self:GetText());

		if ( id ) then
			SE.Open(id);
		end
	end);
	goTo:SetScript("OnEscapePressed", function(self) self:SetText(""); self:ClearFocus(); end);
	SE.Tooltip(goTo, "Type a spell id and press Enter to open it");
	table.insert(frame.controls, { control = goTo, label = "Go to spell id:", side = 1, row = 1, fixed = 100 });

	browser.name = SE.SearchBox(frame, 200, "Name, or an id prefix", function()
		browser.pending = SEARCH_DELAY;
	end);
	browser.name:SetScript("OnEnterPressed", function(self)
		self:ClearFocus();
		browser.pending = nil;
		RunSearch();

		local first = browser.list.items[1];

		if ( first and (#browser.list.items == 1 or tonumber(self:GetText()) == first.id) ) then
			SE.Open(first.id);
		end
	end);
	table.insert(frame.controls, { control = browser.name, label = "Filter by name:", side = 1, row = 2 });

	for _, filter in ipairs(FILTERS) do
		local options;

		if ( filter.enum ) then
			options = {
				items = function() return GameEditor_EnumItems(filter.enum); end,
				text = function(value) return SE.EnumText(filter.enum, value); end,
			};
		else
			options = {
				items = function() return (GameEditor_RefList(filter.ref)); end,
				text = function(value) return GameEditor_ResolveRef(filter.ref, value); end,
			};
		end

		local field = SE.CreatePickField(frame, 200, (string.gsub(filter.label, ":$", "")), options, function()
			RunSearch();
		end);
		browser.filters[filter.key] = field;
		table.insert(frame.controls, { control = field, label = filter.label, side = filter.side, row = filter.row });
	end

	for _, entry in ipairs(frame.controls) do
		entry.text = SE.Label(frame, entry.label);
		entry.text:SetJustifyH("LEFT");
	end

	local clear = SE.Button(frame, "Clear filters", 90, function()
		browser.name:SetQuietText("");
		browser.name.hint:Show();

		for _, field in pairs(browser.filters) do
			field:SetValue(nil);
		end

		RunSearch();
	end, "Back to the full spell list");

	frame.Layout = function(self)
		local half = floor(((self:GetWidth() or 600) - 16) / 2);

		for _, entry in ipairs(self.controls) do
			local x = (entry.side - 1) * (half + 16);
			local y = -(entry.row - 1) * FILTER_ROW;
			entry.text:ClearAllPoints();
			entry.text:SetPoint("TOPLEFT", self, "TOPLEFT", x, y - 4);
			entry.text:SetWidth(FILTER_LABEL - 6);
			entry.control:ClearAllPoints();
			entry.control:SetPoint("TOPLEFT", self, "TOPLEFT", x + FILTER_LABEL, y);
			entry.control:SetWidth(entry.fixed or max(60, half - FILTER_LABEL));
		end

		clear:ClearAllPoints();
		clear:SetPoint("TOPLEFT", self, "TOPLEFT", FILTER_LABEL + 110, 0);
	end;

	frame:SetScript("OnSizeChanged", function(self) self:Layout(); end);
	return frame;
end

-- Texts ------------------------------------------------------------------------------------------------

local function LocaleColumns(locale)
	return {
		name = "m_name["..locale.."]",
		rank = "m_nameSubtext["..locale.."]",
		tooltip = "m_auraDescription["..locale.."]",
		description = "m_description["..locale.."]",
	};
end

local function BuildTexts(parent)
	texts = CreateFrame("Frame", "GameEditorSpellTexts", parent);
	texts.locale = 0;
	texts.columns = LocaleColumns(0);

	local tabHolder = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellTextTabs"), texts);
	tabHolder:SetHeight(24);
	tabHolder:SetPoint("TOPLEFT", texts, "TOPLEFT", 0, 0);
	tabHolder:SetPoint("TOPRIGHT", texts, "TOPRIGHT", 0, 0);

	local body = SE.Box(texts, "GameEditorSpellTextBody", NOGGIT_COLOR_BASE, NOGGIT_COLOR_MID);
	body:SetPoint("TOPLEFT", tabHolder, "BOTTOMLEFT", 0, 0);
	body:SetPoint("BOTTOMRIGHT", texts, "BOTTOMRIGHT", 0, 0);

	local fields = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellTextFields"), body);
	fields:SetPoint("TOPLEFT", body, "TOPLEFT", 8, -8);
	fields:SetPoint("BOTTOMRIGHT", body, "BOTTOMRIGHT", -8, 8);

	local masks = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellTextMasks"), body);
	masks:SetPoint("TOPLEFT", body, "TOPLEFT", 8, -8);
	masks:SetPoint("BOTTOMRIGHT", body, "BOTTOMRIGHT", -8, 8);
	masks:Hide();
	local maskGrid = SE.FieldColumn(masks, {
		{ "m_nameFlags", "Name Locale Mask" },
		{ "m_nameSubtextFlags", "Rank Locale Mask" },
		{ "m_descriptionFlags", "Description Locale Mask" },
		{ "m_auraDescriptionFlags", "Tooltip Locale Mask" },
	}, { labelWidth = 190, editWidth = 110 });
	maskGrid.frame:SetAllPoints(masks);
	masks:SetScript("OnShow", function() SE.SyncGrid(maskGrid); end);

	local empty = SE.Label(body, "Pick a spell in the list to edit its texts.", nil, NOGGIT_COLOR_DISABLED);
	empty:SetPoint("TOPLEFT", body, "TOPLEFT", 10, -12);
	texts.empty = empty;

	local localeNames = {};

	for _, name in ipairs(LOCALES) do
		table.insert(localeNames, name);
	end

	table.insert(localeNames, "Masks");

	local tabs = NoggitTabs_Create(tabHolder, localeNames, 1, function(index)
		if ( index > #LOCALES ) then
			fields:Hide();
			masks:Show();
			return;
		end

		masks:Hide();
		texts.locale = index - 1;
		texts.columns = LocaleColumns(index - 1);
		texts:Refresh();
	end);
	tabs[1]:SetPoint("BOTTOMLEFT", tabHolder, "BOTTOMLEFT", 0, 0);
	texts.fields = fields;

	-- OnTextChanged runs a frame after SetText, only typed text counts as an edit.
	local function LineField(key)
		local edit = NoggitLineEdit_Create(fields, 200);
		edit:SetHeight(24);
		edit:SetMaxLetters(255);
		edit:SetScript("OnTextChanged", function(self, userInput)
			local text = self:GetText();

			if ( userInput and text ~= self.shownText ) then
				self.shownText = text;
				SE.SetValue(texts.columns[key], text, texts);
			end
		end);
		edit.SetValue = function(self, text)
			text = text or "";
			self.shownText = text;

			if ( self:GetText() == text ) then
				return;
			end

			self:SetText(text);
			self:SetCursorPosition(0);
		end;
		return edit;
	end

	local function AreaField(key, shortName)
		local area = SE.CreateTextArea(fields, function(text)
			SE.SetValue(texts.columns[key], text, texts);
		end);
		SE.AttachTokenHelper(area.edit, area);
		area.variables = SE.Button(fields, "Variables", 76, function()
			SE.ShowTokenPicker(area.edit, shortName);
		end, "Lists every $ variable with its current value, a click inserts it at the cursor");
		area.variables:SetHeight(18);
		return area;
	end

	local function Preview(color)
		local box = SE.Box(fields, "GameEditorSpellTextPreview", { 0.05, 0.05, 0.07, 1 }, NOGGIT_COLOR_MID);
		box.text = box:CreateFontString(nil, "ARTWORK", "GameTooltipText");
		box.text:SetJustifyH("LEFT");
		box.text:SetJustifyV("TOP");
		box.text:SetPoint("TOPLEFT", box, "TOPLEFT", 8, -6);
		box.text:SetPoint("BOTTOMRIGHT", box, "BOTTOMRIGHT", -8, 6);
		box.text:SetTextColor(color[1], color[2], color[3]);
		return box;
	end

	local function Title(text)
		return SE.Label(fields, text, "NoggitFontBold");
	end

	local nameTitle, rankTitle = Title("Spell Name:"), Title("Spell Rank:");
	local tooltipTitle, ingameTooltipTitle = Title("Spell Tooltip:"), Title("Spell Ingame Tooltip:");
	local descriptionTitle, ingameDescriptionTitle = Title("Spell Description:"), Title("Spell Ingame Description:");
	texts.name = LineField("name");
	texts.rank = LineField("rank");
	texts.tooltip = AreaField("tooltip", "the tooltip");
	texts.description = AreaField("description", "the description");
	texts.tooltipPreview = Preview({ 1, 1, 1 });
	texts.descriptionPreview = Preview({ 1, 0.82, 0 });
	texts.unresolved = SE.Label(fields, "", nil, NOGGIT_COLOR_DISABLED);

	local help = SE.GlyphButton(fields, 0xF059, SE.TOKEN_HELP, nil, 22);
	local insert = SE.Button(fields, "Insert variable", 100, function()
		local edit = SE.LastTokenEdit();

		if ( not edit or not edit:IsVisible() ) then
			edit = texts.description.edit;
		end

		SE.ShowTokenPicker(edit);
	end, "Pick a $ variable for the last description or tooltip you edited.\nTyping $ in a field lists them too.");
	insert:SetHeight(20);

	texts.Layout = function(self)
		local width = max(300, fields:GetWidth() or 600);
		local height = max(200, fields:GetHeight() or 400);
		local half = floor((width - 12) / 2);
		local right = half + 12;
		local area = max(48, floor((height - 18 - 28 - 2 * (18 + 8) - 30) / 2));
		local y = 0;

		local function Pair(leftTitle, rightTitle, leftWidget, rightWidget, widgetHeight)
			leftTitle:ClearAllPoints();
			leftTitle:SetPoint("TOPLEFT", fields, "TOPLEFT", 0, -y);
			rightTitle:ClearAllPoints();
			rightTitle:SetPoint("TOPLEFT", fields, "TOPLEFT", right, -y);
			y = y + 18;
			leftWidget:ClearAllPoints();
			leftWidget:SetPoint("TOPLEFT", fields, "TOPLEFT", 0, -y);
			leftWidget:SetWidth(half);
			leftWidget:SetHeight(widgetHeight);
			rightWidget:ClearAllPoints();
			rightWidget:SetPoint("TOPLEFT", fields, "TOPLEFT", right, -y);
			rightWidget:SetWidth(half);
			rightWidget:SetHeight(widgetHeight);
			y = y + widgetHeight + 8;
		end

		Pair(nameTitle, rankTitle, self.name, self.rank, 24);
		Pair(tooltipTitle, ingameTooltipTitle, self.tooltip, self.tooltipPreview, area);
		Pair(descriptionTitle, ingameDescriptionTitle, self.description, self.descriptionPreview, area);

		for _, field in ipairs({ self.tooltip, self.description }) do
			field.variables:ClearAllPoints();
			field.variables:SetPoint("BOTTOMRIGHT", field, "TOPRIGHT", 0, 1);
		end

		help:ClearAllPoints();
		help:SetPoint("TOPRIGHT", fields, "TOPRIGHT", 0, -y);
		insert:ClearAllPoints();
		insert:SetPoint("RIGHT", help, "LEFT", -6, 0);
		self.unresolved:ClearAllPoints();
		self.unresolved:SetPoint("TOPLEFT", fields, "TOPLEFT", 0, -y - 4);
		self.unresolved:SetPoint("RIGHT", insert, "LEFT", -8, 0);
	end;

	texts.RefreshPreview = function(self)
		local values = SE.cur and SE.cur.values or {};
		local tooltip = SE.ResolveText(values[self.columns.tooltip] or "", values, true);
		local description = SE.ResolveText(values[self.columns.description] or "", values, true);
		self.tooltipPreview.text:SetText(tooltip ~= "" and tooltip or "|cff8f8f8f(no tooltip)|r");
		self.descriptionPreview.text:SetText(description ~= "" and description or "|cff8f8f8f(no description)|r");

		local missing = 0;

		for _ in string.gmatch(tooltip..description, "|cffff5050") do
			missing = missing + 1;
		end

		self.unresolved:SetText(missing > 0 and format("|cffff5050%d variable%s could not be resolved.|r Hover the question mark for the supported ones.", missing, missing == 1 and "" or "s") or "All variables resolved.");
	end;

	texts.Refresh = function(self)
		if ( not SE.cur ) then
			fields:Hide();
			masks:Hide();
			empty:Show();
			return;
		end

		empty:Hide();

		if ( not masks:IsShown() ) then
			fields:Show();
		end

		local values = SE.cur.values;

		for _, key in ipairs({ "name", "rank", "tooltip", "description" }) do
			local value = values[self.columns[key]];
			self[key]:SetValue(value ~= nil and tostring(value) or "");
		end

		self.stamp = SE.stamp;
		self:RefreshPreview();
	end;

	fields:SetScript("OnSizeChanged", function() texts:Layout(); end);
	texts:SetScript("OnHide", function() SE.HideTokenHelpers(); end);
	texts:SetScript("OnShow", function(self)
		self:Layout();

		if ( self.stamp ~= SE.stamp ) then
			self:Refresh();
		end
	end);

	SE.On("VALUE_CHANGED", function(_, source)
		if ( texts:IsVisible() ) then
			if ( source == texts ) then
				texts.previewQueued = true;
			else
				texts:Refresh();
			end
		end
	end);
	SE.On("SPELL_LOADED", function()
		if ( texts:IsVisible() ) then
			texts:Refresh();
		end
	end);
	SE.On("REFRESH", function()
		if ( texts.previewQueued and texts:IsVisible() ) then
			texts.previewQueued = false;
			texts:RefreshPreview();
		end
	end);

	texts.stamp = -1;
	return texts;
end

local function BuildSelect(parent)
	local frame = CreateFrame("Frame", "GameEditorSpellSelect", parent);
	local list = BuildList(frame);
	list:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, 0);
	list:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 0, 0);
	list:SetWidth(LIST_WIDTH);

	local right = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellSelectRight"), frame);
	right:SetPoint("TOPLEFT", list, "TOPRIGHT", 16, 0);
	right:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", 0, 0);

	local filters = BuildFilters(right);
	filters:SetPoint("TOPLEFT", right, "TOPLEFT", 0, 0);
	filters:SetPoint("TOPRIGHT", right, "TOPRIGHT", 0, 0);

	local textFrame = BuildTexts(right);
	textFrame:SetPoint("TOPLEFT", filters, "BOTTOMLEFT", 0, -10);
	textFrame:SetPoint("BOTTOMRIGHT", right, "BOTTOMRIGHT", 0, 0);

	frame:SetScript("OnShow", function()
		filters:Layout();
	end);

	RunSearch();
	return frame;
end

do
	local columns = {};

	for locale = 0, #LOCALES - 1 do
		for _, column in pairs(LocaleColumns(locale)) do
			table.insert(columns, column);
		end
	end

	table.insert(columns, "m_nameFlags");
	table.insert(columns, "m_nameSubtextFlags");
	table.insert(columns, "m_descriptionFlags");
	table.insert(columns, "m_auraDescriptionFlags");

	SE.AddSection({ key = "select", title = "Select Spell", order = 10, noSpell = true, columns = columns, build = BuildSelect });
end
