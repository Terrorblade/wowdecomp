local ITEM_HEIGHT = 22;
local SEPARATOR_HEIGHT = 9;
local CHECK_GLYPH = 0xF00C;
local SUBMENU_GLYPH = 0xF0DA;

local CLOSE_DELAY = 0.4;

local openMenu;
local contextMenu;
local contextMenus = {};
local menuBarButtons = {};
local idleTime = 0;
local mouseWasDown = false;
local dismissed;

local function CloseSubmenus(menu)
	if ( menu.openSubmenu ) then
		CloseSubmenus(menu.openSubmenu);
		menu.openSubmenu:Hide();
		menu.openSubmenu = nil;
	end
end

function NoggitMenu_CloseAll()
	if ( contextMenu ) then
		CloseSubmenus(contextMenu);
		contextMenu:Hide();
		contextMenu = nil;
	end

	if ( openMenu ) then
		CloseSubmenus(openMenu);
		openMenu:Hide();
		openMenu.owner.open = false;
		NoggitMenuBar_Refresh(openMenu.owner);
		openMenu = nil;
	end

	idleTime = 0;
end

function NoggitMenuItem_Refresh(self, hover)
	if ( hover ~= nil ) then
		self.hover = hover;
	end

	local name = self:GetName();
	local item = self.item;
	local enabled = not item.disabled;
	local bg = _G[name.."Bg"];

	if ( self.hover and enabled ) then
		bg:SetTexture(NOGGIT_COLOR_HOVER[1], NOGGIT_COLOR_HOVER[2], NOGGIT_COLOR_HOVER[3], 1);
	else
		bg:SetTexture(0, 0, 0, 0);
	end

	local color = enabled and NOGGIT_COLOR_TEXT or NOGGIT_COLOR_DISABLED;
	NoggitText_SetColor(_G[name.."Label"], color);
	NoggitText_SetColor(_G[name.."Shortcut"], color);
	NoggitText_SetColor(_G[name.."Check"], color);

	local checked = item.checked and item.checked();

	if ( item.icon ) then
		_G[name.."Check"]:SetText(NoggitUI_Utf8(item.icon));
	else
		_G[name.."Check"]:SetText(checked and NoggitUI_Utf8(CHECK_GLYPH) or "");
	end

	if ( self.hover and enabled and item.submenu ) then
		local menu = self.menu;

		if ( menu.openSubmenu ~= item.submenu.frame ) then
			CloseSubmenus(menu);
			local sub = item.submenu.frame;
			sub:ClearAllPoints();
			sub:SetPoint("TOPLEFT", self, "TOPRIGHT", 0, 0);
			sub:Show();
			menu.openSubmenu = sub;
		end
	elseif ( self.hover ) then
		CloseSubmenus(self.menu);
	end
end

function NoggitMenuItem_OnClick(self)
	local item = self.item;

	if ( item.disabled or item.submenu ) then
		return;
	end

	NoggitMenu_CloseAll();

	if ( item.onClick ) then
		item.onClick();
	end
end

local function BuildMenu(definition, parent)
	local menu = CreateFrame("Frame", NoggitUI_NewName("NoggitMenu"), parent, "NoggitMenuTemplate");
	NoggitBox_Set(menu, NOGGIT_COLOR_BASE, NOGGIT_COLOR_MID);
	menu.items = {};

	local width = 120;
	local y = -4;

	for _, item in ipairs(definition) do
		if ( item.separator ) then
			local line = NoggitUI_SolidTexture(menu, "ARTWORK", NOGGIT_COLOR_HOVER);
			line:SetHeight(1);
			line:SetPoint("TOPLEFT", menu, "TOPLEFT", 1, y - 4);
			line:SetPoint("TOPRIGHT", menu, "TOPRIGHT", -1, y - 4);
			y = y - SEPARATOR_HEIGHT;
		else
			local button = CreateFrame("Button", NoggitUI_NewName("NoggitMenuItem"), menu, "NoggitMenuItemTemplate");
			button.item = item;
			button.menu = menu;
			button:SetPoint("TOPLEFT", menu, "TOPLEFT", 1, y);
			button:SetPoint("TOPRIGHT", menu, "TOPRIGHT", -1, y);
			local name = button:GetName();
			_G[name.."Label"]:SetText(item.text);

			if ( item.submenu ) then
				_G[name.."Shortcut"]:SetFontObject(NoggitAwesomeFont);
				_G[name.."Shortcut"]:SetText(NoggitUI_Utf8(SUBMENU_GLYPH));
				item.submenu.frame = BuildMenu(item.submenu, menu);
			else
				_G[name.."Shortcut"]:SetText(item.shortcut or "");
			end

			width = max(width, 28 + _G[name.."Label"]:GetStringWidth() + 48 + _G[name.."Shortcut"]:GetStringWidth() + 12);
			table.insert(menu.items, button);
			y = y - ITEM_HEIGHT;
		end
	end

	menu:SetWidth(width);
	menu:SetHeight(-y + 4);
	return menu;
end

local function RefreshMenu(menu)
	for _, button in ipairs(menu.items) do
		button.hover = false;
		NoggitMenuItem_Refresh(button);
	end
end

function NoggitMenuBar_Refresh(self)
	local bg = _G[self:GetName().."Bg"];

	if ( self.open ) then
		bg:SetTexture(NOGGIT_COLOR_DARK[1], NOGGIT_COLOR_DARK[2], NOGGIT_COLOR_DARK[3], 1);
	elseif ( self:IsMouseOver() ) then
		bg:SetTexture(NOGGIT_COLOR_HOVER[1], NOGGIT_COLOR_HOVER[2], NOGGIT_COLOR_HOVER[3], 1);
	else
		bg:SetTexture(0, 0, 0, 0);
	end
end

local function OpenMenu(button)
	NoggitMenu_CloseAll();
	openMenu = button.menu;
	button.open = true;
	RefreshMenu(openMenu);
	openMenu:ClearAllPoints();
	openMenu:SetPoint("TOPLEFT", button, "BOTTOMLEFT", 0, 0);
	openMenu:Show();
	idleTime = 0;
	NoggitMenuBar_Refresh(button);
end

function NoggitMenuBar_OnEnter(self)
	if ( not openMenu or openMenu.owner ~= self ) then
		OpenMenu(self);
	end

	NoggitMenuBar_Refresh(self);
end

function NoggitMenuBar_OnClick(self)
	if ( not openMenu or openMenu.owner ~= self ) then
		OpenMenu(self);
	end
end

local function IsOverMenuChain(menu)
	while ( menu ) do
		if ( menu:IsMouseOver() ) then
			return true;
		end

		menu = menu.openSubmenu;
	end

	return false;
end

function NoggitContextMenu_Show(key, definition)
	NoggitMenu_CloseAll();
	local menu = contextMenus[key];

	if ( not menu ) then
		menu = BuildMenu(definition, EditorParent);
		contextMenus[key] = menu;
	end

	RefreshMenu(menu);
	local x, y = NoggitUI_CursorIn(EditorParent);
	menu:ClearAllPoints();
	menu:SetPoint("TOPLEFT", EditorParent, "BOTTOMLEFT", x, y);
	menu:Show();
	contextMenu = menu;
end

-- Same as a context menu but under anchor, showing it again while it is open closes it.
function NoggitDropdownMenu_Show(key, definition, anchor)
	local menu = contextMenus[key];

	if ( menu and contextMenu == menu ) then
		NoggitMenu_CloseAll();
		return;
	end

	-- The press on the anchor already closed it, the click that follows must not open it again.
	if ( menu and dismissed and dismissed.menu == menu and GetTime() - dismissed.time < 0.5 ) then
		dismissed = nil;
		return;
	end

	NoggitMenu_CloseAll();

	if ( not menu ) then
		menu = BuildMenu(definition, EditorParent);
		contextMenus[key] = menu;
	end

	RefreshMenu(menu);
	menu:ClearAllPoints();
	menu:SetPoint("TOPLEFT", anchor, "BOTTOMLEFT", 0, 0);
	menu:Show();
	contextMenu = menu;
end

function NoggitMenu_OnUpdate(elapsed)
	local mouseDown = EditorIsMouseButtonDown();
	local pressed = mouseDown and not mouseWasDown;
	mouseWasDown = mouseDown;

	if ( contextMenu ) then
		if ( pressed and not IsOverMenuChain(contextMenu) ) then
			dismissed = { menu = contextMenu, time = GetTime() };
			NoggitMenu_CloseAll();
		end

		return;
	end

	if ( not openMenu ) then
		return;
	end

	local over = openMenu.owner:IsMouseOver() or IsOverMenuChain(openMenu);

	if ( pressed and not over ) then
		NoggitMenu_CloseAll();
		return;
	end

	if ( over or mouseDown ) then
		idleTime = 0;
		return;
	end

	idleTime = idleTime + elapsed;

	if ( idleTime > CLOSE_DELAY ) then
		NoggitMenu_CloseAll();
	end
end

function NoggitMenuBar_Build(bar, definitions)
	local previous;

	for _, definition in ipairs(definitions) do
		local button = CreateFrame("Button", NoggitUI_NewName("NoggitMenuBarButton"), bar, "NoggitMenuBarButtonTemplate");
		button:SetText(definition.title);
		button:SetWidth(button:GetFontString():GetStringWidth() + 16);

		if ( previous ) then
			button:SetPoint("LEFT", previous, "RIGHT", 4, 0);
		else
			button:SetPoint("BOTTOMLEFT", bar, "BOTTOMLEFT", 4, 0);
		end

		button.menu = BuildMenu(definition, EditorParent);
		button.menu.owner = button;
		table.insert(menuBarButtons, button);
		previous = button;
	end
end
