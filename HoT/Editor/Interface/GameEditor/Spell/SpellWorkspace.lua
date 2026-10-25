-- Spell workspace: main panel laid out like the Personal Spell Editor (command bar, spell tabs, main tab strip, content), menus, keys and registration.

local SE = GameEditorSpell;

local LEFT_OFFSET = 56;
local HEADER_HEIGHT = 56;
local MIN_WIDTH = 900;
local COLLAPSED_WIDTH = 44;

local root;
local panel;
local strip;
local header;
local mainTabs;
local content;
local emptyState;
local hoverTooltip;
local activeSection;
local collapsed = false;

local function SortSections()
	table.sort(SE.SECTIONS, function(a, b)
		return (a.order or 1000) < (b.order or 1000);
	end);
end

local function Section(key)
	for _, section in ipairs(SE.SECTIONS) do
		if ( section.key == key ) then
			return section;
		end
	end
end

local function BuildSection(section)
	if ( not section.frame ) then
		local frame = section.build(content);
		frame:ClearAllPoints();
		frame:SetPoint("TOPLEFT", content, "TOPLEFT", 0, 0);
		frame:SetPoint("BOTTOMRIGHT", content, "BOTTOMRIGHT", 0, 0);
		frame:Hide();
		section.frame = frame;
	end

	return section.frame;
end

local function SectionBadge(section)
	local badge;

	if ( section.badge ) then
		badge = section.badge();
	end

	if ( not badge and section.columns and SE.cur ) then
		local changed = SE.ChangeCount(section.columns);
		badge = changed > 0 and format("|cffffcc00%d|r", changed) or nil;
	end

	return badge;
end

local function RefreshMainTabs()
	if ( not mainTabs ) then
		return;
	end

	for _, tab in ipairs(mainTabs) do
		local section = tab.section;
		local badge = SectionBadge(section);
		local text = section.title..(badge and ("  "..badge) or "");

		if ( tab.shownText ~= text ) then
			tab.shownText = text;
			tab:SetText(text);
			tab:SetWidth(max(55, tab:GetFontString():GetStringWidth() + 20) + 2);
		end

		tab.selected = activeSection == section;
		NoggitTab_Refresh(tab);
	end
end

function SE.SelectSection(key)
	local section = Section(key) or Section("select");

	if ( not section or not content ) then
		return;
	end

	if ( activeSection and activeSection ~= section and activeSection.frame ) then
		activeSection.frame:Hide();
	end

	activeSection = section;
	SE.lastSection = section.key;

	if ( SE.cur or section.noSpell ) then
		emptyState:Hide();
		BuildSection(section):Show();
	else
		if ( section.frame ) then
			section.frame:Hide();
		end

		emptyState:Show();
	end

	RefreshMainTabs();
end

local function BuildMainTabs(parent)
	local holder = CreateFrame("Frame", "GameEditorSpellMainTabs", parent);
	holder:SetHeight(26);
	local titles = {};

	for _, section in ipairs(SE.SECTIONS) do
		table.insert(titles, section.title);
	end

	mainTabs = NoggitTabs_Create(holder, titles, 1, function(index)
		SE.SelectSection(SE.SECTIONS[index].key);
	end);

	for index, tab in ipairs(mainTabs) do
		tab.section = SE.SECTIONS[index];
	end

	mainTabs[1]:SetPoint("BOTTOMLEFT", holder, "BOTTOMLEFT", 0, 0);

	local line = NoggitUI_SolidTexture(holder, "ARTWORK", NOGGIT_COLOR_MID);
	line:SetHeight(1);
	line:SetPoint("BOTTOMLEFT", holder, "BOTTOMLEFT", 0, 0);
	line:SetPoint("BOTTOMRIGHT", holder, "BOTTOMRIGHT", 0, 0);
	return holder;
end

-- Spell tabs ----------------------------------------------------------------------------------------------

local TAB_HEIGHT = 24;
local TAB_MIN = 92;
local TAB_MAX = 200;
local TAB_GAP = 2;
local TAB_BUTTON = 22;
local CLOSE_TEXTURE = "Interface\\GameEditor\\Textures\\icon_close.tga";

local tabBar;
local contextDoc;

local function TabName(doc)
	local name = tostring(doc.values["m_name[0]"] or "");

	if ( name == "" ) then
		return tostring(doc.id);
	end

	if ( string.len(name) > 26 ) then
		-- Cut on a character boundary so a multi byte letter is not split.
		name = string.gsub(string.sub(name, 1, 24), "[\192-\255][\128-\191]*$", "").."...";
	end

	return name;
end

local function TabTooltip(doc)
	local name, rank = tostring(doc.values["m_name[0]"] or ""), tostring(doc.values["m_nameSubtext[0]"] or "");
	local lines = { format("%d  %s%s", doc.id, name ~= "" and name or "(no name)", rank ~= "" and ("  |cff8f8f8f"..rank.."|r") or "") };

	if ( SE.pendingSave[doc.id] ) then
		table.insert(lines, "|cffffcc00saving...|r");
	elseif ( doc.isNew ) then
		table.insert(lines, "|cffffcc00new, not saved|r");
	else
		local changed = SE.DocChangeCount(doc);

		if ( changed > 0 ) then
			table.insert(lines, format("|cffffcc00%d unsaved field%s|r", changed, changed == 1 and "" or "s"));
		end
	end

	for _, text in ipairs(SE.DocWorldDirtyParts and SE.DocWorldDirtyParts(doc) or {}) do
		table.insert(lines, "|cffffcc00"..text.."|r");
	end

	if ( doc.appliedLocal ) then
		table.insert(lines, "|cff80c0ffapplied locally|r");
	end

	if ( doc.remoteChanged ) then
		table.insert(lines, "|cffff8040changed elsewhere|r");
	end

	if ( #lines == 1 ) then
		table.insert(lines, "|cff60d060saved|r");
	end

	table.insert(lines, "\n|cff8f8f8fMiddle click or Ctrl+W closes, Ctrl+Tab switches tabs.|r");
	return table.concat(lines, "\n");
end

local function RefreshTab(tab)
	local doc = tab.doc;

	if ( not doc ) then
		return;
	end

	local active = doc == SE.cur;
	tab.label:SetText((SE.DocHasUnsaved(doc) and "|cffffcc00*|r " or "")..TabName(doc));

	if ( tab.iconId ~= doc.values.m_spellIconID ) then
		tab.iconId = doc.values.m_spellIconID;
		tab.icon:SetTexture(SE.IconPath(tab.iconId));
	end

	if ( active ) then
		NoggitBox_Set(tab, NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID);
		tab.stripe:Show();
		NoggitText_SetColor(tab.label, NOGGIT_COLOR_WHITE);
	else
		NoggitBox_Set(tab, tab.hover and NOGGIT_COLOR_HOVER or NOGGIT_COLOR_MID, nil);
		tab.stripe:Hide();
		NoggitText_SetColor(tab.label, doc.remoteChanged and { 1, 0.55, 0.3, 1 } or NOGGIT_COLOR_TEXT);
	end

	local closeColor = tab.close.hover and NOGGIT_COLOR_WHITE or ((active or tab.hover) and NOGGIT_COLOR_TEXT or NOGGIT_COLOR_DISABLED);
	tab.close.texture:SetVertexColor(closeColor[1], closeColor[2], closeColor[3]);
end

local TAB_MENU = {
	{ text = "Close", shortcut = "Ctrl+W", onClick = function() if ( contextDoc ) then SE.CloseDoc(contextDoc); end end },
	{ text = "Close other tabs", onClick = function() SE.CloseOtherDocs(contextDoc); end },
	{ text = "Close saved tabs", onClick = function() SE.CloseSavedDocs(); end },
	{ separator = true },
	{ text = "Save spell", onClick = function()
		if ( contextDoc and SE.DocIndex(contextDoc) ) then
			SE.SaveDoc(contextDoc);

			if ( contextDoc ~= SE.cur and SE.SaveDocWorld ) then
				SE.SaveDocWorld(contextDoc);
			end
		end
	end },
	{ text = "Copy to a new id", onClick = function()
		if ( contextDoc and SE.DocIndex(contextDoc) ) then
			local values = SE.Copy(contextDoc.values);
			values.m_id = contextDoc.id;
			SE.NewSpell(values);
		end
	end },
};

local LayoutTabs;

local function CreateTab(index)
	local name = "GameEditorSpellDocTab"..index;
	local tab = CreateFrame("Button", name, tabBar, "NoggitBoxTemplate");
	tab:SetHeight(TAB_HEIGHT);
	tab:RegisterForClicks("LeftButtonUp", "MiddleButtonUp", "RightButtonUp");

	tab.icon = tab:CreateTexture(nil, "ARTWORK");
	tab.icon:SetWidth(16);
	tab.icon:SetHeight(16);
	tab.icon:SetPoint("LEFT", tab, "LEFT", 5, 0);
	tab.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93);

	local close = CreateFrame("Button", name.."Close", tab);
	close:SetWidth(16);
	close:SetHeight(16);
	close:SetPoint("RIGHT", tab, "RIGHT", -4, 0);
	close.texture = close:CreateTexture(nil, "ARTWORK");
	close.texture:SetTexture(CLOSE_TEXTURE);
	close.texture:SetTexCoord(0, 0.875, 0, 0.875);
	close.texture:SetWidth(9);
	close.texture:SetHeight(9);
	close.texture:SetPoint("CENTER", close, "CENTER", 0, 0);
	close:SetScript("OnEnter", function(self)
		self.hover = true;
		RefreshTab(tab);
		NoggitTooltip_Show(self, "Close this tab (Ctrl+W or middle click)");
	end);
	close:SetScript("OnLeave", function(self)
		self.hover = false;
		RefreshTab(tab);
		NoggitTooltip_Hide();
	end);
	close:SetScript("OnClick", function()
		if ( tab.doc ) then
			SE.CloseDoc(tab.doc);
		end
	end);
	tab.close = close;

	tab.label = SE.Label(tab, "");
	tab.label:SetPoint("LEFT", tab.icon, "RIGHT", 5, 0);
	tab.label:SetPoint("RIGHT", close, "LEFT", -2, 0);
	tab.label:SetHeight(TAB_HEIGHT);

	tab.stripe = NoggitUI_SolidTexture(tab, "OVERLAY", NOGGIT_COLOR_ACCENT);
	tab.stripe:SetHeight(2);
	tab.stripe:SetPoint("TOPLEFT", tab, "TOPLEFT", 0, 0);
	tab.stripe:SetPoint("TOPRIGHT", tab, "TOPRIGHT", 0, 0);

	tab:SetScript("OnClick", function(self, button)
		local doc = self.doc;

		if ( not doc ) then
			return;
		end

		if ( button == "MiddleButton" ) then
			SE.CloseDoc(doc);
		elseif ( button == "RightButton" ) then
			contextDoc = doc;
			NoggitContextMenu_Show("GameEditorSpellTabContext", TAB_MENU);
		else
			SE.Activate(doc);
		end
	end);
	tab:SetScript("OnEnter", function(self)
		self.hover = true;
		RefreshTab(self);

		if ( self.doc ) then
			NoggitTooltip_Show(self, TabTooltip(self.doc));
		end
	end);
	tab:SetScript("OnLeave", function(self)
		self.hover = false;
		RefreshTab(self);
		NoggitTooltip_Hide();
	end);

	tabBar.tabs[index] = tab;
	return tab;
end

-- Tabs shrink down to TAB_MIN, past that the strip scrolls and keeps the active tab in view.
LayoutTabs = function()
	if ( not tabBar ) then
		return;
	end

	local docs = SE.docs;
	local count = #docs;
	local total = tabBar:GetWidth() or 0;

	for index = count + 1, #tabBar.tabs do
		tabBar.tabs[index].doc = nil;
		tabBar.tabs[index]:Hide();
	end

	if ( count == 0 ) then
		tabBar.empty:Show();
		tabBar.left:Hide();
		tabBar.right:Hide();
		return;
	end

	tabBar.empty:Hide();

	if ( total <= 10 ) then
		return;
	end

	local available = total - TAB_BUTTON - 6;
	local sum = 0;

	for index, doc in ipairs(docs) do
		local tab = tabBar.tabs[index] or CreateTab(index);
		tab.doc = doc;
		RefreshTab(tab);
		tab.preferred = min(TAB_MAX, max(TAB_MIN, floor(tab.label:GetStringWidth() + 56)));
		sum = sum + tab.preferred + TAB_GAP;
	end

	local limit;
	local overflow = false;

	if ( sum - TAB_GAP > available ) then
		limit = floor((available + TAB_GAP) / count) - TAB_GAP;

		if ( limit < TAB_MIN ) then
			limit = TAB_MIN;
			overflow = true;
			available = available - 2 * (TAB_BUTTON + 2);
		end
	end

	local function Width(index)
		local preferred = tabBar.tabs[index].preferred;
		return limit and min(limit, preferred) or preferred;
	end

	local function Span(first, last)
		local width = 0;

		for index = first, last do
			width = width + Width(index) + TAB_GAP;
		end

		return width - TAB_GAP;
	end

	local first = 1;

	if ( overflow ) then
		first = max(1, min(tabBar.first or 1, count));
		local active = SE.DocIndex(SE.cur);

		if ( active and tabBar.followed ~= SE.cur ) then
			if ( active < first ) then
				first = active;
			end

			while ( first < active and Span(first, active) > available ) do
				first = first + 1;
			end
		end

		while ( first > 1 and Span(first - 1, count) <= available ) do
			first = first - 1;
		end
	end

	tabBar.followed = SE.cur;
	tabBar.first = first;
	tabBar.last = first - 1;
	local x = 0;
	local full = false;

	for index = 1, count do
		local tab = tabBar.tabs[index];
		local width = Width(index);

		if ( index < first or full or (x + width > available + 0.5 and index > first) ) then
			full = full or index >= first;
			tab:Hide();
		else
			tab:SetWidth(width);
			tab:ClearAllPoints();
			tab:SetPoint("BOTTOMLEFT", tabBar, "BOTTOMLEFT", x, 0);
			tab:Show();
			x = x + width + TAB_GAP;
			tabBar.last = index;
		end
	end

	if ( overflow ) then
		tabBar.left:Show();
		tabBar.right:Show();
		tabBar.left:SetAlpha(first > 1 and 1 or 0.35);
		tabBar.right:SetAlpha(tabBar.last < count and 1 or 0.35);
	else
		tabBar.left:Hide();
		tabBar.right:Hide();
	end
end

local function ScrollTabs(delta)
	tabBar.first = max(1, min(#SE.docs, (tabBar.first or 1) + delta));
	tabBar.followed = SE.cur;
	LayoutTabs();
end

local function BuildTabBar(parent)
	tabBar = CreateFrame("Frame", "GameEditorSpellTabBar", parent);
	tabBar:SetHeight(TAB_HEIGHT);
	tabBar:EnableMouseWheel(true);
	tabBar.tabs = {};

	tabBar.new = SE.GlyphButton(tabBar, 0xF067, "New spell in a new tab (Ctrl+N)", function() SE.NewSpell(); end, TAB_BUTTON);
	tabBar.new:SetPoint("BOTTOMRIGHT", tabBar, "BOTTOMRIGHT", 0, 1);
	tabBar.right = SE.GlyphButton(tabBar, 0xF054, "More tabs to the right (mouse wheel scrolls)", function() ScrollTabs(1); end, TAB_BUTTON);
	tabBar.right:SetPoint("RIGHT", tabBar.new, "LEFT", -6, 0);
	tabBar.left = SE.GlyphButton(tabBar, 0xF053, "More tabs to the left (mouse wheel scrolls)", function() ScrollTabs(-1); end, TAB_BUTTON);
	tabBar.left:SetPoint("RIGHT", tabBar.right, "LEFT", -2, 0);
	tabBar.left:Hide();
	tabBar.right:Hide();

	tabBar.empty = SE.Label(tabBar, "No spell open. Spells you open get a tab here.", nil, NOGGIT_COLOR_DISABLED);
	tabBar.empty:SetPoint("LEFT", tabBar, "LEFT", 4, 0);

	tabBar:SetScript("OnMouseWheel", function(_, delta) ScrollTabs(-delta); end);
	tabBar:SetScript("OnSizeChanged", function() LayoutTabs(); end);
	return tabBar;
end

-- Command bar ----------------------------------------------------------------------------------------------

local function RefreshHeader()
	local cur = SE.cur;

	if ( not cur ) then
		header.icon:SetTexture("Interface\\Icons\\INV_Misc_QuestionMark");
		header.name:SetText("No spell selected");
		header.rank:SetText("");
		header.info:SetText("Pick a spell in Select Spell or insert a new one.");
		header.state:SetText("");

		for _, button in ipairs(header.spellButtons) do
			button:Disable();
		end

		return;
	end

	local values = cur.values;
	header.icon:SetTexture(SE.IconPath(values.m_spellIconID));
	local name = tostring(values["m_name[0]"] or "");
	header.name:SetText(name ~= "" and name or "(no name)");
	header.rank:SetText(tostring(values["m_nameSubtext[0]"] or ""));
	header.rank:ClearAllPoints();
	header.rank:SetPoint("BOTTOMLEFT", header.name, "BOTTOMRIGHT", 10, 1);

	local family = SE.Number(values.m_spellClassSet);
	local parts = {
		format("|cffffffffID %d|r", cur.id),
		SE.SchoolText(values.m_schoolMask),
		family ~= 0 and SE.EnumText("SpellFamily", family) or nil,
		SE.CastText(values),
		SE.RangeText(values),
		SE.CooldownText(values),
	};
	local info = {};

	for index = 1, 6 do
		if ( parts[index] ) then
			table.insert(info, parts[index]);
		end
	end

	header.info:SetText(table.concat(info, "   |cff5f5f5f|||r   "));

	local state;
	local changed = SE.ChangeCount();

	if ( SE.pendingSave[cur.id] ) then
		state = "|cffffcc00Saving...|r";
	elseif ( cur.isNew ) then
		state = cur.copiedFrom and format("|cffffcc00New, copied from %d, not saved|r", cur.copiedFrom) or "|cffffcc00New, not saved|r";
	elseif ( changed > 0 ) then
		state = format("|cffffcc00%d unsaved change%s|r", changed, changed == 1 and "" or "s");
	else
		state = "|cff60d060Saved|r";
	end

	if ( cur.appliedLocal ) then
		state = state.."  |cff80c0ffapplied locally|r";
	end

	if ( cur.remoteChanged ) then
		state = state.."  |cffff8040changed elsewhere|r";
	end

	header.state:SetText(state);

	local dirty = SE.IsDirty();
	SE.SetEnabled(header.save, dirty);
	SE.SetEnabled(header.revert, dirty or cur.appliedLocal or cur.remoteChanged);
	SE.SetEnabled(header.apply, true);
	SE.SetEnabled(header.preview, true);
	SE.SetEnabled(header.copy, true);
end

local function ShowHoverTooltip(owner)
	if ( not SE.cur ) then
		return;
	end

	if ( not hoverTooltip ) then
		hoverTooltip = SE.CreateTooltipPreview(EditorParent, 320);
		hoverTooltip:SetFrameStrata("TOOLTIP");
	end

	hoverTooltip:ClearAllPoints();
	hoverTooltip:SetPoint("TOPLEFT", owner, "BOTTOMLEFT", 0, -4);
	hoverTooltip:Update(SE.cur.values);
	hoverTooltip:Show();
end

local function HideHoverTooltip()
	if ( hoverTooltip ) then
		hoverTooltip:Hide();
	end
end

local function BuildHeader(parent)
	header = SE.Box(parent, "GameEditorSpellHeader", NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID);
	header:SetHeight(HEADER_HEIGHT);

	local iconFrame = CreateFrame("Button", "GameEditorSpellHeaderIcon", header, "NoggitBoxTemplate");
	NoggitBox_Set(iconFrame, NOGGIT_COLOR_BASE, NOGGIT_COLOR_HOVER);
	iconFrame:SetWidth(40);
	iconFrame:SetHeight(40);
	iconFrame:SetPoint("LEFT", header, "LEFT", 8, 0);
	header.icon = iconFrame:CreateTexture(nil, "ARTWORK");
	header.icon:SetPoint("TOPLEFT", iconFrame, "TOPLEFT", 2, -2);
	header.icon:SetPoint("BOTTOMRIGHT", iconFrame, "BOTTOMRIGHT", -2, 2);
	header.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93);
	iconFrame:SetScript("OnEnter", function(self) ShowHoverTooltip(self); end);
	iconFrame:SetScript("OnLeave", HideHoverTooltip);
	iconFrame:SetScript("OnClick", function()
		if ( SE.cur ) then
			HideHoverTooltip();
			SE.SelectSection("icon");
		end
	end);

	header.name = SE.Label(header, "", "NoggitFont16", NOGGIT_COLOR_WHITE);
	header.name:SetPoint("TOPLEFT", iconFrame, "TOPRIGHT", 10, -2);
	header.rank = SE.Label(header, "", nil, NOGGIT_COLOR_DISABLED);
	header.rank:SetPoint("BOTTOMLEFT", header.name, "BOTTOMRIGHT", 10, 1);

	local collapse = SE.GlyphButton(header, 0xF100, "Collapse the editor to watch the world (Ctrl+E)", function() SE.SetCollapsed(true); end, 22);
	collapse:SetPoint("TOPRIGHT", header, "TOPRIGHT", -6, -6);

	header.state = SE.Label(header, "", nil, nil, "RIGHT");
	header.state:SetPoint("RIGHT", collapse, "LEFT", -8, 0);

	header.save = SE.Button(header, "Save Spell Changes", 130, function() SE.Save(); end, "Write the spell to the dbc database, the server pushes it to every client (Ctrl+S saves every tab)", true);
	header.save:SetPoint("BOTTOMRIGHT", header, "BOTTOMRIGHT", -6, 6);
	header.copy = SE.Button(header, "Copy Spell", 80, function() SE.CopySpell(); end, "Copy the open spell, unsaved edits included, to the next free id in a new tab (Ctrl+D)");
	header.copy:SetPoint("RIGHT", header.save, "LEFT", -4, 0);
	header.new = SE.Button(header, "New Spell", 76, function() SE.NewSpell(); end, "Insert a new spell record with the next free id. You can also right click spells in the list (Ctrl+N)");
	header.new:SetPoint("RIGHT", header.copy, "LEFT", -4, 0);
	header.revert = SE.Button(header, "Revert", 60, function() SE.Revert(); end, "Back to the saved spell, also undoes Apply Locally");
	header.revert:SetPoint("RIGHT", header.new, "LEFT", -12, 0);
	header.apply = SE.Button(header, "Apply Locally", 94, function() SE.ApplyLocal(); end, "Patch this client only, for an instant preview without saving (Ctrl+L)");
	header.apply:SetPoint("RIGHT", header.revert, "LEFT", -4, 0);
	header.preview = SE.Button(header, "Preview", 66, function()
		GameEditor_SelectTool("select");
		SE.PreviewCast();
	end, "Cast the spell from your character on the target (F5), the dock on the right has the options");
	header.preview:SetPoint("RIGHT", header.apply, "LEFT", -4, 0);
	header.spellButtons = { header.save, header.revert, header.apply, header.preview, header.copy };

	header.info = SE.Label(header, "");
	header.info:SetPoint("BOTTOMLEFT", iconFrame, "BOTTOMRIGHT", 10, 2);
	header.info:SetPoint("RIGHT", header.preview, "LEFT", -12, 0);

	return header;
end

-- Layout, collapse and resize ----------------------------------------------------------------------------

local function AreaWidth()
	local width = EditorMainArea:GetWidth() or 0;
	return width > 200 and width or 1200;
end

local function MaxWidth()
	return max(MIN_WIDTH, AreaWidth() - LEFT_OFFSET - 20);
end

local function ApplyLayout()
	SE.mainWidth = max(MIN_WIDTH, min(MaxWidth(), SE.mainWidth or 1180));

	if ( collapsed ) then
		panel:Hide();
		strip:Show();
		root:SetWidth(COLLAPSED_WIDTH);
	else
		strip:Hide();
		panel:Show();
		root:SetWidth(SE.mainWidth);
	end
end

function SE.SetCollapsed(value)
	collapsed = value and true or false;
	HideHoverTooltip();
	ApplyLayout();
	SE.QueueRefresh();
end

function SE.IsCollapsed()
	return collapsed;
end

local function BuildStrip(parent)
	strip = SE.Box(parent, "GameEditorSpellStrip", NOGGIT_COLOR_BASE, NOGGIT_COLOR_MID);
	strip:SetAllPoints(parent);
	strip:EnableMouse(true);
	strip:Hide();

	local expand = SE.GlyphButton(strip, 0xF101, "Show the spell editor (Ctrl+E)", function() SE.SetCollapsed(false); end, 30);
	expand:SetPoint("TOP", strip, "TOP", 0, -8);

	local iconFrame = CreateFrame("Button", "GameEditorSpellStripIcon", strip, "NoggitBoxTemplate");
	NoggitBox_Set(iconFrame, NOGGIT_COLOR_DARK, NOGGIT_COLOR_HOVER);
	iconFrame:SetWidth(32);
	iconFrame:SetHeight(32);
	iconFrame:SetPoint("TOP", expand, "BOTTOM", 0, -10);
	strip.icon = iconFrame:CreateTexture(nil, "ARTWORK");
	strip.icon:SetPoint("TOPLEFT", iconFrame, "TOPLEFT", 2, -2);
	strip.icon:SetPoint("BOTTOMRIGHT", iconFrame, "BOTTOMRIGHT", -2, 2);
	strip.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93);
	iconFrame:SetScript("OnEnter", function(self) ShowHoverTooltip(self); end);
	iconFrame:SetScript("OnLeave", HideHoverTooltip);
	iconFrame:SetScript("OnClick", function() SE.SetCollapsed(false); end);

	local previous = SE.GlyphButton(strip, 0xF053, "Previous spell tab (Ctrl+Shift+Tab)", function() SE.CycleDoc(-1); end, 17);
	previous:SetPoint("TOPRIGHT", iconFrame, "BOTTOM", -1, -4);
	local nextTab = SE.GlyphButton(strip, 0xF054, "Next spell tab (Ctrl+Tab)", function() SE.CycleDoc(1); end, 17);
	nextTab:SetPoint("TOPLEFT", iconFrame, "BOTTOM", 1, -4);
	strip.count = SE.Label(strip, "", "NoggitFont10", NOGGIT_COLOR_DISABLED, "CENTER");
	strip.count:SetPoint("TOP", iconFrame, "BOTTOM", 0, -25);

	local cast = SE.GlyphButton(strip, 0xF04B, "Preview on target (F5)", function() SE.PreviewCast(); end, 30);
	cast:SetPoint("TOP", iconFrame, "BOTTOM", 0, -42);
	local castSelf = SE.GlyphButton(strip, 0xF007, "Preview on self (Shift+F5)", function() SE.PreviewCast("self"); end, 30);
	castSelf:SetPoint("TOP", cast, "BOTTOM", 0, -4);
	local stop = SE.GlyphButton(strip, 0xF04D, "Stop the preview (Ctrl+F5)", function() SE.PreviewStop(); end, 30);
	stop:SetPoint("TOP", castSelf, "BOTTOM", 0, -4);
	strip.save = SE.GlyphButton(strip, 0xF0C7, "Save everything (Ctrl+S)", function() SE.SaveAll(); end, 30);
	strip.save:SetPoint("TOP", stop, "BOTTOM", 0, -14);

	strip.Refresh = function(self)
		self.icon:SetTexture(SE.cur and SE.IconPath(SE.cur.values.m_spellIconID) or "Interface\\Icons\\INV_Misc_QuestionMark");
		local index = SE.DocIndex(SE.cur);
		self.count:SetText(index and format("%d/%d", index, #SE.docs) or "");
		self.save.background = SE.AnyUnsaved() and { 0.45, 0.36, 0.1, 1 } or nil;
		self.save:Refresh();
	end;

	return strip;
end

local function BuildResizeGrip(parent)
	local grip = CreateFrame("Frame", "GameEditorSpellResizeGrip", parent);
	grip:SetWidth(6);
	grip:SetPoint("TOPRIGHT", parent, "TOPRIGHT", 0, -1);
	grip:SetPoint("BOTTOMRIGHT", parent, "BOTTOMRIGHT", 0, 1);
	grip:EnableMouse(true);
	local line = NoggitUI_SolidTexture(grip, "ARTWORK", NOGGIT_COLOR_MID);
	line:SetAllPoints(grip);
	grip:SetScript("OnEnter", function() line:SetTexture(NOGGIT_COLOR_ACCENT[1], NOGGIT_COLOR_ACCENT[2], NOGGIT_COLOR_ACCENT[3], 1); NoggitTooltip_Show(grip, "Drag to resize the editor"); end);
	grip:SetScript("OnLeave", function() line:SetTexture(NOGGIT_COLOR_MID[1], NOGGIT_COLOR_MID[2], NOGGIT_COLOR_MID[3], 1); NoggitTooltip_Hide(); end);
	grip:SetScript("OnMouseDown", function(self) self.dragging = true; end);
	grip:SetScript("OnMouseUp", function(self) self.dragging = false; end);
	grip:SetScript("OnUpdate", function(self)
		if ( not self.dragging ) then
			return;
		end

		if ( not EditorIsMouseButtonDown() ) then
			self.dragging = false;
			return;
		end

		local x = NoggitUI_CursorIn(root);
		SE.mainWidth = floor(x - (root:GetLeft() or 0));
		ApplyLayout();
	end);
	return grip;
end

local function BuildEmptyState(parent)
	local frame = CreateFrame("Frame", "GameEditorSpellEmpty", parent);
	frame:SetAllPoints(parent);
	local title = SE.Label(frame, "No spell selected", "NoggitFont18", NOGGIT_COLOR_WHITE);
	title:SetPoint("TOPLEFT", frame, "TOPLEFT", 6, -10);
	local text = SE.Label(frame, "Pick a spell in the Select Spell tab, open one by id, or insert a new spell.", nil, NOGGIT_COLOR_DISABLED);
	text:SetPoint("TOPLEFT", frame, "TOPLEFT", 6, -40);
	text:SetPoint("RIGHT", frame, "RIGHT", -6, 0);
	local select = SE.Button(frame, "Select Spell", 100, function() SE.SelectSection("select"); end, nil, true);
	select:SetPoint("TOPLEFT", frame, "TOPLEFT", 6, -70);
	local new = SE.Button(frame, "New Spell", 100, function() SE.NewSpell(); end);
	new:SetPoint("LEFT", select, "RIGHT", 6, 0);
	local open = SE.Button(frame, "Open by id", 100, function()
		SE.PickSpell("Open spell", function(id) SE.Open(id); end);
	end);
	open:SetPoint("LEFT", new, "RIGHT", 6, 0);
	return frame;
end

local function BuildMain(parent)
	SortSections();

	root = CreateFrame("Frame", "GameEditorSpellRoot", parent);
	root:SetPoint("TOPLEFT", parent, "TOPLEFT", LEFT_OFFSET, -6);
	root:SetPoint("BOTTOMLEFT", parent, "BOTTOMLEFT", LEFT_OFFSET, 6);
	root:SetWidth(1000);

	panel = SE.Box(root, "GameEditorSpellPanel", NOGGIT_COLOR_BASE, NOGGIT_COLOR_MID);
	panel:SetAllPoints(root);
	panel:EnableMouse(true);
	panel:EnableMouseWheel(true);

	panel.editor = CreateFrame("Frame", "GameEditorSpellEditorArea", panel);
	panel.editor:SetPoint("TOPLEFT", panel, "TOPLEFT", 8, -8);
	panel.editor:SetPoint("BOTTOMRIGHT", panel, "BOTTOMRIGHT", -12, 8);

	BuildHeader(panel.editor);
	header:SetPoint("TOPLEFT", panel.editor, "TOPLEFT", 0, 0);
	header:SetPoint("TOPRIGHT", panel.editor, "TOPRIGHT", 0, 0);

	BuildTabBar(panel.editor);
	tabBar:SetPoint("TOPLEFT", header, "BOTTOMLEFT", 0, -4);
	tabBar:SetPoint("TOPRIGHT", header, "BOTTOMRIGHT", 0, -4);

	local tabHolder = BuildMainTabs(panel.editor);
	tabHolder:SetPoint("TOPLEFT", tabBar, "BOTTOMLEFT", 0, -6);
	tabHolder:SetPoint("TOPRIGHT", tabBar, "BOTTOMRIGHT", 0, -6);

	content = CreateFrame("Frame", "GameEditorSpellContent", panel.editor);
	content:SetPoint("TOPLEFT", tabHolder, "BOTTOMLEFT", 0, -10);
	content:SetPoint("BOTTOMRIGHT", panel.editor, "BOTTOMRIGHT", 0, 0);
	emptyState = BuildEmptyState(content);

	BuildResizeGrip(panel);
	BuildStrip(root);

	SE.mainWidth = min(1180, AreaWidth() - LEFT_OFFSET - 120);
	ApplyLayout();
	EditorMainArea:HookScript("OnSizeChanged", function() ApplyLayout(); end);

	SE.BuildServerSections(BuildSection);

	if ( SE.cur ) then
		SE.Fire("SPELL_LOADED", SE.cur);
	end

	SE.SelectSection(SE.lastSection or "select");
	RefreshHeader();
	LayoutTabs();
	return root;
end

-- Refresh hooks --------------------------------------------------------------------------------------------

SE.On("REFRESH", function()
	if ( not header ) then
		return;
	end

	RefreshHeader();
	RefreshMainTabs();
	LayoutTabs();
	strip:Refresh();
end);

SE.On("SPELL_LOADED", function()
	if ( content and activeSection ) then
		SE.SelectSection(activeSection.key);
	end
end);

-- Menus, keys, status -------------------------------------------------------------------------------------

local function OpenById()
	SE.PickSpell("Open spell", function(id) SE.Open(id); end);
end

local function BuildMenus()
	local separator = { separator = true };
	local goTo = { title = "Go to" };
	SortSections();

	for _, section in ipairs(SE.SECTIONS) do
		local key = section.key;
		table.insert(goTo, {
			text = section.title,
			checked = function() return activeSection and activeSection.key == key; end,
			onClick = function() SE.SetCollapsed(false); SE.SelectSection(key); end,
		});
	end

	table.insert(goTo, separator);

	for _, section in ipairs(SE.SERVER_SECTIONS or {}) do
		local key = section.key;
		table.insert(goTo, {
			text = "Server: "..section.title,
			onClick = function() SE.SetCollapsed(false); SE.SelectSection("server"); SE.SelectServerSection(key); end,
		});
	end

	return {
		{
			title = "Spell",
			{ text = "New spell", shortcut = "Ctrl+N", onClick = function() SE.NewSpell(); end },
			{ text = "Copy to a new id", shortcut = "Ctrl+D", onClick = SE.CopySpell },
			{ text = "Open by id or name", shortcut = "Ctrl+O", onClick = OpenById },
			{ text = "Find in the spell list", shortcut = "Ctrl+F", onClick = function() SE.SetCollapsed(false); SE.FocusSearch(); end },
			separator,
			{ text = "Close tab", shortcut = "Ctrl+W", onClick = function() SE.CloseDoc(); end },
			{ text = "Close saved tabs", onClick = function() SE.CloseSavedDocs(); end },
			{ text = "Next tab", shortcut = "Ctrl+Tab", onClick = function() SE.CycleDoc(1); end },
			{ text = "Previous tab", shortcut = "Ctrl+Shift+Tab", onClick = function() SE.CycleDoc(-1); end },
			separator,
			{ text = "Save spell", onClick = SE.Save },
			{ text = "Save all tabs and changes", shortcut = "Ctrl+S", onClick = SE.SaveAll },
			{ text = "Revert spell", onClick = SE.Revert },
			{ text = "Apply locally", shortcut = "Ctrl+L", onClick = function() SE.ApplyLocal(); end },
			separator,
			{ text = "Preview on target", shortcut = "F5", onClick = function() SE.PreviewCast(); end },
			{ text = "Preview on self", shortcut = "Shift+F5", onClick = function() SE.PreviewCast("self"); end },
			{ text = "Stop preview", shortcut = "Ctrl+F5", onClick = SE.PreviewStop },
			separator,
			{ text = "Spell icon", onClick = function() SE.SetCollapsed(false); SE.SelectSection("icon"); end },
			{ text = "Spell missile", onClick = SE.OpenMissile },
			separator,
			{ text = "Collapse the editor", shortcut = "Ctrl+E", checked = function() return collapsed; end, onClick = function() SE.SetCollapsed(not collapsed); end },
		},
		goTo,
	};
end

local HOTKEYS = {
	["CTRL-S"] = function() SE.SaveAll(); end,
	["CTRL-N"] = function() SE.NewSpell(); end,
	["CTRL-D"] = function() SE.CopySpell(); end,
	["CTRL-O"] = OpenById,
	["CTRL-L"] = function() SE.ApplyLocal(); end,
	["CTRL-E"] = function() SE.SetCollapsed(not collapsed); end,
	["CTRL-F"] = function() SE.SetCollapsed(false); SE.FocusSearch(); end,
	["F5"] = function() SE.PreviewCast(); end,
	["SHIFT-F5"] = function() SE.PreviewCast("self"); end,
	["CTRL-F5"] = function() SE.PreviewStop(); end,
	["CTRL-W"] = function() SE.CloseDoc(); end,
	["CTRL-TAB"] = function() SE.CycleDoc(1); end,
	["CTRL-SHIFT-TAB"] = function() SE.CycleDoc(-1); end,
};

local KEYS = {
	{ "Ctrl+S", "Save every spell tab and every other unsaved row (visuals, server tables)" },
	{ "Ctrl+N", "New spell with the next free id, in a new tab" },
	{ "Ctrl+D", "Copy the open spell to a new id, in a new tab" },
	{ "Ctrl+O", "Open a spell by id or name in a tab" },
	{ "Ctrl+W", "Close the spell tab, asks first when it has unsaved changes" },
	{ "Ctrl+Tab", "Next spell tab" },
	{ "Ctrl+Shift+Tab", "Previous spell tab" },
	{ "Middle click a tab", "Close it, right click for more" },
	{ "$ in a description", "Lists the variables, Up, Down and Enter or a click inserts one" },
	{ "Ctrl+F", "Filter the spell list by name in the Select Spell tab" },
	{ "Ctrl+L", "Apply the spell to this client only" },
	{ "Ctrl+E", "Collapse or expand the editor panel" },
	{ "F5", "Preview the spell on the target" },
	{ "Shift+F5", "Preview the spell on yourself" },
	{ "Ctrl+F5", "Stop the preview" },
	{ "Right click a field", "Revert it, clear it or open the spell it points to" },
};

local function Status()
	local cells = {};
	local cur = SE.cur;

	if ( cur ) then
		table.insert(cells, format("Spell %d: %s", cur.id, tostring(cur.values["m_name[0]"] or "")));

		if ( SE.pendingSave[cur.id] ) then
			table.insert(cells, "saving");
		elseif ( cur.isNew ) then
			table.insert(cells, "new, not saved");
		else
			local changed = SE.ChangeCount();
			table.insert(cells, changed > 0 and format("%d unsaved", changed) or "saved");
		end

		local others = #SE.DirtyEditors();

		for _, doc in ipairs(SE.docs) do
			if ( doc ~= cur and SE.DocHasUnsaved(doc) ) then
				others = others + 1;
			end
		end

		if ( #SE.docs > 1 ) then
			table.insert(cells, format("%d tabs", #SE.docs));
		end

		if ( others > 0 ) then
			table.insert(cells, format("%d other unsaved", others));
		end
	else
		table.insert(cells, "No spell open");
	end

	local preview = SE.PreviewStatusText();

	if ( preview ) then
		table.insert(cells, preview);
	end

	return cells;
end

GameEditor_RefOpeners["spell"] = function(id)
	if ( GameEditor_GetWorkspace() ~= "spell" ) then
		GameEditor_SetWorkspace("spell");
	end

	SE.Open(id);
end;

GameEditor_RegisterWorkspace({
	key = "spell",
	title = "Spells",
	order = 4,
	dockWidth = 300,
	tools = {
		{
			key = "select",
			name = "Spell preview",
			icon = 0xF8A4,
			build = function(parent)
				return SE.CreatePreviewPanel(parent);
			end,
		},
	},
	menus = BuildMenus,
	keys = KEYS,
	buildMain = BuildMain,
	onLoad = function()
		SE.InitEvents();
		SE.RegisterResolvers();
	end,
	onActivate = function()
		SE.QueueRefresh();
	end,
	onDeactivate = function()
		HideHoverTooltip();

		if ( SE.preview.loop ) then
			SE.PreviewStop();
		end
	end,
	onUpdate = function(elapsed)
		SE.FlushRefresh();
		SE.UpdateWorldQueries(elapsed);
		SE.UpdatePreview(elapsed);
	end,
	onHotkey = function(key)
		if ( SE.TokenPopupKey and SE.TokenPopupKey(key) ) then
			return true;
		end

		local handler = HOTKEYS[key];

		if ( handler ) then
			handler();
			return true;
		end

		return false;
	end,
	status = Status,
});
