#include "HoT/Runtime/WorldResume.h"
#include <cstring>
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"
#include "HoT/Core/CVars.h"
#include "HoT/Core/Log.h"
#include "HoT/Core/Modules.h"
#include "HoT/Game/Session.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "WoW/Source/Client.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Glue/CGlueMgr.h"
#include "WoW/Source/Net/NetClient/NetClient.h"
#include "WoW/Source/Net/Types.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/ChatFrame.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"

namespace HoT::WorldResume {

// The connection status code that means the server accepted the handshake.
static const int32_t STATUS_AUTH_OK = 12;
static const uint32_t HANDSHAKE_MS = 5000;

// SMSG_CHAT_SERVER_MESSAGE ids from the server's ServerMessageType.
static const int32_t SERVER_MSG_SHUTDOWN_TIME = 1;
static const int32_t SERVER_MSG_RESTART_TIME = 2;
static const int32_t SERVER_MSG_SHUTDOWN_CANCELLED = 4;
static const int32_t SERVER_MSG_RESTART_CANCELLED = 5;

static bool s_keepWorld = true;
static int32_t s_windowSeconds = 60;

static bool s_active;
static bool s_deliberateDisconnect;
static bool s_fallBackRequested;
static bool s_fallingBack;
static ClientConnection* s_netClient;
static uint32_t s_serverIp;
static uint16_t s_serverPort;
static uint32_t s_deadline;
static uint32_t s_nextAttempt;
static uint32_t s_attempts;
static uint64_t s_playerGuid;
static bool s_authenticated;
static uint32_t s_handshakeDeadline;

static uint32_t WindowSeconds() {
    return s_windowSeconds > 0 ? static_cast<uint32_t>(s_windowSeconds) : 60;
}

static void Reset() {
    s_active = false;
    s_fallBackRequested = false;
    s_netClient = nullptr;
    s_serverIp = 0;
    s_serverPort = 0;
    s_deadline = 0;
    s_nextAttempt = 0;
    s_attempts = 0;
    s_playerGuid = 0;
    s_authenticated = false;
    s_handshakeDeadline = 0;
}

// Must run before any teardown, while the connection still knows where it was pointed.
static bool CaptureServerAddress() {
    WowConnection* connection = s_netClient ? s_netClient->m_serverConnection : nullptr;

    if (!connection) {
        return false;
    }

    s_serverIp = connection->m_connectAddress;
    s_serverPort = connection->m_connectPort;

    return s_serverIp != 0 && s_serverPort != 0;
}

static void NoteServerMessage(int32_t messageId) {
    switch (messageId) {
        case SERVER_MSG_SHUTDOWN_TIME:
            s_deliberateDisconnect = true;
            break;

        case SERVER_MSG_RESTART_TIME:
        case SERVER_MSG_SHUTDOWN_CANCELLED:
        case SERVER_MSG_RESTART_CANCELLED:
            s_deliberateDisconnect = false;
            break;

        default:
            break;
    }
}

// A kick or a ban, never hold the world through one.
static void NoteSessionTerminated() {
    s_deliberateDisconnect = true;

    if (s_active) {
        LOG_INFO << "WorldResume: the server ended the session, giving up the hold";
        s_fallBackRequested = true;
    }
}

bool OnDisconnect(NetClient* client) {
    if (s_fallingBack) {
        return false;
    }

    // A failed attempt reports a disconnect of its own, which would run the teardown being held off.
    if (s_active) {
        return true;
    }

    if (!s_keepWorld || !client || client != ClientServices::s_currentConnection || !Session::InWorld()) {
        return false;
    }

    // Consumed here so an announced shutdown that never arrives cannot block the next crash.
    if (s_deliberateDisconnect) {
        s_deliberateDisconnect = false;
        LOG_INFO << "WorldResume: the server meant to disconnect us, letting the client drop out";
        return false;
    }

    s_netClient = ClientServices::s_currentConnection;

    if (!CaptureServerAddress()) {
        LOG_WARN << "WorldResume: no server address on the live connection, letting the client tear down";
        Reset();
        return false;
    }

    s_playerGuid = ClntObjMgrGetActivePlayer().m_guid;

    uint32_t now = OsGetAsyncTimeMs();
    s_active = true;
    s_fallBackRequested = false;
    s_attempts = 0;
    s_deadline = now + WindowSeconds() * 1000;
    s_nextAttempt = now + 2000;

    LOG_INFO << "WorldResume: holding the world, server " << (s_serverIp & 0xFF) << "." << ((s_serverIp >> 8) & 0xFF) << "." << ((s_serverIp >> 16) & 0xFF) << "." << ((s_serverIp >> 24) & 0xFF) << ":" << s_serverPort;

    return true;
}

bool IsActive() {
    return s_active;
}

// A reused connection keeps the old session's cipher and talks noise to the new server.
static void ResetConnectionCrypt() {
    WowConnection* primary = s_netClient ? s_netClient->m_serverConnection : nullptr;

    if (!primary) {
        return;
    }

    memset(primary->m_sendKeyInit, 0, sizeof(primary->m_sendKeyInit));
    memset(primary->m_receiveKeyInit, 0, sizeof(primary->m_receiveKeyInit));
    primary->m_encrypt = false;
    primary->m_uint375 = 0;
    primary->m_uint376 = 0;
}

static void Attempt() {
    // A previous attempt leaves this at connecting, which ConnectInternal refuses.
    s_netClient->m_netState = NS_INITIALIZED;

    ResetConnectionCrypt();

    char host[32];
    SStrPrintf(host, sizeof(host), "%u.%u.%u.%u", s_serverIp & 0xFF, (s_serverIp >> 8) & 0xFF, (s_serverIp >> 16) & 0xFF, (s_serverIp >> 24) & 0xFF);

    s_attempts++;
    s_authenticated = false;
    s_handshakeDeadline = OsGetAsyncTimeMs() + HANDSHAKE_MS;
    s_netClient->ConnectInternal(host, s_serverPort);
}

// The resume skips the glue stage that normally sends this, and without it the key bindings never initialise.
static void SendReadyForAccountDataTimes() {
    Net::Packet(static_cast<uint32_t>(CMSG_READY_FOR_ACCOUNT_DATA_TIMES)).Send();
}

// The client never asks by itself since as far as it knows it never left.
static void SendPlayerLogin() {
    if (s_playerGuid == 0) {
        LOG_WARN << "WorldResume: no character guid captured, cannot ask the server to log in";
        return;
    }

    ClientConnection* clientServices = ClientServices::s_currentConnection;

    if (!clientServices) {
        LOG_WARN << "WorldResume: no ClientServices, cannot ask the server to log in";
        return;
    }

    clientServices->CharacterLogin(WOWGUID{ s_playerGuid }, 0);
}

// The client keeps its channel list and only joins channels it does not already hold, while the server's
// new Player has none, so rejoin every one. Safe before the login completes, the server queues it.
// Password protected custom channels cannot come back, the client keeps no password.
static void SendChannelRejoins() {
    for (uint32_t i = 0; i < CGChat::s_channels.Count(); i++) {
        const ChatChannel& channel = CGChat::s_channels.m_data[i];

        // m_unk9C is set until the server confirms the join, m_unkA4 is the second join byte.
        if (!channel.m_channelNumber || channel.m_unk9C || !channel.m_name[0]) {
            continue;
        }

        Net::Packet(static_cast<uint32_t>(CMSG_CHAT_JOIN_CHANNEL))
            .PutUInt32(channel.m_zoneChannelID)
            .PutUInt8(1)
            .PutUInt8(channel.m_unkA4)
            .PutString(channel.m_name)
            .PutString("")
            .Send();
    }
}

// The server re-sends every object under a guid the client still holds, so purge the object manager the way a map change does.
static bool ReloadWorld() {
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (!player) {
        LOG_WARN << "WorldResume: no player object, skipping the world reload";
        return false;
    }

    uint32_t mapId = ClntObjMgrGetMapID();
    MapRec* map = g_mapDB.GetRecord(static_cast<int32_t>(mapId));

    if (!map || !map->m_directory) {
        LOG_WARN << "WorldResume: map " << mapId << " is not in Map.dbc, skipping the world reload";
        return false;
    }

    // No worldport ack, a login has nothing to ack.
    ClientReloadWorld(mapId, map->m_directory, player->GetPosition(), player->GetFacing());

    return true;
}

static bool IsConnecting() {
    return s_netClient && s_netClient->m_netState == NS_CONNECTING;
}

// Socket only, says nothing about whether the server will accept anything.
static bool HasLink() {
    return s_netClient && s_netClient->m_netState == NS_CONNECTED;
}

static int32_t ClientServicesStatus() {
    ClientConnection* clientServices = ClientServices::s_currentConnection;

    return clientServices ? clientServices->m_errorCode : 0;
}

static bool HasPendingSocket() {
    return s_netClient && s_netClient->m_serverConnection && s_netClient->m_serverConnection->m_sock >= 0;
}

static uint32_t SecondsLeft() {
    if (!s_active) {
        return 0;
    }

    uint32_t now = OsGetAsyncTimeMs();

    return now >= s_deadline ? 0 : (s_deadline - now) / 1000;
}

// Replays the suppressed teardown, only ever from Pump because it frees the Lua state.
static void FallBackToGlue() {
    ClientConnection* netClient = s_netClient;
    Reset();

    LOG_INFO << "WorldResume: handing back to the glue screens";

    if (netClient) {
        s_fallingBack = true;
        CGlueMgr::NetDisconnectHandler(netClient);
        s_fallingBack = false;
    }
}

static void Pump() {
    if (!s_active) {
        return;
    }

    if (s_fallBackRequested) {
        LOG_INFO << "WorldResume: cancelled by the player";
        FallBackToGlue();
        return;
    }

    uint32_t now = OsGetAsyncTimeMs();

    if (now >= s_deadline) {
        LOG_INFO << "WorldResume: window expired after " << s_attempts << " attempts";
        FallBackToGlue();
        return;
    }

    // Latched because the status moves on as soon as anything else is asked for.
    if (!s_authenticated && ClientServicesStatus() == STATUS_AUTH_OK) {
        s_authenticated = true;
        LOG_INFO << "WorldResume: the server accepted the session after " << s_attempts << " attempts";
    }

    if (s_authenticated && HasLink()) {
        SendReadyForAccountDataTimes();
        SendPlayerLogin();
        SendChannelRejoins();

        // After the login request, the order the client itself uses.
        ReloadWorld();

        LOG_INFO << "WorldResume: session resumed";
        Reset();
        return;
    }

    // Retrying over an in-flight connect orphans the session the server is about to hand us.
    if (IsConnecting()) {
        return;
    }

    if (HasLink() && now < s_handshakeDeadline) {
        return;
    }

    if (now < s_nextAttempt) {
        return;
    }

    // Each try closes the old socket, so back off further while one is held.
    s_nextAttempt = now + (HasPendingSocket() ? 5000 : 2000);
    Attempt();
}

static void Observe_SMSG_CHAT_SERVER_MESSAGE(uint32 opcode, CDataStore* msg) {
    Net::Packet packet(msg);

    if (packet.CanRead(4)) {
        NoteServerMessage(packet.GetInt32());
    }
}

static void Observe_SMSG_SESSION_TERMINATED(uint32 opcode, CDataStore* msg) {
    NoteSessionTerminated();
}

static int32_t Script_WorldResumeIsActive(lua_State* L) {
    lua_pushboolean(L, s_active);
    return 1;
}

static int32_t Script_WorldResumeSecondsLeft(lua_State* L) {
    lua_pushnumber(L, SecondsLeft());
    return 1;
}

// Only records the request, the teardown happens in Pump outside Lua.
static int32_t Script_WorldResumeCancel(lua_State* L) {
    s_fallBackRequested = true;
    lua_pushboolean(L, 1);
    return 1;
}

static void OnKeepWorldChanged(const char* value) {
    s_keepWorld = CVars::Bool(value);
}

static void OnWindowChanged(const char* value) {
    s_windowSeconds = CVars::Int(value);
}

class WorldResumeModule : public Module {
    public:
    const char* Name() const override { return "WorldResume"; }

    void OnInitializeGlobal() override {
        CVars::Register("worldResumeKeepWorld", "Hold the world and resume the session when the game server disconnects, instead of dropping to the glue screens.", "1", OnKeepWorldChanged);
        CVars::Register("worldResumeWindow", "Seconds to hold the world before handing back to the glue screens.", "60", OnWindowChanged);

        Net::RegisterObserver(SMSG_CHAT_SERVER_MESSAGE, Observe_SMSG_CHAT_SERVER_MESSAGE);
        Net::RegisterObserver(Net::SMSG_SESSION_TERMINATED, Observe_SMSG_SESSION_TERMINATED);

        Lua::RegisterFunction("WorldResumeIsActive", Script_WorldResumeIsActive);
        Lua::RegisterFunction("WorldResumeSecondsLeft", Script_WorldResumeSecondsLeft);
        Lua::RegisterFunction("WorldResumeCancel", Script_WorldResumeCancel);
    }

    void OnIdle() override {
        Pump();
    }
};

Module* GetModule() {
    static WorldResumeModule module;
    return &module;
}

}
