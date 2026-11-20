#include "fmod_settings.h"

#include "fmod.h"
#include "fmod_os_misc.h"

#include <CoreAudio/CoreAudio.h>

static int gDeviceListChanged = 0;

static OSStatus FMOD_OS_DeviceListener(AudioObjectID object, UInt32 numaddresses, const AudioObjectPropertyAddress *addresses, void *userdata)
{
    __atomic_store_n(&gDeviceListChanged, 1, __ATOMIC_RELEASE);
    return noErr;
}

static void FMOD_OS_RegisterDeviceListeners()
{
    static bool registered = false;

    if (registered)
    {
        return;
    }
    registered = true;

    AudioObjectPropertyAddress devices = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    AudioObjectPropertyAddress defaultOutput = { kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };

    AudioObjectAddPropertyListener(kAudioObjectSystemObject, &devices, FMOD_OS_DeviceListener, NULL);
    AudioObjectAddPropertyListener(kAudioObjectSystemObject, &defaultOutput, FMOD_OS_DeviceListener, NULL);
}

FMOD_RESULT FMOD_OS_CheckDriverList(bool *devicelistchanged)
{
    FMOD_OS_RegisterDeviceListeners();

    *devicelistchanged = __atomic_exchange_n(&gDeviceListChanged, 0, __ATOMIC_ACQ_REL) != 0;

    return FMOD_OK;
}
