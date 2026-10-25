-- Vehicles workspace data, Vehicle and VehicleSeat rows as documents that preview through DbcEditor_ApplyLocal and save through DbcEditor_Save.

GameEditorVehicle = GameEditorVehicle or {};
local VE = GameEditorVehicle;
local ME = GameEditorModel;

VE.SEATS = 8;
VE.TABLES = { Vehicle = true, VehicleSeat = true };

-- VehicleSeat m_attachmentID picks one of these M2 attachment ids, the same table as CVehiclePassenger_C uses.
VE.SEAT_ATTACHMENTS = { [0] = 20, 34, 19, 21, 22, 17, 23, 24, 25, 15, 16, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 0 };

VE.VEHICLE_FLAGS = {
	{ bit = 0, name = "No strafe", tooltip = "0x1, sets MOVEFLAG2_NO_STRAFE" },
	{ bit = 1, name = "No jumping", tooltip = "0x2, sets MOVEFLAG2_NO_JUMPING" },
	{ bit = 2, name = "Full speed turning", tooltip = "0x4, sets MOVEFLAG2_FULLSPEEDTURNING" },
	{ bit = 4, name = "Allow pitching", tooltip = "0x10, sets MOVEFLAG2_ALLOW_PITCHING" },
	{ bit = 5, name = "Full speed pitching", tooltip = "0x20, sets MOVEFLAG2_FULLSPEEDPITCHING" },
	{ bit = 6, name = "Custom pitch", tooltip = "0x40, uses the pitch min and max of the row instead of -pi/2 to pi/2" },
	{ bit = 10, name = "Adjust aim angle", tooltip = "0x400, IsVehicleAimAngleAdjustable" },
	{ bit = 11, name = "Adjust aim power", tooltip = "0x800, IsVehicleAimPowerAdjustable" },
	{ bit = 21, name = "Fixed position", tooltip = "0x200000, cannons that stay rooted" },
};

VE.SEAT_FLAGS = {
	{ bit = 0, name = "Lower anim for enter", tooltip = "0x1" },
	{ bit = 1, name = "Lower anim for ride", tooltip = "0x2" },
	{ bit = 2, name = "Flag 0x4", tooltip = "0x4" },
	{ bit = 3, name = "Seat exit anim on voluntary exit", tooltip = "0x8" },
	{ bit = 4, name = "Flag 0x10", tooltip = "0x10" },
	{ bit = 5, name = "Flag 0x20", tooltip = "0x20" },
	{ bit = 6, name = "Flag 0x40", tooltip = "0x40" },
	{ bit = 7, name = "Flag 0x80", tooltip = "0x80" },
	{ bit = 8, name = "Flag 0x100", tooltip = "0x100" },
	{ bit = 9, name = "Hide passenger", tooltip = "0x200, the passenger is not drawn" },
	{ bit = 10, name = "Allow turning", tooltip = "0x400" },
	{ bit = 11, name = "Can control", tooltip = "0x800, the control seat" },
	{ bit = 12, name = "Can cast mount spell", tooltip = "0x1000" },
	{ bit = 13, name = "Uncontrolled", tooltip = "0x2000" },
	{ bit = 14, name = "Can attack", tooltip = "0x4000, attacks, spells and items work from the seat" },
	{ bit = 15, name = "Seat exit anim on forced exit", tooltip = "0x8000" },
	{ bit = 16, name = "Flag 0x10000", tooltip = "0x10000" },
	{ bit = 17, name = "Keep visual auras", tooltip = "0x20000" },
	{ bit = 18, name = "Vehicle exit anim voluntary", tooltip = "0x40000" },
	{ bit = 19, name = "Vehicle exit anim forced", tooltip = "0x80000" },
	{ bit = 20, name = "Passenger not selectable", tooltip = "0x100000" },
	{ bit = 21, name = "Flag 0x200000", tooltip = "0x200000" },
	{ bit = 22, name = "Has vehicle enter anim", tooltip = "0x400000" },
	{ bit = 23, name = "Using vehicle controls", tooltip = "0x800000" },
	{ bit = 24, name = "Enable vehicle zoom", tooltip = "0x1000000" },
	{ bit = 25, name = "Can enter or exit", tooltip = "0x2000000, enter and exit at will" },
	{ bit = 26, name = "Can switch", tooltip = "0x4000000, can switch seats" },
	{ bit = 27, name = "Wait for enter transition", tooltip = "0x8000000" },
	{ bit = 28, name = "Wait for exit transition", tooltip = "0x10000000" },
	{ bit = 29, name = "Can cast", tooltip = "0x20000000, shows the vehicle UI" },
	{ bit = 30, name = "Flag 0x40000000", tooltip = "0x40000000, checked with 0x800 when casting" },
	{ bit = 31, name = "Allows interaction", tooltip = "0x80000000" },
};

VE.SEAT_FLAGS_B = {
	{ bit = 1, name = "Usable forced", tooltip = "0x2" },
	{ bit = 3, name = "Targets in raid UI", tooltip = "0x8" },
	{ bit = 5, name = "Ejectable", tooltip = "0x20" },
	{ bit = 6, name = "Usable forced 2", tooltip = "0x40" },
	{ bit = 8, name = "Usable forced 3", tooltip = "0x100" },
	{ bit = 17, name = "Keep pet", tooltip = "0x20000" },
	{ bit = 25, name = "Usable forced 4", tooltip = "0x2000000" },
	{ bit = 26, name = "Can switch", tooltip = "0x4000000" },
	{ bit = 31, name = "Vehicle player frame UI", tooltip = "0x80000000" },
};

-- Labels of the columns, the ones the client never names are labelled from what the seat code reads them as.
VE.LABELS = {
	m_turnSpeed = "Turn speed",
	m_pitchSpeed = "Pitch speed",
	m_pitchMin = "Pitch min",
	m_pitchMax = "Pitch max",
	m_mouseLookOffsetPitch = "Mouse look pitch offset",
	m_cameraFadeDistScalarMin = "Camera fade min",
	m_cameraFadeDistScalarMax = "Camera fade max",
	m_cameraPitchOffset = "Camera pitch offset",
	m_facingLimitRight = "Facing limit right",
	m_facingLimitLeft = "Facing limit left",
	m_cameraYawOffset = "Camera yaw offset",
	m_uiLocomotionType = "UI locomotion type",
	m_vehicleUiindicatorID = "UI indicator",
	m_attachmentID = "Attachment",
	m_field33512213004 = "Enter pre delay",
	m_field33512213017 = "Exit pre delay",
	m_field33512213027 = "Passenger yaw",
	m_passengerPitch = "Passenger pitch",
	m_field33512213029 = "Passenger roll",
	m_passengerAttachmentID = "Passenger attachment",
	m_field33512213034 = "Vehicle anim bone",
	m_field33512213037 = "Vehicle enter anim delay",
	m_field33512213038 = "Vehicle exit anim delay",
	m_enterUisoundID = "Enter sound",
	m_field33512213041 = "Exit sound",
	m_uiSkin = "UI skin",
	m_flagsB = "Flags B",
	m_field33512213044 = "Camera entering delay",
	m_field33512213045 = "Camera entering duration",
	m_field33512213046 = "Camera exiting delay",
	m_field33512213047 = "Camera exiting duration",
	m_field33512213048 = "Camera offset X",
	m_field33512213049 = "Camera offset Y",
	m_field33512213050 = "Camera offset Z",
	m_field33512213051 = "Camera position chase rate",
	m_field33512213052 = "Camera facing chase rate",
	m_field33512213053 = "Camera entering zoom",
	m_field33512213054 = "Camera seat zoom min",
	m_field33512213055 = "Camera seat zoom max",
};

local Num = ME.Num;
VE.Num = Num;
VE.store = GameEditor_CreateDbcDocs("Vehicle editor", VE.TABLES);

function VE.Label(column)
	if ( VE.LABELS[column] ) then
		return VE.LABELS[column];
	end

	local name = string.gsub(column, "^m_", "");
	name = string.gsub(name, "(%l)(%u)", "%1 %2");
	return string.upper(string.sub(name, 1, 1))..string.lower(string.sub(name, 2));
end

-- The M2 attachment id of a seat attachment index, nil when the client finds none for it.
function VE.SeatAttachment(index)
	index = tonumber(index);
	return index and VE.SEAT_ATTACHMENTS[index];
end

function VE.SeatColumn(slot)
	return "m_seatID["..slot.."]";
end

-- Documents -------------------------------------------------------------------------------------------

function VE.OnChanged(func)
	VE.store:OnChanged(func);
end

function VE.Changed()
	VE.store:Changed();
end

function VE.Row(tableName, id)
	return ME.Row(tableName, id);
end

function VE.Doc(tableName, id)
	return VE.store:Doc(tableName, id);
end

function VE.CreateDoc(tableName, values)
	return VE.store:Create(tableName, values);
end

function VE.IsDirty(doc)
	return VE.store:IsDirty(doc);
end

function VE.SetMany(doc, values)
	return VE.store:SetMany(doc, values);
end

function VE.DirtyDocs()
	return VE.store:Dirty();
end

function VE.SaveAll()
	return VE.store:SaveAll();
end

function VE.RevertAll()
	return VE.store:RevertAll();
end
