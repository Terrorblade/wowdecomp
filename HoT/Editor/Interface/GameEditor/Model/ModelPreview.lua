-- Preview of the edited model, a Model frame in the dock or in its own window.

local ME = GameEditorModel;
local Common = GameEditorObject;

local RELOAD_DELAY = 0.4;
local REAPPLY_DELAY = 0.3;
local RETRY_DELAY = 0.1;
local RETRY_LIMIT = 100;
local ZOOM_STEP = 1.15;
local ROTATE_SPEED = 0.012;
local DEFAULT_PITCH = 0.25;
local WORLD_SLOT = 20;

local Num = ME.Num;

local panel;
local model;
local dockHolder;
local window;
local message;
local seqButton;
local timeSlider;
local cameraBox;
local inWorldBox;
local worldShown;

local view = { yaw = 0, pitch = DEFAULT_PITCH, zoom = 1, panX = 0, panY = 0, modelCamera = false };
local shownDoc;
local shownPath;
local reloadIn;
local reapplyIn;
local retries = 0;
local drag;

ME.previewSequence = nil;
ME.previewTime = nil;
ME.previewAttach = nil;
ME.previewAttached = nil;
ME.inWorld = false;

local function SetMessage(text)
	if ( message ) then
		message:SetText(text or "");
	end
end

local function CurrentSequence()
	local info = ME.current and ME.Info(ME.current);
	return info and ME.previewSequence and ME.SequenceByIndex(info, ME.previewSequence);
end

-- Client only rows past the last id, the in world unit or gameobject needs a display that points at the preview file.
local function TempId(tableName)
	return ME.TempId(tableName, "model");
end

local function CopyRow(tableName, id, changes)
	local values = {};

	for key, value in pairs(ME.Row(tableName, id) or {}) do
		if ( type(value) ~= "table" ) then
			values[key] = value;
		end
	end

	for key, value in pairs(changes) do
		values[key] = value;
	end

	return values;
end

local function ApplyLocal(tableName, id, values)
	local ok, result = pcall(DbcEditor_ApplyLocal, tableName, id, values);
	return ok and result;
end

local function WorldDisplay(source, path)
	if ( source.kind == "gameobject" ) then
		local id = TempId("GameObjectDisplayInfo");

		if ( id and ApplyLocal("GameObjectDisplayInfo", id, CopyRow("GameObjectDisplayInfo", source.displayId, { m_modelName = path })) ) then
			return "gameobject", id;
		end

		return nil;
	end

	local modelId, displayId = TempId("CreatureModelData"), TempId("CreatureDisplayInfo");
	local display = ME.Row("CreatureDisplayInfo", source.displayId);
	local modelSource = source.modelDataId or (display and display.m_modelID);
	local modelChanges = { m_modelName = path };

	if ( not ME.Row("CreatureModelData", modelSource) ) then
		modelChanges.m_modelScale = 1;
		modelChanges.m_collisionWidth = 0.5;
		modelChanges.m_collisionHeight = 2;
	end

	if ( not modelId or not displayId or not ApplyLocal("CreatureModelData", modelId, CopyRow("CreatureModelData", modelSource, modelChanges)) ) then
		return nil;
	end

	local displayChanges = { m_modelID = modelId, m_extendedDisplayInfoID = 0 };

	if ( not display ) then
		displayChanges.m_creatureModelScale = 1;
		displayChanges.m_creatureModelAlpha = 255;
	end

	if ( not ApplyLocal("CreatureDisplayInfo", displayId, CopyRow("CreatureDisplayInfo", source.displayId, displayChanges)) ) then
		return nil;
	end

	return "unit", displayId;
end

local function WorldAnim()
	local info = ME.current and ME.Info(ME.current);
	local sequence = info and ME.previewSequence and ME.SequenceByIndex(info, ME.previewSequence);
	return sequence and Num(sequence.id) or nil;
end

-- The previewed revision as a client only creature or gameobject in front of the player, with the same animation and attachment.
local function SyncWorld()
	local entry = ME.inWorld and ME.Current();

	if ( not entry or not shownPath ) then
		if ( worldShown ) then
			ME.Quiet("WorldPreview_Remove", WORLD_SLOT);
			worldShown = nil;
		end

		return;
	end

	local source = entry.source or {};
	local signature = shownPath..":"..tostring(source.kind)..":"..tostring(source.displayId);

	if ( signature ~= worldShown ) then
		ME.Quiet("WorldPreview_Remove", WORLD_SLOT);
		worldShown = nil;
		local kind, displayId = WorldDisplay(source, shownPath);

		if ( not kind ) then
			ME.Notice("In world preview: could not make the client only display rows", "info");
			return;
		end

		local ok, err = ME.Quiet("WorldPreview_Spawn", WORLD_SLOT, { kind = kind, displayId = displayId, anim = WorldAnim() });

		if ( not ok ) then
			ME.Notice("In world preview: "..tostring(err or "the client refused the object"), "info");
			return;
		end

		worldShown = signature;
	else
		ME.Quiet("WorldPreview_SetAnim", WORLD_SLOT, WorldAnim());
	end

	ME.Quiet("WorldPreview_SetAttach", WORLD_SLOT, 1, ME.previewAttach);
end

function ME.SetInWorld(enabled)
	ME.inWorld = enabled and true or false;

	if ( inWorldBox ) then
		NoggitCheckBox_SetChecked(inWorldBox, ME.inWorld);
	end

	SyncWorld();
end

-- Model frame methods are the 3.3.5 CSimpleModel ones, each call is guarded since a bad model raises a Lua error.
local function Try(method, ...)
	if ( model and model[method] ) then
		local ok, err = pcall(model[method], model, ...);
		return ok, err;
	end

	return false;
end

local function ApplySequence()
	local sequence = CurrentSequence();

	if ( not sequence ) then
		return;
	end

	if ( ME.previewTime ) then
		Try("SetSequenceTime", Num(sequence.id), floor(ME.previewTime));
	else
		Try("SetSequence", Num(sequence.id));
	end

	if ( worldShown ) then
		ME.Quiet("WorldPreview_SetAnim", WORLD_SLOT, Num(sequence.id));
	end
end

-- The orbit camera only takes once the model is loaded, so this keeps retrying until it does.
local function ApplyCamera()
	if ( not model or not shownPath ) then
		return;
	end

	if ( view.modelCamera ) then
		ME.Call("ModelEditor_ClearPreviewView", model);
		Try("SetCamera", 0);
		return;
	end

	local framed = ME.Call("ModelEditor_SetPreviewView", model, view.yaw, view.pitch, view.zoom, view.panX, view.panY);

	if ( not framed and retries < RETRY_LIMIT ) then
		retries = retries + 1;
		reapplyIn = RETRY_DELAY;
	end
end

local function ApplyView()
	Try("SetPosition", 0, 0, 0);
	Try("SetFacing", 0);
	Try("SetModelScale", 1);
	Try("SetLight", 1, 0, -0.5, 0.4, -0.75, 0.6, 1, 1, 1, 0.9, 1, 1, 1);
	ApplyCamera();
	ApplySequence();
end

local function RefreshControls()
	if ( not panel ) then
		return;
	end

	local sequence = CurrentSequence();
	seqButton:SetText(sequence and ME.SequenceText(sequence) or "Animation");
	local duration = sequence and max(1, Num(sequence.duration)) or 1;
	NoggitSlider_SetRange(timeSlider, 0, duration, max(1, floor(duration / 200)));
	NoggitSlider_SetValueQuiet(timeSlider, ME.previewTime or 0);
	NoggitCheckBox_SetChecked(cameraBox, view.modelCamera);
end

-- Loads the current revision of the doc, force sets the model again even when the path did not change.
function ME.PreviewReload(force)
	reloadIn = nil;

	if ( not model ) then
		return;
	end

	local doc = ME.current;

	if ( not doc ) then
		Try("ClearModel");
		shownDoc, shownPath = nil, nil;
		SetMessage("No model open");
		SyncWorld();
		RefreshControls();
		return;
	end

	local path, err = ME.Call("ModelEditor_GetPreviewPath", doc);

	if ( not path or path == "" ) then
		SetMessage(err and tostring(err) or "No preview for this model");
		return;
	end

	if ( not force and doc == shownDoc and path == shownPath ) then
		return;
	end

	if ( force ) then
		Try("ClearModel");
	end

	local ok, loadErr = Try("SetModel", path);

	if ( not ok ) then
		SetMessage("Preview failed: "..tostring(loadErr or "Model frame missing"));
		return;
	end

	shownDoc, shownPath = doc, path;
	SetMessage("");
	retries = 0;
	ApplyView();
	reapplyIn = REAPPLY_DELAY;
	SyncWorld();
	RefreshControls();
end

function ME.PreviewResetCamera()
	view.yaw, view.pitch, view.zoom, view.panX, view.panY = 0, DEFAULT_PITCH, 1, 0, 0;
	retries = 0;
	ApplyView();
end

local function PickAnimation()
	local info = ME.current and ME.Info(ME.current);

	if ( not info ) then
		return;
	end

	local items = {};

	for _, sequence in ipairs(info.sequences or {}) do
		if ( ME.IsAvailable(sequence) ) then
			table.insert(items, { value = sequence.index, text = format("%s, %d ms", ME.SequenceText(sequence), Num(sequence.duration)) });
		end
	end

	if ( #items == 0 ) then
		ME.Notice("The model has no animation it can play", "info");
		return;
	end

	GameEditor_PickValue("Animation", items, ME.previewSequence, function(value)
		ME.previewSequence = Num(value);
		ME.previewTime = nil;
		ApplySequence();
		RefreshControls();
	end);
end

local function ApplyAttach()
	if ( model ) then
		ME.Quiet("ModelEditor_SetPreviewAttach", model, 1, ME.previewAttach);
	end

	if ( worldShown ) then
		ME.Quiet("WorldPreview_SetAttach", WORLD_SLOT, 1, ME.previewAttach);
	end

	ME.previewAttached = nil;
end

-- Shows a marker or a rider on an attachment id of the previewed model, nil takes it away.
function ME.SetPreviewAttach(options)
	ME.previewAttach = options;
	ApplyAttach();
end

function ME.PreviewRiderSequences()
	return model and ME.Quiet("ModelEditor_GetPreviewSequences", model, 1);
end

local function OnMouseDown(self, button)
	local x, y = NoggitUI_CursorIn(self);
	drag = { button = button, x = x, y = y, yaw = view.yaw, pitch = view.pitch, panX = view.panX, panY = view.panY, pan = button ~= "LeftButton" or IsShiftKeyDown() };
end

local function OnMouseUp()
	drag = nil;
end

local function OnUpdate(self, elapsed)
	if ( ME.previewAttach ) then
		local state = ME.Quiet("ModelEditor_UpdatePreviewAttach", model);
		ME.previewAttached = type(state) == "table" and state[1] or false;
	end

	if ( drag ) then
		local down = drag.button and IsMouseButtonDown(drag.button);

		if ( not down ) then
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

	if ( reloadIn ) then
		reloadIn = reloadIn - elapsed;

		if ( reloadIn <= 0 ) then
			ME.PreviewReload(false);
		end
	end

	if ( reapplyIn ) then
		reapplyIn = reapplyIn - elapsed;

		if ( reapplyIn <= 0 ) then
			reapplyIn = nil;
			ApplyView();
		end
	end
end

local function Attach(parent)
	panel:SetParent(parent);
	panel:ClearAllPoints();
	panel:SetAllPoints(parent);
	panel:Show();
end

function ME.IsPreviewFloating()
	return window and window:IsShown() and true or false;
end

-- Moves the preview between the dock and its own window, the dock gives the pages more room then.
function ME.SetPreviewFloating(floating)
	if ( not panel ) then
		return;
	end

	if ( floating ) then
		if ( not window ) then
			window = GameEditor_CreateWindow("GameEditorModelPreviewWindow", "Model preview", 460, 460, true);
			window.onClose = function()
				ME.SetPreviewFloating(false);
			end;
		end

		Attach(window.content);
		GameEditor_ShowWindow(window);
	else
		if ( window and window:IsShown() ) then
			window.onClose = nil;
			GameEditor_HideWindow(window);
			window.onClose = function()
				ME.SetPreviewFloating(false);
			end;
		end

		Attach(dockHolder);
	end

	if ( ME.OnPreviewLayout ) then
		ME.OnPreviewLayout(floating);
	end

	reapplyIn = REAPPLY_DELAY;
end

-- Builds the preview inside the holder frame of the dock.
function ME.CreatePreview(holder)
	dockHolder = holder;
	panel = CreateFrame("Frame", "GameEditorModelPreview", holder, "NoggitBoxTemplate");
	NoggitBox_Set(panel, { 0.07, 0.07, 0.08, 1 }, NOGGIT_COLOR_MID);
	panel:SetAllPoints(holder);

	local bar = CreateFrame("Frame", NoggitUI_NewName("GameEditorModelPreviewBar"), panel);
	bar:SetHeight(24);
	bar:SetPoint("BOTTOMLEFT", panel, "BOTTOMLEFT", 2, 2);
	bar:SetPoint("BOTTOMRIGHT", panel, "BOTTOMRIGHT", -2, 2);

	model = CreateFrame("Model", "GameEditorModelPreviewModel", panel);
	model:SetPoint("TOPLEFT", panel, "TOPLEFT", 2, -2);
	model:SetPoint("BOTTOMRIGHT", bar, "TOPRIGHT", 0, 2);
	model:EnableMouse(true);
	model:EnableMouseWheel(true);
	model:SetScript("OnMouseDown", OnMouseDown);
	model:SetScript("OnMouseUp", OnMouseUp);
	model:SetScript("OnMouseWheel", function(_, delta)
		view.zoom = max(0.02, min(50, view.zoom * (delta > 0 and 1 / ZOOM_STEP or ZOOM_STEP)));
		ApplyCamera();
	end);
	model:SetScript("OnUpdate", OnUpdate);
	Common.Tooltip(model, "Drag to orbit, Shift or right drag to pan, wheel to zoom, F5 reloads");

	message = Common.Label(panel, "No model open", nil, NOGGIT_COLOR_DISABLED);
	message:SetPoint("TOPLEFT", panel, "TOPLEFT", 8, -8);
	message:SetPoint("TOPRIGHT", panel, "TOPRIGHT", -8, -8);

	seqButton = Common.Button(bar, "Animation", 150, PickAnimation, "Pick the animation to play, the Model frame plays it by its animation id");
	seqButton:SetHeight(20);
	seqButton:SetPoint("LEFT", bar, "LEFT", 0, 0);

	local play = Common.Button(bar, "Play", 44, function()
		ME.previewTime = nil;
		ApplySequence();
		RefreshControls();
	end, "Play the animation instead of holding one frame");
	play:SetHeight(20);
	play:SetPoint("LEFT", seqButton, "RIGHT", 4, 0);

	local float = Common.Button(bar, "Window", 64, function()
		ME.SetPreviewFloating(not ME.IsPreviewFloating());
	end, "Show the preview in its own window or back in the dock");
	float:SetHeight(20);
	float:SetPoint("RIGHT", bar, "RIGHT", 0, 0);

	local reset = Common.Button(bar, "Reset", 50, ME.PreviewResetCamera, "Reset the turn, zoom and offset");
	reset:SetHeight(20);
	reset:SetPoint("RIGHT", float, "LEFT", -4, 0);

	cameraBox = NoggitCheckBox_Create(bar, "Cam", false, function(_, checked)
		view.modelCamera = checked;
		ApplyView();
	end);
	cameraBox:SetPoint("RIGHT", reset, "LEFT", -4, 0);
	Common.Tooltip(cameraBox, "Look through camera 0 of the model, the portrait camera most creatures have");

	timeSlider = NoggitSlider_Create(bar, 100, 0, 1, 1, function(_, value)
		ME.previewTime = value;
		ApplySequence();
	end);
	timeSlider:SetPoint("LEFT", play, "RIGHT", 8, 0);
	timeSlider:SetPoint("RIGHT", cameraBox, "LEFT", -8, 0);
	Common.Tooltip(timeSlider, "Hold the animation at this time in milliseconds");

	local top = CreateFrame("Frame", NoggitUI_NewName("GameEditorModelPreviewTopBar"), panel);
	top:SetHeight(24);
	top:SetPoint("TOPLEFT", panel, "TOPLEFT", 2, -2);
	top:SetPoint("TOPRIGHT", panel, "TOPRIGHT", -2, -2);
	model:SetPoint("TOPLEFT", top, "BOTTOMLEFT", 0, -2);

	local here = Common.Button(top, "Here", 44, function()
		ME.Quiet("WorldPreview_MoveHere");
	end, "Move the in world preview in front of you again");
	here:SetHeight(20);
	here:SetPoint("RIGHT", top, "RIGHT", 0, 0);

	inWorldBox = NoggitCheckBox_Create(top, "In world", ME.inWorld, function(_, checked)
		ME.SetInWorld(checked);
	end);
	inWorldBox:SetPoint("RIGHT", here, "LEFT", -6, 0);
	Common.Tooltip(inWorldBox, "Also spawn the model as a creature or gameobject in front of you, client side only, nobody else sees it");
	message:ClearAllPoints();
	message:SetPoint("TOPLEFT", model, "TOPLEFT", 6, -6);
	message:SetPoint("TOPRIGHT", model, "TOPRIGHT", -6, -6);

	panel:SetScript("OnShow", function()
		reapplyIn = REAPPLY_DELAY;
	end);

	return panel;
end

ME.OnChanged(function(docs)
	if ( not model ) then
		return;
	end

	if ( ME.current ~= shownDoc ) then
		local info = ME.current and ME.Info(ME.current);

		local sequence = CurrentSequence();

		if ( info and (not sequence or not ME.IsAvailable(sequence)) ) then
			ME.previewSequence = nil;
			ME.previewTime = nil;

			for _, candidate in ipairs(info.sequences or {}) do
				if ( ME.IsAvailable(candidate) ) then
					ME.previewSequence = candidate.index;
					break;
				end
			end
		end

		ME.PreviewReload(false);
	elseif ( ME.current and (docs[ME.current] or docs.all) ) then
		reloadIn = RELOAD_DELAY;
	end

	RefreshControls();
end);
