-- In-menu feedback for editor buttons. A click that sends a save shows Saving and then Saved or Failed on the
-- button, and the server reply goes to the feedback line of the window the button sits in, or the status bar.

local HOLD_TIME = 3;
local REPLY_TIMEOUT = 20;

NOGGIT_COLOR_OK = { 0.3, 0.75, 0.35, 1 };
NOGGIT_COLOR_ERROR = { 0.85, 0.25, 0.25, 1 };

local TINT = {
	busy = { NOGGIT_COLOR_DARK, NOGGIT_COLOR_ACCENT, NOGGIT_COLOR_TEXT },
	ok = { { 0.16, 0.27, 0.18, 1 }, NOGGIT_COLOR_OK, { 0.6, 0.95, 0.6, 1 } },
	error = { { 0.32, 0.14, 0.14, 1 }, NOGGIT_COLOR_ERROR, { 1, 0.6, 0.6, 1 } },
	info = { NOGGIT_COLOR_MID, NOGGIT_COLOR_ACCENT, NOGGIT_COLOR_TEXT },
};

local CREATURE_RESULT = "HOT_CREATURE_EDITOR_RESULT";
local GOB_RESULT = "HOT_GOB_EDITOR_RESULT";

-- Client calls that send a request the server answers, wrapped so the reply can be tied back to the click.
local REQUESTS = {
	WorldTable_Save = { events = { WORLD_TABLE_RESULT = true }, keyed = true, label = "Saving" },
	DbcEditor_Save = { events = { DBC_EDITOR_RESULT = true }, keyed = true, label = "Saving" },
	CreatureEditor_SetFields = { events = { [CREATURE_RESULT] = true }, label = "Saving" },
	CreatureEditor_SetAddon = { events = { [CREATURE_RESULT] = true }, label = "Saving" },
	CreatureEditor_Equipment = { events = { [CREATURE_RESULT] = true }, label = "Saving" },
	CreatureEditor_Waypoint = { events = { [CREATURE_RESULT] = true }, label = "Sending" },
	CreatureEditor_Move = { events = { [CREATURE_RESULT] = true }, label = "Moving" },
	GobEditor_SetFields = { events = { [GOB_RESULT] = true }, label = "Saving" },
	GobEditor_Commit = { events = { [GOB_RESULT] = true }, label = "Saving" },
	GameEditor_Spawn = { byKind = true, label = "Spawning" },
	GameEditor_DeleteSpawn = { byKind = true, label = "Deleting" },
};

local clicks = {};
local pending = {};
local shown = {};

NoggitFeedback_DefaultLine = nil;

function NoggitFeedback_Colors(button)
	local tint = button.feedbackKind and TINT[button.feedbackKind];

	if ( tint ) then
		return tint[1], tint[2];
	end
end

local function SetLabel(button, label, color)
	local text = button.GetFontString and button:GetFontString();

	if ( not text ) then
		return;
	end

	local overlay = button.feedbackLabel;

	if ( not label ) then
		if ( overlay ) then
			overlay:Hide();
		end

		text:SetAlpha(1);
		return;
	end

	if ( not overlay ) then
		overlay = button:CreateFontString(nil, "OVERLAY", "NoggitFont");
		overlay:SetPoint("CENTER", button, "CENTER", 0, 0);
		button.feedbackLabel = overlay;
	end

	overlay:SetText(label);
	NoggitText_SetColor(overlay, color);

	if ( overlay:GetStringWidth() > (button:GetWidth() or 0) - 4 ) then
		overlay:Hide();
		text:SetAlpha(1);
	else
		overlay:Show();
		text:SetAlpha(0);
	end
end

local function ShowOnButton(button, kind, label)
	if ( not button or not button.feedbackRefresh or not button:IsVisible() ) then
		return;
	end

	button.feedbackKind = kind;
	button.feedbackUntil = kind ~= "busy" and (GetTime() + HOLD_TIME) or nil;
	SetLabel(button, label, TINT[kind][3]);
	button.feedbackRefresh(button);
	shown[button] = true;
end

local function ClearButton(button)
	button.feedbackKind = nil;
	button.feedbackUntil = nil;
	SetLabel(button, nil);
	button.feedbackRefresh(button);
	shown[button] = nil;
end

-- Lines -----------------------------------------------------------------------------------------------------

function NoggitFeedback_SetLine(line, text, kind)
	if ( not line or not text or text == "" ) then
		return;
	end

	line:SetText(date("%H:%M:%S").."  "..text);

	if ( kind == "error" ) then
		NoggitText_SetColor(line, TINT.error[3]);
	elseif ( kind == "ok" ) then
		NoggitText_SetColor(line, TINT.ok[3]);
	else
		NoggitText_SetColor(line, NOGGIT_COLOR_TEXT);
	end
end

local function LineFor(frame)
	while ( frame ) do
		if ( frame.feedbackLine ) then
			return frame.feedbackLine;
		end

		frame = frame:GetParent();
	end

	return NoggitFeedback_DefaultLine;
end

local function VisibleLine(line)
	if ( line and line:IsVisible() ) then
		return line;
	end

	return NoggitFeedback_DefaultLine;
end

-- Requests --------------------------------------------------------------------------------------------------

local function SuccessLabel(message)
	message = string.lower(message or "");

	if ( message:find("nothing was saved", 1, true) or message:find("preview", 1, true) ) then
		return "Previewing";
	elseif ( message:find("delet", 1, true) ) then
		return "Deleted";
	elseif ( message:find("spawned", 1, true) ) then
		return "Spawned";
	elseif ( message:find("saved", 1, true) ) then
		return "Saved";
	elseif ( message:find("nothing changed", 1, true) ) then
		return "No change";
	end

	return "Done";
end

local function Finish(click)
	local failed, waiting, last;

	for _, entry in ipairs(click.requests) do
		if ( entry.timedOut ) then
			waiting = waiting or entry;
		elseif ( not entry.ok ) then
			failed = failed or entry;
		else
			last = entry;
		end
	end

	local button = click.source;

	if ( failed ) then
		ShowOnButton(button, "error", "Failed");
		NoggitFeedback_SetLine(VisibleLine(failed.line), failed.message, "error");
	elseif ( waiting ) then
		ShowOnButton(button, "info", "No reply");
	elseif ( last ) then
		ShowOnButton(button, "ok", SuccessLabel(last.message));
	end
end

local function RemovePending(entry)
	for index, candidate in ipairs(pending) do
		if ( candidate == entry ) then
			table.remove(pending, index);
			return;
		end
	end
end

local function Resolve(entry, ok, message, kind)
	if ( entry.done ) then
		return;
	end

	entry.done = true;
	entry.ok = ok;

	if ( not message or message == "" ) then
		message = ok and "Saved" or "The server refused the change";
	end

	entry.message = message;
	RemovePending(entry);
	NoggitFeedback_SetLine(VisibleLine(entry.line), message, kind or (ok and "ok" or "error"));

	local click = entry.click;

	if ( click ) then
		click.open = click.open - 1;

		if ( click.open <= 0 and click.ended ) then
			Finish(click);
		end
	end
end

local function Track(spec, ...)
	local click = clicks[#clicks];
	local entry = { time = GetTime(), click = click, events = spec.events };

	if ( spec.byKind ) then
		local kind = ...;

		if ( kind == "gameobject" ) then
			entry.events = { [GOB_RESULT] = true };
		elseif ( kind == "creature" ) then
			entry.events = { [CREATURE_RESULT] = true };
		else
			entry.events = { [GOB_RESULT] = true, [CREATURE_RESULT] = true };
		end
	elseif ( spec.keyed ) then
		local name, key = ...;
		entry.table = name and string.lower(tostring(name)) or nil;
		entry.key = tonumber(key);
	end

	entry.line = LineFor(click and click.source);
	table.insert(pending, entry);

	if ( click ) then
		click.open = click.open + 1;
		click.busyLabel = click.busyLabel or spec.label;
		table.insert(click.requests, entry);
	end

	return entry;
end

-- A call that errored or returned false sent nothing, so its entry goes away unless a reply already came.
local function Drop(entry)
	if ( entry.done ) then
		return;
	end

	entry.done = true;
	RemovePending(entry);

	local click = entry.click;

	if ( click ) then
		click.open = click.open - 1;

		for index, candidate in ipairs(click.requests) do
			if ( candidate == entry ) then
				table.remove(click.requests, index);
				break;
			end
		end
	end
end

local function Pass(entry, ok, ...)
	if ( not ok ) then
		Drop(entry);
		error((...), 0);
	end

	if ( not (...) ) then
		Drop(entry);
	end

	return ...;
end

local function Wrap(name, spec)
	local raw = _G[name];

	if ( type(raw) ~= "function" ) then
		return;
	end

	_G[name] = function(...)
		return Pass(Track(spec, ...), pcall(raw, ...));
	end;
end

for name, spec in pairs(REQUESTS) do
	Wrap(name, spec);
end

-- Results and notifications ----------------------------------------------------------------------------------

function NoggitFeedback_OnResult(event, ...)
	local ok, name, key, message;

	if ( event == "WORLD_TABLE_RESULT" or event == "DBC_EDITOR_RESULT" ) then
		ok, name, key, message = ...;
		name = name and string.lower(tostring(name)) or nil;
		key = tonumber(key);
	else
		ok, message = ...;
	end

	ok = ok == 1 or ok == true;

	local match;

	for _, entry in ipairs(pending) do
		if ( entry.events[event] and (not entry.table or not name or entry.table == name) and (not entry.key or not key or entry.key == key) ) then
			match = entry;
			break;
		end
	end

	if ( not match and name ) then
		for _, entry in ipairs(pending) do
			if ( entry.events[event] and (not entry.key or not key or entry.key == key) ) then
				match = entry;
				break;
			end
		end
	end

	if ( match ) then
		Resolve(match, ok, message);
	else
		NoggitFeedback_SetLine(VisibleLine(nil), message, ok and "ok" or "error");
	end
end

function NoggitFeedback_OnNotify(text, kind)
	local click = clicks[#clicks];

	if ( click and (not click.notified or kind == "error") ) then
		click.notified = { text = text, kind = kind or "info" };
	end
end

-- Clicks ----------------------------------------------------------------------------------------------------

function NoggitFeedback_Begin(source)
	table.insert(clicks, { source = source, requests = {}, open = 0 });
end

function NoggitFeedback_End()
	local click = table.remove(clicks);

	if ( not click ) then
		return;
	end

	click.ended = true;
	local button = click.source;
	local line = VisibleLine(LineFor(button));

	if ( click.open > 0 ) then
		ShowOnButton(button, "busy", click.busyLabel or "Saving");
		NoggitFeedback_SetLine(line, click.notified and click.notified.text or "Sent, waiting for the server", "info");
	elseif ( #click.requests > 0 ) then
		Finish(click);
	elseif ( click.notified ) then
		ShowOnButton(button, click.notified.kind);
		NoggitFeedback_SetLine(line, click.notified.text, click.notified.kind);
	end
end

local function PreClick(self)
	NoggitFeedback_Begin(self);
end

local function PostClick(self)
	NoggitFeedback_End();
end

function NoggitFeedback_Attach(button, refresh)
	if ( button.feedbackRefresh ) then
		return;
	end

	button.feedbackRefresh = refresh or NoggitButton_Refresh;

	for script, handler in pairs({ PreClick = PreClick, PostClick = PostClick }) do
		if ( button:GetScript(script) ) then
			button:HookScript(script, handler);
		else
			button:SetScript(script, handler);
		end
	end
end

local driver = CreateFrame("Frame");

driver:SetScript("OnUpdate", function()
	while ( #clicks > 0 ) do
		NoggitFeedback_End();
	end

	local now = GetTime();

	for index = #pending, 1, -1 do
		local entry = pending[index];

		if ( entry and now - entry.time > REPLY_TIMEOUT ) then
			entry.timedOut = true;
			Resolve(entry, false, "Sent, but the server has not answered yet", "info");
		end
	end

	for button in pairs(shown) do
		if ( not button:IsVisible() or (button.feedbackUntil and now >= button.feedbackUntil) ) then
			ClearButton(button);
		end
	end
end);
