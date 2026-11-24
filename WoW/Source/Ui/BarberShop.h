#ifndef WOW_SOURCE_UI_BARBERSHOP_H
#define WOW_SOURCE_UI_BARBERSHOP_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Storm/H/stpl.h"
#include "WoW/Source/Net/Types.h"
#include "WoW/Source/Ui/USEABLESTYLE.h"
#include "WoW/Source/DB/Rec/BarberShopStyleRec.h"

struct lua_State;

class CDataStore;

int32_t Packet_SMSG_ENABLE_BARBER_SHOP(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_BARBER_SHOP_RESULT(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);

class CGBarberShop {
    public:

    enum APPLY_RESULT : uint32_t {
        BARBER_SUCCESS = 0,
        BARBER_UI_NOT_ENOUGH_MONEY_1 = 1,
        BARBER_UI_NOT_SITTING = 2,
        BARBER_UI_NOT_ENOUGH_MONEY_3 = 3,
        BARBER_RESULT_NOT_READ = 0xFFFFFFFF,
    };

    static void EnterWorld();
    static STYLEREC* GetStyle(int32_t type, int32_t race, int32_t sex, int32_t data);
    static float GetMinCameraDistance();
    static uint8_t m_barberShopEnabled;
    static uint8_t m_helmHidden;
    static int32_t m_prevView;
    static float m_minCameraDistance;
    static uint8_t m_resetPortrait;
    static STYLEREC* m_currentHairStyle;
    static STYLEREC* m_currentFacialHair;
    static STYLEREC* m_currentSkinColor;
    static TSHashTable<USEABLESTYLE, HASHKEY_NONE> m_useableStyles;
    static uint8_t m_initialized;

    static void AppearanceApplied();
    static void DisableBarberShop();
    static const BarberShopStyleRec* GetCurrentStyle(int32_t type);
    static void ResetAppearance();
    static bool IsCurrentStyle(int32_t type);
    static int32_t GetTotalCost();
    static void ApplyAppearanceChange();
    static void Reset();
    static void SetNextBarberShopStyle(int32_t type, bool backward);
    static void InitializeGame();
    static void ShutdownGame();
    static void RegisterScriptFunctions();
    static void UnregisterScriptFunctions();
};

#endif
