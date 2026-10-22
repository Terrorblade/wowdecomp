#if defined(WOWDECOMP_PLATFORM_POSIX)

#include "WoW/Source/Ui/UILCD/LCD.h"

// The Logitech G-series LCD SDK (lgLcd) is Windows-only. Linux keeps CGLCD's entry points so the callers stay
// unchanged, and the LCD stays uninitialized: the Windows build does the same when no LCD keyboard is attached.

CEzLcd* CGLCD::m_lcd;
int32_t CGLCD::m_chatAlertLines;
uint8_t CGLCD::initComplete;
CGPlayer_C* CGLCD::m_player;
uint8_t CGLCD::m_inWorld;

void CGLCD::Initialize() {
}

void CGLCD::Shutdown() {
}

void CGLCD::Login() {
}

void CGLCD::Logout() {
}

void CGLCD::EnterWorld() {
}

void CGLCD::LeaveWorld() {
}

void CGLCD::ClearWaitQueueScreen() {
}

void CGLCD::IdleDisconnectWarning() {
}

void CGLCD::ShowBattlefieldScreen() {
}

void CGLCD::HideBattlefieldAlert() {
}

void CGLCD::OnBattlefieldReady(const BattlefieldStatus* status) {
}

void CGLCD::OnBattlefieldActive() {
}

void CGLCD::OnWhisper(const char* name) {
}

void CGLCD::AuctionOutbid(const char* name) {
}

void CGLCD::AuctionExpired(const char* name) {
}

void CGLCD::AuctionSold(const char* name) {
}

void CGLCD::AuctionWon(const char* name) {
}

#endif
