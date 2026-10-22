-- Preview of a vehicle display with a human male on its seats, through the attachment calls of the model editor.

local VE = GameEditorVehicle;
local ME = GameEditorModel;
local Common = GameEditorObject;

local REAPPLY_DELAY = 0.3;
local RETRY_DELAY = 0.1;
local RETRY_LIMIT = 100;
local ZOOM_STEP = 1.15;
local ROTATE_SPEED = 0.012;
local DEFAULT_PITCH = 0.25;
local WORLD_VEHICLE = 1;
local WORLD_RIDER = 2;

local Num = VE.Num;

local panel;
local model;
local message;
local vehicleAnimButton;
local riderAnimButton;
local allSeatsBox;
local inWorldBox;
local worldShown;

local view = { yaw = 0, pitch = DEFAULT_PITCH, zoom = 1, panX = 0, panY = 0 };
local shown = { displayId = nil, path = nil, scale = 1 };
local seats = {};
local slotsInUse = {};
local reapplyIn;
local retries = 0;
local drag;
local loadedPath;

VE.vehicleAnim = 0;
VE.riderAnim = nil;
VE.allSeats = false;
VE.inWorld = false;
VE.attached = {};
VE.attachments = nil;

local function SetMessage(text)
	if ( message ) then
		message:SetText(text or "");
	end
end

local function Try(method, ...)
	if ( model and model[method] ) then
		return pcall(model[method], model, ...);
	end

	return false;
end

local function ApplyCamera()
	if ( not model or not shown.path ) then
		return;
	end

	local framed = ME.Quiet("ModelEditor_SetPreviewView", model, view.yaw, view.pitch, view.zoom, view.panX, view.panY);

	if ( not framed and retries < RETRY_LIMIT ) then
		retries = retries + 1;
		reapplyIn = RETRY_DELAY;
	end
end

local function ApplyView()
	Try("SetPosition", 0, 0, 0);
	Try("SetFacing", 0);
	Try("SetModelScale", shown.scale);
	Try("SetLight", 1, 0, -0.5, 0.4, -0.75, 0.6, 1, 1, 1, 0.9, 1, 1, 1);

	if ( shown.displayId ) then
		ME.Quiet("ModelEditor_SetPreviewSkin", model, shown.displayId);
	end

	ApplyCamera();
	Try("SetSequence", VE.vehicleAnim);
end

local function AnimText(id)
	return format("%s (%d)", ME.AnimName(id), Num(id));
end

local function RefreshControls()
	if ( not panel ) then
		return;
	end

	vehicleAnimButton:SetText("Vehicle: "..AnimText(VE.vehicleAnim));
	riderAnimButton:SetText("Rider: "..(VE.riderAnim and AnimText(VE.riderAnim) or "Seat"));
	NoggitCheckBox_SetChecked(allSeatsBox, VE.allSeats);
end

-- What the client plays on a seated passenger, the ride loop when the seat has one and Stand otherwise.
local function SeatAnim(values)
	local flags = Num(values.m_flags);
	flags = flags < 0 and flags + 4294967296 or flags;
	local loop = Num(values.m_rideAnimLoop);

	if ( floor(flags / 2) % 2 == 1 and loop >= 0 ) then
		return loop;
	end

	return 0;
end

-- Riders go on every seat with a row, or only on the selected one, slot numbers are seat slot + 1.
local function ApplyRiders()
	if ( not model ) then
		return;
	end

	local wanted = {};

	for _, seat in ipairs(seats) do
		local attachment = VE.SeatAttachment(seat.values.m_attachmentID);

		if ( attachment and (VE.allSeats or seat.selected) ) then
			wanted[seat.slot + 1] = {
				attachment = attachment,
				rider = true,
				anim = VE.riderAnim or SeatAnim(seat.values),
				offset = { x = Num(seat.values["m_attachmentOffset[0]"]), y = Num(seat.values["m_attachmentOffset[1]"]), z = Num(seat.values["m_attachmentOffset[2]"]) },
				yaw = Num(seat.values.m_field33512213027),
				pitch = Num(seat.values.m_passengerPitch),
				roll = Num(seat.values.m_field33512213029),
			};
		end
	end

	for slot in pairs(slotsInUse) do
		if ( not wanted[slot] ) then
			ME.Quiet("ModelEditor_SetPreviewAttach", model, slot, nil);
		end
	end

	slotsInUse = {};

	for slot, options in pairs(wanted) do
		ME.Quiet("ModelEditor_SetPreviewAttach", model, slot, options);
		slotsInUse[slot] = true;
	end

	VE.attached = {};
end

local RIDER_LOOKS = { m_skinID = 5, m_faceID = 5, m_hairStyleID = 8, m_hairColorID = 7, m_facialHairID = 6 };

-- A client only human male display with random looks, no gear and no baked texture so the character component dresses it.
local function RiderDisplay()
	local modelId = ME.Quiet("WorldPreview_HumanModelData");
	local extraId = ME.TempId("CreatureDisplayInfoExtra", "rider");
	local displayId = ME.TempId("CreatureDisplayInfo", "rider");

	if ( not modelId or not extraId or not displayId ) then
		return nil;
	end

	local extra = { m_displayRaceID = 1, m_displaySexID = 0, m_flags = 0, m_bakeName = "" };

	for key, count in pairs(RIDER_LOOKS) do
		extra[key] = random(0, count - 1);
	end

	for i = 0, 10 do
		extra["m_npcitemDisplay["..i.."]"] = 0;
	end

	local display = { m_modelID = modelId, m_extendedDisplayInfoID = extraId, m_creatureModelScale = 1, m_creatureModelAlpha = 255 };
	local ok = pcall(DbcEditor_ApplyLocal, "CreatureDisplayInfoExtra", extraId, extra) and pcall(DbcEditor_ApplyLocal, "CreatureDisplayInfo", displayId, display);
	return ok and displayId or nil;
end

local function ClearWorld()
	for slot = WORLD_VEHICLE, WORLD_RIDER + VE.SEATS - 1 do
		ME.Quiet("WorldPreview_Remove", slot);
	end

	worldShown = nil;
end

-- The vehicle and its riders as client only units in front of the player, the client vehicle code seats them.
local function SyncWorld()
	if ( not VE.inWorld or not shown.displayId ) then
		if ( worldShown ) then
			ClearWorld();
		end

		return;
	end

	local riders = {};
	local parts = { shown.displayId, VE.vehicleId or 0 };

	for _, seat in ipairs(seats) do
		table.insert(parts, seat.slot..":"..Num(seat.values.m_id));

		if ( VE.vehicleId and (VE.allSeats or seat.selected) ) then
			table.insert(riders, seat.slot);
			table.insert(parts, "r"..seat.slot);
		end
	end

	local signature = table.concat(parts, ",");

	if ( signature == worldShown ) then
		ME.Quiet("WorldPreview_SetAnim", WORLD_VEHICLE, VE.vehicleAnim);

		for _, slot in ipairs(riders) do
			ME.Quiet("WorldPreview_SetAnim", WORLD_RIDER + slot, VE.riderAnim);
		end

		return;
	end

	ClearWorld();
	local ok, err = ME.Quiet("WorldPreview_Spawn", WORLD_VEHICLE, { kind = "unit", displayId = shown.displayId, vehicleId = VE.vehicleId or 0, anim = VE.vehicleAnim });

	if ( not ok ) then
		ME.Notice("In world preview: "..tostring(err or "the client refused the unit"), "info");
		return;
	end

	local riderDisplay = #riders > 0 and RiderDisplay();

	for _, slot in ipairs(riders) do
		ME.Quiet("WorldPreview_Spawn", WORLD_RIDER + slot, { kind = "unit", displayId = riderDisplay, parent = WORLD_VEHICLE, seat = slot, anim = VE.riderAnim });
	end

	worldShown = signature;
end

function VE.SetInWorld(enabled)
	VE.inWorld = enabled and true or false;

	if ( inWorldBox ) then
		NoggitCheckBox_SetChecked(inWorldBox, VE.inWorld);
	end

	SyncWorld();
end

-- seats is a list of { slot, values, selected } from the open vehicle.
function VE.PreviewSeats(list)
	seats = list or {};
	ApplyRiders();
	SyncWorld();
end

-- Shows the model of a CreatureDisplayInfo row with its skin and scale, nil clears the preview.
function VE.SetPreviewDisplay(displayId)
	displayId = tonumber(displayId);

	if ( not model ) then
		return false;
	end

	if ( not displayId or displayId <= 0 ) then
		ME.Quiet("ModelEditor_ClearPreviewAttach", model);
		slotsInUse = {};
		Try("ClearModel");
		shown.displayId, shown.path, loadedPath = nil, nil, nil;
		VE.attachments = nil;
		SetMessage("Pick a creature display to preview the vehicle");
		SyncWorld();
		return false;
	end

	local display = ME.Row("CreatureDisplayInfo", displayId);
	local modelData = display and ME.Row("CreatureModelData", display.m_modelID);
	local path = modelData and modelData.m_modelName or "";

	if ( path == "" ) then
		SetMessage(format("CreatureDisplayInfo %d has no model", displayId));
		return false;
	end

	local scale = Num(display.m_creatureModelScale);
	scale = (scale > 0 and scale or 1) * (Num(modelData.m_modelScale) > 0 and Num(modelData.m_modelScale) or 1);

	if ( displayId == shown.displayId and path == shown.path ) then
		return true;
	end

	local ok, err = Try("SetModel", path);

	if ( not ok ) then
		SetMessage("Preview failed: "..tostring(err or "Model frame missing"));
		return false;
	end

	shown.displayId, shown.path, shown.scale = displayId, path, scale;
	loadedPath = nil;
	VE.attachments = nil;
	SetMessage("");
	retries = 0;
	ApplyView();
	reapplyIn = REAPPLY_DELAY;
	ApplyRiders();
	SyncWorld();
	RefreshControls();
	return true;
end

function VE.PreviewDisplay()
	return shown.displayId, shown.path;
end

-- Attachment ids of the loaded vehicle model as a set, nil while it loads.
function VE.HasAttachment(id)
	return VE.attachments and VE.attachments[Num(id)] or false;
end

function VE.PreviewResetCamera()
	view.yaw, view.pitch, view.zoom, view.panX, view.panY = 0, DEFAULT_PITCH, 1, 0, 0;
	retries = 0;
	ApplyView();
end

local function PickVehicleAnim()
	local items = ME.AnimItems(model and ME.Quiet("ModelEditor_GetPreviewSequences", model));

	if ( #items == 0 ) then
		ME.Notice("The vehicle model is not loaded yet", "info");
		return;
	end

	GameEditor_PickValue("Vehicle animation", items, VE.vehicleAnim, function(value)
		VE.vehicleAnim = Num(value);
		Try("SetSequence", VE.vehicleAnim);
		SyncWorld();
		RefreshControls();
	end);
end

local function PickRiderAnim()
	local slot = next(slotsInUse);
	local items = ME.AnimItems(slot and ME.Quiet("ModelEditor_GetPreviewSequences", model, slot));

	if ( #items == 0 ) then
		ME.Notice("No rider is shown yet, pick a seat whose attachment the model has", "info");
		return;
	end

	table.insert(items, 1, { value = -1, text = "Seat animation (what the seat row plays)" });

	GameEditor_PickValue("Rider animation", items, VE.riderAnim or -1, function(value)
		value = Num(value);
		VE.riderAnim = value >= 0 and value or nil;
		ApplyRiders();
		SyncWorld();
		RefreshControls();
	end);
end

local function OnMouseDown(self, button)
	local x, y = NoggitUI_CursorIn(self);
	drag = { button = button, x = x, y = y, yaw = view.yaw, pitch = view.pitch, panX = view.panX, panY = view.panY, pan = button ~= "LeftButton" or IsShiftKeyDown() };
end

local function OnUpdate(self, elapsed)
	if ( drag ) then
		if ( not EditorIsMouseButtonDown(drag.button) ) then
			drag = nil;
		else
			local x, y = NoggitUI_CursorIn(self);
			local dx, dy = x - drag.x, y - drag.y;

			if ( drag.pan ) then
				local unit = 2.4 * view.zoom / max(1, self:GetHeight());
				view.panX = drag.panX - dx * unit;
				view.panY = drag.panY - dy * unit;
			else
				view.yaw = drag.yaw - dx * ROTATE_SPEED;
				view.pitch = max(-1.5, min(1.5, drag.pitch + dy * ROTATE_SPEED));
			end

			ApplyCamera();
		end
	end

	if ( reapplyIn ) then
		reapplyIn = reapplyIn - elapsed;

		if ( reapplyIn <= 0 ) then
			reapplyIn = nil;
			ApplyView();
		end
	end

	if ( shown.path and loadedPath ~= shown.path ) then
		local list = ME.Quiet("ModelEditor_GetPreviewAttachments", model);

		if ( type(list) == "table" ) then
			loadedPath = shown.path;
			VE.attachments = {};

			for _, attachment in ipairs(list) do
				VE.attachments[Num(attachment.id)] = true;
			end

			VE.Changed();
		end
	end

	if ( next(slotsInUse) ) then
		local state = ME.Quiet("ModelEditor_UpdatePreviewAttach", model);
		VE.attached = type(state) == "table" and state or {};
	end
end

function VE.CreatePreview(holder)
	panel = CreateFrame("Frame", "GameEditorVehiclePreview", holder, "NoggitBoxTemplate");
	NoggitBox_Set(panel, { 0.07, 0.07, 0.08, 1 }, NOGGIT_COLOR_MID);
	panel:SetAllPoints(holder);

	local bar = CreateFrame("Frame", NoggitUI_NewName("GameEditorVehiclePreviewBar"), panel);
	bar:SetHeight(24);
	bar:SetPoint("BOTTOMLEFT", panel, "BOTTOMLEFT", 2, 2);
	bar:SetPoint("BOTTOMRIGHT", panel, "BOTTOMRIGHT", -2, 2);

	model = CreateFrame("Model", "GameEditorVehiclePreviewModel", panel);
	model:SetPoint("TOPLEFT", panel, "TOPLEFT", 2, -2);
	model:SetPoint("BOTTOMRIGHT", bar, "TOPRIGHT", 0, 2);
	model:EnableMouse(true);
	model:EnableMouseWheel(true);
	model:SetScript("OnMouseDown", OnMouseDown);
	model:SetScript("OnMouseUp", function() drag = nil; end);
	model:SetScript("OnMouseWheel", function(_, delta)
		view.zoom = max(0.02, min(50, view.zoom * (delta > 0 and 1 / ZOOM_STEP or ZOOM_STEP)));
		ApplyCamera();
	end);
	model:SetScript("OnUpdate", OnUpdate);
	Common.Tooltip(model, "Drag to orbit, Shift or right drag to pan, wheel to zoom");

	message = Common.Label(panel, "Pick a creature display to preview the vehicle", nil, NOGGIT_COLOR_DISABLED);
	message:SetPoint("TOPLEFT", panel, "TOPLEFT", 8, -8);
	message:SetPoint("TOPRIGHT", panel, "TOPRIGHT", -8, -8);

	vehicleAnimButton = Common.Button(bar, "Vehicle", 150, PickVehicleAnim, "Animation of the vehicle, only the ones its model can play are listed");
	vehicleAnimButton:SetHeight(20);
	vehicleAnimButton:SetPoint("LEFT", bar, "LEFT", 0, 0);

	riderAnimButton = Common.Button(bar, "Rider", 150, PickRiderAnim, "Animation of the human male on the seats, by default the one the seat row plays");
	riderAnimButton:SetHeight(20);
	riderAnimButton:SetPoint("LEFT", vehicleAnimButton, "RIGHT", 4, 0);

	local reset = Common.Button(bar, "Reset", 50, VE.PreviewResetCamera, "Reset the turn, zoom and offset");
	reset:SetHeight(20);
	reset:SetPoint("RIGHT", bar, "RIGHT", 0, 0);

	allSeatsBox = NoggitCheckBox_Create(bar, "All seats", false, function(_, checked)
		VE.allSeats = checked;
		ApplyRiders();
		SyncWorld();
	end);
	allSeatsBox:SetPoint("RIGHT", reset, "LEFT", -6, 0);
	Common.Tooltip(allSeatsBox, "Put a rider on every seat instead of only the selected one");

	local top = CreateFrame("Frame", NoggitUI_NewName("GameEditorVehiclePreviewTopBar"), panel);
	top:SetHeight(24);
	top:SetPoint("TOPLEFT", panel, "TOPLEFT", 2, -2);
	top:SetPoint("TOPRIGHT", panel, "TOPRIGHT", -2, -2);
	model:SetPoint("TOPLEFT", top, "BOTTOMLEFT", 0, -2);

	local here = Common.Button(top, "Here", 44, function()
		ME.Quiet("WorldPreview_MoveHere");
	end, "Move the in world preview in front of you again");
	here:SetHeight(20);
	here:SetPoint("RIGHT", top, "RIGHT", 0, 0);

	inWorldBox = NoggitCheckBox_Create(top, "In world", VE.inWorld, function(_, checked)
		VE.SetInWorld(checked);
	end);
	inWorldBox:SetPoint("RIGHT", here, "LEFT", -6, 0);
	Common.Tooltip(inWorldBox, "Also spawn the vehicle with its riders in front of you, client side only, nobody else sees it");
	message:ClearAllPoints();
	message:SetPoint("TOPLEFT", model, "TOPLEFT", 6, -6);
	message:SetPoint("TOPRIGHT", model, "TOPRIGHT", -6, -6);

	panel:SetScript("OnShow", function()
		reapplyIn = REAPPLY_DELAY;
	end);

	RefreshControls();
	return panel;
end
