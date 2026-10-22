#ifndef ENGINE_SOURCE_STEELSERIESMOUSE_LINUX_HIDMANAGERIMPL_H
#define ENGINE_SOURCE_STEELSERIESMOUSE_LINUX_HIDMANAGERIMPL_H

#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cstdint>
#include "Engine/Source/SteelseriesMouse/HidManager.h"

// The Windows implementation talks to the SteelSeries WoW mouse through SetupAPI/HID. Linux has no such SDK here, so
// this manager never finds a device: the client behaves as on a Windows machine without the mouse attached.
class HidManagerImpl : public HidManager {
    public:
    void* m_window;

    HidManagerImpl();
    ~HidManagerImpl() override;
    bool EnumerateDevices() override;
    void SetWindow(void* window) override;
    uint32_t GetDeviceCount() override;
    bool WriteWait(const void* data, uint32_t size) override;
    bool Write(const void* data, uint32_t size) override;
    bool SetFeature(void* data, uint32_t size) override;
};

#endif

#endif
