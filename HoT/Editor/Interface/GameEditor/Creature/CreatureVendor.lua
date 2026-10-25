-- Vendor page of the creature inspector, the npc_vendor rows of the entry.

GameEditorCreature = GameEditorCreature or {};
local Creature = GameEditorCreature;
local Common = GameEditorObject;

local TABLE = "npc_vendor";
local ROW_HEIGHT = 24;
local NPC_FLAG_VENDOR = 128;
local MAX_ITEMS = 150;

local itemNames = {};

local QUALITY_COLORS = {
	[0] = "ff9d9d9d", [1] = "ffffffff", [2] = "ff1eff00", [3] = "ff0070dd", [4] = "ffa335ee", [5] = "ffff8000", [6] = "ffe6cc80", [7] = "ffe6cc80",
};

-- Item names --------------------------------------------------------------------------------------------------

local cacheTooltip = CreateFrame("GameTooltip", "GameEditorVendorCacheTooltip", nil, "GameTooltipTemplate");
local cacheAsked = {};

local function ItemName(id)
	id = tonumber(id) or 0;

	if ( id == 0 ) then
		return nil;
	end

	local name, _, quality = GetItemInfo(id);

	if ( name ) then
		local color = QUALITY_COLORS[quality or 1] or QUALITY_COLORS[1];
		return format("|c%s%s|r", color, name), true;
	end

	local row = GameEditor_TemplateRow("item", id);

	if ( row and not itemNames[id] ) then
		itemNames[id] = format("|c%s%s|r", QUALITY_COLORS[row.type or 1] or QUALITY_COLORS[1], row.name or "");
	end

	if ( itemNames[id] ) then
		return itemNames[id], true;
	end

	-- Setting the hyperlink makes the client ask the server for the item.
	if ( not cacheAsked[id] ) then
		cacheAsked[id] = true;
		cacheTooltip:SetOwner(UIParent, "ANCHOR_NONE");
		pcall(cacheTooltip.SetHyperlink, cacheTooltip, "item:"..id);
		cacheTooltip:Hide();
	end

	return nil, false;
end

local function ItemText(id)
	id = tonumber(id) or 0;

	if ( id < 0 ) then
		return format("|cff7fbfffEvery item of vendor %d|r", -id);
	end

	local name = ItemName(id);
	return name or (id > 0 and format("|cff7f7f7fitem %d|r", id) or "|cff7f7f7fno item|r");
end

-- Page ----------------------------------------------------------------------------------------------------------

local function Num(value)
	return tonumber(value) or 0;
end

local function NumberEdit(parent, width, tooltip, letters)
	local edit = NoggitLineEdit_Create(parent, width);
	edit:SetHeight(20);
	edit:SetMaxLetters(letters or 9);
	Common.Tooltip(edit, tooltip);
	return edit;
end

local function SameRows(a, b)
	if ( #a ~= #b ) then
		return false;
	end

	for index, row in ipairs(a) do
		local other = b[index];

		if ( row.item ~= other.item or row.maxcount ~= other.maxcount or row.incrtime ~= other.incrtime or row.ExtendedCost ~= other.ExtendedCost ) then
			return false;
		end
	end

	return true;
end

local function CopyRows(rows)
	local copy = {};

	for index, row in ipairs(rows) do
		copy[index] = Common.Copy(row);
	end

	return copy;
end

function Creature.CreateVendorPage(parent, onDirty)
	local page = CreateFrame("Frame", NoggitUI_NewName("GameEditorCreatureVendor"), parent);
	page.rows = {};
	page.items = {};
	page.original = {};
	page.available = true;

	local intro = Common.Paragraph(page, "Items this entry sells (npc_vendor), in the order the vendor window shows them. Max count above 0 limits the stock, it refills after the restock time.");
	intro:SetPoint("TOPLEFT", page, "TOPLEFT", 2, -2);
	intro:SetPoint("TOPRIGHT", page, "TOPRIGHT", -2, -2);
	intro:SetHeight(30);

	page.flagWarning = CreateFrame("Frame", NoggitUI_NewName("GameEditorCreatureVendorFlag"), page);
	page.flagWarning:SetHeight(22);
	page.flagWarning:SetPoint("TOPLEFT", intro, "BOTTOMLEFT", 0, -2);
	page.flagWarning:SetPoint("TOPRIGHT", intro, "BOTTOMRIGHT", 0, -2);

	local flagButton = Common.Button(page.flagWarning, "Add vendor flag", 110, function()
		local data = Creature.GetData();
		local flags = data and data.template and Num(data.template.npcflag) or 0;

		if ( not GameEditor_HasBit(flags, 7) ) then
			CreatureEditor_SetFields(0, { npcflag = flags + NPC_FLAG_VENDOR });
		end
	end, "Save creature_template.npcflag with the vendor bit so players can open the vendor window");
	flagButton:SetPoint("RIGHT", page.flagWarning, "RIGHT", 0, 0);

	local flagText = Common.Label(page.flagWarning, "This entry has no vendor NPC flag, players can not buy from it.", nil, NOGGIT_COLOR_GOLD);
	flagText:SetPoint("LEFT", page.flagWarning, "LEFT", 2, 0);
	flagText:SetPoint("RIGHT", flagButton, "LEFT", -4, 0);

	local header = CreateFrame("Frame", NoggitUI_NewName("GameEditorCreatureVendorColumns"), page);
	header:SetHeight(16);
	header:SetPoint("TOPLEFT", page.flagWarning, "BOTTOMLEFT", 0, -2);
	header:SetPoint("TOPRIGHT", page.flagWarning, "BOTTOMRIGHT", 0, -2);

	local function HeaderLabel(text, x, anchor, tooltip)
		local label = Common.Label(header, text, "NoggitFontBold");
		label:SetPoint(anchor, header, anchor, x, 0);

		if ( tooltip ) then
			local hit = CreateFrame("Frame", nil, header);
			hit:SetAllPoints(label);
			hit:EnableMouse(true);
			Common.Tooltip(hit, tooltip);
		end
	end

	HeaderLabel("Item", 2, "LEFT");
	HeaderLabel("Max", -192, "RIGHT", "Max count, 0 sells without limit");
	HeaderLabel("Restock", -134, "RIGHT", "Seconds until a limited item refills");
	HeaderLabel("Cost", -86, "RIGHT", "ItemExtendedCost id for honor, arena points or tokens, 0 sells for gold");

	local bar = CreateFrame("Frame", NoggitUI_NewName("GameEditorCreatureVendorBar"), page);
	bar:SetHeight(24);
	bar:SetPoint("BOTTOMLEFT", page, "BOTTOMLEFT", 0, 0);
	bar:SetPoint("BOTTOMRIGHT", page, "BOTTOMRIGHT", 0, 0);

	page.save = Common.Button(bar, "Save", 80, function() page:Save(); end, "Replace the npc_vendor rows of this entry and reload the vendors (Ctrl+S saves every tab)");
	page.save:SetPoint("RIGHT", bar, "RIGHT", 0, 0);
	page.revert = Common.Button(bar, "Revert", 70, function() page:Revert(); end, "Throw away the unsaved vendor changes");
	page.revert:SetPoint("RIGHT", page.save, "LEFT", -4, 0);
	page.status = Common.Label(bar, "", nil, NOGGIT_COLOR_DISABLED);
	page.status:SetPoint("LEFT", bar, "LEFT", 2, 0);
	page.status:SetPoint("RIGHT", page.revert, "LEFT", -4, 0);

	local addBar = CreateFrame("Frame", NoggitUI_NewName("GameEditorCreatureVendorAdd"), page);
	addBar:SetHeight(24);
	addBar:SetPoint("BOTTOMLEFT", bar, "TOPLEFT", 0, 4);
	addBar:SetPoint("BOTTOMRIGHT", bar, "TOPRIGHT", 0, 4);

	local addId = NumberEdit(addBar, 70, "Item id, a negative number adds every item of that vendor entry", 10);
	addId:SetPoint("LEFT", addBar, "LEFT", 0, 0);

	local function AddTyped()
		local id = tonumber(addId:GetText());

		if ( not id or id == 0 ) then
			GameEditor_Notify("Type an item id first", "info");
			return;
		end

		page:AddItem(id);
		addId:SetText("");
		addId:ClearFocus();
	end

	local addById = Common.Button(addBar, "Add id", 56, AddTyped, "Add the item id typed on the left");
	addById:SetPoint("LEFT", addId, "RIGHT", 4, 0);
	addId:SetScript("OnEnterPressed", AddTyped);

	local addByName = Common.Button(addBar, "Find item...", 90, function()
		GameEditor_PickRef("Add item to the vendor", "item", nil, function(value)
			page:AddItem(value);
		end, { anyNumber = true });
	end, "Pick from every item, type to filter by name or entry");
	addByName:SetPoint("LEFT", addById, "RIGHT", 4, 0);

	local clear = Common.Button(addBar, "Remove all", 80, function()
		if ( #page.items == 0 ) then
			return;
		end

		NoggitMessageBox_Show("Remove all items", "Remove every item from this vendor? Nothing is saved until you press Save.", {
			{ text = "Remove", onClick = function() page.items = {}; page:Refresh(); end },
			{ text = "Cancel" },
		});
	end, "Empty the list, Save makes it permanent");
	clear:SetPoint("RIGHT", addBar, "RIGHT", 0, 0);

	local scroll, child = GameEditor_CreateScrollFrame(page);
	scroll:SetPoint("TOPLEFT", header, "BOTTOMLEFT", 0, -2);
	scroll:SetPoint("BOTTOMRIGHT", addBar, "TOPRIGHT", 0, 6);
	page.scroll = scroll;
	page.child = child;

	page.empty = Common.Label(child, "", nil, NOGGIT_COLOR_DISABLED);
	page.empty:SetPoint("TOPLEFT", child, "TOPLEFT", 4, -4);

	page.unavailable = Common.Paragraph(page, "");
	page.unavailable:SetPoint("TOPLEFT", page, "TOPLEFT", 4, -8);
	page.unavailable:SetPoint("TOPRIGHT", page, "TOPRIGHT", -4, -8);
	page.unavailable:Hide();

	local function Move(index, delta)
		local items = page.items;
		local target = index + delta;

		if ( target < 1 or target > #items ) then
			return;
		end

		items[index], items[target] = items[target], items[index];
		page:Refresh();
	end

	local function CreateRow(index)
		local row = CreateFrame("Frame", NoggitUI_NewName("GameEditorCreatureVendorRow"), child);
		row:SetHeight(ROW_HEIGHT);
		row:SetPoint("TOPLEFT", child, "TOPLEFT", 0, -(index - 1) * ROW_HEIGHT);
		row:SetPoint("TOPRIGHT", child, "TOPRIGHT", 0, -(index - 1) * ROW_HEIGHT);

		row.delete = Common.Button(row, "X", 20, function() table.remove(page.items, row.index); page:Refresh(); end, "Remove this item");
		row.delete:SetHeight(20);
		row.delete:SetPoint("RIGHT", row, "RIGHT", 0, 0);
		row.down = Common.Button(row, "v", 18, function() Move(row.index, 1); end, "Move down");
		row.down:SetHeight(20);
		row.down:SetPoint("RIGHT", row.delete, "LEFT", -2, 0);
		row.up = Common.Button(row, "^", 18, function() Move(row.index, -1); end, "Move up");
		row.up:SetHeight(20);
		row.up:SetPoint("RIGHT", row.down, "LEFT", -2, 0);

		local function Bind(edit, key)
			edit:SetScript("OnTextChanged", function(self)
				local item = page.items[row.index];

				if ( item and not row.filling ) then
					item[key] = tonumber(self:GetText()) or 0;

					if ( key == "item" ) then
						row.name:SetText(ItemText(item.item));
					end

					page:UpdateState();
				end
			end);
			edit:SetScript("OnEnterPressed", function(self) self:ClearFocus(); end);
		end

		row.cost = NumberEdit(row, 44, "ItemExtendedCost id, 0 sells for gold");
		row.cost:SetPoint("RIGHT", row.up, "LEFT", -6, 0);
		Bind(row.cost, "ExtendedCost");
		row.restock = NumberEdit(row, 50, "Seconds until the limited stock refills");
		row.restock:SetPoint("RIGHT", row.cost, "LEFT", -4, 0);
		Bind(row.restock, "incrtime");
		row.max = NumberEdit(row, 36, "Max count, 0 sells without limit", 3);
		row.max:SetPoint("RIGHT", row.restock, "LEFT", -4, 0);
		Bind(row.max, "maxcount");

		row.id = NumberEdit(row, 60, "Item id, a negative number sells every item of that vendor entry", 10);
		row.id:SetPoint("LEFT", row, "LEFT", 0, 0);
		Bind(row.id, "item");

		row.name = Common.Label(row, "");
		row.name:SetPoint("LEFT", row.id, "RIGHT", 6, 0);
		row.name:SetPoint("RIGHT", row.max, "LEFT", -4, 0);
		row.name:SetHeight(18);

		local hit = CreateFrame("Button", nil, row);
		hit:SetAllPoints(row.name);
		hit:SetScript("OnEnter", function(self)
			local item = page.items[row.index];
			local id = item and item.item or 0;

			if ( id > 0 and GetItemInfo(id) ) then
				GameTooltip:SetOwner(self, "ANCHOR_RIGHT");
				GameTooltip:SetHyperlink("item:"..id);
				GameTooltip:Show();
			end
		end);
		hit:SetScript("OnLeave", function() GameTooltip:Hide(); end);

		return row;
	end

	page.Refresh = function(self)
		for index, item in ipairs(self.items) do
			local row = self.rows[index] or CreateRow(index);
			self.rows[index] = row;
			row.index = index;
			row.filling = true;

			for edit, key in pairs({ [row.id] = "item", [row.max] = "maxcount", [row.restock] = "incrtime", [row.cost] = "ExtendedCost" }) do
				if ( not edit:HasFocus() ) then
					edit:SetText(tostring(item[key] or 0));
				end
			end

			row.filling = false;
			row.name:SetText(ItemText(item.item));
			Common.SetEnabled(row.up, index > 1);
			Common.SetEnabled(row.down, index < #self.items);
			row:Show();
		end

		for index = #self.items + 1, #self.rows do
			self.rows[index]:Hide();
		end

		if ( self.loading ) then
			self.empty:SetText("Loading the vendor list...");
			self.empty:Show();
		elseif ( #self.items == 0 ) then
			self.empty:SetText("This entry sells nothing. Add items below.");
			self.empty:Show();
		else
			self.empty:Hide();
		end

		self.child:SetHeight(max(24, #self.items * ROW_HEIGHT + 4));
		self.scroll.UpdateRange();
		self:UpdateState();
	end;

	page.AddItem = function(self, id)
		id = tonumber(id);

		if ( not id or id == 0 or not self.entry ) then
			return;
		end

		for _, item in ipairs(self.items) do
			if ( item.item == id and Num(item.ExtendedCost) == 0 ) then
				GameEditor_Notify(format("Item %d is already on this vendor", id), "info");
				return;
			end
		end

		table.insert(self.items, { item = id, maxcount = 0, incrtime = 0, ExtendedCost = 0, VerifiedBuild = 0 });
		self:Refresh();
		self.scroll:SetVerticalScroll(self.scroll:GetVerticalScrollRange() or 0);
	end;

	page.HasChanges = function(self)
		return self.available and self.entry ~= nil and not self.loading and not SameRows(self.items, self.original);
	end;

	page.UpdateState = function(self)
		local dirty = self:HasChanges();

		if ( self.saving ) then
			self.status:SetText("Saving...");
			NoggitText_SetColor(self.status, NOGGIT_COLOR_DISABLED);
		elseif ( dirty ) then
			self.status:SetText(format("%d item%s, unsaved changes", #self.items, #self.items == 1 and "" or "s"));
			NoggitText_SetColor(self.status, NOGGIT_COLOR_GOLD);
		else
			self.status:SetText(format("%d item%s", #self.items, #self.items == 1 and "" or "s"));
			NoggitText_SetColor(self.status, NOGGIT_COLOR_DISABLED);
		end

		Common.SetEnabled(self.save, dirty and not self.saving);
		Common.SetEnabled(self.revert, dirty);

		if ( onDirty ) then
			onDirty(self);
		end
	end;

	page.Query = function(self)
		if ( not self.entry or not WorldTable_Query ) then
			return;
		end

		self.loading = true;
		WorldTable_Query(TABLE, self.entry);
		self:Refresh();
	end;

	page.OnRows = function(self, ok, message)
		self.loading = false;

		if ( ok ~= nil and ok ~= 1 and ok ~= true ) then
			GameEditor_Notify("npc_vendor: "..((message and message ~= "") and message or "the server could not read the rows"), "error");
			self:Refresh();
			return;
		end

		local data = WorldTable_GetRows(TABLE, self.entry);
		local rows = {};

		for _, row in ipairs(data and data.rows or {}) do
			table.insert(rows, {
				slot = Num(row.slot),
				item = Num(row.item),
				maxcount = Num(row.maxcount),
				incrtime = Num(row.incrtime),
				ExtendedCost = Num(row.ExtendedCost),
				VerifiedBuild = Num(row.VerifiedBuild),
			});
		end

		table.sort(rows, function(a, b)
			if ( a.slot ~= b.slot ) then
				return a.slot < b.slot;
			end

			return a.item < b.item;
		end);

		local keepEdits = self.keepEdits and self:HasChanges();
		self.keepEdits = nil;
		self.original = rows;

		if ( not keepEdits ) then
			self.items = CopyRows(rows);
		end

		self:Refresh();
	end;

	page.Save = function(self)
		if ( not self:HasChanges() or self.saving ) then
			return false;
		end

		local seen = {};
		local rows = {};

		for index, item in ipairs(self.items) do
			if ( Num(item.item) == 0 ) then
				GameEditor_Notify(format("Row %d has no item, set one or remove the row", index), "error");
				return false;
			end

			local key = Num(item.item)..":"..Num(item.ExtendedCost);

			if ( seen[key] ) then
				GameEditor_Notify(format("Item %d with cost %d is listed twice", Num(item.item), Num(item.ExtendedCost)), "error");
				return false;
			end

			seen[key] = true;

			if ( Num(item.maxcount) > 255 or Num(item.maxcount) < 0 ) then
				GameEditor_Notify(format("Max count of item %d must be 0 to 255", Num(item.item)), "error");
				return false;
			end

			table.insert(rows, {
				entry = self.entry,
				slot = index - 1,
				item = Num(item.item),
				maxcount = Num(item.maxcount),
				incrtime = max(0, Num(item.incrtime)),
				ExtendedCost = max(0, Num(item.ExtendedCost)),
				VerifiedBuild = Num(item.VerifiedBuild),
			});
		end

		if ( #rows > MAX_ITEMS ) then
			GameEditor_Notify(format("A vendor can show at most %d items", MAX_ITEMS), "error");
			return false;
		end

		local columns = { "entry", "slot", "item", "maxcount", "incrtime", "ExtendedCost", "VerifiedBuild" };

		if ( not WorldTable_Save(TABLE, self.entry, columns, rows) ) then
			GameEditor_Notify("The vendor list could not be sent", "error");
			return false;
		end

		self.saving = true;
		self:UpdateState();
		return true;
	end;

	page.OnResult = function(self, ok, message)
		self.saving = false;

		if ( ok == 1 or ok == true ) then
			GameEditor_Notify(format("Vendor list of entry %d saved, reopen the vendor window to see it", self.entry), "info");
			self:Query();
		else
			GameEditor_Notify("npc_vendor: "..(message or "save failed"), "error");
			self:UpdateState();
		end
	end;

	page.Revert = function(self)
		self.items = CopyRows(self.original);
		self:Refresh();
	end;

	page.SetAvailable = function(self, available, text)
		self.available = available and true or false;

		for _, region in ipairs({ intro, header, scroll, addBar, bar }) do
			if ( available ) then
				region:Show();
			else
				region:Hide();
			end
		end

		if ( available ) then
			self.unavailable:Hide();
		else
			self.flagWarning:Hide();
			self.unavailable:SetText(text or "");
			self.unavailable:Show();
		end
	end;

	page.Load = function(self, data)
		if ( not data or not data.entry or data.entry == 0 ) then
			self:SetAvailable(false, "Select a creature or open a template to edit what it sells.");
			return;
		end

		self:SetAvailable(true);

		local flags = Num(data.template and data.template.npcflag);

		if ( GameEditor_HasBit(flags, 7) or GameEditor_HasBit(Num(data.spawn and data.spawn.npcflag), 7) ) then
			self.flagWarning:Hide();
			header:SetPoint("TOPLEFT", intro, "BOTTOMLEFT", 0, -2);
			header:SetPoint("TOPRIGHT", intro, "BOTTOMRIGHT", 0, -2);
		else
			self.flagWarning:Show();
			header:SetPoint("TOPLEFT", self.flagWarning, "BOTTOMLEFT", 0, -2);
			header:SetPoint("TOPRIGHT", self.flagWarning, "BOTTOMRIGHT", 0, -2);
		end

		if ( self.entry ~= data.entry ) then
			self.entry = data.entry;
			self.original = {};
			self.items = {};
			self:Query();
		elseif ( not self.loading and not self.saving and not self:HasChanges() and not self.queried ) then
			self:Query();
		end

		self.queried = true;
	end;

	local elapsedSinceNames = 0;

	page:SetScript("OnUpdate", function(self, elapsed)
		elapsedSinceNames = elapsedSinceNames + elapsed;

		if ( elapsedSinceNames < 1 ) then
			return;
		end

		elapsedSinceNames = 0;

		for index, item in ipairs(self.items) do
			local row = self.rows[index];

			if ( row and row:IsShown() ) then
				row.name:SetText(ItemText(item.item));
			end
		end
	end);

	local events = CreateFrame("Frame", nil, page);
	events:RegisterEvent("WORLD_TABLE_ROWS");
	events:RegisterEvent("WORLD_TABLE_RESULT");
	events:SetScript("OnEvent", function(_, event, ...)
		if ( event == "WORLD_TABLE_ROWS" ) then
			local tableName, key, ok, message = ...;

			if ( string.lower(tostring(tableName or "")) == TABLE and tonumber(key) == page.entry ) then
				page:OnRows(ok, message);
			end
		else
			local ok, tableName, key, message = ...;

			if ( string.lower(tostring(tableName or "")) == TABLE and tonumber(key) == page.entry ) then
				page:OnResult(ok, message);
			end
		end
	end);

	page:Refresh();
	return page;
end
