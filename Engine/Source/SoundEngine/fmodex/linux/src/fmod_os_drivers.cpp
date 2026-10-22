#if defined(WOWDECOMP_PLATFORM_POSIX)

#include "fmod_settings.h"

#include "fmod.h"
#include "fmod_os_misc.h"

#include <SDL.h>

/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]
    Counts SDL's playback + capture devices, like waveOut/waveIn on Windows.  Only meaningful once the SDL output
    has initialized SDL audio.  This runs on every System::update, so it samples at most once a second.

	[SEE_ALSO]
*/
FMOD_RESULT FMOD_OS_CheckDriverList(bool *devicelistchanged)
{
    static int          lastNumDrivers  = -1;
    static unsigned int lastCheckTime   = 0;
    int                 numDrivers      = -1;
    unsigned int        now;

    *devicelistchanged = false;

    if (!SDL_WasInit(SDL_INIT_AUDIO))
    {
        return FMOD_OK;
    }

    FMOD_OS_Time_GetMs(&now);
    if (lastNumDrivers != -1 && now - lastCheckTime < 1000)
    {
        return FMOD_OK;
    }
    lastCheckTime = now;

    numDrivers  = SDL_GetNumAudioDevices(0);
    numDrivers += SDL_GetNumAudioDevices(1);

    *devicelistchanged = (numDrivers != lastNumDrivers && lastNumDrivers != -1);
    lastNumDrivers = numDrivers;

    return FMOD_OK;
}

#endif
