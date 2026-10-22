#if defined(WOWDECOMP_PLATFORM_POSIX)

#include "Engine/Source/Storm/H/SCritSect.h"

CCritSect::CCritSect() {
    InitializeCriticalSection(&this->m_critsect);
}

CCritSect::~CCritSect() {
    DeleteCriticalSection(&this->m_critsect);
}

void CCritSect::Enter() {
    EnterCriticalSection(&this->m_critsect);
}

void CCritSect::Enter(int32_t forwriting) {
    EnterCriticalSection(&this->m_critsect);
}

void CCritSect::Leave() {
    LeaveCriticalSection(&this->m_critsect);
}

void CCritSect::Leave(int32_t fromwriting) {
    LeaveCriticalSection(&this->m_critsect);
}

#endif
