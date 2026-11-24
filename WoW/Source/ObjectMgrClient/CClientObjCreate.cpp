#include "Engine/Source/Base/CDataStore.h"
#include "WoW/Source/ObjectMgrClient/CClientObjCreate.h"

// VERIFY: e0224246 auto
// OFFSET: 0x004D3A80
void CClientObjCreate::Skip(CDataStore* msg) {
    uint16_t flags = 0;
    msg->Get(flags);

    if (flags & UPDATEFLAG_LIVING) {
        CClientMoveUpdate::Skip(msg);
    } else if (flags & UPDATEFLAG_POSITION) {
        C3Vector position(0.0f, 0.0f, 0.0f);
        C3Vector transportPosition(0.0f, 0.0f, 0.0f);
        SmartGUID transport;
        *msg >> transport;
        msg->Get(position.x);
        msg->Get(position.y);
        msg->Get(position.z);
        msg->Get(transportPosition.x);
        msg->Get(transportPosition.y);
        msg->Get(transportPosition.z);

        float facing;
        msg->Get(facing);

        float transportFacing;
        msg->Get(transportFacing);
    } else if (flags & UPDATEFLAG_STATIONARY_POSITION) {
        C3Vector position(0.0f, 0.0f, 0.0f);
        *msg >> position;

        float facing;
        msg->Get(facing);
    }

    if (flags & UPDATEFLAG_UNKNOWN) {
        uint32_t lowGuid = 0;
        msg->Get(lowGuid);
    }

    if (flags & UPDATEFLAG_LOWGUID) {
        uint32_t highGuid = 0;
        msg->Get(highGuid);
    }

    if (flags & UPDATEFLAG_HAS_TARGET) {
        SmartGUID target;
        *msg >> target;
    }

    if (flags & UPDATEFLAG_TRANSPORT) {
        uint32_t transportTime = 0;
        msg->Get(transportTime);
    }

    if (flags & UPDATEFLAG_VEHICLE) {
        uint32_t vehicleId = 0;
        msg->Get(vehicleId);

        uint32_t vehicleOrientation = 0;
        msg->Get(vehicleOrientation);
    }

    if (flags & UPDATEFLAG_ROTATION) {
        uint64_t rotation = 0;
        msg->Get(rotation);
    }
}

// INLINED
CClientObjCreate::CClientObjCreate() {
    this->m_updateFlags = UPDATEFLAG_NONE;
    this->m_attackTarget = WOWGUID();
    this->m_packedRotation = 0;
}

// VERIFY: 3e2ce9a0 manual
// OFFSET: 0x004D3890
int32_t CClientObjCreate::Get(CDataStore* msg) {
    uint16_t flags = 0;
    msg->Get(flags);
    this->m_updateFlags = flags;

    if (flags & UPDATEFLAG_LIVING) {
        *msg >> this->m_move;
    } else if (flags & UPDATEFLAG_POSITION) {
        SmartGUID transport;
        *msg >> transport;
        this->m_move.m_status.m_transportGuid = transport;
        msg->Get(this->m_move.m_status.m_position.x);
        msg->Get(this->m_move.m_status.m_position.y);
        msg->Get(this->m_move.m_status.m_position.z);
        msg->Get(this->m_move.m_status.m_transportPosition.x);
        msg->Get(this->m_move.m_status.m_transportPosition.y);
        msg->Get(this->m_move.m_status.m_transportPosition.z);
        msg->Get(this->m_move.m_status.m_facing);
        msg->Get(this->m_move.m_status.m_transportFacing);
    } else if (flags & UPDATEFLAG_STATIONARY_POSITION) {
        *msg >> this->m_move.m_status.m_position;
        msg->Get(this->m_move.m_status.m_facing);
        this->m_move.m_status.m_transportGuid = WOWGUID();
        this->m_move.m_status.m_transportPosition = this->m_move.m_status.m_position;
    }

    if (this->m_updateFlags & UPDATEFLAG_UNKNOWN) {
        msg->Get(this->m_lowGuid);
    } else {
        this->m_lowGuid = 0;
    }

    if (this->m_updateFlags & UPDATEFLAG_LOWGUID) {
        msg->Get(this->m_highGuid);
    } else {
        this->m_highGuid = 0;
    }

    if (this->m_updateFlags & UPDATEFLAG_HAS_TARGET) {
        *msg >> *reinterpret_cast<SmartGUID*>(&this->m_attackTarget);
    } else {
        this->m_attackTarget = WOWGUID();
    }

    if (this->m_updateFlags & UPDATEFLAG_TRANSPORT) {
        msg->Get(this->m_transportTime);
    }

    if (this->m_updateFlags & UPDATEFLAG_VEHICLE) {
        msg->Get(this->m_vehicleId);
        msg->Get(this->m_vehicleOrientation);
    }

    if (this->m_updateFlags & UPDATEFLAG_ROTATION) {
        uint64_t rotation = 0;
        msg->Get(rotation);
        this->m_packedRotation = rotation;
    } else {
        this->m_packedRotation = 0;
    }

    return msg->m_size >= msg->m_read;
}
