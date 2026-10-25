-- Editor tabs of the spell DBC row, laid out like the Personal Spell Editor: Base, Targets/Procs, Effects, Items, Flags, Icon and Reqs.

local SE = GameEditorSpell;
local Data = GameEditorData or {};

SE.SECTIONS = SE.SECTIONS or {};

local function GroupsByTitle(titles)
	local result = {};

	for _, title in ipairs(titles) do
		for _, group in ipairs(Data.Groups and Data.Groups.Spell or {}) do
			if ( group.title == title ) then
				table.insert(result, { title = group.title, columns = group.columns, collapsed = group.collapsed });
			end
		end
	end

	return result;
end

SE.GroupsByTitle = GroupsByTitle;

local function ColumnsOf(groups)
	local columns = {};

	for _, group in ipairs(groups) do
		for _, column in ipairs(group.columns) do
			table.insert(columns, column);
		end
	end

	return columns;
end

SE.ColumnsOf = ColumnsOf;

function SE.AddSection(section)
	table.insert(SE.SECTIONS, section);
	return section;
end

local function SpecColumns(...)
	local columns = {};

	for index = 1, select("#", ...) do
		for _, entry in ipairs((select(index, ...))) do
			table.insert(columns, entry[1]);
		end
	end

	return columns;
end

-- Context menu for spell grid rows ------------------------------------------------------------------

local contextColumn;

local function FieldOf(column)
	return Data.Fields and Data.Fields.Spell and Data.Fields.Spell[column] or {};
end

local FIELD_MENU = {
	{ text = "Revert this field", onClick = function()
		if ( contextColumn and SE.cur ) then
			SE.SetValue(contextColumn, SE.cur.original[contextColumn]);
		end
	end },
	{ text = "Set to zero or empty", onClick = function()
		if ( contextColumn and SE.cur ) then
			SE.SetValue(contextColumn, FieldOf(contextColumn).type == "string" and "" or 0);
		end
	end },
	{ separator = true },
	{ text = "Open the referenced spell", onClick = function()
		local value = contextColumn and tonumber(SE.GetValue(contextColumn));

		if ( value and value > 0 and FieldOf(contextColumn).ref == "spell" ) then
			SE.Open(value);
		end
	end },
};

local function FieldContext(_, column)
	contextColumn = column;
	NoggitContextMenu_Show("GameEditorSpellFieldContext", FIELD_MENU);
end

-- Grid bound to the open spell. Edits go straight into the shared values table.
function SE.CreateSpellGrid(parent, groups, options)
	options = options or {};
	local grid = GameEditor_CreatePropertyGrid(parent, {
		table = SE.TABLE,
		groups = groups,
		readOnly = { m_id = true },
		filter = options.filter,
		visible = options.visible,
		fields = options.fields,
		labelWidth = options.labelWidth or 190,
		editWidth = options.editWidth,
		onChange = function(self, column)
			SE.Changed(column, self);
		end,
		onRowContext = FieldContext,
	});
	SE.RegisterGrid(grid);
	return grid;
end

local function Pane(parent)
	return CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellPane"), parent);
end

-- A tab whose panes are field columns, synced each time it is shown.
local function SyncOnShow(frame, grids)
	frame:HookScript("OnShow", function()
		for _, grid in ipairs(grids) do
			SE.SyncGrid(grid);
		end
	end);
end

-- Class masks ------------------------------------------------------------------------------------------

local function ClassMaskNames(get)
	return Data.ClassMask and Data.ClassMask[SE.Number(get("m_spellClassSet"))];
end

-- Flag list words for the three words of a class mask, named after the spell family.
function SE.ClassMaskWords(prefix)
	local words = {};

	for word = 0, 2 do
		table.insert(words, {
			column = prefix.."["..word.."]",
			offset = word * 32,
			watch = "m_spellClassSet",
			key = function(get) return SE.Number(get("m_spellClassSet")); end,
			list = function(get)
				local names = ClassMaskNames(get);
				local flags = {};

				for bit = 0, 31 do
					local name = names and names[word * 32 + bit];

					if ( name ) then
						table.insert(flags, { bit = bit, name = type(name) == "table" and (name.name or "") or tostring(name), tooltip = type(name) == "table" and name.tooltip or nil });
					else
						table.insert(flags, { bit = bit, name = format("Flag %d", word * 32 + bit) });
					end
				end

				return flags;
			end,
		});
	end

	return words;
end

local classMaskWindow;

-- SpellClassMaskExtension row of the open spell, the Class Mask Ext button of the Base tab.
function SE.OpenClassMaskExtension()
	if ( not classMaskWindow ) then
		classMaskWindow = GameEditor_CreateWindow("GameEditorSpellClassMaskWindow", "Class mask extension", 460, 520, true);
		local editor = SE.CreateDbcEditor(classMaskWindow.content, "SpellClassMaskExtension", {
			title = "Class mask extension",
			groups = Data.Groups and Data.Groups.SpellClassMaskExtension,
			labelWidth = 200,
			rowId = SE.CurrentId,
			emptyText = "Open a spell first.",
			missingText = "This spell has no class mask extension row. Create one to give it up to three more families, so spell modifiers of other families can affect it.",
		});
		editor.frame:SetAllPoints(classMaskWindow.content);
		classMaskWindow.editor = editor;

		SE.On("SPELL_LOADED", function()
			if ( classMaskWindow:IsShown() ) then
				editor:Load(SE.CurrentId(), not editor:IsDirty() or editor.pending);
			end
		end);
	end

	classMaskWindow.editor:Load(SE.CurrentId());
	GameEditor_ShowWindow(classMaskWindow);
end

-- Base -------------------------------------------------------------------------------------------------

local BASE_LEFT = {
	{ "m_category", "Category" },
	{ "m_dispelType", "Dispel Type" },
	{ "m_mechanic", "Mechanic Type" },
	{ "m_castingTimeIndex", "Cast Time" },
	{ "m_durationIndex", "Duration" },
	{ "m_rangeIndex", "Range" },
	{ "m_casterAuraState", "Caster Aura State" },
	{ "m_spellLevel", "Spell Level" },
	{ "m_baseLevel", "Base Level" },
	{ "m_maxLevel", "Max Level" },
	{ "m_startRecoveryTime", "Global Cooldown Time" },
	{ "m_maxTargets", "Max Targets" },
	{ "m_maxTargetLevel", "Max Targets Level" },
	{ "m_runeCostID", "Rune Cost" },
	{ "m_spellPriority", "Spell Priority" },
	{ "m_modalNextSpell", "Modal Next Spell" },
};

local BASE_MIDDLE = {
	{ "m_spellVisualID[0]", "Spell Visual 1" },
	{ "m_spellVisualID[1]", "Spell Visual 2" },
	{ "m_speed", "Speed" },
	{ "m_targetAuraState", "Target Aura State" },
	{ "m_recoveryTime", "Recovery Time" },
	{ "m_categoryRecoveryTime", "Category Recovery Time" },
	{ "m_spellMissileID", "Spell Missile ID" },
	{ "m_cumulativeAura", "Stacks" },
	{ "m_difficulty", "Difficulty" },
	{ "m_powerType", "Power Type" },
	{ "m_requiredTotemCategoryID[0]", "Totem Category 1" },
	{ "m_requiredTotemCategoryID[1]", "Totem Category 2" },
	{ "m_requiredAreasID", "Area Group" },
	{ "m_descriptionVariablesID", "Spell Description Variables" },
	{ "m_stanceBarOrder", "Stance Bar Order" },
};

local BASE_RIGHT = {
	{ "m_startRecoveryCategory", "Global Cooldown Category" },
	{ "m_manaCost", "Power Cost" },
	{ "m_manaCostPerLevel", "Mana Cost Per Level" },
	{ "m_manaPerSecond", "Mana Cost Per Second" },
	{ "m_manaPerSecondPerLevel", "Per Second Per Level" },
	{ "m_manaCostPct", "Mana Cost %" },
	{ "m_requiresSpellFocus", "Requires Spell Focus" },
	{ "m_preventionType", "Prevention Type" },
	{ "m_defenseType", "Spell Damage Type" },
	{ "m_spellClassSet", "Spell Family Name" },
	{ "m_powerDisplayID", "Power Display Id" },
};

local function BuildBase(parent)
	local frame = CreateFrame("Frame", "GameEditorSpellBase", parent);
	local left = SE.FieldColumn(frame, BASE_LEFT);
	local middle = SE.FieldColumn(frame, BASE_MIDDLE, { labelWidth = 170 });

	local right = Pane(frame);
	local rightGrid = SE.FieldColumn(right, BASE_RIGHT, { labelWidth = 160 });
	rightGrid.frame:SetPoint("TOPLEFT", right, "TOPLEFT", 0, 0);
	rightGrid.frame:SetPoint("TOPRIGHT", right, "TOPRIGHT", 0, 0);
	rightGrid.frame:SetHeight(#BASE_RIGHT * 24 + 6);

	local families = SE.CreateFlagList(right, {
		title = "Spell Family Flags",
		tooltip = "The class mask of this spell. Spell modifiers and effects of the same family pick their targets by these bits. Names come from the Spell Family Name.",
		words = SE.ClassMaskWords("m_spellClassMask"),
	});
	families:SetPoint("TOPLEFT", rightGrid.frame, "BOTTOMLEFT", 0, -6);
	families:SetPoint("BOTTOMRIGHT", right, "BOTTOMRIGHT", 0, 30);

	local onlySet = NoggitCheckBox_Create(right, "Only checked", false, function(_, checked)
		families:SetFilter("", checked);
	end);
	onlySet:SetPoint("BOTTOMLEFT", right, "BOTTOMLEFT", 0, 4);
	local extension = SE.Button(right, "Class Mask Ext", 104, SE.OpenClassMaskExtension,
		"SpellClassMaskExtension row of this spell: up to three more families and masks, so modifiers of other families can affect it");
	extension:SetPoint("BOTTOMRIGHT", right, "BOTTOMRIGHT", 0, 2);

	local school = Pane(frame);
	local schools = SE.CreateFlagList(school, { title = "School", words = { { column = "m_schoolMask" } } });
	schools:SetPoint("TOPLEFT", school, "TOPLEFT", 0, 0);
	schools:SetPoint("TOPRIGHT", school, "TOPRIGHT", 0, 0);
	schools:SetHeight(7 * 20 + 30);
	local facing = SE.CreateFlagList(school, { title = "Facing", words = { { column = "m_facingCasterFlags" } } });
	facing:SetPoint("TOPLEFT", schools, "BOTTOMLEFT", 0, -10);
	facing:SetPoint("TOPRIGHT", schools, "BOTTOMRIGHT", 0, -10);
	facing:SetHeight(2 * 20 + 30);

	SE.SideBySide(frame, {
		{ frame = left.frame, weight = 1 },
		{ frame = middle.frame, weight = 1.05 },
		{ frame = right, weight = 1.05 },
		{ frame = school, width = 150 },
	}, 14);

	SyncOnShow(frame, { left, middle, rightGrid });
	return frame;
end

SE.AddSection({ key = "base", title = "Base", order = 20, columns = SpecColumns(BASE_LEFT, BASE_MIDDLE, BASE_RIGHT,
	{ { "m_spellClassMask[0]" }, { "m_spellClassMask[1]" }, { "m_spellClassMask[2]" }, { "m_schoolMask" }, { "m_facingCasterFlags" } }), build = BuildBase });

-- Targets and procs --------------------------------------------------------------------------------------

local PROC_FIELDS = {
	{ "m_procCharges", "Proc Charges" },
	{ "m_procChance", "Proc Chance" },
};

local function BuildTargets(parent)
	local frame = CreateFrame("Frame", "GameEditorSpellTargets", parent);

	local targets = SE.CreateFlagList(frame, { title = "Targets", tooltip = "Kinds of target the cast needs or accepts", words = { { column = "m_targets" } }, showHex = true });
	local procs = SE.CreateFlagList(frame, { title = "Procs", tooltip = "Events that can trigger the proc of an aura of this spell", words = { { column = "m_procTypeMask" } }, showHex = true });

	local right = Pane(frame);
	local grid = SE.FieldColumn(right, PROC_FIELDS, { labelWidth = 110 });
	grid.frame:SetPoint("TOPLEFT", right, "TOPLEFT", 0, -18);
	grid.frame:SetPoint("TOPRIGHT", right, "TOPRIGHT", 0, -18);
	grid.frame:SetHeight(#PROC_FIELDS * 24 + 6);

	local onlySet = NoggitCheckBox_Create(right, "Only checked", false, function(_, checked)
		targets:SetFilter("", checked);
		procs:SetFilter("", checked);
	end);
	onlySet:SetPoint("TOPLEFT", grid.frame, "BOTTOMLEFT", 0, -10);

	local note = SE.Label(right, "The server side proc rules of this spell (phases, hit masks, cooldown) are in Server, Proc rules.", nil, NOGGIT_COLOR_DISABLED);
	note:SetPoint("TOPLEFT", onlySet, "BOTTOMLEFT", 0, -12);
	note:SetPoint("RIGHT", right, "RIGHT", 0, 0);
	note:SetJustifyV("TOP");
	local open = SE.Button(right, "Open proc rules", 110, function()
		SE.SelectSection("server");

		if ( SE.SelectServerSection ) then
			SE.SelectServerSection("proc");
		end
	end);
	open:SetPoint("TOPLEFT", note, "BOTTOMLEFT", 0, -8);

	SE.SideBySide(frame, {
		{ frame = targets, weight = 1 },
		{ frame = procs, weight = 1.2 },
		{ frame = right, width = 260 },
	}, 14);

	SyncOnShow(frame, { grid });
	return frame;
end

SE.AddSection({ key = "targets", title = "Targets/Procs", order = 30, columns = { "m_targets", "m_procTypeMask", "m_procCharges", "m_procChance" }, build = BuildTargets });

-- Effects ----------------------------------------------------------------------------------------------

local CLASS_MASK_LETTERS = { "A", "B", "C" };

function SE.EffectGroups(index)
	local letter = CLASS_MASK_LETTERS[index];

	return {
		{ title = "Type", columns = { "m_effect["..(index - 1).."]", "m_effectAura["..(index - 1).."]", "m_effectMechanic["..(index - 1).."]" } },
		{ title = "Value", columns = { "m_effectBasePoints["..(index - 1).."]", "m_effectDieSides["..(index - 1).."]", "m_effectRealPointsPerLevel["..(index - 1).."]", "m_effectPointsPerCombo["..(index - 1).."]",
			"m_effectAmplitude["..(index - 1).."]", "m_effectChainAmplitude["..(index - 1).."]", "m_effectBonusCoefficient["..(index - 1).."]" } },
		{ title = "Targets", columns = { "m_implicitTargetA["..(index - 1).."]", "m_implicitTargetB["..(index - 1).."]", "m_effectRadiusIndex["..(index - 1).."]", "m_effectChainTargets["..(index - 1).."]" } },
		{ title = "Parameters", columns = { "m_effectAuraPeriod["..(index - 1).."]", "m_effectMiscValue["..(index - 1).."]", "m_effectMiscValueB["..(index - 1).."]", "m_effectTriggerSpell["..(index - 1).."]", "m_effectItemType["..(index - 1).."]" } },
		{ title = "Spell class mask", columns = { "m_effectSpellClassMask"..letter.."[0]", "m_effectSpellClassMask"..letter.."[1]", "m_effectSpellClassMask"..letter.."[2]" } },
	};
end

function SE.EffectColumns(index)
	return ColumnsOf(SE.EffectGroups(index));
end

local function EffectLeft(index)
	local e = index - 1;

	return {
		{ "m_effect["..e.."]", "Spell Effect" },
		{ "m_effectBasePoints["..e.."]", "Base Points" },
		{ "m_effectDieSides["..e.."]", "Die Sides" },
		{ "m_effectRealPointsPerLevel["..e.."]", "Real Points Per Level" },
		{ "m_effectMechanic["..e.."]", "Mechanic" },
		{ "m_implicitTargetA["..e.."]", "Target A" },
		{ "m_implicitTargetB["..e.."]", "Target B" },
		{ "m_effectRadiusIndex["..e.."]", "Radius" },
		{ "m_effectAura["..e.."]", "Apply Aura Name" },
		{ "m_effectAuraPeriod["..e.."]", "Amplitude" },
		{ "m_effectAmplitude["..e.."]", "Multiple Value" },
		{ "m_effectChainTargets["..e.."]", "Chain Target" },
		{ "m_effectItemType["..e.."]", "Item Type" },
	};
end

local function EffectRight(index)
	local e = index - 1;

	return {
		{ "m_effectTriggerSpell["..e.."]", "Trigger Spell" },
		{ "m_effectPointsPerCombo["..e.."]", "Points Per Combo Point" },
		{ "m_effectMiscValue["..e.."]", "Misc Value A" },
		{ "m_effectMiscValueB["..e.."]", "Misc Value B" },
		{ "m_effectChainAmplitude["..e.."]", "Damage Multiplier" },
		{ "m_effectBonusCoefficient["..e.."]", "Effect Bonus Multiplier" },
		{ "m_effectSpellClassMask"..CLASS_MASK_LETTERS[index].."[0]", "Class Mask 1" },
		{ "m_effectSpellClassMask"..CLASS_MASK_LETTERS[index].."[1]", "Class Mask 2" },
		{ "m_effectSpellClassMask"..CLASS_MASK_LETTERS[index].."[2]", "Class Mask 3" },
	};
end

local familyWindows = {};

-- Checkbox selection of one effect's class mask, the Edit Families button of the effect pages.
local function OpenEffectFamilies(index)
	local window = familyWindows[index];

	if ( not window ) then
		window = GameEditor_CreateWindow("GameEditorSpellEffectFamilies"..index, format("Effect %d class mask", index), 420, 520, true);
		local masks = SE.CreateFlagList(window.content, {
			title = "Spell Family Flags",
			tooltip = "Spells of this spell family, or with a class mask extension block in it, that share a bit with the mask are affected",
			words = SE.ClassMaskWords("m_effectSpellClassMask"..CLASS_MASK_LETTERS[index]),
		});
		masks:SetPoint("TOPLEFT", window.content, "TOPLEFT", 6, -6);
		masks:SetPoint("BOTTOMRIGHT", window.content, "BOTTOMRIGHT", -6, 30);
		local onlySet = NoggitCheckBox_Create(window.content, "Only checked", false, function(_, checked)
			masks:SetFilter("", checked);
		end);
		onlySet:SetPoint("BOTTOMLEFT", window.content, "BOTTOMLEFT", 6, 6);
		familyWindows[index] = window;
	end

	GameEditor_ShowWindow(window);
end

-- Spells the effect's class mask hits, SpellClassMaskExtension blocks included, with the Edit Families button.
function SE.CreateClassMaskTargets(parent, index)
	local letter = CLASS_MASK_LETTERS[index];
	local frame = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellMaskTargets"), parent);
	local title = SE.Label(frame, "Class Mask Target Spells", "NoggitFontBold", NOGGIT_COLOR_WHITE);
	title:SetPoint("TOPLEFT", frame, "TOPLEFT", 2, 0);
	local count = SE.Label(frame, "", "NoggitFont10", NOGGIT_COLOR_DISABLED, "RIGHT");
	count:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -2, -2);

	local list = NoggitListBox_Create(frame, 18, function(row)
		row.text = SE.Label(row, "", "NoggitFont10", NOGGIT_COLOR_WHITE);
		row.text:SetPoint("LEFT", row, "LEFT", 4, 0);
		row.text:SetPoint("RIGHT", row, "RIGHT", -4, 0);
	end, function(row, item)
		row.text:SetText(format("%d  %s%s", item.id, item.name ~= "" and item.name or "|cff8f8f8f(no name)|r", item.extension and "  |cff7fbfff(extension)|r" or ""));
	end);
	list:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, -18);
	list:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", 0, 30);
	list.onDoubleClick = function(item) SE.Open(item.id); end;

	local edit = SE.Button(frame, "Edit Families", 110, function() OpenEffectFamilies(index); end,
		"Pick the class mask bits with checkboxes, each bit named after the spells that carry it");
	edit:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 0, 4);
	local hint = SE.Label(frame, "Double click a spell to open it", "NoggitFont10", NOGGIT_COLOR_DISABLED);
	hint:SetPoint("LEFT", edit, "RIGHT", 8, 0);

	local function Refresh()
		frame.dirty = false;
		local family = SE.Number(SE.GetValue("m_spellClassSet"));
		local items = {};

		if ( SE.cur and DbcEditor_GetClassMaskSpells ) then
			items = DbcEditor_GetClassMaskSpells(family,
				SE.Number(SE.GetValue("m_effectSpellClassMask"..letter.."[0]")),
				SE.Number(SE.GetValue("m_effectSpellClassMask"..letter.."[1]")),
				SE.Number(SE.GetValue("m_effectSpellClassMask"..letter.."[2]"))) or {};
		end

		count:SetText(family == 0 and "spell has no family" or format("%d spells", #items));
		NoggitListBox_SetItems(list, items);
	end

	local function MarkDirty(column)
		if ( column and column ~= "m_spellClassSet" and not string.find(column, "m_effectSpellClassMask"..letter, 1, true) ) then
			return;
		end

		if ( frame:IsVisible() ) then
			Refresh();
		else
			frame.dirty = true;
		end
	end

	frame:SetScript("OnShow", function() if ( frame.dirty ~= false ) then Refresh(); end end);
	SE.On("VALUE_CHANGED", MarkDirty);
	SE.On("SPELL_LOADED", function() MarkDirty(); end);
	SE.On("SPELL_SAVED", function() MarkDirty(); end);
	return frame;
end

local function MiscInfo(values, index)
	local effect = SE.Number(values["m_effect["..(index - 1).."]"]);
	local aura = SE.Number(values["m_effectAura["..(index - 1).."]"]);
	local misc = Data.MiscValue or {};
	return (aura ~= 0 and misc.Aura and misc.Aura[aura]) or (misc.Effect and misc.Effect[effect]);
end

local function MiscText(info, slot, value)
	if ( not info or not info[slot] ) then
		return nil;
	end

	local meaning;

	if ( info[slot.."Enum"] ) then
		meaning = SE.EnumText(info[slot.."Enum"], value);
	elseif ( info[slot.."Flags"] ) then
		local names = {};

		for _, flag in ipairs(Data.Flags and Data.Flags[info[slot.."Flags"]] or {}) do
			if ( GameEditor_HasBit(value, flag.bit) ) then
				table.insert(names, flag.alias or flag.name);
			end
		end

		meaning = #names > 0 and table.concat(names, ", ") or "none";
	elseif ( info[slot.."Ref"] ) then
		meaning = GameEditor_ResolveRef(info[slot.."Ref"], value);
	end

	return format("%s %s%s", info[slot], tostring(value), meaning and ("  |cffffffff"..meaning.."|r") or "");
end

-- Short description of an effect for the tabs and the summary card.
function SE.EffectSummary(values, index)
	local effect = SE.Number(values["m_effect["..(index - 1).."]"]);

	if ( effect == 0 ) then
		return "No effect", nil;
	end

	local name = SE.EnumText("SpellEffect", effect);
	local aura = SE.Number(values["m_effectAura["..(index - 1).."]"]);

	if ( aura ~= 0 ) then
		name = name..": "..SE.EnumText("SpellAura", aura);
	end

	return name, SE.EffectRangeText(values, index);
end

local function ClearEffect(values, index)
	for _, column in ipairs(SE.EffectColumns(index)) do
		values[column] = 0;
	end

	values["m_effectChainAmplitude["..(index - 1).."]"] = 1;
	values["m_effectBonusCoefficient["..(index - 1).."]"] = 1;
end

local function BuildEffects(parent)
	local frame = CreateFrame("Frame", "GameEditorSpellEffects", parent);
	frame.selected = 1;
	frame.pages = {};
	frame.grids = {};

	local tabHolder = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellEffectTabs"), frame);
	tabHolder:SetHeight(24);
	tabHolder:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, 0);
	tabHolder:SetPoint("TOPRIGHT", frame, "TOPRIGHT", 0, 0);
	frame.tabs = NoggitTabs_Create(tabHolder, { "Spell Effect 1", "Spell Effect 2", "Spell Effect 3" }, 1, function(index) frame:Select(index); end);
	frame.tabs[1]:SetPoint("BOTTOMLEFT", tabHolder, "BOTTOMLEFT", 0, 0);

	local card = SE.Box(frame, "GameEditorSpellEffectCard", NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID);
	card:SetPoint("TOPLEFT", tabHolder, "BOTTOMLEFT", 0, -6);
	card:SetPoint("TOPRIGHT", tabHolder, "BOTTOMRIGHT", 0, -6);
	card:SetHeight(76);
	card.lines = {};

	for line = 1, 4 do
		local text = SE.Label(card, "");
		text:SetPoint("TOPLEFT", card, "TOPLEFT", 10, -6 - (line - 1) * 17);
		text:SetPoint("RIGHT", card, "RIGHT", -10, 0);
		card.lines[line] = text;
	end

	NoggitText_SetColor(card.lines[1], NOGGIT_COLOR_WHITE);

	local function CopyEffect(from, to, swap)
		local values = SE.cur and SE.cur.values;

		if ( not values ) then
			return;
		end

		local source = SE.EffectColumns(from);
		local target = SE.EffectColumns(to);

		for position, column in ipairs(source) do
			local value = values[column];

			if ( swap ) then
				values[column] = values[target[position]];
			end

			values[target[position]] = value;
		end

		SE.Changed("m_effect["..(to - 1).."]");
		frame:Select(to);
	end

	local function CopyFromSpell(index)
		SE.PickSpell(format("Copy effect %d from spell", index), function(id)
			local row = id and SE.GetRow(SE.TABLE, id);

			if ( not row ) then
				GameEditor_Notify(format("Spell %s does not exist.", tostring(id)), "error");
				return;
			end

			if ( not SE.cur ) then
				return;
			end

			for _, column in ipairs(SE.EffectColumns(index)) do
				SE.cur.values[column] = row[column];
			end

			SE.Changed("m_effect["..(index - 1).."]");
			GameEditor_Notify(format("Effect %d copied from spell %d.", index, id), "info");
		end);
	end

	local copyMenus = {};

	for from = 1, 3 do
		local definition = {};

		for to = 1, 3 do
			if ( to ~= from ) then
				table.insert(definition, { text = format("Copy effect %d over effect %d", from, to), onClick = function() CopyEffect(from, to, false); end });
				table.insert(definition, { text = format("Swap effect %d with effect %d", from, to), onClick = function() CopyEffect(from, to, true); end });
			end
		end

		copyMenus[from] = definition;
	end

	local function BuildPage(index)
		local page = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellEffectPage"), frame);
		page:SetPoint("TOPLEFT", card, "BOTTOMLEFT", 0, -8);
		page:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", 0, 0);

		local left = Pane(page);
		local leftGrid = SE.FieldColumn(left, EffectLeft(index), { labelWidth = 150, editWidth = 80 });
		leftGrid.frame:SetPoint("TOPLEFT", left, "TOPLEFT", 0, 0);
		leftGrid.frame:SetPoint("BOTTOMRIGHT", left, "BOTTOMRIGHT", 0, 34);

		local copyFrom = SE.Button(left, "Copy From Spell", 116, function() CopyFromSpell(index); end,
			"Pick another spell and copy the same numbered effect from it into this one");
		copyFrom:SetPoint("BOTTOMLEFT", left, "BOTTOMLEFT", 0, 4);
		local reset = SE.Button(left, "Reset Effect", 92, function()
			if ( SE.cur ) then
				ClearEffect(SE.cur.values, index);
				SE.Changed("m_effect["..(index - 1).."]");
			end
		end, "Clear this effect, putting every field back to the value an unused effect has");
		reset:SetPoint("LEFT", copyFrom, "RIGHT", 4, 0);
		local copy = SE.Button(left, "Copy / Swap", 86, function()
			NoggitContextMenu_Show("GameEditorSpellEffectMenu"..index, copyMenus[index]);
		end, "Copy or swap this effect with another effect of the same spell");
		copy:SetPoint("LEFT", reset, "RIGHT", 4, 0);
		page.openTrigger = SE.Button(left, "Open Trigger", 90, function()
			local id = SE.Number(SE.GetValue("m_effectTriggerSpell["..(index - 1).."]"));

			if ( id > 0 ) then
				SE.Open(id);
			end
		end, "Opens the spell this effect triggers in a new tab");
		page.openTrigger:SetPoint("LEFT", copy, "RIGHT", 4, 0);

		local right = Pane(page);
		local rightSpec = EffectRight(index);
		local rightGrid = SE.FieldColumn(right, rightSpec, { labelWidth = 160, editWidth = 80 });
		rightGrid.frame:SetPoint("TOPLEFT", right, "TOPLEFT", 0, 0);
		rightGrid.frame:SetPoint("TOPRIGHT", right, "TOPRIGHT", 0, 0);
		rightGrid.frame:SetHeight(#rightSpec * 24 + 6);

		local targets = SE.CreateClassMaskTargets(right, index);
		targets:SetPoint("TOPLEFT", rightGrid.frame, "BOTTOMLEFT", 0, -6);
		targets:SetPoint("BOTTOMRIGHT", right, "BOTTOMRIGHT", 0, 0);

		SE.SideBySide(page, { { frame = left, weight = 1 }, { frame = right, weight = 1 } }, 16);
		table.insert(frame.grids, leftGrid);
		table.insert(frame.grids, rightGrid);
		page.grids = { leftGrid, rightGrid };
		page:Hide();
		frame.pages[index] = page;
		return page;
	end

	frame.RefreshTabs = function(self)
		local values = SE.cur and SE.cur.values or {};

		for index, tab in ipairs(self.tabs) do
			local changed = SE.ChangeCount(SE.EffectColumns(index));
			local empty = SE.Number(values["m_effect["..(index - 1).."]"]) == 0;
			tab:SetText(format("%sSpell Effect %d%s|r", empty and "|cff8f8f8f" or "|cffffffff", index, changed > 0 and format("  |cffffcc00%d|r", changed) or ""));
			tab:SetWidth(max(110, tab:GetFontString():GetStringWidth() + 24));
			tab.selected = index == self.selected;
			NoggitTab_Refresh(tab);
		end
	end;

	frame.RefreshCard = function(self)
		local values = SE.cur and SE.cur.values or {};
		local index = self.selected;
		local effect = SE.Number(values["m_effect["..(index - 1).."]"]);
		local lines = {};

		if ( effect == 0 ) then
			lines[1] = format("Effect %d does nothing. Pick a Spell Effect in the first row, or copy one from another spell.", index);
		else
			local name, value = SE.EffectSummary(values, index);
			local text = format("%s   |cff8f8f8fvalue|r %s", name, value);
			local amplitude = SE.Number(values["m_effectAuraPeriod["..(index - 1).."]"]);
			local duration = SE.DurationInfo(SE.Number(values.m_durationIndex)) or 0;

			if ( amplitude > 0 and duration > 0 ) then
				local ticks = floor(duration / amplitude);
				text = text..format("   |cff8f8f8f%d ticks every %s sec, total|r %s", ticks, SE.Trim(amplitude / 1000), SE.EffectRangeText(values, index, ticks));
			end

			lines[1] = text;

			local targetA = SE.EnumText("SpellImplicitTarget", SE.Number(values["m_implicitTargetA["..(index - 1).."]"]));
			local targetB = SE.EnumText("SpellImplicitTarget", SE.Number(values["m_implicitTargetB["..(index - 1).."]"]));
			local _, radius = SE.RadiusInfo(SE.Number(values["m_effectRadiusIndex["..(index - 1).."]"]));
			local chain = SE.Number(values["m_effectChainTargets["..(index - 1).."]"]);
			lines[2] = format("|cff8f8f8fTargets|r %s / %s%s%s", targetA, targetB,
				radius and ("   |cff8f8f8fradius|r "..radius) or "", chain > 0 and ("   |cff8f8f8fchain|r "..chain) or "");

			local info = MiscInfo(values, index);
			local misc = {};
			local miscA = MiscText(info, "a", SE.Number(values["m_effectMiscValue["..(index - 1).."]"]));
			local miscB = MiscText(info, "b", SE.Number(values["m_effectMiscValueB["..(index - 1).."]"]));

			if ( miscA ) then
				table.insert(misc, miscA);
			end

			if ( miscB ) then
				table.insert(misc, miscB);
			end

			lines[3] = #misc > 0 and table.concat(misc, "   ") or nil;

			local trigger = SE.Number(values["m_effectTriggerSpell["..(index - 1).."]"]);

			if ( trigger ~= 0 ) then
				local triggerName, triggerRank = SE.SpellName(trigger);
				lines[4] = format("|cff8f8f8fTriggers|r %d %s%s", trigger, triggerName or "|cffff6060missing|r",
					(triggerRank and triggerRank ~= "") and (" ("..triggerRank..")") or "");
			end
		end

		for line = 1, 4 do
			self.card.lines[line]:SetText(lines[line] or "");
		end

		local page = self.pages[index];

		if ( page ) then
			SE.SetEnabled(page.openTrigger, SE.Number(values["m_effectTriggerSpell["..(index - 1).."]"]) > 0);
		end
	end;

	frame.card = card;

	frame.Select = function(self, index)
		self.selected = index;

		for other, page in pairs(self.pages) do
			if ( other ~= index ) then
				page:Hide();
			end
		end

		local page = self.pages[index] or BuildPage(index);
		page:Show();

		for _, grid in ipairs(page.grids) do
			SE.SyncGrid(grid);
		end

		self:RefreshTabs();
		self:RefreshCard();
	end;

	frame:SetScript("OnShow", function(self)
		self:Select(self.selected);
	end);

	SE.On("REFRESH", function()
		if ( frame:IsVisible() ) then
			frame:RefreshTabs();
			frame:RefreshCard();
		end
	end);

	return frame;
end

do
	local columns = {};

	for index = 1, 3 do
		for _, column in ipairs(SE.EffectColumns(index)) do
			table.insert(columns, column);
		end
	end

	SE.AddSection({ key = "effects", title = "Effects", order = 40, columns = columns, build = BuildEffects });
end

-- Items ------------------------------------------------------------------------------------------------

local ITEMS_LEFT = {
	{ "m_totem[0]", "Totem 1" },
	{ "m_totem[1]", "Totem 2" },
};

local ITEMS_MIDDLE = {
	{ "m_requiredTotemCategoryID[0]", "Totem Category 1" },
	{ "m_requiredTotemCategoryID[1]", "Totem Category 2" },
};

for index = 1, 8 do
	table.insert(ITEMS_LEFT, { "m_reagent["..(index - 1).."]", "Reagent "..index });
	table.insert(ITEMS_MIDDLE, { "m_reagentCount["..(index - 1).."]", "Reagent Count "..index });
end

local ITEMS_CLASS = { { "m_equippedItemClass", "Equipped Item Class" } };

local function BuildItems(parent)
	local frame = CreateFrame("Frame", "GameEditorSpellItems", parent);
	local left = SE.FieldColumn(frame, ITEMS_LEFT, { labelWidth = 90, editWidth = 70 });
	local middle = SE.FieldColumn(frame, ITEMS_MIDDLE, { labelWidth = 120, editWidth = 50 });

	local right = Pane(frame);
	local classGrid = SE.FieldColumn(right, ITEMS_CLASS, { labelWidth = 140 });
	classGrid.frame:SetPoint("TOPLEFT", right, "TOPLEFT", 0, 0);
	classGrid.frame:SetPoint("TOPRIGHT", right, "TOPRIGHT", 0, 0);
	classGrid.frame:SetHeight(30);

	local lists = Pane(right);
	lists:SetPoint("TOPLEFT", classGrid.frame, "BOTTOMLEFT", 0, -6);
	lists:SetPoint("BOTTOMRIGHT", right, "BOTTOMRIGHT", 0, 0);
	local inventory = SE.CreateFlagList(lists, { title = "Equipped Item Inventory Type", words = { { column = "m_equippedItemInvTypes" } } });
	local subclass = SE.CreateFlagList(lists, {
		title = "Equipped Item SubClass",
		words = { { column = "m_equippedItemSubclass" } },
		emptyText = "Pick an Equipped Item Class first.",
	});
	SE.SideBySide(lists, { { frame = inventory, weight = 1 }, { frame = subclass, weight = 1 } }, 10);

	SE.SideBySide(frame, {
		{ frame = left.frame, weight = 1 },
		{ frame = middle.frame, weight = 0.8 },
		{ frame = right, weight = 1.7 },
	}, 14);

	SyncOnShow(frame, { left, middle, classGrid });
	return frame;
end

SE.AddSection({ key = "items", title = "Items", order = 50, columns = SpecColumns(ITEMS_LEFT, ITEMS_MIDDLE, ITEMS_CLASS, { { "m_equippedItemInvTypes" }, { "m_equippedItemSubclass" } }), build = BuildItems });

-- Flags ------------------------------------------------------------------------------------------------

local ATTRIBUTE_WORDS = {
	{ column = "m_attributes", title = "Attributes" },
	{ column = "m_attributesEx", title = "Attributes Ex" },
	{ column = "m_attributesExB", title = "Attributes Ex2" },
	{ column = "m_attributesExC", title = "Attributes Ex3" },
	{ column = "m_attributesExD", title = "Attributes Ex4" },
	{ column = "m_attributesExE", title = "Attributes Ex5" },
	{ column = "m_attributesExF", title = "Attributes Ex6" },
	{ column = "m_attributesExG", title = "Attributes Ex7" },
};

-- Search box and the Only checked box above a row of flag lists.
local function FilterBar(parent, lists, withSearch)
	local bar = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellFlagBar"), parent);
	bar:SetHeight(24);
	bar:SetPoint("TOPLEFT", parent, "TOPLEFT", 0, 0);
	bar:SetPoint("TOPRIGHT", parent, "TOPRIGHT", 0, 0);
	bar.text = "";
	bar.onlySet = false;

	local function Apply()
		for _, list in ipairs(lists) do
			list:SetFilter(bar.text, bar.onlySet);
		end
	end

	local anchor;

	if ( withSearch ) then
		local label = SE.Label(bar, "Search:");
		label:SetPoint("LEFT", bar, "LEFT", 2, 0);
		local search = SE.SearchBox(bar, 220, "Flag name, constant or bit", function(text)
			bar.text = text;
			Apply();
		end);
		search:SetPoint("LEFT", label, "RIGHT", 8, 0);
		search:SetScript("OnEnterPressed", function(self) self:ClearFocus(); end);
		anchor = search;
	end

	local onlySet = NoggitCheckBox_Create(bar, "Only checked", false, function(_, checked)
		bar.onlySet = checked;
		Apply();
	end);
	SE.Tooltip(onlySet, "Show only the boxes this spell has checked");

	if ( anchor ) then
		onlySet:SetPoint("LEFT", anchor, "RIGHT", 14, 0);
	else
		onlySet:SetPoint("LEFT", bar, "LEFT", 2, 0);
	end

	bar.Apply = Apply;
	return bar;
end

local function ListsBelow(page, bar, lists, gap, extra)
	local holder = Pane(page);
	holder:SetPoint("TOPLEFT", bar, "BOTTOMLEFT", 0, -6);
	holder:SetPoint("BOTTOMRIGHT", page, "BOTTOMRIGHT", 0, 0);

	for _, list in ipairs(lists) do
		list:SetParent(holder);
	end

	local panes = {};

	for _, list in ipairs(lists) do
		table.insert(panes, { frame = list, weight = 1 });
	end

	for _, pane in ipairs(extra or {}) do
		pane.frame:SetParent(holder);
		table.insert(panes, pane);
	end

	SE.SideBySide(holder, panes, gap or 10);
	return holder;
end

local customEditor;

-- The two SpellCustomAttr masks as checkbox lists, with the save buttons of their own row.
local function BuildCustomAttributes(parent)
	local holder = Pane(parent);
	customEditor = SE.CreateDbcEditor(holder, "SpellCustomAttr", {
		title = "Custom",
		labelWidth = 160,
		rowId = SE.CurrentId,
		hideGrid = true,
		filter = false,
		emptyText = "Open a spell first.",
		missingText = "This spell has no SpellCustomAttr row.",
	});
	customEditor.frame:SetAllPoints(holder);

	local lists = Pane(holder);
	lists:SetPoint("TOPLEFT", holder, "TOPLEFT", 0, -52);
	lists:SetPoint("BOTTOMRIGHT", holder, "BOTTOMRIGHT", 0, 0);

	local function Bound(column, title, tip)
		return SE.CreateFlagList(lists, {
			title = title,
			tooltip = tip,
			words = { { column = column, table = "SpellCustomAttr" } },
			allBits = true,
			get = function(name) return customEditor.values[name]; end,
			original = function(name) return customEditor.original[name]; end,
			set = function(name, value) customEditor.grid:SetValue(name, value); end,
		});
	end

	local first = Bound("m_attributes", "Custom Attributes", "Attributes from SpellCustomAttr.dbc, read by both the core and the client. Saved with the row, not with the spell.");
	local second = Bound("m_attributes2", "Custom Attributes 2", "The second SpellCustomAttr.dbc mask, for attributes added once the first mask is full.");
	SE.SideBySide(lists, { { frame = first, weight = 1 }, { frame = second, weight = 1 } }, 10);
	holder.lists = { first, second };

	customEditor.options.onStatus = function(editor)
		if ( editor.exists or editor.isNew ) then
			lists:Show();

			for _, list in ipairs(holder.lists) do
				list:Refresh();
			end
		else
			lists:Hide();
		end
	end;

	SE.On("SPELL_LOADED", function()
		customEditor:Load(SE.CurrentId(), not customEditor:IsDirty() or customEditor.pending);
	end);
	customEditor:Load(SE.CurrentId(), true);
	return holder;
end

local function BuildFlags(parent)
	local frame = CreateFrame("Frame", "GameEditorSpellFlags", parent);
	local pages = SE.CreateTabPages(frame, { "Interrupts", "Attributes", "Attributes2", "Misc" }, 0);

	do
		local page = pages[1];
		local lists = {
			SE.CreateFlagList(page, { title = "Interrupt Flags", words = { { column = "m_interruptFlags" } }, showHex = true }),
			SE.CreateFlagList(page, { title = "Aura Interrupt Flags", words = { { column = "m_auraInterruptFlags" } }, showHex = true }),
			SE.CreateFlagList(page, { title = "Channel Interrupt Flags", words = { { column = "m_channelInterruptFlags" } }, showHex = true }),
		};
		ListsBelow(page, FilterBar(page, lists, false), lists);
	end

	local function AttributePage(page, first, last, extra)
		local lists = {};

		for index = first, last do
			local word = ATTRIBUTE_WORDS[index];
			table.insert(lists, SE.CreateFlagList(page, { title = word.title, words = { { column = word.column } }, allBits = true, showHex = true }));
		end

		local bar = FilterBar(page, lists, true);
		ListsBelow(page, bar, lists, 8, extra);
		return lists;
	end

	AttributePage(pages[2], 1, 4);

	local custom = BuildCustomAttributes(pages[3]);
	local customLists = AttributePage(pages[3], 5, 8, { { frame = custom, weight = 2 } });

	for _, list in ipairs(custom.lists) do
		table.insert(customLists, list);
	end

	do
		local page = pages[4];
		local lists = {
			SE.CreateFlagList(page, { title = "Creature Type", tooltip = "Creature types the target must be one of. Nothing checked means any", words = { { column = "m_targetCreatureType" } } }),
			SE.CreateFlagList(page, { title = "Stances", tooltip = "Shapeshift forms this spell can be cast in. Nothing checked means any form",
				words = { { column = "m_shapeshiftMask[0]" }, { column = "m_shapeshiftMask[1]", offset = 32 } } }),
			SE.CreateFlagList(page, { title = "Stances Not", tooltip = "Shapeshift forms this spell cannot be cast in. Nothing checked means no form is excluded",
				words = { { column = "m_shapeshiftExclude[0]" }, { column = "m_shapeshiftExclude[1]", offset = 32 } } }),
		};
		ListsBelow(page, FilterBar(page, lists, false), lists);
	end

	return frame;
end

do
	local columns = { "m_interruptFlags", "m_auraInterruptFlags", "m_channelInterruptFlags", "m_targetCreatureType",
		"m_shapeshiftMask[0]", "m_shapeshiftMask[1]", "m_shapeshiftExclude[0]", "m_shapeshiftExclude[1]" };

	for _, word in ipairs(ATTRIBUTE_WORDS) do
		table.insert(columns, word.column);
	end

	SE.AddSection({
		key = "flags",
		title = "Flags",
		order = 60,
		columns = columns,
		build = BuildFlags,
		badge = function()
			local changed = SE.cur and SE.ChangeCount(columns) or 0;

			if ( customEditor and customEditor:IsDirty() ) then
				return changed > 0 and format("|cffffcc00%d+|r", changed) or "|cffffcc00+|r";
			end

			return changed > 0 and format("|cffffcc00%d|r", changed) or nil;
		end,
	});
end

-- Icon -------------------------------------------------------------------------------------------------

local ICON_GAP = 4;

local function BuildIcon(parent)
	local frame = CreateFrame("Frame", "GameEditorSpellIcon", parent);
	frame.size = 40;
	frame.offset = 0;
	frame.items = {};
	frame.buttons = {};
	frame.target = "m_spellIconID";

	local resetSpell = SE.Button(frame, "Reset Spell Icon ID", 130, function()
		if ( SE.cur ) then
			SE.SetValue("m_spellIconID", SE.cur.original.m_spellIconID);
		end
	end, "Back to the saved spell icon");
	resetSpell:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, 0);
	local resetActive = SE.Button(frame, "Reset Active Icon ID", 134, function()
		if ( SE.cur ) then
			SE.SetValue("m_activeIconID", SE.cur.original.m_activeIconID);
		end
	end, "Back to the saved active icon");
	resetActive:SetPoint("LEFT", resetSpell, "RIGHT", 6, 0);

	local function IconBox(label, column, x)
		local title = SE.Label(frame, label);
		title:SetPoint("TOPLEFT", frame, "TOPLEFT", x, -34);
		local box = CreateFrame("Button", NoggitUI_NewName("GameEditorSpellIconCurrent"), frame, "NoggitBoxTemplate");
		box:SetWidth(64);
		box:SetHeight(64);
		box:SetPoint("TOPLEFT", title, "BOTTOMLEFT", 0, -4);
		box.icon = box:CreateTexture(nil, "ARTWORK");
		box.icon:SetPoint("TOPLEFT", box, "TOPLEFT", 3, -3);
		box.icon:SetPoint("BOTTOMRIGHT", box, "BOTTOMRIGHT", -3, 3);
		box.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93);
		box.detail = SE.Label(frame, "", "NoggitFont10", NOGGIT_COLOR_DISABLED);
		box.detail:SetPoint("TOPLEFT", box, "BOTTOMLEFT", 0, -3);
		box.detail:SetWidth(150);
		box.column = column;
		box:SetScript("OnClick", function()
			frame.target = column;
			frame:RefreshCurrent();
		end);
		SE.Tooltip(box, format("Click to make icon clicks below set the %s", string.lower(label)));
		return box;
	end

	frame.spellBox = IconBox("Current Icon", "m_spellIconID", 0);
	frame.activeBox = IconBox("Active Icon", "m_activeIconID", 160);

	local filterLabel = SE.Label(frame, "Filter icons:");
	filterLabel:SetPoint("TOPLEFT", frame, "TOPLEFT", 330, -34);
	frame.search = SE.SearchBox(frame, 200, "File name or id", function()
		frame.pending = 0.3;
	end);
	frame.search:SetPoint("TOPLEFT", filterLabel, "BOTTOMLEFT", 0, -4);
	frame.search:SetScript("OnEnterPressed", function(self)
		self:ClearFocus();
		frame.pending = nil;
		frame:Search();
	end);

	local sizeLabel = SE.Label(frame, "Icon size");
	sizeLabel:SetPoint("TOPLEFT", frame.search, "BOTTOMLEFT", 0, -8);
	local slider = NoggitSlider_Create(frame, 140, 24, 72, 4, function(_, value)
		frame.size = floor(value + 0.5);
		frame:LayoutIcons();
	end);
	slider:SetPoint("LEFT", sizeLabel, "RIGHT", 10, 0);
	NoggitSlider_SetValueQuiet(slider, frame.size);
	SE.Tooltip(slider, "Controls icon width and height in pixels");

	frame.countText = SE.Label(frame, "", nil, NOGGIT_COLOR_DISABLED, "RIGHT");
	frame.countText:SetPoint("TOPRIGHT", frame, "TOPRIGHT", 0, -38);
	frame.hint = SE.Label(frame, "", nil, NOGGIT_COLOR_DISABLED, "RIGHT");
	frame.hint:SetPoint("TOPRIGHT", frame.countText, "BOTTOMRIGHT", 0, -6);

	local grid = SE.Box(frame, "GameEditorSpellIconGrid", NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID);
	grid:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, -136);
	grid:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", 0, 0);
	grid:EnableMouseWheel(true);
	frame.grid = grid;

	local function IconButton()
		local button = NoggitTextureButton_Create(grid, frame.size);
		button.image:ClearAllPoints();
		button.image:SetPoint("TOPLEFT", button, "TOPLEFT", 2, -2);
		button.image:SetPoint("BOTTOMRIGHT", button, "BOTTOMRIGHT", -2, 2);
		button.image:SetTexCoord(0.07, 0.93, 0.07, 0.93);
		button:SetScript("OnEnter", function(self)
			self.hover = true;
			NoggitTextureButton_Refresh(self);

			if ( self.item ) then
				NoggitTooltip_Show(self, format("%d\n|cff8f8f8f%s|r", self.item.id, self.item.name));
			end
		end);
		button:SetScript("OnLeave", function(self)
			self.hover = false;
			NoggitTextureButton_Refresh(self);
			NoggitTooltip_Hide();
		end);
		button:SetScript("OnClick", function(self)
			if ( self.item and SE.cur ) then
				SE.SetValue(frame.target, self.item.id);
			end
		end);
		table.insert(frame.buttons, button);
		return button;
	end

	frame.LayoutIcons = function(self)
		local width = (grid:GetWidth() or 0) - 8;
		local height = (grid:GetHeight() or 0) - 8;
		local step = self.size + ICON_GAP;
		self.columns = max(1, floor((width + ICON_GAP) / step));
		self.rows = max(1, floor((height + ICON_GAP) / step));
		local perPage = self.columns * self.rows;
		local maxOffset = max(0, ceil(#self.items / self.columns) - self.rows);
		self.offset = max(0, min(maxOffset, self.offset));
		local current = SE.Number(SE.GetValue(self.target));

		for index = 1, max(perPage, #self.buttons) do
			local button = self.buttons[index];

			if ( index <= perPage ) then
				button = button or IconButton();
				local item = self.items[self.offset * self.columns + index];
				button:SetWidth(self.size);
				button:SetHeight(self.size);
				button:ClearAllPoints();
				button:SetPoint("TOPLEFT", grid, "TOPLEFT", 4 + mod(index - 1, self.columns) * step, -4 - floor((index - 1) / self.columns) * step);

				if ( item ) then
					button.item = item;
					button.image:SetTexture(item.name);
					button.selected = item.id == current;
					NoggitTextureButton_Refresh(button);
					button:Show();
				else
					button.item = nil;
					button:Hide();
				end
			elseif ( button ) then
				button.item = nil;
				button:Hide();
			end
		end

		local first = self.offset * self.columns + 1;
		local last = min(#self.items, first + perPage - 1);
		self.countText:SetText(#self.items > 0 and format("Icons %d to %d of %d", first, last, #self.items) or "No icon matches");
	end;

	frame.Search = function(self)
		local text = strtrim(self.search:GetText() or "");
		local number = tonumber(text);
		self.items = {};

		if ( number ) then
			local row = SE.GetRow("SpellIcon", number);

			if ( row ) then
				table.insert(self.items, { id = number, name = row.m_textureFilename or SE.IconPath(number) });
			end
		end

		for _, row in ipairs(SE.Filter("SpellIcon", number and "" or text, nil)) do
			if ( not number or row.id ~= number ) then
				if ( not number or string.find(tostring(row.id), text, 1, true) ) then
					table.insert(self.items, { id = row.id, name = row.name });
				end
			end
		end

		self.offset = 0;
		self:LayoutIcons();
	end;

	frame.RefreshCurrent = function(self)
		for _, box in ipairs({ self.spellBox, self.activeBox }) do
			local id = SE.Number(SE.GetValue(box.column));

			if ( id == 0 ) then
				box.icon:SetTexture(0.1, 0.1, 0.1, 1);
				box.detail:SetText("none");
			else
				local path = SE.IconPath(id);
				box.icon:SetTexture(path);
				box.detail:SetText(format("%d  %s", id, SE.IconShortName(path)));
			end

			NoggitBox_Set(box, NOGGIT_COLOR_DARK, box.column == self.target and NOGGIT_COLOR_ACCENT or NOGGIT_COLOR_MID);
		end

		self.hint:SetText(self.target == "m_spellIconID" and "A click sets the spell icon" or "A click sets the active icon");
		self:LayoutIcons();
	end;

	grid:SetScript("OnMouseWheel", function(_, delta)
		frame.offset = frame.offset - delta * 2;
		frame:LayoutIcons();
	end);
	grid:SetScript("OnSizeChanged", function() frame:LayoutIcons(); end);

	frame:SetScript("OnUpdate", function(self, elapsed)
		if ( self.pending ) then
			self.pending = self.pending - elapsed;

			if ( self.pending <= 0 ) then
				self.pending = nil;
				self:Search();
			end
		end
	end);

	frame:SetScript("OnShow", function(self)
		if ( not self.searched ) then
			self.searched = true;
			self:Search();
		end

		self:RefreshCurrent();
	end);

	SE.On("VALUE_CHANGED", function(column)
		if ( frame:IsVisible() and (column == "m_spellIconID" or column == "m_activeIconID") ) then
			frame:RefreshCurrent();
		end
	end);
	SE.On("SPELL_LOADED", function()
		if ( frame:IsVisible() ) then
			frame:RefreshCurrent();
		end
	end);

	return frame;
end

SE.AddSection({ key = "icon", title = "Icon", order = 70, columns = { "m_spellIconID", "m_activeIconID" }, build = BuildIcon });

-- Reqs -------------------------------------------------------------------------------------------------

local REQS = {
	{ "m_excludeCasterAuraState", "Caster Aura State Not" },
	{ "m_excludeTargetAuraState", "Target Aura State Not" },
	{ "m_casterAuraSpell", "Caster Aura Spell" },
	{ "m_targetAuraSpell", "Target Aura Spell" },
	{ "m_excludeCasterAuraSpell", "Exclude Caster Aura Spell" },
	{ "m_excludeTargetAuraSpell", "Exclude Target Aura Spell" },
	{ "m_minFactionID", "Minimum Faction Id" },
	{ "m_minReputation", "Minimum Reputation" },
	{ "m_requiredAuraVision", "Required Aura Vision" },
};

local function BuildReqs(parent)
	local frame = CreateFrame("Frame", "GameEditorSpellReqs", parent);
	local grid = SE.FieldColumn(frame, REQS, { labelWidth = 190, editWidth = 90 });
	local spacer = Pane(frame);
	SE.SideBySide(frame, { { frame = grid.frame, weight = 1.2 }, { frame = spacer, weight = 1 } }, 14);
	SyncOnShow(frame, { grid });
	return frame;
end

SE.AddSection({ key = "reqs", title = "Reqs", order = 80, columns = SpecColumns(REQS), build = BuildReqs });
