-- Legendary preview: the item text a player reads and the client tooltip of every spell in the set.

GameEditorItem = GameEditorItem or {};
local I = GameEditorItem;
local Common = GameEditorObject;

local WIDTH = 340;
local GAP = 8;

local TOOLTIP_BACKDROP = {
	bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
	edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
	tile = true, tileSize = 16, edgeSize = 16,
	insets = { left = 4, right = 4, top = 4, bottom = 4 },
};

local ORANGE = { 1, 0.5, 0 };
local GREEN = { 0, 1, 0 };
local GOLD = { 1, 0.82, 0 };
local WHITE = { 1, 1, 1 };
local GREY = { 0.5, 0.5, 0.5 };
local RED = { 1, 0.3, 0.3 };

local FLAG_TEXT = {
	{ bit = 0, text = "Crafted items only" },
	{ bit = 1, text = "Essence only, never drops" },
	{ bit = 2, text = "Corrupted, rolls nothing else" },
};

local function Num(values, column, default)
	return tonumber(values and values[column]) or default or 0;
end

local function Text(values, column)
	local value = values and values[column];
	return (value ~= nil and tostring(value) ~= "") and tostring(value) or nil;
end

local function TriggerPrefix(trigger)
	if ( trigger == 1 or trigger == 3 ) then
		return ITEM_SPELL_TRIGGER_ONEQUIP or "Equip:";
	elseif ( trigger == 2 ) then
		return ITEM_SPELL_TRIGGER_ONPROC or "Chance on hit:";
	end

	return ITEM_SPELL_TRIGGER_ONUSE or "Use:";
end

local function HasBit(value, bit)
	return floor(value / 2 ^ bit) % 2 == 1;
end

local function RollsOnText(values)
	local class = Num(values, "itemClass", -1);
	local subclass = Num(values, "itemSubClass", -1);
	local slot = Num(values, "itemInventoryType", -1);
	local parts = {};

	if ( class >= 0 and subclass >= 0 ) then
		table.insert(parts, I.EnumText(I.SubClassEnum(class), subclass) or format("subclass %d", subclass));
	elseif ( class ~= -1 ) then
		table.insert(parts, I.EnumText("ItemGenAnyClass", class) or format("class %d", class));
	end

	if ( slot >= 0 ) then
		table.insert(parts, I.EnumText("ItemGenAnyInventoryType", slot) or format("slot %d", slot));
	end

	local text = "Rolls on "..(#parts > 0 and table.concat(parts, ", ") or "any item");
	local low, high = Num(values, "minItemLevel"), Num(values, "maxItemLevel");

	if ( low > 0 and high > 0 ) then
		text = text..format(", item level %d to %d", low, high);
	elseif ( low > 0 ) then
		text = text..format(", item level %d and up", low);
	elseif ( high > 0 ) then
		text = text..format(", up to item level %d", high);
	end

	return text;
end

local function EffectLines(values)
	local lines = {};
	local primary, secondary = Num(values, "primaryStatModifier", 1), Num(values, "secondaryStatModifier", 1);

	if ( primary ~= 1 ) then
		table.insert(lines, format("Primary stats x%s", Common.Float(primary, 2)));
	end

	if ( secondary ~= 1 ) then
		table.insert(lines, format("Secondary stats x%s", Common.Float(secondary, 2)));
	end

	local primaryCount, secondaryCount = Num(values, "primaryStatCountMod"), Num(values, "secondaryStatCountMod");

	if ( primaryCount ~= 0 ) then
		table.insert(lines, format("%+d primary stat%s", primaryCount, abs(primaryCount) == 1 and "" or "s"));
	end

	if ( secondaryCount ~= 0 ) then
		table.insert(lines, format("%+d secondary stat%s", secondaryCount, abs(secondaryCount) == 1 and "" or "s"));
	end

	local sockets = Num(values, "socketMod");

	if ( sockets > 0 ) then
		table.insert(lines, format("+%d socket%s", sockets, sockets == 1 and "" or "s"));
	end

	if ( Num(values, "prismaticSocket") ~= 0 ) then
		table.insert(lines, "Prismatic socket");
	end

	return lines;
end

-- Item card -------------------------------------------------------------------------------------------

local function CreateCard(parent)
	local card = CreateFrame("Frame", NoggitUI_NewName("GameEditorLegendaryCard"), parent);
	card:SetBackdrop(TOOLTIP_BACKDROP);
	card:SetBackdropColor(0.03, 0.03, 0.08, 0.94);
	card:SetBackdropBorderColor(1, 0.5, 0, 1);
	card.lines = {};
	card.used = 0;

	card.Begin = function(self)
		self.used = 0;
		self.y = -10;
	end;

	card.Add = function(self, text, color, font, gap)
		self.used = self.used + 1;
		local line = self.lines[self.used];

		if ( not line ) then
			line = self:CreateFontString(nil, "ARTWORK", "GameTooltipText");
			line:SetJustifyH("LEFT");
			self.lines[self.used] = line;
		end

		line:SetFontObject(font or GameTooltipText);
		line:ClearAllPoints();
		self.y = self.y - (gap or 0);
		line:SetPoint("TOPLEFT", self, "TOPLEFT", 10, self.y);
		line:SetWidth(self:GetWidth() - 20);
		line:SetText(text);
		line:SetTextColor(color[1], color[2], color[3]);
		line:Show();
		self.y = self.y - max(12, line:GetHeight()) - 2;
	end;

	card.Finish = function(self)
		for index = self.used + 1, #self.lines do
			self.lines[index]:Hide();
		end

		self:SetHeight(-self.y + 8);
	end;

	return card;
end

-- One block per spell of the set: a title, the Go button and the client's own spell tooltip.
local function CreateSpellBlock(parent)
	local block = CreateFrame("Frame", NoggitUI_NewName("GameEditorLegendarySpell"), parent);
	block:SetHeight(40);

	block.title = Common.Label(block, "", "NoggitFontBold", NOGGIT_COLOR_TEXT);
	block.title:SetPoint("TOPLEFT", block, "TOPLEFT", 2, -4);
	block.title:SetPoint("RIGHT", block, "RIGHT", -52, 0);

	block.open = Common.Button(block, "Open", 46, function()
		local opener = GameEditor_RefOpeners and GameEditor_RefOpeners["spell"];

		if ( not block.spellId ) then
			GameEditor_Notify("This block has no spell to open", "error");
		elseif ( not opener ) then
			GameEditor_Notify("The spell editor is not loaded", "error");
		else
			opener(block.spellId);
		end
	end, "Open this spell in the spell editor");
	block.open:SetPoint("TOPRIGHT", block, "TOPRIGHT", 0, 0);

	block.info = Common.Label(block, "", nil, NOGGIT_COLOR_DISABLED);
	block.info:SetPoint("TOPLEFT", block, "TOPLEFT", 2, -26);
	block.info:SetPoint("RIGHT", block, "RIGHT", -2, 0);
	block.info:SetJustifyH("LEFT");

	block.tip = CreateFrame("GameTooltip", NoggitUI_NewName("GameEditorLegendarySpellTip"), block, "GameTooltipTemplate");

	block.missing = Common.Paragraph(block, "", RED);
	block.missing:SetPoint("RIGHT", block, "RIGHT", -2, 0);

	return block;
end

local function SpellInfoText(values)
	local parts = { I.EnumText("ItemSpellTrigger", Num(values, "SpellTrigger")) or format("Trigger %d", Num(values, "SpellTrigger")) };
	local charges = Num(values, "SpellCharges");
	local ppm = Num(values, "SpellPPMRate");
	local cooldown = Num(values, "SpellCooldown", -1);
	local categoryCooldown = Num(values, "SpellCategoryCooldown", -1);

	if ( charges ~= 0 ) then
		table.insert(parts, format("%d charge%s", abs(charges), abs(charges) == 1 and "" or "s"));
	end

	if ( ppm > 0 ) then
		table.insert(parts, format("%s procs a minute", Common.Float(ppm, 2)));
	end

	if ( cooldown >= 0 ) then
		table.insert(parts, format("cooldown %s s", Common.Float(cooldown / 1000, 1)));
	end

	if ( categoryCooldown >= 0 and Num(values, "SpellCategory") > 0 ) then
		table.insert(parts, format("category %d, %s s", Num(values, "SpellCategory"), Common.Float(categoryCooldown / 1000, 1)));
	end

	return table.concat(parts, ", ");
end

local function FillBlock(block, values, width)
	local id = Num(values, "SpellId");
	local name = id > 0 and GetSpellInfo(id);
	block.spellId = id > 0 and id or nil;
	block:SetWidth(width);
	block.title:SetText(format("Slot %d   %d  %s", Num(values, "spellIndex"), id, name or ""));
	block.info:SetText(SpellInfoText(values));
	Common.SetEnabled(block.open, block.spellId ~= nil);

	local tip = block.tip;
	local y = 44;

	if ( name ) then
		block.missing:Hide();
		tip:SetOwner(block, "ANCHOR_NONE");
		tip:SetFrameStrata(block:GetFrameStrata());
		tip:SetFrameLevel(block:GetFrameLevel() + 2);
		tip:ClearAllPoints();
		tip:SetPoint("TOPLEFT", block, "TOPLEFT", 0, -y);
		pcall(tip.SetHyperlink, tip, "spell:"..id);
		tip:Show();
		y = y + max(20, tip:GetHeight());
	else
		tip:Hide();
		block.missing:ClearAllPoints();
		block.missing:SetPoint("TOPLEFT", block, "TOPLEFT", 2, -y);
		block.missing:SetPoint("RIGHT", block, "RIGHT", -2, 0);
		block.missing:SetText(id > 0 and format("Spell %d is not in the client Spell.dbc", id) or "No spell set on this slot");
		block.missing:Show();
		y = y + 18;
	end

	block:SetHeight(y + 4);
	block:Show();
	return y + 4;
end

-- Preview pane ------------------------------------------------------------------------------------------

-- source() returns the legendary row values, the spell set rows and the key the set editor has loaded.
function I.CreateLegendaryPreview(parent, source)
	local pane = CreateFrame("Frame", NoggitUI_NewName("GameEditorLegendaryPreview"), parent, "NoggitBoxTemplate");
	NoggitBox_Set(pane, NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID);
	pane:SetWidth(WIDTH);
	pane.width = WIDTH;

	local title = Common.Label(pane, "Preview", "NoggitFont16", NOGGIT_COLOR_WHITE);
	title:SetPoint("TOPLEFT", pane, "TOPLEFT", 8, -6);

	local hint = Common.Label(pane, "Unsaved edits show here", nil, NOGGIT_COLOR_DISABLED);
	hint:SetPoint("LEFT", title, "RIGHT", 8, 0);

	local scroll, child = GameEditor_CreateScrollFrame(pane);
	scroll:SetPoint("TOPLEFT", pane, "TOPLEFT", 6, -30);
	scroll:SetPoint("BOTTOMRIGHT", pane, "BOTTOMRIGHT", -4, 6);

	local empty = Common.Paragraph(child, "Pick a legendary on the left to see its item text and spell tooltips.");
	empty:SetPoint("TOPLEFT", child, "TOPLEFT", 2, -2);
	empty:SetPoint("RIGHT", child, "RIGHT", -2, 0);

	local card = CreateCard(child);
	card:SetPoint("TOPLEFT", child, "TOPLEFT", 0, 0);

	local note = Common.Paragraph(child, "", GOLD);
	local blocks = {};

	pane.Rebuild = function(self)
		self.dirty = false;
		local width = max(200, (scroll:GetWidth() or WIDTH) - 14);
		local values, spells, loadedSet, loading = source();

		for _, block in ipairs(blocks) do
			block:Hide();
		end

		note:Hide();

		if ( not values ) then
			card:Hide();
			empty:Show();
			child:SetHeight(40);
			scroll.UpdateRange();
			return;
		end

		empty:Hide();
		card:SetWidth(width);
		card:Begin();

		local name = Text(values, "nameOverride");
		card:Add(name or format("Legendary %d", Num(values, "Id")), ORANGE, GameTooltipHeaderText);

		if ( not name ) then
			card:Add("No name override, the item keeps its rolled name", GREY);
		end

		local flags = Num(values, "legendaryFlags");

		for _, flag in ipairs(FLAG_TEXT) do
			if ( HasBit(flags, flag.bit) ) then
				card:Add(flag.text, GREY);
			end
		end

		card:Add(RollsOnText(values), GREY);

		for _, line in ipairs(EffectLines(values)) do
			card:Add(line, WHITE);
		end

		local setId = Num(values, "legendarySpellTemplateId");

		if ( loading ) then
			card:Add("Loading the spell set...", GREY, nil, 4);
		elseif ( #spells == 0 and loadedSet == setId ) then
			card:Add(format("Spell set %d has no spells", setId), GREY, nil, 4);
		end

		for _, row in ipairs(spells) do
			local id = Num(row, "SpellId");
			local description = id > 0 and GetSpellDescription and GetSpellDescription(id);

			if ( not description or description == "" ) then
				description = id > 0 and (GetSpellInfo(id) or format("spell %d is not in the client", id)) or "no spell";
			end

			card:Add(TriggerPrefix(Num(row, "SpellTrigger")).." "..description, GREEN, nil, 4);
		end

		local lore = Text(values, "loreOverride");

		if ( lore ) then
			card:Add("\""..lore.."\"", GOLD, nil, 4);
		end

		card:Finish();
		card:Show();

		local y = card:GetHeight() + GAP;

		if ( not loading and loadedSet ~= setId ) then
			note:ClearAllPoints();
			note:SetPoint("TOPLEFT", child, "TOPLEFT", 2, -y);
			note:SetPoint("RIGHT", child, "RIGHT", -2, 0);

			if ( loadedSet ) then
				note:SetText(format("These are the spells of spell set %d. Save the legendary to load spell set %d.", loadedSet, setId));
			else
				note:SetText(format("Save the legendary to load spell set %d.", setId));
			end

			note:Show();
			y = y + note:GetHeight() + GAP;
		end

		for index, row in ipairs(spells) do
			local block = blocks[index];

			if ( not block ) then
				block = CreateSpellBlock(child);
				blocks[index] = block;
			end

			block:ClearAllPoints();
			block:SetPoint("TOPLEFT", child, "TOPLEFT", 0, -y);
			y = y + FillBlock(block, row, width) + GAP;
		end

		child:SetHeight(y + 4);
		scroll.UpdateRange();
	end;

	pane.Refresh = function(self)
		self.dirty = true;
	end;

	pane:RegisterEvent("DBC_RECORD_UPDATED");
	pane:SetScript("OnEvent", function(self) self.dirty = true; end);
	pane:SetScript("OnShow", function(self) self.dirty = true; end);
	pane:SetScript("OnSizeChanged", function(self) self.dirty = true; end);
	pane:SetScript("OnUpdate", function(self)
		if ( self.dirty ) then
			self:Rebuild();
		end
	end);

	pane.dirty = true;
	return pane;
end
