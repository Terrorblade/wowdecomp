#include <cmath>
#include <vector>
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "HoT/Core/Modules.h"
#include "HoT/Features/Features.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "WoW/Source/Net/Types.h"
#include "WoW/Source/Object/Movement.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Object/Types.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/WorldClient/World.h"

namespace HoT::MovementForce {

enum ForceType : uint8_t {
    FORCE_TYPE_DIRECTIONAL = 0,
    FORCE_TYPE_RADIAL = 1,
};

enum ForceFlags : uint32_t {
    FORCE_FLAG_NO_COLLISION = 0x1,
    FORCE_FLAG_NO_GROUND = 0x2,
};

struct ForceField {
    uint32_t m_id;
    uint8_t m_type;
    C3Vector m_origin;
    C3Vector m_direction;
    float m_magnitude;
    float m_radius;
    uint32_t m_flags;
    int32_t m_durationMs;
    uint32_t m_startMs;
};

struct UnitForces {
    uint64_t m_guid;
    std::vector<ForceField> m_fields;
    uint32_t m_lastHeartbeatMs;
};

// The pushed client stays authoritative over its own position, so it keeps telling the server where it drifted.
static const uint32_t kHeartbeatIntervalMs = 250;

// Same flag set World::Intersect callers use for solid geometry.
static const uint32_t kIntersectSolid = 0x00100111;

static const uint32_t kClientOwnsZ = 0xFF | MOVEMENTFLAG_FALLING | MOVEMENTFLAG_ASCENDING | MOVEMENTFLAG_DESCENDING | MOVEMENTFLAG_SWIMMING | MOVEMENTFLAG_FLYING | MOVEMENTFLAG_ONTRANSPORT;

static const float kWaistTrace = 1.2f;
static const float kWallSkin = 0.1f;
static const float kGroundUp = 2.0f;
static const float kGroundDown = 3.0f;
static const float kMaxStepDown = 2.0f;
static const float kZDeadzone = 0.05f;
static const float kOriginDeadzone = 0.05f;

static std::vector<UnitForces> s_units;
static int64_t s_lastClocks;

static UnitForces* FindUnit(uint64_t guid) {
    for (UnitForces& unit : s_units) {
        if (unit.m_guid == guid) {
            return &unit;
        }
    }

    return nullptr;
}

// Sums every active force into a push velocity in yards per second, dt only clamps an inward pull.
static void SumForces(const std::vector<ForceField>& fields, const C3Vector& pos, float facing, float dt, float& outX, float& outY, float& outZ, uint32_t& outFlags) {
    outX = 0.0f;
    outY = 0.0f;
    outZ = 0.0f;
    outFlags = 0;

    for (const ForceField& field : fields) {
        if (field.m_type == FORCE_TYPE_DIRECTIONAL) {
            outX += field.m_direction.x * field.m_magnitude;
            outY += field.m_direction.y * field.m_magnitude;
            outZ += field.m_direction.z * field.m_magnitude;
            outFlags |= field.m_flags;
            continue;
        }

        float dx = pos.x - field.m_origin.x;
        float dy = pos.y - field.m_origin.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        bool inward = field.m_magnitude < 0.0f;

        float dirX;
        float dirY;

        if (dist < kOriginDeadzone) {
            if (inward) {
                continue;
            }

            // Sitting on the origin pushes along the current facing so the unit still slides off.
            dirX = std::cos(facing);
            dirY = std::sin(facing);
        } else {
            dirX = dx / dist;
            dirY = dy / dist;
        }

        float strength = field.m_magnitude;

        if (field.m_radius > 0.0f) {
            if (dist >= field.m_radius) {
                continue;
            }

            strength *= 1.0f - dist / field.m_radius;
        }

        if (inward && dt > 0.0f && -strength * dt > dist) {
            strength = -dist / dt;
        }

        outX += dirX * strength;
        outY += dirY * strength;
        outFlags |= field.m_flags;
    }
}

// Runs identically on every client that sees the unit, which keeps the motion in sync without a packet per frame.
static void DriftUnit(CGUnit_C* unit, const std::vector<ForceField>& fields, float dt) {
    CMovement_C* movement = unit->m_movement;

    if (!movement) {
        return;
    }

    C3Vector& pos = movement->m_position;
    C3Vector& anchor = movement->m_anchorPos;
    float facing = movement->m_facing;
    uint32_t moveFlags = movement->m_flags;

    float pushX;
    float pushY;
    float pushZ;
    uint32_t forceFlags;
    SumForces(fields, pos, facing, dt, pushX, pushY, pushZ, forceFlags);

    if (pushX == 0.0f && pushY == 0.0f && pushZ == 0.0f) {
        return;
    }

    float posX = pos.x;
    float posY = pos.y;
    float posZ = pos.z;

    float dX = pushX * dt;
    float dY = pushY * dt;
    float dZ = pushZ * dt;

    bool onTransport = (moveFlags & MOVEMENTFLAG_ONTRANSPORT) != 0;

    if (!onTransport && (forceFlags & FORCE_FLAG_NO_COLLISION) == 0) {
        C3Vector start(posX, posY, posZ + kWaistTrace);
        C3Vector end(posX + dX, posY + dY, posZ + kWaistTrace + dZ);
        C3Vector hit(0.0f, 0.0f, 0.0f);
        float t = 1.0f;

        if (World::Intersect(start, end, &hit, &t, kIntersectSolid, nullptr) && t < 1.0f) {
            float len = std::sqrt(dX * dX + dY * dY + dZ * dZ);
            float tSafe = t - (len > 0.0f ? kWallSkin / len : 0.0f);

            if (tSafe < 0.0f) {
                tSafe = 0.0f;
            }

            dX *= tSafe;
            dY *= tSafe;
            dZ *= tSafe;
        }
    }

    pos.x = posX + dX;
    pos.y = posY + dY;
    anchor.x += dX;
    anchor.y += dY;

    if (dZ != 0.0f) {
        pos.z = posZ + dZ;
        anchor.z += dZ;
        return;
    }

    bool snapToGround = (moveFlags & kClientOwnsZ) == 0 && (forceFlags & FORCE_FLAG_NO_GROUND) == 0;

    if (!snapToGround) {
        return;
    }

    C3Vector start(pos.x, pos.y, posZ + kGroundUp);
    C3Vector end(pos.x, pos.y, posZ - kGroundDown);
    C3Vector hit(0.0f, 0.0f, 0.0f);
    float t = 1.0f;

    if (World::Intersect(start, end, &hit, &t, kIntersectSolid, nullptr)) {
        float drop = posZ - hit.z;

        if (std::fabs(drop) > kZDeadzone && drop <= kMaxStepDown) {
            pos.z = hit.z;
            anchor.z += hit.z - posZ;
        }
    }
}

// Observers already simulate the force, so this is only a correction channel for the server.
static void SendSelfHeartbeat(CGUnit_C* unit, UnitForces& forces, uint32_t nowMs) {
    if (nowMs - forces.m_lastHeartbeatMs < kHeartbeatIntervalMs) {
        return;
    }

    forces.m_lastHeartbeatMs = nowMs;
    unit->SendMovementUpdate(nowMs, MSG_MOVE_HEARTBEAT, 0.0f, 0, 0, 0xFF);
}

static void TickForces() {
    if (s_units.empty()) {
        return;
    }

    int64_t frequency = OsGetAsyncClocksPerSecond();
    int64_t nowClocks = static_cast<int64_t>(OsGetAsyncTimeClocks());

    if (s_lastClocks == 0) {
        s_lastClocks = nowClocks;
        return;
    }

    float dt = static_cast<float>(nowClocks - s_lastClocks) / static_cast<float>(frequency);
    s_lastClocks = nowClocks;

    if (dt <= 0.0f) {
        return;
    }

    // Clamp after a stall so the unit does not teleport.
    if (dt > 0.1f) {
        dt = 0.1f;
    }

    uint32_t nowMs = OsGetAsyncTimeMs();
    uint64_t selfGuid = ClntObjMgrGetActivePlayer().m_guid;

    for (size_t i = 0; i < s_units.size();) {
        UnitForces& forces = s_units[i];

        for (size_t f = 0; f < forces.m_fields.size();) {
            const ForceField& field = forces.m_fields[f];

            if (field.m_durationMs > 0 && nowMs - field.m_startMs >= static_cast<uint32_t>(field.m_durationMs)) {
                forces.m_fields.erase(forces.m_fields.begin() + f);
            } else {
                ++f;
            }
        }

        if (forces.m_fields.empty()) {
            s_units.erase(s_units.begin() + i);
            continue;
        }

        CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(WOWGUID{ forces.m_guid }, TYPE_UNIT, __FILE__, __LINE__));

        if (unit) {
            DriftUnit(unit, forces.m_fields, dt);

            if (forces.m_guid == selfGuid) {
                SendSelfHeartbeat(unit, forces, nowMs);
            }
        }

        ++i;
    }
}

// Hands the server our exact position at the moment a force started or stopped so it can rebase observers.
static void SendAck(uint64_t guid, uint32_t forceId, bool applied) {
    if (guid != ClntObjMgrGetActivePlayer().m_guid) {
        return;
    }

    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (!player || !player->m_movement) {
        return;
    }

    const CMovement_C* movement = player->m_movement;

    Net::Packet ack(Net::CMSG_MOVEMENT_FORCE_ACK);
    ack.PutUInt64(guid);
    ack.PutUInt32(forceId);
    ack.PutUInt8(applied ? 1 : 0);
    ack.PutFloat(movement->m_position.x);
    ack.PutFloat(movement->m_position.y);
    ack.PutFloat(movement->m_position.z);
    ack.PutFloat(movement->m_facing);
    ack.Send();
}

static int32_t Packet_SMSG_APPLY_MOVEMENT_FORCE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);

    uint64_t guid = packet.GetUInt64();

    ForceField field = {};
    field.m_id = packet.GetUInt32();
    field.m_type = packet.GetUInt8();
    field.m_origin.x = packet.GetFloat();
    field.m_origin.y = packet.GetFloat();
    field.m_origin.z = packet.GetFloat();
    field.m_direction.x = packet.GetFloat();
    field.m_direction.y = packet.GetFloat();
    field.m_direction.z = packet.GetFloat();
    field.m_magnitude = packet.GetFloat();
    field.m_radius = packet.GetFloat();
    field.m_flags = packet.GetUInt32();
    field.m_durationMs = packet.GetInt32();
    field.m_startMs = OsGetAsyncTimeMs();

    if (field.m_radius < 0.0f) {
        field.m_radius = 0.0f;
    }

    if (field.m_type == FORCE_TYPE_DIRECTIONAL) {
        float len = std::sqrt(field.m_direction.x * field.m_direction.x + field.m_direction.y * field.m_direction.y + field.m_direction.z * field.m_direction.z);

        if (len <= 0.0001f) {
            return 1;
        }

        field.m_direction.x /= len;
        field.m_direction.y /= len;
        field.m_direction.z /= len;
    }

    UnitForces* forces = FindUnit(guid);

    if (!forces) {
        UnitForces entry;
        entry.m_guid = guid;
        entry.m_lastHeartbeatMs = OsGetAsyncTimeMs();
        s_units.push_back(std::move(entry));
        forces = &s_units.back();
    }

    for (ForceField& existing : forces->m_fields) {
        if (existing.m_id == field.m_id) {
            // Keep the original start so a refresh does not extend a timed force.
            field.m_startMs = existing.m_startMs;
            existing = field;
            SendAck(guid, field.m_id, true);
            return 1;
        }
    }

    forces->m_fields.push_back(field);
    SendAck(guid, field.m_id, true);
    return 1;
}

static int32_t Packet_SMSG_CLEAR_MOVEMENT_FORCE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint64_t guid = packet.GetUInt64();
    uint32_t id = packet.GetUInt32();

    for (size_t i = 0; i < s_units.size(); ++i) {
        if (s_units[i].m_guid != guid) {
            continue;
        }

        if (id == 0xFFFFFFFF) {
            s_units.erase(s_units.begin() + i);
            SendAck(guid, id, false);
            return 1;
        }

        std::vector<ForceField>& fields = s_units[i].m_fields;

        for (size_t f = 0; f < fields.size(); ++f) {
            if (fields[f].m_id != id) {
                continue;
            }

            fields.erase(fields.begin() + f);

            if (fields.empty()) {
                s_units.erase(s_units.begin() + i);
            }

            SendAck(guid, id, false);
            return 1;
        }

        return 1;
    }

    return 1;
}

class MovementForceModule : public Module {
    public:
    const char* Name() const override {
        return "MovementForce";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_APPLY_MOVEMENT_FORCE, Packet_SMSG_APPLY_MOVEMENT_FORCE);
        Net::RegisterWorldHandler(Net::SMSG_CLEAR_MOVEMENT_FORCE, Packet_SMSG_CLEAR_MOVEMENT_FORCE);
    }

    void OnDestroyGame() override {
        s_units.clear();
    }

    void OnIdle() override {
        TickForces();
    }
};

Module* GetModule() {
    static MovementForceModule module;
    return &module;
}

}
