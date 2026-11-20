-- Visual tab (SpellVisual, kits, missile), the $ variable helpers of the description fields and the all fields grid.

local SE = GameEditorSpell;
local Data = GameEditorData or {};

local KIT_LABELS = {
	m_precastKit = "Precast", m_castKit = "Cast", m_impactKit = "Impact", m_stateKit = "State", m_stateDoneKit = "State done",
	m_channelKit = "Channel", m_casterImpactKit = "Caster impact", m_targetImpactKit = "Target impact",
	m_missileTargetingKit = "Missile target", m_instantAreaKit = "Instant area", m_impactAreaKit = "Impact area",
	m_persistentAreaKit = "Persistent area",
};

local VISUAL_GROUPS = {
	{ title = "Visual", columns = { "m_id", "m_flags", "m_animEventSoundID" } },
	{ title = "Kits", columns = SE.VISUAL_KIT_COLUMNS },
	{ title = "Missile", columns = { "m_hasMissile", "m_missileModel", "m_missilePathType", "m_missileMotion", "m_missileSound", "m_missileAttachment",
		"m_missileDestinationAttachment", "m_missileFollowGroundHeight", "m_missileFollowGroundDropSpeed", "m_missileFollowGroundApproach", "m_missileFollowGroundFlags",
		"m_missileCastOffset[0]", "m_missileCastOffset[1]", "m_missileCastOffset[2]", "m_missileImpactOffset[0]", "m_missileImpactOffset[1]", "m_missileImpactOffset[2]" } },
};

local KIT_GROUPS = {
	{ title = "Kit", columns = { "m_id", "m_flags" } },
	{ title = "Animation", columns = { "m_startAnimID", "m_animID" } },
	{ title = "Effects", columns = { "m_headEffect", "m_chestEffect", "m_baseEffect", "m_leftHandEffect", "m_rightHandEffect", "m_breathEffect",
		"m_leftWeaponEffect", "m_rightWeaponEffect", "m_specialEffect[0]", "m_specialEffect[1]", "m_specialEffect[2]", "m_worldEffect" } },
	{ title = "Sound and camera", columns = { "m_soundID", "m_shakeID" } },
	{ title = "Procedures", columns = { "m_charProc[0]", "m_charProc[1]", "m_charProc[2]", "m_charProc[3]",
		"m_charParamZero[0]", "m_charParamZero[1]", "m_charParamZero[2]", "m_charParamZero[3]", "m_charParamOne[0]", "m_charParamOne[1]", "m_charParamOne[2]", "m_charParamOne[3]",
		"m_charParamTwo[0]", "m_charParamTwo[1]", "m_charParamTwo[2]", "m_charParamTwo[3]", "m_charParamThree[0]", "m_charParamThree[1]", "m_charParamThree[2]", "m_charParamThree[3]" } },
};

local isKitColumn = {};

for _, column in ipairs(SE.VISUAL_KIT_COLUMNS) do
	isKitColumn[column] = true;
end

-- Kit and missile windows ------------------------------------------------------------------------------

local kitWindow;
local missileWindow;
local visualEditor;

local function CreateKitWindow()
	kitWindow = GameEditor_CreateWindow("GameEditorSpellKitWindow", "Spell visual kit", 560, 640, true);
	kitWindow:ClearAllPoints();
	kitWindow:SetPoint("TOPRIGHT", EditorMainArea, "TOPRIGHT", -10, -40);
	kitWindow.editor = SE.CreateDbcEditor(kitWindow.content, "SpellVisualKit", {
		title = "Spell visual kit",
		groups = KIT_GROUPS,
		labelWidth = 190,
		emptyText = "This kit slot of the visual is empty. Create a new kit to fill it.",
		onCreated = function(_, id)
			if ( visualEditor and kitWindow.column and (visualEditor.exists or visualEditor.isNew) ) then
				visualEditor.values[kitWindow.column] = id;
				visualEditor:Bind();
				GameEditor_Notify(format("%s of visual %d now points to the new kit %d. Neither is saved yet, save both rows.", kitWindow.column, visualEditor.id, id), "info");
			end
		end,
		onIdEntered = function(_, id)
			if ( visualEditor and kitWindow.column and (visualEditor.exists or visualEditor.isNew) ) then
				visualEditor.values[kitWindow.column] = id;
				visualEditor:Bind();
			end
		end,
	});
	kitWindow.editor.frame:SetAllPoints(kitWindow.content);
end

function SE.OpenKit(column)
	if ( not visualEditor or not (visualEditor.exists or visualEditor.isNew) ) then
		GameEditor_Notify("Open or create a spell visual first.", "error");
		return;
	end

	if ( not kitWindow ) then
		CreateKitWindow();
	end

	kitWindow.column = column;
	kitWindow:SetTitle(format("%s kit of spell visual %d", KIT_LABELS[column] or column, visualEditor.id));
	kitWindow.editor:Load(SE.Number(visualEditor.values[column]));
	GameEditor_ShowWindow(kitWindow);
end

local function CreateMissileWindow()
	missileWindow = GameEditor_CreateWindow("GameEditorSpellMissileWindow", "Spell missile", 480, 460, true);
	missileWindow:ClearAllPoints();
	missileWindow:SetPoint("TOPRIGHT", EditorMainArea, "TOPRIGHT", -30, -60);
	missileWindow.editor = SE.CreateDbcEditor(missileWindow.content, "SpellMissile", {
		title = "Spell missile",
		labelWidth = 170,
		emptyText = "The spell has no SpellMissile row. Create one to tune the missile flight.",
		onCreated = function(_, id)
			SE.SetValue("m_spellMissileID", id);
		end,
		onIdEntered = function(_, id)
			SE.SetValue("m_spellMissileID", id);
		end,
	});
	missileWindow.editor.frame:SetAllPoints(missileWindow.content);
end

function SE.OpenMissile()
	if ( not SE.cur ) then
		GameEditor_Notify("Open a spell first to edit its missile.", "error");
		return;
	end

	if ( not missileWindow ) then
		CreateMissileWindow();
	end

	missileWindow.editor:Load(SE.Number(SE.GetValue("m_spellMissileID")));
	GameEditor_ShowWindow(missileWindow);
end

-- Visuals section -------------------------------------------------------------------------------------

local function BuildVisuals(parent)
	local frame = CreateFrame("Frame", "GameEditorSpellVisuals", parent);
	local y = SE.SectionTitle(frame, "Visual", "The SpellVisual rows with their kits, and the missile. Visual rows save on their own, the spell only stores the ids.", 0);

	local grid = SE.FieldColumn(frame, {
		{ "m_spellVisualID[0]", "Spell Visual 1" },
		{ "m_spellVisualID[1]", "Spell Visual 2" },
		{ "m_spellMissileID", "Spell Missile ID" },
		{ "m_speed", "Speed" },
	}, { labelWidth = 150, editWidth = 90 });
	grid.frame:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, y);
	grid.frame:SetPoint("TOPRIGHT", frame, "TOPRIGHT", 0, y);
	grid.frame:SetHeight(4 * 24 + 6);
	y = y - 4 * 24 - 12;

	local separator = NoggitSeparator_Create(frame);
	separator:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, y);
	separator:SetPoint("TOPRIGHT", frame, "TOPRIGHT", 0, y);
	y = y - 8;

	frame.slot = 1;
	local tabHolder = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellVisualTabs"), frame);
	tabHolder:SetHeight(24);
	tabHolder:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, y);
	tabHolder:SetPoint("TOPRIGHT", frame, "TOPRIGHT", 0, y);
	local tabs = NoggitTabs_Create(tabHolder, { "Spell visual 1", "Spell visual 2" }, 1, function(index)
		frame.slot = index;
		frame:LoadVisual();
	end);
	tabs[1]:SetPoint("LEFT", tabHolder, "LEFT", 0, 0);

	local missile = SE.Button(tabHolder, "Missile", 70, function() SE.OpenMissile(); end, "Edit the SpellMissile row of this spell (missile speed, gravity, collision)");
	missile:SetPoint("RIGHT", tabHolder, "RIGHT", 0, 0);
	y = y - 30;

	local kitHolder = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellKitButtons"), frame);
	kitHolder:SetHeight(52);
	kitHolder:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, y);
	kitHolder:SetPoint("TOPRIGHT", frame, "TOPRIGHT", 0, y);
	frame.kitButtons = {};

	for _, column in ipairs(SE.VISUAL_KIT_COLUMNS) do
		local button = SE.Button(kitHolder, KIT_LABELS[column], 80, function() SE.OpenKit(column); end);
		button.column = column;
		SE.Tooltip(button, function()
			local id = visualEditor and SE.Number(visualEditor.values[column]) or 0;
			return format("%s kit\n|cff8f8f8f%s|r\n\n%s", KIT_LABELS[column], column, id > 0 and ("Kit "..id..", click to edit it") or "Empty, click to create one");
		end);
		table.insert(frame.kitButtons, button);
	end

	local function LayoutKits()
		local perRow = 6;
		local width = floor(((kitHolder:GetWidth() or 480) - (perRow - 1) * 4) / perRow);

		for index, button in ipairs(frame.kitButtons) do
			local row = floor((index - 1) / perRow);
			local column = mod(index - 1, perRow);
			button:SetWidth(width);
			button:ClearAllPoints();
			button:SetPoint("TOPLEFT", kitHolder, "TOPLEFT", column * (width + 4), -row * 26);
		end
	end

	kitHolder:SetScript("OnSizeChanged", LayoutKits);
	y = y - 58;

	visualEditor = SE.CreateDbcEditor(frame, "SpellVisual", {
		title = "Spell visual",
		groups = VISUAL_GROUPS,
		labelWidth = 200,
		emptyText = "This visual slot of the spell is empty. Create a visual, or type an existing id and press Enter.",
		onRowContext = function(_, column)
			if ( isKitColumn[column] ) then
				SE.OpenKit(column);
			end
		end,
		onCreated = function(_, id)
			SE.SetValue("m_spellVisualID["..(frame.slot - 1).."]", id);
		end,
		onIdEntered = function(_, id)
			SE.SetValue("m_spellVisualID["..(frame.slot - 1).."]", id);
		end,
		onStatus = function()
			if ( frame.RefreshKits ) then
				frame:RefreshKits();
			end
		end,
	});
	visualEditor.frame:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, y);
	visualEditor.frame:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", 0, 0);
	frame.visualEditor = visualEditor;

	frame.RefreshKits = function(self)
		local valid = visualEditor.exists or visualEditor.isNew;

		for _, button in ipairs(self.kitButtons) do
			local id = valid and SE.Number(visualEditor.values[button.column]) or 0;
			button:GetFontString():SetTextColor(unpack(id > 0 and NOGGIT_COLOR_TEXT or NOGGIT_COLOR_DISABLED));
			SE.SetEnabled(button, valid);
		end
	end;

	frame.LoadVisual = function(self, force)
		visualEditor:Load(SE.Number(SE.GetValue("m_spellVisualID["..(self.slot - 1).."]")), force);
		self:RefreshKits();
	end;

	frame.Refresh = function(self)
		SE.SyncGrid(grid);
	end;

	frame:SetScript("OnShow", function(self)
		LayoutKits();
		self:Refresh();
		self:LoadVisual();
	end);

	SE.On("SPELL_LOADED", function()
		frame:LoadVisual(not visualEditor:IsDirty() or visualEditor.pending);

		if ( frame:IsVisible() ) then
			frame:Refresh();
		end

		if ( kitWindow and kitWindow:IsShown() and not kitWindow.editor:IsDirty() ) then
			GameEditor_HideWindow(kitWindow);
		end

		if ( missileWindow and missileWindow:IsShown() ) then
			missileWindow.editor:Load(SE.Number(SE.GetValue("m_spellMissileID")));
		end
	end);

	SE.On("VALUE_CHANGED", function(column)
		if ( column == "m_spellVisualID["..(frame.slot - 1).."]" ) then
			frame:LoadVisual();
		end
	end);

	frame:LoadVisual(true);
	return frame;
end

SE.AddSection({
	key = "visuals",
	title = "Visual",
	order = 90,
	columns = { "m_spellVisualID[0]", "m_spellVisualID[1]", "m_spellMissileID", "m_speed" },
	build = BuildVisuals,
	badge = function()
		if ( visualEditor and visualEditor:IsDirty() ) then
			return "|cffffcc00visual|r";
		end
	end,
});

-- Description variables: the picker window and the $ popup ------------------------------------------

-- Mirrors SE.TOKEN_HELP, the letters SE.ResolveText understands.
local EFFECT_TOKENS = {
	{ "s", "Effect %d value, a range when Die Sides is above 1" },
	{ "m", "Effect %d minimum value" },
	{ "M", "Effect %d maximum value" },
	{ "o", "Effect %d total over the duration, value times ticks" },
	{ "t", "Effect %d tick period in seconds" },
	{ "a", "Effect %d radius in yards" },
	{ "x", "Effect %d chain targets" },
	{ "q", "Effect %d misc value" },
	{ "e", "Effect %d value multiplier" },
	{ "b", "Effect %d points per combo point" },
	{ "f", "Effect %d chain damage multiplier" },
};

local SPELL_TOKENS = {
	{ "d", "Duration" },
	{ "r", "Range in yards" },
	{ "h", "Proc chance" },
	{ "n", "Proc charges" },
	{ "u", "Maximum stacks" },
	{ "i", "Maximum targets" },
	{ "v", "Maximum target level" },
	{ "c", "Power cost" },
	{ "p", "Power per second" },
	{ "z", "Hearthstone location" },
};

local STAT_TOKENS = {
	{ "ap", "Attack power of your character" },
	{ "rap", "Ranged attack power" },
	{ "sp", "Spell power" },
	{ "sph", "Healing spell power" },
	{ "spd", "Spell damage" },
	{ "spfi", "Fire spell power" },
	{ "spn", "Nature spell power" },
	{ "spfr", "Frost spell power" },
	{ "sps", "Shadow spell power" },
	{ "spa", "Arcane spell power" },
	{ "pl", "Your level" },
	{ "mws", "Main hand weapon speed" },
};

local SYNTAX_TOKENS = {
	{ key = "{", token = "${}", label = "${...}", back = 1, text = "Math, for example ${$m1*2}, add .1 after the } for one decimal" },
	{ key = "/", token = "$/10;s1", label = "$/10;s1", text = "Divides the next value, here effect 1 by 10" },
	{ key = "*", token = "$*3;s1", label = "$*3;s1", text = "Multiplies the next value" },
	{ key = "l", token = "$lpoint:points;", label = "$lone:many;", text = "Singular or plural from the last number" },
	{ key = "g", token = "$gmale:female;", label = "$gmale:female;", text = "Text by the player's gender" },
	{ key = "?", token = "$?s12345[known][not known]", label = "$?s12345[a][b]", text = "Text that depends on a known spell" },
	{ key = "<", token = "$<>", label = "$<name>", back = 1, text = "Variable from the Description Variables row" },
};

local OTHER_SPELL_LETTERS = { "s1", "s2", "s3", "m1", "M1", "o1", "o2", "o3", "t1", "a1", "d", "r", "h", "n", "u" };

-- Without highlight the resolver hands an unknown token back unchanged.
local function Resolve(token, values)
	local ok, text = pcall(SE.ResolveText, token, values or {});

	if ( not ok or not text or text == "" or text == token ) then
		return "|cffff5050?|r";
	end

	return text;
end

local function DescriptionVariables(values)
	local names = {};
	local id = SE.Number(values.m_descriptionVariablesID);
	local row = id > 0 and SE.GetRow("SpellDescriptionVariables", id);

	if ( row and row.m_variables ) then
		for name in string.gmatch(tostring(row.m_variables), "%$([%w_]+)=") do
			table.insert(names, name);
		end
	end

	return names, id;
end

-- Every token with its description and its value for the open spell.
local function BuildTokens(values)
	values = values or {};
	local items = {};

	local function Add(key, description, resolve, extra)
		local item = extra or {};
		item.key = key;
		item.token = item.token or ("$"..key);
		item.label = item.label or item.token;
		item.text = description;

		if ( resolve ) then
			item.value = Resolve(item.token, values);
		end

		table.insert(items, item);
		return item;
	end

	for _, definition in ipairs(EFFECT_TOKENS) do
		for index = 1, 3 do
			local item = Add(definition[1]..index, format(definition[2], index), true);

			if ( SE.Number(values["m_effect["..(index - 1).."]"]) == 0 ) then
				item.text = item.text.."  |cff7f7f7f(effect "..index.." is empty)|r";
				item.dim = true;
			end
		end
	end

	for _, definition in ipairs(SPELL_TOKENS) do
		Add(definition[1], definition[2], true);
	end

	local names, variableId = DescriptionVariables(values);

	for _, name in ipairs(names) do
		Add("<"..name..">", format("Description variable of row %d", variableId), true, { token = "$<"..name..">" });
	end

	for _, definition in ipairs(STAT_TOKENS) do
		Add(definition[1], definition[2], true);
	end

	for _, definition in ipairs(SYNTAX_TOKENS) do
		Add(definition.key, definition.text, false, { token = definition.token, label = definition.label, back = definition.back });
	end

	Add("#", "Value of another spell, pick the spell, or type $ and its id", false, { token = "$12345s1", label = "$<spellid>s1", pickSpell = true });
	return items;
end

-- Tokens of another spell, $12345s1 style.
local function OtherSpellTokens(id, values)
	local items = {};
	local name = SE.SpellName(id);
	local exists = name ~= nil or SE.RowExists(SE.TABLE, id);
	local title = exists and format("Spell %d %s", id, name or "") or format("|cffff6060Spell %d does not exist|r", id);

	for _, letter in ipairs(OTHER_SPELL_LETTERS) do
		local token = "$"..id..letter;
		table.insert(items, { key = id..letter, token = token, label = token, text = title, value = exists and Resolve(token, values) or nil });
	end

	return items;
end

-- Popup filter: tokens that start with what was typed after the $, exact case first.
local function FilterPrefix(tokens, prefix, values)
	local _, _, id = string.find(prefix, "^(%d+)");

	if ( id ) then
		local result = {};

		for _, item in ipairs(OtherSpellTokens(tonumber(id), values)) do
			if ( string.sub(item.key, 1, #prefix) == prefix ) then
				table.insert(result, item);
			end
		end

		return result;
	end

	local exact, loose = {}, {};
	local lower = string.lower(prefix);

	for _, item in ipairs(tokens) do
		if ( item.key ~= "#" ) then
			if ( string.sub(item.key, 1, #prefix) == prefix ) then
				table.insert(exact, item);
			elseif ( string.sub(string.lower(item.key), 1, #lower) == lower ) then
				table.insert(loose, item);
			end
		end
	end

	if ( prefix == "" ) then
		for _, item in ipairs(tokens) do
			if ( item.key == "#" ) then
				table.insert(exact, item);
			end
		end
	end

	for _, item in ipairs(loose) do
		table.insert(exact, item);
	end

	return exact;
end

-- Window filter: any part of the token or the description.
local function FilterText(tokens, text, values)
	text = SE.Lower(strtrim(text or ""));

	if ( text == "" ) then
		return tokens;
	end

	local plain = string.gsub(text, "^%$", "");
	local _, _, id = string.find(plain, "^(%d+)");

	if ( id ) then
		return FilterPrefix(tokens, plain, values);
	end

	local result = {};

	for _, item in ipairs(tokens) do
		if ( string.find(SE.Lower(item.label), plain, 1, true) or string.find(SE.Lower(item.text), plain, 1, true) ) then
			table.insert(result, item);
		end
	end

	return result;
end

-- A short list with keyboard selection, shared by the popup and the window.
local function CreateTokenList(parent, prefix, rowCount, rowHeight, onPick)
	local list = { rows = {}, items = {}, offset = 0, selected = 1, rowCount = rowCount };

	local function RefreshRows()
		for index, row in ipairs(list.rows) do
			local itemIndex = list.offset + index;
			local item = list.items[itemIndex];
			row.itemIndex = itemIndex;

			if ( item ) then
				row.token:SetText("|cffffcc00"..item.label.."|r");
				row.text:SetText(item.text or "");
				row.value:SetText(item.value and item.value ~= "" and item.value or "");
				NoggitText_SetColor(row.text, item.dim and NOGGIT_COLOR_DISABLED or NOGGIT_COLOR_TEXT);

				if ( itemIndex == list.selected ) then
					row.bg:SetTexture(NOGGIT_COLOR_ACCENT[1], NOGGIT_COLOR_ACCENT[2], NOGGIT_COLOR_ACCENT[3], 0.55);
				elseif ( row.hover ) then
					row.bg:SetTexture(NOGGIT_COLOR_HOVER[1], NOGGIT_COLOR_HOVER[2], NOGGIT_COLOR_HOVER[3], 1);
				else
					row.bg:SetTexture(0, 0, 0, 0);
				end

				row:Show();
			else
				row:Hide();
			end
		end
	end

	for index = 1, rowCount do
		local row = CreateFrame("Button", prefix.."Row"..index, parent);
		row:SetHeight(rowHeight);
		row.bg = row:CreateTexture(nil, "BACKGROUND");
		row.bg:SetAllPoints(row);
		row.token = SE.Label(row, "", "NoggitFontBold");
		row.token:SetPoint("LEFT", row, "LEFT", 6, 0);
		row.token:SetWidth(118);
		row.token:SetHeight(rowHeight);
		row.value = SE.Label(row, "", nil, NOGGIT_COLOR_WHITE, "RIGHT");
		row.value:SetPoint("RIGHT", row, "RIGHT", -6, 0);
		row.value:SetWidth(110);
		row.value:SetHeight(rowHeight);
		row.text = SE.Label(row, "");
		row.text:SetPoint("LEFT", row.token, "RIGHT", 4, 0);
		row.text:SetPoint("RIGHT", row.value, "LEFT", -6, 0);
		row.text:SetHeight(rowHeight);
		row:SetScript("OnEnter", function(self) self.hover = true; RefreshRows(); end);
		row:SetScript("OnLeave", function(self) self.hover = false; RefreshRows(); end);
		row:SetScript("OnClick", function(self)
			local item = list.items[self.itemIndex];

			if ( item ) then
				onPick(item);
			end
		end);
		row:EnableMouseWheel(true);
		row:SetScript("OnMouseWheel", function(_, delta) list:Scroll(-delta); end);
		list.rows[index] = row;
	end

	function list:SetItems(items)
		self.items = items or {};
		self.offset = 0;
		self.selected = #self.items > 0 and 1 or nil;
		RefreshRows();
	end

	function list:Scroll(delta)
		self.offset = max(0, min(max(0, #self.items - self.rowCount), self.offset + delta));
		RefreshRows();
	end

	function list:Move(delta)
		if ( #self.items == 0 ) then
			return;
		end

		self.selected = max(1, min(#self.items, (self.selected or 0) + delta));

		if ( self.selected <= self.offset ) then
			self.offset = self.selected - 1;
		elseif ( self.selected > self.offset + self.rowCount ) then
			self.offset = self.selected - self.rowCount;
		end

		RefreshRows();
	end

	function list:Selected()
		return self.selected and self.items[self.selected];
	end

	function list:Layout(left, top, right)
		for index, row in ipairs(self.rows) do
			row:ClearAllPoints();
			row:SetPoint("TOPLEFT", parent, "TOPLEFT", left, top - (index - 1) * rowHeight);
			row:SetPoint("TOPRIGHT", parent, "TOPRIGHT", right, top - (index - 1) * rowHeight);
		end
	end

	return list;
end

local function CurrentValues()
	return SE.cur and SE.cur.values or {};
end

-- Puts text at a byte position of an edit box as if it was typed, so the field commits it like an edit.
local function InsertAt(edit, position, text, back, replaceTo)
	edit:SetFocus();

	if ( replaceTo ) then
		edit:HighlightText(position, replaceTo);
		edit:SetCursorPosition(position);
	else
		edit:SetCursorPosition(position);
	end

	edit:Insert(text);

	if ( back ) then
		edit:SetCursorPosition(max(0, edit:GetCursorPosition() - back));
	end
end

local function PickOtherSpell(edit, position, replaceTo, expected)
	SE.PickSpell("Value from another spell", function(id)
		id = tonumber(id);

		if ( not id ) then
			GameEditor_Notify("That is not a valid spell id.", "error");
			return;
		end

		if ( not edit:IsVisible() ) then
			GameEditor_Notify("The text field was closed, nothing was inserted.", "info");
			return;
		end

		local text = edit:GetText() or "";

		if ( replaceTo and string.sub(text, position + 1, replaceTo) ~= expected ) then
			replaceTo = nil;
		end

		InsertAt(edit, min(position, #text), "$"..id.."s1", nil, replaceTo);
	end);
end

-- Popup under a field while a $ token is typed ---------------------------------------------------

local popup;

local function PopupOwner()
	return popup and popup:IsShown() and popup.edit or nil;
end

local function ClosePopup()
	if ( not popup or not popup.edit ) then
		if ( popup ) then
			popup:Hide();
		end

		return;
	end

	local edit = popup.edit;
	popup.edit = nil;
	popup.catcher:Hide();
	popup:Hide();
	edit:SetAltArrowKeyMode(popup.altArrows and true or false);
	edit:SetScript("OnEnterPressed", popup.enterScript);
	edit:SetScript("OnEscapePressed", popup.escapeScript);
	popup.enterScript = nil;
	popup.escapeScript = nil;
end

local function PickFromPopup(item)
	local edit = popup.edit;

	if ( not edit or not item ) then
		ClosePopup();
		return;
	end

	local start, finish = popup.start, edit:GetCursorPosition();
	local expected = string.sub(edit:GetText() or "", start + 1, finish);
	ClosePopup();

	if ( string.sub(expected, 1, 1) ~= "$" ) then
		return;
	end

	if ( item.pickSpell ) then
		PickOtherSpell(edit, start, finish, expected);
		return;
	end

	InsertAt(edit, start, item.token, item.back, finish);
end

-- Moves the cursor one letter, the arrow keys go to the popup while it is open.
local function StepCursor(edit, direction)
	local text = edit:GetText() or "";
	local position = edit:GetCursorPosition();

	repeat
		position = position + direction;
		local byte = string.byte(text, position + 1);
	until ( position <= 0 or position >= #text or not byte or byte < 128 or byte >= 192 )

	edit:SetCursorPosition(max(0, min(#text, position)));
end

local function PopupKey(key)
	local edit = PopupOwner();

	if ( not edit ) then
		return false;
	end

	if ( key == "UP" ) then
		popup.list:Move(-1);
	elseif ( key == "DOWN" ) then
		popup.list:Move(1);
	elseif ( key == "LEFT" or key == "RIGHT" ) then
		ClosePopup();
		StepCursor(edit, key == "LEFT" and -1 or 1);
	elseif ( key == "PRINTSCREEN" ) then
		if ( Screenshot ) then
			Screenshot();
		end
	else
		return false;
	end

	return true;
end

-- Fallback for arrow keys that pass the catcher and reach the workspace hotkeys.
function SE.TokenPopupKey(key)
	if ( key == "UP" or key == "DOWN" or key == "LEFT" or key == "RIGHT" ) then
		return PopupKey(key);
	end

	return false;
end

local function CreatePopup()
	popup = CreateFrame("Frame", "GameEditorSpellTokenPopup", EditorParent, "NoggitBoxTemplate");
	popup:SetFrameStrata("TOOLTIP");
	popup:SetClampedToScreen(true);
	popup:EnableMouse(true);
	popup:EnableMouseWheel(true);
	NoggitBox_Set(popup, NOGGIT_COLOR_BASE, NOGGIT_COLOR_HOVER);
	popup:Hide();

	popup.list = CreateTokenList(popup, "GameEditorSpellTokenPopup", 8, 20, PickFromPopup);
	popup.list:Layout(1, -2, -1);
	popup:SetScript("OnMouseWheel", function(_, delta) popup.list:Scroll(-delta); end);

	popup.hint = SE.Label(popup, "Up and Down choose, Enter or a click inserts, Escape closes", "NoggitFont10", NOGGIT_COLOR_DISABLED);
	popup.hint:SetPoint("BOTTOMLEFT", popup, "BOTTOMLEFT", 7, 4);
	popup:SetHeight(8 * 20 + 22);

	-- Arrow keys the edit box passes on with alt arrow mode reach this frame, it sits one level below the edit box.
	popup.catcher = CreateFrame("Frame", "GameEditorSpellTokenKeys", EditorParent);
	popup.catcher:Hide();
	popup.catcher:EnableKeyboard(true);
	popup.catcher:SetScript("OnKeyDown", function(_, key)
		PopupKey(key);
	end);
end

local function OpenPopup(edit, start)
	if ( not popup ) then
		CreatePopup();
	end

	ClosePopup();
	popup.edit = edit;
	popup.start = start;
	popup.tokens = BuildTokens(CurrentValues());
	popup.altArrows = edit:GetAltArrowKeyMode() and true or false;
	edit:SetAltArrowKeyMode(true);

	-- Enter and Escape belong to the popup while it is open, the field gets its own scripts back on close.
	popup.enterScript = edit:GetScript("OnEnterPressed");
	popup.escapeScript = edit:GetScript("OnEscapePressed");
	edit:SetScript("OnEnterPressed", function() PickFromPopup(popup.list:Selected()); end);
	edit:SetScript("OnEscapePressed", function() ClosePopup(); end);

	local catcher = popup.catcher;
	catcher:SetParent(edit:GetParent());
	catcher:SetFrameStrata(edit:GetFrameStrata());
	catcher:SetFrameLevel(max(0, edit:GetFrameLevel() - 1));
	catcher:Show();

	local anchor = edit.seTokenAnchor or edit;
	popup:ClearAllPoints();
	popup:SetPoint("TOPLEFT", anchor, "BOTTOMLEFT", 0, -2);
	popup:SetWidth(min(560, max(420, anchor:GetWidth() or 420)));
	popup:Show();
end

-- Opens on a typed $, follows the typing, closes when the text before the cursor stops being a token.
local function UpdatePopup(edit, typed)
	local text = edit:GetText() or "";
	local position = edit:GetCursorPosition();
	local before = string.sub(text, 1, position);
	local prefix = string.match(before, "%$(<[%w_]*)$") or string.match(before, "%$(%w*)$");

	if ( PopupOwner() == edit ) then
		if ( not prefix or position - #prefix - 1 ~= popup.start ) then
			ClosePopup();
			return;
		end
	elseif ( typed and prefix == "" ) then
		OpenPopup(edit, position - 1);
	else
		return;
	end

	local items = FilterPrefix(popup.tokens, prefix, CurrentValues());

	if ( #items == 0 ) then
		ClosePopup();
		return;
	end

	popup.list:SetItems(items);
end

local lastTokenEdit;

-- Gives an edit box the $ popup, anchor is the frame the popup opens under.
function SE.AttachTokenHelper(edit, anchor)
	if ( edit.seTokenHelper ) then
		return;
	end

	edit.seTokenHelper = true;
	edit.seTokenAnchor = anchor;

	edit:HookScript("OnTextChanged", function(self, userInput)
		if ( userInput ) then
			UpdatePopup(self, true);
		end
	end);
	edit:HookScript("OnCursorChanged", function(self)
		if ( PopupOwner() == self ) then
			UpdatePopup(self, false);
		end
	end);
	edit:HookScript("OnEditFocusGained", function(self)
		lastTokenEdit = self;
	end);
	edit:HookScript("OnEditFocusLost", function(self)
		self.seLastCursor = self:GetCursorPosition();
		self.seLastText = self:GetText();

		if ( PopupOwner() == self ) then
			ClosePopup();
		end
	end);
end

local TOKEN_COLUMNS = { "^SpellDescription%d+$", "^SpellToolTip%d+$" };

-- Gives the description and tooltip rows of a property grid the $ popup.
function SE.AttachTokenHelperToGrid(grid)
	for column, row in pairs(grid.rowsByColumn or {}) do
		for _, pattern in ipairs(TOKEN_COLUMNS) do
			if ( string.find(column, pattern) and row.edit ) then
				SE.AttachTokenHelper(row.edit);
			end
		end
	end
end

-- Picker window, opened by the Variables buttons ---------------------------------------------------

local tokenWindow;

local function WindowFilter()
	tokenWindow.list:SetItems(FilterText(tokenWindow.tokens, tokenWindow.search:GetText(), CurrentValues()));
end

local function PickFromWindow(item)
	local edit = tokenWindow.target;
	local position = tokenWindow.position;
	GameEditor_HideWindow(tokenWindow);

	if ( not item or not edit or not edit:IsVisible() ) then
		return;
	end

	position = min(position or 0, #(edit:GetText() or ""));

	if ( item.pickSpell ) then
		PickOtherSpell(edit, position);
		return;
	end

	InsertAt(edit, position, item.token, item.back);
end

local function CreateTokenWindow()
	tokenWindow = GameEditor_CreateWindow("GameEditorSpellTokenWindow", "Description variables", 600, 520);
	local content = tokenWindow.content;

	tokenWindow.search = SE.SearchBox(content, 200, "Filter, for example s1, duration or a spell id", function()
		WindowFilter();
	end);
	tokenWindow.search:SetPoint("TOPLEFT", content, "TOPLEFT", 0, 0);
	tokenWindow.search:SetPoint("TOPRIGHT", content, "TOPRIGHT", 0, 0);
	tokenWindow.search:SetScript("OnEnterPressed", function()
		PickFromWindow(tokenWindow.list:Selected());
	end);

	local box = SE.Box(content, "GameEditorSpellTokenWindowList", NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID);
	box:SetPoint("TOPLEFT", content, "TOPLEFT", 0, -28);
	box:SetPoint("BOTTOMRIGHT", content, "BOTTOMRIGHT", 0, 22);
	box:EnableMouseWheel(true);
	tokenWindow.list = CreateTokenList(box, "GameEditorSpellTokenWindow", 19, 22, PickFromWindow);
	tokenWindow.list:Layout(1, -2, -1);
	box:SetScript("OnMouseWheel", function(_, delta) tokenWindow.list:Scroll(-delta); end);

	tokenWindow.note = SE.Label(content, "", "NoggitFont10", NOGGIT_COLOR_DISABLED);
	tokenWindow.note:SetPoint("BOTTOMLEFT", content, "BOTTOMLEFT", 2, 4);
	tokenWindow.note:SetPoint("RIGHT", content, "RIGHT", -2, 0);
end

-- Opens the picker for edit, the pick goes where its cursor was.
function SE.ShowTokenPicker(edit, title)
	if ( not edit ) then
		return;
	end

	if ( not tokenWindow ) then
		CreateTokenWindow();
	end

	ClosePopup();
	local text = edit:GetText() or "";
	local position;

	if ( edit:HasFocus() ) then
		position = edit:GetCursorPosition();
	elseif ( edit.seLastCursor and edit.seLastText == text ) then
		position = edit.seLastCursor;
	else
		position = #text;
	end

	tokenWindow.target = edit;
	tokenWindow.position = position;
	tokenWindow.tokens = BuildTokens(CurrentValues());
	tokenWindow:SetTitle(title and ("Variables for "..title) or "Description variables");
	tokenWindow.note:SetText(format("Click or Enter inserts at the cursor. Values are from spell %s, a red ? could not be resolved.", tostring(SE.CurrentId() or "-")));
	tokenWindow.search:SetQuietText("");
	tokenWindow.search.hint:Show();
	WindowFilter();
	GameEditor_ShowWindow(tokenWindow);
	tokenWindow.search:SetFocus();
end

function SE.LastTokenEdit()
	return lastTokenEdit;
end

function SE.HideTokenHelpers()
	ClosePopup();

	if ( tokenWindow and tokenWindow:IsShown() ) then
		GameEditor_HideWindow(tokenWindow);
	end
end

SE.On("SPELL_LOADED", function() SE.HideTokenHelpers(); end);

-- Texts section ---------------------------------------------------------------------------------------

-- All fields ----------------------------------------------------------------------------------------------

SE.AddSection({
	key = "all",
	title = "All Fields",
	order = 110,
	build = function(parent)
		local groups = {};

		for _, group in ipairs(Data.Groups and Data.Groups.Spell or {}) do
			table.insert(groups, { title = group.title, columns = group.columns, collapsed = group.collapsed });
		end

		local frame = CreateFrame("Frame", "GameEditorSpellAllFields", parent);
		local y = SE.SectionTitle(frame, "All Fields", "Every column of the Spell row in one place. Type in the filter to find a field by label or column name.", 0);
		local grid = SE.CreateSpellGrid(frame, groups, { filter = true });
		SE.AttachTokenHelperToGrid(grid);
		grid.frame:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, y - 2);
		grid.frame:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", 0, 0);
		frame:SetScript("OnShow", function() SE.SyncGrid(grid); end);
		return frame;
	end,
});
