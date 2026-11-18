-- Shared state and actions of the cinematic camera workspace, the document itself lives in the client.

GameEditorCamera = GameEditorCamera or {};
local CE = GameEditorCamera;

CE.POSITION, CE.TARGET, CE.ROLL = 1, 2, 3;
CE.PART_VALUE, CE.PART_IN, CE.PART_OUT = 0, 1, 2;
CE.TRACK_NAMES = { "Position", "Target", "Roll" };
CE.PI = 3.14159265358979;

-- Combo index is the client value plus one.
CE.INTERP_ITEMS = {
	{ text = "None (jumps between keys)" },
	{ text = "Linear" },
	{ text = "Bezier" },
	{ text = "Hermite" },
};

CE.MODE_ITEMS = {
	{ text = "Auto smooth" },
	{ text = "Smooth, matched speed" },
	{ text = "Aligned" },
	{ text = "Free" },
	{ text = "Stop, ease in and out" },
};

CE.MODE_SHORT = { [0] = "auto", "smooth", "aligned", "free", "stop" };

CE.state = { revision = -1, doc = {}, keys = { {}, {}, {} }, track = CE.POSITION };
CE.settings = { keepWorld = false, scaleKeys = true, targetDistance = 20, advance = 2 };

local listeners = {};

function CE.Listen(callback)
	table.insert(listeners, callback);
end

function CE.Deg(radians)
	return (tonumber(radians) or 0) * 180 / CE.PI;
end

function CE.Rad(degrees)
	return (tonumber(degrees) or 0) * CE.PI / 180;
end

function CE.Seconds(ms)
	return format("%.2f", (tonumber(ms) or 0) / 1000);
end

function CE.Length()
	return max(1, tonumber(CE.state.doc.length) or 1);
end

function CE.IsSpline(track)
	local interp = CE.state.doc.interp and CE.state.doc.interp[track] or 0;
	return interp == 2 or interp == 3;
end

local function Pull()
	local state = CE.state;
	state.doc = CameraEditor_GetDoc() or {};

	for track = 1, 3 do
		state.keys[track] = CameraEditor_GetKeys(track) or {};
	end

	local track, index, part = CameraEditor_GetSelection();
	local changed = track ~= state.selTrack or index ~= state.selIndex;
	state.selTrack, state.selIndex, state.selPart = track, index, part;

	-- A key picked in the world shows its own track.
	if ( track and changed ) then
		state.track = track;
	end
end

function CE.Poll()
	if ( not CameraEditor_GetRevision ) then
		return;
	end

	local revision = CameraEditor_GetRevision();

	if ( not revision or revision == CE.state.revision ) then
		return;
	end

	CE.state.revision = revision;
	Pull();

	for _, callback in ipairs(listeners) do
		callback();
	end
end

function CE.Refresh()
	CE.state.revision = -1;
	CE.Poll();
end

function CE.SelectedKey()
	local state = CE.state;

	if ( state.selTrack and state.selIndex ) then
		return state.keys[state.selTrack][state.selIndex], state.selTrack, state.selIndex;
	end
end

function CE.Result(ok, error, okText)
	if ( ok ) then
		if ( okText ) then
			GameEditor_Notify(okText, "ok");
		end
	else
		GameEditor_Notify(error or "That did not work", "error");
	end

	return ok;
end

function CE.GetTime()
	local time, playing, preview, engine, loop, rate = CameraEditor_GetTime();
	return tonumber(time) or 0, playing, preview, engine, loop, tonumber(rate) or 1;
end

-- Actions shared by the panel, the timeline, the menus and the hotkeys.

-- Append keys one step after the last key, otherwise on the playhead, which then moves on a step.
local function Capture(append)
	local ok, result, added = CameraEditor_CaptureView(CE.settings.targetDistance, append, CE.settings.advance * 1000);

	if ( not ok ) then
		CE.Result(false, result);
		return;
	end

	GameEditor_Notify(format("%s the key at %s s", added and "Added" or "Updated", CE.Seconds(result)), added and "ok" or "info");
end

-- Handlers get no arguments through, buttons pass themselves.
function CE.Capture()
	Capture(false);
end

function CE.Append()
	Capture(true);
end

function CE.TogglePlay()
	local _, playing = CE.GetTime();
	CameraEditor_Play(not playing);
end

function CE.TogglePreview()
	local _, _, preview = CE.GetTime();
	CameraEditor_SetPreview(not preview);
end

function CE.ToggleEngine()
	local _, _, _, engine = CE.GetTime();

	if ( engine ) then
		CameraEditor_StopEngine();
	else
		CE.Result(CameraEditor_PlayEngine());
	end
end

function CE.ToggleLoop()
	local _, _, _, _, loop = CE.GetTime();
	CameraEditor_SetLoop(not loop);
end

function CE.Rewind()
	CameraEditor_Play(false);
	CameraEditor_SetTime(0);
end

function CE.AddKey(track, time)
	local _, added = CameraEditor_AddKey(track or CE.state.track, time);

	if ( not added ) then
		GameEditor_Notify("There is already a key there, it is selected. Move the playhead to add another.", "info");
	end
end

function CE.DeleteSelected()
	local key, track, index = CE.SelectedKey();

	if ( key ) then
		CameraEditor_DeleteKey(track, index);
	end
end

-- Moves the playhead to the previous or next key of the shown track.
function CE.StepKey(direction)
	local time = CE.GetTime();
	local keys = CE.state.keys[CE.state.track] or {};
	local best;

	for index, key in ipairs(keys) do
		local delta = (key.time - time) * direction;

		if ( delta > 0.5 and (not best or delta < (keys[best].time - time) * direction) ) then
			best = index;
		end
	end

	if ( best ) then
		CameraEditor_Play(false);
		CameraEditor_SetTime(keys[best].time);
		CameraEditor_Select(CE.state.track, best, CE.PART_VALUE);
	end
end

function CE.Save(path)
	local ok, result = CameraEditor_Save(path);

	if ( not ok ) then
		CE.Result(false, result);
		return;
	end

	CE.Refresh();
	GameEditor_Notify("Saved "..tostring(result), "ok");

	local warnings = CE.state.doc.warnings;

	if ( warnings and warnings ~= "" ) then
		GameEditor_Notify(string.gsub(warnings, "\n$", ""), "info");
	end
end

-- Runs action now or after asking, when there are unsaved changes.
function CE.ConfirmDiscard(action)
	if ( not CE.state.doc.dirty ) then
		action();
		return;
	end

	NoggitMessageBox_Show("Unsaved camera", "The camera has unsaved changes. Throw them away?", {
		{ text = "Discard", onClick = action },
		{ text = "Cancel" },
	});
end

function CE.New()
	CE.ConfirmDiscard(function()
		CameraEditor_New();
		GameEditor_Notify("New camera at your character, fly the view and press K to add keys", "info");
	end);
end

function CE.Open(path)
	CE.ConfirmDiscard(function()
		CE.Result(CameraEditor_Load(path), nil, "Opened "..tostring(path));
	end);
end

function CE.OpenDbc(id)
	CE.ConfirmDiscard(function()
		CE.Result(CameraEditor_LoadDbc(id), nil, "Opened CinematicCamera "..tostring(id));
	end);
end

-- The speed colours of the world path, blue slow to red fast.
function CE.SpeedColor(fraction)
	fraction = max(0, min(1, fraction or 0));

	if ( fraction < 0.5 ) then
		local t = fraction * 2;
		return 40 / 255 * (1 - t), (120 + 135 * t) / 255, 1 - t;
	end

	local t = (fraction - 0.5) * 2;
	return t, 1 - t, 0;
end
