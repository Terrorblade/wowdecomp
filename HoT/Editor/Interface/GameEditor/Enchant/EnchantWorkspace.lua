-- Enchants workspace, edits SpellItemEnchantment rows, their gem conditions and the server proc row, and jumps to the spells they use.

local EN = GameEditorEnchant;
local ME = GameEditorModel;
local Common = GameEditorObject;
local Data = GameEditorData or {};

local TAB_NAMES = { "Enchant", "Condition", "Proc", "Used by" };
local TAB_ENCHANT, TAB_CONDITION, TAB_PROC, TAB_USED = 1, 2, 3, 4;
local INT_LOW, INT_HIGH = -2147483648, 2147483647;
local FORM_WIDTH = 470;

local Num = EN.Num;
local store = EN.store;

local frame;
local idEdit;
local tabs;
local container;
local pages = {};
local empty;
local statusText;
local saveButton;
local revertButton;

EN.enchantId = EN.enchantId or nil;
EN.conditionId = EN.conditionId or nil;

local procRow = GameEditor_CreateWorldRow("spell_enchant_proc_data", function()
	if ( EN.Refresh ) then
		EN.Refresh();
	end
end);

local function EnchantDoc()
	return EN.enchantId and store:Doc(EN.TABLE, EN.enchantId);
end

local function ConditionDoc()
	return EN.conditionId and store:Doc(EN.CONDITION, EN.conditionId);
end

local function DirtyCount()
	return #store:Dirty() + (procRow:IsDirty() and 1 or 0);
end

-- Opening ---------------------------------------------------------------------------------------------------

local function ShowTab(index)
	if ( tabs and tabs.current ~= index ) then
		tabs:Select(index);
	end
end

function EN.Open(id)
	id = tonumber(id);

	if ( not id or not store:Doc(EN.TABLE, id) ) then
		GameEditor_Notify(format("Enchant %s does not exist", tostring(id)), "error");
		return false;
	end

	EN.enchantId = id;
	EN.conditionId = Num(store:Doc(EN.TABLE, id).values.m_conditionID);
	procRow:Load(id);

	if ( GameEditor_GetWorkspace() ~= "enchant" ) then
		GameEditor_SetWorkspace("enchant");
	end

	if ( idEdit ) then
		idEdit:SetText(tostring(id));
		idEdit:ClearFocus();
	end

	store:Changed();
	return true;
end

function EN.OpenCondition(id)
	id = tonumber(id);

	if ( not id or not store:Doc(EN.CONDITION, id) ) then
		GameEditor_Notify(format("Enchant condition %s does not exist", tostring(id)), "error");
		return false;
	end

	EN.conditionId = id;

	if ( GameEditor_GetWorkspace() ~= "enchant" ) then
		GameEditor_SetWorkspace("enchant");
	end

	ShowTab(TAB_CONDITION);
	store:Changed();
	return true;
end

local function Browse()
	GameEditor_PickValue("Enchants", function() return GameEditor_DbcList(EN.TABLE); end, EN.enchantId, function(value)
		EN.Open(value);
	end);
end

local function OpenSpell(id)
	GameEditor_OpenRef("spell", id);
end

-- Editing ---------------------------------------------------------------------------------------------------

local function NewEnchant()
	local source = EnchantDoc();
	local doc = store:Create(EN.TABLE, source and source.values or { [EN.NameColumn()] = "New enchant" });
	EN.Open(doc.id);
	GameEditor_Notify(format("Enchant %d made, it is not on the server until you save", doc.id), "info");
end

local function NewCondition()
	local enchant = EnchantDoc();
	local source = ConditionDoc();
	local condition = store:Create(EN.CONDITION, source and source.values or { ["m_ltOperandType[0]"] = 1, ["m_operator[0]"] = 5, ["m_rtOperand[0]"] = 1 });
	EN.conditionId = condition.id;

	if ( enchant ) then
		store:SetMany(enchant, { m_conditionID = condition.id });
	end
end

local function Save()
	local rows = #store:Dirty();

	if ( rows == 0 and not procRow:IsDirty() ) then
		GameEditor_Notify("Nothing to save", "info");
		return;
	end

	if ( rows > 0 ) then
		store:SaveAll();
	end

	procRow:Save();
end

local function Revert()
	if ( procRow:IsDirty() ) then
		procRow:Revert();
	end

	if ( #store:Dirty() > 0 ) then
		store:ConfirmRevert(function()
			if ( EN.enchantId and not EnchantDoc() ) then
				EN.enchantId = nil;
			end
		end);
	end
end

-- Pages ---------------------------------------------------------------------------------------------------------

local function NewPage()
	local page = CreateFrame("Frame", NoggitUI_NewName("GameEditorEnchantPage"), container);
	page:SetAllPoints(container);
	page:Hide();
	return page;
end

local function PageScroll(page, top)
	page.scroll, page.child = GameEditor_CreateScrollFrame(page);
	page.scroll:SetPoint("TOPLEFT", page, "TOPLEFT", 0, -top);
	page.scroll:SetPoint("BOTTOMRIGHT", page, "BOTTOMRIGHT", 0, 0);
	page.child:SetWidth(FORM_WIDTH + 10);
	page.child:SetHeight(10);
end

local function FitScroll(page, height)
	page.child:SetHeight(height + 10);
	page.child:SetWidth(max(380, (page.scroll:GetWidth() or 480) - 14));
	page.scroll.UpdateRange();
end

local function EffectInfo(data, slot)
	return EN.EFFECT_TYPES[Num(data and data[EN.Column("effect", slot)])];
end

local function EffectTypeItems()
	local items = {};

	for value = 0, #EN.EFFECT_TYPES do
		table.insert(items, { value = value, text = format("%d  %s", value, EN.EFFECT_TYPES[value].name) });
	end

	return items;
end

local function BuildEnchantPage()
	local page = NewPage();
	PageScroll(page, 0);

	local form = ME.CreateForm(page.child, FORM_WIDTH, function(fields)
		local doc = EnchantDoc();

		if ( doc ) then
			if ( fields.m_conditionID ) then
				EN.conditionId = Num(fields.m_conditionID);
			end

			store:SetMany(doc, fields);
		end
	end);

	form:Header("Enchant");
	form:Text(EN.NameColumn(), "Name", "Shown on the item, the client locale only");
	form:Flags("m_flags", "Flags", EN.FLAGS, 32);
	form:Int("m_charges", "Charges", 0, INT_HIGH, "Charges the enchant starts with, 0 never runs out");
	form:Int("m_itemVisual", "Item visual", 0, INT_HIGH, "ItemVisuals id, the glow on the weapon");
	form:Ref("m_srcItemID", "Source item", "item", "The gem that puts this enchant in a socket, the condition is only checked for gems");
	form:Ref("m_conditionID", "Condition", "dbc:SpellItemEnchantmentCondition", "Gem colors a meta gem needs, see the Condition tab");
	form:Ref("m_requiredSkillID", "Required skill", "dbc:SkillLine", "Skill the player needs for the enchant to work");
	form:Int("m_requiredSkillRank", "Skill rank", 0, INT_HIGH);
	form:Int("m_minLevel", "Min level", 0, 255, "Player level the enchant needs to work");

	for slot = 0, EN.EFFECTS - 1 do
		local argColumn = EN.Column("effectArg", slot);
		local function ArgIs(kind)
			return function(data)
				local info = EffectInfo(data, slot);
				return info ~= nil and (info.argRef == kind or info.argEnum == kind);
			end;
		end

		form:Header(format("Effect %d", slot + 1));
		form:Combo(EN.Column("effect", slot), "Type", EffectTypeItems(), "What the effect does", 220);

		local spell = form:Ref(argColumn, "Spell", "spell", "The spell of this effect");
		spell.visible = ArgIs("spell");
		local resistance = form:Combo(argColumn, "School", EN.ResistanceItems(), "Resistance that goes up, 0 is armor", 220);
		resistance.visible = ArgIs("resistance");
		local stat = form:Combo(argColumn, "Stat", EN.StatItems(), "ItemModType of the stat", 220);
		stat.visible = ArgIs("stat");
		local arg = form:Int(argColumn, "Argument", INT_LOW, INT_HIGH, "Not read for this effect type");
		arg.visible = function(data)
			local info = EffectInfo(data, slot);
			return (not info or not (info.argRef or info.argEnum)) and Num(data and data[argColumn]) ~= 0;
		end;

		local points = form:Int(EN.Column("effectPointsMin", slot), "Points", INT_LOW, INT_HIGH);
		points.visible = function(data) return Num(data and data[EN.Column("effect", slot)]) ~= 0; end;
		points.hintFunc = function(_, data)
			local info = EffectInfo(data, slot);
			return info and (info.pointsTip or info.points) or "";
		end;

		local maxPoints = form:Int(EN.Column("effectPointsMax", slot), "Points max", INT_LOW, INT_HIGH, "Only the client tooltip reads it");
		maxPoints.visible = points.visible;
	end

	page.Refresh = function(self)
		local doc = EnchantDoc();
		FitScroll(self, form:Refresh(doc and doc.values));
	end;

	return page;
end

local function BuildConditionPage()
	local page = NewPage();

	local bar = CreateFrame("Frame", NoggitUI_NewName("GameEditorEnchantConditionBar"), page);
	bar:SetHeight(22);
	bar:SetPoint("TOPLEFT", page, "TOPLEFT", 0, 0);
	bar:SetPoint("TOPRIGHT", page, "TOPRIGHT", 0, 0);

	local label = Common.Label(bar, "Condition id");
	label:SetPoint("LEFT", bar, "LEFT", 0, 0);
	page.idSpin = ME.Spin(bar, 80, 0, INT_HIGH, 1, 0, function(value)
		value = floor(Num(value));

		if ( value > 0 and not store:Doc(EN.CONDITION, value) ) then
			GameEditor_Notify(format("Enchant condition %d does not exist", value), "error");
			page:Refresh();
			return;
		end

		EN.conditionId = value;
		local enchant = EnchantDoc();

		if ( enchant ) then
			store:SetMany(enchant, { m_conditionID = value });
		else
			store:Changed();
		end
	end, "SpellItemEnchantmentCondition row of the open enchant");
	page.idSpin:SetPoint("LEFT", label, "RIGHT", 6, 0);

	local new = Common.Button(bar, "New", 50, NewCondition, "Make a new condition row for this enchant, a copy of the shown one when there is one");
	new:SetPoint("LEFT", page.idSpin, "RIGHT", 6, 0);
	local clear = Common.Button(bar, "Clear", 50, function()
		local enchant = EnchantDoc();
		EN.conditionId = 0;

		if ( enchant ) then
			store:SetMany(enchant, { m_conditionID = 0 });
		end
	end, "The enchant needs no gem colors, the condition row itself stays");
	clear:SetPoint("LEFT", new, "RIGHT", 4, 0);

	PageScroll(page, 28);

	local form = ME.CreateForm(page.child, FORM_WIDTH, function(fields)
		local doc = ConditionDoc();

		if ( doc ) then
			store:SetMany(doc, fields);
		end
	end);

	form:Note(function(data) return data and ("Requires "..EN.ConditionText(data)) or ""; end, 34);

	local colors = EN.EnumItems(EN.GEM_COLORS);
	local rightTypes = EN.EnumItems(EN.GEM_COLORS);
	rightTypes[1] = { value = 0, text = "0  A number" };

	for slot = 0, EN.TERMS - 1 do
		local rightType = EN.Column("rtOperandType", slot);
		form:Header(format("Term %d", slot + 1));
		form:Combo(EN.Column("ltOperandType", slot), "Gem color", colors, "Gems of this color are counted, None skips the term", 180);
		form:Combo(EN.Column("operator", slot), "Must be", EN.EnumItems(EN.OPERATORS), "The server only checks less than, more than and at least", 180);
		form:Combo(rightType, "Compared to", rightTypes, "A number, or the count of gems of another color", 180);
		local value = form:Int(EN.Column("rtOperand", slot), "Number", 0, 255);
		value.visible = function(data) return Num(data and data[rightType]) == 0; end;
		form:Combo(EN.Column("logic", slot), "Then", { { value = 0, text = "0  Stop here" }, { value = 1, text = "1  And the next term" } }, "The client tooltip stops at the first term set to stop", 180);
	end

	page.empty = Common.Paragraph(page.child, "");
	page.empty:SetPoint("TOPLEFT", page.child, "TOPLEFT", 0, -4);
	page.empty:SetPoint("TOPRIGHT", page.child, "TOPRIGHT", 0, -4);

	page.Refresh = function(self)
		local doc = ConditionDoc();
		ME.SetSpin(self.idSpin, EN.conditionId or 0, true);

		if ( doc ) then
			self.empty:Hide();
			FitScroll(self, form:Refresh(doc.values));
		else
			form:Refresh(nil);

			for _, item in ipairs(form.items) do
				item.frame:Hide();
			end

			self.empty:SetText(Num(EN.conditionId) > 0 and format("Condition %d does not exist.", EN.conditionId) or "No condition, the enchant works with any gems. New makes one.");
			self.empty:Show();
			FitScroll(self, 40);
		end
	end;

	return page;
end

local function BuildProcPage()
	local page = NewPage();

	local intro = Common.Paragraph(page, "spell_enchant_proc_data on the server, it decides how often the combat spell of this enchant procs.");
	intro:SetPoint("TOPLEFT", page, "TOPLEFT", 2, -2);
	intro:SetPoint("TOPRIGHT", page, "TOPRIGHT", -2, -2);

	local bar = CreateFrame("Frame", NoggitUI_NewName("GameEditorEnchantProcBar"), page);
	bar:SetHeight(22);
	bar:SetPoint("TOPLEFT", page, "TOPLEFT", 0, -40);
	bar:SetPoint("TOPRIGHT", page, "TOPRIGHT", 0, -40);

	page.add = Common.Button(bar, "Add proc row", 100, function()
		procRow:Create({ Chance = 0, ProcsPerMinute = 0 });
	end, "Give the enchant its own proc chance or procs per minute");
	page.add:SetPoint("LEFT", bar, "LEFT", 0, 0);
	page.remove = Common.Button(bar, "Remove", 70, function() procRow:Remove(); end, "Delete the proc row when you save");
	page.remove:SetPoint("LEFT", page.add, "RIGHT", 4, 0);
	page.state = Common.Label(bar, "", nil, NOGGIT_COLOR_DISABLED);
	page.state:SetPoint("LEFT", page.remove, "RIGHT", 8, 0);
	page.state:SetPoint("RIGHT", bar, "RIGHT", 0, 0);

	local columns = {};

	for _, column in ipairs(Data.Columns and Data.Columns.spell_enchant_proc_data or {}) do
		if ( column ~= "EnchantID" ) then
			table.insert(columns, column);
		end
	end

	page.grid = GameEditor_CreatePropertyGrid(page, {
		table = "spell_enchant_proc_data",
		columns = columns,
		labelWidth = 150,
		onChange = function(_, column, value)
			procRow:Set(column, value);
		end,
	});
	page.grid.frame:SetPoint("TOPLEFT", bar, "BOTTOMLEFT", 0, -6);
	page.grid.frame:SetPoint("BOTTOMRIGHT", page, "BOTTOMRIGHT", 0, 0);

	page.Refresh = function(self)
		local exists = procRow:Exists();
		ME.SetEnabled(self.add, procRow.loaded and not exists);
		ME.SetEnabled(self.remove, exists);

		if ( procRow.error ) then
			self.state:SetText("|cffff6060"..procRow.error.."|r");
		elseif ( not procRow.loaded ) then
			self.state:SetText(EN.enchantId and "Loading..." or "");
		elseif ( not exists ) then
			self.state:SetText("No row, the enchant uses its own chance");
		else
			self.state:SetText(procRow:IsDirty() and "|cffffcc00Not saved|r" or "");
		end

		if ( exists ) then
			self.grid.frame:Show();
			self.grid:SetValues(procRow.values, procRow.original ~= nil and procRow:IsDirty());
		else
			self.grid.frame:Hide();
		end
	end;

	return page;
end

local function BuildUsedPage()
	local page = NewPage();

	local intro = Common.Paragraph(page, "Spells that put this enchant on an item and gems that socket it. Double click a row to open it.");
	intro:SetPoint("TOPLEFT", page, "TOPLEFT", 2, -2);
	intro:SetPoint("TOPRIGHT", page, "TOPRIGHT", -2, -2);

	page.list = NoggitListBox_Create(page, 20, function(row)
		row.kind = NoggitUI_FontString(row, "");
		row.kind:SetPoint("LEFT", row, "LEFT", 4, 0);
		row.kind:SetWidth(90);
		NoggitText_SetColor(row.kind, NOGGIT_COLOR_DISABLED);
		row.text = NoggitUI_FontString(row, "");
		row.text:SetPoint("LEFT", row, "LEFT", 98, 0);
		row.text:SetPoint("RIGHT", row, "RIGHT", -4, 0);
	end, function(row, item)
		row.kind:SetText(item.kind);
		row.text:SetText(item.text);
	end);
	page.list:SetPoint("TOPLEFT", page, "TOPLEFT", 0, -24);
	page.list:SetPoint("BOTTOMRIGHT", page, "BOTTOMRIGHT", 0, 0);
	page.list.onDoubleClick = function(item)
		if ( item.open ) then
			item.open();
		end
	end;

	page.Refresh = function(self)
		if ( self.shownFor == EN.enchantId ) then
			return;
		end

		self.shownFor = EN.enchantId;
		local items = {};

		if ( EN.enchantId ) then
			for _, spell in ipairs(EN.SpellsUsing(EN.enchantId)) do
				table.insert(items, {
					kind = "Spell",
					text = format("%d  %s  |cff808080%s|r", spell.id, spell.name or "", EN.SPELL_EFFECTS[spell.effect]),
					open = function() OpenSpell(spell.id); end,
				});
			end

			for _, gem in ipairs(EN.GemsUsing(EN.enchantId)) do
				table.insert(items, { kind = "Gem property", text = format("%d", gem.id) });
			end

			local source = Num(EnchantDoc() and EnchantDoc().values.m_srcItemID);

			if ( source > 0 ) then
				table.insert(items, {
					kind = "Source item",
					text = format("%d  %s", source, GameEditor_ResolveRef("item", source) or ""),
					open = function() GameEditor_OpenRef("item", source); end,
				});
			end
		end

		if ( #items == 0 ) then
			table.insert(items, { kind = "", text = "|cff808080Nothing uses this enchant|r" });
		end

		NoggitListBox_SetItems(self.list, items);
	end;

	return page;
end

-- Refresh ------------------------------------------------------------------------------------------------------

local function RefreshStatus()
	local dirty = DirtyCount();

	if ( dirty > 0 ) then
		statusText:SetText(format("%d unsaved row%s", dirty, dirty == 1 and "" or "s"));
		NoggitText_SetColor(statusText, NOGGIT_COLOR_GOLD);
	else
		local doc = EnchantDoc();
		statusText:SetText(doc and format("Enchant %d  %s", doc.id, doc.values[EN.NameColumn()] or "") or "Open an enchant to edit it");
		NoggitText_SetColor(statusText, NOGGIT_COLOR_DISABLED);
	end

	ME.SetEnabled(saveButton, dirty > 0);
	ME.SetEnabled(revertButton, dirty > 0);
end

function EN.Refresh()
	if ( not frame or not frame:IsShown() ) then
		return;
	end

	RefreshStatus();

	if ( not EnchantDoc() and tabs.current ~= TAB_CONDITION ) then
		container:Hide();
		empty:Set({
			title = "No enchant open",
			text = "Type a SpellItemEnchantment id above or browse them by name.",
			primary = "Browse enchants", onPrimary = Browse,
			secondary = "New enchant", onSecondary = NewEnchant,
			help = "Edits apply to this client at once and reach the server when you save. Spell ids have a Go button that opens them in the spell editor.",
		});
		return;
	end

	empty:Hide();
	container:Show();

	local page = pages[tabs.current];

	if ( page and page.Refresh ) then
		page:Refresh();
	end
end

store:OnChanged(EN.Refresh);

local function ShowPage(index)
	for key, page in pairs(pages) do
		if ( key == index ) then
			page:Show();
		else
			page:Hide();
		end
	end

	EN.Refresh();
end

-- Build ----------------------------------------------------------------------------------------------------------

local function Build(parent)
	frame = CreateFrame("Frame", "GameEditorEnchantPanel", parent);

	local row = CreateFrame("Frame", NoggitUI_NewName("GameEditorEnchantRow"), frame);
	row:SetHeight(22);
	row:SetPoint("TOPLEFT", frame, "TOPLEFT", 6, -6);
	row:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -6, -6);

	local label = Common.Label(row, "Enchant");
	label:SetPoint("LEFT", row, "LEFT", 0, 0);
	idEdit = NoggitLineEdit_Create(row, 70);
	idEdit:SetHeight(22);
	idEdit:SetMaxLetters(10);
	idEdit:SetPoint("LEFT", label, "RIGHT", 6, 0);
	idEdit:SetScript("OnEnterPressed", function(self) EN.Open(tonumber(strtrim(self:GetText() or ""))); end);
	idEdit:SetScript("OnEscapePressed", function(self) self:ClearFocus(); end);
	Common.Tooltip(idEdit, "SpellItemEnchantment id, Enter opens it");

	local browse = Common.Button(row, "Browse", 60, Browse, "Search the enchants by name");
	browse:SetPoint("LEFT", idEdit, "RIGHT", 4, 0);
	local new = Common.Button(row, "New", 46, NewEnchant, "Make a new enchant, a copy of the open one when there is one");
	new:SetPoint("LEFT", browse, "RIGHT", 4, 0);

	local bar = CreateFrame("Frame", NoggitUI_NewName("GameEditorEnchantBar"), frame);
	bar:SetHeight(24);
	bar:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 6, 6);
	bar:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -6, 6);

	saveButton = Common.Button(bar, "Save", 70, Save, "Send the changed enchant, condition and proc rows to the server (Ctrl+S)");
	saveButton:SetPoint("RIGHT", bar, "RIGHT", 0, 0);
	revertButton = Common.Button(bar, "Revert", 60, Revert, "Throw away the unsaved changes");
	revertButton:SetPoint("RIGHT", saveButton, "LEFT", -4, 0);
	statusText = Common.Label(bar, "", nil, NOGGIT_COLOR_DISABLED);
	statusText:SetPoint("LEFT", bar, "LEFT", 2, 0);
	statusText:SetPoint("RIGHT", revertButton, "LEFT", -4, 0);

	tabs = Common.CreateTabs(frame, TAB_NAMES, ShowPage);
	tabs.tabs[1]:SetPoint("TOPLEFT", row, "BOTTOMLEFT", 0, -8);

	container = CreateFrame("Frame", NoggitUI_NewName("GameEditorEnchantPages"), frame);
	container:SetPoint("TOPLEFT", tabs.tabs[1], "BOTTOMLEFT", 0, -6);
	container:SetPoint("RIGHT", frame, "RIGHT", -6, 0);
	container:SetPoint("BOTTOM", bar, "TOP", 0, 6);

	empty = Common.CreateEmptyState(frame);
	empty:ClearAllPoints();
	empty:SetPoint("TOPLEFT", tabs.tabs[1], "BOTTOMLEFT", -6, -6);
	empty:SetPoint("BOTTOMRIGHT", bar, "TOPRIGHT", 6, 6);

	pages[TAB_ENCHANT] = BuildEnchantPage();
	pages[TAB_CONDITION] = BuildConditionPage();
	pages[TAB_PROC] = BuildProcPage();
	pages[TAB_USED] = BuildUsedPage();

	frame:SetScript("OnShow", function() ShowPage(tabs.current); end);

	if ( EN.enchantId ) then
		idEdit:SetText(tostring(EN.enchantId));
	end

	ShowPage(tabs.current);
	return frame;
end

-- Refs -------------------------------------------------------------------------------------------------------------

GameEditor_RefOpeners["dbc:SpellItemEnchantment"] = EN.Open;
GameEditor_RefOpeners["dbc:SpellItemEnchantmentCondition"] = EN.OpenCondition;

GameEditor_RefResolvers["dbc:SpellItemEnchantmentCondition"] = function(value)
	local row = GameEditor_DbcRow(EN.CONDITION, value);
	return row and EN.ConditionText(row) or nil;
end;

-- Workspace --------------------------------------------------------------------------------------------------------

local separator = { separator = true };

local function BuildMenus()
	return {
		{
			title = "Enchant",
			{ text = "Browse enchants...", onClick = Browse },
			{ text = "New enchant", onClick = NewEnchant },
			{ text = "New condition", onClick = NewCondition },
			separator,
			{ text = "Save", shortcut = "Ctrl+S", onClick = Save },
			{ text = "Revert", onClick = Revert },
		},
	};
end

local function BuildStatus()
	local cells = { EN.enchantId and format("Enchant %d", EN.enchantId) or "No enchant open" };
	local dirty = DirtyCount();

	if ( dirty > 0 ) then
		table.insert(cells, format("|cffffcc00%d unsaved|r", dirty));
	end

	return cells;
end

GameEditor_RegisterWorkspace({
	key = "enchant",
	title = "Enchants",
	category = "Items",
	order = 7,
	dockWidth = 520,
	tools = {
		{ key = "select", name = "Enchants", icon = 0xF8A4, build = Build },
	},
	menus = BuildMenus,
	keys = { { "Ctrl+S", "Save the changed enchant, condition and proc rows" } },
	onActivate = EN.Refresh,
	onHotkey = function(key)
		if ( key == "CTRL-S" ) then
			Save();
			return true;
		end

		return false;
	end,
	status = BuildStatus,
});
