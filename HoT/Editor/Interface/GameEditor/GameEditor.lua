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
local statusFeedback;
local toasts = {};
local windows = {};
local fpsText = "";
local fpsElapsed = 0;

local consoleWindow;
local consoleFrame;
local cameraWindow;
local keysWindow;

local MINI_MIN_WIDTH = 320;
local MINI_MIN_HEIGHT = 300;
local COLLAPSE_WIDTH = 24;
local COLLAPSE_SNAP = 48;
local MINI_MAIN_TRIM = 48;
local SECTION_STATUS_TIME = 0.25;

local miniMode = false;
local modeApplied = false;
local fullReturn;
local dockSplitter;
local miniTabs = {};
local sectionOrder = {};
local sectionsById = {};
local focusedSection;
local focusClock = 0;
local mouseWasDown = false;
local sectionStatusElapsed = 0;
local layout = { windows = {}, views = {}, sizes = {} };
local layoutText;
local layoutDirty = false;

local SectionsOf, SectionFor, OpenSection, RefreshMiniTabs;

-- The toggle keys are set in the game's Key Bindings menu under Game Editor.
local function ToggleKeyText(mini)
	local key;

	if ( mini ) then
		key = GameEditor_GetMiniToggleKey and GameEditor_GetMiniToggleKey();
	else
		key = GameEditor_GetToggleKey and GameEditor_GetToggleKey();
	end

	return key or "not bound";
end

local SHELL_KEYS = {
	{ function() return ToggleKeyText(false); end, "Open and close the game editor, change it in Key Bindings > Game Editor" },
	{ function() return ToggleKeyText(true); end, "Open the mini editor, a bar of every editor over the normal game, only Ctrl hotkeys reach the editor" },
	{ "Tab", "Hide or show the editor UI" },
	{ "Ctrl+1 .. Ctrl+9", "Switch workspace" },
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

-- The mini editor shows only its bar, every section opens in a window of its own.
local function ChromeHidden()
	return uiHidden or miniMode;
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

	NoggitFeedback_OnNotify(text, kind);

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

-- Layout -------------------------------------------------------------------------------------------

local function Round(value)
	return floor((value or 0) + 0.5);
end

local function LoadLayout()
	layout = { windows = {}, views = {}, sizes = {} };
	layoutText = GameEditor_LoadLayout and GameEditor_LoadLayout() or "";

	for entry in string.gmatch(layoutText, "[^\r\n]+") do
		local key, value = string.match(entry, "^(%w+)=(.*)$");

		if ( key == "ws" or key == "tool" or key == "miniws" or key == "open" or key == "focus" ) then
			layout[key] = value;
		elseif ( key == "miniopen" ) then
			layout.miniopen = value == "1";
		elseif ( key == "n" ) then
			local name, size = string.match(value, "^([%w_]+)/(%d+)$");

			if ( name ) then
				layout.sizes[name] = tonumber(size);
			end
		elseif ( key == "v" ) then
			local name, x, y, w, h = string.match(value, "^([%w_]+)/(%-?%d+)/(%-?%d+)/(%d+)/(%d+)$");

			if ( name ) then
				layout.views[name] = { tonumber(x), tonumber(y), tonumber(w), tonumber(h) };
			end
		elseif ( key == "w" ) then
			local name, x, y, w, h, shown = string.match(value, "^([%w_]+)/(%-?%d+)/(%-?%d+)/(%d+)/(%d+)/(%d)$");

			if ( name ) then
				layout.windows[name] = { tonumber(x), tonumber(y), tonumber(w), tonumber(h), shown == "1" };
			end
		end
	end
end

local function FrameRect(frame)
	local left, top = frame:GetLeft(), frame:GetTop();

	if ( not left or not top ) then
		return;
	end

	return Round(left), Round(top), Round(frame:GetWidth()), Round(frame:GetHeight());
end

local function PlaceAt(frame, left, top, width, height)
	frame:ClearAllPoints();
	frame:SetPoint("TOPLEFT", EditorParent, "BOTTOMLEFT", left, top);

	if ( width and height ) then
		frame:SetWidth(width);
		frame:SetHeight(height);
	end
end

local function WindowShown(window)
	return window:IsShown() or window.hiddenByUI or window.hiddenByWorkspace;
end

-- Anchors a window to the screen where it is now, so a layout change around it does not move it.
local function PinWindow(window)
	local left, top, width, height = FrameRect(window);

	if ( left ) then
		PlaceAt(window, left, top, window.resizable and width or nil, window.resizable and height or nil);
		window.pinned = true;
	end
end

local function SaveLayout()
	layoutDirty = false;

	if ( not loaded ) then
		return;
	end

	local parts = { "v1" };

	local fullKey, fullTool;

	-- The mini editor keeps the workspace the full editor goes back to.
	if ( miniMode and fullReturn ) then
		fullKey, fullTool = fullReturn.key, fullReturn.tool;
	elseif ( miniMode ) then
		fullKey, fullTool = layout.ws, layout.tool;
	elseif ( activeWorkspace ) then
		fullKey, fullTool = activeWorkspace.key, activeTool;
	end

	if ( fullKey ) then
		table.insert(parts, "ws="..fullKey);

		if ( fullTool ) then
			table.insert(parts, "tool="..fullTool);
		end
	end

	local open = {};

	for _, section in ipairs(sectionOrder) do
		if ( section.open ) then
			table.insert(open, section.id);
		end
	end

	table.insert(parts, "open="..table.concat(open, ","));

	if ( layout.focus ) then
		table.insert(parts, "focus="..layout.focus);
	end

	local views = {};

	for name in pairs(layout.views) do
		table.insert(views, name);
	end

	table.sort(views);

	for _, name in ipairs(views) do
		table.insert(parts, format("v=%s/%d/%d/%d/%d", name, unpack(layout.views[name])));
	end

	local sizes = {};

	for name in pairs(layout.sizes) do
		table.insert(sizes, name);
	end

	table.sort(sizes);

	for _, name in ipairs(sizes) do
		table.insert(parts, format("n=%s/%d", name, layout.sizes[name]));
	end

	for window in pairs(windows) do
		local name = not window.autoName and window:GetName();
		local shown = WindowShown(window) and true or false;

		if ( name and (window.pinned or window.everShown or shown) ) then
			local left, top, width, height = FrameRect(window);

			if ( left ) then
				layout.windows[name] = { left, top, width, height, shown };
			elseif ( layout.windows[name] ) then
				layout.windows[name][5] = shown;
			end
		elseif ( name and layout.windows[name] ) then
			layout.windows[name][5] = shown;
		end
	end

	local entries = {};

	for name, rect in pairs(layout.windows) do
		table.insert(entries, { name = name, rect = rect });
	end

	table.sort(entries, function(a, b)
		if ( a.rect[5] ~= b.rect[5] ) then
			return a.rect[5];
		end

		return a.name < b.name;
	end);

	for _, entry in ipairs(entries) do
		local rect = entry.rect;
		table.insert(parts, format("w=%s/%d/%d/%d/%d/%d", entry.name, rect[1], rect[2], rect[3], rect[4], rect[5] and 1 or 0));
	end

	local text = table.concat(parts, "\n").."\n";

	if ( text ~= layoutText and GameEditor_SaveLayout ) then
		layoutText = text;
		GameEditor_SaveLayout(text);
	end
end

-- A frame that cuts off whatever its content draws outside it, returns the clip frame and the content frame.
-- Model frames draw in the wrong place inside one, so panes with 3D previews stay unclipped.
function GameEditor_CreateClipFrame(parent)
	local clip = CreateFrame("ScrollFrame", NoggitUI_NewName("GameEditorClip"), parent);
	local content = CreateFrame("Frame", NoggitUI_NewName("GameEditorClipContent"), clip);
	content:SetWidth(1);
	content:SetHeight(1);
	clip:SetScrollChild(content);
	clip:SetScript("OnSizeChanged", function(self, width, height)
		content:SetWidth(max(1, width or self:GetWidth() or 1));
		content:SetHeight(max(1, height or self:GetHeight() or 1));
	end);
	clip.content = content;
	return clip, content;
end

-- Pane sizes set with a splitter, kept in the layout file by name.
function GameEditor_GetLayoutSize(name, default)
	return layout.sizes[name] or default;
end

function GameEditor_SetLayoutSize(name, size)
	size = Round(size);

	if ( layout.sizes[name] ~= size ) then
		layout.sizes[name] = size;
		layoutDirty = true;
	end
end

-- A thin vertical handle that sizes a pane by dragging. sign is 1 when the pane is left of the handle, -1 when right.
-- Dragging under options.collapse sets the size to 0, the handle stays so the pane can be dragged back out.
function GameEditor_CreateSplitter(parent, options)
	local handle = CreateFrame("Button", NoggitUI_NewName("GameEditorSplitter"), parent);
	handle:SetWidth(6);
	handle:EnableMouse(true);

	local line = NoggitUI_SolidTexture(handle, "OVERLAY", NOGGIT_COLOR_ACCENT);
	line:SetWidth(2);
	line:SetPoint("TOP", handle, "TOP", 0, 0);
	line:SetPoint("BOTTOM", handle, "BOTTOM", 0, 0);
	line:Hide();

	local rest = NoggitUI_SolidTexture(handle, "ARTWORK", NOGGIT_COLOR_HOVER);
	rest:SetWidth(2);
	rest:SetPoint("TOP", handle, "TOP", 0, 0);
	rest:SetPoint("BOTTOM", handle, "BOTTOM", 0, 0);
	rest:Hide();

	local function Clamp(size)
		local low = options.min and options.min() or 0;
		local high = options.max and options.max() or size;
		size = max(low, min(high, size));

		if ( options.collapse and size < options.collapse ) then
			size = 0;
		end

		return size;
	end

	local function Finish(self)
		self.dragging = false;

		if ( not self.hover ) then
			line:Hide();
		end
	end

	handle:SetScript("OnEnter", function(self)
		self.hover = true;
		line:Show();
		NoggitTooltip_Show(self, options.collapse and "Drag to resize, drag closed to collapse" or "Drag to resize");
	end);
	handle:SetScript("OnLeave", function(self)
		self.hover = false;
		NoggitTooltip_Hide();

		if ( not self.dragging ) then
			line:Hide();
		end
	end);
	handle:SetScript("OnMouseDown", function(self)
		self.startX = NoggitUI_CursorIn(self);
		self.startSize = options.get();
		self.dragging = true;
		line:Show();
	end);
	handle:SetScript("OnMouseUp", Finish);
	handle:SetScript("OnHide", Finish);
	handle:SetScript("OnUpdate", function(self)
		if ( not self.dragging ) then
			-- A collapsed pane keeps a visible line to grab.
			if ( options.collapse and options.get() < 1 ) then
				rest:Show();
				self:SetWidth(4);
			else
				rest:Hide();
				self:SetWidth(6);
			end

			return;
		end

		if ( not EditorIsMouseButtonDown() ) then
			Finish(self);
			return;
		end

		local x = NoggitUI_CursorIn(self);
		local size = Clamp(self.startSize + (x - self.startX) * (options.sign or 1));

		if ( size ~= self.lastSize ) then
			self.lastSize = size;
			options.set(size);
		end
	end);

	handle.Clamp = Clamp;
	return handle;
end

-- A window opened for the first time this session goes back where it was last time.
local function RestoreWindow(window)
	if ( window.restored ) then
		return;
	end

	window.restored = true;
	local saved = not window.autoName and layout.windows[window:GetName() or ""];

	if ( saved ) then
		PlaceAt(window, saved[1], saved[2], window.resizable and saved[3] or nil, window.resizable and saved[4] or nil);
		window.pinned = true;
	end
end

-- Windows ------------------------------------------------------------------------------------------

local function RaiseWindow(window)
	window:Raise();
end

local function EndWindowMove(window)
	window:StopMovingOrSizing();
	PinWindow(window);
	layoutDirty = true;
end

-- Every window resizes and clips its content (noClip is for model frames), fixed-size callers get 8px added for the grip row.
function GameEditor_CreateWindow(name, title, width, height, resizable, noClip)
	local window = CreateFrame("Frame", name or NoggitUI_NewName("GameEditorWindow"), EditorParent, "NoggitBoxTemplate");
	window.autoName = not name;
	window.resizable = true;
	window.chromeWidth = 12;
	window.chromeHeight = 46;
	width = width or 400;
	height = (height or 300) + (resizable and 0 or 8);
	window:SetFrameStrata("DIALOG");
	window:SetToplevel(true);
	window:EnableMouse(true);
	window:SetMovable(true);
	window:SetClampedToScreen(true);
	window:SetWidth(width);
	window:SetHeight(height);
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
	bar:SetScript("OnDragStop", function() EndWindowMove(window); end);
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

	local content;

	if ( not noClip ) then
		local clip;
		clip, content = GameEditor_CreateClipFrame(window);
		clip:SetPoint("TOPLEFT", bar, "BOTTOMLEFT", 6, -6);
		clip:SetPoint("BOTTOMRIGHT", window, "BOTTOMRIGHT", -6, 14);
		window.clip = clip;
	else
		content = CreateFrame("Frame", NoggitUI_NewName("GameEditorWindowContent"), window);
		content:SetPoint("TOPLEFT", bar, "BOTTOMLEFT", 6, -6);
		content:SetPoint("BOTTOMRIGHT", window, "BOTTOMRIGHT", -6, 14);
	end

	window.content = content;

	window:SetResizable(true);
	window:SetMinResize(min(width, 220), min(height, 100));
	local grip = CreateFrame("Button", NoggitUI_NewName("GameEditorWindowGrip"), window);
	grip:SetWidth(14);
	grip:SetHeight(14);
	grip:SetPoint("BOTTOMRIGHT", window, "BOTTOMRIGHT", -1, 1);
	local gripMark = NoggitUI_SolidTexture(grip, "ARTWORK", NOGGIT_COLOR_HOVER);
	gripMark:SetWidth(8);
	gripMark:SetHeight(8);
	gripMark:SetPoint("BOTTOMRIGHT", grip, "BOTTOMRIGHT", -2, 2);
	grip:SetScript("OnMouseDown", function() window:StartSizing("BOTTOMRIGHT"); end);
	grip:SetScript("OnMouseUp", function() EndWindowMove(window); end);
	grip:SetFrameLevel(window:GetFrameLevel() + 10);

	window.feedbackLine = NoggitUI_FontString(window, "");
	window.feedbackLine:SetHeight(14);
	window.feedbackLine:SetPoint("BOTTOMLEFT", window, "BOTTOMLEFT", 8, 0);
	window.feedbackLine:SetPoint("BOTTOMRIGHT", window, "BOTTOMRIGHT", -18, 0);

	window.SetTitle = function(self, text)
		self.titleText:SetText(text);
	end;

	windows[window] = true;
	return window;
end

function GameEditor_ShowWindow(window)
	if ( window ) then
		RestoreWindow(window);
		window.hiddenByWorkspace = nil;
		window.everShown = true;
		window:Show();
		window:Raise();
		layoutDirty = true;
	end
end

function GameEditor_HideWindow(window)
	if ( window ) then
		window:Hide();
		window.hiddenByWorkspace = nil;
		layoutDirty = true;

		if ( window.onClose ) then
			window.onClose(window);
		end
	end
end

-- Windows of one workspace hide while another is shown and come back with it.
function GameEditor_SetWindowWorkspace(window, key)
	if ( window ) then
		window.workspace = key;
	end
end

local function HideWorkspaceWindows(workspace)
	for window in pairs(windows) do
		if ( window.workspace == workspace.key and window:IsShown() ) then
			window:Hide();
			window.hiddenByWorkspace = true;
		end
	end
end

local function ShowWorkspaceWindows(workspace)
	for window in pairs(windows) do
		if ( window.workspace == workspace.key and window.hiddenByWorkspace ) then
			window.hiddenByWorkspace = nil;

			if ( uiHidden ) then
				window.hiddenByUI = true;
			else
				window:Show();
			end
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

-- Camera and editor settings ---------------------------------------------------------------------------

function GameEditor_RecenterCamera()
	if ( not FreeCam_GetState() ) then
		GameEditor_Notify("The free camera is off, the view already follows your character", "info");
		return;
	end

	FreeCam_Recenter();
end

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

	local recenter = NoggitPushButton_Create(content, "Camera to character", 140, GameEditor_RecenterCamera);
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
		self.toggleText:SetText(format("Toggle keys: %s full, %s mini (Key Bindings > Game Editor)", ToggleKeyText(false), ToggleKeyText(true)));
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
			table.insert(rows, type(key[1]) == "function" and { key[1](), key[2] } or key);
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

-- mainMinWidth is a number or a function: how much of the main area its content uses, the gap included.
local function MainMinWidth(workspace)
	local width = workspace.mainMinWidth;

	if ( type(width) == "function" ) then
		width = SafeCall(workspace.key, width);
	end

	return width or 0;
end

-- The dock keeps its dragged width while there is room, then shrinks so the gap stays and collapses at zero.
local function DockWidth(workspace)
	local wanted = layout.sizes["dock_"..workspace.key] or workspace.dockWidth or DOCK_WIDTH;
	local room = (EditorFrameRect:GetWidth() or 0) - MainMinWidth(workspace);
	return max(0, min(wanted, room));
end

local function LayoutShell()
	local top = EditorMenuBar;

	EditorLeftToolbar:ClearAllPoints();
	EditorLeftToolbar:SetPoint("TOPLEFT", top, "BOTTOMLEFT", 5, -5);

	EditorToolDock:ClearAllPoints();
	EditorToolDock:SetPoint("TOPRIGHT", top, "BOTTOMRIGHT", 0, 0);
	EditorToolDock:SetPoint("BOTTOMRIGHT", EditorStatusBar, "TOPRIGHT", 0, 0);

	local width = activeWorkspace and DockWidth(activeWorkspace) or DOCK_WIDTH;
	EditorToolDock:SetWidth(max(1, width));

	if ( width < COLLAPSE_WIDTH ) then
		EditorToolDock:Hide();
	elseif ( toolPanelShown and not ChromeHidden() ) then
		EditorToolDock:Show();
	end

	-- The handle stays while the dock is only collapsed, not when the tool has no dock or the UI is hidden.
	if ( dockSplitter ) then
		if ( not activeWorkspace or not toolPanelShown or ChromeHidden() ) then
			dockSplitter:Hide();
		else
			dockSplitter:Show();
		end
	end

	EditorMainArea:ClearAllPoints();
	EditorMainArea:SetPoint("TOPLEFT", top, "BOTTOMLEFT", 0, 0);

	if ( EditorToolDock:IsShown() ) then
		EditorMainArea:SetPoint("BOTTOMRIGHT", EditorToolDock, "BOTTOMLEFT", 0, 0);
	else
		EditorMainArea:SetPoint("BOTTOMRIGHT", EditorStatusBar, "TOPRIGHT", 0, 0);
	end
end

function GameEditor_SetToolPanelShown(shown)
	toolPanelShown = shown;

	if ( shown and not ChromeHidden() ) then
		EditorToolDock:Show();
	else
		EditorToolDock:Hide();
	end

	LayoutShell();
end

-- Shows the shell frames the mode and the Tab toggle call for.
local function ApplyChrome()
	local hidden = ChromeHidden();

	if ( uiHidden ) then
		EditorMenuBar:Hide();
	else
		EditorMenuBar:Show();
	end

	for _, frame in ipairs({ EditorStatusBar, EditorMainArea }) do
		if ( hidden ) then
			frame:Hide();
		else
			frame:Show();
		end
	end

	if ( not hidden and activeWorkspace and (activeWorkspace.visibleTools or 0) > 0 ) then
		EditorLeftToolbar:Show();
	else
		EditorLeftToolbar:Hide();
	end

	if ( not hidden and activeWorkspace and activeWorkspace.viewbar ) then
		EditorViewToolbar:Show();
	else
		EditorViewToolbar:Hide();
	end

	GameEditor_SetToolPanelShown(toolPanelShown);
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
		button.tool = tool;

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

-- Tools that work on the world (move, rotate, waypoints) are left out of the mini editor.
local function MiniTools(workspace)
	local tools = {};

	for _, tool in ipairs(workspace.tools) do
		if ( not tool.world ) then
			table.insert(tools, tool);
		end
	end

	return tools;
end

local function LayoutToolbar(workspace)
	local bar = workspace.toolbar;
	local y = -TOOLBAR_PADDING;
	local count = 0;

	for _, button in ipairs(bar.buttons) do
		if ( miniMode and button.tool.world ) then
			button:Hide();
		else
			button:Show();
			button:ClearAllPoints();
			button:SetPoint("TOP", bar, "TOP", 0, y);
			y = y - button:GetHeight() - TOOLBAR_SPACING;
			count = count + 1;
		end
	end

	bar.height = -y + TOOLBAR_PADDING - TOOLBAR_SPACING;
	bar:SetHeight(max(1, bar.height));
	workspace.visibleTools = count;

	if ( workspace == activeWorkspace ) then
		EditorLeftToolbar:SetHeight(max(44, bar.height));
	end
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

-- The frames a workspace builds into, clipped unless noClip. trim slides inner left to hide the toolbar gutter.
local function NewHost(workspace, area)
	local host = {};

	if ( workspace.noClip ) then
		host.outer = CreateFrame("Frame", NoggitUI_NewName("GameEditorHost"), area);
		host.content = host.outer;
	else
		host.outer, host.content = GameEditor_CreateClipFrame(area);
	end

	host.inner = CreateFrame("Frame", NoggitUI_NewName("GameEditorHostInner"), host.content);
	host.inner:SetAllPoints(host.content);
	host.outer:SetAllPoints(area);
	host.area = area;
	return host;
end

local function AttachHost(host, area, trim)
	if ( host.outer:GetParent() ~= area ) then
		host.outer:SetParent(area);
	end

	host.outer:ClearAllPoints();
	host.outer:SetAllPoints(area);
	host.inner:ClearAllPoints();
	host.inner:SetPoint("TOPLEFT", host.content, "TOPLEFT", -(trim or 0), 0);
	host.inner:SetPoint("BOTTOMRIGHT", host.content, "BOTTOMRIGHT", 0, 0);
	host.outer:Show();
	host.area = area;
end

local function MainHost(workspace)
	workspace.mainHost = workspace.mainHost or NewHost(workspace, EditorMainArea);
	return workspace.mainHost;
end

-- Every place that shows a workspace's tool panels has its own host, "full" for the shell dock and one per mini section.
local function DockHost(workspace, key)
	workspace.dockHosts = workspace.dockHosts or {};
	workspace.dockHosts[key] = workspace.dockHosts[key] or NewHost(workspace, EditorToolDockContent);
	return workspace.dockHosts[key];
end

local function BuildMain(workspace)
	if ( workspace.buildMain and not workspace.main ) then
		workspace.main = SafeCall(workspace.key, workspace.buildMain, MainHost(workspace).inner);
	end

	if ( workspace.main ) then
		workspace.main:Show();
	end
end

local function HidePanels(workspace)
	for _, panel in pairs(workspace.panels) do
		panel:Hide();
	end
end

-- Several tools can share one panel, so a panel moves into whichever host shows it.
local function ShowPanel(workspace, tool, host)
	local panel = workspace.panels[tool.key];

	if ( not panel and tool.build and not tool.noPanel ) then
		panel = SafeCall(workspace.key, tool.build, host.inner);
		workspace.panels[tool.key] = panel;
	end

	if ( panel ) then
		if ( panel:GetParent() ~= host.inner ) then
			panel:SetParent(host.inner);
		end

		panel:ClearAllPoints();
		panel:SetPoint("TOPLEFT", host.inner, "TOPLEFT", 0, 0);
		panel:SetPoint("BOTTOMRIGHT", host.inner, "BOTTOMRIGHT", 0, 0);
		panel:Show();
	end

	return panel;
end

-- Hides what a workspace shows in the full editor's shell.
local function HideFullUI(workspace)
	HidePanels(workspace);

	for _, field in ipairs({ "toolbar", "viewbar", "menuStrip" }) do
		if ( workspace[field] ) then
			workspace[field]:Hide();
		end
	end

	if ( workspace.mainHost and workspace.mainHost.area == EditorMainArea ) then
		workspace.mainHost.outer:Hide();
	end

	if ( workspace.dockHosts and workspace.dockHosts.full ) then
		workspace.dockHosts.full.outer:Hide();
	end
end

-- A workspace is live while it shows anywhere. Closing the editor suspends it, reopening runs onActivate again.
local function Activate(workspace)
	if ( workspace.live and not workspace.suspended ) then
		return;
	end

	workspace.live = true;
	workspace.suspended = nil;
	SafeCall(workspace.key, workspace.onActivate);
	ShowWorkspaceWindows(workspace);
end

local function Deactivate(workspace, closing)
	if ( not workspace.live ) then
		return;
	end

	local suspended = workspace.suspended;
	workspace.live = nil;
	workspace.suspended = nil;

	if ( not suspended ) then
		SafeCall(workspace.key, workspace.onDeactivate, closing);
	end

	HideWorkspaceWindows(workspace);
end

local function Suspend()
	for _, workspace in ipairs(ordered) do
		if ( workspace.live and not workspace.suspended ) then
			SafeCall(workspace.key, workspace.onDeactivate, true);
			workspace.suspended = true;
		end
	end
end

local function Resume()
	for _, workspace in ipairs(ordered) do
		if ( workspace.live and workspace.suspended ) then
			workspace.suspended = nil;
			SafeCall(workspace.key, workspace.onActivate);
		end
	end

	GameEditor_SetTool(activeTool or "select");
end

-- True while the workspace shows, in the full editor or in any mini window.
function GameEditor_IsWorkspaceOpen(key)
	local workspace = workspaces[key];
	return (workspace and workspace.live and not workspace.suspended) and true or false;
end

function GameEditor_SelectTool(key)
	local tool, workspace = FindTool(key);

	if ( not tool or tool.disabled ) then
		return;
	end

	if ( miniMode or workspace ~= activeWorkspace ) then
		GameEditor_SetWorkspace(workspace.key, key);
		return;
	end

	activeTool = key;
	workspace.lastTool = key;
	GameEditor_SetTool(key);

	HidePanels(workspace);
	local panel = ShowPanel(workspace, tool, DockHost(workspace, "full"));
	EditorToolDockTitleText:SetText(tool.name or key);
	GameEditor_SetToolPanelShown(panel ~= nil and not tool.noPanel);
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

local modeMenuItems = {};

local function WorkspaceMenuItem(workspace)
	local item = {
		text = workspace.title or workspace.key,
		shortcut = WorkspaceShortcut(workspace),
		checked = function() return activeWorkspace == workspace; end,
		onClick = function() GameEditor_SetWorkspace(workspace.key); end,
		fullOnly = workspace.fullOnly,
	};

	if ( workspace.fullOnly and modeMenuItems ) then
		table.insert(modeMenuItems, item);
	end

	return item;
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

	if ( miniMode and workspace.fullOnly ) then
		GameEditor_Notify(format("%s needs the full editor (%s)", workspace.title or workspace.key, ToggleKeyText(false)), "info");
		return;
	end

	if ( miniMode ) then
		local section = SectionFor(workspace, toolKey);

		if ( section ) then
			OpenSection(section);
		end

		return;
	end

	NoggitMenu_CloseAll();
	NoggitTooltip_Hide();

	if ( activeWorkspace and activeWorkspace ~= workspace ) then
		local previous = activeWorkspace;
		Deactivate(previous);
		HideFullUI(previous);
	end

	activeWorkspace = workspace;
	EditorToolDock:SetWidth(workspace.dockWidth or DOCK_WIDTH);

	BuildToolbar(workspace);
	BuildViewbar(workspace);
	BuildMenuStrip(workspace);

	workspace.toolbar:Show();
	LayoutToolbar(workspace);

	if ( workspace.viewbar ) then
		workspace.viewbar:Show();
		EditorViewToolbar:SetWidth(workspace.viewbar.width);
	end

	if ( workspace.menuStrip ) then
		workspace.menuStrip:Show();
	end

	if ( workspace.buildMain ) then
		AttachHost(MainHost(workspace), EditorMainArea, 0);
		BuildMain(workspace);
	end

	AttachHost(DockHost(workspace, "full"), EditorToolDockContent, 0);
	RefreshWorkspaceTabs();
	Activate(workspace);

	local tool = toolKey or workspace.lastTool or (workspace.tools[1] and workspace.tools[1].key);

	if ( tool ) then
		GameEditor_SelectTool(tool);
	else
		activeTool = nil;
		GameEditor_SetTool("select");
		GameEditor_SetToolPanelShown(false);
	end

	GameEditor_RefreshViewToolbar();
	ApplyChrome();
	layoutDirty = true;
end

-- Mini editor ---------------------------------------------------------------------------------------------

function GameEditor_IsMiniMode()
	return miniMode;
end

function GameEditor_CurrentToggleKey()
	return ToggleKeyText(miniMode);
end

-- A workspace with several mini tools lists each one on the bar, the item generator next to the item editor.
local function MiniEntries(workspace)
	local tools = MiniTools(workspace);
	local entries = {};

	if ( #tools <= 1 ) then
		table.insert(entries, { workspace = workspace, tool = tools[1] and tools[1].key, title = workspace.title or workspace.key });
	else
		for _, tool in ipairs(tools) do
			table.insert(entries, { workspace = workspace, tool = tool.key, title = tool.name or tool.key, split = true });
		end
	end

	return entries;
end

local function ToolOf(workspace, key)
	for _, tool in ipairs(workspace.tools) do
		if ( tool.key == key ) then
			return tool;
		end
	end
end

-- Each bar entry is a section with a window of its own, a tool sets sectionMain to fill the main area itself.
function SectionsOf(workspace)
	if ( workspace.sections ) then
		return workspace.sections;
	end

	workspace.sections = {};

	if ( workspace.fullOnly ) then
		return workspace.sections;
	end

	for _, entry in ipairs(MiniEntries(workspace)) do
		local id = workspace.key;

		if ( entry.split ) then
			id = string.gsub(workspace.key.."_"..entry.tool, "[^%w_]", "_");
		end

		local tool = entry.tool and ToolOf(workspace, entry.tool);
		local category = workspace.category;
		local section = {
			id = id,
			workspace = workspace,
			tool = entry.tool,
			title = entry.title,
			split = entry.split,
			fullTitle = category and category ~= entry.title and (category..": "..entry.title) or entry.title,
			sectionMain = tool and tool.sectionMain,
		};

		section.hasMain = (section.sectionMain or workspace.buildMain) and true or false;
		section.hasPanel = (tool and tool.build and not tool.noPanel) and true or false;
		table.insert(workspace.sections, section);
		table.insert(sectionOrder, section);
		sectionsById[id] = section;
	end

	return workspace.sections;
end

local function AnyOpen(workspace)
	for _, section in ipairs(SectionsOf(workspace)) do
		if ( section.open ) then
			return true;
		end
	end

	return false;
end

local function LatestOpen()
	local best;

	for _, section in ipairs(sectionOrder) do
		if ( section.open and (not best or (section.focusTime or 0) > (best.focusTime or 0)) ) then
			best = section;
		end
	end

	return best;
end

-- The section a tool key opens, a key without its own section goes to the open one or the last one used.
function SectionFor(workspace, toolKey)
	local list = SectionsOf(workspace);

	if ( #list <= 1 ) then
		return list[1];
	end

	local open, last;

	for _, section in ipairs(list) do
		if ( toolKey and section.tool == toolKey ) then
			return section;
		end

		if ( section.open and (not open or (section.focusTime or 0) > (open.focusTime or 0)) ) then
			open = section;
		end

		if ( section.tool == workspace.lastTool ) then
			last = section;
		end
	end

	return open or last or list[1];
end

local function ScreenSize()
	return EditorParent:GetWidth() or 1024, EditorParent:GetHeight() or 768;
end

local function SectionMainMin(section)
	return max(0, MainMinWidth(section.workspace) - MINI_MAIN_TRIM);
end

local function SectionDockWidth(section)
	local workspace = section.workspace;
	local wanted = layout.sizes["dock_"..section.id] or layout.sizes["dock_"..workspace.key] or workspace.dockWidth or DOCK_WIDTH;
	local room = (section.window.body:GetWidth() or 0) - SectionMainMin(section);
	return max(0, min(wanted, room));
end

local function LayoutSection(section)
	local window = section.window;

	if ( not window ) then
		return;
	end

	local body, main, dock, splitter = window.body, window.mainArea, window.dockArea, window.splitter;
	main:ClearAllPoints();
	dock:ClearAllPoints();

	if ( not section.hasMain ) then
		main:Hide();
		splitter:Hide();
		dock:SetAllPoints(body);
		dock:Show();
		return;
	end

	main:Show();
	main:SetPoint("TOPLEFT", body, "TOPLEFT", 0, 0);

	if ( not section.hasPanel ) then
		dock:Hide();
		splitter:Hide();
		main:SetPoint("BOTTOMRIGHT", body, "BOTTOMRIGHT", 0, 0);
		return;
	end

	local width = SectionDockWidth(section);
	dock:SetPoint("TOPRIGHT", body, "TOPRIGHT", 0, 0);
	dock:SetPoint("BOTTOMRIGHT", body, "BOTTOMRIGHT", 0, 0);
	splitter:Show();

	if ( width < COLLAPSE_WIDTH ) then
		dock:SetWidth(1);
		dock:Hide();
		main:SetPoint("BOTTOMRIGHT", body, "BOTTOMRIGHT", 0, 0);
	else
		dock:SetWidth(width);
		dock:Show();
		main:SetPoint("BOTTOMRIGHT", dock, "BOTTOMLEFT", 0, 0);
	end
end

local function EndSectionMove(section)
	local window = section.window;
	window:StopMovingOrSizing();
	local left, top, width, height = FrameRect(window);

	if ( left ) then
		layout.views[section.id] = { left, top, width, height };
		PlaceAt(window, left, top, width, height);
	end

	layoutDirty = true;
end

local CloseSection;

local function SectionWindow(section)
	if ( section.window ) then
		return section.window;
	end

	local name = "GameEditorSection_"..section.id;
	local window = CreateFrame("Frame", name, EditorParent, "NoggitBoxTemplate");
	window:SetFrameStrata("MEDIUM");
	window:SetToplevel(true);
	window:EnableMouse(true);
	window:SetMovable(true);
	window:SetResizable(true);
	window:SetClampedToScreen(true);
	window:SetMinResize(MINI_MIN_WIDTH, MINI_MIN_HEIGHT);
	window:Hide();
	NoggitBox_Set(window, NOGGIT_COLOR_BASE, NOGGIT_COLOR_MID);
	window.editorSection = section;
	section.window = window;

	local bar = CreateFrame("Frame", name.."Bar", window, "NoggitBoxTemplate");
	bar:SetHeight(24);
	bar:SetPoint("TOPLEFT", window, "TOPLEFT", 0, 0);
	bar:SetPoint("TOPRIGHT", window, "TOPRIGHT", 0, 0);
	bar:EnableMouse(true);
	bar:RegisterForDrag("LeftButton");
	bar:SetScript("OnDragStart", function() window:StartMoving(); end);
	bar:SetScript("OnDragStop", function() EndSectionMove(section); end);
	NoggitBox_Set(bar, NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID);

	local title = NoggitUI_FontString(bar, section.fullTitle);
	title:SetPoint("LEFT", bar, "LEFT", 10, 0);
	title:SetPoint("RIGHT", bar, "RIGHT", -40, 0);

	local close = CreateFrame("Button", name.."Close", bar, "NoggitBoxTemplate");
	close:SetWidth(32);
	close:SetHeight(24);
	close:SetPoint("RIGHT", bar, "RIGHT", 0, 0);
	NoggitBox_Set(close, NOGGIT_COLOR_DARK, nil);
	local icon = close:CreateTexture(nil, "ARTWORK");
	icon:SetTexture("Interface\\GameEditor\\Textures\\icon_close.tga");
	icon:SetTexCoord(0, 0.875, 0, 0.875);
	icon:SetWidth(12);
	icon:SetHeight(12);
	icon:SetPoint("CENTER", close, "CENTER", 0, 0);
	close:SetScript("OnEnter", function(self)
		NoggitBox_Set(self, NOGGIT_COLOR_MID, nil);
		NoggitTooltip_Show(self, "Close this editor, the bar and the other windows stay");
	end);
	close:SetScript("OnLeave", function(self)
		NoggitBox_Set(self, NOGGIT_COLOR_DARK, nil);
		NoggitTooltip_Hide();
	end);
	close:SetScript("OnClick", function() CloseSection(section); end);

	local foot = CreateFrame("Frame", name.."Foot", window);
	foot:SetHeight(18);
	foot:SetPoint("BOTTOMLEFT", window, "BOTTOMLEFT", 0, 0);
	foot:SetPoint("BOTTOMRIGHT", window, "BOTTOMRIGHT", 0, 0);

	window.statusText = NoggitUI_FontString(foot, "");
	window.statusText:SetPoint("LEFT", foot, "LEFT", 8, 0);
	window.feedbackLine = NoggitUI_FontString(foot, "");
	window.feedbackLine:SetPoint("LEFT", window.statusText, "RIGHT", 12, 0);
	window.feedbackLine:SetPoint("RIGHT", foot, "RIGHT", -18, 0);

	local body = CreateFrame("Frame", name.."Body", window);
	body:SetPoint("TOPLEFT", bar, "BOTTOMLEFT", 1, 0);
	body:SetPoint("BOTTOMRIGHT", foot, "TOPRIGHT", -1, 0);
	window.body = body;

	window.mainArea = CreateFrame("Frame", name.."Main", body);
	NoggitUI_SolidTexture(window.mainArea, "BACKGROUND", NOGGIT_COLOR_DARK):SetAllPoints(window.mainArea);
	window.dockArea = CreateFrame("Frame", name.."Dock", body);

	window.splitter = GameEditor_CreateSplitter(body, {
		sign = -1,
		collapse = COLLAPSE_SNAP,
		get = function() return window.dockArea:IsShown() and window.dockArea:GetWidth() or 0; end,
		set = function(width)
			GameEditor_SetLayoutSize("dock_"..section.id, width);
			LayoutSection(section);
		end,
		max = function() return max(0, (body:GetWidth() or 0) - SectionMainMin(section)); end,
	});
	window.splitter:SetPoint("TOPLEFT", window.dockArea, "TOPLEFT", -3, 0);
	window.splitter:SetPoint("BOTTOMLEFT", window.dockArea, "BOTTOMLEFT", -3, 0);
	window.splitter:SetFrameLevel(window:GetFrameLevel() + 40);

	local grip = CreateFrame("Button", name.."Grip", window);
	grip:SetWidth(14);
	grip:SetHeight(14);
	grip:SetPoint("BOTTOMRIGHT", window, "BOTTOMRIGHT", -1, 1);
	local mark = NoggitUI_SolidTexture(grip, "ARTWORK", NOGGIT_COLOR_HOVER);
	mark:SetWidth(8);
	mark:SetHeight(8);
	mark:SetPoint("BOTTOMRIGHT", grip, "BOTTOMRIGHT", -2, 2);
	grip:SetScript("OnMouseDown", function() window:StartSizing("BOTTOMRIGHT"); end);
	grip:SetScript("OnMouseUp", function() EndSectionMove(section); end);
	grip:SetFrameLevel(window:GetFrameLevel() + 45);

	window:SetScript("OnSizeChanged", function() LayoutSection(section); end);
	return window;
end

local function OpenCount()
	local count = 0;

	for _, section in ipairs(sectionOrder) do
		if ( section.window and section.window:IsShown() ) then
			count = count + 1;
		end
	end

	return count;
end

-- A section goes back where it was moved to, a new one opens under the bar a step down from the last.
local function PlaceSection(section)
	local saved = layout.views[section.id];
	local left, top, width, height;

	if ( saved ) then
		left, top, width, height = saved[1], saved[2], saved[3], saved[4];
	else
		local screenWidth, screenHeight = ScreenSize();

		if ( section.hasMain ) then
			width, height = min(1180, screenWidth - 80), min(680, screenHeight - 140);
		else
			width, height = (section.workspace.dockWidth or DOCK_WIDTH) + 4, min(640, screenHeight - 140);
		end

		local offset = 24 * OpenCount();
		left, top = Round((screenWidth - width) / 2) + offset, Round(screenHeight - 50) - offset;
	end

	PlaceAt(section.window, left, top, max(MINI_MIN_WIDTH, width), max(MINI_MIN_HEIGHT, height));
end

local function AttachSection(section)
	local workspace = section.workspace;
	local window = SectionWindow(section);

	if ( section.sectionMain ) then
		section.mainHost = section.mainHost or NewHost(workspace, window.mainArea);
		AttachHost(section.mainHost, window.mainArea, 0);
		SafeCall(workspace.key, section.sectionMain, section.mainHost.inner);
	elseif ( section.hasMain ) then
		AttachHost(MainHost(workspace), window.mainArea, MINI_MAIN_TRIM);
		BuildMain(workspace);
	end

	if ( section.hasPanel ) then
		local host = DockHost(workspace, section.id);
		AttachHost(host, window.dockArea, 0);
		ShowPanel(workspace, ToolOf(workspace, section.tool), host);
	end
end

-- The focused section owns the menu bar, the hotkeys and the world tool.
local function FocusSection(section)
	local workspace = section.workspace;
	local changed = focusedSection ~= section;
	focusClock = focusClock + 1;
	section.focusTime = focusClock;
	focusedSection = section;
	layout.focus = section.id;

	if ( activeWorkspace ~= workspace ) then
		if ( activeWorkspace and activeWorkspace.menuStrip ) then
			activeWorkspace.menuStrip:Hide();
		end

		activeWorkspace = workspace;
	end

	if ( workspace.menuStrip ) then
		workspace.menuStrip:Show();
	end

	activeTool = section.tool;

	if ( section.tool ) then
		workspace.lastTool = section.tool;
	end

	NoggitFeedback_DefaultLine = section.window.feedbackLine;
	GameEditor_SetTool(section.tool or "select");

	if ( changed ) then
		SafeCall(workspace.key, workspace.onToolSelected, section.tool);
	end

	GameEditor_RefreshViewToolbar();
	RefreshMiniTabs();
	layoutDirty = true;
end

local function ClearFocus()
	if ( activeWorkspace and activeWorkspace.menuStrip ) then
		activeWorkspace.menuStrip:Hide();
	end

	focusedSection = nil;
	activeWorkspace = nil;
	activeTool = nil;
	NoggitFeedback_DefaultLine = statusFeedback;
	GameEditor_SetTool("select");
end

function OpenSection(section)
	local workspace = section.workspace;
	NoggitMenu_CloseAll();
	NoggitTooltip_Hide();
	BuildMenuStrip(workspace);

	local window = SectionWindow(section);

	if ( not window:IsShown() ) then
		AttachSection(section);
		PlaceSection(section);
		section.open = true;
		window:Show();
		Activate(workspace);
	end

	window:Raise();
	LayoutSection(section);
	FocusSection(section);
	layoutDirty = true;
end

function CloseSection(section)
	section.open = false;
	NoggitMenu_CloseAll();
	NoggitTooltip_Hide();

	if ( section.window ) then
		section.window:Hide();
	end

	local workspace = section.workspace;

	if ( not AnyOpen(workspace) ) then
		Deactivate(workspace);

		if ( workspace.menuStrip ) then
			workspace.menuStrip:Hide();
		end
	end

	if ( focusedSection == section ) then
		local latest = LatestOpen();

		if ( latest ) then
			FocusSection(latest);
		else
			ClearFocus();
			layout.focus = nil;
		end
	end

	RefreshMiniTabs();
	layoutDirty = true;
end

-- Called on a mouse press, the window under the cursor takes the focus.
local function FocusUnderMouse()
	local frame = GetMouseFocus();

	while ( frame ) do
		local section = frame.editorSection;

		if ( section ) then
			if ( section ~= focusedSection and section.open ) then
				FocusSection(section);
			end

			return;
		end

		frame = frame.GetParent and frame:GetParent();
	end
end

-- Each window shows its workspace's status cells, a split workspace only updates the focused one.
local function UpdateSectionStatus(elapsed)
	sectionStatusElapsed = sectionStatusElapsed + elapsed;

	if ( sectionStatusElapsed < SECTION_STATUS_TIME ) then
		return;
	end

	sectionStatusElapsed = 0;

	for _, section in ipairs(sectionOrder) do
		local window = section.window;
		local workspace = section.workspace;

		if ( window and window:IsVisible() and workspace.status and (not section.split or section == focusedSection) ) then
			local cells = SafeCall(workspace.key, workspace.status);
			window.statusText:SetText(type(cells) == "table" and table.concat(cells, "    ") or "");
		end
	end
end

local function ToggleSection(section)
	if ( section.open and section == focusedSection and section.window and section.window:IsShown() ) then
		CloseSection(section);
	else
		OpenSection(section);
	end
end

function RefreshMiniTabs()
	local width = 0;

	for _, tab in ipairs(miniTabs) do
		local open, focused;

		for _, section in ipairs(tab.sections) do
			if ( section.open ) then
				open = true;
			end

			if ( section == focusedSection ) then
				focused = section;
			end
		end

		if ( tab.dropdown ) then
			tab.text:SetText(focused and (tab.title..": "..focused.title) or tab.title);
		end

		tab:SetWidth(tab.text:GetStringWidth() + (tab.dropdown and 38 or 24));
		NoggitBox_Set(tab, (miniMode and focused) and NOGGIT_COLOR_ACCENT or (tab.hover and NOGGIT_COLOR_HOVER or NOGGIT_COLOR_BASE), nil);

		if ( open and not focused ) then
			tab.underline:Show();
		else
			tab.underline:Hide();
		end

		width = width + tab:GetWidth() + 2;
	end

	if ( miniTabs.holder ) then
		miniTabs.holder:SetWidth(max(10, width));
	end
end

local function BuildMiniTabs()
	local holder = CreateFrame("Frame", "EditorMiniTabs", EditorMenuBar);
	holder:SetHeight(26);
	holder:SetPoint("RIGHT", EditorMenuBar, "RIGHT", -40, 0);
	holder:Hide();
	miniTabs.holder = holder;
	local previous;

	for _, group in ipairs(TabEntries()) do
		local sections = {};

		for _, workspace in ipairs(group.workspaces or { group.workspace }) do
			for _, section in ipairs(SectionsOf(workspace)) do
				table.insert(sections, section);
			end
		end

		if ( #sections > 0 ) then
			local tab = CreateFrame("Button", NoggitUI_NewName("GameEditorMiniTab"), holder, "NoggitBoxTemplate");
			tab:SetHeight(22);
			tab.sections = sections;
			tab.dropdown = #sections > 1;
			tab.title = group.title;
			tab.text = NoggitUI_FontString(tab, tab.title, nil, "CENTER");
			tab.underline = NoggitUI_SolidTexture(tab, "OVERLAY", NOGGIT_COLOR_ACCENT);
			tab.underline:SetHeight(2);
			tab.underline:SetPoint("BOTTOMLEFT", tab, "BOTTOMLEFT", 2, 1);
			tab.underline:SetPoint("BOTTOMRIGHT", tab, "BOTTOMRIGHT", -2, 1);
			tab.underline:Hide();

			if ( tab.dropdown ) then
				tab.text:SetPoint("LEFT", tab, "LEFT", 12, 0);
				local caret = NoggitUI_CreateCaret(tab, 10);
				caret:SetOpen(true);
				caret:SetPoint("RIGHT", tab, "RIGHT", -10, 0);

				local items = {};

				for _, section in ipairs(sections) do
					table.insert(items, {
						text = section.title,
						checked = function() return section.open; end,
						onClick = function() ToggleSection(section); end,
					});
				end

				tab:SetScript("OnClick", function(self)
					NoggitDropdownMenu_Show("GameEditorMiniTab"..group.title, items, self);
				end);
			else
				tab.text:SetPoint("CENTER", tab, "CENTER", 0, 0);
				tab:SetScript("OnClick", function() ToggleSection(sections[1]); end);
			end

			tab:SetScript("OnEnter", function(self)
				self.hover = true;
				RefreshMiniTabs();
			end);
			tab:SetScript("OnLeave", function(self)
				self.hover = false;
				RefreshMiniTabs();
			end);

			if ( previous ) then
				tab:SetPoint("LEFT", previous, "RIGHT", 2, 0);
			else
				tab:SetPoint("LEFT", holder, "LEFT", 0, 0);
			end

			table.insert(miniTabs, tab);
			previous = tab;
		end
	end

	RefreshMiniTabs();
end

-- Marks the sections the layout file had open, the windows open the first time the mini editor shows.
local function RestoreSections()
	if ( layout.open ) then
		for id in string.gmatch(layout.open, "[%w_]+") do
			if ( sectionsById[id] ) then
				sectionsById[id].open = true;
			end
		end
	elseif ( layout.miniopen and layout.miniws and workspaces[layout.miniws] ) then
		local section = SectionsOf(workspaces[layout.miniws])[1];

		if ( section ) then
			section.open = true;
			layout.focus = section.id;
		end
	end
end

local function BuildMiniChrome()
	EditorFrameRect:SetScript("OnSizeChanged", function() LayoutShell(); end);

	dockSplitter = GameEditor_CreateSplitter(EditorParent, {
		sign = -1,
		collapse = COLLAPSE_SNAP,
		get = function() return EditorToolDock:IsShown() and EditorToolDock:GetWidth() or 0; end,
		set = function(width)
			GameEditor_SetLayoutSize("dock_"..activeWorkspace.key, width);
			LayoutShell();
		end,
		max = function() return max(0, (EditorFrameRect:GetWidth() or 0) - MainMinWidth(activeWorkspace)); end,
	});
	dockSplitter:SetPoint("TOPLEFT", EditorToolDock, "TOPLEFT", -3, 0);
	dockSplitter:SetPoint("BOTTOMLEFT", EditorToolDock, "BOTTOMLEFT", -3, 0);
	dockSplitter:SetFrameLevel(EditorToolDock:GetFrameLevel() + 4);

	BuildMiniTabs();
end

local function RefreshModeMenus()
	for _, item in ipairs(modeMenuItems) do
		if ( item.fullOnly or item.uiToggle ) then
			item.disabled = miniMode;
		end
	end
end

-- Moves the editor between the full shell and the mini windows, returns true when the mode changed.
local function ApplyMode(mini)
	if ( modeApplied and mini == miniMode ) then
		return false;
	end

	local changed = modeApplied;

	if ( modeApplied ) then
		for window in pairs(windows) do
			if ( window.everShown or WindowShown(window) ) then
				PinWindow(window);
			end
		end
	end

	modeApplied = true;
	miniMode = mini;
	NoggitMenu_CloseAll();
	NoggitTooltip_Hide();

	if ( mini ) then
		if ( uiHidden ) then
			GameEditor_SetUIHidden(false);
		end

		EditorWorkspaceTabs:Hide();
		miniTabs.holder:Show();
		EditorMenuBar:SetFrameStrata("HIGH");
	else
		EditorWorkspaceTabs:Show();
		miniTabs.holder:Hide();
		EditorMenuBar:SetFrameStrata("MEDIUM");
	end

	RefreshModeMenus();
	EditorParent:Raise();

	if ( mini ) then
		local current = activeWorkspace;

		if ( current ) then
			fullReturn = { key = current.key, tool = activeTool };
			HideFullUI(current);
		end

		for _, workspace in ipairs(ordered) do
			if ( workspace.live and not AnyOpen(workspace) ) then
				Deactivate(workspace);
			end
		end

		local focus = focusedSection or sectionsById[layout.focus or ""];
		ClearFocus();

		for _, section in ipairs(sectionOrder) do
			if ( section.open ) then
				OpenSection(section);
			end
		end

		if ( focus and focus.open ) then
			focus.window:Raise();
			FocusSection(focus);
		end
	else
		for _, section in ipairs(sectionOrder) do
			if ( section.window ) then
				section.window:Hide();
			end
		end

		local target = (fullReturn and workspaces[fullReturn.key]) or activeWorkspace or SortedWorkspaces()[1];
		local tool = fullReturn and target and fullReturn.key == target.key and fullReturn.tool or nil;
		fullReturn = nil;

		for _, workspace in ipairs(ordered) do
			if ( workspace.live and workspace ~= target ) then
				Deactivate(workspace);
			end
		end

		if ( activeWorkspace and activeWorkspace.menuStrip ) then
			activeWorkspace.menuStrip:Hide();
		end

		activeWorkspace = nil;
		NoggitFeedback_DefaultLine = statusFeedback;

		if ( target ) then
			GameEditor_SetWorkspace(target.key, tool);
		end
	end

	ApplyChrome();
	RefreshMiniTabs();
	layoutDirty = true;
	return changed;
end

-- Shell menus and keys -----------------------------------------------------------------------------------

local function BuildShellMenus()
	shellMenuStrip = CreateFrame("Frame", "GameEditorShellMenus", EditorMenuBar);
	shellMenuStrip:SetHeight(EditorMenuBar:GetHeight());
	shellMenuStrip:SetPoint("BOTTOMLEFT", EditorMenuBar, "BOTTOMLEFT", 0, 0);

	local separator = { separator = true };
	local workspaceItems = {};
	local toggleUI = { text = "Toggle UI", shortcut = "Tab", uiToggle = true, onClick = function() GameEditor_SetUIHidden(not uiHidden); end };
	local fullKey = ToggleKeyText(false);
	local miniKey = ToggleKeyText(true);
	table.insert(modeMenuItems, toggleUI);

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
			{
				text = "Full editor", shortcut = fullKey,
				checked = function() return not miniMode; end,
				onClick = function() GameEditor_SetActive(true, false); end,
			},
			{
				text = "Mini editor, keeps the game UI", shortcut = miniKey,
				checked = function() return miniMode; end,
				onClick = function() GameEditor_SetActive(true, true); end,
			},
			separator,
			toggleUI,
			{ text = "Camera to character", onClick = GameEditor_RecenterCamera },
			{ text = "Reload UI", onClick = function() ReloadUI(); end },
			separator,
			{ text = "Leave the editor", shortcut = fullKey.." / "..miniKey, onClick = function() GameEditor_SetActive(false); end },
		},
	});

	local width = 4;

	for _, child in ipairs({ shellMenuStrip:GetChildren() }) do
		width = width + child:GetWidth() + 4;
	end

	shellMenuStrip:SetWidth(width);
end

function GameEditor_SetUIHidden(hidden)
	if ( hidden and miniMode ) then
		return;
	end

	uiHidden = hidden;
	NoggitMenu_CloseAll();
	NoggitTooltip_Hide();
	ApplyChrome();

	for window in pairs(windows) do
		if ( hidden and window:IsShown() ) then
			window.hiddenByUI = true;
			window:Hide();
		elseif ( not hidden and window.hiddenByUI ) then
			window.hiddenByUI = nil;
			window:Show();
		end
	end
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

	local _, _, number = string.find(key, "^CTRL%-(%d)$");

	if ( number ) then
		local workspace = SortedWorkspaces()[tonumber(number)];

		if ( workspace ) then
			GameEditor_SetWorkspace(workspace.key);
		end

		return workspace ~= nil or not miniMode;
	end

	if ( activeWorkspace and activeWorkspace.onHotkey ) then
		local handled = SafeCall(activeWorkspace.key, activeWorkspace.onHotkey, key);

		if ( handled ) then
			return true;
		end
	end

	if ( key == "ESCAPE" and not miniMode ) then
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

	if ( statusFeedback and statusFeedback.after ~= index - 1 ) then
		statusFeedback.after = index - 1;
		statusFeedback:ClearAllPoints();
		statusFeedback:SetPoint("LEFT", index > 1 and StatusCell(index - 1) or EditorStatusBar, index > 1 and "RIGHT" or "LEFT", 10, 0);
		statusFeedback:SetPoint("RIGHT", EditorStatusBar, "RIGHT", -18, 0);
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
		local active, mini = ...;
		active = active == 1 or active == true;
		mini = mini == 1 or mini == true;

		if ( active ) then
			NoggitMenu_CloseAll();

			-- Already open, this is a switch between the full and the mini editor.
			if ( self:IsShown() and not skipNextActivate ) then
				ApplyMode(mini);
				SaveLayout();
				return;
			end

			self:Show();
			ApplyScale();
			local switched = ApplyMode(mini);

			if ( skipNextActivate ) then
				skipNextActivate = false;
			elseif ( not switched ) then
				Resume();
			end
		else
			-- Closing keeps every window as it is, they come back with the editor.
			Suspend();

			NoggitMenu_CloseAll();
			NoggitTooltip_Hide();
			self:Hide();
			SaveLayout();
		end

		return;
	end

	if ( event == "PLAYER_LOGOUT" ) then
		SaveLayout();
		return;
	end

	if ( event == "CHAT_MSG_SYSTEM" ) then
		if ( consoleFrame ) then
			consoleFrame:AddMessage((...), 1, 1, 0.4);
		end

		return;
	end

	if ( RESULT_EVENTS[event] ) then
		NoggitFeedback_OnResult(event, ...);
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
	self:RegisterEvent("PLAYER_LOGOUT");

	for event in pairs(RESULT_EVENTS) do
		self:RegisterEvent(event);
	end

	LoadLayout();
	CreateConsole();
	statusFeedback = NoggitUI_FontString(EditorStatusBar, "");
	statusFeedback:SetHeight(18);
	statusFeedback:SetPoint("RIGHT", EditorStatusBar, "RIGHT", -18, 0);
	NoggitFeedback_DefaultLine = statusFeedback;
	CreateCameraWindow();
	CreateKeysWindow();
	BuildShellMenus();
	BuildWorkspaceTabs();
	BuildMiniChrome();

	loaded = true;

	for _, workspace in ipairs(SortedWorkspaces()) do
		SafeCall(workspace.key, workspace.onLoad);
	end

	local saved = layout.ws and workspaces[layout.ws];
	local first = saved or SortedWorkspaces()[1];

	RestoreSections();

	if ( first ) then
		GameEditor_SetWorkspace(first.key, saved and layout.tool and FindTool(layout.tool) and layout.tool or nil);
	else
		GameEditor_SetToolPanelShown(false);
	end

	for _, window in ipairs({ consoleWindow, cameraWindow, keysWindow }) do
		local rect = layout.windows[window:GetName()];

		if ( rect and rect[5] ) then
			GameEditor_ShowWindow(window);
		end
	end

	skipNextActivate = true;
	self:Show();
	GameEditor_Print(format("Game editor ready, %s leaves it. Game Editor > Keys lists the controls.", GameEditor_CurrentToggleKey()));
end

function GameEditor_OnUpdate(self, elapsed)
	ApplyScale();
	NoggitMenu_OnUpdate(elapsed);
	LayoutToasts(elapsed);

	if ( layoutDirty ) then
		SaveLayout();
	end

	if ( miniMode ) then
		local down = EditorIsMouseButtonDown();

		if ( down and not mouseWasDown ) then
			FocusUnderMouse();
		end

		mouseWasDown = down;

		for _, workspace in ipairs(ordered) do
			if ( workspace.live and not workspace.suspended and workspace.onUpdate ) then
				SafeCall(workspace.key, workspace.onUpdate, elapsed);
			end
		end

		UpdateSectionStatus(elapsed);
	elseif ( activeWorkspace and activeWorkspace.onUpdate ) then
		SafeCall(activeWorkspace.key, activeWorkspace.onUpdate, elapsed);
	end

	if ( not ChromeHidden() ) then
		UpdateStatus(elapsed);
	end
end
