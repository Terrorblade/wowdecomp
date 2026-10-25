-- DBC rows as editable documents, edits preview through DbcEditor_ApplyLocal and reach the server through DbcEditor_Save.

local Docs = {};
Docs.__index = Docs;

local stores = {};

local function Num(value)
	return tonumber(value) or 0;
end

local function Invoke(name, ...)
	local func = _G[name];

	if ( type(func) ~= "function" ) then
		return false, format("%s is not in this client build yet", name);
	end

	local ok, result = pcall(func, ...);

	if ( not ok ) then
		return false, format("%s failed: %s", name, tostring(result));
	end

	return true, result;
end

local function Copy(source)
	if ( type(source) ~= "table" ) then
		return source;
	end

	local copy = {};

	for key, value in pairs(source) do
		copy[key] = Copy(value);
	end

	return copy;
end

local function Same(a, b)
	if ( type(a) == "number" and type(b) == "number" ) then
		return math.abs(a - b) < 0.00001;
	end

	return tostring(a or "") == tostring(b or "");
end

local function Key(tableName, id)
	return tableName..":"..id;
end

function GameEditor_DbcRow(tableName, id)
	id = tonumber(id);

	if ( not id or id <= 0 ) then
		return nil;
	end

	local ok, row = Invoke("DbcEditor_GetRow", tableName, id);
	return ok and type(row) == "table" and row or nil;
end

function GameEditor_DbcMaxId(tableName)
	local ok, value = Invoke("DbcEditor_GetMaxId", tableName);
	return ok and Num(value) or 0;
end

-- The name column of a table, the localized name when the table has one.
function GameEditor_DbcNameColumn(tableName)
	local ok, schema = Invoke("DbcEditor_GetSchema", tableName);

	for _, column in ipairs(ok and type(schema) == "table" and schema or {}) do
		if ( column.isName ) then
			return column.name;
		end
	end
end

-- label prefixes error messages, tables is a set of the table names this store edits.
function GameEditor_CreateDbcDocs(label, tables)
	local store = setmetatable({ label = label, tables = tables, docs = {}, listeners = {} }, Docs);
	table.insert(stores, store);
	return store;
end

function Docs:OnChanged(func)
	table.insert(self.listeners, func);
end

function Docs:Changed()
	for _, func in ipairs(self.listeners) do
		local ok, err = pcall(func);

		if ( not ok ) then
			GameEditor_Print(self.label..": "..tostring(err), 1, 0.3, 0.3);
		end
	end
end

local function ApplyLocal(tableName, id, values)
	local ok, result = Invoke("DbcEditor_ApplyLocal", tableName, id, values);
	return ok and result;
end

function Docs:Find(tableName, id)
	id = tonumber(id);
	return id and self.docs[Key(tableName, id)];
end

-- The open document of a row, loaded from the client the first time, nil when the row does not exist.
function Docs:Doc(tableName, id)
	id = tonumber(id);

	if ( not id or id <= 0 ) then
		return nil;
	end

	local doc = self.docs[Key(tableName, id)];

	if ( doc ) then
		return doc;
	end

	local row = GameEditor_DbcRow(tableName, id);

	if ( not row ) then
		return nil;
	end

	doc = { tableName = tableName, id = id, values = row, original = Copy(row) };
	self.docs[Key(tableName, id)] = doc;
	return doc;
end

-- A new row copied from values, it only exists in this client until it is saved.
function Docs:Create(tableName, values)
	local id = GameEditor_DbcMaxId(tableName) + 1;

	for _, doc in pairs(self.docs) do
		if ( doc.tableName == tableName and doc.id >= id ) then
			id = doc.id + 1;
		end
	end

	values = Copy(values or {});
	values.m_id = id;

	local doc = { tableName = tableName, id = id, values = values, original = Copy(values), isNew = true };
	self.docs[Key(tableName, id)] = doc;
	ApplyLocal(tableName, id, values);
	self:Changed();
	return doc;
end

function Docs:IsDirty(doc)
	if ( not doc ) then
		return false;
	end

	if ( doc.isNew ) then
		return true;
	end

	for column, value in pairs(doc.values) do
		if ( not Same(value, doc.original[column]) ) then
			return true;
		end
	end

	return false;
end

function Docs:SetMany(doc, values)
	local changes = {};
	local any = false;

	for column, value in pairs(values) do
		if ( not Same(doc.values[column], value) ) then
			doc.values[column] = value;
			changes[column] = value;
			any = true;
		end
	end

	if ( any ) then
		ApplyLocal(doc.tableName, doc.id, changes);
		self:Changed();
	end

	return any;
end

function Docs:Dirty()
	local list = {};

	for _, doc in pairs(self.docs) do
		if ( self:IsDirty(doc) ) then
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

function Docs:SaveAll()
	local sent = 0;

	for _, doc in ipairs(self:Dirty()) do
		if ( not doc.saving ) then
			local ok, result = Invoke("DbcEditor_Save", doc.tableName, doc.id, doc.values);

			if ( ok and result ) then
				doc.saving = Copy(doc.values);
				sent = sent + 1;
			else
				GameEditor_Notify(format("Could not send %s %d", doc.tableName, doc.id), "error");
			end
		end
	end

	if ( sent == 0 ) then
		GameEditor_Notify("Nothing to save", "info");
	end

	self:Changed();
	return sent;
end

function Docs:RevertAll()
	local count = 0;

	for _, doc in ipairs(self:Dirty()) do
		if ( doc.isNew ) then
			self.docs[Key(doc.tableName, doc.id)] = nil;
		else
			local ok, reverted = Invoke("DbcEditor_RevertLocal", doc.tableName, doc.id);

			if ( not ok or not reverted ) then
				ApplyLocal(doc.tableName, doc.id, doc.original);
			end

			doc.values = GameEditor_DbcRow(doc.tableName, doc.id) or Copy(doc.original);
			doc.original = Copy(doc.values);
		end

		count = count + 1;
	end

	self:Changed();
	return count;
end

-- Asks before throwing away the unsaved rows, then runs after.
function Docs:ConfirmRevert(after)
	local count = #self:Dirty();

	if ( count == 0 ) then
		GameEditor_Notify("Nothing to revert", "info");
		return;
	end

	NoggitMessageBox_Show("Revert", format("Throw away the changes to %d row%s?", count, count == 1 and "" or "s"), {
		{ text = "Revert", onClick = function()
			self:RevertAll();

			if ( after ) then
				after();
			end
		end },
		{ text = "Cancel" },
	});
end

-- Events ------------------------------------------------------------------------------------------------

function Docs:OnResult(ok, tableName, id)
	local doc = self.tables[tableName] and self:Find(tableName, id);

	if ( not doc or not doc.saving ) then
		return;
	end

	if ( ok == true or ok == 1 ) then
		doc.original = doc.saving;
		doc.isNew = false;
	end

	doc.saving = nil;
	self:Changed();
end

function Docs:OnRecordUpdated(tableName, id)
	local doc = self.tables[tableName] and self:Find(tableName, id);

	if ( not doc or doc.saving ) then
		return;
	end

	if ( self:IsDirty(doc) ) then
		ApplyLocal(tableName, doc.id, doc.values);
		GameEditor_Notify(format("%s %d was changed elsewhere, Save overwrites it and Revert loads it", tableName, doc.id), "info");
	else
		local row = GameEditor_DbcRow(tableName, doc.id);

		if ( row ) then
			doc.values = row;
			doc.original = Copy(row);
		end
	end

	self:Changed();
end

local events = CreateFrame("Frame", "GameEditorDbcDocsEvents");
events:RegisterEvent("DBC_EDITOR_RESULT");
events:RegisterEvent("DBC_RECORD_UPDATED");
events:SetScript("OnEvent", function(_, event, ...)
	for _, store in ipairs(stores) do
		if ( event == "DBC_EDITOR_RESULT" ) then
			store:OnResult(...);
		else
			store:OnRecordUpdated(...);
		end
	end
end);
