#ifndef WOW_SOURCE_UI_GMTICKETINFO_H
#define WOW_SOURCE_UI_GMTICKETINFO_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/Net/Types.h"

class CDataStore;
struct lua_State;

enum GM_TICKET_RESPONSE : uint32_t {
    GM_TICKET_RESPONSE_DB_ERROR       = 0,
    GM_TICKET_RESPONSE_ALREADY_EXISTS = 1,
    GM_TICKET_RESPONSE_CREATED        = 2,
    GM_TICKET_RESPONSE_CREATE_ERROR   = 3,
    GM_TICKET_RESPONSE_UPDATED        = 4,
    GM_TICKET_RESPONSE_UPDATE_ERROR   = 5,
    GM_TICKET_RESPONSE_HAS_TICKET     = 6,
    GM_TICKET_RESPONSE_DELETED        = 9,
};

enum GM_TICKET_STATUS : uint32_t {
    GM_TICKET_STATUS_UNKNOWN_1 = 1,
    GM_TICKET_STATUS_CLOSED    = 2,
    GM_TICKET_STATUS_SURVEY    = 3,
    GM_TICKET_STATUS_UNKNOWN_4 = 4,
};

int32_t Packet_Group_5(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);

struct StaticConstant {
    /* 0000 */ const char* m_name;
    /* 0004 */ int32_t m_value;
};

class CGGMTicketInfo {
    public:
    static void LeaveWorld();
    static void EnterWorld();
    static void InitializeGame();
    static void ShutdownGame();
    static int32_t m_ticketID;
    static int32_t m_responseID;
    static void CreateTicket(const char* text, int32_t needResponse, uint32_t moreHelp);
};

class CGGMReportLag {
    public:
    static void Submit(int32_t type);
};

void GMTicketInfoRegisterScriptFunctions();
void GMTicketInfoUnregisterScriptFunctions();

void SendGetTicketOpcode();
void SendGMTicketUpdateText(const char* text);
void SendGMTicketDeleteTicket();
void SendGMResponseResolve();

#endif
