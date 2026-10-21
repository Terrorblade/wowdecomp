#if defined(WOWDECOMP_PLATFORM_LINUX)

#include "fmod_settings.h"

#ifdef FMOD_SUPPORT_SDL

#include "fmod.h"
#include "fmod_debug.h"
#include "fmod_output_sdl.h"
#include "fmod_memory.h"
#include "fmod_soundi.h"
#include "fmod_string.h"
#include "fmod_systemi.h"

#include <SDL.h>

namespace FMOD
{

FMOD_OUTPUT_DESCRIPTION_EX sdloutput;

static const char *SDL_DEFAULT_DRIVER_NAME = "Default Audio Device";


#ifdef PLUGIN_EXPORTS

#ifdef __cplusplus
extern "C" {
#endif

    /*
        FMODGetOutputDescription is mandantory for every fmod plugin.  This is the symbol the registerplugin function searches for.
        Must be declared with F_API to make it export as stdcall.
    */
    F_DECLSPEC F_DLLEXPORT FMOD_OUTPUT_DESCRIPTION_EX * F_API FMODGetOutputDescriptionEx()
    {
        return OutputSDL::getDescriptionEx();
    }

#ifdef __cplusplus
}
#endif

#endif  /* PLUGIN_EXPORTS */


/*
	[DESCRIPTION]
    Brings up the SDL audio subsystem once for the process.  SDL refcounts subsystems, and several FMOD systems (game,
    voice chat, device enumeration) share it, so it is left running until SDL_Quit.

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
static bool OutputSDL_InitAudio()
{
    static bool initialized = false;

    if (!initialized)
    {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        {
            FLOG((FMOD_DEBUG_LEVEL_ERROR, __FILE__, __LINE__, "OutputSDL_InitAudio", "SDL_InitSubSystem(SDL_INIT_AUDIO) failed: %s\n", SDL_GetError()));
            return false;
        }

        initialized = true;
    }

    return true;
}


/*
	[DESCRIPTION]
    SDL device name for an FMOD driver index, 0 (default device) gives NULL.

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
static const char *OutputSDL_GetDeviceName(int id)
{
    if (id <= 0)
    {
        return 0;
    }

    return SDL_GetAudioDeviceName(id - 1, 0);
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]
    Linux

	[SEE_ALSO]
]
*/
FMOD_OUTPUT_DESCRIPTION_EX *OutputSDL::getDescriptionEx()
{
    FMOD_memset(&sdloutput, 0, sizeof(FMOD_OUTPUT_DESCRIPTION_EX));

    sdloutput.name                    = "FMOD SDL Output";
    sdloutput.version                 = 0x00010100;
    sdloutput.polling                 = true;
    sdloutput.getnumdrivers           = &OutputSDL::getNumDriversCallback;
    sdloutput.getdrivername           = &OutputSDL::getDriverNameCallback;
    sdloutput.getdrivercapsex         = &OutputSDL::getDriverCapsExCallback;
    sdloutput.init                    = &OutputSDL::initCallback;
    sdloutput.close                   = &OutputSDL::closeCallback;
    sdloutput.start                   = &OutputSDL::startCallback;
    sdloutput.stop                    = &OutputSDL::stopCallback;
    sdloutput.gethandle               = &OutputSDL::getHandleCallback;
    sdloutput.getposition             = &OutputSDL::getPositionCallback;
    sdloutput.lock                    = &OutputSDL::lockCallback;

    /*
        No record callbacks: System::getRecordNumDrivers reports 0 input devices.
    */

    /*
        Private members
    */
    sdloutput.mType                   = FMOD_OUTPUTTYPE_ALSA;    /* The Linux output slot; see FMOD_OS_Output_GetDefault. */
    sdloutput.mSize                   = sizeof(OutputSDL);

    return &sdloutput;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]
    FMOD_OK

	[REMARKS]
    Driver 0 is the system default device, followed by every playback device SDL enumerates.

    [PLATFORMS]
    Linux

	[SEE_ALSO]
]
*/
FMOD_RESULT OutputSDL::getNumDrivers(int *numdrivers)
{
    int count;

    if (!OutputSDL_InitAudio())
    {
        *numdrivers = 0;
        return FMOD_OK;
    }

    count = SDL_GetNumAudioDevices(0);
    if (count < 0)
    {
        count = 0;
    }

    *numdrivers = count + 1;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]
    FMOD_OK

	[REMARKS]

    [PLATFORMS]
    Linux

	[SEE_ALSO]
]
*/
FMOD_RESULT OutputSDL::getDriverName(int id, char *name, int namelen)
{
    const char *devicename;

    if (!OutputSDL_InitAudio())
    {
        return FMOD_ERR_OUTPUT_INIT;
    }

    devicename = OutputSDL_GetDeviceName(id);

    FMOD_strncpy(name, devicename ? devicename : SDL_DEFAULT_DRIVER_NAME, namelen);
    if (namelen > 0)
    {
        name[namelen - 1] = 0;
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]
    FMOD_OK

	[REMARKS]
    SDL converts any format/channel count to what the device takes, so every format FMOD can mix to is accepted
    except PCM24 (no SDL format; init mixes PCM32 instead).  The "control panel" speaker mode comes from the
    device's preferred channel count.

    [PLATFORMS]
    Linux

	[SEE_ALSO]
]
*/
FMOD_RESULT OutputSDL::getDriverCapsEx(int id, FMOD_CAPS *caps, int *minfrequency, int *maxfrequency, FMOD_SPEAKERMODE *controlpanelspeakermode)
{
    SDL_AudioSpec spec;
    int           rc;

    if (!OutputSDL_InitAudio())
    {
        return FMOD_ERR_OUTPUT_INIT;
    }

    *caps = (FMOD_CAPS)(*caps | FMOD_CAPS_OUTPUT_MULTICHANNEL);
    *caps = (FMOD_CAPS)(*caps | FMOD_CAPS_OUTPUT_FORMAT_PCM8);
    *caps = (FMOD_CAPS)(*caps | FMOD_CAPS_OUTPUT_FORMAT_PCM16);
    *caps = (FMOD_CAPS)(*caps | FMOD_CAPS_OUTPUT_FORMAT_PCM32);
    *caps = (FMOD_CAPS)(*caps | FMOD_CAPS_OUTPUT_FORMAT_PCMFLOAT);

    *minfrequency = 8000;
    *maxfrequency = 192000;

    SDL_zero(spec);
    if (id <= 0)
    {
        char *defaultname = 0;

        rc = SDL_GetDefaultAudioInfo(&defaultname, &spec, 0);
        if (defaultname)
        {
            SDL_free(defaultname);
        }
    }
    else
    {
        rc = SDL_GetAudioDeviceSpec(id - 1, 0, &spec);
    }

    if (rc == 0)
    {
        switch (spec.channels)
        {
            case 1:  *controlpanelspeakermode = FMOD_SPEAKERMODE_MONO;     break;
            case 4:  *controlpanelspeakermode = FMOD_SPEAKERMODE_QUAD;     break;
            case 5:  *controlpanelspeakermode = FMOD_SPEAKERMODE_SURROUND; break;
            case 6:  *controlpanelspeakermode = FMOD_SPEAKERMODE_5POINT1;  break;
            case 8:  *controlpanelspeakermode = FMOD_SPEAKERMODE_7POINT1;  break;
            default: *controlpanelspeakermode = FMOD_SPEAKERMODE_STEREO;   break;
        }
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]
    FMOD_OK

	[REMARKS]
    Opens the device with the exact rate/format/channels FMOD mixes at (SDL converts if the hardware differs) and
    allocates the looping ring buffer, like the single looping WinMM block.

    [PLATFORMS]
    Linux

	[SEE_ALSO]
]
*/
FMOD_RESULT OutputSDL::init(int selecteddriver, FMOD_INITFLAGS flags, int *outputrate, int outputchannels, FMOD_SOUND_FORMAT *outputformat, int dspbufferlength, int dspnumbuffers, void *extradriverdata)
{
    SDL_AudioSpec desired, obtained;
    unsigned int  bufferlength;
    int           numbuffers;
    FMOD_RESULT   result;

    Plugin::init(); /* Set up gGlobal - for debug / file / memory access by this plugin. */

	FLOG((FMOD_DEBUG_LEVEL_ALL, __FILE__, __LINE__, "OutputSDL::init", "Initializing.\n"));

    if (!OutputSDL_InitAudio())
    {
        return FMOD_ERR_OUTPUT_INIT;
    }

    SDL_zero(desired);

    switch (*outputformat)
    {
        case FMOD_SOUND_FORMAT_PCM8:
        {
            desired.format = AUDIO_S8;
            break;
        }
        case FMOD_SOUND_FORMAT_PCM16:
        {
            desired.format = AUDIO_S16SYS;
            break;
        }
        case FMOD_SOUND_FORMAT_PCM24:
        case FMOD_SOUND_FORMAT_PCM32:
        {
            *outputformat  = FMOD_SOUND_FORMAT_PCM32;
            desired.format = AUDIO_S32SYS;
            break;
        }
        case FMOD_SOUND_FORMAT_PCMFLOAT:
        {
            desired.format = AUDIO_F32SYS;
            break;
        }
        default:
        {
            return FMOD_ERR_FORMAT;
        }
    }

    result = mSystem->getDSPBufferSize(&bufferlength, &numbuffers);
    if (result != FMOD_OK)
    {
        return result;
    }

    desired.freq     = *outputrate;
    desired.channels = (Uint8)outputchannels;
    desired.samples  = (Uint16)(bufferlength > 0 && bufferlength <= 8192 ? bufferlength : 1024);
    desired.callback = &OutputSDL::audioCallback;
    desired.userdata = this;

    SoundI::getBytesFromSamples(1, &mFrameBytes, outputchannels, *outputformat);
    SoundI::getBytesFromSamples(bufferlength * numbuffers, &mBufferLengthBytes, outputchannels, *outputformat);

    mBuffer = (char *)FMOD_Memory_Calloc(mBufferLengthBytes);
    if (!mBuffer)
    {
        return FMOD_ERR_MEMORY;
    }

    __atomic_store_n(&mReadOffset, 0u, __ATOMIC_RELEASE);

    mDevice = SDL_OpenAudioDevice(OutputSDL_GetDeviceName(selecteddriver), 0, &desired, &obtained, 0);
    if (!mDevice)
    {
	    FLOG((FMOD_DEBUG_LEVEL_ERROR, __FILE__, __LINE__, "OutputSDL::init", "SDL_OpenAudioDevice failed: %s\n", SDL_GetError()));

        FMOD_Memory_Free(mBuffer);
        mBuffer = 0;

        return FMOD_ERR_OUTPUT_INIT;
    }

    FLOG((FMOD_DEBUG_LEVEL_ALL, __FILE__, __LINE__, "OutputSDL::init", "Opened SDL audio device %d (%d Hz, %d channels, %d sample buffer)\n", mDevice, obtained.freq, obtained.channels, obtained.samples));

    FLOG((FMOD_DEBUG_LEVEL_ALL, __FILE__, __LINE__, "OutputSDL::init", "Done.\n"));

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]
    FMOD_OK

	[REMARKS]

    [PLATFORMS]
    Linux

	[SEE_ALSO]
]
*/
FMOD_RESULT OutputSDL::close()
{
    if (mDevice)
    {
        SDL_CloseAudioDevice(mDevice);    /* Waits for a running callback to return. */
        mDevice = 0;
    }

    if (mBuffer)
    {
        FMOD_Memory_Free(mBuffer);
    }
	mBuffer = 0;
    mBufferLengthBytes = 0;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]
    FMOD_OK

	[REMARKS]

    [PLATFORMS]
    Linux

	[SEE_ALSO]
]
*/
FMOD_RESULT OutputSDL::getHandle(void **handle)
{
    *handle = (void *)(uintptr_t)mDevice;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]
    FMOD_OK

	[REMARKS]

    [PLATFORMS]
    Linux

	[SEE_ALSO]
]
*/
FMOD_RESULT OutputSDL::start()
{
    FMOD_RESULT result;

    result = OutputPolled::start();
    if (result != FMOD_OK)
    {
        return result;
    }

    if (!mDevice)
    {
        return FMOD_ERR_OUTPUT_DRIVERCALL;
    }

    SDL_PauseAudioDevice(mDevice, 0);

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]
    FMOD_OK

	[REMARKS]

    [PLATFORMS]
    Linux

	[SEE_ALSO]
]
*/
FMOD_RESULT OutputSDL::stop()
{
    FMOD_RESULT result;

    result = OutputPolled::stop();
    if (result != FMOD_OK)
    {
        return result;
    }

    if (mDevice)
    {
        SDL_PauseAudioDevice(mDevice, 1);
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]
    FMOD_OK

	[REMARKS]
    Play cursor in samples within the ring buffer (like waveOutGetPosition on the looping WinMM block).

    [PLATFORMS]
    Linux

	[SEE_ALSO]
]
*/
FMOD_RESULT OutputSDL::getPosition(unsigned int *pcm)
{
    if (!mFrameBytes)
    {
        *pcm = 0;
        return FMOD_OK;
    }

    *pcm = __atomic_load_n(&mReadOffset, __ATOMIC_ACQUIRE) / mFrameBytes;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]
    FMOD_OK

	[REMARKS]

    [PLATFORMS]
    Linux

	[SEE_ALSO]
]
*/
FMOD_RESULT OutputSDL::lock(unsigned int offset, unsigned int length, void **ptr1, void **ptr2, unsigned int *len1, unsigned int *len2)
{
	offset %= mBufferLengthBytes;

	if (offset + length > mBufferLengthBytes)
	{
		*ptr1 = (char *)mBuffer + offset;
		*ptr2 = (char *)mBuffer;
		*len1 = mBufferLengthBytes - offset;
		*len2 = length - (mBufferLengthBytes - offset);
	}
	else
	{
		*ptr1 = (char *)mBuffer + offset;
		*ptr2 = 0;
		*len1 = length;
		*len2 = 0;
	}

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]
    SDL audio thread callback: plays the ring buffer from the cursor and advances it.

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]
    The FMOD mixer only fills blocks behind the cursor, so the region read here is never being written.

    [PLATFORMS]
    Linux

	[SEE_ALSO]
]
*/
void OutputSDL::audioCallback(void *userdata, unsigned char *stream, int len)
{
    OutputSDL   *sdl = (OutputSDL *)userdata;
    unsigned int offset;
    unsigned int remaining = (unsigned int)len;

    if (!sdl->mBuffer || !sdl->mBufferLengthBytes)
    {
        FMOD_memset(stream, 0, len);
        return;
    }

    offset = __atomic_load_n(&sdl->mReadOffset, __ATOMIC_ACQUIRE);

    while (remaining)
    {
        unsigned int chunk = sdl->mBufferLengthBytes - offset;

        if (chunk > remaining)
        {
            chunk = remaining;
        }

        FMOD_memcpy(stream, sdl->mBuffer + offset, chunk);

        stream    += chunk;
        remaining -= chunk;
        offset    += chunk;

        if (offset >= sdl->mBufferLengthBytes)
        {
            offset = 0;
        }
    }

    __atomic_store_n(&sdl->mReadOffset, offset, __ATOMIC_RELEASE);
}


/*
    ==============================================================================================================

    CALLBACK INTERFACE

    ==============================================================================================================
*/

FMOD_RESULT F_CALLBACK OutputSDL::getNumDriversCallback(FMOD_OUTPUT_STATE *output, int *numdrivers)
{
    OutputSDL *sdl = (OutputSDL *)output;

    return sdl->getNumDrivers(numdrivers);
}

FMOD_RESULT F_CALLBACK OutputSDL::getDriverNameCallback(FMOD_OUTPUT_STATE *output, int id, char *name, int namelen)
{
    OutputSDL *sdl = (OutputSDL *)output;

    return sdl->getDriverName(id, name, namelen);
}

FMOD_RESULT F_CALLBACK OutputSDL::getDriverCapsExCallback(FMOD_OUTPUT_STATE *output, int id, FMOD_CAPS *caps, int *minfrequency, int *maxfrequency, FMOD_SPEAKERMODE *controlpanelspeakermode)
{
    OutputSDL *sdl = (OutputSDL *)output;

    return sdl->getDriverCapsEx(id, caps, minfrequency, maxfrequency, controlpanelspeakermode);
}

FMOD_RESULT F_CALLBACK OutputSDL::initCallback(FMOD_OUTPUT_STATE *output, int selecteddriver, FMOD_INITFLAGS flags, int *outputrate, int outputchannels, FMOD_SOUND_FORMAT *outputformat, int dspbufferlength, int dspnumbuffers, void *extradriverdata)
{
    OutputSDL *sdl = (OutputSDL *)output;

    return sdl->init(selecteddriver, flags, outputrate, outputchannels, outputformat, dspbufferlength, dspnumbuffers, extradriverdata);
}

FMOD_RESULT F_CALLBACK OutputSDL::closeCallback(FMOD_OUTPUT_STATE *output)
{
    OutputSDL *sdl = (OutputSDL *)output;

    return sdl->close();
}

FMOD_RESULT F_CALLBACK OutputSDL::startCallback(FMOD_OUTPUT_STATE *output)
{
    OutputSDL *sdl = (OutputSDL *)output;

    return sdl->start();
}

FMOD_RESULT F_CALLBACK OutputSDL::stopCallback(FMOD_OUTPUT_STATE *output)
{
    OutputSDL *sdl = (OutputSDL *)output;

    return sdl->stop();
}

FMOD_RESULT F_CALLBACK OutputSDL::getHandleCallback(FMOD_OUTPUT_STATE *output, void **handle)
{
    OutputSDL *sdl = (OutputSDL *)output;

    return sdl->getHandle(handle);
}

FMOD_RESULT F_CALLBACK OutputSDL::getPositionCallback(FMOD_OUTPUT_STATE *output, unsigned int *pcm)
{
    OutputSDL *sdl = (OutputSDL *)output;

    return sdl->getPosition(pcm);
}

FMOD_RESULT F_CALLBACK OutputSDL::lockCallback(FMOD_OUTPUT_STATE *output, unsigned int offset, unsigned int length, void **ptr1, void **ptr2, unsigned int *len1, unsigned int *len2)
{
    OutputSDL *sdl = (OutputSDL *)output;

    return sdl->lock(offset, length, ptr1, ptr2, len1, len2);
}

}

#endif  /* #ifdef FMOD_SUPPORT_SDL */

#endif
