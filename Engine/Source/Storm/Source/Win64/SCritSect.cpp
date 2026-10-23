
#include "Engine/Source/Storm/H/SCritSect.h"

// INLINED
CCritSect::CCritSect() {
    InitializeCriticalSection(&this->m_critsect);
}

// INLINED
CCritSect::~CCritSect() {
    DeleteCriticalSection(&this->m_critsect);
}

// INLINED
void CCritSect::Enter() {
    EnterCriticalSection(&this->m_critsect);
}

// INLINED
void CCritSect::Enter(int32_t forwriting) {
    EnterCriticalSection(&this->m_critsect);
}

// INLINED
void CCritSect::Leave() {
    LeaveCriticalSection(&this->m_critsect);
}

// INLINED
void CCritSect::Leave(int32_t fromwriting) {
    LeaveCriticalSection(&this->m_critsect);
}

