#include "Container_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

// VERIFY: 5c3fa743 manual
// OFFSET: 0x00706A20
CGContainer_C::CGContainer_C(uint32_t time, const CClientObjCreate& create) : CGItem_C(time, create) {
    uint32_t numSlots = this->m_container->m_containerFieldNumSlots;
    uint32_t maxSlots = sizeof(this->m_container->m_containerFieldSlot1) / sizeof(this->m_container->m_containerFieldSlot1[0]);
    this->m_bag.m_numSlots = static_cast<int32_t>(numSlots > maxSlots ? maxSlots : numSlots);
    this->m_bag.m_slots = reinterpret_cast<WOWGUID*>(this->m_container->m_containerFieldSlot1);
    this->m_bag.m_guid = this->m_obj->m_guid;
    this->m_bag.m_hasBank = 0;
}

// VERIFY: e44f2652 manual
// OFFSET: [0x00706AE0, 0x00706B60]
CGContainer_C::~CGContainer_C() {
}

// VERIFY: 17f87b2b auto
// OFFSET: 0x00706AD0
const CGBag_C* CGContainer_C::GetBag() const {
    return &this->m_bag;
}

// VERIFY: 46d8ee2c auto
// OFFSET: 0x00706AD0
CGBag_C* CGContainer_C::GetBag() {
    return &this->m_bag;
}

// VERIFY: dfd9376a auto
// OFFSET: 0x007069E0
int32_t CGContainer_C::GetFreeSlots() const {
    if (!this->m_container) {
        return 0;
    }

    int32_t freeSlots = this->m_bag.m_numSlots;

    for (int32_t i = 0; i < this->m_bag.m_numSlots; i++) {
        if (this->m_container->m_containerFieldSlot1[i]) {
            freeSlots--;
        }
    }

    return freeSlots;
}

// VERIFY: a8862a2c auto
// OFFSET: 0x00706960
void CGContainer_C::SetStorage(uint32_t* storage, uint32_t* saved) {
    this->CGItem_C::SetStorage(storage, saved);
    this->m_container = reinterpret_cast<CGContainerData*>(reinterpret_cast<uint8_t*>(storage) + sizeof(CGObjectData) + sizeof(CGItemData));
    this->m_containerMirror = saved + CGItem::TotalFieldsSaved();
}

// VERIFY: 0f5f4e76 auto
// OFFSET: 0x005EEB70
void CGContainer_C::Initialize() {
}

// VERIFY: 03677fda manual
// OFFSET: 0x007069A0
void CGContainer_C::Reenable() {
    CGObject_C::Reenable();

    for (uint32_t i = 0; i < static_cast<uint32_t>(this->m_bag.m_numSlots); i++) {
        if (this->m_bag.m_slots[i]) {
            ClntObjMgrReenableObject(this->m_bag.m_slots[i]);
        }
    }
}
