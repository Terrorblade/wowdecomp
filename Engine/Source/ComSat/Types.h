#ifndef ENGINE_SOURCE_COMSAT_TYPES_H
#define ENGINE_SOURCE_COMSAT_TYPES_H

#include <cstddef>
#include <cstdint>

enum MuteSetting {
    MUTE_SETTING_MUTE = 0,
    MUTE_SETTING_UNMUTE = 1,
    MUTE_SETTING_TOGGLE = 2,
};

enum HeadsetDeviceStatus {
    HEADSET_DEVICE_STATUS_OK = 0,
    HEADSET_DEVICE_STATUS_ERROR = 1,
};

#endif
