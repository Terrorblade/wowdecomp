#include "HoT/Core/Modules.h"
#include "HoT/Features/Features.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"

namespace HoT::PlayerSettings {

enum PlayerSettingId : uint8_t {
    PLAYER_SETTING_AOE_LOOT = 0,
    PLAYER_SETTING_AUTO_ITEM_LEVEL = 1,
    PLAYER_SETTING_PET_AUTO_LOOT = 2,
    NUM_PLAYER_SETTINGS = 3,
};

static uint32_t s_values[NUM_PLAYER_SETTINGS];
static bool s_hasData;

static int32_t Packet_SMSG_PLAYER_SETTINGS(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint8_t count = packet.GetUInt8();

    for (uint8_t i = 0; i < count && packet.CanRead(5); i++) {
        uint8_t setting = packet.GetUInt8();
        uint32_t value = packet.GetUInt32();

        if (setting < NUM_PLAYER_SETTINGS) {
            s_values[setting] = value;
        }
    }

    s_hasData = true;
    Lua::SignalEvent("HOT_PLAYER_SETTINGS_UPDATE", nullptr);
    return 1;
}

static int32_t Script_GetPlayerSetting(lua_State* L) {
    uint32_t setting = static_cast<uint32_t>(lua_tonumber(L, 1));

    if (!s_hasData || setting >= NUM_PLAYER_SETTINGS) {
        lua_pushnil(L);
    } else {
        lua_pushnumber(L, s_values[setting]);
    }

    return 1;
}

static int32_t Script_SetPlayerSetting(lua_State* L) {
    uint32_t setting = static_cast<uint32_t>(lua_tonumber(L, 1));
    uint32_t value = static_cast<uint32_t>(lua_tonumber(L, 2));

    if (setting >= NUM_PLAYER_SETTINGS) {
        return 0;
    }

    s_values[setting] = value;
    s_hasData = true;

    Net::Packet packet(Net::CMSG_PLAYER_SETTING_UPDATE);
    packet.PutUInt8(static_cast<uint8_t>(setting));
    packet.PutUInt32(value);
    packet.Send();
    return 0;
}

class PlayerSettingsModule : public Module {
    public:
    const char* Name() const override {
        return "PlayerSettings";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_PLAYER_SETTINGS, Packet_SMSG_PLAYER_SETTINGS);

        Lua::RegisterFunction("GetPlayerSetting", Script_GetPlayerSetting);
        Lua::RegisterFunction("SetPlayerSetting", Script_SetPlayerSetting);
    }
};

Module* GetModule() {
    static PlayerSettingsModule module;
    return &module;
}

}
