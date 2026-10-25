-- One row of a world table per key, read through WorldTable_Query and written through WorldTable_Save.

local Row = {};
Row.__index = Row;

local rows = {};

local function Copy(source)
	if ( type(source) ~= "table" ) then
		return source;
	end

	local copy = {};

	for key, value in pairs(source) do
		copy[key] = value;
	end

	return copy;
end

local function Same(a, b)
	if ( tonumber(a) and tonumber(b) ) then
		return math.abs(tonumber(a) - tonumber(b)) < 0.00001;
	end

	return tostring(a or "") == tostring(b or "");
end

-- onChange(row) runs after every load, edit and save result.
function GameEditor_CreateWorldRow(tableName, onChange)
	local row = setmetatable({ tableName = tableName, onChange = onChange }, Row);
	table.insert(rows, row);
	return row;
end

function Row:Changed()
	if ( self.onChange ) then
		local ok, err = pcall(self.onChange, self);

		if ( not ok ) then
			GameEditor_Print(self.tableName..": "..tostring(err), 1, 0.3, 0.3);
		end
	end
end

function Row:Load(key)
	key = tonumber(key);
	self.key = key;
	self.values, self.original, self.columns = nil, nil, nil;
	self.loaded, self.saving, self.sent, self.error = false, false, nil, nil;

	if ( key ) then
		if ( type(WorldTable_Query) ~= "function" ) then
			self.error = "WorldTable_Query is not in this client build";
		elseif ( not pcall(WorldTable_Query, self.tableName, key) ) then
			self.error = "Could not ask the server";
		end
	end

	self:Changed();
end

function Row:Exists()
	return self.values ~= nil;
end

function Row:Create(defaults)
	if ( not self.loaded or self.values ) then
		return;
	end

	local values = {};

	for _, column in ipairs(self.columns or {}) do
		values[column.name] = column.type == "string" and "" or 0;
	end

	for column, value in pairs(defaults or {}) do
		values[column] = value;
	end

	values[self.keyColumn] = self.key;
	self.values = values;
	self:Changed();
end

function Row:Remove()
	self.values = nil;
	self:Changed();
end

function Row:Set(column, value)
	if ( self.values and not Same(self.values[column], value) ) then
		self.values[column] = value;
		self:Changed();
	end
end

function Row:IsDirty()
	if ( (self.values == nil) ~= (self.original == nil) ) then
		return true;
	end

	for column, value in pairs(self.values or {}) do
		if ( not Same(value, self.original[column]) ) then
			return true;
		end
	end

	return false;
end

function Row:Revert()
	self.values = Copy(self.original);
	self:Changed();
end

function Row:Save()
	if ( not self.loaded or self.saving or not self:IsDirty() ) then
		return false;
	end

	local list = {};
	local out = {};

	for _, column in ipairs(self.columns or {}) do
		table.insert(list, { name = column.name, type = column.type });

		if ( self.values ) then
			local value = self.values[column.name];
			out[column.name] = column.type == "string" and tostring(value or "") or (tonumber(value) or 0);
		end
	end

	local ok, sent = pcall(WorldTable_Save, self.tableName, self.key, list, self.values and { out } or {});

	if ( not ok or not sent ) then
		GameEditor_Notify(format("Could not send %s %d", self.tableName, self.key), "error");
		return false;
	end

	self.sent = Copy(self.values);
	self.saving = true;
	self:Changed();
	return true;
end

function Row:OnRows(tableName, key, ok, err)
	if ( string.lower(tableName or "") ~= self.tableName or tonumber(key) ~= self.key ) then
		return;
	end

	if ( ok ~= 1 and ok ~= true ) then
		self.error = err ~= "" and err or "The server could not read the table";
		self:Changed();
		return;
	end

	local ok2, set = pcall(WorldTable_GetRows, self.tableName, self.key);

	if ( not ok2 or type(set) ~= "table" ) then
		return;
	end

	self.columns = set.columns or {};
	self.keyColumn = set.keyColumn;
	self.values = set.rows and set.rows[1] and Copy(set.rows[1]) or nil;
	self.original = Copy(self.values);
	self.loaded = true;
	self.error = nil;
	self:Changed();
end

function Row:OnResult(ok, tableName, key)
	if ( not self.saving or string.lower(tableName or "") ~= self.tableName or tonumber(key) ~= self.key ) then
		return;
	end

	if ( ok == 1 or ok == true ) then
		self.original = self.sent;
	end

	self.saving, self.sent = false, nil;
	self:Changed();
end

local events = CreateFrame("Frame", "GameEditorWorldRowEvents");
events:RegisterEvent("WORLD_TABLE_ROWS");
events:RegisterEvent("WORLD_TABLE_RESULT");
events:SetScript("OnEvent", function(_, event, ...)
	for _, row in ipairs(rows) do
		if ( event == "WORLD_TABLE_ROWS" ) then
			row:OnRows(...);
		else
			row:OnResult(...);
		end
	end
end);
