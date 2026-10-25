-- Building blocks of the spell editor: buttons, text areas, the icon browser, DBC row editors and world table editors.

local SE = GameEditorSpell;
local Data = GameEditorData or {};

local ICON_SIZE = 40;
local ICON_COLUMNS = 9;
local ICON_ROWS = 7;

-- Basic widgets ---------------------------------------------------------------------------------------------

function SE.Box(parent, prefix, background, border)
	local frame = CreateFrame("Frame", NoggitUI_NewName(prefix or "GameEditorSpellBox"), parent, "NoggitBoxTemplate");
	NoggitBox_Set(frame, background, border);
	return frame;
end

function SE.Label(parent, text, font, color, justify)
	local label = NoggitUI_FontString(parent, text or "", font, justify);

	if ( color ) then
		NoggitText_SetColor(label, color);
	end

	return label;
end

function SE.Tooltip(frame, text)
	frame.seTooltip = text;
	frame:HookScript("OnEnter", function(self)
		local tip = self.seTooltip;

		if ( type(tip) == "function" ) then
			tip = tip(self);
		end

		NoggitTooltip_Show(self, tip);
	end);
	frame:HookScript("OnLeave", NoggitTooltip_Hide);
end

function SE.Button(parent, text, width, onClick, tooltip, primary)
	local button = NoggitPushButton_Create(parent, text, width, onClick);

	if ( tooltip ) then
		SE.Tooltip(button, tooltip);
	end

	if ( primary ) then
		local line = NoggitUI_SolidTexture(button, "OVERLAY", NOGGIT_COLOR_ACCENT);
		line:SetHeight(2);
		line:SetPoint("BOTTOMLEFT", button, "BOTTOMLEFT", 1, 1);
		line:SetPoint("BOTTOMRIGHT", button, "BOTTOMRIGHT", -1, 1);
		button.accentLine = line;
	end

	return button;
end

function SE.SetEnabled(button, enabled)
	if ( enabled ) then
		button:Enable();
	else
		button:Disable();
	end
end

local function GlyphRefresh(self)
	if ( self.checked ) then
		NoggitBox_Set(self, NOGGIT_COLOR_ACCENT, nil);
	elseif ( self.hover ) then
		NoggitBox_Set(self, NOGGIT_COLOR_HOVER, nil);
	else
		NoggitBox_Set(self, self.background or NOGGIT_COLOR_MID, nil);
	end
end

-- Square button with a Font Awesome glyph.
function SE.GlyphButton(parent, glyph, tooltip, onClick, size)
	local button = CreateFrame("Button", NoggitUI_NewName("GameEditorSpellGlyph"), parent, "NoggitBoxTemplate");
	button:SetWidth(size or 22);
	button:SetHeight(size or 22);
	button.text = NoggitUI_FontString(button, "", "NoggitAwesomeFont", "CENTER");
	button.text:SetPoint("CENTER", button, "CENTER", 0, 0);
	button.icon = button:CreateTexture(nil, "ARTWORK");
	button.icon:SetWidth(floor((size or 22) * 0.55 + 0.5));
	button.icon:SetHeight(floor((size or 22) * 0.55 + 0.5));
	button.icon:SetPoint("CENTER", button, "CENTER", 0, 0);
	button.icon:SetVertexColor(NOGGIT_COLOR_TEXT[1], NOGGIT_COLOR_TEXT[2], NOGGIT_COLOR_TEXT[3]);
	button:SetScript("OnEnter", function(self)
		self.hover = true;
		GlyphRefresh(self);
		local tip = self.tooltip;

		if ( type(tip) == "function" ) then
			tip = tip(self);
		end

		NoggitTooltip_Show(self, tip);
	end);
	button:SetScript("OnLeave", function(self)
		self.hover = false;
		GlyphRefresh(self);
		NoggitTooltip_Hide();
	end);
	button:SetScript("OnClick", onClick);
	button.tooltip = tooltip;
	button.Refresh = GlyphRefresh;
	button.SetGlyph = function(self, codepoint)
		local texture = NoggitUI_GlyphTexture(codepoint);

		if ( texture ) then
			self.text:Hide();
			self.icon:SetTexture(texture);
			self.icon:Show();
		else
			self.icon:Hide();
			self.text:SetText(NoggitUI_Utf8(codepoint));
			self.text:Show();
		end
	end;
	button:SetGlyph(glyph);
	button.SetChecked = function(self, checked) self.checked = checked and true or false; GlyphRefresh(self); end;
	GlyphRefresh(button);
	return button;
end

-- Line edit with a grey hint while empty.
function SE.SearchBox(parent, width, hint, onChange)
	local edit = NoggitLineEdit_Create(parent, width or 200);
	edit:SetHeight(22);
	edit:SetAutoFocus(false);
	local hintText = SE.Label(edit, hint or "", nil, NOGGIT_COLOR_DISABLED);
	hintText:SetPoint("LEFT", edit, "LEFT", 8, 0);
	edit.hint = hintText;
	edit:SetScript("OnEditFocusGained", function() hintText:Hide(); end);
	edit:SetScript("OnEditFocusLost", function(self)
		if ( self:GetText() == "" ) then
			hintText:Show();
		end
	end);
	edit:SetScript("OnEscapePressed", function(self)
		self:SetText("");
		self:ClearFocus();
	end);
	edit:SetScript("OnTextChanged", function(self)
		local text = self:GetText();

		if ( text ~= "" ) then
			hintText:Hide();
		elseif ( not self:HasFocus() ) then
			hintText:Show();
		end

		-- The client runs OnTextChanged on the next update, so a quiet text is recognised by its content.
		if ( self.quietText ~= nil and self.quietText == text ) then
			self.quietText = nil;
			return;
		end

		self.quietText = nil;

		if ( onChange ) then
			onChange(text);
		end
	end);
	edit.SetQuietText = function(self, text)
		text = text or "";

		if ( self:GetText() ~= text ) then
			self.quietText = text;
			self:SetText(text);
		end
	end;
	return edit;
end

-- Multi line text area with its own scrolling.
function SE.CreateTextArea(parent, onChange)
	local box = SE.Box(parent, "GameEditorSpellTextArea", NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID);
	box:EnableMouse(true);

	local scroll = CreateFrame("ScrollFrame", NoggitUI_NewName("GameEditorSpellTextScroll"), box);
	scroll:SetPoint("TOPLEFT", box, "TOPLEFT", 6, -5);
	scroll:SetPoint("BOTTOMRIGHT", box, "BOTTOMRIGHT", -6, 5);
	scroll:EnableMouseWheel(true);

	local edit = CreateFrame("EditBox", NoggitUI_NewName("GameEditorSpellTextEdit"), scroll);
	edit:SetMultiLine(true);
	edit:SetAutoFocus(false);
	edit:SetFontObject(NoggitFont);
	edit:SetTextColor(NOGGIT_COLOR_TEXT[1], NOGGIT_COLOR_TEXT[2], NOGGIT_COLOR_TEXT[3]);
	edit:SetWidth(200);
	edit:SetHeight(20);
	edit:SetMaxLetters(0);
	scroll:SetScrollChild(edit);

	local function Clamp(value)
		local range = scroll:GetVerticalScrollRange() or 0;
		scroll:SetVerticalScroll(max(0, min(range, value)));
	end

	scroll:SetScript("OnSizeChanged", function(self, width)
		edit:SetWidth(max(20, width or self:GetWidth()));
	end);
	scroll:SetScript("OnMouseWheel", function(self, delta)
		Clamp((self:GetVerticalScroll() or 0) - delta * 30);
	end);
	edit:SetScript("OnCursorChanged", function(_, _, y, _, height)
		local offset = scroll:GetVerticalScroll() or 0;
		local visible = scroll:GetHeight() or 0;
		y = -(y or 0);

		if ( y < offset ) then
			Clamp(y);
		elseif ( y + (height or 0) > offset + visible ) then
			Clamp(y + (height or 0) - visible);
		end
	end);
	-- OnTextChanged runs a frame after SetText, only typing, pasting and Insert count as edits.
	edit:SetScript("OnTextChanged", function(self, userInput)
		scroll:UpdateScrollChildRect();

		if ( not userInput ) then
			return;
		end

		local text = self:GetText();

		if ( text == box.shownText ) then
			return;
		end

		box.shownText = text;

		if ( box.onChange ) then
			box.onChange(text);
		end
	end);
	edit:SetScript("OnEscapePressed", function(self) self:ClearFocus(); end);
	edit:SetScript("OnEditFocusGained", function() NoggitBox_Set(box, NOGGIT_COLOR_DARK, NOGGIT_COLOR_ACCENT); end);
	edit:SetScript("OnEditFocusLost", function() NoggitBox_Set(box, NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID); end);
	box:SetScript("OnMouseDown", function() edit:SetFocus(); end);

	box.edit = edit;
	box.scroll = scroll;
	box.onChange = onChange;
	box.SetValue = function(self, text)
		text = text or "";
		self.shownText = text;

		if ( edit:GetText() == text ) then
			return;
		end

		edit:SetText(text);
	end;
	return box;
end

-- Small heading with an optional one line description below it, returns the bottom offset.
function SE.SectionTitle(parent, title, description, y)
	y = y or 0;
	local heading = SE.Label(parent, title, "NoggitFont16", NOGGIT_COLOR_WHITE);
	heading:SetPoint("TOPLEFT", parent, "TOPLEFT", 2, y);
	y = y - 22;

	local text;

	if ( description ) then
		text = SE.Label(parent, description, nil, NOGGIT_COLOR_DISABLED);
		text:SetPoint("TOPLEFT", parent, "TOPLEFT", 2, y);
		text:SetPoint("RIGHT", parent, "RIGHT", -4, 0);
		text:SetJustifyV("TOP");
		y = y - max(14, text:GetHeight()) - 6;
	end

	return y, heading, text;
end

-- Groups that cover every column of a DBC, columns missing from the given groups land in "Other".
function SE.CompleteGroups(tableName, groups)
	local used = {};
	local result = {};

	for _, group in ipairs(groups or {}) do
		table.insert(result, group);

		for _, column in ipairs(group.columns) do
			used[column] = true;
		end
	end

	local rest = {};

	for _, column in ipairs(SE.Schema(tableName).columns) do
		if ( not used[column] ) then
			table.insert(rest, column);
		end
	end

	if ( #rest > 0 ) then
		table.insert(result, { title = #result > 0 and "Other" or nil, columns = rest });
	end

	return result;
end

-- Icon browser ---------------------------------------------------------------------------------------------

local iconWindow;

local function IconPage()
	local window = iconWindow;
	local perPage = ICON_COLUMNS * ICON_ROWS;
	local pages = max(1, ceil(#window.items / perPage));
	window.page = max(1, min(pages, window.page));

	for index, button in ipairs(window.buttons) do
		local item = window.items[(window.page - 1) * perPage + index];

		if ( item ) then
			button.item = item;
			button.image:SetTexture(item.name);
			button.selected = item.id == window.current;
			NoggitTextureButton_Refresh(button);
			button:Show();
		else
			button.item = nil;
			button:Hide();
		end
	end

	window.pageText:SetText(format("Page %d of %d, %d icons", window.page, pages, #window.items));
end

local function IconSearch()
	iconWindow.items = {};

	for _, item in ipairs(GameEditor_FilterList(GameEditor_DbcList("SpellIcon"), iconWindow.search:GetText())) do
		table.insert(iconWindow.items, { id = item.value, name = item.text });
	end
	iconWindow.page = 1;

	for index, item in ipairs(iconWindow.items) do
		if ( item.id == iconWindow.current ) then
			iconWindow.page = ceil(index / (ICON_COLUMNS * ICON_ROWS));
			break;
		end
	end

	IconPage();
end

local function CreateIconWindow()
	local width = ICON_COLUMNS * (ICON_SIZE + 4) + 16;
	iconWindow = GameEditor_CreateWindow("GameEditorSpellIconWindow", "Spell icons", width, ICON_ROWS * (ICON_SIZE + 4) + 112);
	local content = iconWindow.content;

	iconWindow.search = SE.SearchBox(content, 200, "Search icon file name or id", function()
		iconWindow.pending = 0.15;
	end);
	iconWindow.search:SetPoint("TOPLEFT", content, "TOPLEFT", 0, 0);
	iconWindow.search:SetPoint("TOPRIGHT", content, "TOPRIGHT", 0, 0);
	iconWindow.search:SetScript("OnEnterPressed", function(self)
		self:ClearFocus();
		IconSearch();
	end);

	iconWindow.buttons = {};

	for row = 0, ICON_ROWS - 1 do
		for column = 0, ICON_COLUMNS - 1 do
			local button = NoggitTextureButton_Create(content, ICON_SIZE);
			button:SetPoint("TOPLEFT", content, "TOPLEFT", column * (ICON_SIZE + 4), -30 - row * (ICON_SIZE + 4));
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
				if ( self.item and iconWindow.onPick ) then
					iconWindow.onPick(self.item.id, self.item.name);
				end

				GameEditor_HideWindow(iconWindow);
			end);
			table.insert(iconWindow.buttons, button);
		end
	end

	local bottom = -30 - ICON_ROWS * (ICON_SIZE + 4) - 6;
	local previous = SE.GlyphButton(content, 0xF053, "Previous page", function()
		iconWindow.page = iconWindow.page - 1;
		IconPage();
	end);
	previous:SetPoint("TOPLEFT", content, "TOPLEFT", 0, bottom);
	local nextPage = SE.GlyphButton(content, 0xF054, "Next page", function()
		iconWindow.page = iconWindow.page + 1;
		IconPage();
	end);
	nextPage:SetPoint("LEFT", previous, "RIGHT", 4, 0);
	iconWindow.pageText = SE.Label(content, "", nil, NOGGIT_COLOR_DISABLED);
	iconWindow.pageText:SetPoint("LEFT", nextPage, "RIGHT", 8, 0);

	content:EnableMouseWheel(true);
	content:SetScript("OnMouseWheel", function(_, delta)
		iconWindow.page = iconWindow.page - delta;
		IconPage();
	end);

	iconWindow:SetScript("OnUpdate", function(self, elapsed)
		if ( self.pending ) then
			self.pending = self.pending - elapsed;

			if ( self.pending <= 0 ) then
				self.pending = nil;
				IconSearch();
			end
		end
	end);
end

function SE.ShowIconBrowser(title, current, onPick)
	if ( not iconWindow ) then
		CreateIconWindow();
	end

	iconWindow:SetTitle(title or "Spell icons");
	iconWindow.current = tonumber(current);
	iconWindow.onPick = onPick;
	iconWindow.search:SetQuietText("");
	iconWindow.search.hint:Show();
	GameEditor_ShowWindow(iconWindow);
	IconSearch();
end

-- DBC row editor -------------------------------------------------------------------------------------------

local DbcRowEditor = {};
DbcRowEditor.__index = DbcRowEditor;

function DbcRowEditor:IsDirty()
	if ( self.isNew ) then
		return true;
	end

	if ( not self.exists ) then
		return false;
	end

	return SE.DiffCount(self.values, self.original, SE.Schema(self.tableName).columns) > 0;
end

function DbcRowEditor:ChangeCount()
	return SE.DiffCount(self.values, self.original, SE.Schema(self.tableName).columns);
end

function DbcRowEditor:DirtyText()
	if ( self.isNew ) then
		return format("%s %d is new and not saved", self.tableName, self.id);
	end

	return format("%s %d has %d unsaved field%s", self.tableName, self.id, self:ChangeCount(), self:ChangeCount() == 1 and "" or "s");
end

function DbcRowEditor:Bind()
	self.grid.values = self.values;
	self.grid.original = self.original;
	self.grid:Refresh();
	self:UpdateHeader();
end

function DbcRowEditor:UpdateHeader()
	local title = self.options.title or self.tableName;
	local status;

	if ( self.id == 0 or (not self.exists and not self.isNew) ) then
		self.titleText:SetText(title);
		status = self.id == 0 and "|cff8f8f8fnot set|r" or format("|cffff6060%d does not exist|r", self.id);
		self.grid.frame:Hide();
		self.empty:Show();
		self.emptyText:SetText(self.id == 0 and (self.options.emptyText or "Nothing linked yet.") or self.options.missingText or format("There is no %s row %d in this client.", self.tableName, self.id));
	else
		self.titleText:SetText(format("%s %d", title, self.id));

		if ( not self.options.hideGrid ) then
			self.grid.frame:Show();
		end

		self.empty:Hide();

		if ( self.pending ) then
			status = "|cffffcc00saving...|r";
		elseif ( self.isNew ) then
			status = "|cffffcc00new, not saved|r";
		elseif ( self:IsDirty() ) then
			status = format("|cffffcc00%d unsaved|r", self:ChangeCount());
		elseif ( self.appliedLocal ) then
			status = "|cff80c0ffapplied locally|r";
		else
			status = "|cff60d060saved|r";
		end
	end

	self.statusText:SetText(status);
	local editable = self.exists or self.isNew;
	SE.SetEnabled(self.saveButton, editable);
	SE.SetEnabled(self.revertButton, editable);
	SE.SetEnabled(self.applyButton, editable);
	SE.SetEnabled(self.copyButton, editable);

	if ( self.options.onStatus ) then
		self.options.onStatus(self);
	end
end

function DbcRowEditor:LoadNow(id)
	id = tonumber(id) or 0;
	self.id = id;
	self.isNew = false;
	self.appliedLocal = false;
	self.pending = false;
	local row = id > 0 and SE.GetRow(self.tableName, id) or nil;
	self.exists = row ~= nil;
	self.values = row or {};
	self.original = SE.Copy(self.values);
	self.idEdit:SetText(id > 0 and tostring(id) or "");
	self:Bind();
end

-- Loads a row, asks first when the current one has unsaved changes.
function DbcRowEditor:Load(id, force)
	id = tonumber(id) or 0;

	if ( id == self.id and not force and (self.exists or self.isNew) ) then
		self:UpdateHeader();
		return;
	end

	if ( force or not self:IsDirty() ) then
		self:LoadNow(id);
		return;
	end

	NoggitMessageBox_Show("Unsaved changes", self:DirtyText()..".", {
		{ text = "Save", onClick = function() self:Save(); self:LoadNow(id); end },
		{ text = "Discard", onClick = function() self:Discard(); self:LoadNow(id); end },
		{ text = "Cancel", onClick = function() self.idEdit:SetText(tostring(self.id)); end },
	});
end

function DbcRowEditor:Save()
	if ( not (self.exists or self.isNew) or not DbcEditor_Save ) then
		return;
	end

	local row = SE.FullRow(self.tableName, self.values);
	row.m_id = self.id;
	self.saving = SE.Copy(row);
	self.pending = true;
	local ok, err = pcall(DbcEditor_Save, self.tableName, self.id, row);

	if ( not ok ) then
		self.pending = false;
		GameEditor_Notify("Saving failed: "..tostring(err), "error");
	end

	self:UpdateHeader();
end

function DbcRowEditor:ApplyLocal(quiet)
	if ( not (self.exists or self.isNew) or not DbcEditor_ApplyLocal ) then
		return;
	end

	local row = SE.FullRow(self.tableName, self.values);
	row.m_id = self.id;

	if ( pcall(DbcEditor_ApplyLocal, self.tableName, self.id, row) ) then
		self.appliedLocal = true;

		if ( not quiet ) then
			GameEditor_Notify(format("%s %d applied to this client only.", self.tableName, self.id), "info");
		end
	end

	self:UpdateHeader();
end

function DbcRowEditor:DiscardLocal()
	if ( self.appliedLocal ) then
		SE.RevertLocalRow(self.tableName, self.id, self.exists and self.original);
	end

	self.appliedLocal = false;
end

function DbcRowEditor:Discard()
	self:DiscardLocal();

	if ( self.isNew ) then
		self:LoadNow(0);
	else
		self:LoadNow(self.id);
	end
end

function DbcRowEditor:Revert()
	if ( self.isNew ) then
		self.values = SE.Copy(self.original);
		self:Bind();
		return;
	end

	self:Discard();
end

-- New row with the next free id, or options.rowId for tables keyed by another row, a copy of the open row when copy is set.
function DbcRowEditor:NewRow(copy)
	local id = self.options.rowId and tonumber(self.options.rowId(self)) or SE.NextId(self.tableName);
	local values;

	if ( not id or id <= 0 ) then
		return;
	end

	if ( copy and (self.exists or self.isNew) ) then
		values = SE.FullRow(self.tableName, self.values);
	else
		values = SE.EmptyRow(self.tableName, id);
	end

	values.m_id = id;
	self:DiscardLocal();
	self.id = id;
	self.values = values;
	self.original = SE.Copy(values);
	self.exists = false;
	self.isNew = true;
	self.idEdit:SetText(tostring(id));
	self:Bind();

	if ( self.options.onCreated ) then
		self.options.onCreated(self, id);
	end
end

function SE.CreateDbcEditor(parent, tableName, options)
	options = options or {};
	local editor = setmetatable({ tableName = tableName, options = options, id = 0, values = {}, original = {} }, DbcRowEditor);
	local frame = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellDbcEditor"), parent);
	editor.frame = frame;

	editor.titleText = SE.Label(frame, tableName, "NoggitFontBold", NOGGIT_COLOR_WHITE);
	editor.titleText:SetPoint("TOPLEFT", frame, "TOPLEFT", 2, -4);
	editor.statusText = SE.Label(frame, "");
	editor.statusText:SetPoint("LEFT", editor.titleText, "RIGHT", 10, 0);

	editor.idEdit = NoggitLineEdit_Create(frame, 70);
	editor.idEdit:SetHeight(22);
	editor.idEdit:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, -24);
	editor.idEdit:SetScript("OnEnterPressed", function(self)
		self:ClearFocus();
		local id = tonumber(self:GetText());

		if ( id ) then
			editor:Load(id);

			if ( options.onIdEntered ) then
				options.onIdEntered(editor, id);
			end
		end
	end);
	SE.Tooltip(editor.idEdit, "Type a row id and press Enter to open it");

	local pick = SE.GlyphButton(frame, 0xF002, "Search rows", function()
		GameEditor_PickValue(tableName, function()
			local items = GameEditor_RefList("dbc:"..tableName);
			local named = {};

			for index, item in ipairs(items) do
				named[index] = item.text ~= "" and item or { value = item.value, text = GameEditor_ResolveRef("dbc:"..tableName, item.value) or "" };
			end

			return named;
		end, editor.id, function(value)
			editor:Load(value);

			if ( options.onIdEntered ) then
				options.onIdEntered(editor, value);
			end
		end);
	end);
	pick:SetPoint("LEFT", editor.idEdit, "RIGHT", 3, 0);

	editor.newButton = SE.Button(frame, "New", 46, function() editor:NewRow(false); end, "Create an empty row with the next free id");
	editor.newButton:SetPoint("LEFT", pick, "RIGHT", 8, 0);
	editor.copyButton = SE.Button(frame, "Copy", 50, function() editor:NewRow(true); end, "Copy this row to the next free id");
	editor.copyButton:SetPoint("LEFT", editor.newButton, "RIGHT", 4, 0);
	editor.saveButton = SE.Button(frame, "Save", 54, function() editor:Save(); end, "Write this row to the dbc database, the server pushes it to every client", true);
	editor.saveButton:SetPoint("TOPRIGHT", frame, "TOPRIGHT", 0, -24);
	editor.revertButton = SE.Button(frame, "Revert", 58, function() editor:Revert(); end, "Throw away the changes of this row");
	editor.revertButton:SetPoint("RIGHT", editor.saveButton, "LEFT", -4, 0);
	editor.applyButton = SE.Button(frame, "Apply", 54, function() editor:ApplyLocal(); end, "Patch this client only, to preview without saving");
	editor.applyButton:SetPoint("RIGHT", editor.revertButton, "LEFT", -4, 0);

	if ( options.rowId ) then
		editor.idEdit:Hide();
		pick:Hide();
		editor.newButton:Hide();
		editor.copyButton:Hide();
	end

	editor.grid = GameEditor_CreatePropertyGrid(frame, {
		table = tableName,
		groups = SE.CompleteGroups(tableName, options.groups),
		readOnly = { m_id = true },
		filter = options.filter ~= false,
		labelWidth = options.labelWidth or 180,
		onChange = function()
			editor:UpdateHeader();
		end,
		onRowContext = options.onRowContext and function(_, column)
			options.onRowContext(editor, column);
		end or nil,
	});
	editor.grid.frame:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, -52);
	editor.grid.frame:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", 0, 0);

	if ( options.hideGrid ) then
		editor.grid.frame:Hide();
	end

	editor.empty =CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellDbcEmpty"), frame);
	editor.empty:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, -56);
	editor.empty:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", 0, 0);
	editor.emptyText = SE.Label(editor.empty, "", nil, NOGGIT_COLOR_DISABLED);
	editor.emptyText:SetPoint("TOPLEFT", editor.empty, "TOPLEFT", 4, -6);
	editor.emptyText:SetPoint("RIGHT", editor.empty, "RIGHT", -4, 0);
	local create = SE.Button(editor.empty, "Create a new "..tableName, nil, function() editor:NewRow(false); end);
	create:SetPoint("TOPLEFT", editor.empty, "TOPLEFT", 4, -30);

	SE.On("DBC_EDITOR_RESULT", function(ok, resultTable, id)
		if ( SE.Lower(resultTable) ~= SE.Lower(tableName) or tonumber(id) ~= editor.id ) then
			return;
		end

		editor.pending = false;

		if ( SE.IsOk(ok) ) then
			editor.original = editor.saving or SE.Copy(editor.values);
			editor.isNew = false;
			editor.exists = true;
			editor.appliedLocal = false;
			SE.reservedIds[tableName] = nil;
			editor:Bind();

			if ( options.onSaved ) then
				options.onSaved(editor);
			end
		else
			editor:UpdateHeader();
		end
	end);

	SE.On("DBC_RECORD_UPDATED", function(resultTable, id)
		if ( SE.Lower(resultTable) ~= SE.Lower(tableName) or tonumber(id) ~= editor.id or editor.pending ) then
			return;
		end

		editor.appliedLocal = false;

		if ( not editor:IsDirty() ) then
			editor:LoadNow(editor.id);
		end
	end);

	SE.RegisterEditor(editor);
	editor:UpdateHeader();
	return editor;
end

-- World table editor ---------------------------------------------------------------------------------------

local WorldTableEditor = {};
WorldTableEditor.__index = WorldTableEditor;

local function RowsEqual(a, b, columns)
	for _, column in ipairs(columns) do
		if ( not SE.SameValue(a[column], b[column]) ) then
			return false;
		end
	end

	return true;
end

function WorldTableEditor:ColumnNames()
	if ( self.columns ) then
		return self.columns;
	end

	return Data.Columns and Data.Columns[self.tableName] or {};
end

function WorldTableEditor:RowsDirty(rows, snapshot)
	if ( #rows ~= #snapshot ) then
		return true;
	end

	local columns = self:ColumnNames();

	for index, row in ipairs(rows) do
		if ( not RowsEqual(row, snapshot[index], columns) ) then
			return true;
		end
	end

	return false;
end

function WorldTableEditor:IsDirty()
	return self.loaded and self:RowsDirty(self.rows, self.snapshot) or false;
end

-- The rows of one key, kept by a spell tab that is not active.
function WorldTableEditor:CaptureState()
	return { key = self.key, keyColumn = self.keyColumn, rows = self.rows, snapshot = self.snapshot, selected = self.selected };
end

function WorldTableEditor:RestoreState(state)
	self.key = state.key;
	self.keyColumn = state.keyColumn or self.keyColumn;
	self.rows = state.rows;
	self.snapshot = state.snapshot;
	self.loaded = true;
	self.loading = false;
	self.saving = false;
	self.error = nil;
	self:Select(state.selected);
end

function WorldTableEditor:IsStateDirty(state)
	return self:RowsDirty(state.rows, state.snapshot);
end

function WorldTableEditor:StateDirtyText(state)
	return format("%s of %s has unsaved rows", self.tableName, tostring(state.key));
end

function WorldTableEditor:DirtyText()
	return format("%s of %s has unsaved rows", self.tableName, tostring(self.key));
end

function WorldTableEditor:Discard()
	self:ApplyRows(self.snapshot);
end

function WorldTableEditor:ApplyRows(rows)
	self.rows = {};

	for _, row in ipairs(rows or {}) do
		table.insert(self.rows, SE.Copy(row));
	end

	self:Select(min(max(self.selected or 1, 1), #self.rows));
end

function WorldTableEditor:BuildGrid(columns)
	if ( self.grid ) then
		return;
	end

	local fields = {};
	local names = {};
	local dataFields = Data.Fields and Data.Fields[self.tableName] or {};
	local optionFields = self.options.fields or {};

	for _, column in ipairs(columns) do
		local name = column.name or column[1] or column;
		local kind = SE.NormalizeType(column.type or column[2] or (dataFields[name] and dataFields[name].type));
		local field = SE.Copy(optionFields[name] or dataFields[name] or { label = name });
		field.type = kind;
		fields[name] = field;
		table.insert(names, name);
	end

	self.columns = names;
	self.types = {};

	for name, field in pairs(fields) do
		self.types[name] = field.type;
	end

	local readOnly = {};

	if ( self.keyColumn ) then
		readOnly[self.keyColumn] = true;
	end

	self.grid = GameEditor_CreatePropertyGrid(self.gridHolder, {
		table = self.tableName,
		columns = names,
		fields = fields,
		readOnly = readOnly,
		labelWidth = self.options.labelWidth or 190,
		onChange = function()
			self:RefreshList();
			self:UpdateStatus();
		end,
	});
	self.grid.frame:SetAllPoints(self.gridHolder);
end

function WorldTableEditor:Summary(row)
	if ( self.options.summary ) then
		local ok, text = pcall(self.options.summary, row, self);

		if ( ok and text ) then
			return text;
		end
	end

	local parts = {};

	for _, column in ipairs(self:ColumnNames()) do
		if ( column ~= self.keyColumn and #parts < 4 ) then
			table.insert(parts, column.."="..tostring(row[column] or ""));
		end
	end

	return table.concat(parts, ", ");
end

function WorldTableEditor:RefreshList()
	local items = {};

	for index, row in ipairs(self.rows) do
		local changed = not self.snapshot[index] or not RowsEqual(row, self.snapshot[index], self:ColumnNames());
		table.insert(items, { index = index, text = self:Summary(row), changed = changed });
	end

	local selected = self.selected;
	NoggitListBox_SetItems(self.list, items, true);
	self.list.selected = (selected and selected > 0 and selected <= #items) and selected or nil;
	NoggitListBox_Refresh(self.list);
end

function WorldTableEditor:Select(index)
	self.selected = (index and index >= 1 and index <= #self.rows) and index or nil;
	self:RefreshList();

	if ( self.grid ) then
		if ( self.selected ) then
			self.grid.values = self.rows[self.selected];
			self.grid.original = self.snapshot[self.selected] or {};
			self.grid.frame:Show();
			self.grid:Refresh();
		else
			self.grid.frame:Hide();
		end
	end

	self:UpdateStatus();

	if ( self.options.onSelect ) then
		self.options.onSelect(self, self.selected and self.rows[self.selected]);
	end
end

function WorldTableEditor:UpdateStatus()
	local text;

	if ( not self.key ) then
		text = (SE.cur and SE.cur.isNew) and "|cff8f8f8fsave the new spell first|r" or "|cff8f8f8fnothing to load|r";
	elseif ( self.error ) then
		text = "|cffff6060"..self.error.."|r";
	elseif ( self.loading ) then
		text = "|cffffcc00loading...|r";
	elseif ( not self.loaded ) then
		text = "|cff8f8f8fnot loaded|r";
	elseif ( self.saving ) then
		text = "|cffffcc00saving...|r";
	elseif ( self:IsDirty() ) then
		text = format("|cffffcc00%d row%s, unsaved|r", #self.rows, #self.rows == 1 and "" or "s");
	else
		text = format("|cff60d060%d row%s saved|r", #self.rows, #self.rows == 1 and "" or "s");
	end

	self.statusText:SetText(text);
	if ( self.loaded and #self.rows == 0 ) then
		self.emptyText:Show();
	else
		self.emptyText:Hide();
	end

	local ready = self.loaded and self.key ~= nil;
	SE.SetEnabled(self.addButton, ready);
	SE.SetEnabled(self.saveButton, ready);
	SE.SetEnabled(self.revertButton, ready);
	SE.SetEnabled(self.duplicateButton, ready and self.selected ~= nil);
	SE.SetEnabled(self.deleteButton, ready and self.selected ~= nil);

	if ( self.options.onStatus ) then
		self.options.onStatus(self);
	end

	SE.QueueRefresh();
end

function WorldTableEditor:SetKey(key)
	self.key = key;
	self.rows = {};
	self.snapshot = {};
	self.loaded = false;
	self.loading = false;
	self.saving = false;
	self.error = nil;
	self.selected = nil;
	self:Select(nil);
end

function WorldTableEditor:Query()
	if ( not self.key ) then
		return;
	end

	if ( not WorldTable_Query ) then
		self.error = "WorldTable_Query is not available in this client build";
		self:UpdateStatus();
		return;
	end

	self.loading = true;
	self.error = nil;
	pcall(WorldTable_Query, self.tableName, self.key);
	self:UpdateStatus();
end

function WorldTableEditor:EnsureLoaded()
	if ( self.key and not self.loaded and not self.loading ) then
		self:Query();
	end
end

function WorldTableEditor:OnRows()
	local ok, data = pcall(WorldTable_GetRows, self.tableName, self.key);
	self.loading = false;

	if ( not ok or type(data) ~= "table" ) then
		self.error = "the server sent no rows, the table may not be editable";
		self:UpdateStatus();
		return;
	end

	if ( data.keyColumn and data.keyColumn ~= "" ) then
		self.keyColumn = data.keyColumn;
	end

	self:BuildGrid((data.columns and #data.columns > 0) and data.columns or self:ColumnNames());

	local rows = {};

	for _, row in ipairs(data.rows or {}) do
		if ( row[1] ~= nil and row[self.columns[1]] == nil ) then
			local map = {};

			for index, name in ipairs(self.columns) do
				map[name] = row[index];
			end

			row = map;
		end

		table.insert(rows, row);
	end

	self.snapshot = {};

	for _, row in ipairs(rows) do
		table.insert(self.snapshot, SE.Copy(row));
	end

	self.loaded = true;
	self.error = nil;
	self:ApplyRows(rows);
	self:Select(#self.rows > 0 and 1 or nil);
end

function WorldTableEditor:NewRowValues()
	local row = {};

	for _, column in ipairs(self:ColumnNames()) do
		row[column] = (self.types and self.types[column] == "string") and "" or 0;
	end

	if ( self.keyColumn ) then
		row[self.keyColumn] = self.key;
	end

	if ( self.options.defaults ) then
		self.options.defaults(row, self.key, self);
	end

	return row;
end

function WorldTableEditor:AddRow(values)
	if ( not self.loaded ) then
		return;
	end

	table.insert(self.rows, values or self:NewRowValues());
	self:Select(#self.rows);
end

function WorldTableEditor:DuplicateRow()
	if ( self.selected ) then
		self:AddRow(SE.Copy(self.rows[self.selected]));
	end
end

function WorldTableEditor:DeleteRow()
	if ( self.selected ) then
		table.remove(self.rows, self.selected);
		self:Select(min(self.selected, #self.rows));
	end
end

function WorldTableEditor:SelectedRow()
	return self.selected and self.rows[self.selected];
end

function WorldTableEditor:SetSelectedValue(column, value)
	local row = self:SelectedRow();

	if ( row ) then
		row[column] = value;
		self.grid:Refresh();
		self:RefreshList();
		self:UpdateStatus();
	end
end

-- Rows as the server expects them, every column filled and the key column set.
function WorldTableEditor:RowsForSave(source, key, keyColumn)
	local rows = {};

	for _, row in ipairs(source) do
		local copy = SE.Copy(row);

		if ( keyColumn ) then
			copy[keyColumn] = key;
		end

		for _, column in ipairs(self:ColumnNames()) do
			if ( copy[column] == nil ) then
				copy[column] = (self.types and self.types[column] == "string") and "" or 0;
			end
		end

		table.insert(rows, copy);
	end

	return rows;
end

function WorldTableEditor:Save()
	if ( not self.loaded or not self.key ) then
		return;
	end

	if ( not WorldTable_Save ) then
		GameEditor_Notify("WorldTable_Save is not available in this client build.", "error");
		return;
	end

	self.saving = true;
	local ok, err = pcall(WorldTable_Save, self.tableName, self.key, self:RowsForSave(self.rows, self.key, self.keyColumn));

	if ( not ok ) then
		self.saving = false;
		GameEditor_Notify("Saving failed: "..tostring(err), "error");
	end

	self:UpdateStatus();
end

-- Saves rows kept by an inactive spell tab, the reply is shown as a toast by the shell.
function WorldTableEditor:SaveState(state)
	if ( not state.key or not WorldTable_Save ) then
		return false;
	end

	local ok, err = pcall(WorldTable_Save, self.tableName, state.key, self:RowsForSave(state.rows, state.key, state.keyColumn or self.keyColumn));

	if ( not ok ) then
		GameEditor_Notify("Saving failed: "..tostring(err), "error");
	end

	return ok;
end

function SE.CreateWorldTableEditor(parent, tableName, options)
	options = options or {};
	local editor = setmetatable({ tableName = tableName, options = options, keyColumn = options.keyColumn, rows = {}, snapshot = {} }, WorldTableEditor);
	local frame = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellWorldEditor"), parent);
	editor.frame = frame;

	local y = 0;

	if ( options.title ) then
		y = SE.SectionTitle(frame, options.title, options.description, 0);
	end

	local tableLabel = SE.Label(frame, tableName, "NoggitFontBold", NOGGIT_COLOR_GOLD);
	tableLabel:SetPoint("TOPLEFT", frame, "TOPLEFT", 2, y - 4);
	editor.statusText = SE.Label(frame, "");
	editor.statusText:SetPoint("LEFT", tableLabel, "RIGHT", 10, 0);

	editor.saveButton = SE.Button(frame, "Save", 54, function() editor:Save(); end, "Replace every row of this spell in "..tableName.." and reload it on the server", true);
	editor.saveButton:SetPoint("TOPRIGHT", frame, "TOPRIGHT", 0, y);
	editor.revertButton = SE.Button(frame, "Revert", 58, function() editor:Discard(); end, "Back to the rows the server sent");
	editor.revertButton:SetPoint("RIGHT", editor.saveButton, "LEFT", -4, 0);
	local reload = SE.GlyphButton(frame, 0xF021, "Ask the server again", function() editor:Query(); end, 23);
	reload:SetPoint("RIGHT", editor.revertButton, "LEFT", -4, 0);
	y = y - 28;

	editor.addButton = SE.Button(frame, "Add row", 70, function() editor:AddRow(); end);
	editor.addButton:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, y);
	editor.duplicateButton = SE.Button(frame, "Duplicate", 74, function() editor:DuplicateRow(); end);
	editor.duplicateButton:SetPoint("LEFT", editor.addButton, "RIGHT", 4, 0);
	editor.deleteButton = SE.Button(frame, "Delete row", 80, function() editor:DeleteRow(); end, "Removed for good when you save");
	editor.deleteButton:SetPoint("LEFT", editor.duplicateButton, "RIGHT", 4, 0);

	local previous = editor.deleteButton;

	for _, extra in ipairs(options.buttons or {}) do
		local button = SE.Button(frame, extra.text, extra.width, function() extra.onClick(editor); end, extra.tooltip);
		button:SetPoint("LEFT", previous, "RIGHT", 10, 0);
		previous = button;
		extra.button = button;
	end

	y = y - 28;

	editor.list = NoggitListBox_Create(frame, 20, function(row)
		row.index = SE.Label(row, "");
		row.index:SetPoint("LEFT", row, "LEFT", 6, 0);
		row.index:SetWidth(24);
		NoggitText_SetColor(row.index, NOGGIT_COLOR_DISABLED);
		row.text = SE.Label(row, "");
		row.text:SetPoint("LEFT", row, "LEFT", 32, 0);
		row.text:SetPoint("RIGHT", row, "RIGHT", -4, 0);
	end, function(row, item)
		row.index:SetText(item.index);
		row.text:SetText(item.text);
		NoggitText_SetColor(row.text, item.changed and NOGGIT_COLOR_GOLD or NOGGIT_COLOR_TEXT);
	end);
	editor.list:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, y);
	editor.list:SetPoint("TOPRIGHT", frame, "TOPRIGHT", 0, y);
	editor.list:SetHeight(options.listHeight or 106);
	editor.list.onSelect = function(_, index) editor:Select(index); end;

	editor.emptyText = SE.Label(editor.list, "No rows. Add row creates one, Save writes it.", nil, NOGGIT_COLOR_DISABLED);
	editor.emptyText:SetPoint("CENTER", editor.list, "CENTER", 0, 0);
	editor.emptyText:Hide();

	editor.gridHolder = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellWorldGrid"), frame);
	editor.gridHolder:SetPoint("TOPLEFT", editor.list, "BOTTOMLEFT", 0, -6);
	editor.gridHolder:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", 0, 0);

	SE.On("WORLD_TABLE_ROWS", function(resultTable, key, ok, message)
		if ( SE.Lower(resultTable) ~= SE.Lower(tableName) or tonumber(key) ~= tonumber(editor.key) ) then
			return;
		end

		if ( ok ~= nil and not SE.IsOk(ok) ) then
			editor.loading = false;
			editor.error = (message and message ~= "") and message or "the server could not read the rows";
			editor:UpdateStatus();
			return;
		end

		editor:OnRows();
	end);

	SE.On("WORLD_TABLE_RESULT", function(ok, resultTable, key, message)
		if ( SE.Lower(resultTable) ~= SE.Lower(tableName) or tonumber(key) ~= tonumber(editor.key) ) then
			return;
		end

		editor.loading = false;
		editor.saving = false;

		if ( SE.IsOk(ok) ) then
			editor:Query();
		elseif ( not editor.loaded ) then
			editor.error = message or "the server refused the request";
		end

		editor:UpdateStatus();
	end);

	frame:SetScript("OnShow", function() editor:EnsureLoaded(); end);
	SE.RegisterEditor(editor);
	editor:SetKey(nil);
	return editor;
end
