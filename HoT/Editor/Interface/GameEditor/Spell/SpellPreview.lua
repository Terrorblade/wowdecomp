-- Spell visual preview: the dock panel that plays the open spell from the player on a target.

local SE = GameEditorSpell;

SE.preview = { mode = "auto", applyFirst = true, loop = false, interval = 3, timer = 0 };

local TARGET_MODES = {
	{ key = "auto", text = "Automatic", tooltip = "Your target, else the creature or object selected in the editor, else yourself" },
	{ key = "target", text = "Current target", tooltip = "The unit you have targeted" },
	{ key = "selection", text = "Editor selection", tooltip = "The creature or gameobject selected with the select tool" },
	{ key = "self", text = "Myself", tooltip = "Your own character" },
};

local panel;

local function TargetOf(mode)
	if ( mode == "target" or mode == "auto" ) then
		if ( UnitExists("target") ) then
			return SE.GuidToDecimal(UnitGUID("target")), UnitName("target") or "target";
		end

		if ( mode == "target" ) then
			return nil, nil;
		end
	end

	if ( mode == "selection" or mode == "auto" ) then
		local kind, guid, _, name = GameEditor_GetSelection();

		if ( (kind == "creature" or kind == "gameobject") and guid ) then
			return guid, (name or kind).." (selection)";
		end

		if ( mode == "selection" ) then
			return nil, nil;
		end
	end

	return SE.GuidToDecimal(UnitGUID("player")), "yourself";
end

function SE.PreviewTargetGuid(mode)
	return (TargetOf(mode or SE.preview.mode));
end

function SE.PreviewTargetText(mode)
	local guid, name = TargetOf(mode or SE.preview.mode);

	if ( not guid ) then
		return "|cffff6060nothing to cast on|r";
	end

	return name;
end

-- Applies unsaved edits to this client so the preview shows them, the spell row and any dirty visual rows.
local function ApplyEdits()
	if ( not SE.preview.applyFirst or not SE.cur ) then
		return;
	end

	if ( SE.IsDirty() or SE.cur.isNew ) then
		SE.ApplyLocal(true);
	end

	for _, editor in ipairs(SE.DirtyEditors()) do
		if ( editor.ApplyLocal ) then
			editor:ApplyLocal(true);
		end
	end
end

function SE.PreviewCast(mode)
	local cur = SE.cur;

	if ( not cur ) then
		GameEditor_Notify("Open a spell to preview it.", "error");
		return;
	end

	if ( not SpellPreview_Cast ) then
		GameEditor_Notify("SpellPreview_Cast is not available in this client build.", "error");
		return;
	end

	mode = mode or SE.preview.mode;
	local guid = TargetOf(mode);

	if ( not guid ) then
		GameEditor_Notify("There is no target for that preview mode.", "error");
		return;
	end

	ApplyEdits();
	local ok, err = pcall(SpellPreview_Cast, cur.id, guid);

	if ( not ok ) then
		GameEditor_Notify("Preview failed: "..tostring(err), "error");
		return;
	end

	SE.preview.lastMode = mode;
	SE.preview.timer = SE.preview.interval;
	SE.preview.started = true;
end

function SE.PreviewStop(quiet)
	if ( quiet ~= true ) then
		if ( SE.PreviewIsPlaying() or SE.preview.loop ) then
			GameEditor_Notify("Preview stopped.", "info");
		else
			GameEditor_Notify("No preview is playing.", "info");
		end
	end

	SE.preview.loop = false;

	if ( panel ) then
		NoggitCheckBox_SetChecked(panel.loop, false);
	end

	if ( SpellPreview_Stop ) then
		pcall(SpellPreview_Stop);
	end
end

function SE.PreviewIsPlaying()
	if ( not SpellPreview_IsPlaying ) then
		return false;
	end

	local ok, playing = pcall(SpellPreview_IsPlaying);
	return ok and (playing == true or playing == 1);
end

function SE.UpdatePreview(elapsed)
	local preview = SE.preview;

	if ( not preview.loop or not preview.started ) then
		return;
	end

	if ( SE.PreviewIsPlaying() ) then
		preview.timer = preview.interval;
		return;
	end

	preview.timer = preview.timer - elapsed;

	if ( preview.timer <= 0 ) then
		SE.PreviewCast(preview.lastMode);
	end
end

function SE.PreviewStatusText()
	if ( SE.PreviewIsPlaying() ) then
		return "Preview playing";
	end

	if ( SE.preview.loop and SE.preview.started ) then
		return format("Preview again in %.1f s", max(0, SE.preview.timer));
	end
end

function SE.CreatePreviewPanel(parent)
	panel = CreateFrame("Frame", "GameEditorSpellPreviewPanel", parent);

	local scroll, child = GameEditor_CreateScrollFrame(panel);
	scroll:SetPoint("TOPLEFT", panel, "TOPLEFT", 8, -8);
	scroll:SetPoint("BOTTOMRIGHT", panel, "BOTTOMRIGHT", -2, 6);
	panel.scroll = scroll;
	panel.child = child;

	local spellBox = SE.Box(child, "GameEditorSpellPreviewSpell", NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID);
	spellBox:SetHeight(46);
	spellBox:SetPoint("TOPLEFT", child, "TOPLEFT", 0, 0);
	spellBox:SetPoint("TOPRIGHT", child, "TOPRIGHT", -4, 0);
	panel.icon = spellBox:CreateTexture(nil, "ARTWORK");
	panel.icon:SetWidth(36);
	panel.icon:SetHeight(36);
	panel.icon:SetPoint("LEFT", spellBox, "LEFT", 5, 0);
	panel.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93);
	panel.name = SE.Label(spellBox, "", "NoggitFontBold", NOGGIT_COLOR_WHITE);
	panel.name:SetPoint("TOPLEFT", spellBox, "TOPLEFT", 48, -7);
	panel.name:SetPoint("RIGHT", spellBox, "RIGHT", -6, 0);
	panel.detail = SE.Label(spellBox, "", nil, NOGGIT_COLOR_DISABLED);
	panel.detail:SetPoint("TOPLEFT", spellBox, "TOPLEFT", 48, -25);
	panel.detail:SetPoint("RIGHT", spellBox, "RIGHT", -6, 0);

	local y = -56;
	local heading = SE.Label(child, "Cast on", "NoggitFontBold");
	heading:SetPoint("TOPLEFT", child, "TOPLEFT", 2, y);
	y = y - 20;

	local radios = {};
	panel.radios = {};

	for _, mode in ipairs(TARGET_MODES) do
		local radio = NoggitCheckBox_Create(child, mode.text, SE.preview.mode == mode.key, function()
			SE.preview.mode = mode.key;
			panel:Refresh();
		end, radios);
		radio.tooltip = mode.tooltip;
		radio:SetPoint("TOPLEFT", child, "TOPLEFT", 4, y);
		panel.radios[mode.key] = radio;
		y = y - 22;
	end

	panel.targetText = SE.Label(child, "", nil, NOGGIT_COLOR_DISABLED);
	panel.targetText:SetPoint("TOPLEFT", child, "TOPLEFT", 4, y - 2);
	panel.targetText:SetPoint("RIGHT", child, "RIGHT", -4, 0);
	y = y - 26;

	local cast = SE.Button(child, "Cast on target", 10, function() SE.PreviewCast(); end, "Play the open spell from your character on the chosen target (F5)", true);
	cast:SetHeight(28);
	cast:SetPoint("TOPLEFT", child, "TOPLEFT", 0, y);
	cast:SetPoint("TOPRIGHT", child, "TOPRIGHT", -4, y);
	y = y - 34;

	local castSelf = SE.Button(child, "Cast on self", 10, function() SE.PreviewCast("self"); end, "Play the spell on your own character (Shift+F5)");
	castSelf:SetPoint("TOPLEFT", child, "TOPLEFT", 0, y);
	castSelf:SetPoint("RIGHT", child, "CENTER", -2, 0);
	local stop = SE.Button(child, "Stop", 10, function() SE.PreviewStop(); end, "Stop the preview and the repeat (Ctrl+F5)");
	stop:SetPoint("TOPLEFT", child, "TOP", 2, y);
	stop:SetPoint("RIGHT", child, "RIGHT", -4, 0);
	y = y - 32;

	panel.apply = NoggitCheckBox_Create(child, "Apply unsaved edits before casting", SE.preview.applyFirst, function(_, checked)
		SE.preview.applyFirst = checked;
	end);
	panel.apply.tooltip = "Patches this client with the edited rows first (Apply locally), nothing is saved and the server is not involved";
	panel.apply:SetPoint("TOPLEFT", child, "TOPLEFT", 4, y);
	y = y - 24;

	panel.loop = NoggitCheckBox_Create(child, "Repeat every", SE.preview.loop, function(_, checked)
		SE.preview.loop = checked;
		SE.preview.timer = 0;

		if ( checked and not SE.preview.started ) then
			SE.PreviewCast();
		end
	end);
	panel.loop:SetPoint("TOPLEFT", child, "TOPLEFT", 4, y);
	local interval = NoggitSpinBox_Create(child, 70, 0.5, 60, 0.5, 1, function(_, value)
		SE.preview.interval = value;
	end);
	NoggitSpinBox_SetValue(interval, SE.preview.interval, true);
	interval:SetPoint("LEFT", panel.loop, "RIGHT", 6, 0);
	local seconds = SE.Label(child, "seconds", nil, NOGGIT_COLOR_DISABLED);
	seconds:SetPoint("LEFT", interval, "RIGHT", 6, 0);
	y = y - 28;

	panel.freeCam = NoggitCheckBox_Create(child, "Free camera", GameEditor_IsFreeCamEnabled(), function(_, checked)
		GameEditor_SetFreeCamEnabled(checked);
	end);
	panel.freeCam.tooltip = "Off returns to your normal camera so you can watch your character cast, WASD walks and the wheel zooms";
	panel.freeCam:SetPoint("TOPLEFT", child, "TOPLEFT", 4, y);
	panel.freeCam:HookScript("OnShow", function(self)
		NoggitCheckBox_SetChecked(self, GameEditor_IsFreeCamEnabled());
	end);
	y = y - 24;

	panel.status = SE.Label(child, "", nil, NOGGIT_COLOR_GOLD);
	panel.status:SetPoint("TOPLEFT", child, "TOPLEFT", 4, y);
	y = y - 20;

	local explain = SE.Label(child, "The preview runs on this client only. Your character starts the cast of the open spell and, after its cast time, "..
		"the missile and impact visuals play on the target. Nothing is sent to the server, no damage is done and no aura is applied. "..
		"Click a creature in the world with the select tool to use it as the target.", nil, NOGGIT_COLOR_DISABLED);
	explain:SetPoint("TOPLEFT", child, "TOPLEFT", 4, y);
	explain:SetPoint("RIGHT", child, "RIGHT", -6, 0);
	explain:SetJustifyV("TOP");
	panel.explain = explain;

	local tooltipTitle = SE.Label(child, "Tooltip", "NoggitFontBold");
	panel.tooltipTitle = tooltipTitle;
	panel.tooltip = SE.CreateTooltipPreview(child, 260);

	panel.Layout = function(self)
		self.scroll.UpdateRange();
		local width = max(160, self.child:GetWidth() - 4);
		local top = y - max(14, self.explain:GetHeight()) - 14;
		self.tooltipTitle:ClearAllPoints();
		self.tooltipTitle:SetPoint("TOPLEFT", self.child, "TOPLEFT", 2, top);
		top = top - 20;
		self.tooltip:ClearAllPoints();
		self.tooltip:SetPoint("TOPLEFT", self.child, "TOPLEFT", 0, top);
		self.tooltip:SetWidth(width);
		self.tooltip:Update(SE.cur and SE.cur.values);
		top = top - self.tooltip:GetHeight() - 8;
		self.child:SetHeight(-top);
		self.scroll.UpdateRange();
	end;

	panel.Refresh = function(self)
		local cur = SE.cur;

		if ( cur ) then
			self.icon:SetTexture(SE.IconPath(cur.values.m_spellIconID));
			self.name:SetText(tostring(cur.values["m_name[0]"] or ""));
			self.detail:SetText(format("%d%s%s", cur.id, (cur.values["m_nameSubtext[0]"] and cur.values["m_nameSubtext[0]"] ~= "") and ("  -  "..cur.values["m_nameSubtext[0]"]) or "",
				(SE.IsDirty() and "  |cffffcc00unsaved|r") or ""));
		else
			self.icon:SetTexture("Interface\\Icons\\INV_Misc_QuestionMark");
			self.name:SetText("No spell open");
			self.detail:SetText("Pick one in the spell list");
		end

		for key, radio in pairs(self.radios) do
			NoggitCheckBox_SetChecked(radio, key == SE.preview.mode);
		end

		self.targetText:SetText("Casts on: "..SE.PreviewTargetText());
		self:Layout();
	end;

	panel:SetScript("OnShow", function(self) self:Refresh(); end);
	panel:SetScript("OnSizeChanged", function(self) self:Layout(); end);
	panel:SetScript("OnUpdate", function(self, elapsed)
		self.statusElapsed = (self.statusElapsed or 0) + elapsed;

		if ( self.statusElapsed > 0.2 ) then
			self.statusElapsed = 0;
			self.status:SetText(SE.PreviewStatusText() or "");
		end
	end);

	SE.On("REFRESH", function()
		if ( panel:IsVisible() ) then
			panel:Refresh();
		end
	end);
	SE.On("GAME_EDITOR_SELECTION_CHANGED", function()
		if ( panel:IsVisible() ) then
			panel.targetText:SetText("Casts on: "..SE.PreviewTargetText());
		end
	end);
	SE.On("PLAYER_TARGET_CHANGED", function()
		if ( panel:IsVisible() ) then
			panel.targetText:SetText("Casts on: "..SE.PreviewTargetText());
		end
	end);

	return panel;
end
