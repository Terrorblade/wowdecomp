#include "HoT/HoT.h"
#include "Engine/Source/Event/Event.h"
#include "HoT/Core/Hotkeys.h"
#include "HoT/Core/Log.h"
#include "HoT/Profiler/Profiler.h"
#include "HoT/Core/Modules.h"
#include "HoT/DB/Custom.h"
#include "HoT/Features/Features.h"
#include "HoT/Features/GemSocket.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/UIUtil/Cursor.h"
#include "WoW/Source/Ui/AuctionHouse.h"
#include "WoW/Source/Ui/CGBankInfo.h"
#include "WoW/Source/Ui/GuildBankFrame.h"
#include "WoW/Source/Ui/MailInfo.h"
#include "WoW/Source/Ui/MerchantFrame.h"
#include "WoW/Source/Ui/TradeFrame.h"
#include "HoT/Lua/Events.h"
#include "HoT/Lua/Script.h"
#include "HoT/Game/Session.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "HoT/Streaming/Channels.h"

namespace HoT {

static int32_t Idle(const void* data, void* param) {
    Profiler::Scope scope(Profiler::Zone::HoTModules);

    for (Module* module : Modules()) {
        module->OnIdle();
    }

    return 1;
}

void OnInitializeGlobal() {
    Log::Reset();
    LOG_INFO << "HoT initializing";
    Lua::RegisterEvents();

    for (Module* module : Modules()) {
        module->OnInitializeGlobal();
    }
}

void OnClientDBInitialized() {
    DB::LoadCustomTables();

    for (Module* module : Modules()) {
        module->OnClientDBInitialized();
    }
}

void OnInitializeClient() {
    for (Module* module : Modules()) {
        module->OnInitializeClient();
    }

    EventRegisterEx(EVENT_ID_IDLE, Idle, nullptr, 0.0f);
    Hotkeys::Install();
}

void OnRealmConnectionCreated(NetClient* connection) {
    Net::ApplyRealmHandlers(connection);
}

void OnPlayerClientInitialize() {
    Net::ApplyWorldHandlers();

    for (Module* module : Modules()) {
        module->OnPlayerClientInitialize();
    }

    // The DLL sent this after an archive scan finished, so it never reached the server before the login completed.
    Session::SendSanityCheck(true);
}

void OnNetMessage(uint32_t msgId, CDataStore* msg) {
    Net::Observe(msgId, msg);
}

void OnGameScriptsLoaded(const char** stockEvents, uint32_t stockCount) {
    Lua::LoadGame(stockEvents, stockCount);

    for (Module* module : Modules()) {
        module->OnScriptsLoaded(Lua::State::Game);
    }
}

void OnGlueScriptsLoaded(const char** stockEvents, uint32_t stockCount) {
    Lua::LoadGlue(stockEvents, stockCount);

    for (Module* module : Modules()) {
        module->OnScriptsLoaded(Lua::State::Glue);
    }
}

void OnCharacterEnum() {
    Session::SetInWorld(false);
    Streaming::Channels::OnCharacterEnum();
}

void OnCharacterListUpdated() {
    Net::Packet(Net::CMSG_REQUEST_CHAR_ENUM_LEVELS).Send();
}

void OnEnterWorld() {
    Session::SetInWorld(true);

    for (Module* module : Modules()) {
        module->OnEnterWorld();
    }
}

void OnLoadingScreenStarted() {
    for (Module* module : Modules()) {
        module->OnLoadingScreenStarted();
    }
}

static const uint32_t CURSOR_RESET_MODE_REPAIR = 0x11;

// Clicks the stock client must handle, such as selling, banking, mailing or trading the item.
static bool ClientOwnsContainerItemClick(lua_State* L) {
    if (lua_type(L, 3) == LUA_TSTRING || Spell_C_IsTargeting() || CursorGetResetMode() == CURSOR_RESET_MODE_REPAIR) {
        return true;
    }

    if (CGBankInfo::m_banker || CGMerchantInfo::m_merchant) {
        return true;
    }

    if (CGMailInfo::m_sendMailShowing && CGMailInfo::m_object) {
        return true;
    }

    if (CGAuctionHouse::m_auctionsTabShowing && CGAuctionHouse::m_auctioneer) {
        return true;
    }

    return static_cast<bool>(CGTradeInfo::m_tradingPlayer) || static_cast<bool>(CGGuildBankInfo::m_banker);
}

bool OnUseContainerItem(lua_State* L) {
    if (ClientOwnsContainerItemClick(L)) {
        return false;
    }

    return Crafting::HandleUseContainerItem(L) || GemSocket::HandleUseContainerItem(L);
}

void OnDestroyGame() {
    for (Module* module : Modules()) {
        module->OnDestroyGame();
    }
}

}
