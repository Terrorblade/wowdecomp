-- Enchants workspace data, SpellItemEnchantment and SpellItemEnchantmentCondition rows plus the enchant proc row of the server.

GameEditorEnchant = GameEditorEnchant or {};
local EN = GameEditorEnchant;
local Data = GameEditorData or {};

EN.TABLE = "SpellItemEnchantment";
EN.CONDITION = "SpellItemEnchantmentCondition";
EN.EFFECTS = 3;
EN.TERMS = 5;

EN.store = GameEditor_CreateDbcDocs("Enchant editor", { [EN.TABLE] = true, [EN.CONDITION] = true });

-- Spell effects whose misc value is an enchant id.
EN.SPELL_EFFECTS = { [53] = "Enchant item", [54] = "Enchant item temporary", [92] = "Enchant held item", [156] = "Enchant item prismatic" };

-- Player::ApplyEnchantment, the points are EffectPointsMin, the server never reads EffectPointsMax.
EN.EFFECT_TYPES = {
	[0] = { name = "None" },
	[1] = { name = "Combat spell", arg = "Spell", argRef = "spell", argTip = "Cast on the target when a melee hit procs it", points = "Chance", pointsTip = "Proc chance in percent, 0 uses the weapon speed chance. spell_enchant_proc_data overrides both" },
	[2] = { name = "Damage", points = "Damage", pointsTip = "Weapon damage added" },
	[3] = { name = "Equip spell", arg = "Spell", argRef = "spell", argTip = "Aura applied while the item is equipped" },
	[4] = { name = "Resistance", arg = "School", argEnum = "resistance", argTip = "Resistance that goes up, 0 is armor", points = "Amount", pointsTip = "0 takes the amount from the random suffix of the item" },
	[5] = { name = "Stat", arg = "Stat", argEnum = "stat", argTip = "ItemModType of the stat", points = "Amount", pointsTip = "0 takes the amount from the random suffix of the item" },
	[6] = { name = "Totem", points = "Damage", pointsTip = "Rockbiter style damage, scaled by weapon speed" },
	[7] = { name = "Use spell", arg = "Spell", argRef = "spell", argTip = "Spell the item casts when it is used" },
	[8] = { name = "Prismatic socket", pointsTip = "Adds a prismatic socket to the item" },
};

EN.FLAGS = {
	{ bit = 0, name = "Soulbound", tooltip = "0x1, the item binds when this enchant is on it" },
	{ bit = 1, name = "Flag 0x2", tooltip = "0x2" },
	{ bit = 2, name = "Flag 0x4", tooltip = "0x4" },
	{ bit = 3, name = "Flag 0x8", tooltip = "0x8" },
};

-- SpellItemEnchantmentCondition, the client tooltip reads every operator, the server only checks 2, 3 and 5.
EN.GEM_COLORS = { [0] = "None", "Meta", "Red", "Yellow", "Blue" };
EN.OPERATORS = { [0] = "equal to", "not equal to", "less than", "more than", "at most", "at least" };

local Num = function(value) return tonumber(value) or 0; end;
EN.Num = Num;

function EN.ResistanceItems()
	local schools = Data.Enums and Data.Enums.SpellSchool or {};
	local items = { { value = 0, text = "0  Armor" } };

	for value = 1, 6 do
		table.insert(items, { value = value, text = format("%d  %s", value, schools[value] or "School "..value) });
	end

	return items;
end

function EN.StatItems()
	local stats = Data.Enums and Data.Enums.ItemModType or {};
	local keys = {};

	for value in pairs(stats) do
		table.insert(keys, value);
	end

	table.sort(keys);
	local items = {};

	for _, value in ipairs(keys) do
		table.insert(items, { value = value, text = format("%d  %s", value, tostring(stats[value])) });
	end

	return items;
end

function EN.EnumItems(names)
	local items = {};

	for value = 0, #names do
		table.insert(items, { value = value, text = format("%d  %s", value, names[value]) });
	end

	return items;
end

function EN.Column(name, slot)
	return format("m_%s[%d]", name, slot);
end

function EN.NameColumn()
	EN.nameColumn = EN.nameColumn or GameEditor_DbcNameColumn(EN.TABLE) or "m_name[0]";
	return EN.nameColumn;
end

function EN.Name(id)
	local row = GameEditor_DbcRow(EN.TABLE, id);
	return row and row[EN.NameColumn()] or nil;
end

-- One line per effect, used in lists and the status bar.
function EN.EffectText(values, slot)
	local kind = Num(values[EN.Column("effect", slot)]);
	local info = EN.EFFECT_TYPES[kind];

	if ( kind == 0 ) then
		return nil;
	end

	local arg = Num(values[EN.Column("effectArg", slot)]);
	local points = Num(values[EN.Column("effectPointsMin", slot)]);
	local text = info and info.name or ("Type "..kind);

	if ( info and info.argRef ) then
		text = format("%s %d %s", text, arg, GameEditor_ResolveRef(info.argRef, arg) or "");
	elseif ( info and info.argEnum ) then
		text = format("%s %d", text, arg);
	end

	if ( info and info.points ) then
		text = format("%s, %s %d", text, string.lower(info.points), points);
	end

	return text;
end

-- The condition as the item tooltip says it, one term per line.
function EN.ConditionText(values)
	local lines = {};

	for slot = 0, EN.TERMS - 1 do
		local left = Num(values[EN.Column("ltOperandType", slot)]);

		if ( left ~= 0 ) then
			local op = EN.OPERATORS[Num(values[EN.Column("operator", slot)])] or "?";
			local rightType = Num(values[EN.Column("rtOperandType", slot)]);
			local right = rightType ~= 0 and (EN.GEM_COLORS[rightType] or "?").." gems" or tostring(Num(values[EN.Column("rtOperand", slot)]));
			table.insert(lines, format("%s gems %s %s", EN.GEM_COLORS[left] or "?", op, right));
		end

		if ( Num(values[EN.Column("logic", slot)]) == 0 ) then
			break;
		end
	end

	return #lines > 0 and table.concat(lines, ", and ") or "No terms";
end

-- Spells with an enchant effect whose misc value is this enchant.
function EN.SpellsUsing(id)
	local found = {};
	local ok, rows = pcall(DbcEditor_Filter, "Spell", { text = "", where = { { columns = { "m_effectMiscValue[0]", "m_effectMiscValue[1]", "m_effectMiscValue[2]" }, value = id } } }, 500);

	for _, entry in ipairs(ok and type(rows) == "table" and rows or {}) do
		local row = GameEditor_DbcRow("Spell", entry.id);

		for slot = 0, 2 do
			local effect = row and Num(row["m_effect["..slot.."]"]);

			if ( effect and EN.SPELL_EFFECTS[effect] and Num(row["m_effectMiscValue["..slot.."]"]) == id ) then
				table.insert(found, { id = entry.id, name = entry.name, effect = effect });
				break;
			end
		end
	end

	return found;
end

-- GemProperties rows that put this enchant in a socket.
function EN.GemsUsing(id)
	local ok, rows = pcall(DbcEditor_Filter, "GemProperties", { text = "", where = { { columns = { "m_enchantID" }, value = id } } }, 100);
	return ok and type(rows) == "table" and rows or {};
end
