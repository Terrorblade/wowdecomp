-- Game editor shell. Workspaces (map, creatures, objects, spells) register here and the shell hosts them.

local TOOLBAR_SPACING = 4;
local TOOLBAR_PADDING = 5;
local DOCK_WIDTH = 265;
local TOAST_TIME = 4;
local MAX_TOASTS = 5;

local workspaces = {};
local ordered = {};
local activeWorkspace;
local activeTool;
local toolPanelShown = true;
local uiHidden = false;
local loaded = false;
local skipNextActivate = false;
local screenHeight;
local userScale = 1;

local shellMenuStrip;
local statusCells = {};
local commandBox;
local toasts = {};
local windows = {};
local fpsText = "";
local fpsElapsed = 0;

local consoleWindow;
local consoleFrame;
local cameraWindow;
local keysWindow;

local SHELL_KEYS = {
	{ "F12", "Open and close the game editor" },
	{ "Tab", "Hide or show the editor UI" },
	{ "Ctrl+1 .. Ctrl+9", "Switch workspace" },
	{ "Enter", "Focus the command line (. commands go to the server, / commands run like chat)" },
	{ "W A S D, Space, X", "Fly the camera, Shift is fast and Ctrl is slow" },
	{ "Right mouse", "Look around" },
	{ "Mouse wheel", "Camera speed, when no tool uses it" },
	{ "Left click", "Select a creature or gameobject" },
	{ "Left drag", "Move the selection along the ground" },
	{ "Shift + drag", "Move the selection up and down" },
	{ "Alt + drag", "Move the selection flat at its current height" },
	{ "Wheel while dragging", "Raise or lower the selection, Ctrl for fine steps" },
	{ "Ctrl + click", "Show the gizmo, click the selection again to cycle move, rotate, off" },
	{ "Ctrl + wheel", "Turn the selection, Shift for single degrees" },
	{ "Page Up / Page Down", "Raise or lower the selection, Shift fine, Ctrl coarse" },
	{ "Arrow keys", "Nudge the selection relative to the camera" },
	{ "[ and ]", "Turn the selection by 15 degrees, Shift for 1 degree" },
	{ "G", "Drop the selection to the ground" },
	{ "Escape", "Cancel a drag, hide the gizmo, then clear the selection" },
};

local function Color(color, alpha)
	return color[1], color[2], color[3], alpha or color[4];
end

local function SortedWorkspaces()
	table.sort(ordered, function(a, b)
		if ( (a.order or 100) == (b.order or 100) ) then
			return a.key < b.key;
		end

		return (a.order or 100) < (b.order or 100);
	end);

	return ordered;
end

local function SafeCall(owner, func, ...)
	if ( not func ) then
		return;
	end

	local ok, a, b, c = pcall(func, ...);

	if ( not ok ) then
		GameEditor_Print(format("Game editor: %s failed: %s", tostring(owner), tostring(a)), 1, 0.3, 0.3);
		return;
	end

	return a, b, c;
end

function GameEditor_Print(text, r, g, b)
	text = tostring(text);

	if ( consoleFrame ) then
		consoleFrame:AddMessage(text, r or 0.85, g or 0.85, b or 0.85);
	end

	if ( DEFAULT_CHAT_FRAME ) then
		DEFAULT_CHAT_FRAME:AddMessage(text, r or 1, g or 0.82, b or 0);
	end
end

-- Toasts under the menu bar, kind is "ok", "error" or "info".
function GameEditor_Notify(text, kind)
	if ( not loaded or not text or text == "" ) then
		return;
	end

	local toast;

	for _, candidate in ipairs(toasts) do
		if ( not candidate:IsShown() ) then
			toast = candidate;
			break;
		end
	end

	if ( not toast ) then
		if ( #toasts < MAX_TOASTS ) then
			toast = CreateFrame("Frame", NoggitUI_NewName("GameEditorToast"), EditorNotifications, "NoggitBoxTemplate");
			toast:SetWidth(420);
			toast:SetHeight(28);
			toast:SetFrameStrata("DIALOG");
			toast.stripe = NoggitUI_SolidTexture(toast, "ARTWORK", NOGGIT_COLOR_ACCENT);
			toast.stripe:SetWidth(4);
			toast.stripe:SetPoint("TOPLEFT", toast, "TOPLEFT", 0, 0);
			toast.stripe:SetPoint("BOTTOMLEFT", toast, "BOTTOMLEFT", 0, 0);
			toast.text = NoggitUI_FontString(toast, "");
			toast.text:SetPoint("LEFT", toast, "LEFT", 12, 0);
			toast.text:SetPoint("RIGHT", toast, "RIGHT", -8, 0);
			toast:EnableMouse(true);
			toast:SetScript("OnMouseUp", function(self) self:Hide(); end);
			table.insert(toasts, toast);
		else
			toast = toasts[1];

			for _, candidate in ipairs(toasts) do
				if ( candidate.age > toast.age ) then
					toast = candidate;
				end
			end
		end
	end

	local stripe = NOGGIT_COLOR_ACCENT;

	if ( kind == "error" ) then
		stripe = { 0.85, 0.25, 0.25, 1 };
	elseif ( kind == "ok" ) then
		stripe = { 0.3, 0.75, 0.35, 1 };
	end

	NoggitBox_Set(toast, NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID);
	toast.stripe:SetTexture(Color(stripe));
	toast.text:SetText(text);
	toast.age = 0;
	toast:SetAlpha(1);
	toast:Show();

	GameEditor_Print(text, kind == "error" and 1 or 0.85, kind == "error" and 0.35 or 0.85, kind == "error" and 0.35 or 0.85);
end

local function LayoutToasts(elapsed)
	local shown = {};

	for _, toast in ipairs(toasts) do
		if ( toast:IsShown() ) then
			toast.age = toast.age + elapsed;

			if ( toast.age > TOAST_TIME ) then
				local alpha = 1 - (toast.age - TOAST_TIME) * 2;

				if ( alpha <= 0 ) then
					toast:Hide();
				else
					toast:SetAlpha(alpha);
				end
			end

			if ( toast:IsShown() ) then
				table.insert(shown, toast);
			end
		end
	end

	table.sort(shown, function(a, b) return a.age < b.age; end);

	for index, toast in ipairs(shown) do
		toast:ClearAllPoints();
		toast:SetPoint("TOP", EditorNotifications, "TOP", 0, -(index - 1) * 32);
	end
end

-- Windows ------------------------------------------------------------------------------------------

local function RaiseWindow(window)
	window:Raise();
end

function GameEditor_CreateWindow(name, title, width, height, resizable)
	local window = CreateFrame("Frame", name or NoggitUI_NewName("GameEditorWindow"), EditorParent, "NoggitBoxTemplate");
	window:SetFrameStrata("DIALOG");
	window:SetToplevel(true);
	window:EnableMouse(true);
	window:SetMovable(true);
	window:SetClampedToScreen(true);
	window:SetWidth(width or 400);
	window:SetHeight(height or 300);
	window:SetPoint("CENTER", EditorParent, "CENTER", 0, 0);
	window:Hide();
	NoggitBox_Set(window, NOGGIT_COLOR_BASE, NOGGIT_COLOR_MID);
	window:SetScript("OnMouseDown", RaiseWindow);

	local bar = CreateFrame("Frame", NoggitUI_NewName("GameEditorWindowBar"), window, "NoggitBoxTemplate");
	bar:SetHeight(26);
	bar:SetPoint("TOPLEFT", window, "TOPLEFT", 0, 0);
	bar:SetPoint("TOPRIGHT", window, "TOPRIGHT", 0, 0);
	bar:EnableMouse(true);
	bar:RegisterForDrag("LeftButton");
	bar:SetScript("OnMouseDown", function() RaiseWindow(window); end);
	bar:SetScript("OnDragStart", function() window:StartMoving(); end);
	bar:SetScript("OnDragStop", function() window:StopMovingOrSizing(); end);
	NoggitBox_Set(bar, NOGGIT_COLOR_DARK, nil);
	window.bar = bar;

	window.titleText = NoggitUI_FontString(bar, title or "");
	window.titleText:SetPoint("LEFT", bar, "LEFT", 10, 0);
	window.titleText:SetPoint("RIGHT", bar, "RIGHT", -40, 0);

	local close = CreateFrame("Button", NoggitUI_NewName("GameEditorWindowClose"), bar, "NoggitBoxTemplate");
	close:SetWidth(32);
	close:SetHeight(26);
	close:SetPoint("RIGHT", bar, "RIGHT", 0, 0);
	NoggitBox_Set(close, NOGGIT_COLOR_DARK, nil);
	local icon = close:CreateTexture(nil, "ARTWORK");
	icon:SetTexture("Interface\\GameEditor\\Textures\\icon_close.tga");
	icon:SetTexCoord(0, 0.875, 0, 0.875);
	icon:SetWidth(12);
	icon:SetHeight(12);
	icon:SetPoint("CENTER", close, "CENTER", 0, 0);
	close:SetScript("OnEnter", function(self) NoggitBox_Set(self, NOGGIT_COLOR_MID, nil); end);
	close:SetScript("OnLeave", function(self) NoggitBox_Set(self, NOGGIT_COLOR_DARK, nil); end);
	close:SetScript("OnClick", function() GameEditor_HideWindow(window); end);

	local content = CreateFrame("Frame", NoggitUI_NewName("GameEditorWindowContent"), window);
	content:SetPoint("TOPLEFT", bar, "BOTTOMLEFT", 6, -6);
	content:SetPoint("BOTTOMRIGHT", window, "BOTTOMRIGHT", -6, resizable and 14 or 6);
	window.content = content;

	if ( resizable ) then
		window:SetResizable(true);
		window:SetMinResize(220, 140);
		local grip = CreateFrame("Button", NoggitUI_NewName("GameEditorWindowGrip"), window);
		grip:SetWidth(14);
		grip:SetHeight(14);
		grip:SetPoint("BOTTOMRIGHT", window, "BOTTOMRIGHT", -1, 1);
		local gripMark = NoggitUI_SolidTexture(grip, "ARTWORK", NOGGIT_COLOR_HOVER);
		gripMark:SetWidth(8);
		gripMark:SetHeight(8);
		gripMark:SetPoint("BOTTOMRIGHT", grip, "BOTTOMRIGHT", -2, 2);
		grip:SetScript("OnMouseDown", function() window:StartSizing("BOTTOMRIGHT"); end);
		grip:SetScript("OnMouseUp", function() window:StopMovingOrSizing(); end);
	end

	window.SetTitle = function(self, text)
		self.titleText:SetText(text);
	end;

	windows[window] = true;
	return window;
end

function GameEditor_ShowWindow(window)
	if ( window ) then
		window:Show();
		window:Raise();
	end
end

function GameEditor_HideWindow(window)
	if ( window ) then
		window:Hide();

		if ( window.onClose ) then
			window.onClose(window);
		end
	end
end

function GameEditor_ToggleWindow(window)
	if ( window and window:IsShown() ) then
		GameEditor_HideWindow(window);
	else
		GameEditor_ShowWindow(window);
	end
end

-- A vertical scroll area, returns the scroll frame and the child to put content in. Call scroll:UpdateRange() after changing the child height.
function GameEditor_CreateScrollFrame(parent)
	local scroll = CreateFrame("ScrollFrame", NoggitUI_NewName("GameEditorScroll"), parent);
	local child = CreateFrame("Frame", NoggitUI_NewName("GameEditorScrollChild"), scroll);
	child:SetWidth(10);
	child:SetHeight(10);
	scroll:SetScrollChild(child);
	scroll:EnableMouseWheel(true);

	local track = CreateFrame("Frame", NoggitUI_NewName("GameEditorScrollTrack"), scroll);
	track:SetWidth(8);
	track:SetFrameLevel(child:GetFrameLevel() + 10);
	track:SetPoint("TOPRIGHT", scroll, "TOPRIGHT", 0, 0);
	track:SetPoint("BOTTOMRIGHT", scroll, "BOTTOMRIGHT", 0, 0);
	track:EnableMouse(true);
	NoggitUI_SolidTexture(track, "BACKGROUND", NOGGIT_COLOR_DARK):SetAllPoints(track);
	local thumb = NoggitUI_SolidTexture(track, "ARTWORK", NOGGIT_COLOR_HOVER);
	thumb:SetWidth(8);
	scroll.track = track;

	local function Refresh()
		local range = scroll:GetVerticalScrollRange() or 0;
		local height = scroll:GetHeight() or 1;
		local value = scroll:GetVerticalScroll() or 0;

		if ( range <= 0 ) then
			track:Hide();
			return;
		end

		track:Show();
		local trackHeight = track:GetHeight() or height;
		local thumbHeight = max(16, trackHeight * height / (height + range));
		thumb:SetHeight(thumbHeight);
		thumb:ClearAllPoints();
		thumb:SetPoint("TOP", track, "TOP", 0, -(trackHeight - thumbHeight) * value / range);
	end

	local function SetScroll(value)
		local range = scroll:GetVerticalScrollRange() or 0;
		scroll:SetVerticalScroll(max(0, min(range, value)));
		Refresh();
	end

	scroll.SetScroll = function(_, value) SetScroll(value); end;
	scroll.UpdateRange = function()
		child:SetWidth(max(10, (scroll:GetWidth() or 10) - 10));
		scroll:UpdateScrollChildRect();
		SetScroll(scroll:GetVerticalScroll() or 0);
	end;

	scroll:SetScript("OnMouseWheel", function(self, delta)
		SetScroll((self:GetVerticalScroll() or 0) - delta * 40);
	end);
	scroll:SetScript("OnSizeChanged", function() scroll.UpdateRange(); end);
	scroll:SetScript("OnScrollRangeChanged", Refresh);

	local function ThumbTop()
		local range = scroll:GetVerticalScrollRange() or 0;
		local free = track:GetHeight() - thumb:GetHeight();
		return range > 0 and free * (scroll:GetVerticalScroll() or 0) / range or 0;
	end

	local function DragTo(y)
		local range = scroll:GetVerticalScrollRange() or 0;
		local free = max(1, track:GetHeight() - thumb:GetHeight());
		SetScroll((track:GetTop() - y - track.grab) / free * range);
	end

	track:SetScript("OnMouseDown", function(self)
		local _, y = NoggitUI_CursorIn(self);
		local offset = self:GetTop() - y - ThumbTop();

		if ( offset >= 0 and offset <= thumb:GetHeight() ) then
			self.grab = offset;
		else
			self.grab = thumb:GetHeight() / 2;
			DragTo(y);
		end

		self.dragging = true;
	end);
	track:SetScript("OnMouseUp", function(self) self.dragging = false; end);
	track:SetScript("OnHide", function(self) self.dragging = false; end);
	track:SetScript("OnUpdate", function(self)
		if ( not self.dragging ) then
			return;
		end

		if ( not EditorIsMouseButtonDown() ) then
			self.dragging = false;
			return;
		end

		local _, y = NoggitUI_CursorIn(self);
		DragTo(y);
	end);

	return scroll, child;
end

-- Console and command line ---------------------------------------------------------------------------

local function CreateConsole()
	consoleWindow = GameEditor_CreateWindow("GameEditorConsole", "Console", 560, 260, true);
	consoleWindow:ClearAllPoints();
	consoleWindow:SetPoint("BOTTOMLEFT", EditorStatusBar, "TOPLEFT", 60, 10);

	consoleFrame = CreateFrame("ScrollingMessageFrame", "GameEditorConsoleMessages", consoleWindow.content);
	consoleFrame:SetAllPoints(consoleWindow.content);
	consoleFrame:SetFontObject(NoggitFont);
	consoleFrame:SetJustifyH("LEFT");
	consoleFrame:SetMaxLines(500);
	consoleFrame:SetFading(false);
	consoleFrame:EnableMouseWheel(true);
	consoleFrame:SetScript("OnMouseWheel", function(self, delta)
		if ( delta > 0 ) then
			self:ScrollUp();
		else
			self:ScrollDown();
		end
	end);
end

function GameEditor_RunCommand(text)
	text = strtrim(text or "");

	if ( text == "" ) then
		return;
	end

	if ( commandBox ) then
		commandBox:AddHistoryLine(text);
	end

	GameEditor_Print("> "..text, 0.6, 0.8, 1);

	if ( string.sub(text, 1, 1) == "/" ) then
		local editBox = ChatFrameEditBox or (DEFAULT_CHAT_FRAME and DEFAULT_CHAT_FRAME.editBox);

		if ( editBox and ChatEdit_SendText ) then
			editBox:SetText(text);
			ChatEdit_SendText(editBox, 1);
			editBox:SetText("");
		end

		return;
	end

	if ( string.sub(text, 1, 1) ~= "." ) then
		text = "."..text;
	end

	SendChatMessage(text, "SAY");
end

local function CreateCommandBox()
	commandBox = NoggitLineEdit_Create(EditorStatusBar, 300);
	commandBox:SetHeight(18);
	commandBox:SetPoint("RIGHT", EditorStatusBar, "RIGHT", -4, 0);
	commandBox:SetAutoFocus(false);
	commandBox:SetMaxLetters(255);
	commandBox:SetHistoryLines(50);
	commandBox:SetAltArrowKeyMode(false);

	local hint = NoggitUI_FontString(commandBox, "Command (Enter)");
	hint:SetPoint("LEFT", commandBox, "LEFT", 6, 0);
	NoggitText_SetColor(hint, NOGGIT_COLOR_DISABLED);
	commandBox.hint = hint;

	commandBox:SetScript("OnEditFocusGained", function() hint:Hide(); end);
	commandBox:SetScript("OnEditFocusLost", function(self)
		if ( self:GetText() == "" ) then
			hint:Show();
		end
	end);
	commandBox:SetScript("OnEnterPressed", function(self)
		local text = self:GetText();
		self:SetText("");
		self:ClearFocus();
		GameEditor_RunCommand(text);
	end);
	commandBox:SetScript("OnEscapePressed", function(self)
		self:SetText("");
		self:ClearFocus();
	end);
end

-- Camera and editor settings ---------------------------------------------------------------------------

local function ApplyScale()
	local height;

	if ( EditorGetWindowSize ) then
		local _, windowHeight = EditorGetWindowSize();
		height = windowHeight;
	end

	if ( not height or height <= 0 ) then
		local resolution = GetCVar("gxResolution") or "";
		local _, _, parsed = string.find(resolution, "%d+x(%d+)");
		height = tonumber(parsed) or 768;
	end

	userScale = tonumber(GetCVar("editorUIScale") or "") or userScale or 1;
	userScale = max(0.5, min(3, userScale));

	if ( height ~= screenHeight or EditorParent.appliedUserScale ~= userScale ) then
		screenHeight = height;
		EditorParent.appliedUserScale = userScale;
		EditorParent:SetScale(768 / height * userScale);
	end
end

local function CameraRow(parent, y, label)
	local text = NoggitUI_FontString(parent, label);
	text:SetPoint("TOPLEFT", parent, "TOPLEFT", 4, y - 4);
	return text;
end

function GameEditor_IsFreeCamEnabled()
	return GetCVar("editorFreeCam") ~= "0";
end

-- Off keeps the normal camera, the character walks and the mouse wheel zooms while the editor is open.
function GameEditor_SetFreeCamEnabled(enabled)
	SetCVar("editorFreeCam", enabled and "1" or "0");

	if ( cameraWindow and cameraWindow.freeCam ) then
		NoggitCheckBox_SetChecked(cameraWindow.freeCam, enabled and true or false);
	end
end

local function CreateCameraWindow()
	cameraWindow = GameEditor_CreateWindow("GameEditorCameraWindow", "Camera and editor settings", 300, 290);
	local content = cameraWindow.content;

	CameraRow(content, 0, "Camera speed (yards a second)");
	cameraWindow.speed = NoggitSpinBox_Create(content, 90, 1, 4000, 5, 0, function(value)
		FreeCam_SetSpeed(value);
	end);
	cameraWindow.speed:SetPoint("TOPRIGHT", content, "TOPRIGHT", -4, 0);

	CameraRow(content, -30, "Keep the character this close (yards)");
	cameraWindow.lag = NoggitSpinBox_Create(content, 90, 0, 500, 5, 0, function(value)
		FreeCam_SetMaxLag(value);
	end);
	cameraWindow.lag:SetPoint("TOPRIGHT", content, "TOPRIGHT", -4, -30);

	cameraWindow.hide = NoggitCheckBox_Create(content, "Hide my character", true, function(_, checked)
		FreeCam_SetHideCharacter(checked);
	end);
	cameraWindow.hide:SetPoint("TOPLEFT", content, "TOPLEFT", 0, -62);

	cameraWindow.sync = NoggitCheckBox_Create(content, "Bring my character along with the camera", true, function(_, checked)
		FreeCam_SetSyncCharacter(checked);
	end);
	cameraWindow.sync:SetPoint("TOPLEFT", content, "TOPLEFT", 0, -88);

	local recenter = NoggitPushButton_Create(content, "Camera to character", 140, function()
		FreeCam_Recenter();
	end);
	recenter:SetPoint("TOPLEFT", content, "TOPLEFT", 4, -120);

	CameraRow(content, -156, "Editor UI scale");
	cameraWindow.scale = NoggitSpinBox_Create(content, 90, 0.5, 3, 0.1, 1, function(value)
		SetCVar("editorUIScale", tostring(value));
		ApplyScale();
	end);
	cameraWindow.scale:SetPoint("TOPRIGHT", content, "TOPRIGHT", -4, -156);

	cameraWindow.freeCam = NoggitCheckBox_Create(content, "Free camera while editing", true, function(_, checked)
		GameEditor_SetFreeCamEnabled(checked);
	end);
	cameraWindow.freeCam:SetPoint("TOPLEFT", content, "TOPLEFT", 0, -188);

	local toggle = NoggitUI_FontString(content, "");
	toggle:SetPoint("TOPLEFT", content, "TOPLEFT", 4, -220);
	NoggitText_SetColor(toggle, NOGGIT_COLOR_DISABLED);
	cameraWindow.toggleText = toggle;

	cameraWindow:SetScript("OnShow", function(self)
		local _, speed, hide, sync, lag = FreeCam_GetState();
		NoggitSpinBox_SetValue(self.speed, speed or 40, true);
		NoggitSpinBox_SetValue(self.lag, lag or 60, true);
		NoggitCheckBox_SetChecked(self.hide, hide);
		NoggitCheckBox_SetChecked(self.sync, sync);
		NoggitSpinBox_SetValue(self.scale, userScale, true);
		NoggitCheckBox_SetChecked(self.freeCam, GameEditor_IsFreeCamEnabled());
		self.toggleText:SetText(format("Toggle key: %s (CVar editorToggleKey)", GameEditor_GetToggleKey() or "F12"));
	end);
end

local function CreateKeysWindow()
	keysWindow = GameEditor_CreateWindow("GameEditorKeysWindow", "Keys", 520, 460, true);
	local scroll, child = GameEditor_CreateScrollFrame(keysWindow.content);
	scroll:SetAllPoints(keysWindow.content);
	keysWindow.scroll = scroll;
	keysWindow.child = child;
	keysWindow.lines = {};

	keysWindow:SetScript("OnShow", function(self)
		local rows = {};

		for _, key in ipairs(SHELL_KEYS) do
			table.insert(rows, key);
		end

		for _, workspace in ipairs(SortedWorkspaces()) do
			if ( workspace.keys and #workspace.keys > 0 ) then
				table.insert(rows, { workspace.title });

				for _, key in ipairs(workspace.keys) do
					table.insert(rows, key);
				end
			end
		end

		for index, row in ipairs(rows) do
			local line = self.lines[index];

			if ( not line ) then
				line = {};
				line.key = NoggitUI_FontString(self.child, "", "NoggitFontBold");
				line.text = NoggitUI_FontString(self.child, "");
				line.key:SetPoint("TOPLEFT", self.child, "TOPLEFT", 4, -(index - 1) * 20);
				line.text:SetPoint("TOPLEFT", self.child, "TOPLEFT", 180, -(index - 1) * 20);
				line.text:SetPoint("RIGHT", self.child, "RIGHT", -4, 0);
				self.lines[index] = line;
			end

			line.key:SetText(row[1] or "");
			line.text:SetText(row[2] or "");
			NoggitText_SetColor(line.key, row[2] and NOGGIT_COLOR_TEXT or NOGGIT_COLOR_GOLD);
			line.key:Show();
			line.text:Show();
		end

		for index = #rows + 1, #self.lines do
			self.lines[index].key:Hide();
			self.lines[index].text:Hide();
		end

		self.child:SetHeight(#rows * 20 + 4);
		self.scroll.UpdateRange();
	end);
end

-- Workspaces ---------------------------------------------------------------------------------------------

function GameEditor_RegisterWorkspace(definition)
	assert(definition and definition.key, "GameEditor_RegisterWorkspace needs a key");

	if ( workspaces[definition.key] ) then
		for index, existing in ipairs(ordered) do
			if ( existing.key == definition.key ) then
				table.remove(ordered, index);
				break;
			end
		end
	end

	definition.tools = definition.tools or {};
	definition.panels = {};
	workspaces[definition.key] = definition;
	table.insert(ordered, definition);

	if ( loaded ) then
		GameEditor_Print("Game editor: workspace "..definition.key.." registered after load, reload the UI to show it");
	end
end

function GameEditor_GetWorkspace()
	return activeWorkspace and activeWorkspace.key;
end

function GameEditor_GetActiveTool()
	return activeTool;
end

local function FindTool(key)
	if ( activeWorkspace ) then
		for _, tool in ipairs(activeWorkspace.tools) do
			if ( tool.key == key ) then
				return tool, activeWorkspace;
			end
		end
	end

	for _, workspace in ipairs(ordered) do
		for _, tool in ipairs(workspace.tools) do
			if ( tool.key == key ) then
				return tool, workspace;
			end
		end
	end
end

local function LayoutMainArea()
	EditorMainArea:ClearAllPoints();
	EditorMainArea:SetPoint("TOPLEFT", EditorMenuBar, "BOTTOMLEFT", 0, 0);

	if ( EditorToolDock:IsShown() ) then
		EditorMainArea:SetPoint("BOTTOMRIGHT", EditorToolDock, "BOTTOMLEFT", 0, 0);
	else
		EditorMainArea:SetPoint("BOTTOMRIGHT", EditorStatusBar, "TOPRIGHT", 0, 0);
	end
end

function GameEditor_SetToolPanelShown(shown)
	toolPanelShown = shown;

	if ( shown and not uiHidden ) then
		EditorToolDock:Show();
	else
		EditorToolDock:Hide();
	end

	LayoutMainArea();
end

local function BuildToolbar(workspace)
	if ( workspace.toolbar ) then
		return;
	end

	local bar = CreateFrame("Frame", NoggitUI_NewName("GameEditorToolbar"), EditorLeftToolbar);
	bar:SetPoint("TOPLEFT", EditorLeftToolbar, "TOPLEFT", 0, 0);
	bar:SetPoint("TOPRIGHT", EditorLeftToolbar, "TOPRIGHT", 0, 0);
	bar.buttons = {};

	local y = -TOOLBAR_PADDING;

	for _, tool in ipairs(workspace.tools) do
		local button = NoggitToolButton_Create(bar, tool.icon or 0xF8A0, tool.name, function()
			GameEditor_SelectTool(tool.key);
		end);
		button:SetPoint("TOP", bar, "TOP", 0, y);
		button.toolKey = tool.key;

		if ( tool.disabled ) then
			button:Disable();
		end

		table.insert(bar.buttons, button);
		y = y - button:GetHeight() - TOOLBAR_SPACING;
	end

	bar.height = -y + TOOLBAR_PADDING - TOOLBAR_SPACING;
	bar:SetHeight(bar.height);
	bar:Hide();
	workspace.toolbar = bar;
end

local function BuildViewbar(workspace)
	if ( workspace.viewbar or not workspace.viewItems ) then
		return;
	end

	local bar = CreateFrame("Frame", NoggitUI_NewName("GameEditorViewbar"), EditorViewToolbar);
	bar:SetAllPoints(EditorViewToolbar);
	bar.buttons = {};

	local x = TOOLBAR_PADDING;

	for _, item in ipairs(workspace.viewItems) do
		if ( item.separator ) then
			local line = NoggitUI_SolidTexture(bar, "ARTWORK", NOGGIT_COLOR_MID);
			line:SetWidth(2);
			line:SetPoint("TOPLEFT", bar, "TOPLEFT", x, -6);
			line:SetPoint("BOTTOMLEFT", bar, "BOTTOMLEFT", x, 6);
			x = x + 2 + TOOLBAR_SPACING;
		else
			local button = NoggitToolButton_Create(bar, item.icon, item.text, function()
				SafeCall(workspace.key, item.onClick);
				GameEditor_RefreshViewToolbar();
			end);
			button:SetPoint("LEFT", bar, "LEFT", x, 0);
			button.item = item;

			if ( not item.onClick ) then
				button:Disable();
			end

			table.insert(bar.buttons, button);
			x = x + button:GetWidth() + TOOLBAR_SPACING;
		end
	end

	bar.width = x - TOOLBAR_SPACING + TOOLBAR_PADDING;
	bar:Hide();
	workspace.viewbar = bar;
end

function GameEditor_RefreshViewToolbar()
	local bar = activeWorkspace and activeWorkspace.viewbar;

	if ( not bar ) then
		return;
	end

	for _, button in ipairs(bar.buttons) do
		if ( button.item.checked ) then
			NoggitToolButton_SetChecked(button, SafeCall(activeWorkspace.key, button.item.checked) and true or false);
		end
	end
end

local function BuildMenuStrip(workspace)
	if ( workspace.menuStrip or not workspace.menus ) then
		return;
	end

	local definitions = SafeCall(workspace.key, workspace.menus);
	local strip = CreateFrame("Frame", NoggitUI_NewName("GameEditorMenuStrip"), EditorMenuBar);
	strip:SetHeight(EditorMenuBar:GetHeight());
	strip:SetPoint("BOTTOMLEFT", shellMenuStrip, "BOTTOMRIGHT", 0, 0);
	strip:SetWidth(1);
	strip:Hide();

	if ( definitions and #definitions > 0 ) then
		NoggitMenuBar_Build(strip, definitions);
	end

	workspace.menuStrip = strip;
end

local function RefreshToolButtons()
	local bar = activeWorkspace and activeWorkspace.toolbar;

	if ( not bar ) then
		return;
	end

	for _, button in ipairs(bar.buttons) do
		NoggitToolButton_SetChecked(button, button.toolKey == activeTool);
	end
end

local function HidePanels(workspace)
	for _, panel in pairs(workspace.panels) do
		panel:Hide();
	end
end

function GameEditor_SelectTool(key)
	local tool, workspace = FindTool(key);

	if ( not tool or tool.disabled ) then
		return;
	end

	if ( workspace ~= activeWorkspace ) then
		GameEditor_SetWorkspace(workspace.key, key);
		return;
	end

	activeTool = key;
	workspace.lastTool = key;
	GameEditor_SetTool(key);

	HidePanels(workspace);

	if ( not tool.noPanel and tool.build and not workspace.panels[key] ) then
		local panel = SafeCall(workspace.key, tool.build, EditorToolDockContent);

		if ( panel ) then
			panel:ClearAllPoints();
			panel:SetPoint("TOPLEFT", EditorToolDockContent, "TOPLEFT", 0, 0);
			panel:SetPoint("BOTTOMRIGHT", EditorToolDockContent, "BOTTOMRIGHT", 0, 0);
			workspace.panels[key] = panel;
		end
	end

	local panel = workspace.panels[key];

	if ( panel ) then
		panel:Show();
	end

	EditorToolDockTitleText:SetText(tool.name or key);

	if ( tool.noPanel or not panel ) then
		GameEditor_SetToolPanelShown(false);
	else
		GameEditor_SetToolPanelShown(true);
	end

	RefreshToolButtons();
	SafeCall(workspace.key, workspace.onToolSelected, key);
end

-- Workspaces with a category share one tab that opens a list of them, the rest get a tab each.
local function TabEntries()
	local entries = {};
	local byCategory = {};

	for _, workspace in ipairs(SortedWorkspaces()) do
		local category = workspace.category;

		if ( category ) then
			local entry = byCategory[category];

			if ( not entry ) then
				entry = { title = category, workspaces = {} };
				byCategory[category] = entry;
				table.insert(entries, entry);
			end

			table.insert(entry.workspaces, workspace);
		else
			table.insert(entries, { title = workspace.title or workspace.key, workspace = workspace });
		end
	end

	return entries;
end

local function WorkspaceShortcut(workspace)
	for index, candidate in ipairs(SortedWorkspaces()) do
		if ( candidate == workspace ) then
			return index <= 9 and ("Ctrl+"..index) or nil;
		end
	end
end

local function WorkspaceMenuItem(workspace)
	return {
		text = workspace.title or workspace.key,
		shortcut = WorkspaceShortcut(workspace),
		checked = function() return activeWorkspace == workspace; end,
		onClick = function() GameEditor_SetWorkspace(workspace.key); end,
	};
end

local tabEntries = {};

local function RefreshWorkspaceTabs()
	local width = 0;

	for _, entry in ipairs(tabEntries) do
		local tab = entry.tab;
		local active = entry.workspace == activeWorkspace;

		if ( entry.workspaces ) then
			local shown;

			for _, workspace in ipairs(entry.workspaces) do
				if ( workspace == activeWorkspace ) then
					shown = workspace;
				end
			end

			active = shown ~= nil;
			tab.text:SetText(shown and (entry.title..": "..(shown.title or shown.key)) or entry.title);
			tab:SetWidth(tab.text:GetStringWidth() + 38);
		end

		NoggitBox_Set(tab, active and NOGGIT_COLOR_ACCENT or (tab.hover and NOGGIT_COLOR_HOVER or NOGGIT_COLOR_BASE), nil);
		width = width + tab:GetWidth() + 2;
	end

	EditorWorkspaceTabs:SetWidth(max(10, width));
end

local function BuildWorkspaceTabs()
	local previous;
	tabEntries = TabEntries();

	for _, entry in ipairs(tabEntries) do
		local tab = CreateFrame("Button", NoggitUI_NewName("GameEditorWorkspaceTab"), EditorWorkspaceTabs, "NoggitBoxTemplate");
		tab:SetHeight(22);
		tab.text = NoggitUI_FontString(tab, entry.title, nil, "CENTER");

		if ( entry.workspaces ) then
			tab.text:SetPoint("LEFT", tab, "LEFT", 12, 0);
			local caret = NoggitUI_CreateCaret(tab, 10);
			caret:SetOpen(true);
			caret:SetPoint("RIGHT", tab, "RIGHT", -10, 0);

			local items = {};

			for _, workspace in ipairs(entry.workspaces) do
				table.insert(items, WorkspaceMenuItem(workspace));
			end

			tab:SetScript("OnClick", function(self)
				NoggitDropdownMenu_Show("GameEditorCategory"..entry.title, items, self);
			end);
		else
			tab.text:SetPoint("CENTER", tab, "CENTER", 0, 0);
			tab:SetScript("OnClick", function()
				GameEditor_SetWorkspace(entry.workspace.key);
			end);
		end

		tab:SetWidth(tab.text:GetStringWidth() + 24);

		if ( previous ) then
			tab:SetPoint("LEFT", previous, "RIGHT", 2, 0);
		else
			tab:SetPoint("LEFT", EditorWorkspaceTabs, "LEFT", 0, 0);
		end

		tab:SetScript("OnEnter", function(self)
			self.hover = true;
			RefreshWorkspaceTabs();
		end);
		tab:SetScript("OnLeave", function(self)
			self.hover = false;
			RefreshWorkspaceTabs();
		end);

		entry.tab = tab;
		previous = tab;
	end

	RefreshWorkspaceTabs();
end

function GameEditor_SetWorkspace(key, toolKey)
	local workspace = workspaces[key];

	if ( not workspace ) then
		return;
	end

	NoggitMenu_CloseAll();
	NoggitTooltip_Hide();

	if ( activeWorkspace and activeWorkspace ~= workspace ) then
		local previous = activeWorkspace;
		SafeCall(previous.key, previous.onDeactivate);
		HidePanels(previous);

		if ( previous.toolbar ) then
			previous.toolbar:Hide();
		end

		if ( previous.viewbar ) then
			previous.viewbar:Hide();
		end

		if ( previous.menuStrip ) then
			previous.menuStrip:Hide();
		end

		if ( previous.main ) then
			previous.main:Hide();
		end
	end

	activeWorkspace = workspace;
	EditorToolDock:SetWidth(workspace.dockWidth or DOCK_WIDTH);

	BuildToolbar(workspace);
	BuildViewbar(workspace);
	BuildMenuStrip(workspace);

	workspace.toolbar:Show();
	EditorLeftToolbar:SetHeight(max(44, workspace.toolbar.height));

	if ( #workspace.tools == 0 ) then
		EditorLeftToolbar:Hide();
	elseif ( not uiHidden ) then
		EditorLeftToolbar:Show();
	end

	if ( workspace.viewbar ) then
		workspace.viewbar:Show();
		EditorViewToolbar:SetWidth(workspace.viewbar.width);

		if ( not uiHidden ) then
			EditorViewToolbar:Show();
		end
	else
		EditorViewToolbar:Hide();
	end

	if ( workspace.menuStrip ) then
		workspace.menuStrip:Show();
	end

	if ( workspace.buildMain and not workspace.main ) then
		workspace.main = SafeCall(workspace.key, workspace.buildMain, EditorMainArea);
	end

	if ( workspace.main ) then
		workspace.main:Show();
	end

	RefreshWorkspaceTabs();
	SafeCall(workspace.key, workspace.onActivate);

	local tool = toolKey or workspace.lastTool or (workspace.tools[1] and workspace.tools[1].key);

	if ( tool ) then
		GameEditor_SelectTool(tool);
	else
		activeTool = nil;
		GameEditor_SetTool("select");
		GameEditor_SetToolPanelShown(false);
	end

	GameEditor_RefreshViewToolbar();
end

-- Shell menus and keys -----------------------------------------------------------------------------------

local function BuildShellMenus()
	shellMenuStrip = CreateFrame("Frame", "GameEditorShellMenus", EditorMenuBar);
	shellMenuStrip:SetHeight(EditorMenuBar:GetHeight());
	shellMenuStrip:SetPoint("BOTTOMLEFT", EditorMenuBar, "BOTTOMLEFT", 0, 0);

	local separator = { separator = true };
	local workspaceItems = {};

	for _, entry in ipairs(TabEntries()) do
		if ( entry.workspaces ) then
			local items = {};

			for _, workspace in ipairs(entry.workspaces) do
				table.insert(items, WorkspaceMenuItem(workspace));
			end

			table.insert(workspaceItems, { text = entry.title, submenu = items });
		else
			table.insert(workspaceItems, WorkspaceMenuItem(entry.workspace));
		end
	end

	NoggitMenuBar_Build(shellMenuStrip, {
		{
			title = "Game Editor",
			{ text = "Workspace", submenu = workspaceItems },
			separator,
			{ text = "Camera and editor settings", onClick = function() GameEditor_ToggleWindow(cameraWindow); end },
			{ text = "Console", onClick = function() GameEditor_ToggleWindow(consoleWindow); end },
			{ text = "Keys", onClick = function() GameEditor_ToggleWindow(keysWindow); end },
			separator,
			{ text = "Toggle UI", shortcut = "Tab", onClick = function() GameEditor_SetUIHidden(not uiHidden); end },
			{ text = "Camera to character", onClick = function() FreeCam_Recenter(); end },
			{ text = "Reload UI", onClick = function() ReloadUI(); end },
			separator,
			{ text = "Leave the editor", shortcut = GameEditor_GetToggleKey() or "F12", onClick = function() GameEditor_SetActive(false); end },
		},
	});

	local width = 4;

	for _, child in ipairs({ shellMenuStrip:GetChildren() }) do
		width = width + child:GetWidth() + 4;
	end

	shellMenuStrip:SetWidth(width);
end

function GameEditor_SetUIHidden(hidden)
	uiHidden = hidden;
	NoggitMenu_CloseAll();
	NoggitTooltip_Hide();

	local frames = { EditorMenuBar, EditorStatusBar, EditorMainArea };

	for _, frame in ipairs(frames) do
		if ( hidden ) then
			frame:Hide();
		else
			frame:Show();
		end
	end

	if ( hidden ) then
		EditorLeftToolbar:Hide();
		EditorViewToolbar:Hide();
	else
		if ( activeWorkspace and #activeWorkspace.tools > 0 ) then
			EditorLeftToolbar:Show();
		end

		if ( activeWorkspace and activeWorkspace.viewbar ) then
			EditorViewToolbar:Show();
		end
	end

	for window in pairs(windows) do
		if ( hidden and window:IsShown() ) then
			window.hiddenByUI = true;
			window:Hide();
		elseif ( not hidden and window.hiddenByUI ) then
			window.hiddenByUI = nil;
			window:Show();
		end
	end

	GameEditor_SetToolPanelShown(toolPanelShown);
end

function GameEditor_IsUIHidden()
	return uiHidden;
end

-- Called from C++ for keys the active tool did not take, return true when handled.
function GameEditor_OnHotkey(key)
	if ( not loaded ) then
		return false;
	end

	if ( key == "TAB" ) then
		GameEditor_SetUIHidden(not uiHidden);
		return true;
	end

	if ( key == "ENTER" and commandBox and not uiHidden ) then
		commandBox:SetFocus();
		return true;
	end

	local _, _, number = string.find(key, "^CTRL%-(%d)$");

	if ( number ) then
		local workspace = SortedWorkspaces()[tonumber(number)];

		if ( workspace ) then
			GameEditor_SetWorkspace(workspace.key);
		end

		return true;
	end

	if ( activeWorkspace and activeWorkspace.onHotkey ) then
		local handled = SafeCall(activeWorkspace.key, activeWorkspace.onHotkey, key);

		if ( handled ) then
			return true;
		end
	end

	if ( key == "ESCAPE" ) then
		for window in pairs(windows) do
			if ( window:IsShown() ) then
				GameEditor_HideWindow(window);
				return true;
			end
		end
	end

	return false;
end

-- Status bar ----------------------------------------------------------------------------------------------

local function StatusCell(index)
	local cell = statusCells[index];

	if ( cell ) then
		return cell;
	end

	cell = CreateFrame("Frame", NoggitUI_NewName("GameEditorStatusCell"), EditorStatusBar);
	cell:SetHeight(18);

	if ( index == 1 ) then
		cell:SetPoint("LEFT", EditorStatusBar, "LEFT", 4, 0);
	else
		cell:SetPoint("LEFT", StatusCell(index - 1), "RIGHT", 4, 0);
	end

	local border = NoggitUI_SolidTexture(cell, "BORDER", NOGGIT_COLOR_MID);
	border:SetWidth(1);
	border:SetPoint("TOPLEFT", cell, "TOPLEFT", 0, 0);
	border:SetPoint("BOTTOMLEFT", cell, "BOTTOMLEFT", 0, 0);

	cell.text = NoggitUI_FontString(cell, "");
	cell.text:SetPoint("LEFT", cell, "LEFT", 5, 0);
	statusCells[index] = cell;
	return cell;
end

local function SetStatus(index, text)
	local cell = StatusCell(index);

	if ( not text or text == "" ) then
		cell:Hide();
		cell:SetWidth(1);
		return;
	end

	cell:Show();
	cell.text:SetText(text);
	cell:SetWidth(cell.text:GetStringWidth() + 10);
end

local function UpdateStatus(elapsed)
	local _, speed, _, _, _, x, y, z = FreeCam_GetState();
	local index = 1;

	if ( x ) then
		SetStatus(index, format("%.1f, %.1f, %.1f  (%d y/s)", x, y, z, speed or 0));
	else
		SetStatus(index, "");
	end

	index = index + 1;

	local zone = GetRealZoneText() or "";
	local subZone = GetSubZoneText() or "";
	SetStatus(index, subZone ~= "" and subZone ~= zone and (zone.." - "..subZone) or zone);
	index = index + 1;

	local kind, guid, entry, name = GameEditor_GetSelection();

	if ( kind ) then
		SetStatus(index, format("%s: %s (%s)", kind == "gameobject" and "Object" or "Creature", name or "?", tostring(entry or "?")));
	else
		SetStatus(index, "Nothing selected");
	end

	index = index + 1;

	fpsElapsed = fpsElapsed + elapsed;

	if ( fpsElapsed > 0.5 ) then
		fpsElapsed = 0;
		fpsText = format("%d fps", floor((GetFramerate() or 0) + 0.5));
	end

	SetStatus(index, fpsText);
	index = index + 1;

	if ( activeWorkspace and activeWorkspace.status ) then
		local cells = SafeCall(activeWorkspace.key, activeWorkspace.status);

		if ( type(cells) == "table" ) then
			for _, text in ipairs(cells) do
				SetStatus(index, text);
				index = index + 1;
			end
		end
	end

	for unused = index, #statusCells do
		SetStatus(unused, "");
	end
end

-- Server results become toasts so every workspace gets feedback for free.
local RESULT_EVENTS = {
	HOT_CREATURE_EDITOR_RESULT = true,
	HOT_GOB_EDITOR_RESULT = true,
	DBC_EDITOR_RESULT = true,
	WORLD_TABLE_RESULT = true,
};

function GameEditor_OnEvent(self, event, ...)
	if ( event == "GAME_EDITOR_MODE_CHANGED" ) then
		local active = ...;

		if ( active == 1 or active == true ) then
			NoggitMenu_CloseAll();
			self:Show();
			ApplyScale();

			if ( skipNextActivate ) then
				skipNextActivate = false;
			elseif ( activeWorkspace ) then
				SafeCall(activeWorkspace.key, activeWorkspace.onActivate);

				if ( activeTool ) then
					GameEditor_SetTool(activeTool);
				end
			end
		else
			if ( activeWorkspace ) then
				SafeCall(activeWorkspace.key, activeWorkspace.onDeactivate);
			end

			NoggitMenu_CloseAll();
			NoggitTooltip_Hide();

			if ( commandBox ) then
				commandBox:ClearFocus();
			end

			self:Hide();
		end

		return;
	end

	if ( event == "CHAT_MSG_SYSTEM" ) then
		if ( consoleFrame ) then
			consoleFrame:AddMessage((...), 1, 1, 0.4);
		end

		return;
	end

	if ( RESULT_EVENTS[event] ) then
		local ok = ...;
		local message;

		for index = select("#", ...), 2, -1 do
			local value = select(index, ...);

			if ( type(value) == "string" and value ~= "" ) then
				message = value;
				break;
			end
		end

		if ( message ) then
			GameEditor_Notify(message, (ok == 1 or ok == true) and "ok" or "error");
		end
	end
end

function GameEditor_OnLoad(self)
	ApplyScale();

	NoggitBox_Set(EditorMenuBar, NOGGIT_COLOR_BASE, nil);
	NoggitBox_Set(EditorStatusBar, NOGGIT_COLOR_BASE, nil);
	NoggitBox_Set(EditorToolDock, NOGGIT_COLOR_BASE, nil);
	NoggitBox_Set(EditorToolDockTitle, NOGGIT_COLOR_DARK, nil);
	NoggitBox_Set(EditorToolDockClose, NOGGIT_COLOR_DARK, nil);
	NoggitBox_Set(EditorExitButton, NOGGIT_COLOR_BASE, nil);
	NoggitBox_Set(EditorLeftToolbar, NOGGIT_COLOR_BASE, nil);
	NoggitBox_Set(EditorViewToolbar, NOGGIT_COLOR_BASE, nil);
	EditorToolDock:SetWidth(DOCK_WIDTH);

	self:RegisterEvent("GAME_EDITOR_MODE_CHANGED");
	self:RegisterEvent("CHAT_MSG_SYSTEM");

	for event in pairs(RESULT_EVENTS) do
		self:RegisterEvent(event);
	end

	CreateConsole();
	CreateCommandBox();
	CreateCameraWindow();
	CreateKeysWindow();
	BuildShellMenus();
	BuildWorkspaceTabs();

	loaded = true;

	for _, workspace in ipairs(SortedWorkspaces()) do
		SafeCall(workspace.key, workspace.onLoad);
	end

	local first = SortedWorkspaces()[1];

	if ( first ) then
		GameEditor_SetWorkspace(first.key);
	else
		GameEditor_SetToolPanelShown(false);
	end

	skipNextActivate = true;
	self:Show();
	GameEditor_Print(format("Game editor ready, %s leaves it. Game Editor > Keys lists the controls.", GameEditor_GetToggleKey() or "F12"));
end

function GameEditor_OnUpdate(self, elapsed)
	ApplyScale();
	NoggitMenu_OnUpdate(elapsed);
	LayoutToasts(elapsed);

	if ( activeWorkspace and activeWorkspace.onUpdate ) then
		SafeCall(activeWorkspace.key, activeWorkspace.onUpdate, elapsed);
	end

	if ( not uiHidden ) then
		UpdateStatus(elapsed);
	end
end
