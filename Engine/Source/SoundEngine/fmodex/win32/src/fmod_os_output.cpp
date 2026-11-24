#include "fmod_settings.h"

#include <windows.h>
#include <VersionHelpers.h>

#include "fmod.h"
#include "fmod_memory.h"
#include "fmod_output.h"
#include "fmod_output_asio.h"
#include "fmod_output_wasapi.h"
#include "fmod_output_dsound.h"
#include "fmod_output_winmm.h"
#include "fmod_output_openal.h"
#include "fmod_pluginfactory.h"


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]
    Win32, Win64

	[SEE_ALSO]
]
*/
// VERIFY: f1ada7ef auto
// OFFSET: 0x00915FC0
FMOD_RESULT FMOD_OS_Output_Register(FMOD::PluginFactory *pluginfactory)
{
#ifdef FMOD_USE_PLUGINS

    CHECK_RESULT(pluginfactory->tryLoadPlugin("output_dsound"));
    CHECK_RESULT(pluginfactory->tryLoadPlugin("output_winmm"));
    CHECK_RESULT(pluginfactory->tryLoadPlugin("output_wasapi"));
    CHECK_RESULT(pluginfactory->tryLoadPlugin("output_asio"));

#else

    #ifdef FMOD_SUPPORT_DSOUND
    pluginfactory->registerOutput(FMOD::OutputDSound::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_WINMM
    pluginfactory->registerOutput(FMOD::OutputWinMM::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_WASAPI
    pluginfactory->registerOutput(FMOD::OutputWASAPI::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_ASIO
    pluginfactory->registerOutput(FMOD::OutputASIO::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_OPENAL
    pluginfactory->registerOutput(FMOD::OutputOpenAL::getDescriptionEx());
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

    [PLATFORMS]
    Win32, Win64

	[SEE_ALSO]
]
*/
// VERIFY: 94ba128a manual
// OFFSET: 0x00916030
FMOD_RESULT FMOD_OS_Output_GetDefault(FMOD_OUTPUTTYPE *outputtype)
{
    if (!outputtype)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *outputtype = IsWindowsVistaOrGreater() ? FMOD_OUTPUTTYPE_WASAPI : FMOD_OUTPUTTYPE_DSOUND;

    return FMOD_OK;
}
