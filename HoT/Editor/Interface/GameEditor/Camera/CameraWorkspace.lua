-- Cinematic camera workspace: edits a camera M2 in the world, the timeline sits under the view.

local CE = GameEditorCamera;
local Common = GameEditorObject;

local LABEL_WIDTH = 86;
local SPIN_WIDTH = 92;
local COORD = 40000;
local DEFAULT_PATH = "Cameras\\NewCamera.m2";

local frame;
local scroll;
local child;
local widgets = {};
local keyWidgets = {};
local trackTabs;
local keyList;
local dbcWindow;

local function HasFocus(spin)
	local edit = _G[spin:GetName().."Edit"];
	return edit and edit.HasFocus and edit:HasFocus();
end

local function SetSpin(spin, value)
	if ( spin and not HasFocus(spin) ) then
		NoggitSpinBox_SetValue(spin, tonumber(value) or 0, true);
	end
end

-- Hooks so the widget keeps its own hover scripts.
local function Tip(widget, text)
	widget:HookScript("OnEnter", function(self) NoggitTooltip_Show(self, text); end);
	widget:HookScript("OnLeave", NoggitTooltip_Hide);
end

local function Show(widget, shown)
	if ( shown ) then
		widget:Show();
	else
		widget:Hide();
	end
end

-- Layout helpers ------------------------------------------------------------------------------------------

local function Header(y, text)
	local label = Common.Label(child, text, "NoggitFontBold", NOGGIT_COLOR_GOLD);
	label:SetPoint("TOPLEFT", child, "TOPLEFT", 6, y);
	local line = NoggitSeparator_Create(child);
	line:SetPoint("LEFT", label, "RIGHT", 6, 0);
	line:SetPoint("RIGHT", child, "RIGHT", -6, 0);
	return y - 22;
end

local function Label(parent, y, text, x)
	local label = Common.Label(parent, text);
	label:SetPoint("TOPLEFT", parent, "TOPLEFT", x or 8, y - 4);
	return label;
end

local function Spin(parent, x, y, minValue, maxValue, step, decimals, onChange, width)
	local spin = NoggitSpinBox_Create(parent, width or SPIN_WIDTH, minValue, maxValue, step, decimals, onChange);
	spin:SetPoint("TOPLEFT", parent, "TOPLEFT", x, y);
	return spin;
end

-- Three spins in a row with X, Y and Z in front.
local function Vector(parent, y, text, step, decimals, onChange, range)
	local label = Label(parent, y, text);
	local spins = {};
	local x = LABEL_WIDTH;

	for axis, name in ipairs({ "X", "Y", "Z" }) do
		local axisLabel = Common.Label(parent, name, nil, NOGGIT_COLOR_DISABLED);
		axisLabel:SetPoint("TOPLEFT", parent, "TOPLEFT", x, y - 4);
		spins[axis] = Spin(parent, x + 12, y, -(range or COORD), range or COORD, step, decimals, function()
			onChange(spins[1].value, spins[2].value, spins[3].value);
		end);
		x = x + SPIN_WIDTH + 18;
	end

	spins.label = label;
	return spins;
end

-- Document sections ---------------------------------------------------------------------------------------

local function ApplyOrigin()
	CameraEditor_SetOrigin(widgets.origin[1].value, widgets.origin[2].value, widgets.origin[3].value, CE.Rad(widgets.facing.value), CE.settings.keepWorld);
end

local function OriginToCharacter()
	local x, y, z, facing = CameraEditor_GetPlayerPosition();

	if ( x ) then
		CameraEditor_SetOrigin(x, y, z, facing, CE.settings.keepWorld);
	end
end

local function OriginToView()
	local x, y, z = CameraEditor_GetViewPosition();

	if ( x ) then
		CameraEditor_SetOrigin(x, y, z, CE.state.doc.facing or 0, CE.settings.keepWorld);
	end
end

local function PathText()
	local text = widgets.path:GetText();
	return (text and text ~= "") and text or CE.state.doc.path;
end

local function SaveAs()
	CE.Save(PathText() or DEFAULT_PATH);
end

local function SaveCurrent()
	local path = CE.state.doc.path;
	CE.Save((path and path ~= "") and path or (PathText() or DEFAULT_PATH));
end

local function ShowDbcWindow()
	if ( not dbcWindow ) then
		dbcWindow = GameEditor_CreateWindow("GameEditorCameraDbcWindow", "CinematicCamera.dbc", 520, 420, true);
		local content = dbcWindow.content;

		local filter = NoggitLineEdit_Create(content, 200);
		filter:SetPoint("TOPLEFT", content, "TOPLEFT", 0, 0);
		filter:SetPoint("TOPRIGHT", content, "TOPRIGHT", 0, 0);

		local list = NoggitListBox_Create(content, 20, function(row)
			row.idText = NoggitUI_FontString(row, "");
			row.idText:SetPoint("LEFT", row, "LEFT", 6, 0);
			row.idText:SetWidth(50);
			row.model = NoggitUI_FontString(row, "");
			row.model:SetPoint("LEFT", row, "LEFT", 60, 0);
			row.model:SetPoint("RIGHT", row, "RIGHT", -4, 0);
		end, function(row, item)
			row.idText:SetText(item.id);
			row.model:SetText(item.model);
		end);
		list:SetPoint("TOPLEFT", filter, "BOTTOMLEFT", 0, -6);
		list:SetPoint("BOTTOMRIGHT", content, "BOTTOMRIGHT", 0, 30);
		list.onDoubleClick = function(item)
			GameEditor_HideWindow(dbcWindow);
			CE.OpenDbc(item.id);
		end;

		local open = Common.Button(content, "Open", 80, function()
			local item = list.selected and list.items[list.selected];

			if ( item ) then
				GameEditor_HideWindow(dbcWindow);
				CE.OpenDbc(item.id);
			end
		end, "Open the row's model with its origin and facing (double click a row does the same)");
		open:SetPoint("BOTTOMRIGHT", content, "BOTTOMRIGHT", 0, 0);

		local function Fill()
			local text = string.lower(filter:GetText() or "");
			local items = {};

			for _, row in ipairs(CameraEditor_GetDbcRows() or {}) do
				if ( text == "" or string.find(string.lower(row.model), text, 1, true) or tostring(row.id) == text ) then
					table.insert(items, row);
				end
			end

			NoggitListBox_SetItems(list, items);
		end

		filter:SetScript("OnTextChanged", Fill);
		filter:SetScript("OnEscapePressed", function(self) self:ClearFocus(); end);
		filter:SetScript("OnEnterPressed", function(self) self:ClearFocus(); end);
		dbcWindow.fill = Fill;
	end

	dbcWindow.fill();
	GameEditor_ShowWindow(dbcWindow);
end

local function BuildFile(y)
	y = Header(y, "Camera file");

	widgets.path = NoggitLineEdit_Create(child, 300);
	widgets.path:SetHeight(22);
	widgets.path:SetPoint("TOPLEFT", child, "TOPLEFT", 8, y);
	widgets.path:SetPoint("TOPRIGHT", child, "TOPRIGHT", -8, y);
	widgets.path:SetText(DEFAULT_PATH);
	widgets.path:SetScript("OnEnterPressed", function(self) self:ClearFocus(); end);
	widgets.path:SetScript("OnEscapePressed", function(self) self:ClearFocus(); end);
	y = y - 28;

	local new = Common.Button(child, "New", 60, CE.New, "Start an empty camera at your character (Ctrl+N)");
	new:SetPoint("TOPLEFT", child, "TOPLEFT", 8, y);
	local open = Common.Button(child, "Open", 60, function() CE.Open(PathText()); end, "Open the M2 named above, any model with a camera works");
	open:SetPoint("LEFT", new, "RIGHT", 4, 0);
	local dbc = Common.Button(child, "From DBC...", 90, ShowDbcWindow, "Pick a CinematicCamera.dbc row, its model opens with the row's origin and facing");
	dbc:SetPoint("LEFT", open, "RIGHT", 4, 0);
	local save = Common.Button(child, "Save", 60, SaveCurrent, "Save to the opened file (Ctrl+S)");
	save:SetPoint("LEFT", dbc, "RIGHT", 4, 0);
	local saveAs = Common.Button(child, "Save as", 70, SaveAs, "Save to the path typed above, the 00.skin is written next to it");
	saveAs:SetPoint("LEFT", save, "RIGHT", 4, 0);
	y = y - 28;

	widgets.fileInfo = Common.Paragraph(child, "");
	widgets.fileInfo:SetPoint("TOPLEFT", child, "TOPLEFT", 8, y);
	widgets.fileInfo:SetPoint("TOPRIGHT", child, "TOPRIGHT", -8, y);
	return y - 34;
end

local function BuildOrigin(y)
	y = Header(y, "Origin (CinematicCamera.dbc)");

	widgets.origin = Vector(child, y, "Origin", 0.5, 3, ApplyOrigin);
	y = y - 26;

	Label(child, y, "Facing");
	widgets.facing = Spin(child, LABEL_WIDTH + 12, y, -360, 360, 1, 2, ApplyOrigin);
	local degrees = Common.Label(child, "degrees", nil, NOGGIT_COLOR_DISABLED);
	degrees:SetPoint("LEFT", widgets.facing, "RIGHT", 6, 0);
	y = y - 28;

	local character = Common.Button(child, "To character", 100, OriginToCharacter, "Put the origin and facing at your character");
	character:SetPoint("TOPLEFT", child, "TOPLEFT", 8, y);
	local view = Common.Button(child, "To view", 80, OriginToView, "Put the origin at the camera, the facing stays");
	view:SetPoint("LEFT", character, "RIGHT", 4, 0);
	local keep = NoggitCheckBox_Create(child, "Keep the path in place", CE.settings.keepWorld, function(_, checked) CE.settings.keepWorld = checked; end);
	keep:SetPoint("LEFT", view, "RIGHT", 10, 0);
	Tip(keep, "On: moving the origin leaves the path where it is in the world. Off: the path moves with the origin.");
	y = y - 28;

	widgets.dbcText = Common.Paragraph(child, "");
	widgets.dbcText:SetPoint("TOPLEFT", child, "TOPLEFT", 8, y);
	widgets.dbcText:SetPoint("TOPRIGHT", child, "TOPRIGHT", -8, y);
	return y - 32;
end

local function BuildTiming(y)
	y = Header(y, "Timing");

	Label(child, y, "Length");
	widgets.length = Spin(child, LABEL_WIDTH + 12, y, 0.1, 3600, 0.5, 2, function(_, value)
		CameraEditor_SetLength(value * 1000, CE.settings.scaleKeys);
	end);
	local seconds = Common.Label(child, "seconds", nil, NOGGIT_COLOR_DISABLED);
	seconds:SetPoint("LEFT", widgets.length, "RIGHT", 6, 0);
	local scale = NoggitCheckBox_Create(child, "Scale keys", CE.settings.scaleKeys, function(_, checked) CE.settings.scaleKeys = checked; end);
	scale:SetPoint("LEFT", seconds, "RIGHT", 12, 0);
	Tip(scale, "Stretch every key time with the new length, off keeps the keys where they are");
	y = y - 28;

	local even = Common.Button(child, "Even out speed", 110, CameraEditor_ConstantSpeed, "Retime the position keys by distance so the camera moves at one speed, the target and roll keys follow");
	even:SetPoint("TOPLEFT", child, "TOPLEFT", 8, y);
	widgets.shift = Spin(child, 130, y, -600, 600, 0.1, 2, nil, 74);
	NoggitSpinBox_SetValue(widgets.shift, 0.5, true);
	local shift = Common.Button(child, "Shift all keys", 100, function()
		CameraEditor_ShiftTimes(widgets.shift.value * 1000);
	end, "Move every key of every track by that many seconds");
	shift:SetPoint("LEFT", widgets.shift, "RIGHT", 4, 0);
	y = y - 28;

	widgets.timingInfo = Common.Paragraph(child, "");
	widgets.timingInfo:SetPoint("TOPLEFT", child, "TOPLEFT", 8, y);
	widgets.timingInfo:SetPoint("TOPRIGHT", child, "TOPRIGHT", -8, y);
	return y - 22;
end

local function BuildSmooth(y)
	y = Header(y, "Smooth");
	local settings = CE.settings;

	local function Setting(x, rowY, text, key, minValue, maxValue, step, decimals, tip)
		local label = Common.Label(child, text, nil, NOGGIT_COLOR_DISABLED);
		label:SetPoint("TOPLEFT", child, "TOPLEFT", x, rowY - 4);
		local spin = NoggitSpinBox_Create(child, 64, minValue, maxValue, step, decimals, function(_, value) settings[key] = value; end);
		spin:SetPoint("LEFT", label, "RIGHT", 6, 0);
		NoggitSpinBox_SetValue(spin, settings[key], true);
		Tip(spin, tip);
		return spin;
	end

	Setting(8, y, "Look over", "lookSeconds", 0, 30, 0.25, 2, "Seconds to average the aim over, this removes the left and right swing of a hand flown camera. 0 keeps the aim at each key exactly but still stops it swinging between keys.");
	Setting(200, y, "Path over", "pathSeconds", 0, 30, 0.25, 2, "Seconds to average the position keys over, 0 leaves the path alone. The first and last keys never move.");
	y = y - 26;

	local bank = NoggitCheckBox_Create(child, "Bank into turns", settings.bank, function(_, checked) settings.bank = checked; end);
	bank:SetPoint("TOPLEFT", child, "TOPLEFT", 8, y);
	Tip(bank, "Rebuild the roll track so the camera leans into turns like a plane, harder the faster it turns. Off leaves the roll track alone.");
	Setting(140, y, "Strength", "bankStrength", 0, 4, 0.1, 2, "How hard the camera leans for a given turn, 1 is a real plane");
	Setting(290, y, "Max", "maxBank", 0, 75, 1, 0, "Most lean in degrees");
	y = y - 28;

	local smooth = Common.Button(child, "Smooth camera", 120, CE.Smooth, "Rebuild the target (and roll) keys on the position keys from a smoothed aim, Ctrl+Z undoes it");
	smooth:SetPoint("TOPLEFT", child, "TOPLEFT", 8, y);
	local clearRoll = Common.Button(child, "Clear roll", 80, function() CameraEditor_ClearTrack(CE.ROLL); end, "Delete every roll key, the camera stays level");
	clearRoll:SetPoint("LEFT", smooth, "RIGHT", 4, 0);
	local even = Common.Button(child, "Even out speed", 110, CameraEditor_ConstantSpeed, "Retime the position keys by distance so the camera moves at one speed");
	even:SetPoint("LEFT", clearRoll, "RIGHT", 4, 0);
	y = y - 26;

	local help = Common.Paragraph(child, "The client rolls around the world X axis, so banking fades out while the camera faces along Y. Smooth again after moving position keys, the target follows the keys as they were.");
	help:SetPoint("TOPLEFT", child, "TOPLEFT", 8, y);
	help:SetPoint("TOPRIGHT", child, "TOPRIGHT", -8, y);
	return y - 46;
end

local function BuildCapture(y)
	y = Header(y, "Capture");

	local append = Common.Button(child, "Add view as next key (Shift+K)", 200, CE.Append, "Add the current view as a new key one step after the last key, the length grows to fit");
	append:SetPoint("TOPLEFT", child, "TOPLEFT", 8, y);
	local capture = Common.Button(child, "Key at playhead (K)", 140, CE.Capture, "Key the view at the playhead, replacing a key already there, then move the playhead on one step");
	capture:SetPoint("LEFT", append, "RIGHT", 4, 0);
	y = y - 28;

	Label(child, y, "Step");
	widgets.advance = NoggitSpinBox_Create(child, 70, 0, 600, 0.5, 2, function(_, value) CE.settings.advance = value; end);
	widgets.advance:SetPoint("TOPLEFT", child, "TOPLEFT", 48, y);
	NoggitSpinBox_SetValue(widgets.advance, CE.settings.advance, true);
	Tip(widgets.advance, "Seconds between keys added with Shift+K, and how far the playhead moves on after K. 0 keeps the playhead still.");
	local label = Common.Label(child, "s   Target distance", nil, NOGGIT_COLOR_DISABLED);
	label:SetPoint("LEFT", widgets.advance, "RIGHT", 6, 0);
	widgets.targetDistance = NoggitSpinBox_Create(child, 70, 0, 1000, 5, 0, function(_, value) CE.settings.targetDistance = value; end);
	widgets.targetDistance:SetPoint("LEFT", label, "RIGHT", 6, 0);
	NoggitSpinBox_SetValue(widgets.targetDistance, CE.settings.targetDistance, true);
	Tip(widgets.targetDistance, "How far ahead of the view the target key goes, 0 puts it on the surface under the middle of the screen");
	y = y - 26;

	local help = Common.Paragraph(child, "Fly the free camera to frame each shot and press Shift+K to add it as the next key. K keys the playhead instead, a key already there is replaced. Click keys in the world to select them, drag them or use the arrows, ctrl drags them level and alt click drops them on the ground.");
	help:SetPoint("TOPLEFT", child, "TOPLEFT", 8, y);
	help:SetPoint("TOPRIGHT", child, "TOPRIGHT", -8, y);
	return y - 58;
end

-- Track and key sections ----------------------------------------------------------------------------------

local function SelectTrack(track)
	CE.state.track = track;

	for index, tab in ipairs(trackTabs) do
		tab.selected = index == track;
		NoggitTab_Refresh(tab);
	end

	CE.RefreshPanel();
end

local function KeyLabel(track, key)
	if ( track == CE.ROLL ) then
		return format("%.1f deg", CE.Deg(key.x));
	end

	return format("%.1f, %.1f, %.1f", key.wx, key.wy, key.wz);
end

local function BuildTrack(y)
	y = Header(y, "Tracks");

	trackTabs = NoggitTabs_Create(child, CE.TRACK_NAMES, CE.state.track, SelectTrack);
	trackTabs[1]:SetPoint("TOPLEFT", child, "TOPLEFT", 8, y);
	y = y - 30;

	-- The curve and handle buttons hit the shown track, or all three with All tracks on.
	local function ForTracks(action)
		if ( CE.settings.allTracks ) then
			for track = 1, 3 do
				action(track);
			end
		else
			action(CE.state.track);
		end
	end

	Label(child, y, "Curve");
	widgets.interp = NoggitComboBox_Create(child, 200, CE.INTERP_ITEMS, 3, function(_, index)
		ForTracks(function(track) CameraEditor_SetInterp(track, index - 1); end);
	end);
	widgets.interp:SetPoint("TOPLEFT", child, "TOPLEFT", LABEL_WIDTH, y);
	Tip(widgets.interp, "Bezier and Hermite are both smooth and convert into each other without changing the path. Linear ignores the handles, None holds each key until the next.");
	local allTracks = NoggitCheckBox_Create(child, "All tracks", CE.settings.allTracks, function(_, checked) CE.settings.allTracks = checked; end);
	allTracks:SetPoint("LEFT", widgets.interp, "RIGHT", 10, 0);
	Tip(allTracks, "The curve type, All auto and All free change position, target and roll together instead of only the shown track");
	y = y - 28;

	keyList = NoggitListBox_Create(child, 18, function(row)
		row.time = NoggitUI_FontString(row, "");
		row.time:SetPoint("LEFT", row, "LEFT", 6, 0);
		row.time:SetWidth(60);
		row.value = NoggitUI_FontString(row, "");
		row.value:SetPoint("LEFT", row, "LEFT", 70, 0);
		row.value:SetWidth(220);
		row.mode = NoggitUI_FontString(row, "", nil, "RIGHT");
		row.mode:SetPoint("RIGHT", row, "RIGHT", -4, 0);
		row.mode:SetWidth(70);
		NoggitText_SetColor(row.mode, NOGGIT_COLOR_DISABLED);
	end, function(row, item)
		row.time:SetText(CE.Seconds(item.key.time).." s");
		row.value:SetText(KeyLabel(CE.state.track, item.key));
		row.mode:SetText(CE.IsSpline(CE.state.track) and CE.MODE_SHORT[item.key.mode] or "");
	end);
	keyList:SetPoint("TOPLEFT", child, "TOPLEFT", 8, y);
	keyList:SetPoint("TOPRIGHT", child, "TOPRIGHT", -8, y);
	keyList:SetHeight(130);
	keyList.onSelect = function(item)
		CameraEditor_Select(CE.state.track, item.index, CE.PART_VALUE);
	end;
	keyList.onDoubleClick = function(item)
		CameraEditor_Play(false);
		CameraEditor_SetTime(item.key.time);
	end;
	y = y - 136;

	local add = Common.Button(child, "Add at playhead", 110, function() CE.AddKey(); end, "Add a key to this track at the playhead, on the curve as it is now (double click a timeline lane does the same)");
	add:SetPoint("TOPLEFT", child, "TOPLEFT", 8, y);
	local delete = Common.Button(child, "Delete", 60, CE.DeleteSelected, "Delete the selected key (Delete)");
	delete:SetPoint("LEFT", add, "RIGHT", 4, 0);
	local auto = Common.Button(child, "All auto", 70, function() ForTracks(function(track) CameraEditor_SetTrackMode(track, 0); end); end, "Every key gets automatic smooth handles, on the shown track or all of them with All tracks on");
	auto:SetPoint("LEFT", delete, "RIGHT", 4, 0);
	local free = Common.Button(child, "All free", 70, function() ForTracks(function(track) CameraEditor_SetTrackMode(track, 3); end); end, "Every key keeps its handles exactly as they are, on the shown track or all of them with All tracks on");
	free:SetPoint("LEFT", auto, "RIGHT", 4, 0);
	return y - 30;
end

local function BuildKey(y)
	y = Header(y, "Selected key");
	local box = CreateFrame("Frame", nil, child);
	box:SetPoint("TOPLEFT", child, "TOPLEFT", 0, y);
	box:SetPoint("TOPRIGHT", child, "TOPRIGHT", 0, y);
	box:SetHeight(210);
	keyWidgets.box = box;

	keyWidgets.none = Common.Paragraph(child, "Select a key in the list, on the timeline or in the world.");
	keyWidgets.none:SetPoint("TOPLEFT", child, "TOPLEFT", 8, y);
	keyWidgets.none:SetPoint("TOPRIGHT", child, "TOPRIGHT", -8, y);

	local row = 0;
	Label(box, row, "Time");
	keyWidgets.time = Spin(box, LABEL_WIDTH + 12, row, 0, 3600, 0.05, 3, function(_, value)
		local _, track, index = CE.SelectedKey();

		if ( track ) then
			CameraEditor_SetKeyTime(track, index, value * 1000);
		end
	end);
	local seconds = Common.Label(box, "seconds", nil, NOGGIT_COLOR_DISABLED);
	seconds:SetPoint("LEFT", keyWidgets.time, "RIGHT", 6, 0);
	row = row - 26;

	keyWidgets.value = Vector(box, row, "World", 0.25, 3, function(x, y2, z)
		local _, track, index = CE.SelectedKey();

		if ( track and track ~= CE.ROLL ) then
			CameraEditor_SetKeyValue(track, index, x, y2, z, true);
		end
	end);
	keyWidgets.roll = Spin(box, LABEL_WIDTH + 12, row, -3600, 3600, 1, 2, function(_, value)
		local _, track, index = CE.SelectedKey();

		if ( track == CE.ROLL ) then
			CameraEditor_SetKeyValue(track, index, CE.Rad(value), 0, 0, false);
		end
	end);
	keyWidgets.rollLabel = Common.Label(box, "degrees", nil, NOGGIT_COLOR_DISABLED);
	keyWidgets.rollLabel:SetPoint("LEFT", keyWidgets.roll, "RIGHT", 6, 0);
	row = row - 26;

	keyWidgets.localText = Common.Label(box, "", nil, NOGGIT_COLOR_DISABLED);
	keyWidgets.localText:SetPoint("TOPLEFT", box, "TOPLEFT", LABEL_WIDTH, row - 4);
	row = row - 22;

	keyWidgets.modeLabel = Label(box, row, "Handles");
	keyWidgets.mode = NoggitComboBox_Create(box, 200, CE.MODE_ITEMS, 1, function(_, index)
		local _, track, keyIndex = CE.SelectedKey();

		if ( track ) then
			CameraEditor_SetKeyMode(track, keyIndex, index - 1);
		end
	end);
	keyWidgets.mode:SetPoint("TOPLEFT", box, "TOPLEFT", LABEL_WIDTH, row);
	Tip(keyWidgets.mode, "Auto: handles follow the neighbours so the speed is continuous. Smooth: you place them, the other side matches the speed. Aligned: straight through, lengths free. Free: both sides independent. Stop: the camera eases to a halt on the key.");
	row = row - 28;

	local function Handle(out)
		return function(x, y2, z)
			local _, track, index = CE.SelectedKey();

			if ( track ) then
				CameraEditor_SetHandle(track, index, out, x, y2, z);
			end
		end;
	end

	keyWidgets.inHandle = Vector(box, row, "In handle", 0.1, 3, Handle(false), 10000);
	row = row - 26;
	keyWidgets.outHandle = Vector(box, row, "Out handle", 0.1, 3, Handle(true), 10000);
	row = row - 28;

	local goTo = Common.Button(box, "Playhead here", 100, function()
		local key = CE.SelectedKey();

		if ( key ) then
			CameraEditor_Play(false);
			CameraEditor_SetTime(key.time);
		end
	end, "Move the playhead to this key");
	goTo:SetPoint("TOPLEFT", box, "TOPLEFT", 8, row);
	keyWidgets.fly = Common.Button(box, "Fly here", 70, function()
		local _, track, index = CE.SelectedKey();

		if ( track ) then
			CameraEditor_GoToKey(track, index);
		end
	end, "Move the free camera onto this key");
	keyWidgets.fly:SetPoint("LEFT", goTo, "RIGHT", 4, 0);
	keyWidgets.toView = Common.Button(box, "Set to view", 86, function()
		local _, track, index = CE.SelectedKey();
		local x, y2, z = CameraEditor_GetViewPosition();

		if ( track == CE.POSITION and x ) then
			CameraEditor_SetKeyValue(track, index, x, y2, z, true);
		end
	end, "Move this position key to where the view is now");
	keyWidgets.toView:SetPoint("LEFT", keyWidgets.fly, "RIGHT", 4, 0);
	local delete = Common.Button(box, "Delete", 60, CE.DeleteSelected, "Delete this key (Delete)");
	delete:SetPoint("LEFT", keyWidgets.toView, "RIGHT", 4, 0);

	return y - 214;
end

local function BuildDisplay(y)
	y = Header(y, "Show in the world");
	local display = CameraEditor_GetDisplay() or {};
	local options = {
		{ "target", "Target path" },
		{ "speed", "Speed colours" },
		{ "handles", "Handles" },
		{ "allHandles", "Every key's handles" },
		{ "frustum", "Camera at playhead" },
	};
	local x = 8;
	local rowY = y;

	for index, option in ipairs(options) do
		local box = NoggitCheckBox_Create(child, option[2], display[option[1]], function(_, checked) CameraEditor_SetDisplay(option[1], checked); end);
		box:SetPoint("TOPLEFT", child, "TOPLEFT", x, rowY);
		widgets["display_"..option[1]] = box;

		if ( index == 3 ) then
			x = 8;
			rowY = rowY - 22;
		else
			x = x + 140;
		end
	end

	return rowY - 28;
end

-- Refresh -------------------------------------------------------------------------------------------------

local function RefreshKey()
	local key, track = CE.SelectedKey();
	Show(keyWidgets.none, not key);
	Show(keyWidgets.box, key ~= nil);

	if ( not key ) then
		return;
	end

	local spline = CE.IsSpline(track);
	local roll = track == CE.ROLL;
	SetSpin(keyWidgets.time, key.time / 1000);

	for axis, field in ipairs({ "wx", "wy", "wz" }) do
		local spin = keyWidgets.value[axis];
		Show(spin, not roll);
		SetSpin(spin, key[field]);
	end

	Show(keyWidgets.value.label, true);
	keyWidgets.value.label:SetText(roll and "Roll" or "World");
	Show(keyWidgets.roll, roll);
	Show(keyWidgets.rollLabel, roll);
	SetSpin(keyWidgets.roll, CE.Deg(key.x));
	keyWidgets.localText:SetText(roll and "" or format("Local to the origin: %.3f, %.3f, %.3f", key.x, key.y, key.z));

	Show(keyWidgets.mode, spline);
	Show(keyWidgets.modeLabel, spline);
	NoggitComboBox_SetSelected(keyWidgets.mode, (key.mode or 0) + 1, true);

	local count = #(CE.state.keys[track] or {});
	local index = CE.state.selIndex or 1;

	for _, entry in ipairs({ { keyWidgets.inHandle, "in", index > 1 }, { keyWidgets.outHandle, "out", index < count } }) do
		local spins, prefix, exists = entry[1], entry[2], entry[3];
		local shown = spline and exists;
		Show(spins.label, shown);

		for axis, suffix in ipairs({ "X", "Y", "Z" }) do
			Show(spins[axis], shown and (not roll or axis == 1));
			SetSpin(spins[axis], key[prefix..suffix]);
		end
	end

	Show(keyWidgets.fly, not roll);
	Show(keyWidgets.toView, track == CE.POSITION);
end

function CE.RefreshPanel()
	if ( not frame ) then
		return;
	end

	local state = CE.state;
	local doc = state.doc;

	if ( doc.path and doc.path ~= "" and not widgets.path:HasFocus() and widgets.path.shownPath ~= doc.path ) then
		widgets.path:SetText(doc.path);
		widgets.path.shownPath = doc.path;
	end

	local fileText = (doc.path and doc.path ~= "") and doc.path or "Not saved yet";

	if ( doc.dirty ) then
		fileText = fileText.."  |cffffcc00unsaved changes|r";
	end

	if ( doc.warnings and doc.warnings ~= "" ) then
		fileText = fileText.."\n|cffff8040"..string.gsub(doc.warnings, "\n", " ").."|r";
	end

	widgets.fileInfo:SetText(fileText);

	SetSpin(widgets.origin[1], doc.x);
	SetSpin(widgets.origin[2], doc.y);
	SetSpin(widgets.origin[3], doc.z);
	SetSpin(widgets.facing, CE.Deg(doc.facing));
	widgets.dbcText:SetText(format("Row values: origin %.3f %.3f %.3f, facing %.4f radians", doc.x or 0, doc.y or 0, doc.z or 0, doc.facing or 0));

	SetSpin(widgets.length, (doc.length or 0) / 1000);
	local pathLength = tonumber(doc.pathLength) or 0;
	widgets.timingInfo:SetText(format("Path %.1f yards, %.1f yards a second on average", pathLength, pathLength / max(0.001, (doc.length or 1) / 1000)));

	for index, tab in ipairs(trackTabs) do
		tab.selected = index == state.track;
		NoggitTab_Refresh(tab);
	end

	local interp = doc.interp and doc.interp[state.track] or 2;
	NoggitComboBox_SetSelected(widgets.interp, interp + 1, true);

	local items = {};

	for index, key in ipairs(state.keys[state.track] or {}) do
		table.insert(items, { index = index, key = key });
	end

	NoggitListBox_SetItems(keyList, items, true);
	keyList.selected = (state.selTrack == state.track) and state.selIndex or nil;
	NoggitListBox_Refresh(keyList);

	RefreshKey();
end

-- Panel ---------------------------------------------------------------------------------------------------

local function Build(parent)
	frame = CreateFrame("Frame", "GameEditorCameraPanel", parent);
	scroll, child = GameEditor_CreateScrollFrame(frame);
	scroll:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, -2);
	scroll:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", 0, 2);

	local y = -4;
	y = BuildFile(y);
	y = BuildOrigin(y);
	y = BuildTiming(y);
	y = BuildCapture(y);
	y = BuildSmooth(y);
	y = BuildTrack(y);
	y = BuildKey(y);
	y = BuildDisplay(y);

	child:SetHeight(-y + 8);
	scroll.UpdateRange();

	CE.Listen(CE.RefreshPanel);
	CE.Refresh();
	return frame;
end

-- Menus, keys and status ----------------------------------------------------------------------------------

local separator = { separator = true };

local function DisplayItem(text, name)
	return {
		text = text,
		checked = function() return (CameraEditor_GetDisplay() or {})[name]; end,
		onClick = function()
			local display = CameraEditor_GetDisplay() or {};
			CameraEditor_SetDisplay(name, not display[name]);
			local box = widgets["display_"..name];

			if ( box ) then
				NoggitCheckBox_SetChecked(box, not display[name]);
			end
		end,
	};
end

local function BuildMenus()
	return {
		{
			title = "Camera",
			{ text = "New camera", shortcut = "Ctrl+N", onClick = CE.New },
			{ text = "Open the typed path", onClick = function() CE.Open(PathText()); end },
			{ text = "Open from CinematicCamera.dbc", onClick = ShowDbcWindow },
			separator,
			{ text = "Save", shortcut = "Ctrl+S", onClick = SaveCurrent },
			{ text = "Save as the typed path", onClick = SaveAs },
		},
		{
			title = "Edit",
			{ text = "Undo", shortcut = "Ctrl+Z", onClick = CameraEditor_Undo },
			{ text = "Redo", shortcut = "Ctrl+Y", onClick = CameraEditor_Redo },
			separator,
			{ text = "Add the view as the next key", shortcut = "Shift+K", onClick = CE.Append },
			{ text = "Key the view at the playhead", shortcut = "K", onClick = CE.Capture },
			{ text = "Add a key to the shown track", onClick = function() CE.AddKey(); end },
			{ text = "Delete the selected key", shortcut = "Delete", onClick = CE.DeleteSelected },
			separator,
			{ text = "Even out the speed", onClick = CameraEditor_ConstantSpeed },
			{ text = "Smooth the camera", onClick = CE.Smooth },
			{ text = "Clear the roll", onClick = function() CameraEditor_ClearTrack(CE.ROLL); end },
			{ text = "Auto handles on every track", onClick = function()
				for track = 1, 3 do
					CameraEditor_SetTrackMode(track, 0);
				end
			end },
		},
		{
			title = "Play",
			{ text = "Play or pause", shortcut = "P", onClick = CE.TogglePlay },
			{ text = "Back to the start", shortcut = "Home", onClick = CE.Rewind },
			{ text = "Look through the camera", shortcut = "V", checked = function() return select(3, CE.GetTime()); end, onClick = CE.TogglePreview },
			{ text = "Loop", checked = function() return select(5, CE.GetTime()); end, onClick = CE.ToggleLoop },
			separator,
			{ text = "Play in engine", onClick = CE.ToggleEngine },
		},
		{
			title = "View",
			DisplayItem("Target path", "target"),
			DisplayItem("Speed colours", "speed"),
			DisplayItem("Handles of the selected key", "handles"),
			DisplayItem("Handles of every key", "allHandles"),
			DisplayItem("Camera at the playhead", "frustum"),
		},
	};
end

local KEYS = {
	{ "Shift+K", "Add the view as a new key one step after the last key" },
	{ "K", "Key the view at the playhead (replaces a key there), then step the playhead on" },
	{ "P", "Play or pause" },
	{ "V", "Look through the camera at the playhead" },
	{ "[ and ]", "Previous and next key of the shown track" },
	{ "Home", "Back to the start" },
	{ "Delete", "Delete the selected key" },
	{ "Ctrl+Z, Ctrl+Y", "Undo and redo" },
	{ "Ctrl+S", "Save" },
	{ "Ctrl+N", "New camera" },
	{ "Click", "Select a key or handle in the world, drag to move it across the view" },
	{ "Ctrl drag", "Move the point level" },
	{ "Alt click", "Drop the selected point on the surface under the mouse" },
	{ "Timeline", "Click to scrub, drag a key to retime it, double click a lane to add a key" },
};

local HOTKEYS = {
	["K"] = CE.Capture,
	["SHIFT-K"] = CE.Append,
	["P"] = CE.TogglePlay,
	["V"] = CE.TogglePreview,
	["["] = function() CE.StepKey(-1); end,
	["]"] = function() CE.StepKey(1); end,
	["HOME"] = CE.Rewind,
	["DELETE"] = CE.DeleteSelected,
	["CTRL-Z"] = CameraEditor_Undo,
	["CTRL-Y"] = CameraEditor_Redo,
	["CTRL-SHIFT-Z"] = CameraEditor_Redo,
	["CTRL-S"] = SaveCurrent,
	["CTRL-N"] = CE.New,
};

local function BuildStatus()
	local doc = CE.state.doc;
	local time = CE.GetTime();
	local cells = {};
	table.insert(cells, ((doc.path and doc.path ~= "") and doc.path or "New camera")..(doc.dirty and " *" or ""));
	table.insert(cells, format("%s / %s s", CE.Seconds(time), CE.Seconds(doc.length)));

	local counts = doc.counts or {};
	table.insert(cells, format("%d position, %d target, %d roll keys", counts[1] or 0, counts[2] or 0, counts[3] or 0));
	return cells;
end

GameEditor_RegisterWorkspace({
	key = "camera",
	title = "Cinematic Camera",
	category = "Visuals",
	order = 12,
	dockWidth = 470,
	tools = {
		{ key = "camera.edit", name = "Camera path", icon = 0xF8D8, build = Build },
	},
	buildMain = CE.BuildTimeline,
	menus = BuildMenus,
	keys = KEYS,
	onActivate = function()
		CameraEditor_SetActive(true);
		CE.Refresh();
	end,
	onDeactivate = function()
		CameraEditor_SetActive(false);
	end,
	onUpdate = function()
		CE.Poll();
		CE.TimelineUpdate();
	end,
	onHotkey = function(key)
		local handler = HOTKEYS[key];

		if ( handler ) then
			handler();
			return true;
		end

		return false;
	end,
	status = BuildStatus,
});
