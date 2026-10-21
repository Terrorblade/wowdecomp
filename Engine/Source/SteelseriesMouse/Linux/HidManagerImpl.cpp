#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cstdint>
#include "Engine/Source/SteelseriesMouse/Linux/HidManagerImpl.h"

HidManager::~HidManager() {
}

HidManager* HidManager::Create() {
    // Windows placement-news into SMemAlloc memory; WowMouse frees it with plain delete, so allocate with new here.
    return new HidManagerImpl();
}

HidManagerImpl::HidManagerImpl() : m_window(nullptr) {
}

HidManagerImpl::~HidManagerImpl() {
}

bool HidManagerImpl::EnumerateDevices() {
    return false;
}

void HidManagerImpl::SetWindow(void* window) {
    this->m_window = window;
}

uint32_t HidManagerImpl::GetDeviceCount() {
    return 0;
}

bool HidManagerImpl::WriteWait(const void* data, uint32_t size) {
    return false;
}

bool HidManagerImpl::Write(const void* data, uint32_t size) {
    return false;
}

bool HidManagerImpl::SetFeature(void* data, uint32_t size) {
    return false;
}

#endif
