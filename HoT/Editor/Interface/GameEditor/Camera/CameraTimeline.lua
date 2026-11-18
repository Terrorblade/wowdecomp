-- Timeline under the world view: transport, ruler, speed graph and one lane of keys per track.

local CE = GameEditorCamera;
local Common = GameEditorObject;

local HEIGHT = 176;
local LABEL_WIDTH = 78;
local BAR_HEIGHT = 28;
local RULER_HEIGHT = 16;
local GRAPH_HEIGHT = 46;
local LANE_HEIGHT = 20;
local BAR_COUNT = 160;
local MARKER_WIDTH = 9;
local DOUBLE_CLICK = 0.35;

local TRACK_COLORS = {
	{ 0.35, 0.67, 1 },
	{ 1, 0.59, 0.16 },
	{ 0.75, 0.55, 1 },
};

local frame;
local area;
local ruler;
local graph;
local graphMax;
local lanes = {};
local markers = { {}, {}, {} };
local bars = {};
local ticks = {};
local playhead;
local timeText;
local playButton;
local previewBox;
local loopBox;
local engineButton;
local rateCombo;

local scrubbing = false;
local drag;
local lastLaneClick = {};

local RATES = { 0.25, 0.5, 1, 2 };

local function Width()
	return max(1, area:GetWidth() or 1);
end

local function TimeToX(ms)
	return (ms / CE.Length()) * Width();
end

local function CursorTime()
	local x = NoggitUI_CursorIn(area);
	local fraction = (x - (area:GetLeft() or 0)) / Width();
	return max(0, min(1, fraction)) * CE.Length();
end

local function Snap(ms)
	if ( IsControlKeyDown() ) then
		return floor(ms + 0.5);
	end

	return floor(ms / 10 + 0.5) * 10;
end

-- Ruler ---------------------------------------------------------------------------------------------------

local STEPS = { 50, 100, 250, 500, 1000, 2000, 5000, 10000, 30000, 60000 };

local function RefreshRuler()
	local length = CE.Length();
	local width = Width();
	local step = STEPS[#STEPS];

	for _, candidate in ipairs(STEPS) do
		if ( width / (length / candidate) >= 56 ) then
			step = candidate;
			break;
		end
	end

	local count = floor(length / step);

	for index = 0, count do
		local tick = ticks[index + 1];

		if ( not tick ) then
			tick = {};
			tick.line = NoggitUI_SolidTexture(ruler, "ARTWORK", NOGGIT_COLOR_HOVER);
			tick.line:SetWidth(1);
			tick.line:SetHeight(6);
			tick.text = NoggitUI_FontString(ruler, "");
			NoggitText_SetColor(tick.text, NOGGIT_COLOR_DISABLED);
			ticks[index + 1] = tick;
		end

		local x = TimeToX(index * step);
		tick.line:ClearAllPoints();
		tick.line:SetPoint("BOTTOMLEFT", ruler, "BOTTOMLEFT", x, 0);
		tick.text:ClearAllPoints();
		tick.text:SetPoint("BOTTOMLEFT", ruler, "BOTTOMLEFT", x + 2, 3);
		tick.text:SetText(step < 1000 and format("%.2f", index * step / 1000) or format("%d", index * step / 1000));
		tick.line:Show();
		tick.text:Show();
	end

	for index = count + 2, #ticks do
		ticks[index].line:Hide();
		ticks[index].text:Hide();
	end
end

-- Speed graph ---------------------------------------------------------------------------------------------

local function RefreshGraph()
	local speeds, maximum = CameraEditor_GetSpeedGraph(BAR_COUNT, CE.POSITION);
	maximum = tonumber(maximum) or 0;
	local width = Width() / BAR_COUNT;

	for index = 1, BAR_COUNT do
		local bar = bars[index];

		if ( not bar ) then
			bar = graph:CreateTexture(nil, "ARTWORK");
			bars[index] = bar;
		end

		local speed = speeds and speeds[index] or 0;
		local fraction = maximum > 0 and speed / maximum or 0;
		bar:ClearAllPoints();
		bar:SetPoint("BOTTOMLEFT", graph, "BOTTOMLEFT", (index - 1) * width, 0);
		bar:SetWidth(max(1, width - 0.5));
		bar:SetHeight(max(1, fraction * (GRAPH_HEIGHT - 4)));
		local r, g, b = CE.SpeedColor(fraction);
		bar:SetTexture(r, g, b, fraction > 0 and 0.85 or 0.15);
	end

	graphMax:SetText(maximum > 0 and format("top %.1f yd/s", maximum) or "no movement");
end

-- Key markers ---------------------------------------------------------------------------------------------

local function MarkerColor(marker)
	local state = CE.state;
	local color = TRACK_COLORS[marker.track];

	if ( drag and drag.marker == marker ) then
		return 1, 1, 1;
	end

	if ( state.selTrack == marker.track and state.selIndex == marker.index ) then
		return 1, 0.9, 0.15;
	end

	return color[1], color[2], color[3];
end

local function PlaceMarker(marker, time)
	marker:ClearAllPoints();
	marker:SetPoint("CENTER", lanes[marker.track], "LEFT", TimeToX(time), 0);
end

local function CreateMarker(track)
	local marker = CreateFrame("Button", nil, lanes[track]);
	marker:SetWidth(MARKER_WIDTH);
	marker:SetHeight(LANE_HEIGHT - 4);
	marker:SetFrameLevel(lanes[track]:GetFrameLevel() + 2);
	marker.texture = NoggitUI_SolidTexture(marker, "ARTWORK", NOGGIT_COLOR_TEXT);
	marker.texture:SetPoint("TOPLEFT", marker, "TOPLEFT", 1, 0);
	marker.texture:SetPoint("BOTTOMRIGHT", marker, "BOTTOMRIGHT", -1, 0);
	marker.track = track;

	marker:SetScript("OnMouseDown", function(self, button)
		if ( button ~= "LeftButton" ) then
			return;
		end

		CameraEditor_Select(self.track, self.index, CE.PART_VALUE);
		drag = { marker = self, startX = NoggitUI_CursorIn(area), time = self.time, moved = false };
	end);
	marker:SetScript("OnEnter", function(self)
		NoggitTooltip_Show(self, format("%s key %d at %s s, drag to move it in time (ctrl for single milliseconds)", CE.TRACK_NAMES[self.track], self.index, CE.Seconds(self.time)));
	end);
	marker:SetScript("OnLeave", NoggitTooltip_Hide);

	return marker;
end

local function RefreshMarkers()
	for track = 1, 3 do
		local keys = CE.state.keys[track] or {};
		local pool = markers[track];

		for index, key in ipairs(keys) do
			local marker = pool[index];

			if ( not marker ) then
				marker = CreateMarker(track);
				pool[index] = marker;
			end

			marker.index = index;
			marker.time = key.time;

			if ( not (drag and drag.marker == marker) ) then
				PlaceMarker(marker, key.time);
			end

			marker.texture:SetTexture(MarkerColor(marker));
			marker:Show();
		end

		for index = #keys + 1, #pool do
			pool[index]:Hide();
		end
	end
end

local function FinishDrag()
	local current = drag;
	drag = nil;

	if ( current and current.moved and current.marker:IsShown() ) then
		CameraEditor_SetKeyTime(current.marker.track, current.marker.index, current.newTime);
	end

	RefreshMarkers();
end

-- Refresh -------------------------------------------------------------------------------------------------

local function Refresh()
	if ( not frame or not frame:IsShown() ) then
		return;
	end

	RefreshRuler();
	RefreshGraph();
	RefreshMarkers();
end

local function UpdateTransport()
	local time, playing, preview, engine, loop, rate = CE.GetTime();
	local length = CE.Length();

	playhead:ClearAllPoints();
	playhead:SetPoint("TOP", ruler, "TOPLEFT", TimeToX(min(time, length)), 0);
	timeText:SetText(format("%s / %s s", CE.Seconds(time), CE.Seconds(length)));
	playButton:SetText((playing or engine) and "Pause" or "Play");
	engineButton:SetText(engine and "Stop engine" or "Play in engine");
	NoggitCheckBox_SetChecked(previewBox, preview);
	NoggitCheckBox_SetChecked(loopBox, loop);
end

function CE.TimelineUpdate()
	if ( not frame or not frame:IsShown() ) then
		return;
	end

	if ( scrubbing ) then
		if ( EditorIsMouseButtonDown() ) then
			CameraEditor_SetTime(CursorTime());
		else
			scrubbing = false;
		end
	end

	if ( drag ) then
		if ( EditorIsMouseButtonDown() ) then
			local x = NoggitUI_CursorIn(area);

			if ( drag.moved or math.abs(x - drag.startX) > 2 ) then
				drag.moved = true;
				drag.newTime = Snap(CursorTime());
				PlaceMarker(drag.marker, drag.newTime);
				NoggitTooltip_Show(drag.marker, CE.Seconds(drag.newTime).." s");
			end
		else
			NoggitTooltip_Hide();
			FinishDrag();
		end
	end

	UpdateTransport();
end

-- Building ------------------------------------------------------------------------------------------------

local function StartScrub()
	CameraEditor_Play(false);
	scrubbing = true;
	CameraEditor_SetTime(CursorTime());
end

local function LaneMouseDown(lane, button)
	if ( button ~= "LeftButton" ) then
		return;
	end

	local now = EditorGetTime();

	-- Double click adds a key there, a single click scrubs.
	if ( lastLaneClick.lane == lane and now - (lastLaneClick.time or 0) < DOUBLE_CLICK ) then
		lastLaneClick = {};
		CameraEditor_AddKey(lane.track, Snap(CursorTime()));
		return;
	end

	lastLaneClick = { lane = lane, time = now };
	StartScrub();
end

local function Strip(parent, height, anchor)
	local strip = CreateFrame("Frame", nil, parent);
	strip:SetHeight(height);
	strip:SetPoint("TOPLEFT", anchor, "BOTTOMLEFT", 0, -2);
	strip:SetPoint("TOPRIGHT", anchor, "BOTTOMRIGHT", 0, -2);
	strip:EnableMouse(true);
	return strip;
end

local function SideLabel(strip, text, color)
	local label = Common.Label(frame, text, nil, color);
	label:SetPoint("RIGHT", strip, "LEFT", -8, 0);
	return label;
end

function CE.BuildTimeline(parent)
	frame = CreateFrame("Frame", "GameEditorCameraTimeline", parent, "NoggitBoxTemplate");
	frame:SetHeight(HEIGHT);
	frame:SetPoint("BOTTOMLEFT", parent, "BOTTOMLEFT", 0, 0);
	frame:SetPoint("BOTTOMRIGHT", parent, "BOTTOMRIGHT", 0, 0);
	frame:EnableMouse(true);
	NoggitBox_Set(frame, { 0.122, 0.125, 0.137, 0.94 }, NOGGIT_COLOR_MID);

	-- Transport.
	local bar = CreateFrame("Frame", nil, frame);
	bar:SetHeight(BAR_HEIGHT);
	bar:SetPoint("TOPLEFT", frame, "TOPLEFT", 6, -4);
	bar:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -6, -4);

	local rewind = Common.Button(bar, "|<", 30, CE.Rewind, "Back to the start (Home)");
	rewind:SetPoint("LEFT", bar, "LEFT", 0, 0);
	local previous = Common.Button(bar, "<", 26, function() CE.StepKey(-1); end, "Previous key of the shown track ([)");
	previous:SetPoint("LEFT", rewind, "RIGHT", 3, 0);
	playButton = Common.Button(bar, "Play", 64, function()
		local _, _, _, engine = CE.GetTime();

		if ( engine ) then
			CameraEditor_StopEngine();
		else
			CE.TogglePlay();
		end
	end, "Play or pause the preview (P)");
	playButton:SetPoint("LEFT", previous, "RIGHT", 3, 0);
	local nextKey = Common.Button(bar, ">", 26, function() CE.StepKey(1); end, "Next key of the shown track (])");
	nextKey:SetPoint("LEFT", playButton, "RIGHT", 3, 0);

	previewBox = NoggitCheckBox_Create(bar, "Look through (V)", false, function(_, checked) CameraEditor_SetPreview(checked); end);
	previewBox:SetPoint("LEFT", nextKey, "RIGHT", 12, 0);
	loopBox = NoggitCheckBox_Create(bar, "Loop", false, function(_, checked) CameraEditor_SetLoop(checked); end);
	loopBox:SetPoint("LEFT", previewBox, "RIGHT", 8, 0);

	local rateItems = {};

	for _, rate in ipairs(RATES) do
		table.insert(rateItems, { text = format("%gx speed", rate), rate = rate });
	end

	rateCombo = NoggitComboBox_Create(bar, 96, rateItems, 3, function(_, _, item) CameraEditor_SetRate(item.rate); end);
	rateCombo:SetPoint("LEFT", loopBox, "RIGHT", 8, 0);

	engineButton = Common.Button(bar, "Play in engine", 104, CE.ToggleEngine, "Build the M2 in memory and play it through the client's own cinematic camera, exactly as the game will");
	engineButton:SetPoint("LEFT", rateCombo, "RIGHT", 8, 0);

	local capture = Common.Button(bar, "Capture view (K)", 116, CE.Capture, "Key the position and target at the playhead from the current view, fly the free camera to frame the shot first");
	capture:SetPoint("LEFT", engineButton, "RIGHT", 8, 0);

	timeText = Common.Label(bar, "", "NoggitFontBold");
	timeText:SetPoint("RIGHT", bar, "RIGHT", -4, 0);

	-- The time area, everything below shares its horizontal scale.
	area = CreateFrame("Frame", nil, frame);
	area:SetPoint("TOPLEFT", bar, "BOTTOMLEFT", LABEL_WIDTH, -4);
	area:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -14, 6);

	ruler = CreateFrame("Frame", nil, area);
	ruler:SetHeight(RULER_HEIGHT);
	ruler:SetPoint("TOPLEFT", area, "TOPLEFT", 0, 0);
	ruler:SetPoint("TOPRIGHT", area, "TOPRIGHT", 0, 0);
	ruler:EnableMouse(true);
	ruler:SetScript("OnMouseDown", StartScrub);
	SideLabel(ruler, "Seconds", NOGGIT_COLOR_DISABLED);

	graph = Strip(area, GRAPH_HEIGHT, ruler);
	NoggitUI_SolidTexture(graph, "BACKGROUND", NOGGIT_COLOR_BASE):SetAllPoints(graph);
	graph:SetScript("OnMouseDown", StartScrub);
	graph:SetScript("OnEnter", function(self)
		NoggitTooltip_Show(self, "Camera speed over time. A jump between bars at a key is the stutter you see in game, smooth or matched handles remove it.");
	end);
	graph:SetScript("OnLeave", NoggitTooltip_Hide);
	SideLabel(graph, "Speed", NOGGIT_COLOR_DISABLED);
	graphMax = Common.Label(graph, "", nil, NOGGIT_COLOR_DISABLED);
	graphMax:SetPoint("TOPRIGHT", graph, "TOPRIGHT", -4, -2);

	local anchor = graph;

	for track = 1, 3 do
		local lane = Strip(area, LANE_HEIGHT, anchor);
		lane.track = track;
		NoggitUI_SolidTexture(lane, "BACKGROUND", NOGGIT_COLOR_BASE):SetAllPoints(lane);
		lane:SetScript("OnMouseDown", LaneMouseDown);
		local color = TRACK_COLORS[track];
		SideLabel(lane, CE.TRACK_NAMES[track], { color[1], color[2], color[3], 1 });
		lanes[track] = lane;
		anchor = lane;
	end

	-- Drawn above the strips, it never takes the mouse.
	local overlay = CreateFrame("Frame", nil, area);
	overlay:SetAllPoints(area);
	overlay:SetFrameLevel(area:GetFrameLevel() + 10);
	playhead = NoggitUI_SolidTexture(overlay, "OVERLAY", { 1, 0.25, 0.25, 0.95 });
	playhead:SetWidth(2);
	playhead:SetHeight(RULER_HEIGHT + GRAPH_HEIGHT + LANE_HEIGHT * 3 + 8);

	frame:SetScript("OnSizeChanged", Refresh);
	frame:SetScript("OnShow", Refresh);
	CE.Listen(Refresh);

	return frame;
end
