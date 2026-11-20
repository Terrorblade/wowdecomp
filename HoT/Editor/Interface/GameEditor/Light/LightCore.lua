-- Lights workspace data, every touched Light, LightParams, band and LightSkybox row is a document that previews through DbcEditor_ApplyLocal and saves through DbcEditor_Save.

GameEditorLight = GameEditorLight or {};
local LE = GameEditorLight;

LE.DAY = 2880;
LE.INT_BANDS = 18;
LE.FLOAT_BANDS = 6;
LE.MAX_KEYS = 16;
LE.SLOTS = 8;
LE.YARD = 36;

LE.TABLES = { Light = true, LightParams = true, LightIntBand = true, LightFloatBand = true, LightSkybox = true };

-- Slots 0 to 3 are picked by DayNight from the water and storm state, 4 to 7 only through a screen effect.
LE.SLOT_NAMES = {
	[0] = "Clear",
	[1] = "Clear underwater",
	[2] = "Storm",
	[3] = "Storm underwater",
	[4] = "Death",
	[5] = "Unknown 5",
	[6] = "Unknown 6",
	[7] = "Unknown 7",
};

LE.SLOT_TIPS = {
	[0] = "Used above water in clear weather",
	[1] = "Used while the camera is under water",
	[2] = "Blended in by weather intensity",
	[3] = "Blended in by weather intensity under water",
	[4] = "Picked by the death screen effect, only read from the map default light",
	[5] = "Picked by a screen effect that asks for slot 5, only read from the map default light",
	[6] = "Picked by a screen effect that asks for slot 6, only read from the map default light",
	[7] = "Picked by a screen effect that asks for slot 7, only read from the map default light",
};

-- Order of DayNight::LoadLightingBands.
LE.INT_BAND_NAMES = {
	[0] = "Diffuse",
	[1] = "Ambient",
	[2] = "Sky top",
	[3] = "Sky middle",
	[4] = "Sky band 1",
	[5] = "Sky band 2",
	[6] = "Sky smog",
	[7] = "Fog",
	[8] = "Shadow opacity",
	[9] = "Sun",
	[10] = "Sun halo",
	[11] = "Cloud emissive",
	[12] = "Cloud layer 1 ambient",
	[13] = "Cloud layer 2 ambient",
	[14] = "Ocean close",
	[15] = "Ocean far",
	[16] = "River close",
	[17] = "River far",
};

LE.FLOAT_BAND_NAMES = {
	[0] = "Fog end",
	[1] = "Fog start scalar",
	[2] = "Glow",
	[3] = "Cloud density",
	[4] = "Unknown 4",
	[5] = "Unknown 5",
};

LE.FLOAT_BAND_TIPS = {
	[0] = "Fog end distance in DBC units, 36 units are one yard",
	[1] = "Where the fog starts as a fraction of the fog end, clamped to -1 .. 1",
	[2] = "Read into the light state, its use is not known",
	[3] = "Cloud density",
	[4] = "Read into the light state, its use is not known",
	[5] = "Read into the light state, its use is not known",
};

LE.COORDS = { "m_gameCoords[0]", "m_gameCoords[1]", "m_gameCoords[2]" };

LE.docs = LE.docs or {};
LE.listeners = LE.listeners or {};

local function Key(tableName, id)
	return tableName..":"..id;
end

function LE.Copy(source)
	local copy = {};

	for key, value in pairs(source or {}) do
		copy[key] = value;
	end

	return copy;
end

function LE.Same(a, b)
	if ( type(a) == "number" and type(b) == "number" ) then
		return abs(a - b) < 0.00001;
	end

	return tostring(a or "") == tostring(b or "");
end

function LE.IsOk(ok)
	return ok == true or ok == 1;
end

function LE.SlotColumn(slot)
	return "m_lightParamsID["..slot.."]";
end

function LE.IntBandId(paramsId, band)
	return paramsId * LE.INT_BANDS - (LE.INT_BANDS - 1) + band;
end

function LE.FloatBandId(paramsId, band)
	return paramsId * LE.FLOAT_BANDS - (LE.FLOAT_BANDS - 1) + band;
end

function LE.FormatTime(time)
	time = floor(tonumber(time) or 0);
	local minutes = floor(time / 2);
	local text = format("%02d:%02d", floor(minutes / 60), mod(minutes, 60));

	if ( mod(time, 2) == 1 ) then
		text = text..":30";
	end

	return text;
end

-- "HH:MM" or "HH:MM:30" to half minutes, nil when the text is not a time.
function LE.ParseTime(text)
	local hours, minutes, seconds = string.match(text or "", "^%s*(%d+):(%d+):?(%d*)%s*$");

	if ( not hours ) then
		return nil;
	end

	local time = (tonumber(hours) * 60 + tonumber(minutes)) * 2;

	if ( tonumber(seconds) and tonumber(seconds) >= 30 ) then
		time = time + 1;
	end

	return max(0, min(LE.DAY - 1, time));
end

-- Colors are stored as 0x00RRGGBB.
function LE.SplitColor(value)
	value = tonumber(value) or 0;

	if ( value < 0 ) then
		value = value + 4294967296;
	end

	return mod(floor(value / 65536), 256), mod(floor(value / 256), 256), mod(value, 256);
end

function LE.JoinColor(r, g, b)
	return floor(r + 0.5) * 65536 + floor(g + 0.5) * 256 + floor(b + 0.5);
end

-- Listeners -------------------------------------------------------------------------------------------

function LE.OnChanged(func)
	table.insert(LE.listeners, func);
end

function LE.Changed()
	for _, func in ipairs(LE.listeners) do
		local ok, err = pcall(func);

		if ( not ok ) then
			GameEditor_Print("Light editor: "..tostring(err), 1, 0.3, 0.3);
		end
	end
end

-- Rows and documents ------------------------------------------------------------------------------------

function LE.GetRow(tableName, id)
	if ( not DbcEditor_GetRow or not id ) then
		return nil;
	end

	local ok, row = pcall(DbcEditor_GetRow, tableName, id);

	if ( ok and type(row) == "table" ) then
		return row;
	end
end

function LE.MaxId(tableName)
	if ( not DbcEditor_GetMaxId ) then
		return 0;
	end

	local ok, value = pcall(DbcEditor_GetMaxId, tableName);
	return ok and tonumber(value) or 0;
end

function LE.FindDoc(tableName, id)
	id = tonumber(id);
	return id and LE.docs[Key(tableName, id)];
end

-- The open document of a row, loaded from the client the first time, nil when the row does not exist.
function LE.Doc(tableName, id)
	id = tonumber(id);

	if ( not id or id <= 0 ) then
		return nil;
	end

	local doc = LE.docs[Key(tableName, id)];

	if ( doc ) then
		return doc;
	end

	local row = LE.GetRow(tableName, id);

	if ( not row ) then
		return nil;
	end

	doc = { tableName = tableName, id = id, values = row, original = LE.Copy(row) };
	LE.docs[Key(tableName, id)] = doc;
	return doc;
end

-- Values of a row without opening it.
function LE.Values(tableName, id)
	local doc = LE.FindDoc(tableName, id);

	if ( doc ) then
		return doc.values;
	end

	return LE.GetRow(tableName, tonumber(id));
end

local function ApplyLocal(tableName, id, values)
	if ( not DbcEditor_ApplyLocal ) then
		GameEditor_Notify("DbcEditor_ApplyLocal is not available in this client build", "error");
		return false;
	end

	local ok, result = pcall(DbcEditor_ApplyLocal, tableName, id, values);
	return ok and result;
end

-- A new row that only exists in this client until it is saved.
function LE.CreateDoc(tableName, id, values)
	values = LE.Copy(values);
	values.m_id = id;

	local doc = { tableName = tableName, id = id, values = values, original = LE.Copy(values), isNew = true };
	LE.docs[Key(tableName, id)] = doc;
	ApplyLocal(tableName, id, values);
	return doc;
end

function LE.IsDirty(doc)
	if ( not doc ) then
		return false;
	end

	if ( doc.isNew ) then
		return true;
	end

	for column, value in pairs(doc.values) do
		if ( not LE.Same(value, doc.original[column]) ) then
			return true;
		end
	end

	return false;
end

-- Writes the changed columns into the document and the live client row.
function LE.SetMany(doc, values, quiet)
	local changes = {};
	local any = false;

	for column, value in pairs(values) do
		if ( not LE.Same(doc.values[column], value) ) then
			doc.values[column] = value;
			changes[column] = value;
			any = true;
		end
	end

	if ( any ) then
		ApplyLocal(doc.tableName, doc.id, changes);

		if ( not quiet ) then
			LE.Changed();
		end
	end

	return any;
end

function LE.Set(doc, column, value, quiet)
	return LE.SetMany(doc, { [column] = value }, quiet);
end

function LE.DirtyDocs(filter)
	local list = {};

	for _, doc in pairs(LE.docs) do
		if ( LE.IsDirty(doc) and (not filter or filter(doc)) ) then
			table.insert(list, doc);
		end
	end

	table.sort(list, function(a, b)
		if ( a.tableName == b.tableName ) then
			return a.id < b.id;
		end

		return a.tableName < b.tableName;
	end);

	return list;
end

function LE.DirtyCount()
	return #LE.DirtyDocs();
end

function LE.SaveAll()
	if ( not DbcEditor_Save ) then
		GameEditor_Notify("DbcEditor_Save is not available in this client build", "error");
		return 0;
	end

	local sent, waiting, failed = 0, 0, 0;

	for _, doc in ipairs(LE.DirtyDocs()) do
		if ( doc.saving ) then
			waiting = waiting + 1;
		else
			local ok, result = pcall(DbcEditor_Save, doc.tableName, doc.id, doc.values);

			if ( ok and result ) then
				doc.saving = LE.Copy(doc.values);
				sent = sent + 1;
			else
				GameEditor_Notify(format("Could not send %s %d", doc.tableName, doc.id), "error");
				failed = failed + 1;
			end
		end
	end

	if ( sent == 0 and waiting > 0 ) then
		GameEditor_Notify(format("%d row%s already saving, waiting for the server", waiting, waiting == 1 and " is" or "s are"), "info");
	elseif ( sent == 0 and failed == 0 ) then
		GameEditor_Notify("Nothing to save, no rows have changes", "info");
	end

	LE.Changed();
	return sent;
end

function LE.RevertDoc(doc)
	if ( not LE.IsDirty(doc) ) then
		return false;
	end

	-- A row the server never had stays in the client, the editor stops tracking it and a new light moves off every map.
	if ( doc.isNew ) then
		local values = LE.Copy(doc.original);

		if ( doc.tableName == "Light" ) then
			values.m_continentID = -1;
		end

		ApplyLocal(doc.tableName, doc.id, values);
		LE.docs[Key(doc.tableName, doc.id)] = nil;
		return true;
	end

	local reverted = false;

	if ( DbcEditor_RevertLocal ) then
		local ok, result = pcall(DbcEditor_RevertLocal, doc.tableName, doc.id);
		reverted = ok and result;
	end

	if ( not reverted ) then
		ApplyLocal(doc.tableName, doc.id, doc.original);
	end

	local row = LE.GetRow(doc.tableName, doc.id);

	if ( row ) then
		doc.values = row;
		doc.original = LE.Copy(row);
	else
		doc.values = LE.Copy(doc.original);
	end

	doc.remoteChanged = false;
	return true;
end

function LE.RevertAll()
	local count = 0;

	for _, doc in ipairs(LE.DirtyDocs()) do
		if ( LE.RevertDoc(doc) ) then
			count = count + 1;
		end
	end

	LE.Changed();
	return count;
end

-- New rows ----------------------------------------------------------------------------------------------

-- A params id whose 18 int band and 6 float band ids are all past the loaded tables.
function LE.NextParamsId()
	local id = LE.MaxId("LightParams") + 1;
	id = max(id, ceil(LE.MaxId("LightIntBand") / LE.INT_BANDS) + 1);
	id = max(id, ceil(LE.MaxId("LightFloatBand") / LE.FLOAT_BANDS) + 1);
	return id;
end

-- Copies a LightParams row with its bands into a new params id, source 0 makes an empty set.
function LE.CopyParams(source)
	source = tonumber(source) or 0;
	local id = LE.NextParamsId();
	local row = source > 0 and LE.Values("LightParams", source);

	LE.CreateDoc("LightParams", id, row or { m_glow = 0.5, m_waterShallowAlpha = 0.5, m_waterDeepAlpha = 1, m_oceanShallowAlpha = 0.75, m_oceanDeepAlpha = 1 });

	for band = 0, LE.INT_BANDS - 1 do
		local values = source > 0 and LE.Values("LightIntBand", LE.IntBandId(source, band));
		LE.CreateDoc("LightIntBand", LE.IntBandId(id, band), values or { m_num = 0 });
	end

	for band = 0, LE.FLOAT_BANDS - 1 do
		local values = source > 0 and LE.Values("LightFloatBand", LE.FloatBandId(source, band));
		LE.CreateDoc("LightFloatBand", LE.FloatBandId(id, band), values or { m_num = 0 });
	end

	return id;
end

-- The band row of a params set, created empty when create is set and it is missing.
function LE.BandDoc(tableName, paramsId, band, create)
	paramsId = tonumber(paramsId) or 0;

	if ( paramsId <= 0 ) then
		return nil;
	end

	local id = tableName == "LightIntBand" and LE.IntBandId(paramsId, band) or LE.FloatBandId(paramsId, band);
	local doc = LE.Doc(tableName, id);

	if ( not doc and create ) then
		doc = LE.CreateDoc(tableName, id, { m_num = 0 });
	end

	return doc;
end

-- Band keys ----------------------------------------------------------------------------------------------

function LE.GetKeys(values)
	local keys = {};
	local count = min(LE.MAX_KEYS, max(0, tonumber(values and values.m_num) or 0));

	for i = 0, count - 1 do
		table.insert(keys, { time = tonumber(values["m_time["..i.."]"]) or 0, value = tonumber(values["m_data["..i.."]"]) or 0 });
	end

	return keys;
end

function LE.SortKeys(keys)
	table.sort(keys, function(a, b) return a.time < b.time; end);
	return keys;
end

function LE.SetKeys(doc, keys)
	LE.SortKeys(keys);
	local values = { m_num = min(#keys, LE.MAX_KEYS) };

	for i = 0, LE.MAX_KEYS - 1 do
		local key = keys[i + 1];
		values["m_time["..i.."]"] = key and floor(key.time) or 0;
		values["m_data["..i.."]"] = key and key.value or 0;
	end

	LE.SetMany(doc, values);
end

-- Same pick of the two keys and the blend as GetBandTimeRange.
function LE.BandRange(keys, time)
	local count = #keys;

	for index = 1, count do
		local start = keys[index].time;
		local nextIndex = mod(index, count) + 1;
		local finish = keys[nextIndex].time;

		if ( finish > start ) then
			if ( time >= start and time <= finish ) then
				return index, nextIndex, (time - start) / (finish - start);
			end
		elseif ( time <= finish or time >= start ) then
			finish = finish + LE.DAY;

			if ( time < start ) then
				time = time + LE.DAY;
			end

			return index, nextIndex, (time - start) / max(1, finish - start);
		end
	end

	return 1, 1, 0;
end

function LE.InterpColor(keys, time)
	if ( #keys == 0 ) then
		return 0, 0, 0;
	end

	local from, to, t = LE.BandRange(keys, time);
	local r1, g1, b1 = LE.SplitColor(keys[from].value);
	local r2, g2, b2 = LE.SplitColor(keys[to].value);
	return r1 + (r2 - r1) * t, g1 + (g2 - g1) * t, b1 + (b2 - b1) * t;
end

function LE.InterpFloat(keys, time)
	if ( #keys == 0 ) then
		return 0;
	end

	local from, to, t = LE.BandRange(keys, time);
	return keys[from].value + (keys[to].value - keys[from].value) * t;
end

-- Events -------------------------------------------------------------------------------------------------

local function OnResult(ok, tableName, id)
	id = tonumber(id);
	local doc = id and LE.TABLES[tableName] and LE.FindDoc(tableName, id);

	if ( not doc or not doc.saving ) then
		return;
	end

	if ( LE.IsOk(ok) ) then
		doc.original = doc.saving;
		doc.isNew = false;
		doc.remoteChanged = false;
	end

	doc.saving = nil;
	LE.Changed();
end

local function OnRecordUpdated(tableName, id)
	id = tonumber(id);

	if ( not id or not LE.TABLES[tableName] ) then
		return;
	end

	local doc = LE.FindDoc(tableName, id);

	if ( doc and not doc.saving ) then
		if ( LE.IsDirty(doc) ) then
			-- The push replaced the live row, the unsaved edits go back on top so the preview keeps matching the editor.
			doc.remoteChanged = true;
			ApplyLocal(tableName, id, doc.values);
			GameEditor_Notify(format("%s %d was changed elsewhere, Save overwrites it and Revert loads it", tableName, id), "info");
		else
			local row = LE.GetRow(tableName, id);

			if ( row ) then
				doc.values = row;
				doc.original = LE.Copy(row);
				doc.isNew = false;
			end
		end
	end

	LE.Changed();
end

local events = CreateFrame("Frame", "GameEditorLightEvents");
events:RegisterEvent("DBC_EDITOR_RESULT");
events:RegisterEvent("DBC_RECORD_UPDATED");
events:SetScript("OnEvent", function(_, event, ...)
	if ( event == "DBC_EDITOR_RESULT" ) then
		OnResult(...);
	elseif ( event == "DBC_RECORD_UPDATED" ) then
		OnRecordUpdated(...);
	end
end);
