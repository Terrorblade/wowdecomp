-- Models workspace data, open model documents, guarded ModelEditor calls, names and track helpers.

GameEditorModel = GameEditorModel or {};
local ME = GameEditorModel;

ME.RECENT_MAX = 12;
ME.RECENT_CVAR = "editorModelRecent";
ME.NOTICE_REPEAT = 3;

-- Texture types as CM2Model::ReplaceTexture uses them, 11 to 13 come from the display texture variations.
ME.TEXTURE_TYPES = {
	[0] = "Hardcoded",
	[1] = "Body skin",
	[2] = "Object skin",
	[3] = "Weapon blade",
	[4] = "Weapon handle",
	[5] = "Environment",
	[6] = "Hair",
	[7] = "Facial hair",
	[8] = "Skin extra",
	[9] = "UI skin",
	[10] = "Tauren mane",
	[11] = "Monster skin 1",
	[12] = "Monster skin 2",
	[13] = "Monster skin 3",
	[14] = "Item icon",
};

ME.TEXTURE_TIPS = {
	[0] = "The path in the model is drawn as it is",
	[1] = "Filled by the character skin of a player or a creature with extra display info",
	[2] = "Filled by the object or item the model belongs to, for example a cape or a shoulder",
	[3] = "Filled by the weapon blade texture",
	[4] = "Filled by the weapon handle texture",
	[5] = "Filled by the environment map",
	[6] = "Filled by the character hair texture",
	[7] = "Filled by the character facial hair texture",
	[8] = "Filled by the character skin extra texture",
	[9] = "Filled by the UI skin",
	[10] = "Filled by the tauren mane texture",
	[11] = "Filled by texture variation 1 of the CreatureDisplayInfo row, or of the GameObjectArtKit row",
	[12] = "Filled by texture variation 2 of the CreatureDisplayInfo row, or of the GameObjectArtKit row",
	[13] = "Filled by texture variation 3 of the CreatureDisplayInfo row, or of the GameObjectArtKit row",
	[14] = "Filled by Model:ReplaceIconTexture in the UI",
};

ME.TEXTURE_FLAGS = {
	{ bit = 0, name = "Wrap X", tooltip = "The texture repeats along U" },
	{ bit = 1, name = "Wrap Y", tooltip = "The texture repeats along V" },
};

-- M2Material blend modes, particles use the first seven with the same meaning.
ME.BLEND_MODES = {
	[0] = "Opaque",
	[1] = "Alpha key",
	[2] = "Alpha",
	[3] = "No alpha add",
	[4] = "Add",
	[5] = "Mod",
	[6] = "Mod2x",
	[7] = "Blend add",
};

-- M2Material render flags as the ribbon and batch setup in CM2Model reads them.
ME.MATERIAL_FLAGS = {
	{ bit = 0, name = "Unlit", tooltip = "Scene lighting does not touch it" },
	{ bit = 1, name = "Unfogged", tooltip = "Fog does not tint it" },
	{ bit = 2, name = "Two sided", tooltip = "Back faces are drawn too" },
	{ bit = 3, name = "No depth test", tooltip = "Draws over everything in front of it" },
	{ bit = 4, name = "No depth write", tooltip = "Does not hide what is drawn after it" },
	{ bit = 6, name = "Flag 0x40", tooltip = "The client leaves batches with this flag out of one of its batch lists, the use is not named" },
	{ bit = 7, name = "Flag 0x80", tooltip = "Keeps a blended batch in that batch list, the use is not named" },
};

ME.EMITTER_TYPES = {
	[1] = "Plane",
	[2] = "Sphere",
	[3] = "Spline",
};

-- M2Particle flags, named from the way CM2Model sets up CParticleEmitter2.
ME.PARTICLE_FLAGS = {
	{ bit = 0, name = "Unlit", tooltip = "0x1, the particles ignore scene lighting" },
	{ bit = 1, name = "Emitter flag 0x20", tooltip = "0x2, sets emitter flag 0x20 that changes how geometry model particles are built" },
	{ bit = 2, name = "Align to velocity", tooltip = "0x4, quads turn along the direction they move" },
	{ bit = 3, name = "Unfogged", tooltip = "0x8, fog does not tint the particles" },
	{ bit = 4, name = "Model space", tooltip = "0x10, particles stay in the space of the emitter and move with the model" },
	{ bit = 5, name = "Inherit scale", tooltip = "0x20, particle size follows the scale of the model" },
	{ bit = 6, name = "Inherit velocity", tooltip = "0x40, new particles take on the speed of the moving emitter" },
	{ bit = 7, name = "Spline option 0x80", tooltip = "0x80, spline emitters only, changes how the spawn point on the spline is picked" },
	{ bit = 8, name = "Spline emits up", tooltip = "0x100, spline emitters only, particles start moving straight up" },
	{ bit = 9, name = "Random spin direction", tooltip = "0x200, spin and tumble turn either way at random" },
	{ bit = 10, name = "Clamp tail to age", tooltip = "0x400, the tail is never longer than the particle is old" },
	{ bit = 11, name = "Emit along movement", tooltip = "0x800, spreads new particles along the path the emitter moved this frame" },
	{ bit = 12, name = "Flat XY quads", tooltip = "0x1000, quads lie in the XY plane instead of facing the camera" },
	{ bit = 13, name = "Project to ground", tooltip = "0x2000, new particles snap onto the ground below" },
	{ bit = 14, name = "Follow emitter", tooltip = "0x4000, particles keep following the emitter position" },
	{ bit = 15, name = "Burst", tooltip = "0x8000, one burst when it becomes visible instead of a steady stream" },
	{ bit = 16, name = "Random texture cell", tooltip = "0x10000, each particle starts on a random cell of the texture grid" },
	{ bit = 17, name = "Head", tooltip = "0x20000, draws the head quad" },
	{ bit = 18, name = "Tail", tooltip = "0x40000, draws the tail quad stretched along the motion" },
	{ bit = 19, name = "Separate scale variation", tooltip = "0x80000, width and height vary on their own" },
	{ bit = 23, name = "Compressed gravity", tooltip = "0x800000, the gravity keys are packed vectors, leave this as the file has it" },
	{ bit = 28, name = "Multi texture", tooltip = "0x10000000, modern emitters that blend three textures" },
};

ME.INTERPOLATION = {
	[0] = "None",
	[1] = "Linear",
	[2] = "Bezier",
	[3] = "Hermite",
};

ME.SEQUENCE_ALIAS = 0x40;

-- M2 attachment ids by the names the client code and the model files use them for.
ME.ATTACHMENT_NAMES = {
	[0] = "Shield / mount",
	[1] = "Right hand",
	[2] = "Left hand",
	[3] = "Right elbow",
	[4] = "Left elbow",
	[5] = "Right shoulder",
	[6] = "Left shoulder",
	[7] = "Right knee",
	[8] = "Left knee",
	[9] = "Right hip",
	[10] = "Left hip",
	[11] = "Helm",
	[12] = "Back",
	[13] = "Right shoulder flap",
	[14] = "Left shoulder flap",
	[15] = "Chest blood front",
	[16] = "Chest blood back",
	[17] = "Breath",
	[18] = "Player name",
	[19] = "Base",
	[20] = "Head",
	[21] = "Spell left hand",
	[22] = "Spell right hand",
	[23] = "Special 1",
	[24] = "Special 2",
	[25] = "Special 3",
	[26] = "Sheath main hand",
	[27] = "Sheath off hand",
	[28] = "Sheath shield",
	[29] = "Player name mounted",
	[30] = "Large weapon left",
	[31] = "Large weapon right",
	[32] = "Hip weapon left",
	[33] = "Hip weapon right",
	[34] = "Chest",
	[35] = "Hand arrow",
	[36] = "Bullet",
	[37] = "Spell hand omni",
	[38] = "Spell hand directed",
	[39] = "Vehicle seat 1",
	[40] = "Vehicle seat 2",
	[41] = "Vehicle seat 3",
	[42] = "Vehicle seat 4",
	[43] = "Vehicle seat 5",
	[44] = "Vehicle seat 6",
	[45] = "Vehicle seat 7",
	[46] = "Vehicle seat 8",
	[47] = "Left foot",
	[48] = "Right foot",
	[49] = "Shield no glove",
	[50] = "Spine low",
	[51] = "Altered right shoulder",
	[52] = "Altered left shoulder",
	[53] = "Belt buckle",
	[54] = "Sheath crossbow",
	[55] = "Head top",
};

ME.MOUNT_ANIM = 91;

ME.docs = ME.docs or {};
ME.order = ME.order or {};
ME.recent = ME.recent or {};
ME.listeners = ME.listeners or {};

local notices = {};
local animNames = {};
local pendingDocs = {};
local flushQueued = false;

local function Num(value)
	return tonumber(value) or 0;
end

ME.Num = Num;

function ME.HasBit(value, bit)
	if ( GameEditor_HasBit ) then
		return GameEditor_HasBit(Num(value), bit);
	end

	return mod(floor(Num(value) / 2 ^ bit), 2) == 1;
end

function ME.SetBit(value, bit, set)
	if ( GameEditor_SetBit ) then
		return GameEditor_SetBit(Num(value), bit, set);
	end

	value = Num(value);
	local has = ME.HasBit(value, bit);

	if ( set and not has ) then
		return value + 2 ^ bit;
	elseif ( not set and has ) then
		return value - 2 ^ bit;
	end

	return value;
end

-- Notices -------------------------------------------------------------------------------------------------

-- Error toast that does not repeat the same text within a few seconds.
function ME.Notice(text, kind)
	text = tostring(text or "");

	if ( text == "" ) then
		return;
	end

	local now = GetTime();

	if ( notices[text] and now - notices[text] < ME.NOTICE_REPEAT ) then
		return;
	end

	notices[text] = now;
	GameEditor_Notify(text, kind or "error");
end

-- Guarded calls ---------------------------------------------------------------------------------------------

local function Invoke(name, ...)
	local func = _G[name];

	if ( type(func) ~= "function" ) then
		return false, format("%s is not in this client build yet", name);
	end

	local ok, a, b = pcall(func, ...);

	if ( not ok ) then
		return false, format("%s failed: %s", name, tostring(a));
	end

	return true, a, b;
end

function ME.Has(name)
	return type(_G[name]) == "function";
end

-- Calls a function and returns what it returns, a missing function or a Lua error shows a notice.
function ME.Call(name, ...)
	local ok, a, b = Invoke(name, ...);

	if ( not ok ) then
		ME.Notice(a);
		return nil, a;
	end

	return a, b;
end

-- Same without any notice, for optional lookups.
function ME.Quiet(name, ...)
	local ok, a, b = Invoke(name, ...);

	if ( ok ) then
		return a, b;
	end
end

-- For calls that return ok, err. A false or nil result shows the message the API returned.
function ME.Do(name, ...)
	local ok, a, b = Invoke(name, ...);

	if ( not ok ) then
		ME.Notice(a);
		return nil, a;
	end

	if ( a == nil or a == false ) then
		ME.Notice(b or (name.." failed"));
		return nil, b;
	end

	return a, b;
end

-- Listeners --------------------------------------------------------------------------------------------------

function ME.OnChanged(func)
	table.insert(ME.listeners, func);
end

local function Flush()
	flushQueued = false;
	local docs = pendingDocs;
	pendingDocs = {};

	for _, func in ipairs(ME.listeners) do
		local ok, err = pcall(func, docs);

		if ( not ok ) then
			GameEditor_Print("Model editor: "..tostring(err), 1, 0.3, 0.3);
		end
	end
end

local flusher = CreateFrame("Frame", "GameEditorModelFlusher");
flusher:Hide();
flusher:SetScript("OnUpdate", function(self)
	self:Hide();
	Flush();
end);

-- Listeners run once on the next frame with every doc that changed, docs.all is set for a full refresh.
function ME.Changed(doc)
	if ( doc ) then
		pendingDocs[doc] = true;
	else
		pendingDocs.all = true;
	end

	if ( not flushQueued ) then
		flushQueued = true;
		flusher:Show();
	end
end

-- Paths ---------------------------------------------------------------------------------------------------------

function ME.CleanPath(path)
	path = strtrim(tostring(path or ""));
	path = string.gsub(path, "^\"(.*)\"$", "%1");
	path = string.gsub(path, "/", "\\");
	path = string.gsub(path, "\\\\+", "\\");
	return path;
end

function ME.StripExtension(path)
	return (string.gsub(path or "", "%.[^%.\\]*$", ""));
end

function ME.Extension(path)
	return string.lower(string.match(path or "", "%.([^%.\\]*)$") or "");
end

function ME.ToM2(path)
	local ext = ME.Extension(path);

	if ( ext == "mdx" or ext == "mdl" ) then
		return ME.StripExtension(path)..".m2";
	end

	return path;
end

-- DBC model names use .mdx like the shipped rows, the client reads them as .m2.
function ME.ToMdx(path)
	local ext = ME.Extension(path);

	if ( ext == "m2" or ext == "mdl" ) then
		return ME.StripExtension(path)..".mdx";
	end

	return path;
end

function ME.Folder(path)
	return string.match(path or "", "^(.*)\\[^\\]*$") or "";
end

function ME.FileName(path)
	return string.match(path or "", "([^\\]*)$") or "";
end

function ME.BaseName(path)
	return ME.StripExtension(ME.FileName(path));
end

function ME.SamePath(a, b)
	return string.lower(ME.StripExtension(ME.CleanPath(a))) == string.lower(ME.StripExtension(ME.CleanPath(b)));
end

function ME.FileExists(path)
	path = ME.CleanPath(path);

	if ( path == "" ) then
		return false;
	end

	local exists = ME.Call("ModelEditor_FileExists", path);
	return exists and true or false, exists ~= nil;
end

-- Recent models -------------------------------------------------------------------------------------------------

local function RecentCVar()
	if ( GetCVar(ME.RECENT_CVAR) ~= nil ) then
		return true;
	end

	if ( RegisterCVar ) then
		pcall(RegisterCVar, ME.RECENT_CVAR, "");
	end

	return GetCVar(ME.RECENT_CVAR) ~= nil;
end

function ME.LoadRecent()
	if ( not RecentCVar() ) then
		return;
	end

	local text = GetCVar(ME.RECENT_CVAR) or "";
	ME.recent = {};

	for path in string.gmatch(text, "[^;]+") do
		if ( #ME.recent < ME.RECENT_MAX ) then
			table.insert(ME.recent, path);
		end
	end
end

local function SaveRecent()
	if ( RecentCVar() ) then
		pcall(SetCVar, ME.RECENT_CVAR, table.concat(ME.recent, ";"));
	end
end

function ME.AddRecent(path)
	path = ME.CleanPath(path);

	if ( path == "" ) then
		return;
	end

	for index = #ME.recent, 1, -1 do
		if ( ME.SamePath(ME.recent[index], path) ) then
			table.remove(ME.recent, index);
		end
	end

	table.insert(ME.recent, 1, path);

	while ( #ME.recent > ME.RECENT_MAX ) do
		table.remove(ME.recent);
	end

	SaveRecent();
end

-- DBC rows --------------------------------------------------------------------------------------------------------

function ME.Row(tableName, id)
	id = tonumber(id);

	if ( not id or id <= 0 or not DbcEditor_GetRow ) then
		return nil;
	end

	local ok, row = pcall(DbcEditor_GetRow, tableName, id);
	return ok and type(row) == "table" and row or nil;
end

local tempIds = {};

-- A client only row id past the last one, the same key always gets the same id and no two keys share one.
function ME.TempId(tableName, key)
	local ids = tempIds[tableName];

	if ( not ids ) then
		ids = { next = ME.MaxId(tableName) + 1 };
		tempIds[tableName] = ids;
	end

	if ( not ids[key] and ids.next > 1 ) then
		ids[key] = ids.next;
		ids.next = ids.next + 1;
	end

	return ids[key];
end

function ME.MaxId(tableName)
	if ( not DbcEditor_GetMaxId ) then
		return 0;
	end

	local ok, value = pcall(DbcEditor_GetMaxId, tableName);
	return ok and Num(value) or 0;
end

function ME.Find(tableName, column, query, limit)
	if ( not DbcEditor_Find ) then
		return {};
	end

	local ok, rows;

	if ( column ) then
		ok, rows = pcall(DbcEditor_Find, tableName, column, query, limit or 50);
	else
		ok, rows = pcall(DbcEditor_Find, tableName, query, limit or 50);
	end

	return ok and type(rows) == "table" and rows or {};
end

-- CreatureModelData or GameObjectDisplayInfo rows whose model name is this path.
function ME.FindByModelName(tableName, path)
	local found = {};
	local base = ME.BaseName(path);

	if ( base == "" ) then
		return found;
	end

	for _, row in ipairs(ME.Find(tableName, "m_modelName", base, 200)) do
		if ( ME.SamePath(row.name, path) ) then
			table.insert(found, row.id);
		end
	end

	table.sort(found);
	return found;
end

function ME.AnimName(id)
	id = Num(id);

	if ( animNames[id] == nil ) then
		local row = ME.Quiet("DbcEditor_GetRow", "AnimationData", id);
		animNames[id] = type(row) == "table" and row.m_name or false;
	end

	return animNames[id] or format("Anim %d", id);
end

-- Documents ---------------------------------------------------------------------------------------------------------

local function NewEntry(doc, path)
	local entry = {
		doc = doc,
		path = path,
		source = {},
		sel = { texture = 0, batch = 0, particle = 0, ribbon = 0 },
		particles = {},
		ribbons = {},
	};

	ME.docs[doc] = entry;
	table.insert(ME.order, doc);
	return entry;
end

local function RemoveEntry(doc)
	ME.docs[doc] = nil;

	for index = #ME.order, 1, -1 do
		if ( ME.order[index] == doc ) then
			table.remove(ME.order, index);
		end
	end

	if ( ME.current == doc ) then
		ME.current = ME.order[#ME.order];
	end
end

function ME.Entry(doc)
	return doc and ME.docs[doc];
end

function ME.Current()
	return ME.current and ME.docs[ME.current];
end

function ME.Invalidate(doc)
	local entry = ME.docs[doc or 0];

	if ( entry ) then
		entry.info = nil;
		entry.particles = {};
		entry.ribbons = {};
	end
end

function ME.Info(doc)
	local entry = ME.docs[doc or 0];

	if ( not entry ) then
		return nil;
	end

	if ( not entry.info ) then
		local info = ME.Call("ModelEditor_GetInfo", doc);

		if ( type(info) == "table" ) then
			entry.info = info;
			entry.path = info.path or entry.path;
		end
	end

	return entry.info;
end

function ME.Particle(doc, index)
	local entry = ME.docs[doc or 0];

	if ( not entry or not index ) then
		return nil;
	end

	if ( entry.particles[index] == nil ) then
		local data = ME.Call("ModelEditor_GetParticle", doc, index);
		entry.particles[index] = type(data) == "table" and data or false;
	end

	return entry.particles[index] or nil;
end

function ME.Ribbon(doc, index)
	local entry = ME.docs[doc or 0];

	if ( not entry or not index ) then
		return nil;
	end

	if ( entry.ribbons[index] == nil ) then
		local data = ME.Call("ModelEditor_GetRibbon", doc, index);
		entry.ribbons[index] = type(data) == "table" and data or false;
	end

	return entry.ribbons[index] or nil;
end

function ME.IsDirty(doc)
	local info = ME.Info(doc);
	return info and info.dirty and true or false;
end

function ME.DirtyCount()
	local count = 0;

	for _, doc in ipairs(ME.order) do
		local entry = ME.docs[doc];

		if ( entry and entry.info and entry.info.dirty ) then
			count = count + 1;
		end
	end

	return count;
end

function ME.DocName(doc)
	local entry = ME.docs[doc or 0];

	if ( not entry ) then
		return "";
	end

	local info = entry.info;
	return (info and info.name and info.name ~= "" and info.name) or ME.BaseName(entry.path);
end

-- Brings the open docs in line with the client, the UI may have reloaded while docs stayed open.
function ME.SyncDocs()
	local docs = ME.Quiet("ModelEditor_GetDocs");

	if ( type(docs) ~= "table" ) then
		return;
	end

	local seen = {};

	for _, item in ipairs(docs) do
		local doc = item.doc;

		if ( doc ) then
			seen[doc] = true;
			local entry = ME.docs[doc] or NewEntry(doc, item.path);
			entry.path = item.path or entry.path;
		end
	end

	for _, doc in ipairs(ME.Copy(ME.order)) do
		if ( not seen[doc] ) then
			RemoveEntry(doc);
		end
	end

	if ( not ME.current or not ME.docs[ME.current] ) then
		ME.current = ME.order[#ME.order];
	end
end

function ME.SetCurrent(doc)
	if ( doc and ME.docs[doc] ) then
		ME.current = doc;
		ME.Changed();
	end
end

-- Opens a game path, source tells where it came from so Add to DBC can copy those rows.
function ME.Open(path, source)
	path = ME.CleanPath(path);

	if ( path == "" ) then
		ME.Notice("Type a model path first, for example Creature\\Murloc\\Murloc.m2", "info");
		return nil;
	end

	local exists, known = ME.FileExists(ME.ToM2(path));

	if ( known and not exists ) then
		exists = ME.FileExists(path);
	end

	if ( known and not exists ) then
		ME.Notice(format("%s was not found in the archives or the game folder", path));
		return nil;
	end

	local doc = ME.Do("ModelEditor_Open", path);

	if ( not doc ) then
		return nil;
	end

	local entry = ME.docs[doc] or NewEntry(doc, path);
	entry.info = nil;

	for key, value in pairs(source or {}) do
		entry.source[key] = value;
	end

	ME.AddRecent(path);
	ME.current = doc;
	ME.Invalidate(doc);
	ME.Changed();
	return doc;
end

function ME.OpenCreatureDisplay(displayId, extra)
	displayId = tonumber(displayId);
	local display = ME.Row("CreatureDisplayInfo", displayId);

	if ( not display ) then
		ME.Notice(format("CreatureDisplayInfo %s does not exist", tostring(displayId)));
		return nil;
	end

	local modelData = ME.Row("CreatureModelData", display.m_modelID);

	if ( not modelData or (modelData.m_modelName or "") == "" ) then
		ME.Notice(format("CreatureDisplayInfo %d points at CreatureModelData %s, it has no model", displayId, tostring(display.m_modelID)));
		return nil;
	end

	local source = { kind = "creature", displayId = displayId, modelDataId = Num(display.m_modelID) };

	for key, value in pairs(extra or {}) do
		source[key] = value;
	end

	return ME.Open(modelData.m_modelName, source);
end

function ME.OpenGameObjectDisplay(displayId, extra)
	displayId = tonumber(displayId);
	local display = ME.Row("GameObjectDisplayInfo", displayId);

	if ( not display or (display.m_modelName or "") == "" ) then
		ME.Notice(format("GameObjectDisplayInfo %s does not exist or has no model", tostring(displayId)));
		return nil;
	end

	local source = { kind = "gameobject", displayId = displayId };

	for key, value in pairs(extra or {}) do
		source[key] = value;
	end

	return ME.Open(display.m_modelName, source);
end

function ME.Close(doc)
	if ( not doc or not ME.docs[doc] ) then
		return;
	end

	ME.Call("ModelEditor_Close", doc);
	RemoveEntry(doc);
	ME.Changed();
end

-- After a successful edit, the change event does the same when the client sends it.
function ME.Edited(doc)
	ME.Invalidate(doc);
	ME.Changed(doc);
end

-- An edit call that refreshes the doc when it worked, returns what the call returned.
function ME.Edit(name, doc, ...)
	local result, message = ME.Do(name, doc, ...);

	if ( result ~= nil ) then
		ME.Edited(doc);
	end

	return result, message;
end

-- Values and tracks --------------------------------------------------------------------------------------------

local DEFAULT_COMPS = { "x", "y", "z", "w" };

function ME.Copy(source)
	if ( type(source) ~= "table" ) then
		return source;
	end

	local copy = {};

	for key, value in pairs(source) do
		copy[key] = ME.Copy(value);
	end

	return copy;
end

-- Component i of a vector table that may use x, y, z keys or array slots.
function ME.Comp(value, index, comps)
	if ( type(value) ~= "table" ) then
		return index == 1 and Num(value) or 0;
	end

	local name = comps and comps[index] or DEFAULT_COMPS[index];
	return Num(value[name] or value[index] or value[DEFAULT_COMPS[index]]);
end

-- Vector tables carry both the named keys and the array slots so either reading works.
function ME.MakeVector(values, comps)
	local vector = {};

	for index, value in ipairs(values) do
		vector[index] = value;
		vector[(comps and comps[index]) or DEFAULT_COMPS[index]] = value;
	end

	return vector;
end

function ME.SameValue(a, b)
	if ( type(a) ~= "table" and type(b) ~= "table" ) then
		return math.abs(Num(a) - Num(b)) < 0.00001;
	end

	for index = 1, 3 do
		if ( math.abs(ME.Comp(a, index) - ME.Comp(b, index)) >= 0.00001 ) then
			return false;
		end
	end

	return true;
end

function ME.IsAlias(sequence)
	return sequence and ME.HasBit(sequence.flags, 6) or false;
end

function ME.TrackKeyCount(track)
	local count, sequences = 0, 0;

	for _, sequence in ipairs(track and track.sequences or {}) do
		local keys = #(sequence.times or {});

		if ( keys > 0 ) then
			count = count + keys;
			sequences = sequences + 1;
		end
	end

	return count, sequences;
end

-- The value when every sequence holds one equal key, state is "empty", "constant" or "animated".
function ME.TrackConstant(track)
	local value;

	for _, sequence in ipairs(track and track.sequences or {}) do
		local values = sequence.values or {};

		if ( #values > 1 ) then
			return nil, "animated";
		end

		if ( #values == 1 ) then
			if ( value == nil ) then
				value = values[1];
			elseif ( not ME.SameValue(value, values[1]) ) then
				return nil, "animated";
			end
		end
	end

	if ( value == nil ) then
		return nil, "empty";
	end

	return value, "constant";
end

function ME.IsEditableSequence(sequence)
	return sequence and not sequence.external and not ME.IsAlias(sequence) or false;
end

-- A track that holds the value at time 0 in every editable sequence, alias and .anim sequences are left out.
function ME.ConstantTrack(value, info, old)
	local track = { interpolation = old and old.interpolation or 0, globalSequence = -1, sequences = {} };
	local sequences = info and info.sequences;

	for _, sequence in ipairs(sequences or {}) do
		if ( ME.IsEditableSequence(sequence) ) then
			table.insert(track.sequences, { sequence = sequence.index, times = { 0 }, values = { ME.Copy(value) } });
		end
	end

	if ( #track.sequences == 0 and (not sequences or #sequences == 0) ) then
		table.insert(track.sequences, { sequence = 0, times = { 0 }, values = { ME.Copy(value) } });
	end

	return track;
end

function ME.SequenceByIndex(info, index)
	for _, sequence in ipairs(info and info.sequences or {}) do
		if ( sequence.index == index ) then
			return sequence;
		end
	end
end

function ME.SequenceText(sequence)
	if ( not sequence ) then
		return "No sequence";
	end

	local text = format("%d  %s (%d)", sequence.index, ME.AnimName(sequence.id), Num(sequence.id));

	if ( Num(sequence.variation) > 0 ) then
		text = text..format(" var %d", sequence.variation);
	end

	return text;
end

function ME.AttachmentName(id)
	id = Num(id);
	return ME.ATTACHMENT_NAMES[id] or format("Attachment %d", id);
end

function ME.IsAvailable(sequence)
	return sequence and sequence.available ~= false or false;
end

-- One entry per animation id that can play, the Model frame and SetBoneSequence pick the variation themselves.
function ME.AnimItems(sequences)
	local items = {};
	local seen = {};

	for _, sequence in ipairs(sequences or {}) do
		local id = Num(sequence.id);

		if ( ME.IsAvailable(sequence) and not seen[id] ) then
			seen[id] = true;
			table.insert(items, { value = id, text = format("%s (%d)", ME.AnimName(id), id) });
		end
	end

	table.sort(items, function(a, b) return a.value < b.value; end);
	return items;
end

function ME.TextureText(info, index)
	index = tonumber(index);

	for _, texture in ipairs(info and info.textures or {}) do
		if ( texture.index == index ) then
			if ( Num(texture.type) ~= 0 ) then
				return format("%d %s", index, ME.TEXTURE_TYPES[Num(texture.type)] or ("type "..texture.type));
			end

			return format("%d %s", index, ME.FileName(texture.path or ""));
		end
	end

	return format("%s (missing)", tostring(index));
end

-- Events --------------------------------------------------------------------------------------------------------

ME.selectionHandlers = ME.selectionHandlers or {};

local events = CreateFrame("Frame", "GameEditorModelEvents");
events:RegisterEvent("MODEL_EDITOR_CHANGED");
events:RegisterEvent("MODEL_EDITOR_SAVED");
events:RegisterEvent("DBC_EDITOR_RESULT");
events:RegisterEvent("HOT_CREATURE_EDITOR_DATA");
events:RegisterEvent("HOT_GOB_EDITOR_DATA");
events:SetScript("OnEvent", function(_, event, ...)
	if ( event == "MODEL_EDITOR_CHANGED" ) then
		local doc = tonumber((...));

		if ( doc and ME.docs[doc] ) then
			ME.Edited(doc);
		end
	elseif ( event == "MODEL_EDITOR_SAVED" ) then
		local doc, path = ...;
		doc = tonumber(doc);
		local entry = doc and ME.docs[doc];

		if ( entry ) then
			if ( path and path ~= "" ) then
				entry.path = path;
				ME.AddRecent(path);
			end

			ME.Edited(doc);
		end
	elseif ( event == "DBC_EDITOR_RESULT" ) then
		if ( ME.OnDbcResult ) then
			ME.OnDbcResult(...);
		end
	else
		for _, handler in ipairs(ME.selectionHandlers) do
			pcall(handler, event, ...);
		end
	end
end);

ME.LoadRecent();
