#if defined(WOWDECOMP_PLATFORM_POSIX)

#include "fmod_settings.h"

#include "fmod.h"
#include "fmod_memory.h"
#include "fmod_output.h"
#include "fmod_output_sdl.h"
#include "fmod_pluginfactory.h"
#include "fmod_os_output.h"


/*
[
	[DESCRIPTION]
    Registers the platform outputs.  NoSound / WavWriter (and their NRT variants) are registered by the portable code.

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]
    Linux

	[SEE_ALSO]
]
*/
FMOD_RESULT FMOD_OS_Output_Register(FMOD::PluginFactory *pluginfactory)
{
#ifdef FMOD_USE_PLUGINS

    CHECK_RESULT(pluginfactory->tryLoadPlugin("output_sdl"));

#else

    #ifdef FMOD_SUPPORT_SDL
    pluginfactory->registerOutput(FMOD::OutputSDL::getDescriptionEx());
    #endif

#endif

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]
    The SDL output occupies the FMOD_OUTPUTTYPE_ALSA slot, the native Linux output type of FMOD Ex.

    [PLATFORMS]
    Linux

	[SEE_ALSO]
]
*/
FMOD_RESULT FMOD_OS_Output_GetDefault(FMOD_OUTPUTTYPE *outputtype)
{
    if (!outputtype)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

#ifdef FMOD_SUPPORT_SDL
    *outputtype = FMOD_OUTPUTTYPE_ALSA;
#else
    *outputtype = FMOD_OUTPUTTYPE_NOSOUND;
#endif

    return FMOD_OK;
}

#endif
