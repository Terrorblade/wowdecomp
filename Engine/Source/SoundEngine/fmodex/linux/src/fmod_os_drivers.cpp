#if defined(WOWDECOMP_PLATFORM_POSIX)

#include "fmod_settings.h"

#include "fmod.h"
#include "fmod_os_misc.h"

#include <SDL.h>

static int gDeviceListChanged = 0;

static int SDLCALL FMOD_OS_DeviceEventWatch(void *userdata, SDL_Event *event)
{
    if (event->type == SDL_AUDIODEVICEADDED || event->type == SDL_AUDIODEVICEREMOVED)
    {
        __atomic_store_n(&gDeviceListChanged, 1, __ATOMIC_RELEASE);
    }

    return 1;
}

FMOD_RESULT FMOD_OS_CheckDriverList(bool *devicelistchanged)
{
    static bool registered = false;

    *devicelistchanged = false;

    if (!SDL_WasInit(SDL_INIT_AUDIO))
    {
        return FMOD_OK;
    }

    if (!registered)
    {
        SDL_AddEventWatch(FMOD_OS_DeviceEventWatch, NULL);
        registered = true;
        return FMOD_OK;
    }

    *devicelistchanged = __atomic_exchange_n(&gDeviceListChanged, 0, __ATOMIC_ACQ_REL) != 0;

    return FMOD_OK;
}

#endif
