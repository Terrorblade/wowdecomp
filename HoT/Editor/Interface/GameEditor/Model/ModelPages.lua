-- Textures, Particles, Ribbons and Attachments pages of the models workspace.

local ME = GameEditorModel;
local Common = GameEditorObject;

local LABEL_WIDTH = ME.LABEL_WIDTH;
local BIG = 1000000;
local LIST_HEIGHT = 112;
local MULTI_TEXTURE_BIT = 28;

local Num = ME.Num;

local function NewPage(container)
	local page = CreateFrame("Frame", NoggitUI_NewName("GameEditorModelPage"), container);
	page:SetAllPoints(container);
	page:Hide();
	return page;
end

local function PageScroll(page, top)
	page.scroll, page.child = GameEditor_CreateScrollFrame(page);
	page.scroll:SetPoint("TOPLEFT", page, "TOPLEFT", 0, -top);
	page.scroll:SetPoint("BOTTOMRIGHT", page, "BOTTOMRIGHT", 0, 0);
	page.child:SetWidth(480);
	page.child:SetHeight(10);
end

local function ChildWidth(page)
	return max(380, (page.scroll:GetWidth() or 480) - 14);
end

local function Doc()
	return ME.current;
end

local function Info()
	return ME.current and ME.Info(ME.current);
end

local function Entry()
	return ME.Current();
end

local function ConfirmRemove(title, text, onConfirm)
	NoggitMessageBox_Show(title, text, {
		{ text = "Remove", onClick = onConfirm },
		{ text = "Cancel" },
	});
end

local function TextureRecord(info, index)
	for _, texture in ipairs(info and info.textures or {}) do
		if ( texture.index == index ) then
			return texture;
		end
	end
end

-- Path shown for a texture, modern models name it by a file data id from TXID.
local function TexturePathText(texture)
	local path = texture.path or "";
	local fdid = Num(texture.fileDataId);

	if ( fdid > 0 ) then
		return format("%s  |cff7f7f7f(fdid %d)|r", path ~= "" and path or "unresolved", fdid);
	end

	if ( path == "" ) then
		return Num(texture.type) ~= 0 and "|cff7f7f7fset by the client|r" or "|cffff6060no path|r";
	end

	return path;
end

-- Textures page --------------------------------------------------------------------------------------------------

local importWindow;

local function ImportRefresh()
	local files = ME.Call("ModelEditor_ListImportFiles");
	local items = {};

	for _, name in ipairs(type(files) == "table" and files or {}) do
		table.insert(items, { name = name });
	end

	NoggitListBox_SetItems(importWindow.list, items);
	importWindow.status:SetText(#items > 0 and format("%d images in the Import folder of the game", #items) or "Put png, jpg, bmp, tga or blp files into <game folder>\\Import\\ and press Refresh");
end

local function SuggestImportPath(name)
	local entry = Entry();
	local folder = entry and ME.Folder(entry.path) or "";
	local base = ME.StripExtension(name or "texture");
	return (folder ~= "" and (folder.."\\") or "")..base..".blp";
end

local function DoImport()
	local list = importWindow.list;
	local item = list.items[list.selected or 0];

	if ( not item ) then
		ME.Notice("Pick an image in the list first", "info");
		return;
	end

	local dest = ME.CleanPath(importWindow.dest:GetText());

	if ( ME.Extension(dest) ~= "blp" ) then
		ME.Notice("The destination has to be a .blp game path", "info");
		return;
	end

	local ok, message = ME.Do("ModelEditor_ImportTexture", item.name, dest);

	if ( not ok ) then
		return;
	end

	GameEditor_Notify(message and tostring(message) ~= "" and tostring(message) or format("Imported %s to %s", item.name, dest), "ok");

	if ( importWindow.useIt.checked and importWindow.target and Doc() == importWindow.target.doc ) then
		ME.Edit("ModelEditor_SetTexture", importWindow.target.doc, importWindow.target.index, { path = dest });
	end
end

local function ShowImport(index)
	if ( not importWindow ) then
		importWindow = GameEditor_CreateWindow("GameEditorModelImportWindow", "Import image", 420, 400, true);
		local c = importWindow.content;

		local refresh = Common.Button(c, "Refresh", 70, ImportRefresh, "Read the Import folder again");
		refresh:SetPoint("TOPRIGHT", c, "TOPRIGHT", 0, 0);
		importWindow.status = Common.Paragraph(c, "");
		importWindow.status:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -4);
		importWindow.status:SetPoint("RIGHT", refresh, "LEFT", -6, 0);

		local bottom = CreateFrame("Frame", NoggitUI_NewName("GameEditorModelImportBottom"), c);
		bottom:SetHeight(80);
		bottom:SetPoint("BOTTOMLEFT", c, "BOTTOMLEFT", 0, 0);
		bottom:SetPoint("BOTTOMRIGHT", c, "BOTTOMRIGHT", 0, 0);

		local destLabel = Common.Label(bottom, "Save as");
		destLabel:SetPoint("TOPLEFT", bottom, "TOPLEFT", 0, -4);
		importWindow.dest = NoggitLineEdit_Create(bottom, 330);
		importWindow.dest:SetHeight(22);
		importWindow.dest:SetMaxLetters(260);
		importWindow.dest:SetPoint("TOPLEFT", bottom, "TOPLEFT", 60, 0);
		importWindow.dest:SetPoint("TOPRIGHT", bottom, "TOPRIGHT", 0, 0);
		importWindow.dest:SetScript("OnEnterPressed", function(self) self:ClearFocus(); end);
		Common.Tooltip(importWindow.dest, "Game path of the BLP to write, it is written loose under the game folder");

		importWindow.useIt = NoggitCheckBox_Create(bottom, "Use it for the selected texture", true);
		importWindow.useIt:SetPoint("TOPLEFT", bottom, "TOPLEFT", 0, -28);

		local import = Common.Button(bottom, "Import", 80, DoImport, "Convert the image to a BLP2 the client reads, with mipmaps, sizes go to a power of two");
		import:SetPoint("BOTTOMRIGHT", bottom, "BOTTOMRIGHT", 0, 0);

		importWindow.list = NoggitListBox_Create(c, 20, function(row)
			row.text = NoggitUI_FontString(row, "");
			row.text:SetPoint("LEFT", row, "LEFT", 6, 0);
			row.text:SetPoint("RIGHT", row, "RIGHT", -4, 0);
		end, function(row, item)
			row.text:SetText(item.name);
		end);
		importWindow.list:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -28);
		importWindow.list:SetPoint("BOTTOMRIGHT", bottom, "TOPRIGHT", 0, 6);
		importWindow.list.onSelect = function(item)
			importWindow.dest:SetText(SuggestImportPath(item.name));
		end;
		importWindow.list.onDoubleClick = DoImport;
	end

	importWindow.target = index and { doc = Doc(), index = index } or nil;
	if ( index ) then
		importWindow.useIt:Show();
	else
		importWindow.useIt:Hide();
	end
	GameEditor_ShowWindow(importWindow);
	ImportRefresh();
end

local function TypeItems()
	return ME.EnumItems(ME.TEXTURE_TYPES, 0, 14);
end

local function BuildTexturePage(container)
	local page = NewPage(container);
	PageScroll(page, 0);
	local c = page.child;

	page.title = Common.Label(c, "Textures", "NoggitFontBold", NOGGIT_COLOR_GOLD);
	page.title:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -2);
	page.modern = Common.Paragraph(c, "");
	page.modern:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -18);
	page.modern:SetPoint("TOPRIGHT", c, "TOPRIGHT", 0, -18);

	page.list = NoggitListBox_Create(c, 20, function(row)
		row.index = NoggitUI_FontString(row, "");
		row.index:SetPoint("LEFT", row, "LEFT", 4, 0);
		row.index:SetWidth(24);
		NoggitText_SetColor(row.index, NOGGIT_COLOR_DISABLED);
		row.kind = NoggitUI_FontString(row, "");
		row.kind:SetPoint("LEFT", row, "LEFT", 30, 0);
		row.kind:SetWidth(110);
		row.path = NoggitUI_FontString(row, "");
		row.path:SetPoint("LEFT", row, "LEFT", 144, 0);
		row.path:SetPoint("RIGHT", row, "RIGHT", -4, 0);
	end, function(row, item)
		row.index:SetText(tostring(item.index));
		row.kind:SetText(ME.TEXTURE_TYPES[Num(item.type)] or ("type "..tostring(item.type)));
		row.path:SetText(TexturePathText(item));
	end);
	page.list:SetPoint("TOPLEFT", c, "TOPLEFT", 0, -36);
	page.list:SetPoint("TOPRIGHT", c, "TOPRIGHT", 0, -36);
	page.list:SetHeight(LIST_HEIGHT);
	page.list.onSelect = function(item)
		local entry = Entry();

		if ( entry ) then
			entry.sel.texture = item.index;
			page:Refresh();
		end
	end;

	local y = -36 - LIST_HEIGHT - 4;

	page.add = Common.Button(c, "Add texture", 90, function()
		local doc = Doc();

		if ( doc ) then
			local index = ME.Edit("ModelEditor_AddTexture", doc, { type = 0, flags = 0, path = "" });

			if ( index ) then
				Entry().sel.texture = Num(index);
			end
		end
	end, "Add a texture slot at the end, combos still point at the old ones");
	page.add:SetPoint("TOPLEFT", c, "TOPLEFT", 0, y);

	page.import = Common.Button(c, "Import image...", 110, function()
		local entry = Entry();
		ShowImport(entry and entry.sel.texture);
	end, "Turn a png, jpg, bmp or tga from <game folder>\\Import\\ into a BLP");
	page.import:SetPoint("LEFT", page.add, "RIGHT", 4, 0);

	y = y - 30;
	page.editTitle = Common.Label(c, "", "NoggitFontBold");
	page.editTitle:SetPoint("TOPLEFT", c, "TOPLEFT", 0, y);

	local function SetSelected(fields)
		local entry = Entry();

		if ( entry ) then
			ME.Edit("ModelEditor_SetTexture", entry.doc, entry.sel.texture, fields);
		end
	end

	y = y - 22;
	local typeLabel = Common.Label(c, "Type");
	typeLabel:SetPoint("TOPLEFT", c, "TOPLEFT", 0, y - 4);
	page.type = NoggitComboBox_Create(c, 220, TypeItems(), 1, function(_, _, item)
		SetSelected({ type = item.value });
	end);
	page.type:SetPoint("TOPLEFT", c, "TOPLEFT", LABEL_WIDTH, y);

	y = y - 26;
	local flagsLabel = Common.Label(c, "Flags");
	flagsLabel:SetPoint("TOPLEFT", c, "TOPLEFT", 0, y - 4);
	page.wrapX = NoggitCheckBox_Create(c, "Wrap X", false, function(_, checked)
		local texture = TextureRecord(Info(), Entry() and Entry().sel.texture);

		if ( texture ) then
			SetSelected({ flags = ME.SetBit(texture.flags, 0, checked) });
		end
	end);
	page.wrapX:SetPoint("TOPLEFT", c, "TOPLEFT", LABEL_WIDTH, y);
	page.wrapY = NoggitCheckBox_Create(c, "Wrap Y", false, function(_, checked)
		local texture = TextureRecord(Info(), Entry() and Entry().sel.texture);

		if ( texture ) then
			SetSelected({ flags = ME.SetBit(texture.flags, 1, checked) });
		end
	end);
	page.wrapY:SetPoint("LEFT", page.wrapX, "RIGHT", 16, 0);

	y = y - 26;
	local pathLabel = Common.Label(c, "Path");
	pathLabel:SetPoint("TOPLEFT", c, "TOPLEFT", 0, y - 4);
	page.check = Common.Button(c, "Check", 56, function()
		page:CheckPath(page.path:GetText());
	end, "Look the path up and read the size of the BLP");
	page.check:SetPoint("TOPRIGHT", c, "TOPRIGHT", 0, y);
	page.path = NoggitLineEdit_Create(c, 260);
	page.path:SetHeight(22);
	page.path:SetMaxLetters(260);
	page.path:SetPoint("TOPLEFT", c, "TOPLEFT", LABEL_WIDTH, y);
	page.path:SetPoint("RIGHT", page.check, "LEFT", -4, 0);
	Common.Tooltip(page.path, "BLP game path, Enter applies it. Replaceable types keep an empty path, the client fills them");

	local function ApplyPath(text)
		text = ME.CleanPath(text);
		local exists, known = ME.FileExists(text);

		if ( text ~= "" and known and not exists ) then
			NoggitMessageBox_Show("Texture not found", format("%s is not in the archives or the game folder. Use it anyway?", text), {
				{ text = "Use it", onClick = function() SetSelected({ path = text }); end },
				{ text = "Cancel" },
			});
			return;
		end

		SetSelected({ path = text });
	end

	page.path:SetScript("OnEnterPressed", function(self)
		self:ClearFocus();
		ApplyPath(self:GetText());
	end);
	page.path:SetScript("OnEscapePressed", function(self)
		self:ClearFocus();
		page:Refresh();
	end);

	y = y - 24;
	page.pathInfo = Common.Paragraph(c, "");
	page.pathInfo:SetPoint("TOPLEFT", c, "TOPLEFT", LABEL_WIDTH, y);
	page.pathInfo:SetPoint("TOPRIGHT", c, "TOPRIGHT", 0, y);

	y = y - 18;
	page.typeInfo = Common.Paragraph(c, "");
	page.typeInfo:SetPoint("TOPLEFT", c, "TOPLEFT", 0, y);
	page.typeInfo:SetPoint("TOPRIGHT", c, "TOPRIGHT", 0, y);

	-- Batches, combos and materials.
	y = y - 34;
	local batchTitle = Common.Label(c, "Batches", "NoggitFontBold", NOGGIT_COLOR_GOLD);
	batchTitle:SetPoint("TOPLEFT", c, "TOPLEFT", 0, y);
	local batchNote = Common.Label(c, "Skin profile 0, which texture combo and material each geoset draws with", nil, NOGGIT_COLOR_DISABLED);
	batchNote:SetPoint("LEFT", batchTitle, "RIGHT", 8, 0);

	y = y - 18;
	page.batches = NoggitListBox_Create(c, 20, function(row)
		row.text = NoggitUI_FontString(row, "");
		row.text:SetPoint("LEFT", row, "LEFT", 4, 0);
		row.text:SetPoint("RIGHT", row, "RIGHT", -4, 0);
	end, function(row, item)
		local info = Info();
		local combo = page:Combo(item.textureCombo);
		local material = ME.MaterialByIndex(info, Num(item.material));
		row.text:SetText(format("%d  geoset %d (section %d)  combo %d -> %s  material %d %s", item.index, Num(item.geoset), Num(item.skinSection), Num(item.textureCombo),
			combo and ME.TextureText(info, combo.texture) or "?", Num(item.material), material and (ME.BLEND_MODES[Num(material.blendMode)] or "") or "?"));
	end);
	page.batches:SetPoint("TOPLEFT", c, "TOPLEFT", 0, y);
	page.batches:SetPoint("TOPRIGHT", c, "TOPRIGHT", 0, y);
	page.batches:SetHeight(LIST_HEIGHT);
	page.batches.onSelect = function(item)
		local entry = Entry();

		if ( entry ) then
			entry.sel.batch = item.index;
			page:Refresh();
		end
	end;

	y = y - LIST_HEIGHT - 8;
	page.batchTitle = Common.Label(c, "", "NoggitFontBold");
	page.batchTitle:SetPoint("TOPLEFT", c, "TOPLEFT", 0, y);

	y = y - 22;
	local comboLabel = Common.Label(c, "Combo texture");
	comboLabel:SetPoint("TOPLEFT", c, "TOPLEFT", 0, y - 4);
	page.comboTexture = NoggitComboBox_Create(c, 220, { { value = 0, text = "" } }, 1, function(_, _, item)
		local batch = page:Batch();

		if ( batch ) then
			ME.Edit("ModelEditor_SetTextureCombo", Doc(), Num(batch.textureCombo), item.value);
		end
	end);
	page.comboTexture:SetPoint("TOPLEFT", c, "TOPLEFT", LABEL_WIDTH, y);
	page.comboUsers = Common.Label(c, "", nil, NOGGIT_COLOR_DISABLED);
	page.comboUsers:SetPoint("LEFT", page.comboTexture, "RIGHT", 8, 0);
	page.comboUsers:SetPoint("RIGHT", c, "RIGHT", 0, 0);

	local function SetMaterial(flags, blend)
		local batch = page:Batch();

		if ( batch ) then
			ME.Edit("ModelEditor_SetMaterial", Doc(), Num(batch.material), flags, blend);
		end
	end

	y = y - 26;
	local blendLabel = Common.Label(c, "Blend mode");
	blendLabel:SetPoint("TOPLEFT", c, "TOPLEFT", 0, y - 4);
	page.blend = NoggitComboBox_Create(c, 220, ME.EnumItems(ME.BLEND_MODES, 0, 7), 1, function(_, _, item)
		local material = page:Material();

		if ( material ) then
			SetMaterial(Num(material.flags), item.value);
		end
	end);
	page.blend:SetPoint("TOPLEFT", c, "TOPLEFT", LABEL_WIDTH, y);
	page.materialUsers = Common.Label(c, "", nil, NOGGIT_COLOR_DISABLED);
	page.materialUsers:SetPoint("LEFT", page.blend, "RIGHT", 8, 0);
	page.materialUsers:SetPoint("RIGHT", c, "RIGHT", 0, 0);

	y = y - 26;
	local renderLabel = Common.Label(c, "Render flags");
	renderLabel:SetPoint("TOPLEFT", c, "TOPLEFT", 0, y - 4);
	page.materialFlags = {};

	for index, flag in ipairs(ME.MATERIAL_FLAGS) do
		local column = mod(index - 1, 3);
		local line = floor((index - 1) / 3);
		local box = NoggitCheckBox_Create(c, flag.name, false, function(_, checked)
			local material = page:Material();

			if ( material ) then
				SetMaterial(ME.SetBit(material.flags, flag.bit, checked), Num(material.blendMode));
			end
		end);
		box:SetPoint("TOPLEFT", c, "TOPLEFT", LABEL_WIDTH + column * 112, y - line * 22);
		Common.Tooltip(box, format("0x%X, %s", 2 ^ flag.bit, flag.tooltip));
		box.flag = flag;
		page.materialFlags[index] = box;
	end

	y = y - 3 * 22 - 8;
	c:SetHeight(-y);

	page.Combo = function(self, index)
		for _, combo in ipairs(Info() and Info().textureCombos or {}) do
			if ( combo.index == Num(index) ) then
				return combo;
			end
		end
	end;

	page.Batch = function(self)
		local entry = Entry();
		local info = Info();

		for _, batch in ipairs(info and info.batches or {}) do
			if ( entry and batch.index == entry.sel.batch ) then
				return batch;
			end
		end
	end;

	page.Material = function(self)
		local batch = self:Batch();
		return batch and ME.MaterialByIndex(Info(), Num(batch.material));
	end;

	page.CheckPath = function(self, text)
		text = ME.CleanPath(text);

		if ( text == "" ) then
			self.pathInfo:SetText("");
			return;
		end

		local exists, known = ME.FileExists(text);

		if ( not known ) then
			self.pathInfo:SetText("");
			return;
		end

		if ( not exists ) then
			self.pathInfo:SetText("|cffff6060Not found in the archives or the game folder|r");
			return;
		end

		local texInfo = ME.Quiet("ModelEditor_GetTextureInfo", text);

		if ( type(texInfo) == "table" ) then
			local extra = "";

			if ( texInfo.alphaBits ~= nil ) then
				extra = extra..format(", alpha %d bit", Num(texInfo.alphaBits));
			end

			if ( texInfo.mips ~= nil ) then
				extra = extra..format(", %d mips", Num(texInfo.mips));
			end

			self.pathInfo:SetText(format("|cff60ff60Found|r, %d x %d %s%s", Num(texInfo.width), Num(texInfo.height), tostring(texInfo.format or ""), extra));
		else
			self.pathInfo:SetText("|cff60ff60Found|r");
		end
	end;

	page.Refresh = function(self)
		local entry = Entry();
		local info = Info();
		local textures = info and info.textures or {};

		if ( info and info.modern ) then
			self.modern:SetText("Modern model, textures named by a TXID file data id show the fdid. A new path replaces the fdid for that slot.");
		else
			self.modern:SetText("");
		end

		NoggitListBox_SetItems(self.list, textures, true);
		self.list.selected = nil;

		for index, texture in ipairs(textures) do
			if ( entry and texture.index == entry.sel.texture ) then
				self.list.selected = index;
			end
		end

		if ( entry and not self.list.selected and textures[1] ) then
			entry.sel.texture = textures[1].index;
			self.list.selected = 1;
		end

		NoggitListBox_Refresh(self.list);
		ME.SetEnabled(self.add, info ~= nil);
		ME.SetEnabled(self.import, info ~= nil);

		local texture = entry and TextureRecord(info, entry.sel.texture);
		self.type:EnableMouse(texture ~= nil);
		ME.SetEnabled(self.wrapX, texture ~= nil);
		ME.SetEnabled(self.wrapY, texture ~= nil);
		NoggitCheckBox_Refresh(self.wrapX);
		NoggitCheckBox_Refresh(self.wrapY);
		self.path:EnableMouse(texture ~= nil);

		if ( texture ) then
			local kind = Num(texture.type);
			self.editTitle:SetText(format("Texture %d", texture.index)..(Num(texture.fileDataId) > 0 and format(", file data id %d", texture.fileDataId) or ""));

			if ( ME.TEXTURE_TYPES[kind] ) then
				NoggitComboBox_SetSelected(self.type, kind + 1, true);
			else
				self.type.text:SetText(format("type %d", kind));
			end

			NoggitCheckBox_SetChecked(self.wrapX, ME.HasBit(texture.flags, 0));
			NoggitCheckBox_SetChecked(self.wrapY, ME.HasBit(texture.flags, 1));

			if ( not self.path:HasFocus() ) then
				self.path:SetText(texture.path or "");
			end

			local tip = ME.TEXTURE_TIPS[kind] or "";

			if ( kind >= 11 and kind <= 13 ) then
				tip = tip..format(". The DBC value is a file name in the folder of the model, %s", ME.Folder(entry.path) ~= "" and ME.Folder(entry.path) or "the model folder");
			end

			self.typeInfo:SetText(tip);

			if ( self.checkedFor ~= (entry.doc..":"..texture.index..":"..(texture.path or "")) ) then
				self.checkedFor = entry.doc..":"..texture.index..":"..(texture.path or "");
				self:CheckPath(kind == 0 and texture.path or "");
			end
		else
			self.editTitle:SetText(info and "No texture selected" or "No model open");
			self.path:SetText("");
			self.pathInfo:SetText("");
			self.typeInfo:SetText("");
		end

		local batches = info and info.batches or {};
		NoggitListBox_SetItems(self.batches, batches, true);
		self.batches.selected = nil;

		for index, batch in ipairs(batches) do
			if ( entry and batch.index == entry.sel.batch ) then
				self.batches.selected = index;
			end
		end

		if ( entry and not self.batches.selected and batches[1] ) then
			entry.sel.batch = batches[1].index;
			self.batches.selected = 1;
		end

		NoggitListBox_Refresh(self.batches);

		local batch = self:Batch();
		local material = self:Material();
		self.comboTexture:EnableMouse(batch ~= nil);
		self.blend:EnableMouse(material ~= nil);

		if ( batch ) then
			self.batchTitle:SetText(format("Batch %d, geoset %d, shader %d, priority %d", batch.index, Num(batch.geoset), Num(batch.shader), Num(batch.priorityPlane)));
			self.comboTexture.items = ME.TextureItems(info);
			local combo = self:Combo(batch.textureCombo);
			local found;

			for index, item in ipairs(self.comboTexture.items) do
				if ( combo and item.value == Num(combo.texture) ) then
					found = index;
				end
			end

			if ( found ) then
				NoggitComboBox_SetSelected(self.comboTexture, found, true);
			else
				self.comboTexture.text:SetText(combo and format("%d (missing)", Num(combo.texture)) or "no combo");
			end

			local comboUsers, materialUsers = 0, 0;

			for _, other in ipairs(batches) do
				if ( Num(other.textureCombo) == Num(batch.textureCombo) ) then
					comboUsers = comboUsers + 1;
				end

				if ( Num(other.material) == Num(batch.material) ) then
					materialUsers = materialUsers + 1;
				end
			end

			self.comboUsers:SetText(format("combo %d%s", Num(batch.textureCombo), comboUsers > 1 and format(", %d batches share it", comboUsers) or "") .. (Num(batch.textureCount) > 1 and format(", %d layers", batch.textureCount) or ""));
			self.materialUsers:SetText(format("material %d%s", Num(batch.material), materialUsers > 1 and format(", %d batches share it", materialUsers) or ""));
		else
			self.batchTitle:SetText(info and "No batch selected" or "");
			self.comboUsers:SetText("");
			self.materialUsers:SetText("");
		end

		if ( material ) then
			NoggitComboBox_SetSelected(self.blend, min(7, Num(material.blendMode)) + 1, true);

			if ( Num(material.blendMode) > 7 ) then
				self.blend.text:SetText(format("blend %d", material.blendMode));
			end
		end

		for _, box in ipairs(self.materialFlags) do
			ME.SetEnabled(box, material ~= nil);
			NoggitCheckBox_SetChecked(box, material and ME.HasBit(material.flags, box.flag.bit) or false);
		end

		self.child:SetWidth(ChildWidth(self));
		self.scroll.UpdateRange();
	end;

	return page;
end

-- Emitter list shared by the particle and ribbon pages -----------------------------------------------------------

local function EmitterList(page, options)
	page.list = NoggitListBox_Create(page, 20, function(row)
		row.text = NoggitUI_FontString(row, "");
		row.text:SetPoint("LEFT", row, "LEFT", 4, 0);
		row.text:SetPoint("RIGHT", row, "RIGHT", -4, 0);
	end, function(row, item)
		row.text:SetText(options.rowText(item));
	end);
	page.list:SetPoint("TOPLEFT", page, "TOPLEFT", 0, 0);
	page.list:SetPoint("TOPRIGHT", page, "TOPRIGHT", 0, 0);
	page.list:SetHeight(92);
	page.list.onSelect = function(item)
		local entry = Entry();

		if ( entry ) then
			entry.sel[options.key] = item.index;
			ME.HideTrackWindow();
			page:Refresh();
		end
	end;

	local bar = CreateFrame("Frame", NoggitUI_NewName("GameEditorModelEmitterBar"), page);
	bar:SetHeight(22);
	bar:SetPoint("TOPLEFT", page.list, "BOTTOMLEFT", 0, -4);
	bar:SetPoint("TOPRIGHT", page.list, "BOTTOMRIGHT", 0, -4);

	local function Select(index)
		local entry = Entry();

		if ( entry and index ) then
			entry.sel[options.key] = Num(index);
		end
	end

	page.add = Common.Button(bar, "Add", 60, function()
		if ( Doc() ) then
			Select(ME.Edit(options.add, Doc(), nil));
		end
	end, "Add a new "..options.noun.." with default values");
	page.add:SetPoint("LEFT", bar, "LEFT", 0, 0);

	page.duplicate = Common.Button(bar, "Duplicate", 80, function()
		local entry = Entry();

		if ( entry and page:Selected() ) then
			Select(ME.Edit(options.add, entry.doc, entry.sel[options.key]));
		end
	end, "Add a copy of the selected "..options.noun);
	page.duplicate:SetPoint("LEFT", page.add, "RIGHT", 4, 0);

	page.remove = Common.Button(bar, "Remove", 70, function()
		local entry = Entry();
		local index = entry and entry.sel[options.key];

		if ( not entry or not page:Selected() ) then
			return;
		end

		ConfirmRemove("Remove "..options.noun, format("Remove %s %d from the model?", options.noun, index), function()
			if ( ME.Edit(options.remove, entry.doc, index) ) then
				entry.sel[options.key] = max(0, index - 1);
				ME.HideTrackWindow();
			end
		end);
	end, "Remove the selected "..options.noun);
	page.remove:SetPoint("LEFT", page.duplicate, "RIGHT", 4, 0);

	page.countText = Common.Label(bar, "", nil, NOGGIT_COLOR_DISABLED);
	page.countText:SetPoint("LEFT", page.remove, "RIGHT", 8, 0);
	page.countText:SetPoint("RIGHT", bar, "RIGHT", 0, 0);

	page.Selected = function(self)
		local entry = Entry();
		local info = Info();

		for _, item in ipairs(info and info[options.infoKey] or {}) do
			if ( entry and item.index == entry.sel[options.key] ) then
				return item;
			end
		end
	end;

	page.RefreshList = function(self)
		local entry = Entry();
		local info = Info();
		local items = info and info[options.infoKey] or {};
		NoggitListBox_SetItems(self.list, items, true);
		self.list.selected = nil;

		for index, item in ipairs(items) do
			if ( entry and item.index == entry.sel[options.key] ) then
				self.list.selected = index;
			end
		end

		if ( entry and not self.list.selected and items[1] ) then
			entry.sel[options.key] = items[1].index;
			self.list.selected = 1;
		end

		NoggitListBox_Refresh(self.list);
		self.countText:SetText(info and format("%d %s%s", #items, options.noun, #items == 1 and "" or "s") or "");
		ME.SetEnabled(self.add, info ~= nil);
		ME.SetEnabled(self.duplicate, self:Selected() ~= nil);
		ME.SetEnabled(self.remove, self:Selected() ~= nil);
	end;

	return bar;
end

-- Keys GetParticle returns that the form already shows, anything else lands in the Advanced group.
local KNOWN_PARTICLE_KEYS = {};

local function Known(key)
	KNOWN_PARTICLE_KEYS[key] = true;
	return key;
end

for _, key in ipairs({ "index", "spline", "textures", "gravityPacked", "multiTexture", "gravityVectors" }) do
	Known(key);
end

-- Numbers and vectors of keys the form does not know, numbers as spin boxes.
local function AdvancedItem(form)
	local item = form:Item(24);
	local rows = {};
	local empty = Common.Label(item.frame, "", nil, NOGGIT_COLOR_DISABLED);
	empty:SetPoint("TOPLEFT", item.frame, "TOPLEFT", 0, -4);

	local function Row(index)
		local row = rows[index];

		if ( row ) then
			return row;
		end

		row = CreateFrame("Frame", NoggitUI_NewName("GameEditorModelExtra"), item.frame);
		row:SetHeight(24);
		row:SetPoint("TOPLEFT", item.frame, "TOPLEFT", 0, -(index - 1) * 24);
		row:SetPoint("TOPRIGHT", item.frame, "TOPRIGHT", 0, -(index - 1) * 24);
		row.label = Common.Label(row, "");
		row.label:SetPoint("LEFT", row, "LEFT", 0, 0);
		row.label:SetWidth(LABEL_WIDTH - 6);
		row.spin = ME.Spin(row, 110, -BIG * 1000, BIG * 1000, 0.1, 3, function(value)
			if ( row.key ) then
				form:Set(row.key, value);
			end
		end, "A field of the modern format this editor has no name for, it is sent back as a number");
		row.spin:SetPoint("LEFT", row, "LEFT", LABEL_WIDTH, 0);
		row.vector = ME.CreateValueEditor(row, "vec3", function(value)
			if ( row.key ) then
				form:Set(row.key, value);
			end
		end);
		row.vector:SetPoint("LEFT", row, "LEFT", LABEL_WIDTH, 0);
		row.text = Common.Label(row, "", nil, NOGGIT_COLOR_DISABLED);
		row.text:SetPoint("LEFT", row, "LEFT", LABEL_WIDTH, 0);
		row.text:SetPoint("RIGHT", row, "RIGHT", 0, 0);
		rows[index] = row;
		return row;
	end

	item.refresh = function(data)
		local keys = {};

		for key in pairs(data or {}) do
			if ( type(key) == "string" and not KNOWN_PARTICLE_KEYS[key] ) then
				table.insert(keys, key);
			end
		end

		table.sort(keys);

		for index, key in ipairs(keys) do
			local row = Row(index);
			local value = data[key];
			row.key = key;
			row.label:SetText(key);
			row:Show();
			row.spin:Hide();
			row.vector:Hide();
			row.text:Hide();

			if ( type(value) == "number" ) then
				row.spin:Show();
				ME.SetSpin(row.spin, value, true);
			elseif ( type(value) == "table" and (value.x or value[1]) and not value.sequences and not value.times ) then
				row.vector:Show();
				row.vector:SetValue(value);
			else
				row.text:Show();
				row.text:SetText(type(value) == "table" and "table, not editable here" or tostring(value));
			end
		end

		for index = #keys + 1, #rows do
			rows[index]:Hide();
			rows[index].key = nil;
		end

		empty:SetText(#keys == 0 and "No extra fields" or "");
		item.height = max(24, #keys * 24);
	end;

	return item;
end

local function MultiTexture(data)
	if ( not data ) then
		return false;
	end

	if ( data.multiTexture ~= nil ) then
		return data.multiTexture and true or false;
	end

	return ME.HasBit(data.flags, MULTI_TEXTURE_BIT);
end

local function GravityVector(data)
	return data and data.gravityPacked and true or false;
end

local CELL_ITEMS = {};

for power = 0, 8 do
	table.insert(CELL_ITEMS, { value = 2 ^ power, text = tostring(2 ^ power) });
end

local function ParticleTextureItem(form, slot)
	local item = form:Combo(Known("textures"), format("Texture %d", slot), ME.TextureItems, "Multi texture emitters blend three textures");
	local combo = item.combo;
	combo.onChange = function(_, _, entry)
		local data = form.data;

		if ( data and entry ) then
			local textures = ME.Copy(data.textures) or {};
			textures[slot] = entry.value;
			form:Set("textures", textures);
		end
	end;
	item.refresh = function(data, info)
		combo.items = ME.TextureItems(info);
		local value = data and data.textures and data.textures[slot];
		local found;

		for index, entry in ipairs(combo.items) do
			if ( entry.value == Num(value) ) then
				found = index;
			end
		end

		if ( found ) then
			NoggitComboBox_SetSelected(combo, found, true);
		else
			combo.text:SetText(value and format("%s (missing)", tostring(value)) or "");
		end

		combo:EnableMouse(data ~= nil);
	end;
	item.visible = MultiTexture;
	return item;
end

-- Particles page --------------------------------------------------------------------------------------------------

local function BuildParticlePage(container)
	local page = NewPage(container);
	EmitterList(page, {
		key = "particle",
		infoKey = "particles",
		noun = "emitter",
		add = "ModelEditor_AddParticle",
		remove = "ModelEditor_RemoveParticle",
		rowText = function(item)
			return format("%d  id %d  %s  texture %s  %s  bone %d", item.index, Num(item.id), ME.EMITTER_TYPES[Num(item.emitterType)] or ("type "..tostring(item.emitterType)),
				ME.TextureText(Info(), item.texture), ME.BLEND_MODES[Num(item.blendMode)] or ("blend "..tostring(item.blendMode)), Num(item.bone));
		end,
	});
	PageScroll(page, 122);

	local form = ME.CreateForm(page.child, 470, function(fields)
		local entry = Entry();

		if ( not entry ) then
			return;
		end

		local data = page.form.data;

		if ( Num(fields.emitterType) == 3 and #(data and data.spline or {}) < 2 ) then
			NoggitMessageBox_Show("Spline emitter", "A spline emitter needs at least 2 spline points and this one has fewer. Give it a straight line of 2 points so it can switch?", {
				{ text = "Add points", onClick = function()
					ME.Edit("ModelEditor_SetParticle", entry.doc, entry.sel.particle, { emitterType = 3, spline = { ME.MakeVector({ 0, 0, 0 }, { "x", "y", "z" }), ME.MakeVector({ 0, 0, 1 }, { "x", "y", "z" }) } });
				end },
				{ text = "Cancel", onClick = function() page:Refresh(); end },
			});
			return;
		end

		ME.Edit("ModelEditor_SetParticle", entry.doc, entry.sel.particle, fields);
	end);
	page.form = form;

	local function Float(key, label, tooltip)
		return form:Float(Known(key), label, tooltip);
	end

	local function Track(key, label, kind, tooltip)
		return form:Track(Known(key), label, kind or "float", tooltip);
	end

	form:Header("General");
	form:Int(Known("particleId"), "Id", -1, 2147483647, "Particle id, -1 when it has none");
	form:Combo(Known("emitterType"), "Emitter type", ME.EnumItems(ME.EMITTER_TYPES, 1, 3), "Plane spawns in a rectangle, sphere in a ball, spline along the spline points");
	form:Combo(Known("blendMode"), "Blend mode", ME.EnumItems(ME.BLEND_MODES, 0, 7));
	local texture = form:Combo(Known("textureIndex"), "Texture", ME.TextureItems, "Texture index of the model");
	texture.visible = function(data) return not MultiTexture(data); end;
	ParticleTextureItem(form, 1);
	ParticleTextureItem(form, 2);
	ParticleTextureItem(form, 3);
	form:Combo(Known("rows"), "Texture rows", CELL_ITEMS, "Rows of the cell grid in the texture, a power of two", 90);
	form:Combo(Known("cols"), "Texture columns", CELL_ITEMS, "Columns of the cell grid in the texture, a power of two", 90);
	local bone = form:Int(Known("boneIndex"), "Bone", 0, 65535, "Bone the emitter is attached to");
	bone.hintFunc = function(value, data, info)
		local count = #(info and info.bones or {});
		return format("of %d bones", count);
	end;
	form:Value(Known("position"), "Position", "vec3", "Offset from the bone");
	form:Int(Known("priorityPlane"), "Priority plane", -128, 127, "Draw order against other transparent things");
	form:Int(Known("colorIndex"), "Color index", 0, 65535, "11, 12 or 13 take the replacement colors of ParticleColor.dbc from the display, 0 for none");
	form:Flags(Known("flags"), "Flags", ME.PARTICLE_FLAGS, 32);

	form:Header("Emission");
	Track("emissionRateTrack", "Rate", "float", "Particles a second");
	Float("emissionRateVariation", "Rate variation");
	Track("lifeTrack", "Life", "float", "Seconds a particle lives");
	Float("lifeVariation", "Life variation");
	Track("speedTrack", "Speed", "float", "Start speed");
	Track("variationTrack", "Speed variation");
	Track("latitudeTrack", "Latitude", "float", "Spread angle in radians");
	Track("longitudeTrack", "Longitude", "float", "Second spread angle in radians");
	Track("widthTrack", "Area width");
	Track("lengthTrack", "Area length");
	Track("zsourceTrack", "Z source", "float", "Above 0 particles move away from a point this far below the emitter");
	local gravity = Track("gravityTrack", "Gravity", "float", "Pull down per second");
	gravity.visible = function(data) return not GravityVector(data); end;
	local gravityVector = form:Track("gravityVectors", "Gravity vector", "vec3", "The emitter packs its gravity as vectors, the keys are directions with strength");
	gravityVector.visible = GravityVector;
	local convert = form:Item(ME.ROW);
	local convertNote = Common.Label(convert.frame, "Packed gravity (flag 0x800000)", nil, NOGGIT_COLOR_DISABLED);
	convertNote:SetPoint("TOPLEFT", convert.frame, "TOPLEFT", LABEL_WIDTH, -4);
	local convertButton = Common.Button(convert.frame, "Convert to plain gravity", 160, function()
		local data = form.data;

		if ( not data or not data.gravityPacked ) then
			return;
		end

		NoggitMessageBox_Show("Plain gravity", "This turns the packed gravity vectors into plain downward gravity and clears flag 0x800000. The directions are lost. Go on?", {
			{ text = "Convert", onClick = function()
				local track = ME.Copy(data.gravityTrack);

				if ( type(track) ~= "table" ) then
					track = ME.ConstantTrack(0, Info());
				end

				form:Set("gravityTrack", track);
			end },
			{ text = "Cancel" },
		});
	end, "Write gravity as plain floats again, this clears the packed gravity flag");
	convertButton:SetHeight(20);
	convertButton:SetPoint("TOPRIGHT", convert.frame, "TOPRIGHT", 0, -1);
	convert.visible = GravityVector;
	Float("drag", "Drag");
	Track("visibilityTrack", "Visibility", "byte", "1 emits, 0 stops");
	form:Note(function(data)
		if ( data and Num(data.emitterType) == 3 ) then
			return format("Spline with %d points, the points are kept as they are", #(data.spline or {}));
		end
	end).visible = function(data) return data and Num(data.emitterType) == 3; end;

	form:Header("Appearance over the lifetime");
	form:PartTrack(Known("colorTrack"), "Color", "pcolor", "Color keys, 0 to 255", true);
	form:PartTrack(Known("alphaTrack"), "Alpha", "alpha", "Opacity keys, 0 to 1", true);
	form:PartTrack(Known("scaleTrack"), "Scale", "vec2", "Width and height keys", true);
	form:Value(Known("scaleVariation"), "Scale variation", "vec2");
	form:PartTrack(Known("headCellTrack"), "Head cell", "int", "Texture cell of the head");
	form:PartTrack(Known("tailCellTrack"), "Tail cell", "int", "Texture cell of the tail");
	Float("tailLength", "Tail length", "Seconds of motion the tail covers");
	Float("twinkleFPS", "Twinkle per second");
	Float("twinkleOnOff", "Twinkle on and off", "Share of the time the particle is shown, 0 to 1");
	form:Value(Known("twinkleScale"), "Twinkle scale", "range", "Smallest and largest twinkle size");

	local function HasKey(key)
		return function(data) return data and data[key] ~= nil; end;
	end

	form:Header("Multi texture").visible = HasKey("multiTextureParam0");
	form:Value(Known("multiTextureParam0"), "Param 0 first", "fp69", "Modern multi texture parameter, plus or minus 64", 1).visible = HasKey("multiTextureParam0");
	form:Value("multiTextureParam0", "Param 0 second", "fp69", "Modern multi texture parameter, plus or minus 64", 2).visible = HasKey("multiTextureParam0");
	form:Value(Known("multiTextureParam1"), "Param 1 first", "fp69", "Modern multi texture parameter, plus or minus 64", 1).visible = HasKey("multiTextureParam1");
	form:Value("multiTextureParam1", "Param 1 second", "fp69", "Modern multi texture parameter, plus or minus 64", 2).visible = HasKey("multiTextureParam1");

	form:Header("Motion");
	Float("initialSpin", "Start spin", "Radians");
	Float("initialSpinVariation", "Start spin variation");
	Float("spin", "Spin", "Radians a second");
	Float("spinVariation", "Spin variation");
	form:Value(Known("tumble"), "Tumble min", "vec3", "Smallest tumble per axis, radians a second", "min");
	form:Value("tumble", "Tumble max", "vec3", "Largest tumble per axis, radians a second", "max");
	form:Value(Known("windVector"), "Wind", "vec3", "Wind direction and strength");
	Float("windTime", "Wind time", "Seconds the wind pushes");
	Float("ivelScale", "Inherit velocity scale");
	Float("followSpeed1", "Follow speed 1");
	Float("followScale1", "Follow scale 1");
	Float("followSpeed2", "Follow speed 2");
	Float("followScale2", "Follow scale 2");

	form:Header("Models");
	form:Path(Known("geometryMdl"), "Geometry model", "Each particle is this model instead of a quad");
	form:Path(Known("recursionMdl"), "Recursion model", "The emitters of this model run on each particle");

	form:Header("Advanced");
	form:Note("Extra fields of the modern format the editor has no name for.", 20);
	AdvancedItem(form);

	page.empty = Common.Paragraph(page.child, "");
	page.empty:SetPoint("TOPLEFT", page.child, "TOPLEFT", 0, -4);
	page.empty:SetPoint("TOPRIGHT", page.child, "TOPRIGHT", 0, -4);

	page.Refresh = function(self)
		self:RefreshList();
		local entry = Entry();
		local selected = self:Selected();
		local data = selected and ME.Particle(entry.doc, selected.index);

		if ( data ) then
			self.empty:Hide();
			self.child:SetHeight(form:Refresh(data, Info()) + 10);
		else
			form:Refresh(nil, Info());

			for _, item in ipairs(form.items) do
				item.frame:Hide();
			end

			self.empty:SetText(Info() and (selected and "This emitter could not be read." or "The model has no particle emitters, Add makes one.") or "No model open");
			self.empty:Show();
			self.child:SetHeight(40);
		end

		self.child:SetWidth(ChildWidth(self));
		self.scroll.UpdateRange();
	end;

	return page;
end

-- Ribbons page ------------------------------------------------------------------------------------------------------

local function BuildRibbonPage(container)
	local page = NewPage(container);
	EmitterList(page, {
		key = "ribbon",
		infoKey = "ribbons",
		noun = "ribbon",
		add = "ModelEditor_AddRibbon",
		remove = "ModelEditor_RemoveRibbon",
		rowText = function(item)
			return format("%d  id %d  bone %d", item.index, Num(item.id), Num(item.bone));
		end,
	});
	PageScroll(page, 122);

	local form = ME.CreateForm(page.child, 470, function(fields)
		local entry = Entry();

		if ( entry ) then
			ME.Edit("ModelEditor_SetRibbon", entry.doc, entry.sel.ribbon, fields);
		end
	end);
	page.form = form;

	form:Header("General");
	form:Int("ribbonId", "Id", -1, 2147483647, "Ribbon id, -1 when it has none");
	local bone = form:Int("boneIndex", "Bone", 0, 65535, "Bone the ribbon is attached to");
	bone.hintFunc = function(value, data, info)
		return format("of %d bones", #(info and info.bones or {}));
	end;
	form:Value("position", "Position", "vec3", "Offset from the bone");
	form:Int("priorityPlane", "Priority plane", -32768, 32767, "Draw order against other transparent things");
	form:RibbonTextures("textureIndices", "materialIndices", "Textures");
	form:Int("textureRows", "Texture rows", 1, 256, "Rows of the cell grid, 1 to 256");
	form:Int("textureCols", "Texture columns", 1, 256, "Columns of the cell grid, 1 to 256");
	form:Track("textureSlotTrack", "Texture slot", "int", "Which cell of the texture grid shows");

	form:Header("Look");
	form:Track("colorTrack", "Color", "color", "Color from 0 to 1");
	form:Track("alphaTrack", "Alpha", "alpha", "Opacity from 0 to 1");
	form:Track("heightAboveTrack", "Height above", "float", "Width of the ribbon above the emitter");
	form:Track("heightBelowTrack", "Height below", "float", "Width of the ribbon below the emitter");
	form:Track("visibilityTrack", "Visibility", "byte", "1 draws, 0 hides");

	form:Header("Motion");
	form:Float("edgesPerSecond", "Edges per second", "How often a new segment starts");
	form:Float("edgeLifetime", "Edge lifetime", "Seconds a segment stays");
	form:Float("gravity", "Gravity", "Pull down on the old segments");

	page.empty = Common.Paragraph(page.child, "");
	page.empty:SetPoint("TOPLEFT", page.child, "TOPLEFT", 0, -4);
	page.empty:SetPoint("TOPRIGHT", page.child, "TOPRIGHT", 0, -4);

	page.Refresh = function(self)
		self:RefreshList();
		local entry = Entry();
		local selected = self:Selected();
		local data = selected and ME.Ribbon(entry.doc, selected.index);

		if ( data ) then
			self.empty:Hide();
			self.child:SetHeight(form:Refresh(data, Info()) + 10);
		else
			form:Refresh(nil, Info());

			for _, item in ipairs(form.items) do
				item.frame:Hide();
			end

			self.empty:SetText(Info() and (selected and "This ribbon could not be read." or "The model has no ribbon emitters, Add makes one.") or "No model open");
			self.empty:Show();
			self.child:SetHeight(40);
		end

		self.child:SetWidth(ChildWidth(self));
		self.scroll.UpdateRange();
	end;

	return page;
end

local ATTACH_KINDS = {
	{ value = "marker", text = "Marker cube" },
	{ value = "rider", text = "Human male" },
};

local function BuildAttachmentPage(container)
	local page = NewPage(container);
	local preview = { shown = true, kind = "marker", anim = ME.MOUNT_ANIM, size = 1 };

	EmitterList(page, {
		key = "attachment",
		infoKey = "attachments",
		noun = "attachment",
		add = "ModelEditor_AddAttachment",
		remove = "ModelEditor_RemoveAttachment",
		rowText = function(item)
			return format("%d  %s (%d)  bone %d", item.index, ME.AttachmentName(item.id), Num(item.id), Num(item.bone));
		end,
	});
	PageScroll(page, 122);

	local function ApplyPreview()
		local selected = page:IsShown() and preview.shown and page:Selected();

		if ( not selected ) then
			ME.SetPreviewAttach(nil);
			return;
		end

		ME.SetPreviewAttach({ attachment = Num(selected.id), rider = preview.kind == "rider", anim = preview.anim, size = preview.size });
	end

	local form = ME.CreateForm(page.child, 470, function(fields)
		local entry = Entry();

		if ( entry ) then
			ME.Edit("ModelEditor_SetAttachment", entry.doc, entry.sel.attachment, fields);
		end
	end);
	page.form = form;

	form:Header("Attachment");
	form:Combo("attachmentId", "Id", ME.EnumItems(ME.ATTACHMENT_NAMES, 0, 255), "What the client attaches here, items, spell effects and vehicle seats look the id up", 240);
	local bone = form:Int("boneIndex", "Bone", 0, 65535, "Bone the point moves with");
	bone.hintFunc = function(value, data, info)
		return format("of %d bones", #(info and info.bones or {}));
	end;
	form:Value("position", "Position", "vec3", "Offset from the bone pivot in model units");
	form:Track("animateTrack", "Visible", "byte", "1 shows what hangs on the point, 0 hides it");

	form:Header("Preview");
	local controls = form:Item(52);
	local showBox = NoggitCheckBox_Create(controls.frame, "Show in the preview", true, function(_, checked)
		preview.shown = checked;
		ApplyPreview();
	end);
	showBox:SetPoint("TOPLEFT", controls.frame, "TOPLEFT", 0, 0);

	local kindCombo = NoggitComboBox_Create(controls.frame, 120, ATTACH_KINDS, 1, function(_, _, item)
		if ( item ) then
			preview.kind = item.value;
			ApplyPreview();
			page:Refresh();
		end
	end);
	kindCombo:SetPoint("TOPLEFT", controls.frame, "TOPLEFT", LABEL_WIDTH, 0);

	local sizeSpin = ME.Spin(controls.frame, 70, 0.1, 20, 0.1, 1, function(value)
		preview.size = value;
		ApplyPreview();
	end, "Size of the marker cube");
	sizeSpin:SetPoint("LEFT", kindCombo, "RIGHT", 6, 0);
	ME.SetSpin(sizeSpin, preview.size, true);

	local animButton = Common.Button(controls.frame, "Mount", 150, function()
		local items = ME.AnimItems(ME.PreviewRiderSequences());

		if ( #items == 0 ) then
			ME.Notice("The human male is still loading", "info");
			return;
		end

		GameEditor_PickValue("Rider animation", items, preview.anim, function(value)
			preview.anim = Num(value);
			ApplyPreview();
			page:Refresh();
		end);
	end, "Animation the human male plays, only the ones its model has are listed");
	animButton:SetHeight(20);
	animButton:SetPoint("LEFT", kindCombo, "RIGHT", 6, 0);

	local stateText = Common.Label(controls.frame, "", nil, NOGGIT_COLOR_DISABLED);
	stateText:SetPoint("TOPLEFT", controls.frame, "TOPLEFT", 0, -28);
	stateText:SetPoint("RIGHT", controls.frame, "RIGHT", 0, 0);
	controls.refresh = function()
		NoggitCheckBox_SetChecked(showBox, preview.shown);
		local rider = preview.kind == "rider";

		if ( rider ) then
			sizeSpin:Hide();
			animButton:Show();
			animButton:SetText(format("%s (%d)", ME.AnimName(preview.anim), preview.anim));
		else
			animButton:Hide();
			sizeSpin:Show();
		end
	end;

	page.empty = Common.Paragraph(page.child, "");
	page.empty:SetPoint("TOPLEFT", page.child, "TOPLEFT", 0, -4);
	page.empty:SetPoint("TOPRIGHT", page.child, "TOPRIGHT", 0, -4);

	local elapsedSince = 0;
	page:SetScript("OnUpdate", function(_, elapsed)
		elapsedSince = elapsedSince + elapsed;

		if ( elapsedSince < 0.25 ) then
			return;
		end

		elapsedSince = 0;
		local selected = page:Selected();

		if ( not selected or not preview.shown ) then
			stateText:SetText("");
		elseif ( ME.previewAttached ) then
			stateText:SetText(format("Showing on %s, saved edits move it after the preview reloads", ME.AttachmentName(selected.id)));
		elseif ( ME.previewAttached == false ) then
			stateText:SetText("The preview has not loaded this attachment yet");
		end
	end);
	page:SetScript("OnShow", ApplyPreview);
	page:SetScript("OnHide", function()
		ME.SetPreviewAttach(nil);
	end);

	page.Refresh = function(self)
		self:RefreshList();
		local entry = Entry();
		local selected = self:Selected();
		local data = selected and ME.Call("ModelEditor_GetAttachment", entry.doc, selected.index);

		if ( type(data) == "table" ) then
			self.empty:Hide();
			self.child:SetHeight(form:Refresh(data, Info()) + 10);
		else
			form:Refresh(nil, Info());

			for _, item in ipairs(form.items) do
				item.frame:Hide();
			end

			self.empty:SetText(Info() and "The model has no attachment points, Add makes one on bone 0." or "No model open");
			self.empty:Show();
			self.child:SetHeight(40);
		end

		self.child:SetWidth(ChildWidth(self));
		self.scroll.UpdateRange();
		ApplyPreview();
	end;

	return page;
end

ME.BuildTexturePage = BuildTexturePage;
ME.BuildAttachmentPage = BuildAttachmentPage;
ME.BuildParticlePage = BuildParticlePage;
ME.BuildRibbonPage = BuildRibbonPage;
ME.ShowImport = ShowImport;
