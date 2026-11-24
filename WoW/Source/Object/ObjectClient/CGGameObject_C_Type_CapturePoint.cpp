#include "CGGameObject_C_Type_CapturePoint.h"
#include "WoW/Source/Object/GameObjectDef.h"
#include "WoW/Source/Object/GameObjectStats.h"
#include "WoW/Source/Ui/GameUI.h"

// VERIFY: 09194bd4 auto
// OFFSET: 0x007127E0
CGGameObject_C_Type_CapturePoint::CGGameObject_C_Type_CapturePoint(CGGameObject_C* owner) : CGGameObject_C_TypeAnimated(owner) {
}

// VERIFY: 19850204 auto
// OFFSET: 0x0070FF80
bool CGGameObject_C_Type_CapturePoint::CanHighlight() const {
    uint32_t highlightIndex = CGameObjectDef::GetPropNum(this->m_owner->m_gameObject->m_typeID, CGameObjectDef::PROP_HIGHLIGHT);
    GameObjectStats* highlightStats = this->m_owner->m_stats;
    uint32_t highlight = (highlightStats && highlightIndex < 24) ? highlightStats->m_propValue[highlightIndex] : 0;

    return highlight != 0;
}

// VERIFY: c0c21cab auto
// OFFSET: 0x00427A90
bool CGGameObject_C_Type_CapturePoint::CanUse() const {
    return false;
}

// VERIFY: 9740d5a5 auto
// OFFSET: 0x00710940
void CGGameObject_C_Type_CapturePoint::PostInitWithStats() {
    uint32_t a3Index = CGameObjectDef::GetPropNum(this->m_owner->m_gameObject->m_typeID, CGameObjectDef::PROP_RADIUS);
    GameObjectStats* a3Stats = this->m_owner->m_stats;
    uint32_t a3 = (a3Stats && a3Index < 24) ? a3Stats->m_propValue[a3Index] : 0;
    uint32_t a2Index = CGameObjectDef::GetPropNum(this->m_owner->m_gameObject->m_typeID, CGameObjectDef::PROP_WORLDSTATE1);
    GameObjectStats* a2Stats = this->m_owner->m_stats;
    uint32_t a2 = (a2Stats && a2Index < 24) ? a2Stats->m_propValue[a2Index] : 0;

    g_capturePointUIManager.AddCapturePoint(this->m_owner, a2, a3);
}

// VERIFY: f1c33a5b auto
// OFFSET: 0x00710940
void CGGameObject_C_Type_CapturePoint::Reenable() {
    uint32_t a3Index = CGameObjectDef::GetPropNum(this->m_owner->m_gameObject->m_typeID, CGameObjectDef::PROP_RADIUS);
    GameObjectStats* a3Stats = this->m_owner->m_stats;
    uint32_t a3 = (a3Stats && a3Index < 24) ? a3Stats->m_propValue[a3Index] : 0;
    uint32_t a2Index = CGameObjectDef::GetPropNum(this->m_owner->m_gameObject->m_typeID, CGameObjectDef::PROP_WORLDSTATE1);
    GameObjectStats* a2Stats = this->m_owner->m_stats;
    uint32_t a2 = (a2Stats && a2Index < 24) ? a2Stats->m_propValue[a2Index] : 0;

    g_capturePointUIManager.AddCapturePoint(this->m_owner, a2, a3);
}

// VERIFY: 9bb61b0e auto
// OFFSET: 0x0070CA30
void CGGameObject_C_Type_CapturePoint::Disable() {
    g_capturePointUIManager.RemoveCapturePoint(this->m_owner);
}
