#ifndef WOW_SOURCE_UI_CKBSYSTEM_H
#define WOW_SOURCE_UI_CKBSYSTEM_H

#include "Engine/Source/Base/DynamicString.h"
#include "WoW/Source/Net/Types.h"

extern CDynamicString s_motd;
extern CDynamicString s_serverStatus;
extern CDynamicString s_serverNotice;

class CKBSystem {
    public:
    static void LeaveWorld();
    static void Initialize();
    static void Shutdown();
    static void CacheServerMessage(const char* text, int32_t id);
};

#endif
