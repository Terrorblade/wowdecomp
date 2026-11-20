-- Waypoint tool panel, edits the waypoint_data path of the selected creature.

GameEditorCreature = GameEditorCreature or {};
local Creature = GameEditorCreature;
local Common = GameEditorObject;

local Waypoints = {};
Creature.Waypoints = Waypoints;

local OP_CREATE, OP_ADD, OP_MOVE, OP_DELETE, OP_SET, OP_ASSIGN, OP_UNASSIGN, OP_PREVIEW, OP_STOP = 0, 1, 2, 3, 4, 5, 6, 7, 8;

local MOVE_TYPES = { { text = "Walk" }, { text = "Run" }, { text = "Land" }, { text = "Takeoff" } };

Waypoints.HELP = table.concat({
	"- Click a node in the world or in the list to select it.",
	"- Drag a node to move it, it is saved when you let go.",
	"- Shift+click the ground adds a node after the selected one (or after the last one). With no path it starts a new path.",
	"- Insert adds a node under the mouse with the new node settings below.",
	"- Delete removes the selected node, Escape clears the node selection.",
	"- A path only drives the creature once it is assigned and the creature uses waypoint movement. Assign does both.",
}, "\n");

local frame, body, empty, list, editor;
local state = {
	viewPath = nil,
	expectCreate = false,
	rerequested = false,
	pendingSelect = nil,
	spawnKey = nil,
	selected = 0,
	showAlways = false,
};

local function MoveTypeName(value)
	local item = MOVE_TYPES[(tonumber(value) or 0) + 1];
	return item and item.text or tostring(value);
end

local function Data()
	return Creature.GetSelectionData();
end

local function FindNode(data, point)
	for index, node in ipairs(data and data.nodes or {}) do
		if ( node.point == point ) then
			return node, index;
		end
	end
end

local function LastPoint(data)
	local nodes = data and data.nodes or {};
	local last = nodes[#nodes];
	return last and last.point or 0;
end

function Waypoints.GetViewPath()
	return state.viewPath;
end

function Waypoints.GetSelectedPoint()
	return state.selected;
end

function Waypoints.IsShowingAlways()
	return state.showAlways;
end

local function PushDrawing()
	CreatureEditor_SetPathDrawing(state.showAlways, state.selected or 0);
end

function Waypoints.SetShowAlways(show)
	state.showAlways = show and true or false;
	PushDrawing();

	if ( frame and frame.showAlways ) then
		NoggitCheckBox_SetChecked(frame.showAlways, state.showAlways);
	end
end

local function Send(op, args)
	local data = Data();

	if ( not data ) then
		GameEditor_Notify("Select a creature first", "info");
		return false;
	end

	args = args or {};

	if ( args.pathId == nil ) then
		args.pathId = data.pathId;
	end

	return CreatureEditor_Waypoint(op, args);
end

local function LoadEditor()
	if ( not editor ) then
		return;
	end

	local node = FindNode(Data(), state.selected);
	editor.node = node;

	if ( not node ) then
		editor.title:SetText("No node selected");
		NoggitText_SetColor(editor.title, NOGGIT_COLOR_DISABLED);
	else
		editor.title:SetText(format("Node %d", node.point));
		NoggitText_SetColor(editor.title, NOGGIT_COLOR_WHITE);
		NoggitSpinBox_SetValue(editor.delay, tonumber(node.delay) or 0, true);
		NoggitComboBox_SetSelected(editor.move, (tonumber(node.moveType) or 0) + 1, true);
		NoggitCheckBox_SetChecked(editor.face, node.o ~= nil);
		NoggitSpinBox_SetValue(editor.facing, node.o and (math.deg(node.o) % 360) or 0, true);
	end

	local enabled = node ~= nil;
	editor.delay:SetEnabledState(enabled);
	editor.facing:SetEnabledState(enabled);

	for _, widget in ipairs(editor.nodeButtons) do
		Common.SetEnabled(widget, enabled);
	end
end

local function ScrollTo(index)
	if ( not index or list.visibleCount == 0 ) then
		return;
	end

	if ( index <= list.offset or index > list.offset + list.visibleCount ) then
		list.offset = max(0, index - floor(list.visibleCount / 2));
	end
end

-- Selects a node in the list and in the world, point 0 clears.
function Waypoints.SelectPoint(point, fromWorld)
	state.selected = tonumber(point) or 0;

	if ( not fromWorld ) then
		PushDrawing();
	end

	if ( not list ) then
		return;
	end

	local _, index = FindNode(Data(), state.selected);
	list.selected = index;
	ScrollTo(index);
	NoggitListBox_Refresh(list);
	LoadEditor();
end

local function AddNode(x, y, z)
	local data = Data();

	if ( not data ) then
		GameEditor_Notify("Select a creature first", "error");
		return;
	end

	if ( not x ) then
		return;
	end

	local args = { x = x, y = y, z = z, delay = frame and frame.newDelay.value or 0, moveType = frame and (frame.newMove.selected - 1) or 0 };

	if ( (data.pathId or 0) == 0 ) then
		state.expectCreate = true;
		state.pendingSelect = 1;
		Send(OP_CREATE, args);
		return;
	end

	local after = state.selected > 0 and FindNode(data, state.selected) and state.selected or LastPoint(data);
	args.afterPoint = after;
	state.pendingSelect = after + 1;
	Send(OP_ADD, args);
end

function Waypoints.AddAtCursor()
	local x, y, z = GameEditor_GetCursorWorldPosition();

	if ( not x ) then
		GameEditor_Notify("Point the mouse at the ground to add a node there", "info");
		return;
	end

	AddNode(x, y, z);
end

function Waypoints.AddAtCharacter()
	local x, y, z = CreatureEditor_GetPlayerPosition();

	if ( not x ) then
		GameEditor_Notify("Your character position is not available", "error");
		return;
	end

	AddNode(x, y, z);
end

function Waypoints.DeleteSelected()
	local data = Data();
	local node = FindNode(data, state.selected);

	if ( not node ) then
		return false;
	end

	Send(OP_DELETE, { point = node.point });
	Waypoints.SelectPoint(node.point - 1);
	return true;
end

local function ApplyNode()
	local node = editor.node;

	if ( not node ) then
		GameEditor_Notify("Select a node first", "error");
		return;
	end

	local args = { point = node.point, delay = floor(editor.delay.value + 0.5), moveType = editor.move.selected - 1 };

	if ( editor.face.checked ) then
		args.o = math.rad(editor.facing.value % 360);
	end

	Send(OP_SET, args);
end

local function MoveNodeToCharacter()
	local node = editor.node;
	local x, y, z = CreatureEditor_GetPlayerPosition();

	if ( not node ) then
		GameEditor_Notify("Select a node first", "error");
	elseif ( not x ) then
		GameEditor_Notify("Your character position is not available", "error");
	else
		Send(OP_MOVE, { point = node.point, x = x, y = y, z = z });
	end
end

function Waypoints.LoadPath(pathId)
	local data = Data();
	pathId = tonumber(pathId);

	if ( not data ) then
		GameEditor_Notify("Select a creature first", "error");
		return;
	end

	if ( not pathId ) then
		GameEditor_Notify("Enter a path id first", "error");
		return;
	end

	if ( pathId == 0 or pathId == data.assignedPath ) then
		state.viewPath = nil;
	else
		state.viewPath = pathId;
	end

	state.rerequested = false;
	Waypoints.SelectPoint(0);
	CreatureEditor_Request(tostring(data.guid), pathId);
	GameEditor_Notify(state.viewPath and format("Loading path %d from the server", pathId) or "Loading the assigned path from the server", "info");
end

local function ScopeName(scope)
	return scope == 0 and "template addon" or "spawn addon";
end

local function Assign()
	local data = Data();

	if ( not data or (data.pathId or 0) == 0 ) then
		GameEditor_Notify("Create or load a path first", "info");
		return;
	end

	local scope = frame.scopeSpawn.checked and 1 or 0;

	if ( scope == 1 and (data.spawnId or 0) == 0 ) then
		GameEditor_Notify("This creature is not a database spawn, assign the path to the template", "info");
		return;
	end

	if ( scope == 0 ) then
		NoggitMessageBox_Show("Assign path", format("Assign path %d to entry %d?\n\nEvery spawn of this entry without its own spawn addon path walks it, and the template movement type becomes waypoint movement.", data.pathId, data.entry or 0), {
			{ text = "Assign", onClick = function() Send(OP_ASSIGN, { scope = 0 }); end },
			{ text = "Cancel" },
		});
	else
		Send(OP_ASSIGN, { scope = 1 });
	end
end

local function Unassign()
	local data = Data();

	if ( not data ) then
		GameEditor_Notify("Select a creature first", "error");
		return;
	end

	local scope = frame.scopeSpawn.checked and 1 or 0;
	Send(OP_UNASSIGN, { scope = scope });
end

local function AssignedText(data)
	local template = data.templateAddon and tonumber(data.templateAddon.path_id) or 0;
	local spawn = data.spawnAddon and tonumber(data.spawnAddon.path_id) or 0;
	local parts = {};

	if ( spawn > 0 ) then
		table.insert(parts, format("Spawn addon path %d", spawn));
	end

	if ( template > 0 ) then
		table.insert(parts, format("template addon path %d%s", template, spawn > 0 and " (the spawn addon wins)" or ""));
	end

	if ( #parts == 0 ) then
		table.insert(parts, "No path assigned");
	end

	local text = table.concat(parts, ", ")..".  Movement: "..Common.EnumText("MovementType", data.defaultMovement);

	if ( (data.assignedPath or 0) > 0 and data.defaultMovement ~= 2 ) then
		text = text.."  |cffffcc00The path is not walked until the movement type is waypoint movement.|r";
	end

	return text;
end

local function Refresh()
	if ( not frame ) then
		return;
	end

	local kind, _, _, name = GameEditor_GetSelection();
	local data = Data();

	if ( kind ~= "creature" ) then
		body:Hide();
		empty:Set({
			title = "No creature selected",
			text = kind == "gameobject" and "Gameobjects have no waypoints. Click a creature to edit its path." or "Click a creature in the world to show and edit its waypoint path.",
			primary = "Select my target", onPrimary = Common.SelectTarget,
			help = Waypoints.HELP,
		});
		return;
	end

	if ( not data ) then
		body:Hide();
		empty:Set({
			title = format("Loading %s...", name or "creature"),
			text = "Waiting for the creature snapshot from the server.",
			primary = "Ask again", onPrimary = function() Creature.Reload(); end,
			help = Waypoints.HELP,
		});
		return;
	end

	empty:Hide();
	body:Show();

	local pathId = data.pathId or 0;
	frame.creature:SetText(format("%s  (entry %d, spawn %s)", data.name or "", data.entry or 0, (data.spawnId or 0) > 0 and tostring(data.spawnId) or "none"));

	if ( not frame.pathEdit:HasFocus() ) then
		frame.pathEdit:SetText(pathId > 0 and tostring(pathId) or "");
	end

	if ( pathId == 0 ) then
		frame.pathInfo:SetText("This creature has no path. Shift+click the ground, press Insert or use New path to start one.");
	elseif ( pathId == data.assignedPath ) then
		frame.pathInfo:SetText(format("Path %d, %d nodes. This is the path the creature walks.", pathId, #(data.nodes or {})));
	else
		frame.pathInfo:SetText(format("Path %d, %d nodes. |cffffcc00Not assigned to this creature.|r", pathId, #(data.nodes or {})));
	end

	frame.assigned:SetText(AssignedText(data));
	Common.SetEnabled(frame.mine, state.viewPath ~= nil);

	local items = {};

	for _, node in ipairs(data.nodes or {}) do
		table.insert(items, node);
	end

	NoggitListBox_SetItems(list, items, true);

	if ( not FindNode(data, state.selected) ) then
		state.selected = 0;
	end

	Waypoints.SelectPoint(state.selected);

	local hasPath = pathId > 0;
	Common.SetEnabled(frame.assign, hasPath);
	Common.SetEnabled(frame.walk, hasPath and #items > 0);
	Common.SetEnabled(frame.scopeSpawn, (data.spawnId or 0) > 0);

	if ( (data.spawnId or 0) == 0 and frame.scopeSpawn.checked ) then
		NoggitCheckBox_OnClick(frame.scopeTemplate);
	end
end

Waypoints.Refresh = Refresh;

-- Called for every creature snapshot, keeps the viewed path and the pending selection.
function Waypoints.OnData()
	local data = Data();

	if ( not data ) then
		return;
	end

	local spawnKey = (data.spawnId or 0) > 0 and ("spawn"..data.spawnId) or ("guid"..tostring(data.guid));

	if ( state.spawnKey ~= spawnKey ) then
		state.spawnKey = spawnKey;
		state.viewPath = nil;
		state.expectCreate = false;
		state.rerequested = false;
		state.pendingSelect = nil;
		state.selected = 0;
	end

	if ( state.expectCreate and (data.pathId or 0) > 0 ) then
		state.expectCreate = false;
		state.viewPath = data.pathId ~= data.assignedPath and data.pathId or nil;
	end

	if ( state.viewPath and state.viewPath == data.assignedPath ) then
		state.viewPath = nil;
	end

	if ( state.viewPath and data.pathId ~= state.viewPath ) then
		if ( not state.rerequested ) then
			state.rerequested = true;
			CreatureEditor_Request(tostring(data.guid), state.viewPath);
		end
	else
		state.rerequested = false;
	end

	if ( state.pendingSelect and FindNode(data, state.pendingSelect) ) then
		state.selected = state.pendingSelect;
		state.pendingSelect = nil;
		PushDrawing();
	end

	Refresh();
end

function Waypoints.OnPointSelected(point)
	Waypoints.SelectPoint(point, true);
end

function Waypoints.StatusText()
	local data = Data();

	if ( not data ) then
		return nil;
	end

	if ( (data.pathId or 0) == 0 ) then
		return "No path";
	end

	if ( state.selected > 0 ) then
		return format("Path %d  node %d of %d", data.pathId, state.selected, #(data.nodes or {}));
	end

	return format("Path %d  %d nodes", data.pathId, #(data.nodes or {}));
end

local function Section(parent, text, anchor, y)
	local label = Common.Label(parent, text, "NoggitFontBold");
	label:SetPoint("TOPLEFT", anchor, "BOTTOMLEFT", 0, y or -10);
	return label;
end

local function BuildPathArea(parent)
	local top = CreateFrame("Frame", "GameEditorWaypointsTop", parent);
	top:SetHeight(118);
	top:SetPoint("TOPLEFT", parent, "TOPLEFT", 0, 0);
	top:SetPoint("TOPRIGHT", parent, "TOPRIGHT", 0, 0);

	local title = Common.Label(top, "Waypoints", "NoggitFont16", NOGGIT_COLOR_WHITE);
	title:SetPoint("TOPLEFT", top, "TOPLEFT", 2, -4);

	frame.creature = Common.Label(top, "", nil, NOGGIT_COLOR_DISABLED);
	frame.creature:SetPoint("TOPLEFT", top, "TOPLEFT", 2, -26);
	frame.creature:SetPoint("TOPRIGHT", top, "TOPRIGHT", -2, -26);

	local pathLabel = Common.Label(top, "Path");
	pathLabel:SetPoint("TOPLEFT", top, "TOPLEFT", 2, -50);

	frame.pathEdit = NoggitLineEdit_Create(top, 70);
	frame.pathEdit:SetHeight(22);
	frame.pathEdit:SetNumeric(true);
	frame.pathEdit:SetMaxLetters(9);
	frame.pathEdit:SetPoint("LEFT", pathLabel, "RIGHT", 8, 0);
	frame.pathEdit:SetScript("OnEnterPressed", function(self)
		self:ClearFocus();
		Waypoints.LoadPath(self:GetText());
	end);

	local load = Common.Button(top, "Load", 50, function()
		frame.pathEdit:ClearFocus();
		Waypoints.LoadPath(frame.pathEdit:GetText());
	end, "Show and edit the waypoint_data path with this id");
	load:SetPoint("LEFT", frame.pathEdit, "RIGHT", 4, 0);

	frame.mine = Common.Button(top, "Assigned", 70, function()
		Waypoints.LoadPath(0);
	end, "Go back to the path the creature walks");
	frame.mine:SetPoint("LEFT", load, "RIGHT", 4, 0);

	local create = Common.Button(top, "New path", 76, function()
		local _, _, _, _, x, y, z = GameEditor_GetSelection();

		if ( not Data() ) then
			GameEditor_Notify("Select a creature first", "error");
		elseif ( not x ) then
			GameEditor_Notify("The creature position is not available", "error");
		else
			state.expectCreate = true;
			state.pendingSelect = 1;
			Send(OP_CREATE, { x = x, y = y, z = z, delay = frame.newDelay.value, moveType = frame.newMove.selected - 1 });
		end
	end, "Create a new path with its first node at the creature. It is not assigned until you press Assign.");
	create:SetPoint("LEFT", frame.mine, "RIGHT", 4, 0);

	frame.pathInfo = Common.Paragraph(top, "", NOGGIT_COLOR_TEXT);
	frame.pathInfo:SetPoint("TOPLEFT", top, "TOPLEFT", 2, -78);
	frame.pathInfo:SetPoint("TOPRIGHT", top, "TOPRIGHT", -2, -78);

	frame.assigned = Common.Paragraph(top, "");
	frame.assigned:SetPoint("TOPLEFT", top, "TOPLEFT", 2, -96);
	frame.assigned:SetPoint("TOPRIGHT", top, "TOPRIGHT", -2, -96);

	return top;
end

local COLUMNS = {
	{ "#", 6, 26 }, { "X", 34, 66 }, { "Y", 102, 66 }, { "Z", 170, 56 }, { "Delay", 228, 56 }, { "Move", 286, 56 }, { "Facing", 344, 50 },
};

local function BuildList(parent, top, bottom)
	local columns = CreateFrame("Frame", "GameEditorWaypointsColumns", parent);
	columns:SetHeight(16);
	columns:SetPoint("TOPLEFT", top, "BOTTOMLEFT", 0, -8);
	columns:SetPoint("TOPRIGHT", top, "BOTTOMRIGHT", 0, -8);

	for _, column in ipairs(COLUMNS) do
		local label = Common.Label(columns, column[1], "NoggitFontBold");
		label:SetPoint("LEFT", columns, "LEFT", column[2] + 2, 0);
	end

	list = NoggitListBox_Create(parent, 20, function(row)
		row.cells = {};

		for index, column in ipairs(COLUMNS) do
			local cell = NoggitUI_FontString(row, "");
			cell:SetPoint("LEFT", row, "LEFT", column[2], 0);
			cell:SetWidth(column[3]);
			row.cells[index] = cell;
		end

		NoggitText_SetColor(row.cells[1], NOGGIT_COLOR_DISABLED);
	end, function(row, node)
		local cells = row.cells;
		cells[1]:SetText(tostring(node.point));
		cells[2]:SetText(Common.Float(node.x, 1));
		cells[3]:SetText(Common.Float(node.y, 1));
		cells[4]:SetText(Common.Float(node.z, 1));
		cells[5]:SetText((node.delay or 0) > 0 and format("%.1fs", node.delay / 1000) or "-");
		cells[6]:SetText(MoveTypeName(node.moveType));
		cells[7]:SetText(node.o and format("%d", floor(math.deg(node.o) % 360 + 0.5)) or "-");
	end);
	list:SetPoint("TOPLEFT", columns, "BOTTOMLEFT", 0, -2);
	list:SetPoint("BOTTOMRIGHT", bottom, "TOPRIGHT", 0, 6);
	list.onSelect = function(node)
		Waypoints.SelectPoint(node.point);
	end;
	list.onDoubleClick = function(node)
		GameEditor_RunCommand(format(".go xyz %.3f %.3f %.3f", node.x, node.y, node.z));
	end;
end

local function BuildEditor(parent)
	editor = CreateFrame("Frame", "GameEditorWaypointsNode", parent, "NoggitBoxTemplate");
	NoggitBox_Set(editor, NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID);
	editor:SetHeight(84);
	editor.nodeButtons = {};

	editor.title = Common.Label(editor, "No node selected", "NoggitFontBold");
	editor.title:SetPoint("TOPLEFT", editor, "TOPLEFT", 8, -6);

	local delayLabel = Common.Label(editor, "Delay ms", nil, NOGGIT_COLOR_DISABLED);
	delayLabel:SetPoint("TOPLEFT", editor, "TOPLEFT", 8, -26);
	editor.delay = NoggitSpinBox_Create(editor, 84, 0, 3600000, 500, 0);
	editor.delay:SetPoint("TOPLEFT", delayLabel, "BOTTOMLEFT", -2, -2);
	Common.Tooltip(editor.delay, "How long the creature waits at this node, in milliseconds");

	local moveLabel = Common.Label(editor, "Move type", nil, NOGGIT_COLOR_DISABLED);
	moveLabel:SetPoint("TOPLEFT", editor, "TOPLEFT", 100, -26);
	editor.move = NoggitComboBox_Create(editor, 84, MOVE_TYPES, 1);
	editor.move:SetPoint("TOPLEFT", moveLabel, "BOTTOMLEFT", -2, -2);

	editor.face = NoggitCheckBox_Create(editor, "Face", false);
	editor.face:SetPoint("TOPLEFT", editor, "TOPLEFT", 190, -22);
	Common.Tooltip(editor.face, "Turn the creature to this facing when it reaches the node. Off saves no facing.");
	editor.facing = NoggitSpinBox_Create(editor, 70, 0, 360, 5, 1, function()
		NoggitCheckBox_SetChecked(editor.face, true);
	end);
	editor.facing:SetPoint("TOPLEFT", editor, "TOPLEFT", 190, -42);

	local myFacing = Common.Button(editor, "Mine", 44, function()
		local _, _, _, o = CreatureEditor_GetPlayerPosition();

		if ( o ) then
			NoggitSpinBox_SetValue(editor.facing, math.deg(o) % 360, true);
			NoggitCheckBox_SetChecked(editor.face, true);
		else
			GameEditor_Notify("Your character position is not available", "error");
		end
	end, "Use the facing of your character");
	myFacing:SetPoint("LEFT", editor.facing, "RIGHT", 4, 0);

	local apply = Common.Button(editor, "Apply", 60, ApplyNode, "Save delay, move type and facing of this node");
	apply:SetPoint("TOPRIGHT", editor, "TOPRIGHT", -6, -6);
	local toMe = Common.Button(editor, "To me", 60, MoveNodeToCharacter, "Move this node to the position of your character");
	toMe:SetPoint("TOPRIGHT", apply, "BOTTOMRIGHT", 0, -4);
	local delete = Common.Button(editor, "Delete", 60, function()
		if ( not Waypoints.DeleteSelected() ) then
			GameEditor_Notify("Select a node first", "error");
		end
	end, "Delete this node (Delete)");
	delete:SetPoint("TOPRIGHT", toMe, "BOTTOMRIGHT", 0, -4);

	editor.nodeButtons = { apply, toMe, delete, myFacing, editor.face, editor.move };
	return editor;
end

local function BuildBottom(parent)
	local bottom = CreateFrame("Frame", "GameEditorWaypointsBottom", parent);
	bottom:SetHeight(370);
	bottom:SetPoint("BOTTOMLEFT", parent, "BOTTOMLEFT", 0, 0);
	bottom:SetPoint("BOTTOMRIGHT", parent, "BOTTOMRIGHT", 0, 0);

	BuildEditor(bottom);
	editor:SetPoint("TOPLEFT", bottom, "TOPLEFT", 0, 0);
	editor:SetPoint("TOPRIGHT", bottom, "TOPRIGHT", 0, 0);

	local newTitle = Section(bottom, "New nodes", editor, -8);

	local function PushNodeDefaults()
		if ( frame.newDelay and frame.newMove ) then
			CreatureEditor_SetNodeDefaults(floor(frame.newDelay.value + 0.5), frame.newMove.selected - 1);
		end
	end

	frame.newDelay = NoggitSpinBox_Create(bottom, 84, 0, 3600000, 500, 0, PushNodeDefaults);
	frame.newDelay:SetPoint("TOPLEFT", newTitle, "BOTTOMLEFT", -2, -4);
	Common.Tooltip(frame.newDelay, "Delay in milliseconds for nodes added in the world or with the buttons");
	frame.newMove = NoggitComboBox_Create(bottom, 84, MOVE_TYPES, 1, PushNodeDefaults);
	frame.newMove:SetPoint("LEFT", frame.newDelay, "RIGHT", 6, 0);

	local addMe = Common.Button(bottom, "Add at my character", 140, Waypoints.AddAtCharacter, "Add a node at your character after the selected node");
	addMe:SetPoint("LEFT", frame.newMove, "RIGHT", 6, 0);

	local pathTitle = Section(bottom, "Path", frame.newDelay, -10);

	local group = {};
	frame.scopeTemplate = NoggitCheckBox_Create(bottom, "Template addon", true, nil, group);
	frame.scopeTemplate:SetPoint("TOPLEFT", pathTitle, "BOTTOMLEFT", -2, -4);
	Common.Tooltip(frame.scopeTemplate, "Assign to creature_template_addon, every spawn of the entry without its own path walks it");
	frame.scopeSpawn = NoggitCheckBox_Create(bottom, "Spawn addon", false, nil, group);
	frame.scopeSpawn:SetPoint("LEFT", frame.scopeTemplate, "RIGHT", 12, 0);
	Common.Tooltip(frame.scopeSpawn, "Assign to creature_addon, only this spawn walks it");

	frame.assign = Common.Button(bottom, "Assign", 64, Assign, "Set path_id of the chosen addon to this path and switch it to waypoint movement");
	frame.assign:SetPoint("TOPLEFT", frame.scopeTemplate, "BOTTOMLEFT", 2, -6);
	local unassign = Common.Button(bottom, "Unassign", 70, Unassign, "Clear path_id of the chosen addon and switch it back to idle movement");
	unassign:SetPoint("LEFT", frame.assign, "RIGHT", 4, 0);
	frame.walk = Common.Button(bottom, "Walk path", 76, function() Send(OP_PREVIEW); end, "Let the creature walk this path now without saving anything");
	frame.walk:SetPoint("LEFT", unassign, "RIGHT", 12, 0);
	local stop = Common.Button(bottom, "Stop", 50, function() Send(OP_STOP, { pathId = 0 }); end, "Stop the preview walk and return the creature to its normal movement");
	stop:SetPoint("LEFT", frame.walk, "RIGHT", 4, 0);

	frame.showAlways = NoggitCheckBox_Create(bottom, "Show the path in every tool", state.showAlways, function(_, checked)
		Waypoints.SetShowAlways(checked);
	end);
	frame.showAlways:SetPoint("TOPLEFT", frame.assign, "BOTTOMLEFT", -2, -8);

	local help = Common.Paragraph(bottom, Waypoints.HELP);
	help:SetPoint("TOPLEFT", frame.showAlways, "BOTTOMLEFT", 2, -8);
	help:SetPoint("BOTTOMRIGHT", bottom, "BOTTOMRIGHT", -2, 2);

	return bottom;
end

function Waypoints.Build(parent)
	if ( frame ) then
		return frame;
	end

	frame = CreateFrame("Frame", "GameEditorCreatureWaypoints", parent);

	empty = Common.CreateEmptyState(frame);

	body = CreateFrame("Frame", "GameEditorCreatureWaypointsBody", frame);
	body:SetPoint("TOPLEFT", frame, "TOPLEFT", 6, -4);
	body:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -6, 6);

	local top = BuildPathArea(body);
	local bottom = BuildBottom(body);
	BuildList(body, top, bottom);

	frame:SetScript("OnShow", Refresh);
	Refresh();
	return frame;
end
