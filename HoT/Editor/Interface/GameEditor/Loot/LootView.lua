-- Loot tree of a source: its own loot tables first, then every reference they pull, with an editor for the selected line.

GameEditorLoot = GameEditorLoot or {};
local L = GameEditorLoot;

local ROW_HEIGHT = 22;
local INDENT = 14;
local MAX_DEPTH = 8;
local DETAIL_HEIGHT = 224;

local Num = L.Num;

local function Common()
	return GameEditorObject;
end

local function Label(parent, text, font, color)
	return Common().Label(parent, text, font, color);
end

local function Button(parent, text, width, onClick, tooltip)
	local button = Common().Button(parent, text, width, onClick, tooltip);
	button:SetHeight(20);
	return button;
end

local function SetShown(widget, shown)
	if ( shown ) then
		widget:Show();
	else
		widget:Hide();
	end
end

local function NumberText(value)
	value = Num(value);

	if ( value == math.floor(value) ) then
		return format("%d", value);
	end

	return (string.gsub(format("%.4f", value), "0+$", ""));
end

local function FormatChance(value)
	value = Num(value);

	if ( value == math.floor(value) ) then
		return format("%d%%", value);
	end

	return NumberText(value).."%";
end

local function CountText(row)
	if ( L.IsReference(row) ) then
		return format("x%d", Num(row.MaxCount));
	end

	local minCount, maxCount = Num(row.MinCount), Num(row.MaxCount);
	return minCount == maxCount and tostring(minCount) or format("%d-%d", minCount, maxCount);
end

local function CategoryTitle(category)
	if ( category.table == L.REFERENCE_TABLE and not category.isRoot ) then
		return format("Reference %d", category.key);
	end

	return category.label or (L.byTable[category.table] and L.byTable[category.table].title) or category.table;
end

local function ViaText(via)
	local parts = {};

	for _, link in ipairs(via or {}) do
		local row = link.row;
		local group = Num(row.GroupId) > 0 and format(", only group %d", Num(row.GroupId)) or "";
		table.insert(parts, format("%s (%s, %s%s)", CategoryTitle(link.category), FormatChance(row.Chance), CountText(row), group));
	end

	return table.concat(parts, "; ");
end

-- Builds the visible lines from the roots, references expand below the table that pulls them.
local function BuildLines(view)
	local lines = {};
	local placed = {};

	local function Info(text, depth, category)
		table.insert(lines, { kind = "info", text = text, depth = depth, category = category });
	end

	local function AddCategory(spec, depth, via, stack)
		local id = spec.key > 0 and L.SetId(spec.table, spec.key) or ("missing:"..spec.table);

		if ( placed[id] ) then
			if ( via ) then
				table.insert(placed[id].via, via);
			end

			return;
		end

		local set = spec.key > 0 and L.Load(spec.table, spec.key) or nil;
		local category = {
			kind = "category", id = id, table = spec.table, key = spec.key, set = set, label = spec.label, depth = depth,
			via = via and { via } or {}, isRoot = via == nil, spec = spec,
		};
		category.category = category;
		placed[id] = category;
		table.insert(lines, category);

		if ( view.collapsed[id] ) then
			return;
		end

		if ( not set ) then
			Info(spec.missingText or "No loot id is set", depth + 1, category);
			return;
		end

		if ( set.error and not set.loaded ) then
			Info("Could not load: "..set.error, depth + 1, category);
			return;
		end

		if ( not set.loaded ) then
			Info("Loading...", depth + 1, category);
			return;
		end

		if ( #set.rows == 0 ) then
			Info("No rows. Select this line and add an item or a reference.", depth + 1, category);
		end

		for _, row in ipairs(set.rows) do
			table.insert(lines, { kind = "row", set = set, row = row, depth = depth + 1, category = category });
		end

		stack[id] = true;

		for _, row in ipairs(set.rows) do
			local reference = Num(row.Reference);

			if ( reference > 0 ) then
				local refId = L.SetId(L.REFERENCE_TABLE, reference);

				if ( stack[refId] ) then
					Info(format("Reference %d pulls itself again, the server skips this loop", reference), depth + 1, category);
				elseif ( depth + 1 >= MAX_DEPTH ) then
					Info(format("Reference %d is nested too deep to show here", reference), depth + 1, category);
				else
					AddCategory({ table = L.REFERENCE_TABLE, key = reference }, depth + 1, { category = category, row = row }, stack);
				end
			end
		end

		stack[id] = nil;
	end

	for _, root in ipairs(view.roots) do
		AddCategory({ table = root.table, key = Num(root.key), label = root.label, missingText = root.missingText, onCreate = root.onCreate }, 0, nil, {});
	end

	return lines, placed;
end

local function LineKey(line)
	if ( not line ) then
		return nil;
	end

	if ( line.kind == "row" ) then
		return "row:"..tostring(line.row._uid);
	end

	return "cat:"..tostring(line.category and line.category.id or "");
end

-- Row and category editor -------------------------------------------------------------------------------------

local function NumberEdit(parent, width, tooltip, letters)
	local edit = NoggitLineEdit_Create(parent, width);
	edit:SetHeight(20);
	edit:SetMaxLetters(letters or 10);
	edit:SetAutoFocus(false);
	Common().Tooltip(edit, tooltip);
	edit:SetScript("OnEnterPressed", function(self) self:ClearFocus(); end);
	edit:SetScript("OnEscapePressed", function(self) self:ClearFocus(); end);
	return edit;
end

local FIELDS = {
	{ key = "Chance", label = "Chance", tooltip = "Drop chance in percent. Inside a group 0 means the row shares what the explicit chances of the group leave over.", decimals = true },
	{ key = "GroupId", label = "Group", tooltip = "0 rolls on its own, rows with the same group above 0 drop at most one item of the group" },
	{ key = "LootMode", label = "Loot mode", tooltip = "Bit mask, the row drops only while the creature or object has one of these loot mode bits, 1 is the normal mode" },
	{ key = "MinCount", label = "Min count", tooltip = "Least stack size that drops" },
	{ key = "MaxCount", label = "Max count", tooltip = "Largest stack size, on a reference this is how many times the reference rolls" },
	{ key = "Reference", label = "Reference", tooltip = "reference_loot_template id, above 0 turns the row into a reference that rolls that loot id" },
	{ key = "RequiredDungeonLevel", label = "Dungeon lvl", tooltip = "The row only drops when the dungeon level is at least this" },
	{ key = "MapId", label = "Map", tooltip = "The row only drops on this map id, 0 drops everywhere" },
	{ key = "MagicFindChanceCoef", label = "MF chance", tooltip = "Magic find chance coefficient, 0 uses the default 0.008", decimals = true },
	{ key = "MagicFindAmountCoef", label = "MF amount", tooltip = "Magic find amount coefficient", decimals = true },
};

local function BuildRowEditor(view, parent)
	local editor = CreateFrame("Frame", NoggitUI_NewName("GameEditorLootRowEditor"), parent);
	editor:SetAllPoints(parent);
	editor.edits = {};

	editor.icon = editor:CreateTexture(nil, "ARTWORK");
	editor.icon:SetWidth(30);
	editor.icon:SetHeight(30);
	editor.icon:SetPoint("TOPLEFT", editor, "TOPLEFT", 2, -4);

	editor.title = Label(editor, "", "NoggitFontBold");
	editor.title:SetPoint("TOPLEFT", editor.icon, "TOPRIGHT", 6, -1);
	editor.title:SetPoint("RIGHT", editor, "RIGHT", -4, 0);
	editor.title:SetJustifyH("LEFT");

	editor.where = Label(editor, "", nil, NOGGIT_COLOR_DISABLED);
	editor.where:SetPoint("TOPLEFT", editor.title, "BOTTOMLEFT", 0, -3);
	editor.where:SetPoint("RIGHT", editor, "RIGHT", -4, 0);
	editor.where:SetJustifyH("LEFT");

	local hit = CreateFrame("Frame", nil, editor);
	hit:SetAllPoints(editor.icon);
	hit:EnableMouse(true);
	hit:SetScript("OnEnter", function(self)
		local line = view.selectedLine;
		local item = line and line.row and not L.IsReference(line.row) and Num(line.row.Item) or 0;

		if ( item > 0 and GetItemInfo(item) ) then
			GameTooltip:SetOwner(self, "ANCHOR_RIGHT");
			GameTooltip:SetHyperlink("item:"..item);
			GameTooltip:Show();
		end
	end);
	hit:SetScript("OnLeave", function() GameTooltip:Hide(); end);

	local function Apply(key, text, decimals)
		local line = view.selectedLine;

		if ( not line or line.kind ~= "row" or editor.filling ) then
			return;
		end

		local row = line.row;
		local value;

		if ( key == "Comment" ) then
			value = text or "";
		else
			value = tonumber(text) or 0;

			if ( not decimals ) then
				value = math.floor(value);
			end
		end

		if ( key == "Comment" and tostring(row.Comment or "") == value ) then
			return;
		elseif ( key ~= "Comment" and math.abs(Num(row[key]) - value) <= 0.00001 * max(1, math.abs(value)) ) then
			return;
		end

		if ( key == "Reference" and Num(row.Item) == Num(row.Reference) ) then
			row.Item = value;
		end

		L.SetValue(line.set, row, key, value);
	end

	local top = -40;
	local itemLabel = Label(editor, "Item", nil, NOGGIT_COLOR_DISABLED);
	itemLabel:SetPoint("TOPLEFT", editor, "TOPLEFT", 2, top - 3);
	editor.item = NumberEdit(editor, 70, "Item entry, on a reference row this only has to be unique inside the loot id");
	editor.item:SetPoint("TOPLEFT", editor, "TOPLEFT", 70, top);
	editor.item:SetScript("OnTextChanged", function(self) Apply("Item", self:GetText()); end);

	editor.find = Button(editor, "Find item...", 84, function()
		local line = view.selectedLine;

		if ( not line or line.kind ~= "row" ) then
			return;
		end

		L.PickItem("Item of this loot row", function(value)
			L.SetValue(line.set, line.row, "Item", Num(value));
			view:FillEditor(true);
		end);
	end, "Pick from every item, type to filter by name or entry");
	editor.find:SetPoint("LEFT", editor.item, "RIGHT", 4, 0);

	editor.quest = NoggitCheckBox_Create(editor, "Quest item", false, function(_, checked)
		local line = view.selectedLine;

		if ( line and line.kind == "row" and not editor.filling ) then
			L.SetValue(line.set, line.row, "QuestRequired", checked and 1 or 0);
		end
	end);
	editor.quest:SetPoint("LEFT", editor.find, "RIGHT", 10, 0);
	editor.quest:HookScript("OnEnter", function(self) NoggitTooltip_Show(self, "Drops only for players on a quest that needs the item, references ignore it"); end);
	editor.quest:HookScript("OnLeave", NoggitTooltip_Hide);

	local columns = 3;
	local cellWidth = 136;

	for index, field in ipairs(FIELDS) do
		local column = (index - 1) % columns;
		local line = math.floor((index - 1) / columns);
		local x = 2 + column * cellWidth;
		local y = top - 26 - line * 24;

		local label = Label(editor, field.label, nil, NOGGIT_COLOR_DISABLED);
		label:SetPoint("TOPLEFT", editor, "TOPLEFT", x, y - 3);
		local edit = NumberEdit(editor, 56, field.tooltip);
		edit:SetPoint("TOPLEFT", editor, "TOPLEFT", x + 68, y);
		edit:SetScript("OnTextChanged", function(self) Apply(field.key, self:GetText(), field.decimals); end);
		editor.edits[field.key] = edit;
	end

	local commentTop = top - 26 - math.ceil(#FIELDS / columns) * 24;
	local commentLabel = Label(editor, "Comment", nil, NOGGIT_COLOR_DISABLED);
	commentLabel:SetPoint("TOPLEFT", editor, "TOPLEFT", 2, commentTop - 3);
	editor.comment = NoggitLineEdit_Create(editor, 200);
	editor.comment:SetHeight(20);
	editor.comment:SetMaxLetters(255);
	editor.comment:SetAutoFocus(false);
	editor.comment:SetPoint("TOPLEFT", editor, "TOPLEFT", 70, commentTop);
	editor.comment:SetPoint("RIGHT", editor, "RIGHT", -4, 0);
	editor.comment:SetScript("OnTextChanged", function(self) Apply("Comment", self:GetText()); end);
	editor.comment:SetScript("OnEnterPressed", function(self) self:ClearFocus(); end);
	editor.comment:SetScript("OnEscapePressed", function(self) self:ClearFocus(); end);

	local bar = CreateFrame("Frame", nil, editor);
	bar:SetHeight(20);
	bar:SetPoint("BOTTOMLEFT", editor, "BOTTOMLEFT", 0, 0);
	bar:SetPoint("BOTTOMRIGHT", editor, "BOTTOMRIGHT", 0, 0);

	editor.delete = Button(bar, "Delete row", 76, function() view:DeleteSelected(); end, "Remove this row, Save makes it permanent (Del)");
	editor.delete:SetPoint("LEFT", bar, "LEFT", 0, 0);
	editor.duplicate = Button(bar, "Duplicate", 70, function()
		local line = view.selectedLine;

		if ( line and line.kind == "row" ) then
			local copy = L.CopyRow(line.row);
			copy._uid = nil;
			copy.Item = 0;
			view:SelectRow(L.AddRow(line.set, copy));
		end
	end, "Copy this row with an empty item, set the item before saving");
	editor.duplicate:SetPoint("LEFT", editor.delete, "RIGHT", 4, 0);
	editor.openRef = Button(bar, "Go to reference", 104, function()
		local line = view.selectedLine;

		if ( line and line.kind == "row" and L.IsReference(line.row) ) then
			view:SelectCategory(L.SetId(L.REFERENCE_TABLE, Num(line.row.Reference)));
		end
	end, "Select the reference this row rolls (double click does the same)");
	editor.openRef:SetPoint("LEFT", editor.duplicate, "RIGHT", 4, 0);
	editor.state = Label(bar, "", nil, NOGGIT_COLOR_GOLD);
	editor.state:SetPoint("RIGHT", bar, "RIGHT", -2, 0);

	editor.Fill = function(self, line, force)
		local row = line.row;
		local isReference = L.IsReference(row);
		self.filling = true;

		local function Set(edit, text)
			if ( (force or not edit:HasFocus()) and edit:GetText() ~= text ) then
				edit:SetText(text);
			end
		end

		Set(self.item, NumberText(row.Item));

		for _, field in ipairs(FIELDS) do
			Set(self.edits[field.key], NumberText(row[field.key]));
		end

		Set(self.comment, row.Comment or "");
		NoggitCheckBox_SetChecked(self.quest, Num(row.QuestRequired) ~= 0);
		self.filling = false;

		if ( isReference ) then
			local refSet = L.Get(L.REFERENCE_TABLE, Num(row.Reference));
			local count = refSet and refSet.loaded and #refSet.rows;
			self.icon:SetTexture(L.REFERENCE_ICON);
			self.title:SetText(format("|cff7fbfffReference %d|r%s", Num(row.Reference), count and format("  (%d row%s)", count, count == 1 and "" or "s") or ""));
		else
			local _, _, icon = L.ItemInfo(row.Item);
			self.icon:SetTexture(icon);
			self.title:SetText(format("%s  |cff7f7f7f%d|r", L.ItemText(row.Item), Num(row.Item)));
		end

		self.where:SetText(format("Row of %s %d", line.set.table, line.set.key));
		Common().SetEnabled(self.openRef, isReference);

		if ( row._new ) then
			self.state:SetText("new row");
		elseif ( L.RowChanged(line.set, row) ) then
			self.state:SetText("changed");
		else
			self.state:SetText("");
		end
	end;

	return editor;
end

local function BuildCategoryEditor(view, parent)
	local editor = CreateFrame("Frame", NoggitUI_NewName("GameEditorLootCategoryEditor"), parent);
	editor:SetAllPoints(parent);

	editor.title = Label(editor, "", "NoggitFontBold");
	editor.title:SetPoint("TOPLEFT", editor, "TOPLEFT", 2, -4);
	editor.title:SetPoint("RIGHT", editor, "RIGHT", -4, 0);
	editor.title:SetJustifyH("LEFT");

	editor.text = Common().Paragraph(editor, "", NOGGIT_COLOR_TEXT);
	editor.text:SetPoint("TOPLEFT", editor, "TOPLEFT", 2, -22);
	editor.text:SetPoint("RIGHT", editor, "RIGHT", -4, 0);
	editor.text:SetHeight(100);
	editor.text:SetJustifyH("LEFT");

	local addBar = CreateFrame("Frame", nil, editor);
	addBar:SetHeight(20);
	addBar:SetPoint("BOTTOMLEFT", editor, "BOTTOMLEFT", 0, 26);
	addBar:SetPoint("BOTTOMRIGHT", editor, "BOTTOMRIGHT", 0, 26);
	editor.addBar = addBar;

	editor.addId = NumberEdit(addBar, 64, "Item entry or reference id to add");
	editor.addId:SetPoint("LEFT", addBar, "LEFT", 0, 0);

	local function TypedId()
		local id = tonumber(editor.addId:GetText());

		if ( not id or id <= 0 ) then
			GameEditor_Notify("Type an id first", "info");
			return nil;
		end

		editor.addId:SetText("");
		editor.addId:ClearFocus();
		return math.floor(id);
	end

	editor.addItem = Button(addBar, "Add item", 64, function()
		local id = TypedId();

		if ( id ) then
			view:AddToSelected({ Item = id });
		end
	end, "Add the typed item entry to this loot id");
	editor.addItem:SetPoint("LEFT", editor.addId, "RIGHT", 4, 0);
	editor.addId:SetScript("OnEnterPressed", function() editor.addItem:Click(); end);

	editor.addRef = Button(addBar, "Add reference", 94, function()
		local id = TypedId();

		if ( id ) then
			view:AddToSelected({ Item = id, Reference = id });
		end
	end, "Add the typed reference_loot_template id, an unused id becomes a new empty reference");
	editor.addRef:SetPoint("LEFT", editor.addItem, "RIGHT", 4, 0);

	editor.findItem = Button(addBar, "Find item...", 84, function()
		local category = view.selectedLine and view.selectedLine.category;

		if ( not category or not category.set ) then
			return;
		end

		L.PickItem("Add item to "..CategoryTitle(category), function(value)
			view:AddToSelected({ Item = Num(value) });
		end);
	end, "Pick from every item, type to filter by name or entry");
	editor.findItem:SetPoint("LEFT", editor.addRef, "RIGHT", 4, 0);

	editor.create = Button(addBar, "Create loot id", 110, function()
		local category = view.selectedLine and view.selectedLine.category;

		if ( category and category.spec.onCreate ) then
			category.spec.onCreate();
		end
	end, "Point the source at a loot id so rows can be added");
	editor.create:SetPoint("LEFT", addBar, "LEFT", 0, 0);

	local bar = CreateFrame("Frame", nil, editor);
	bar:SetHeight(20);
	bar:SetPoint("BOTTOMLEFT", editor, "BOTTOMLEFT", 0, 0);
	bar:SetPoint("BOTTOMRIGHT", editor, "BOTTOMRIGHT", 0, 0);

	editor.save = Button(bar, "Save", 56, function()
		local set = view.selectedLine and view.selectedLine.category and view.selectedLine.category.set;
		L.Save(set);
	end, "Replace the rows of this loot id on the server and reload the loot table");
	editor.save:SetPoint("LEFT", bar, "LEFT", 0, 0);
	editor.revert = Button(bar, "Revert", 56, function()
		L.Revert(view.selectedLine and view.selectedLine.category and view.selectedLine.category.set);
	end, "Throw away the unsaved changes of this loot id");
	editor.revert:SetPoint("LEFT", editor.save, "RIGHT", 4, 0);
	editor.reload = Button(bar, "Reload", 56, function()
		local set = view.selectedLine and view.selectedLine.category and view.selectedLine.category.set;

		if ( set ) then
			L.Reload(set);
		end
	end, "Load this loot id again from the server, unsaved changes are lost");
	editor.reload:SetPoint("LEFT", editor.revert, "RIGHT", 4, 0);
	editor.usedBy = Button(bar, "Used by...", 74, function()
		local category = view.selectedLine and view.selectedLine.category;

		if ( category ) then
			L.ShowReferenceUse(category.key);
		end
	end, "Every loot row that rolls this reference, editing it changes all of them");
	editor.usedBy:SetPoint("LEFT", editor.reload, "RIGHT", 4, 0);
	editor.collapse = Button(bar, "Collapse", 64, function()
		local category = view.selectedLine and view.selectedLine.category;

		if ( category ) then
			view:ToggleCollapsed(category.id);
		end
	end, "Hide or show the rows of this loot id (double click the line does the same)");
	editor.collapse:SetPoint("RIGHT", bar, "RIGHT", 0, 0);

	editor.Fill = function(self, line)
		local category = line.category;
		local set = category.set;
		local info = L.byTable[category.table];
		local lines = {};

		self.title:SetText(format("%s   |cff7f7f7f%s %s|r", CategoryTitle(category), category.table, category.key > 0 and tostring(category.key) or "-"));

		if ( not set ) then
			table.insert(lines, category.spec.missingText or "No loot id is set.");
		else
			if ( #category.via > 0 ) then
				table.insert(lines, "Rolled by "..ViaText(category.via));
			elseif ( info and info.source ~= "reference" ) then
				table.insert(lines, format("%s of the source, loot id %d.", info.title, category.key));
			end

			if ( set.loaded ) then
				local items, references = 0, 0;

				for _, row in ipairs(set.rows) do
					if ( L.IsReference(row) ) then
						references = references + 1;
					else
						items = items + 1;
					end
				end

				table.insert(lines, format("%d item row%s, %d reference%s.", items, items == 1 and "" or "s", references, references == 1 and "" or "s"));

				for _, group in ipairs(L.GroupSummary(set)) do
					local rest = 100 - group.explicit;
					local text = format("Group %d: %d rows, explicit chances add up to %s", group.group, group.count, FormatChance(group.explicit));

					if ( group.equal > 0 ) then
						text = text..format(", %d equal row%s share %s", group.equal, group.equal == 1 and "" or "s", FormatChance(max(0, rest)));
					end

					if ( group.explicit > 100 ) then
						text = "|cffff6060"..text..", above 100% so some rows never drop|r";
					end

					table.insert(lines, text);
				end
			elseif ( set.loading ) then
				table.insert(lines, "Loading...");
			end

			if ( category.table == L.REFERENCE_TABLE ) then
				table.insert(lines, "|cffffcc00References can be shared, Used by lists every loot id that rolls this one.|r");
			end
		end

		self.text:SetText(table.concat(lines, "\n"));

		local hasSet = set ~= nil and set.loaded;
		local dirty = set and L.IsDirty(set);

		for _, widget in ipairs({ self.addId, self.addItem, self.addRef, self.findItem }) do
			SetShown(widget, set ~= nil);
			Common().SetEnabled(widget, hasSet and not set.saving);
		end

		SetShown(self.create, not set and category.spec.onCreate ~= nil);
		Common().SetEnabled(self.save, dirty and not set.saving);
		Common().SetEnabled(self.revert, dirty);
		Common().SetEnabled(self.reload, set ~= nil and not set.loading);
		SetShown(self.usedBy, category.table == L.REFERENCE_TABLE);
		self.collapse:SetText(view.collapsed[category.id] and "Expand" or "Collapse");
	end;

	return editor;
end

-- View --------------------------------------------------------------------------------------------------------

-- options: onDirty(view), embedded (adds Open in Loot), emptyText.
function L.CreateView(parent, options)
	options = options or {};

	local view = CreateFrame("Frame", NoggitUI_NewName("GameEditorLootView"), parent);
	view.roots = {};
	view.collapsed = {};
	view.lines = {};
	view.placed = {};

	view.title = Label(view, "", "NoggitFontBold");
	view.title:SetPoint("TOPLEFT", view, "TOPLEFT", 2, -4);
	view.title:SetJustifyH("LEFT");

	local toolbar = CreateFrame("Frame", nil, view);
	toolbar:SetHeight(20);
	toolbar:SetPoint("TOPLEFT", view, "TOPLEFT", 0, -22);
	toolbar:SetPoint("TOPRIGHT", view, "TOPRIGHT", 0, -22);

	view.saveAll = Button(toolbar, "Save all", 64, function() view:Save(); end, "Save every changed loot id shown here (Ctrl+S)");
	view.saveAll:SetPoint("LEFT", toolbar, "LEFT", 0, 0);
	view.revertAll = Button(toolbar, "Revert all", 70, function() view:Revert(); end, "Throw away every unsaved loot change shown here");
	view.revertAll:SetPoint("LEFT", view.saveAll, "RIGHT", 4, 0);
	view.reload = Button(toolbar, "Reload", 56, function() view:Reload(); end, "Load every loot id shown here again from the server (F5)");
	view.reload:SetPoint("LEFT", view.revertAll, "RIGHT", 4, 0);

	if ( options.embedded ) then
		view.open = Button(toolbar, "Open in Loot", 90, function()
			L.OpenRoots(view.roots, view.titleText);
		end, "Show this loot in the Loot workspace with more room");
		view.open:SetPoint("RIGHT", toolbar, "RIGHT", 0, 0);
	end

	view.status = Label(toolbar, "", nil, NOGGIT_COLOR_DISABLED);
	view.status:SetPoint("LEFT", view.reload, "RIGHT", 8, 0);
	view.status:SetPoint("RIGHT", view.open or toolbar, view.open and "LEFT" or "RIGHT", -4, 0);
	view.status:SetJustifyH("LEFT");
	view.title:SetPoint("RIGHT", view, "RIGHT", -4, 0);

	local header = CreateFrame("Frame", nil, view);
	header:SetHeight(14);
	header:SetPoint("TOPLEFT", toolbar, "BOTTOMLEFT", 0, -4);
	header:SetPoint("TOPRIGHT", toolbar, "BOTTOMRIGHT", 0, -4);

	local function HeaderLabel(text, anchor, x)
		local label = Label(header, text, nil, NOGGIT_COLOR_DISABLED);
		label:SetPoint(anchor, header, anchor, x, 0);
	end

	HeaderLabel("Item", "LEFT", 4);
	HeaderLabel("Chance", "RIGHT", -128);
	HeaderLabel("Grp", "RIGHT", -96);
	HeaderLabel("Count", "RIGHT", -52);
	HeaderLabel("Q", "RIGHT", -28);

	local detail = CreateFrame("Frame", NoggitUI_NewName("GameEditorLootDetail"), view, "NoggitBoxTemplate");
	NoggitBox_Set(detail, NOGGIT_COLOR_BASE, NOGGIT_COLOR_MID);
	detail:SetHeight(DETAIL_HEIGHT);
	detail:SetPoint("BOTTOMLEFT", view, "BOTTOMLEFT", 0, 0);
	detail:SetPoint("BOTTOMRIGHT", view, "BOTTOMRIGHT", 0, 0);

	local detailInner = CreateFrame("Frame", nil, detail);
	detailInner:SetPoint("TOPLEFT", detail, "TOPLEFT", 6, -4);
	detailInner:SetPoint("BOTTOMRIGHT", detail, "BOTTOMRIGHT", -6, 6);

	view.rowEditor = BuildRowEditor(view, detailInner);
	view.categoryEditor = BuildCategoryEditor(view, detailInner);
	view.noSelection = Common().Paragraph(detailInner, "Click a loot line to edit it. Click a table or reference line to add rows, save or revert it. Double click a reference row to jump to it.");
	view.noSelection:SetPoint("TOPLEFT", detailInner, "TOPLEFT", 2, -4);
	view.noSelection:SetPoint("TOPRIGHT", detailInner, "TOPRIGHT", -2, -4);

	local list = NoggitListBox_Create(view, ROW_HEIGHT, function(row)
		row.icon = row:CreateTexture(nil, "ARTWORK");
		row.icon:SetWidth(18);
		row.icon:SetHeight(18);
		row.entry = NoggitUI_FontString(row, "");
		row.entry:SetWidth(52);
		NoggitText_SetColor(row.entry, NOGGIT_COLOR_DISABLED);
		row.mark = NoggitUI_FontString(row, "", nil, "RIGHT");
		row.mark:SetPoint("RIGHT", row, "RIGHT", -2, 0);
		row.mark:SetWidth(10);
		NoggitText_SetColor(row.mark, NOGGIT_COLOR_GOLD);
		row.quest = NoggitUI_FontString(row, "", nil, "CENTER");
		row.quest:SetPoint("RIGHT", row, "RIGHT", -14, 0);
		row.quest:SetWidth(16);
		NoggitText_SetColor(row.quest, NOGGIT_COLOR_GOLD);
		row.count = NoggitUI_FontString(row, "", nil, "RIGHT");
		row.count:SetPoint("RIGHT", row, "RIGHT", -34, 0);
		row.count:SetWidth(40);
		row.group = NoggitUI_FontString(row, "", nil, "RIGHT");
		row.group:SetPoint("RIGHT", row, "RIGHT", -80, 0);
		row.group:SetWidth(24);
		row.chance = NoggitUI_FontString(row, "", nil, "RIGHT");
		row.chance:SetPoint("RIGHT", row, "RIGHT", -108, 0);
		row.chance:SetWidth(58);
		row.name = NoggitUI_FontString(row, "");
		row.name:SetPoint("RIGHT", row.chance, "LEFT", -4, 0);
		row.name:SetJustifyH("LEFT");
		row.title = NoggitUI_FontString(row, "", "NoggitFontBold");
		row.title:SetJustifyH("LEFT");
		row.state = NoggitUI_FontString(row, "", nil, "RIGHT");
		row.state:SetPoint("RIGHT", row, "RIGHT", -14, 0);
		row.title:SetPoint("RIGHT", row.state, "LEFT", -6, 0);

		row:HookScript("OnEnter", function(self)
			local line = self.line;
			local item = line and line.kind == "row" and not L.IsReference(line.row) and Num(line.row.Item) or 0;

			if ( item > 0 and GetItemInfo(item) ) then
				GameTooltip:SetOwner(self, "ANCHOR_RIGHT");
				GameTooltip:SetHyperlink("item:"..item);
				GameTooltip:Show();
			end
		end);
		row:HookScript("OnLeave", function() GameTooltip:Hide(); end);
	end, function(row, line)
		row.line = line;
		local indent = 4 + (line.depth or 0) * INDENT;
		local isRow = line.kind == "row";

		for _, part in ipairs({ row.icon, row.entry, row.name, row.chance, row.group, row.count, row.quest, row.mark }) do
			SetShown(part, isRow);
		end

		SetShown(row.title, not isRow);
		SetShown(row.state, line.kind == "category");

		if ( isRow ) then
			local data = line.row;
			row.icon:SetPoint("LEFT", row, "LEFT", indent, 0);
			row.entry:SetPoint("LEFT", row.icon, "RIGHT", 4, 0);
			row.name:SetPoint("LEFT", row.entry, "RIGHT", 2, 0);

			if ( L.IsReference(data) ) then
				local refSet = L.Get(L.REFERENCE_TABLE, Num(data.Reference));
				local count = refSet and refSet.loaded and #refSet.rows;
				row.icon:SetTexture(L.REFERENCE_ICON);
				row.entry:SetText(tostring(Num(data.Reference)));
				row.name:SetText(format("|cff7fbfffReference %d|r%s", Num(data.Reference), count and format(" |cff7f7f7f(%d row%s)|r", count, count == 1 and "" or "s") or ""));
				row.quest:SetText("");
			else
				local _, _, icon = L.ItemInfo(data.Item);
				row.icon:SetTexture(icon);
				row.entry:SetText(tostring(Num(data.Item)));
				row.name:SetText(L.ItemText(data.Item));
				row.quest:SetText(Num(data.QuestRequired) ~= 0 and "Q" or "");
			end

			local chance = Num(data.Chance);
			row.chance:SetText(chance == 0 and Num(data.GroupId) > 0 and "|cff7f7f7fequal|r" or FormatChance(chance));
			row.group:SetText(Num(data.GroupId) > 0 and tostring(Num(data.GroupId)) or "");
			row.count:SetText(CountText(data));
			row.mark:SetText((data._new or L.RowChanged(line.set, data)) and "*" or "");
		elseif ( line.kind == "category" ) then
			local set = line.set;
			local caret = view.collapsed[line.id] and "+ " or "- ";
			local text = caret..CategoryTitle(line);

			if ( line.key > 0 ) then
				text = text..format("  |cff7f7f7f%d|r", line.key);
			end

			if ( #line.via > 0 ) then
				text = text.."  |cff7f7f7fvia "..ViaText(line.via).."|r";
			end

			row.title:SetPoint("LEFT", row, "LEFT", indent, 0);
			row.title:SetText(text);
			NoggitText_SetColor(row.title, line.isRoot and NOGGIT_COLOR_WHITE or { 0.5, 0.75, 1, 1 });

			local state = "";

			if ( not set ) then
				state = "|cff7f7f7fnone|r";
			elseif ( set.saving ) then
				state = "|cff7f7f7fsaving|r";
			elseif ( set.loading and not set.loaded ) then
				state = "|cff7f7f7floading|r";
			elseif ( L.IsDirty(set) ) then
				state = "|cffffcc00unsaved|r";
			elseif ( set.loaded ) then
				state = format("|cff7f7f7f%d|r", #set.rows);
			end

			row.state:SetText(state);
		else
			row.title:SetPoint("LEFT", row, "LEFT", indent, 0);
			row.title:SetText("|cff7f7f7f"..(line.text or "").."|r");
		end
	end);
	list:SetPoint("TOPLEFT", header, "BOTTOMLEFT", 0, -2);
	list:SetPoint("RIGHT", view, "RIGHT", 0, 0);
	list:SetPoint("BOTTOM", detail, "TOP", 0, 6);
	view.list = list;

	list.onSelect = function(line)
		view.selectedKey = LineKey(line);
		view.selectedLine = line;
		view:FillEditor(true);
	end;

	list.onDoubleClick = function(line)
		if ( line.kind == "category" ) then
			view:ToggleCollapsed(line.id);
		elseif ( line.kind == "row" and L.IsReference(line.row) ) then
			view:SelectCategory(L.SetId(L.REFERENCE_TABLE, Num(line.row.Reference)));
		end
	end;

	view.FillEditor = function(self, force)
		local line = self.selectedLine;
		SetShown(self.rowEditor, line and line.kind == "row");
		SetShown(self.categoryEditor, line and line.kind ~= "row");
		SetShown(self.noSelection, not line);

		if ( line and line.kind == "row" ) then
			self.rowEditor:Fill(line, force);
		elseif ( line ) then
			self.categoryEditor:Fill(line);
		end
	end;

	view.Sets = function(self)
		local list = {};

		for _, category in pairs(self.placed) do
			if ( category.set ) then
				table.insert(list, category.set);
			end
		end

		return list;
	end;

	view.HasChanges = function(self)
		for _, set in ipairs(self:Sets()) do
			if ( L.IsDirty(set) ) then
				return true;
			end
		end

		return false;
	end;

	view.Save = function(self)
		local dirty = {};

		for _, set in ipairs(self:Sets()) do
			if ( L.IsDirty(set) ) then
				table.insert(dirty, set);
			end
		end

		return L.SaveAll(dirty) > 0;
	end;

	view.Revert = function(self)
		for _, set in ipairs(self:Sets()) do
			if ( L.IsDirty(set) ) then
				L.Revert(set);
			end
		end
	end;

	view.Reload = function(self)
		local function Run()
			for _, set in ipairs(self:Sets()) do
				L.Reload(set);
			end
		end

		if ( self:HasChanges() ) then
			NoggitMessageBox_Show("Reload loot", "Reloading throws away the unsaved loot changes shown here.", {
				{ text = "Reload", onClick = Run },
				{ text = "Cancel" },
			});
		else
			Run();
		end
	end;

	view.ToggleCollapsed = function(self, id)
		self.collapsed[id] = not self.collapsed[id] or nil;
		self:Rebuild();
	end;

	view.SelectKey = function(self, key)
		for index, line in ipairs(self.lines) do
			if ( LineKey(line) == key ) then
				self.list.selected = index;
				self.selectedKey = key;
				self.selectedLine = line;

				if ( index <= self.list.offset or index > self.list.offset + self.list.visibleCount ) then
					self.list.offset = max(0, index - math.floor(self.list.visibleCount / 2));
				end

				NoggitListBox_Refresh(self.list);
				self:FillEditor(true);
				return true;
			end
		end

		return false;
	end;

	view.SelectRow = function(self, row)
		if ( row ) then
			self:Rebuild();
			self:SelectKey("row:"..tostring(row._uid));
		end
	end;

	view.SelectCategory = function(self, id)
		self.collapsed[id] = nil;
		self:Rebuild();
		self:SelectKey("cat:"..id);
	end;

	view.AddToSelected = function(self, values)
		local category = self.selectedLine and self.selectedLine.category;
		local set = category and category.set;

		if ( not set or not set.loaded ) then
			GameEditor_Notify("Select a loaded loot table or reference first", "info");
			return;
		end

		for _, row in ipairs(set.rows) do
			if ( Num(row.Item) == Num(values.Item) ) then
				GameEditor_Notify(format("%d is already a row of this loot id", Num(values.Item)), "info");
				self:SelectKey("row:"..tostring(row._uid));
				return;
			end
		end

		self.collapsed[category.id] = nil;
		self:SelectRow(L.AddRow(set, values));
	end;

	view.DeleteSelected = function(self)
		local line = self.selectedLine;

		if ( not line or line.kind ~= "row" ) then
			return false;
		end

		local index = self.list.selected or 1;
		L.RemoveRow(line.set, line.row);
		self:Rebuild();

		local nextLine = self.lines[min(index, #self.lines)];

		if ( nextLine ) then
			self:SelectKey(LineKey(nextLine));
		end

		return true;
	end;

	view.UpdateStatus = function(self)
		local tables, rows, dirty = 0, 0, 0;

		for _, set in ipairs(self:Sets()) do
			tables = tables + 1;
			rows = rows + #set.rows;

			if ( L.IsDirty(set) ) then
				dirty = dirty + 1;
			end
		end

		if ( dirty > 0 ) then
			self.status:SetText(format("|cffffcc00%d of %d loot id%s unsaved|r", dirty, tables, tables == 1 and "" or "s"));
		else
			self.status:SetText(format("%d loot id%s, %d row%s", tables, tables == 1 and "" or "s", rows, rows == 1 and "" or "s"));
		end

		Common().SetEnabled(self.saveAll, dirty > 0);
		Common().SetEnabled(self.revertAll, dirty > 0);

		if ( options.onDirty ) then
			options.onDirty(self);
		end
	end;

	view.Rebuild = function(self)
		self.pendingRebuild = nil;
		self.lines, self.placed = BuildLines(self);
		NoggitListBox_SetItems(self.list, self.lines, true);

		local found = false;

		if ( self.selectedKey ) then
			for index, line in ipairs(self.lines) do
				if ( LineKey(line) == self.selectedKey ) then
					self.list.selected = index;
					self.selectedLine = line;
					found = true;
					break;
				end
			end
		end

		if ( not found ) then
			self.list.selected = nil;
			self.selectedLine = nil;
			self.selectedKey = nil;
		end

		NoggitListBox_Refresh(self.list);
		self:FillEditor(false);
		self:UpdateStatus();
	end;

	-- view:SetRoots(roots, title) roots: { { table, key, label, missingText, onCreate }, ... }
	view.SetRoots = function(self, roots, title)
		local same = #roots == #self.roots;

		for index, root in ipairs(roots) do
			local old = self.roots[index];

			if ( not old or old.table ~= root.table or Num(old.key) ~= Num(root.key) ) then
				same = false;
			end
		end

		self.roots = roots;
		self.titleText = title;
		self.title:SetText(title or "");

		if ( not same ) then
			self.selectedKey = nil;
			self.selectedLine = nil;
			self.list.offset = 0;
		end

		-- Hidden tabs only ask the server once they are shown.
		if ( self:IsVisible() ) then
			self:Rebuild();
		else
			self.pendingRebuild = true;
		end
	end;

	L.On(function()
		view.pendingRebuild = true;
	end);

	local elapsedSinceNames = 0;

	view:SetScript("OnUpdate", function(self, elapsed)
		if ( self.pendingRebuild ) then
			self:Rebuild();
		end

		-- Rows typed by hand wait for the item cache, redraw them now and then.
		elapsedSinceNames = elapsedSinceNames + elapsed;

		if ( elapsedSinceNames >= 1 ) then
			elapsedSinceNames = 0;
			NoggitListBox_Refresh(self.list);
			self:FillEditor(false);
		end
	end);

	view:SetScript("OnShow", function(self) self:Rebuild(); end);
	view:FillEditor(false);
	return view;
end

-- Reference use window ------------------------------------------------------------------------------------------

function L.ShowReferenceUse(referenceId)
	referenceId = Num(referenceId);

	if ( referenceId <= 0 ) then
		return;
	end

	GameEditor_Notify(format("Looking up who rolls reference %d...", referenceId), "info");

	local sent = L.FindReferenceUse(referenceId, function(hits, truncated)
		if ( #hits == 0 ) then
			GameEditor_Notify(format("No loot row rolls reference %d", referenceId), "info");
			return;
		end

		local items = {};

		for index, hit in ipairs(hits) do
			local name = hit.name ~= "" and hit.name or (L.SourceName(hit.info.source, hit.entry) or "");
			table.insert(items, { value = index, text = format("%s %d  %s  |cff7f7f7f%s|r", hit.info.title, hit.entry, name, hit.text) });
		end

		GameEditor_PickValue(format("Reference %d is rolled by%s", referenceId, truncated and " (first ones)" or ""), items, nil, function(value)
			local hit = hits[Num(value)];

			if ( hit ) then
				L.OpenRoots({ { table = hit.table, key = hit.entry, label = hit.info.title } }, format("%s %d %s", hit.info.title, hit.entry, hit.name or ""));
			end
		end);
	end);

	if ( not sent ) then
		GameEditor_Notify("Reference lookups need TemplateSearch_Query in this client build", "error");
	end
end
