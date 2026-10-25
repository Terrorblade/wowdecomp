-- Vehicles workspace, edits Vehicle and its VehicleSeat rows and previews a creature display with riders on the seats.

local VE = GameEditorVehicle;
local ME = GameEditorModel;
local Common = GameEditorObject;

local PREVIEW_HEIGHT = 250;
local LIST_HEIGHT = 112;
local TAB_NAMES = { "Seats", "Vehicle" };
local TAB_SEATS, TAB_VEHICLE = 1, 2;
local INT_LOW, INT_HIGH = -2147483648, 2147483647;

local SEAT_ANIMS = {
	"m_enterAnimStart", "m_enterAnimLoop", "m_rideAnimStart", "m_rideAnimLoop", "m_rideUpperAnimStart", "m_rideUpperAnimLoop",
	"m_exitAnimStart", "m_exitAnimLoop", "m_exitAnimEnd", "m_vehicleEnterAnim", "m_vehicleExitAnim", "m_vehicleRideAnimLoop",
};

local SEAT_PLACEMENT = {
	"m_attachmentID", "m_attachmentOffset[0]", "m_attachmentOffset[1]", "m_attachmentOffset[2]",
	"m_field33512213027", "m_passengerPitch", "m_field33512213029", "m_flags", "m_flagsB",
};

local Num = VE.Num;

local frame;
local vehicleEdit;
local displayEdit;
local tabs;
local container;
local pages = {};
local empty;
local statusText;
local saveButton;
local revertButton;
local vehicleIds;
local pendingSelection;

VE.vehicleId = VE.vehicleId or nil;
VE.slot = VE.slot or 0;
VE.displayId = VE.displayId or nil;

local function VehicleDoc()
	return VE.vehicleId and VE.Doc("Vehicle", VE.vehicleId);
end

local function SeatId(slot)
	local doc = VehicleDoc();
	return doc and Num(doc.values[VE.SeatColumn(slot)]) or 0;
end

local function SeatDoc(slot)
	local id = SeatId(slot or VE.slot);
	return id > 0 and VE.Doc("VehicleSeat", id) or nil;
end

local function SyncPreviewSeats()
	local list = {};

	for slot = 0, VE.SEATS - 1 do
		local seat = SeatDoc(slot);

		if ( seat ) then
			table.insert(list, { slot = slot, values = seat.values, selected = slot == VE.slot });
		end
	end

	VE.PreviewSeats(list);
end

-- Opening ---------------------------------------------------------------------------------------------------

local function OpenVehicle(id)
	id = tonumber(id);

	if ( not id or not VE.Doc("Vehicle", id) ) then
		ME.Notice(format("Vehicle %s does not exist", tostring(id)));
		return false;
	end

	VE.vehicleId = id;
	VE.slot = 0;

	for slot = 0, VE.SEATS - 1 do
		if ( SeatId(slot) > 0 ) then
			VE.slot = slot;
			break;
		end
	end

	vehicleEdit:SetText(tostring(id));
	vehicleEdit:ClearFocus();
	VE.Changed();
	return true;
end

local function ShowDisplay(id)
	id = tonumber(id);

	if ( not id or not ME.Row("CreatureDisplayInfo", id) ) then
		ME.Notice(format("CreatureDisplayInfo %s does not exist", tostring(id)));
		return false;
	end

	VE.displayId = id;
	displayEdit:SetText(tostring(id));
	displayEdit:ClearFocus();
	VE.SetPreviewDisplay(id);
	SyncPreviewSeats();
	VE.Changed();
	return true;
end

local function VehicleIds()
	if ( not vehicleIds ) then
		vehicleIds = {};

		for id = 1, ME.MaxId("Vehicle") do
			local row = ME.Row("Vehicle", id);

			if ( row ) then
				local seats = 0;

				for slot = 0, VE.SEATS - 1 do
					if ( Num(row[VE.SeatColumn(slot)]) > 0 ) then
						seats = seats + 1;
					end
				end

				table.insert(vehicleIds, { value = id, text = format("%d seat%s", seats, seats == 1 and "" or "s") });
			end
		end
	end

	return vehicleIds;
end

local function BrowseVehicles()
	GameEditor_PickValue("Vehicles", VehicleIds, VE.vehicleId, function(value)
		OpenVehicle(value);
	end);
end

local function BrowseDisplays()
	GameEditor_PickRef("Creature displays", "dbc:CreatureDisplayInfo", VE.displayId, function(value)
		ShowDisplay(value);
	end);
end

-- The selected creature gives its display and, when its template has one, its vehicle.
local function FromSelection()
	local kind, guid = GameEditor_GetSelection();

	if ( kind ~= "creature" ) then
		ME.Notice("Select a creature in the world first", "info");
		return;
	end

	local data = ME.Quiet("CreatureEditor_GetData", "selection");

	if ( type(data) ~= "table" or data.guid ~= guid ) then
		pendingSelection = guid;
		ME.Call("CreatureEditor_Request", guid);
		GameEditor_Notify("Loading the creature, the vehicle opens when its data arrives", "info");
		return;
	end

	local template = data.template or {};
	local displayId = Num(data.spawn and data.spawn.modelid);

	for index = 1, 4 do
		if ( displayId <= 0 ) then
			displayId = Num(template["modelid"..index]);
		end
	end

	local vehicleId = Num(template.VehicleId);

	if ( vehicleId > 0 ) then
		OpenVehicle(vehicleId);
	else
		ME.Notice("The creature template has no VehicleId", "info");
	end

	if ( displayId > 0 ) then
		ShowDisplay(displayId);
	end
end

table.insert(ME.selectionHandlers, function(event, arg1)
	if ( not pendingSelection or event ~= "HOT_CREATURE_EDITOR_DATA" or tonumber(arg1) == 1 ) then
		return;
	end

	local _, guid = GameEditor_GetSelection();
	local wanted = pendingSelection;
	pendingSelection = nil;

	if ( guid == wanted ) then
		FromSelection();
	end
end);

-- Editing ---------------------------------------------------------------------------------------------------

local function NewVehicle()
	local source = VehicleDoc();
	local doc = VE.CreateDoc("Vehicle", source and source.values or { m_turnSpeed = 3.14159, m_pitchSpeed = 3.14159, m_pitchMin = -1.5708, m_pitchMax = 1.5708 });
	vehicleIds = nil;
	OpenVehicle(doc.id);
	GameEditor_Notify(format("Vehicle %d made, it is not on the server until you save", doc.id), "info");
end

local function NewSeat()
	local vehicle = VehicleDoc();

	if ( not vehicle ) then
		ME.Notice("Open a vehicle first", "info");
		return;
	end

	local source = SeatDoc(VE.slot);
	local seat = VE.CreateDoc("VehicleSeat", source and source.values or { m_attachmentID = 13, m_flags = 0x02000800, m_rideAnimLoop = ME.MOUNT_ANIM });
	VE.SetMany(vehicle, { [VE.SeatColumn(VE.slot)] = seat.id });
	SyncPreviewSeats();
end

local function ClearSeat()
	local vehicle = VehicleDoc();

	if ( vehicle and SeatId(VE.slot) > 0 ) then
		VE.SetMany(vehicle, { [VE.SeatColumn(VE.slot)] = 0 });
		SyncPreviewSeats();
	end
end

local function Save()
	VE.SaveAll();
end

local function Revert()
	local count = #VE.DirtyDocs();

	if ( count == 0 ) then
		GameEditor_Notify("Nothing to revert", "info");
		return;
	end

	NoggitMessageBox_Show("Revert", format("Throw away the changes to %d row%s?", count, count == 1 and "" or "s"), {
		{ text = "Revert", onClick = function()
			VE.RevertAll();
			vehicleIds = nil;

			if ( VE.vehicleId and not VE.Doc("Vehicle", VE.vehicleId) ) then
				VE.vehicleId = nil;
			end

			SyncPreviewSeats();
		end },
		{ text = "Cancel" },
	});
end

local function OpenInModelEditor()
	if ( not VE.displayId ) then
		ME.Notice("Pick a creature display first", "info");
		return;
	end

	GameEditor_SetWorkspace("model");
	ME.OpenCreatureDisplay(VE.displayId);
end

-- Forms -------------------------------------------------------------------------------------------------------

local function NewPage()
	local page = CreateFrame("Frame", NoggitUI_NewName("GameEditorVehiclePage"), container);
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

local function AnimHint(item)
	item.hintFunc = function(value)
		return Num(value) > 0 and ME.AnimName(value) or "";
	end;
end

local function AddColumn(form, column, flagSets)
	local name = column.name;
	local label = VE.Label(name);

	if ( flagSets and flagSets[name] ) then
		return form:Flags(name, label, flagSets[name], 32);
	elseif ( column.type == "float" ) then
		return form:Float(name, label);
	elseif ( column.type == "string" ) then
		return form:Path(name, label, "Game path");
	elseif ( column.type == "uint" ) then
		return form:Int(name, label, 0, 4294967295);
	end

	return form:Int(name, label, INT_LOW, INT_HIGH);
end

-- Every stored column of the table that is not in skip.
local function AddColumns(form, tableName, skip, flagSets)
	local schema = ME.Quiet("DbcEditor_GetSchema", tableName);

	for _, column in ipairs(type(schema) == "table" and schema or {}) do
		if ( column.stored and not skip[column.name] ) then
			AddColumn(form, column, flagSets);
		end
	end
end

local function Set(list)
	local set = {};

	for _, key in ipairs(list) do
		set[key] = true;
	end

	return set;
end

local function SeatAttachmentItems()
	local items = {};

	for index = 0, #VE.SEAT_ATTACHMENTS do
		local id = VE.SEAT_ATTACHMENTS[index];
		local mark = "";

		if ( VE.attachments ) then
			mark = VE.HasAttachment(id) and "" or "  |cffff6060not in the model|r";
		end

		table.insert(items, { value = index, text = format("%d  %s (%d)%s", index, ME.AttachmentName(id), id, mark) });
	end

	return items;
end

local function SeatRowText(item)
	if ( item.seatId <= 0 ) then
		return format("Seat %d  |cff808080empty|r", item.slot + 1);
	end

	local doc = VE.Doc("VehicleSeat", item.seatId);

	if ( not doc ) then
		return format("Seat %d  id %d  |cffff6060row missing|r", item.slot + 1, item.seatId);
	end

	local attachment = VE.SeatAttachment(doc.values.m_attachmentID);
	local state = "";

	if ( not attachment ) then
		state = "  |cffff6060no attachment|r";
	elseif ( VE.attachments and not VE.HasAttachment(attachment) ) then
		state = "  |cffff6060model lacks it|r";
	end

	return format("Seat %d  id %d  %s%s%s", item.slot + 1, item.seatId, ME.AttachmentName(attachment or -1), state, VE.IsDirty(doc) and "  |cffffcc00*|r" or "");
end

local function BuildSeatPage()
	local page = NewPage();

	page.list = NoggitListBox_Create(page, 20, function(row)
		row.text = NoggitUI_FontString(row, "");
		row.text:SetPoint("LEFT", row, "LEFT", 4, 0);
		row.text:SetPoint("RIGHT", row, "RIGHT", -4, 0);
	end, function(row, item)
		row.text:SetText(SeatRowText(item));
	end);
	page.list:SetPoint("TOPLEFT", page, "TOPLEFT", 0, 0);
	page.list:SetPoint("TOPRIGHT", page, "TOPRIGHT", 0, 0);
	page.list:SetHeight(LIST_HEIGHT);
	page.list.onSelect = function(item)
		VE.slot = item.slot;
		SyncPreviewSeats();
		page:Refresh();
	end;

	local bar = CreateFrame("Frame", NoggitUI_NewName("GameEditorVehicleSeatBar"), page);
	bar:SetHeight(22);
	bar:SetPoint("TOPLEFT", page.list, "BOTTOMLEFT", 0, -4);
	bar:SetPoint("TOPRIGHT", page.list, "BOTTOMRIGHT", 0, -4);

	local idLabel = Common.Label(bar, "Seat id");
	idLabel:SetPoint("LEFT", bar, "LEFT", 0, 0);
	page.seatSpin = ME.Spin(bar, 80, 0, INT_HIGH, 1, 0, function(value)
		local vehicle = VehicleDoc();
		value = floor(Num(value));

		if ( not vehicle ) then
			return;
		end

		if ( value > 0 and not VE.Doc("VehicleSeat", value) ) then
			ME.Notice(format("VehicleSeat %d does not exist", value));
			page:Refresh();
			return;
		end

		VE.SetMany(vehicle, { [VE.SeatColumn(VE.slot)] = value });
		SyncPreviewSeats();
	end, "VehicleSeat row this slot uses, 0 leaves it empty");
	page.seatSpin:SetPoint("LEFT", idLabel, "RIGHT", 6, 0);

	page.newSeat = Common.Button(bar, "New seat", 70, NewSeat, "Make a new VehicleSeat row for this slot, a copy of the current seat when it has one");
	page.newSeat:SetPoint("LEFT", page.seatSpin, "RIGHT", 6, 0);
	page.clearSeat = Common.Button(bar, "Clear", 50, ClearSeat, "Empty this slot, the seat row itself stays");
	page.clearSeat:SetPoint("LEFT", page.newSeat, "RIGHT", 4, 0);
	page.stateText = Common.Label(bar, "", nil, NOGGIT_COLOR_DISABLED);
	page.stateText:SetPoint("LEFT", page.clearSeat, "RIGHT", 8, 0);
	page.stateText:SetPoint("RIGHT", bar, "RIGHT", 0, 0);

	PageScroll(page, LIST_HEIGHT + 30);

	local form = ME.CreateForm(page.child, 470, function(fields)
		local seat = SeatDoc(VE.slot);

		if ( seat ) then
			VE.SetMany(seat, fields);
			SyncPreviewSeats();
		end
	end);

	form:Header("Placement");
	form:Combo("m_attachmentID", "Attachment", SeatAttachmentItems, "Index into the seat attachment table, the M2 attachment id is in brackets", 260);
	form:Float("m_attachmentOffset[0]", "Offset X", "Forward from the attachment in its own space");
	form:Float("m_attachmentOffset[1]", "Offset Y", "Left from the attachment");
	form:Float("m_attachmentOffset[2]", "Offset Z", "Up from the attachment");
	form:Float("m_field33512213027", "Passenger yaw", "Radians, turns the passenger around Z");
	form:Float("m_passengerPitch", "Passenger pitch", "Radians, tilts the passenger around Y");
	form:Float("m_field33512213029", "Passenger roll", "Radians, tilts the passenger around X");
	form:Flags("m_flags", "Flags", VE.SEAT_FLAGS, 32);
	form:Flags("m_flagsB", "Flags B", VE.SEAT_FLAGS_B, 32);

	form:Header("Animations");

	for _, column in ipairs(SEAT_ANIMS) do
		AnimHint(form:Int(column, VE.Label(column), INT_LOW, INT_HIGH, "AnimationData id"));
	end

	local skip = Set(SEAT_PLACEMENT);

	for _, column in ipairs(SEAT_ANIMS) do
		skip[column] = true;
	end

	skip.m_id = true;
	form:Header("Other fields");
	AddColumns(form, "VehicleSeat", skip);

	page.empty = Common.Paragraph(page.child, "");
	page.empty:SetPoint("TOPLEFT", page.child, "TOPLEFT", 0, -4);
	page.empty:SetPoint("TOPRIGHT", page.child, "TOPRIGHT", 0, -4);

	page:SetScript("OnUpdate", function(self, elapsed)
		self.wait = (self.wait or 0) + elapsed;

		if ( self.wait < 0.3 ) then
			return;
		end

		self.wait = 0;
		local seat = SeatDoc(VE.slot);
		local attachment = seat and VE.SeatAttachment(seat.values.m_attachmentID);
		local text = "";

		if ( seat and not VE.displayId ) then
			text = "Pick a display to see the rider";
		elseif ( seat and not attachment ) then
			text = "No attachment for this index";
		elseif ( seat and VE.attachments and not VE.HasAttachment(attachment) ) then
			text = format("The model has no %s", ME.AttachmentName(attachment));
		elseif ( seat and VE.attached[VE.slot + 1] ) then
			text = "|cff60ff60rider shown|r";
		end

		self.stateText:SetText(text);
	end);

	page.Refresh = function(self)
		local items = {};

		for slot = 0, VE.SEATS - 1 do
			table.insert(items, { slot = slot, seatId = SeatId(slot) });
		end

		NoggitListBox_SetItems(self.list, items, true);
		self.list.selected = VE.slot + 1;
		NoggitListBox_Refresh(self.list);

		local vehicle = VehicleDoc();
		ME.SetSpin(self.seatSpin, SeatId(VE.slot), vehicle ~= nil);
		ME.SetEnabled(self.newSeat, vehicle ~= nil);
		ME.SetEnabled(self.clearSeat, SeatId(VE.slot) > 0);

		local seat = SeatDoc(VE.slot);

		if ( seat ) then
			self.empty:Hide();
			self.child:SetHeight(form:Refresh(seat.values) + 10);
		else
			form:Refresh(nil);

			for _, item in ipairs(form.items) do
				item.frame:Hide();
			end

			self.empty:SetText(SeatId(VE.slot) > 0 and format("VehicleSeat %d does not exist.", SeatId(VE.slot)) or "This slot is empty, New seat makes a row for it or type the id of one.");
			self.empty:Show();
			self.child:SetHeight(40);
		end

		self.child:SetWidth(ChildWidth(self));
		self.scroll.UpdateRange();
	end;

	return page;
end

local function BuildVehiclePage()
	local page = NewPage();
	PageScroll(page, 0);

	local form = ME.CreateForm(page.child, 470, function(fields)
		local doc = VehicleDoc();

		if ( doc ) then
			VE.SetMany(doc, fields);
		end
	end);

	form:Header("Vehicle");
	local skip = { m_id = true };

	for slot = 0, VE.SEATS - 1 do
		skip[VE.SeatColumn(slot)] = true;
	end

	AddColumns(form, "Vehicle", skip, { m_flags = VE.VEHICLE_FLAGS });

	page.Refresh = function(self)
		local doc = VehicleDoc();
		self.child:SetHeight(form:Refresh(doc and doc.values) + 10);
		self.child:SetWidth(ChildWidth(self));
		self.scroll.UpdateRange();
	end;

	return page;
end

-- Refresh ------------------------------------------------------------------------------------------------------

local function RefreshStatus()
	local dirty = #VE.DirtyDocs();

	if ( dirty > 0 ) then
		statusText:SetText(format("%d unsaved row%s", dirty, dirty == 1 and "" or "s"));
		NoggitText_SetColor(statusText, NOGGIT_COLOR_GOLD);
	else
		statusText:SetText(VE.vehicleId and format("Vehicle %d", VE.vehicleId) or "Open a vehicle to edit it");
		NoggitText_SetColor(statusText, NOGGIT_COLOR_DISABLED);
	end

	ME.SetEnabled(saveButton, dirty > 0);
	ME.SetEnabled(revertButton, dirty > 0);
end

local function RefreshPages()
	if ( not frame ) then
		return;
	end

	RefreshStatus();

	if ( not VehicleDoc() ) then
		container:Hide();
		empty:Set({
			title = "No vehicle open",
			text = "Type a Vehicle id above or browse them, then pick a creature display to see the seats with a human male on them.",
			primary = "From selection", onPrimary = FromSelection,
			secondary = "Browse vehicles", onSecondary = BrowseVehicles,
			third = "New vehicle", onThird = NewVehicle,
			help = "Edits apply to this client at once and reach the server when you save. The server reads Vehicle and VehicleSeat at startup.",
		});
		return;
	end

	empty:Hide();
	container:Show();

	local page = pages[tabs.current];

	if ( page and page.Refresh ) then
		page:Refresh();
	end
end

VE.OnChanged(function()
	if ( frame and frame:IsShown() ) then
		RefreshPages();
	end
end);

local function ShowPage(index)
	for key, page in pairs(pages) do
		if ( key == index ) then
			page:Show();
		else
			page:Hide();
		end
	end

	RefreshPages();
end

-- Build ----------------------------------------------------------------------------------------------------------

local function IdRow(parent, anchor, label, tooltip, onEnter)
	local row = CreateFrame("Frame", NoggitUI_NewName("GameEditorVehicleRow"), parent);
	row:SetHeight(22);

	if ( anchor ) then
		row:SetPoint("TOPLEFT", anchor, "BOTTOMLEFT", 0, -4);
		row:SetPoint("TOPRIGHT", anchor, "BOTTOMRIGHT", 0, -4);
	else
		row:SetPoint("TOPLEFT", parent, "TOPLEFT", 6, -6);
		row:SetPoint("TOPRIGHT", parent, "TOPRIGHT", -6, -6);
	end

	local text = Common.Label(row, label);
	text:SetPoint("LEFT", row, "LEFT", 0, 0);
	text:SetWidth(52);
	local edit = NoggitLineEdit_Create(row, 70);
	edit:SetHeight(22);
	edit:SetMaxLetters(10);
	edit:SetPoint("LEFT", text, "RIGHT", 4, 0);
	edit:SetScript("OnEnterPressed", function(self)
		onEnter(tonumber(strtrim(self:GetText() or "")));
	end);
	edit:SetScript("OnEscapePressed", function(self) self:ClearFocus(); end);
	Common.Tooltip(edit, tooltip);
	return row, edit;
end

local function Build(parent)
	frame = CreateFrame("Frame", "GameEditorVehiclePanel", parent);

	local vehicleRow;
	vehicleRow, vehicleEdit = IdRow(frame, nil, "Vehicle", "Vehicle.dbc id, Enter opens it", OpenVehicle);
	local browse = Common.Button(vehicleRow, "Browse", 60, BrowseVehicles, "List the Vehicle rows");
	browse:SetPoint("LEFT", vehicleEdit, "RIGHT", 4, 0);
	local new = Common.Button(vehicleRow, "New", 46, NewVehicle, "Make a new Vehicle row, a copy of the open one when there is one");
	new:SetPoint("LEFT", browse, "RIGHT", 4, 0);
	local fromSelection = Common.Button(vehicleRow, "From selection", 104, FromSelection, "Open the vehicle and display of the selected creature");
	fromSelection:SetPoint("RIGHT", vehicleRow, "RIGHT", 0, 0);

	local displayRow;
	displayRow, displayEdit = IdRow(frame, vehicleRow, "Display", "CreatureDisplayInfo id of the vehicle model, Enter shows it", ShowDisplay);
	local browseDisplay = Common.Button(displayRow, "Browse", 60, BrowseDisplays, "Search creature displays by id or by model name");
	browseDisplay:SetPoint("LEFT", displayEdit, "RIGHT", 4, 0);
	local modelEditor = Common.Button(displayRow, "Edit attachments", 116, OpenInModelEditor, "Open the display model on the Attach tab of the model editor");
	modelEditor:SetPoint("RIGHT", displayRow, "RIGHT", 0, 0);

	local previewHolder = CreateFrame("Frame", NoggitUI_NewName("GameEditorVehiclePreviewHolder"), frame);
	previewHolder:SetHeight(PREVIEW_HEIGHT);
	previewHolder:SetPoint("TOPLEFT", displayRow, "BOTTOMLEFT", 0, -6);
	previewHolder:SetPoint("RIGHT", frame, "RIGHT", -6, 0);
	VE.CreatePreview(previewHolder);

	local bar = CreateFrame("Frame", NoggitUI_NewName("GameEditorVehicleBar"), frame);
	bar:SetHeight(24);
	bar:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 6, 6);
	bar:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -6, 6);

	saveButton = Common.Button(bar, "Save", 70, Save, "Send every changed Vehicle and VehicleSeat row to the server (Ctrl+S)");
	saveButton:SetPoint("RIGHT", bar, "RIGHT", 0, 0);
	revertButton = Common.Button(bar, "Revert", 60, Revert, "Throw away the unsaved changes");
	revertButton:SetPoint("RIGHT", saveButton, "LEFT", -4, 0);
	statusText = Common.Label(bar, "", nil, NOGGIT_COLOR_DISABLED);
	statusText:SetPoint("LEFT", bar, "LEFT", 2, 0);
	statusText:SetPoint("RIGHT", revertButton, "LEFT", -4, 0);

	tabs = Common.CreateTabs(frame, TAB_NAMES, ShowPage);
	tabs.tabs[1]:SetPoint("TOPLEFT", previewHolder, "BOTTOMLEFT", 0, -6);

	container = CreateFrame("Frame", NoggitUI_NewName("GameEditorVehiclePages"), frame);
	container:SetPoint("TOPLEFT", tabs.tabs[1], "BOTTOMLEFT", 0, -6);
	container:SetPoint("RIGHT", frame, "RIGHT", -6, 0);
	container:SetPoint("BOTTOM", bar, "TOP", 0, 6);

	empty = Common.CreateEmptyState(frame);
	empty:ClearAllPoints();
	empty:SetPoint("TOPLEFT", tabs.tabs[1], "BOTTOMLEFT", -6, -6);
	empty:SetPoint("BOTTOMRIGHT", bar, "TOPRIGHT", 6, 6);

	pages[TAB_SEATS] = BuildSeatPage();
	pages[TAB_VEHICLE] = BuildVehiclePage();

	frame:SetScript("OnShow", function()
		ShowPage(tabs.current);
	end);

	if ( VE.vehicleId ) then
		vehicleEdit:SetText(tostring(VE.vehicleId));
	end

	if ( VE.displayId ) then
		displayEdit:SetText(tostring(VE.displayId));
		VE.SetPreviewDisplay(VE.displayId);
	end

	SyncPreviewSeats();
	ShowPage(tabs.current);
	return frame;
end

-- Workspace --------------------------------------------------------------------------------------------------------

local separator = { separator = true };

local function BuildMenus()
	return {
		{
			title = "Vehicle",
			{ text = "Open from selection", onClick = FromSelection },
			{ text = "Browse vehicles...", onClick = BrowseVehicles },
			{ text = "Browse displays...", onClick = BrowseDisplays },
			separator,
			{ text = "New vehicle", onClick = NewVehicle },
			{ text = "New seat in this slot", onClick = NewSeat },
			separator,
			{ text = "Save", shortcut = "Ctrl+S", onClick = Save },
			{ text = "Revert", onClick = Revert },
			separator,
			{ text = "Edit the model attachments", onClick = OpenInModelEditor },
			{ text = "Riders on all seats", checked = function() return VE.allSeats; end, onClick = function()
				VE.allSeats = not VE.allSeats;
				SyncPreviewSeats();
			end },
		},
	};
end

local KEYS = {
	{ "Ctrl+S", "Save the changed Vehicle and VehicleSeat rows" },
	{ "Drag in the preview", "Turn the vehicle, Shift or right drag moves it" },
	{ "Wheel in the preview", "Zoom" },
};

local function BuildStatus()
	local cells = {};
	table.insert(cells, VE.vehicleId and format("Vehicle %d", VE.vehicleId) or "No vehicle open");

	if ( VE.displayId ) then
		table.insert(cells, format("Display %d", VE.displayId));
	end

	local dirty = #VE.DirtyDocs();

	if ( dirty > 0 ) then
		table.insert(cells, format("|cffffcc00%d unsaved|r", dirty));
	end

	return cells;
end

GameEditor_RegisterWorkspace({
	key = "vehicle",
	title = "Vehicles",
	category = "Visuals",
	order = 12,
	dockWidth = 520,
	tools = {
		{ key = "select", name = "Vehicles", icon = 0xF8C0, build = Build },
	},
	menus = BuildMenus,
	keys = KEYS,
	onActivate = function()
		SyncPreviewSeats();
		VE.Changed();
	end,
	onHotkey = function(key)
		if ( key == "CTRL-S" ) then
			Save();
			return true;
		end

		return false;
	end,
	status = BuildStatus,
});
