-- Loot tabs of the creature and gameobject inspectors, the loot tree of the shown entry.

GameEditorLoot = GameEditorLoot or {};
local L = GameEditorLoot;

local Num = L.Num;

local GAMEOBJECT_TYPE_CHEST = 3;
local GAMEOBJECT_TYPE_FISHINGHOLE = 25;

local function CreatePage(parent, onDirty)
	local page = CreateFrame("Frame", NoggitUI_NewName("GameEditorLootPage"), parent);

	page.view = L.CreateView(page, { embedded = true, onDirty = function() if ( onDirty ) then onDirty(page); end end });
	page.view:SetAllPoints(page);

	page.unavailable = GameEditorObject.Paragraph(page, "");
	page.unavailable:SetPoint("TOPLEFT", page, "TOPLEFT", 4, -8);
	page.unavailable:SetPoint("TOPRIGHT", page, "TOPRIGHT", -4, -8);
	page.unavailable:Hide();

	page.HasChanges = function(self)
		return self.view:IsShown() and self.view:HasChanges();
	end;

	page.Save = function(self)
		return self.view:Save();
	end;

	page.Revert = function(self)
		self.view:Revert();
	end;

	page.SetAvailable = function(self, available, text)
		if ( available ) then
			self.unavailable:Hide();
			self.view:Show();
		else
			self.view:Hide();
			self.unavailable:SetText(text or "");
			self.unavailable:Show();
		end
	end;

	return page;
end

local function Root(index, key, missingText, onCreate)
	local info = L.TABLES[index];
	key = Num(key);

	return {
		table = info.table,
		key = key,
		label = info.title,
		missingText = key <= 0 and missingText or nil,
		onCreate = key <= 0 and onCreate or nil,
	};
end

function L.CreateCreaturePage(parent, onDirty)
	local page = CreatePage(parent, onDirty);

	page.Load = function(self, data)
		local template = data and data.template;

		if ( not data or not data.entry or data.entry == 0 or not template ) then
			self:SetAvailable(false, "Select a creature or open a template to edit its loot.");
			return;
		end

		self:SetAvailable(true);
		local entry = data.entry;

		local function Creator(field)
			return function()
				CreatureEditor_SetFields(0, { [field] = entry });
				GameEditor_Notify(format("creature_template.%s of %d set to %d, add rows once the template comes back", field, entry, entry), "info");
			end;
		end

		local roots = {};

		for _, index in ipairs(L.SOURCES.creature.tables) do
			local info = L.TABLES[index];
			table.insert(roots, Root(index, template[info.field],
				format("creature_template.%s is 0, this creature has no %s. Create loot id saves %s = %d.", info.field, string.lower(info.title), info.field, entry),
				Creator(info.field)));
		end

		self.view:SetRoots(roots, format("%s  |cff7f7f7fcreature %d|r", (data.name and data.name ~= "") and data.name or "Creature", entry));
	end;

	return page;
end

function L.CreateGameObjectPage(parent, onDirty)
	local page = CreatePage(parent, onDirty);

	page.Load = function(self, data)
		local template = data and data.template;

		if ( not data or not data.entry or data.entry == 0 or not template ) then
			self:SetAvailable(false, "Select a gameobject or open a template to edit its loot.");
			return;
		end

		local goType = Num(template.type or data.type);

		if ( goType ~= GAMEOBJECT_TYPE_CHEST and goType ~= GAMEOBJECT_TYPE_FISHINGHOLE ) then
			self:SetAvailable(false, "Only chests and fishing holes have gameobject loot, their Data1 is the gameobject_loot_template id. Change the type on the Template tab to give this object loot.");
			return;
		end

		self:SetAvailable(true);
		local entry = data.entry;

		local root = Root(3, template.Data1,
			format("gameobject_template.Data1 is 0, this object has no loot. Create loot id saves Data1 = %d.", entry),
			function()
				GobEditor_SetFields(0, { Data1 = entry });
				GameEditor_Notify(format("gameobject_template.Data1 of %d set to %d, add rows once the template comes back", entry, entry), "info");
			end);

		self.view:SetRoots({ root }, format("%s  |cff7f7f7fgameobject %d|r", (data.name and data.name ~= "") and data.name or "Gameobject", entry));
	end;

	return page;
end
