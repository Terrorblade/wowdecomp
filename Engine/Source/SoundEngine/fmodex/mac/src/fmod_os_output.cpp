#include "fmod_settings.h"
#include "fmod.h"
#include "fmod_memory.h"
#include "fmod_output.h"
#include "fmod_output_coreaudio.h"
#include "fmod_pluginfactory.h"

/*
    The outputs of the macOS build: CoreAudio, the default.
*/
FMOD_RESULT FMOD_OS_Output_Register(FMOD::PluginFactory *pluginfactory)
{
#ifdef FMOD_SUPPORT_COREAUDIO
    pluginfactory->registerOutput(FMOD::OutputCoreAudio::getDescriptionEx());
#endif

    return FMOD_OK;
}

FMOD_RESULT FMOD_OS_Output_GetDefault(FMOD_OUTPUTTYPE *outputtype)
{
    if (!outputtype)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *outputtype = FMOD_OUTPUTTYPE_COREAUDIO;

    return FMOD_OK;
}
