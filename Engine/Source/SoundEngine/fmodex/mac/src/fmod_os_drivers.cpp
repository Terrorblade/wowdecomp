#include "fmod_settings.h"

#include "fmod.h"
#include "fmod_os_misc.h"

#include <CoreAudio/CoreAudio.h>

/*
    Reports when audio devices come or go, as the Win32 layer counts wave devices.
*/
FMOD_RESULT FMOD_OS_CheckDriverList(bool *devicelistchanged)
{
    static int lastNumDrivers = -1;

    AudioObjectPropertyAddress address = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    UInt32 size = 0;
    int numDrivers = -1;

    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, NULL, &size) == noErr)
    {
        numDrivers = (int)(size / sizeof(AudioDeviceID));
    }

    *devicelistchanged = (numDrivers != lastNumDrivers && lastNumDrivers != -1);
    lastNumDrivers = numDrivers;

    return FMOD_OK;
}
