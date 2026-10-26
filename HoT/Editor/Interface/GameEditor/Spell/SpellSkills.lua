-- Skills tab: the SkillLineAbility rows that put the open spell in a skill line, with a browser of every SkillLine by category.

local SE = GameEditorSpell;
local Data = GameEditorData or {};

local TABLE = "SkillLineAbility";
local LIST_WIDTH = 330;
local FORM_WIDTH = 470;
local INT_HIGH = 2147483647;

local Num = SE.Number;

local store = GameEditor_CreateDbcDocs("Spell skills", { [TABLE] = true });
SE.skillStore = store;

local ACQUIRE_METHODS = {
	{ value = 0, text = "0  Trainer, quest or item" },
	{ value = 1, text = "1  At the min skill value" },
	{ value = 2, text = "2  Together with the skill" },
};

local ACQUIRE_SHORT = { [0] = "taught", [1] = "at min skill", [2] = "with the skill" };

local CLASS_FALLBACK = {
	{ bit = 0, name = "Warrior" },
	{ bit = 1, name = "Paladin" },
	{ bit = 2, name = "Hunter" },
	{ bit = 3, name = "Rogue" },
	{ bit = 4, name = "Priest" },
	{ bit = 5, name = "Death Knight" },
	{ bit = 6, name = "Shaman" },
	{ bit = 7, name = "Mage" },
	{ bit = 8, name = "Warlock" },
	{ bit = 10, name = "Druid" },
};

-- Names --------------------------------------------------------------------------------------------------

local function LocaleColumn(tableName, member)
	if ( not DbcEditor_GetSchema ) then
		return nil;
	end

	local ok, schema = pcall(DbcEditor_GetSchema, tableName);

	for _, column in ipairs(ok and type(schema) == "table" and schema or {}) do
		if ( column.stored and string.find(column.name or "", "^"..member.."%[") ) then
			return column.name;
		end
	end
end

local columns = {};

local function NameColumn(tableName, member)
	local key = tableName.."."..member;

	if ( columns[key] == nil ) then
		columns[key] = LocaleColumn(tableName, member) or false;
	end

	return columns[key] or nil;
end

local cache = {};

-- SkillLineCategory rows by id, rebuilt whenever the client list of the table changes.
local function Categories()
	local list = GameEditor_DbcList("SkillLineCategory");

	if ( cache.categorySource ~= list ) then
		local categories = {};

		for _, item in ipairs(list) do
			local row = GameEditor_DbcRow("SkillLineCategory", item.value);
			categories[item.value] = { name = (item.text and item.text ~= "") and item.text or format("Category %d", item.value), sort = Num(row and row.m_sortIndex) };
		end

		cache.categorySource = list;
		cache.categories = categories;
		cache.items = nil;
	end

	return cache.categories;
end

local function SkillText(name, category)
	name = (name and name ~= "") and name or "(no name)";
	return category and format("%s  [%s]", name, category.name) or name;
end

-- "Fire  [Class Skills]" for a SkillLine id, nil when the client has no such row.
function SE.SkillLineText(id)
	local row = GameEditor_DbcRow("SkillLine", id);

	if ( not row ) then
		return nil;
	end

	local column = NameColumn("SkillLine", "m_displayName");
	return SkillText(column and row[column], Categories()[Num(row.m_categoryID)]);
end

-- Every SkillLine as picker items, grouped by category in the category sort order.
function SE.SkillLineItems()
	local categories = Categories();
	local list = GameEditor_DbcList("SkillLine");

	if ( cache.items and cache.source == list ) then
		return cache.items;
	end

	local items = {};

	for _, item in ipairs(list) do
		local row = GameEditor_DbcRow("SkillLine", item.value);
		local categoryId = Num(row and row.m_categoryID);
		local category = categories[categoryId];
		table.insert(items, {
			value = item.value,
			text = SkillText(item.text, category),
			sortName = string.lower(item.text or ""),
			sortIndex = category and category.sort or 100000,
			category = categoryId,
		});
	end

	table.sort(items, function(a, b)
		if ( a.sortIndex ~= b.sortIndex ) then
			return a.sortIndex < b.sortIndex;
		end

		if ( a.category ~= b.category ) then
			return a.category < b.category;
		end

		if ( a.sortName ~= b.sortName ) then
			return a.sortName < b.sortName;
		end

		return a.value < b.value;
	end);

	cache.source = list;
	cache.items = items;
	return items;
end

GameEditor_RefResolvers["dbc:SkillLine"] = function(value)
	return SE.SkillLineText(value);
end;

GameEditor_RefLists["dbc:SkillLine"] = SE.SkillLineItems;

local raceFlags;
local classFlags;

-- Race bits from ChrRaces, bit = race id - 1.
local function RaceFlags()
	if ( raceFlags ) then
		return raceFlags;
	end

	local column = NameColumn("ChrRaces", "m_name");
	local flags = {};

	for _, item in ipairs(GameEditor_DbcList("ChrRaces")) do
		local id = Num(item.value);

		if ( id >= 1 and id <= 32 ) then
			local row = column and GameEditor_DbcRow("ChrRaces", id);
			local name = row and row[column];
			table.insert(flags, { bit = id - 1, name = (name and name ~= "") and name or format("Race %d", id), tooltip = format("ChrRaces %d", id) });
		end
	end

	table.sort(flags, function(a, b) return a.bit < b.bit; end);

	if ( #flags == 0 ) then
		return Data.Flags and Data.Flags.RaceMask or {};
	end

	raceFlags = flags;
	return flags;
end

local function ClassFlags()
	classFlags = classFlags or (Data.Flags and Data.Flags.ItemClassMask) or CLASS_FALLBACK;
	return classFlags;
end

local function MaskText(value, flags, every, noun)
	value = Num(value);

	if ( value == 0 ) then
		return every;
	end

	local names = {};

	for _, flag in ipairs(flags) do
		if ( GameEditor_HasBit(value, flag.bit) ) then
			table.insert(names, flag.name);
		end
	end

	if ( #names == 0 ) then
		return format("%s mask %d", noun, value);
	end

	if ( #names > 2 ) then
		return format("%d %ss", #names, noun);
	end

	return table.concat(names, ", ");
end

-- Rows ---------------------------------------------------------------------------------------------------

local function BlankValues()
	local values = {};

	for _, name in ipairs(SE.Schema(TABLE).columns) do
		if ( name ~= "m_id" ) then
			values[name] = 0;
		end
	end

	return values;
end

-- A row that belonged to the spell and was blanked by Remove, it stays listed until it is saved.
local function IsRemoved(doc, spellId)
	spellId = spellId or SE.CurrentId();
	return doc ~= nil and Num(doc.values.m_spell) ~= spellId and Num(doc.original.m_spell) == spellId;
end

-- Documents of the rows whose spell is spellId, plus its removed rows that are not saved yet.
local function RowsOf(spellId)
	local rows = {};
	local seen = {};

	if ( not spellId ) then
		return rows;
	end

	for _, entry in ipairs(SE.Filter(TABLE, "", { { columns = { "m_spell" }, value = spellId } })) do
		local doc = store:Doc(TABLE, entry.id);

		if ( doc and not seen[doc.id] ) then
			seen[doc.id] = true;
			table.insert(rows, doc);
		end
	end

	for _, doc in ipairs(store:Dirty()) do
		if ( not seen[doc.id] and (Num(doc.values.m_spell) == spellId or (not doc.isNew and Num(doc.original.m_spell) == spellId)) ) then
			seen[doc.id] = true;
			table.insert(rows, doc);
		end
	end

	table.sort(rows, function(a, b) return a.id < b.id; end);
	return rows;
end

local function SpellDirtyCount(spellId)
	local count = 0;

	for _, doc in ipairs(store:Dirty()) do
		if ( Num(doc.values.m_spell) == spellId or Num(doc.original.m_spell) == spellId ) then
			count = count + 1;
		end
	end

	return count;
end

local function NewIds()
	local ids = {};

	for _, doc in ipairs(store:Dirty()) do
		if ( doc.isNew ) then
			table.insert(ids, doc.id);
		end
	end

	return ids;
end

-- Rows made and then thrown away stay in the client store, blanking them keeps them off every spell.
local function BlankLocal(ids)
	if ( #ids == 0 or not DbcEditor_ApplyLocal ) then
		return;
	end

	local values = BlankValues();

	for _, id in ipairs(ids) do
		pcall(DbcEditor_ApplyLocal, TABLE, id, values);
	end

	GameEditor_InvalidateDbcList(TABLE);
end

local function RevertAll()
	local ids = NewIds();
	store:ConfirmRevert(function()
		BlankLocal(ids);
		store:Changed();
	end);
end

local editor = {};

function editor:IsDirty()
	return #store:Dirty() > 0;
end

function editor:Save()
	store:SaveAll();
end

function editor:DirtyText()
	local count = #store:Dirty();
	return format("%d skill line row%s not saved", count, count == 1 and "" or "s");
end

function editor:Discard()
	local ids = NewIds();
	store:RevertAll();
	BlankLocal(ids);
	store:Changed();
end

SE.RegisterEditor(editor);

-- Tab ------------------------------------------------------------------------------------------------------

local frame;
local form;
local selectedId;

local function SelectedDoc()
	return selectedId and store:Find(TABLE, selectedId) or nil;
end

local function DocState(doc, spellId)
	if ( doc.saving ) then
		return "|cffffcc00saving|r";
	elseif ( IsRemoved(doc, spellId) ) then
		return "|cffff6060removed, not saved|r";
	elseif ( doc.isNew ) then
		return "|cffffcc00new, not saved|r";
	elseif ( store:IsDirty(doc) ) then
		return "|cffffcc00not saved|r";
	end
end

local function RowItem(doc, spellId)
	local removed = IsRemoved(doc, spellId);
	local values = removed and doc.original or doc.values;
	local skill = Num(values.m_skillLine);
	local title = SE.SkillLineText(skill) or format("|cffff6060Skill line %d does not exist|r", skill);
	local parts = {
		format("Row %d", doc.id),
		ACQUIRE_SHORT[Num(values.m_acquireMethod)] or format("acquire %d", Num(values.m_acquireMethod)),
		MaskText(values.m_raceMask, RaceFlags(), "every race", "race"),
		MaskText(values.m_classMask, ClassFlags(), "every class", "class"),
	};
	local state = DocState(doc, spellId);

	if ( state ) then
		table.insert(parts, 1, state);
	end

	return { doc = doc, title = removed and ("|cffff6060Removed|r  "..title) or title, detail = table.concat(parts, "  |  ") };
end

local function UpdateButtons(doc)
	local dirty = #store:Dirty();
	SE.SetEnabled(frame.remove, doc ~= nil);
	frame.remove:SetText(IsRemoved(doc) and "Restore" or "Remove");
	SE.SetEnabled(frame.save, dirty > 0);
	SE.SetEnabled(frame.revert, dirty > 0);

	if ( dirty > 0 ) then
		frame.state:SetText(format("|cffffcc00%d unsaved skill line row%s|r", dirty, dirty == 1 and "" or "s"));
	else
		frame.state:SetText("SkillLineAbility rows, saved to the dbc database and pushed live");
	end
end

local function RefreshForm()
	local doc = SelectedDoc();
	UpdateButtons(doc);

	if ( not doc ) then
		frame.holder:Hide();
		frame.placeholder:SetText("");
		return;
	end

	if ( IsRemoved(doc) ) then
		frame.holder:Hide();
		frame.placeholder:SetText(format("Row %d is removed. Saving blanks it in the dbc database (spell and skill line 0), Restore brings it back.", doc.id));
		return;
	end

	frame.placeholder:SetText("");
	frame.holder:Show();
	local height = form:Refresh(doc.values);
	frame.child:SetHeight(height + 10);
	frame.child:SetWidth(max(380, (frame.scroll:GetWidth() or FORM_WIDTH) - 14));
	frame.scroll.UpdateRange();
end

local function Refresh()
	if ( not frame or not frame:IsVisible() ) then
		return;
	end

	local spellId = SE.CurrentId();
	local docs = RowsOf(spellId);
	local items = {};
	local selectedIndex;

	for index, doc in ipairs(docs) do
		table.insert(items, RowItem(doc, spellId));

		if ( doc.id == selectedId ) then
			selectedIndex = index;
		end
	end

	if ( not selectedIndex ) then
		selectedIndex = #items > 0 and 1 or nil;
		selectedId = selectedIndex and docs[1].id or nil;
	end

	NoggitListBox_SetItems(frame.list, items, true);
	frame.list.selected = selectedIndex;
	NoggitListBox_Refresh(frame.list);

	if ( #items == 0 ) then
		frame.empty:Show();
	else
		frame.empty:Hide();
	end

	RefreshForm();
end

SE.RefreshSkills = Refresh;

local function AddSkill(skillLine)
	local spellId = SE.CurrentId();

	if ( not spellId ) then
		return;
	end

	for _, doc in ipairs(RowsOf(spellId)) do
		if ( not IsRemoved(doc, spellId) and Num(doc.values.m_skillLine) == skillLine ) then
			selectedId = doc.id;
			Refresh();
			GameEditor_Notify(format("Spell %d is already in %s", spellId, SE.SkillLineText(skillLine) or tostring(skillLine)), "info");
			return;
		end
	end

	local values = SE.FullRow(TABLE, {});
	values.m_skillLine = skillLine;
	values.m_spell = spellId;
	local doc = store:Create(TABLE, values);
	selectedId = doc.id;
	Refresh();
	GameEditor_Notify(format("SkillLineAbility %d added, Save writes it to the dbc database", doc.id), "info");
end

local function Add()
	if ( SE.cur ) then
		GameEditor_PickValue("Add the spell to a skill line", SE.SkillLineItems, nil, AddSkill);
	end
end

local function ChangeSkill(doc)
	if ( doc and not IsRemoved(doc) ) then
		GameEditor_PickValue("Skill line", SE.SkillLineItems, Num(doc.values.m_skillLine), function(value)
			store:SetMany(doc, { m_skillLine = value });
		end);
	end
end

local function RemoveSelected()
	local doc = SelectedDoc();

	if ( not doc ) then
		return;
	end

	if ( IsRemoved(doc) ) then
		local values = {};

		for column, value in pairs(doc.original) do
			if ( column ~= "m_id" ) then
				values[column] = value;
			end
		end

		store:SetMany(doc, values);
	elseif ( doc.isNew and not doc.saving ) then
		selectedId = nil;
		store:SetMany(doc, BlankValues());
		store:Drop(doc);
	else
		store:SetMany(doc, BlankValues());
	end
end

local function SkillNote(data)
	local skill = Num(data and data.m_skillLine);
	local row = skill ~= 0 and GameEditor_DbcRow("SkillLine", skill);

	if ( not row ) then
		return skill ~= 0 and "|cffff6060There is no such skill line in this client|r" or "|cffff6060Pick a skill line|r";
	end

	local category = Categories()[Num(row.m_categoryID)];
	local column = NameColumn("SkillLine", "m_description");
	local text = column and tostring(row[column] or "") or "";

	if ( string.len(text) > 90 ) then
		text = string.gsub(string.sub(text, 1, 88), "[\192-\255][\128-\191]*$", "").."...";
	end

	return format("Category: %s%s", category and category.name or "none", text ~= "" and ("\n|cff8f8f8f"..text.."|r") or "");
end

local function PreviousRanks(data)
	local spellId = SE.CurrentId();

	if ( not data or not spellId ) then
		return "";
	end

	local names = {};
	local where = { { columns = { "m_supercededBySpell" }, value = spellId }, { columns = { "m_skillLine" }, value = Num(data.m_skillLine) } };

	for _, entry in ipairs(SE.Filter(TABLE, "", where, 5)) do
		local row = GameEditor_DbcRow(TABLE, entry.id);
		local spell = Num(row and row.m_spell);

		if ( spell ~= 0 ) then
			table.insert(names, format("%d %s", spell, (SE.SpellName(spell)) or ""));
		end
	end

	return #names > 0 and ("Replaces "..table.concat(names, ", ")) or "";
end

local function BuildForm(parent)
	form = GameEditorModel.CreateForm(parent, FORM_WIDTH, function(fields)
		local doc = SelectedDoc();

		if ( doc and not IsRemoved(doc) ) then
			store:SetMany(doc, fields);
		end
	end);

	form:Header("Skill line");
	form:Ref("m_skillLine", "Skill line", "dbc:SkillLine", "SkillLine the spell belongs to, ... browses every skill line by category");
	form:Note(SkillNote, 34);

	form:Header("Who gets it");
	form:Flags("m_raceMask", "Races", RaceFlags(), 32, "Races this row is for, nothing checked means every race");
	form:Flags("m_classMask", "Classes", ClassFlags(), 32, "Classes this row is for, nothing checked means every class");
	form:Flags("m_excludeRace", "Exclude races", RaceFlags(), 32, "Races left out, the server does not read it");
	form:Flags("m_excludeClass", "Exclude classes", ClassFlags(), 32, "Classes left out, the server does not read it");

	form:Header("Learning");
	form:Combo("m_acquireMethod", "Learned", ACQUIRE_METHODS, "Trainer, quest or item: nothing automatic. At the min skill value: learned once the skill reaches Min skill and removed below it. Together with the skill: learned and removed with the skill itself", 240);
	form:Int("m_minSkillLineRank", "Min skill", 0, INT_HIGH, "Skill value the spell needs, also where it is learned when Learned is at the min skill value");
	form:Ref("m_supercededBySpell", "Superseded by", "spell", "The next rank, learning it replaces this spell in the spellbook");
	local previous = form:Note(PreviousRanks, 20);
	previous.visible = function(data) return PreviousRanks(data) ~= ""; end;

	form:Header("Crafting");
	form:Int("m_trivialSkillLineRankLow", "Yellow at", 0, INT_HIGH, "Skill value where the recipe turns yellow and skill ups get rarer");
	form:Int("m_trivialSkillLineRankHigh", "Grey at", 0, INT_HIGH, "Skill value where the recipe turns grey and gives no more skill ups");

	form:Header("Other");
	form:Int("m_characterPoints[0]", "Character points 1", 0, INT_HIGH, "The server does not read it");
	form:Int("m_characterPoints[1]", "Character points 2", 0, INT_HIGH, "The server does not read it");
end

local function BuildSkills(parent)
	frame = CreateFrame("Frame", "GameEditorSpellSkills", parent);

	local bar = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellSkillBar"), frame);
	bar:SetHeight(24);
	bar:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, 0);
	bar:SetPoint("TOPRIGHT", frame, "TOPRIGHT", 0, 0);

	local add = SE.Button(bar, "Add Skill Line", 104, Add, "Browse every skill line and put the spell in one with a new SkillLineAbility row");
	add:SetPoint("LEFT", bar, "LEFT", 0, 0);
	frame.remove = SE.Button(bar, "Remove", 70, RemoveSelected, "Take the spell out of the selected skill line, Save blanks the row. On a removed row it brings it back");
	frame.remove:SetPoint("LEFT", add, "RIGHT", 4, 0);
	frame.save = SE.Button(bar, "Save Skill Rows", 112, function() store:SaveAll(); end, "Write the changed SkillLineAbility rows to the dbc database, the server applies them at once (Ctrl+S saves them too)", true);
	frame.save:SetPoint("RIGHT", bar, "RIGHT", 0, 0);
	frame.revert = SE.Button(bar, "Revert", 60, RevertAll, "Throw away every unsaved skill line row");
	frame.revert:SetPoint("RIGHT", frame.save, "LEFT", -4, 0);
	frame.state = SE.Label(bar, "", nil, NOGGIT_COLOR_DISABLED);
	frame.state:SetPoint("LEFT", frame.remove, "RIGHT", 12, 0);
	frame.state:SetPoint("RIGHT", frame.revert, "LEFT", -12, 0);
	frame.state:SetHeight(16);

	frame.list = NoggitListBox_Create(frame, 36, function(row)
		row.title = SE.Label(row, "");
		row.title:SetPoint("TOPLEFT", row, "TOPLEFT", 6, -4);
		row.title:SetPoint("RIGHT", row, "RIGHT", -4, 0);
		row.title:SetHeight(14);
		row.detail = SE.Label(row, "", "NoggitFont10", NOGGIT_COLOR_DISABLED);
		row.detail:SetPoint("TOPLEFT", row.title, "BOTTOMLEFT", 0, -3);
		row.detail:SetPoint("RIGHT", row, "RIGHT", -4, 0);
		row.detail:SetHeight(12);
	end, function(row, item)
		row.title:SetText(item and item.title or "");
		row.detail:SetText(item and item.detail or "");
	end);
	frame.list:SetPoint("TOPLEFT", bar, "BOTTOMLEFT", 0, -8);
	frame.list:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 0, 0);
	frame.list:SetWidth(LIST_WIDTH);
	frame.list.onSelect = function(item)
		selectedId = item.doc.id;
		RefreshForm();
	end;
	frame.list.onDoubleClick = function(item)
		ChangeSkill(item.doc);
	end;

	frame.empty = SE.Label(frame.list, "This spell is in no skill line. Add Skill Line puts it in one, for a skill's spell list, a profession or auto learning.", nil, NOGGIT_COLOR_DISABLED);
	frame.empty:SetPoint("TOPLEFT", frame.list, "TOPLEFT", 8, -8);
	frame.empty:SetPoint("RIGHT", frame.list, "RIGHT", -8, 0);
	frame.empty:SetJustifyV("TOP");

	frame.holder = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellSkillForm"), frame);
	frame.holder:SetPoint("TOPLEFT", frame.list, "TOPRIGHT", 14, 0);
	frame.holder:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", 0, 0);
	frame.scroll, frame.child = GameEditor_CreateScrollFrame(frame.holder);
	frame.scroll:SetAllPoints(frame.holder);
	frame.child:SetWidth(FORM_WIDTH + 10);
	frame.child:SetHeight(10);
	BuildForm(frame.child);

	frame.placeholder = SE.Label(frame, "", nil, NOGGIT_COLOR_DISABLED);
	frame.placeholder:SetPoint("TOPLEFT", frame.list, "TOPRIGHT", 14, -4);
	frame.placeholder:SetPoint("RIGHT", frame, "RIGHT", 0, 0);
	frame.placeholder:SetJustifyV("TOP");

	frame:SetScript("OnShow", Refresh);
	return frame;
end

store:OnChanged(function()
	SE.QueueRefresh();
	Refresh();
end);

SE.On("SPELL_LOADED", Refresh);

SE.On("DBC_RECORD_UPDATED", function(tableName)
	if ( SE.Lower(tableName) == "skilllineability" ) then
		Refresh();
	end
end);

SE.AddSection({
	key = "skills",
	title = "Skills",
	order = 85,
	build = BuildSkills,
	badge = function()
		local count = SE.cur and SpellDirtyCount(SE.cur.id) or 0;
		return count > 0 and format("|cffffcc00%d|r", count) or nil;
	end,
});
