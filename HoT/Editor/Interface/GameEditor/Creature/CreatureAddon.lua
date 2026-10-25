-- Template addon and spawn addon pages of the creature inspector.

GameEditorCreature = GameEditorCreature or {};
local Creature = GameEditorCreature;
local Common = GameEditorObject;

local ADDON_FIELDS = (GameEditorData and GameEditorData.Fields and GameEditorData.Fields.creature_addon) or {};

local COLUMNS = { "path_id", "mount", "standState", "animTier", "sheathState", "emote", "visFlags", "pvpFlags", "visibilityDistanceType" };

local FIELDS = {
	path_id = ADDON_FIELDS.path_id,
	mount = ADDON_FIELDS.mount,
	standState = ADDON_FIELDS.StandState,
	animTier = ADDON_FIELDS.AnimTier,
	visFlags = ADDON_FIELDS.VisFlags,
	sheathState = ADDON_FIELDS.SheathState,
	pvpFlags = ADDON_FIELDS.PvPFlags,
	emote = ADDON_FIELDS.emote,
	visibilityDistanceType = ADDON_FIELDS.visibilityDistanceType,
};

local DEFAULTS = {
	path_id = 0, mount = 0, standState = 0, animTier = 0, visFlags = 0,
	sheathState = 1, pvpFlags = 0, emote = 0, visibilityDistanceType = 0,
};

local function SameAuras(a, b)
	if ( #a ~= #b ) then
		return false;
	end

	local set = {};

	for _, spell in ipairs(a) do
		set[spell] = true;
	end

	for _, spell in ipairs(b) do
		if ( not set[spell] ) then
			return false;
		end
	end

	return true;
end

local function CopyList(list)
	local copy = {};

	for index, value in ipairs(list or {}) do
		copy[index] = value;
	end

	return copy;
end

-- scope 0 is creature_template_addon, 1 is creature_addon.
function Creature.CreateAddonPage(parent, scope, onDirty)
	local page = CreateFrame("Frame", NoggitUI_NewName("GameEditorCreatureAddonPage"), parent);
	page.scope = scope;
	page.auras = {};
	page.storedAuras = {};
	page.available = true;

	local tableName = scope == 0 and "creature_template_addon" or "creature_addon";

	page.state = Common.Paragraph(page, "", NOGGIT_COLOR_TEXT);
	page.state:SetPoint("TOPLEFT", page, "TOPLEFT", 2, -2);
	page.state:SetPoint("TOPRIGHT", page, "TOPRIGHT", -2, -2);
	page.state:SetHeight(30);

	-- Bottom bar first so the rest can stack on it.
	local bar = CreateFrame("Frame", NoggitUI_NewName("GameEditorCreatureAddonBar"), page);
	bar:SetHeight(24);
	bar:SetPoint("BOTTOMLEFT", page, "BOTTOMLEFT", 0, 0);
	bar:SetPoint("BOTTOMRIGHT", page, "BOTTOMRIGHT", 0, 0);

	page.save = Common.Button(bar, "Save", 64, function() page:Save(); end, "Save the whole addon row to "..tableName.." and apply it to the live creatures it covers");
	page.save:SetPoint("RIGHT", bar, "RIGHT", 0, 0);
	page.revert = Common.Button(bar, "Revert", 60, function() page:Revert(); end, "Back to the stored row");
	page.revert:SetPoint("RIGHT", page.save, "LEFT", -4, 0);
	page.preview = Common.Button(bar, "Preview", 64, function() page:Send(0); end, "Apply these values to this creature only, nothing is saved. A respawn brings the stored row back.");
	page.preview:SetPoint("RIGHT", page.revert, "LEFT", -4, 0);
	page.delete = Common.Button(bar, "Delete row", 76, function() page:Delete(); end, "Delete the "..tableName.." row");
	page.delete:SetPoint("LEFT", bar, "LEFT", 0, 0);
	page.live = Common.Button(bar, "Live values", 76, function() page:LoadLive(); end, "Fill the form from what the creature shows right now");
	page.live:SetPoint("LEFT", page.delete, "RIGHT", 4, 0);

	-- Aura section above the bar.
	local auraBox = CreateFrame("Frame", NoggitUI_NewName("GameEditorCreatureAddonAuras"), page);
	auraBox:SetHeight(150);
	auraBox:SetPoint("BOTTOMLEFT", bar, "TOPLEFT", 0, 6);
	auraBox:SetPoint("BOTTOMRIGHT", bar, "TOPRIGHT", 0, 6);

	local auraTitle = Common.Label(auraBox, "Auras", "NoggitFontBold");
	auraTitle:SetPoint("TOPLEFT", auraBox, "TOPLEFT", 2, 0);
	page.auraCount = Common.Label(auraBox, "", nil, NOGGIT_COLOR_DISABLED);
	page.auraCount:SetPoint("LEFT", auraTitle, "RIGHT", 8, 0);

	local addRow = CreateFrame("Frame", NoggitUI_NewName("GameEditorCreatureAddonAuraAdd"), auraBox);
	addRow:SetHeight(22);
	addRow:SetPoint("BOTTOMLEFT", auraBox, "BOTTOMLEFT", 0, 0);
	addRow:SetPoint("BOTTOMRIGHT", auraBox, "BOTTOMRIGHT", 0, 0);

	page.auraEdit = NoggitLineEdit_Create(addRow, 90);
	page.auraEdit:SetHeight(22);
	page.auraEdit:SetPoint("LEFT", addRow, "LEFT", 0, 0);
	page.auraEdit:SetScript("OnEnterPressed", function(self)
		self:ClearFocus();
		page:AddAura(tonumber(self:GetText()));
	end);
	Common.Tooltip(page.auraEdit, "Spell id of an aura to add");

	local add = Common.Button(addRow, "Add", 50, function() page:AddAura(tonumber(page.auraEdit:GetText())); end, "Add the spell id to the aura list");
	add:SetPoint("LEFT", page.auraEdit, "RIGHT", 4, 0);

	local pick = Common.Button(addRow, "Find...", 60, function()
		GameEditor_PickRef("Aura spell", "spell", nil, function(value) page:AddAura(value); end);
	end, "Search a spell by name");
	pick:SetPoint("LEFT", add, "RIGHT", 4, 0);
	page.pick = pick;

	page.removeAura = Common.Button(addRow, "Remove", 66, function() page:RemoveAura(); end, "Remove the selected aura from the list");
	page.removeAura:SetPoint("RIGHT", addRow, "RIGHT", 0, 0);

	page.auraList = NoggitListBox_Create(auraBox, 20, function(row)
		row.id = NoggitUI_FontString(row, "");
		row.id:SetPoint("LEFT", row, "LEFT", 6, 0);
		row.id:SetWidth(60);
		NoggitText_SetColor(row.id, NOGGIT_COLOR_DISABLED);
		row.name = NoggitUI_FontString(row, "");
		row.name:SetPoint("LEFT", row, "LEFT", 70, 0);
		row.name:SetPoint("RIGHT", row, "RIGHT", -4, 0);
	end, function(row, item)
		row.id:SetText(tostring(item.value));
		row.name:SetText(Common.SpellText(item.value));
	end);
	page.auraList:SetPoint("TOPLEFT", auraTitle, "BOTTOMLEFT", -2, -4);
	page.auraList:SetPoint("BOTTOMRIGHT", addRow, "TOPRIGHT", 0, 4);
	page.auraList.onDoubleClick = function() page:RemoveAura(); end;

	page.grid = GameEditor_CreatePropertyGrid(page, {
		table = "creature_addon",
		columns = COLUMNS,
		fields = FIELDS,
		labelWidth = 150,
		onChange = function() page:UpdateState(); end,
	});
	page.grid.frame:SetPoint("TOPLEFT", page.state, "BOTTOMLEFT", -2, -4);
	page.grid.frame:SetPoint("BOTTOMRIGHT", auraBox, "TOPRIGHT", 0, 6);

	page.unavailable = Common.Paragraph(page, "");
	page.unavailable:SetPoint("TOPLEFT", page, "TOPLEFT", 4, -8);
	page.unavailable:SetPoint("TOPRIGHT", page, "TOPRIGHT", -4, -8);
	page.unavailable:Hide();

	page.content = { page.state, bar, auraBox, page.grid.frame };

	local function RefreshAuras()
		local items = {};

		for _, spell in ipairs(page.auras) do
			table.insert(items, { value = spell });
		end

		NoggitListBox_SetItems(page.auraList, items, true);
		page.auraCount:SetText(#items == 0 and "none, the list is saved as it is" or format("%d", #items));
	end

	page.AurasChanged = function(self)
		return not SameAuras(self.auras, self.storedAuras);
	end;

	page.HasChanges = function(self)
		return self.available and (self.grid:HasChanges() or self:AurasChanged());
	end;

	page.UpdateState = function(self)
		local dirty = self:HasChanges();
		Common.SetEnabled(self.save, self.available);
		Common.SetEnabled(self.revert, dirty);
		Common.SetEnabled(self.delete, self.available and self.exists);
		Common.SetEnabled(self.preview, self.available and not self.isTemplate);
		Common.SetEnabled(self.live, self.available and not self.isTemplate);
		Common.SetEnabled(self.pick, Common.CanSearchSpells());

		if ( onDirty ) then
			onDirty(self);
		end
	end;

	page.AddAura = function(self, spell)
		spell = tonumber(spell);

		if ( not spell or spell <= 0 ) then
			GameEditor_Notify("Enter a spell id", "info");
			return;
		end

		for _, existing in ipairs(self.auras) do
			if ( existing == spell ) then
				return;
			end
		end

		table.insert(self.auras, spell);
		self.auraEdit:SetText("");
		RefreshAuras();
		self:UpdateState();
	end;

	page.RemoveAura = function(self)
		local index = self.auraList.selected;

		if ( not index or not self.auras[index] ) then
			GameEditor_Notify("Select an aura in the list first", "info");
			return;
		end

		table.remove(self.auras, index);
		self.auraList.selected = nil;
		RefreshAuras();
		self:UpdateState();
	end;

	page.Values = function(self)
		local values = {};

		for _, column in ipairs(COLUMNS) do
			values[column] = tonumber(self.grid:GetValue(column)) or DEFAULTS[column];
		end

		values.auras = CopyList(self.auras);
		return values;
	end;

	page.Send = function(self, mode)
		if ( not self.available ) then
			return;
		end

		CreatureEditor_SetAddon(self.scope, mode, self:Values());
	end;

	page.Save = function(self)
		if ( not self:HasChanges() and self.exists ) then
			return false;
		end

		self:Send(1);
		return true;
	end;

	page.Delete = function(self)
		if ( not self.exists ) then
			return;
		end

		NoggitMessageBox_Show("Delete addon row", format("Delete the %s row of %s?\n\nThe creature falls back to %s.", tableName, self.keyText or "this creature", self.scope == 0 and "no addon" or "the template addon"), {
			{ text = "Delete", onClick = function() CreatureEditor_SetAddon(self.scope, 2, self:Values()); end },
			{ text = "Cancel" },
		});
	end;

	page.Revert = function(self)
		self.grid:Revert();
		self.auras = CopyList(self.storedAuras);
		RefreshAuras();
		self:UpdateState();
	end;

	page.LoadLive = function(self)
		local data = Creature.GetData();

		if ( not data or not self.available or data.isTemplate ) then
			return;
		end

		local live = data.live or {};

		for _, column in ipairs(COLUMNS) do
			local value = live[column];

			if ( column == "path_id" ) then
				value = data.assignedPath;
			end

			if ( value ~= nil ) then
				self.grid:SetValue(column, value, true);
			end
		end

		self.auras = {};

		for _, aura in ipairs(live.auras or {}) do
			if ( not aura.passive ) then
				table.insert(self.auras, aura.spell);
			end
		end

		RefreshAuras();
		self:UpdateState();
		GameEditor_Notify("Live values loaded, check them and press Save", "info");
	end;

	page.SetAvailable = function(self, available, text)
		self.available = available and true or false;

		for _, widget in ipairs(self.content) do
			if ( available ) then
				widget:Show();
			else
				widget:Hide();
			end
		end

		if ( available ) then
			self.unavailable:Hide();
		else
			self.unavailable:SetText(text or "");
			self.unavailable:Show();
		end
	end;

	page.Load = function(self, data, keepEdits)
		local stored = data and (self.scope == 0 and data.templateAddon or data.spawnAddon);
		self.isTemplate = data and data.isTemplate or false;

		if ( self.scope == 1 and self.isTemplate ) then
			self:SetAvailable(false, "A template has no spawn addon. Close the template and select a creature in the world to edit its creature_addon row.");
			self.exists = false;
			self:UpdateState();
			return;
		end

		if ( self.scope == 1 and (not data or (data.spawnId or 0) == 0) ) then
			self:SetAvailable(false, "This creature is not a database spawn, so it can not have a spawn addon. Use the template Addon tab instead.");
			self.exists = false;
			self:UpdateState();
			return;
		end

		self:SetAvailable(true);
		self.exists = stored ~= nil;
		self.keyText = self.scope == 0 and format("entry %d", data.entry or 0) or format("spawn %d", data.spawnId or 0);

		local values = {};

		for column, default in pairs(DEFAULTS) do
			values[column] = stored and stored[column] or default;
		end

		local storedAuras = CopyList(stored and stored.auras);
		local keepAuras = keepEdits and self:AurasChanged() and SameAuras(storedAuras, self.storedAuras);
		Common.LoadGrid(self.grid, values, keepEdits);
		self.storedAuras = storedAuras;

		if ( not keepAuras ) then
			self.auras = CopyList(storedAuras);
		end

		local text;

		if ( self.exists ) then
			text = format("%s row of %s.", tableName, self.keyText);
		else
			text = format("No %s row for %s yet, Save creates it.", tableName, self.keyText);
		end

		if ( self.scope == 0 and data.spawnAddon ) then
			text = text.." |cffffcc00This spawn has its own spawn addon, it wins over this one.|r";
		elseif ( self.scope == 1 and not self.exists and data.templateAddon ) then
			text = text.." The creature uses the template addon now.";
		end

		self.state:SetText(text);
		RefreshAuras();
		self:UpdateState();
	end;

	RefreshAuras();
	page:UpdateState();
	return page;
end
