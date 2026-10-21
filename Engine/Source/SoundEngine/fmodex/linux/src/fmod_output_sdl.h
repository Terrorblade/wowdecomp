#ifndef _FMOD_OUTPUT_SDL_H
#define _FMOD_OUTPUT_SDL_H

#if defined(WOWDECOMP_PLATFORM_LINUX)

#include "fmod_settings.h"

#ifdef FMOD_SUPPORT_SDL

#include "fmod_output_polled.h"

namespace FMOD
{
    /*
        SDL2 audio output.  Polled like the WinMM output: FMOD's mixer thread fills a looping ring buffer of
        dspbufferlength * dspnumbuffers samples, and the SDL audio callback plays it, advancing the play cursor that
        getPosition reports.  Driver 0 is the system default device, drivers 1..n are SDL's playback devices.

        Instances are calloc'd by PluginFactory::createOutput as an OutputPolled of mSize bytes, so no constructor
        runs: members must be valid when zeroed.
    */
    class OutputSDL : public OutputPolled
    {
      private:

        unsigned int        mDevice;                /* SDL_AudioDeviceID */
        char               *mBuffer;
        unsigned int        mBufferLengthBytes;
        unsigned int        mFrameBytes;
        unsigned int        mReadOffset;            /* byte offset of the play cursor in mBuffer, shared with the SDL audio thread */

        static void         audioCallback(void *userdata, unsigned char *stream, int len);

      public:

        static FMOD_OUTPUT_DESCRIPTION_EX *getDescriptionEx();

        FMOD_RESULT              getNumDrivers(int *numdrivers);
        FMOD_RESULT              getDriverName(int id, char *name, int namelen);
        FMOD_RESULT              getDriverCapsEx(int id, FMOD_CAPS *caps, int *minfrequency, int *maxfrequency, FMOD_SPEAKERMODE *controlpanelspeakermode);
	    FMOD_RESULT              init(int selecteddriver, FMOD_INITFLAGS flags, int *outputrate, int outputchannels, FMOD_SOUND_FORMAT *outputformat, int dspbufferlength, int dspnumbuffers, void *extradriverdata);
	    FMOD_RESULT              close();
        FMOD_RESULT              getHandle(void **handle);
        FMOD_RESULT              start();
        FMOD_RESULT              stop();
	    FMOD_RESULT              getPosition(unsigned int *pcm);
	    FMOD_RESULT              lock(unsigned int offset, unsigned int length, void **ptr1, void **ptr2, unsigned int *len1, unsigned int *len2);

        static FMOD_RESULT F_CALLBACK getNumDriversCallback         (FMOD_OUTPUT_STATE *output, int *numdrivers);
        static FMOD_RESULT F_CALLBACK getDriverNameCallback         (FMOD_OUTPUT_STATE *output, int id, char *name, int namelen);
        static FMOD_RESULT F_CALLBACK getDriverCapsExCallback       (FMOD_OUTPUT_STATE *output, int id, FMOD_CAPS *caps, int *minfrequency, int *maxfrequency, FMOD_SPEAKERMODE *controlpanelspeakermode);
        static FMOD_RESULT F_CALLBACK initCallback                  (FMOD_OUTPUT_STATE *output, int selecteddriver, FMOD_INITFLAGS flags, int *outputrate, int outputchannels, FMOD_SOUND_FORMAT *outputformat, int dspbufferlength, int dspnumbuffers, void *extradriverdata);
        static FMOD_RESULT F_CALLBACK closeCallback                 (FMOD_OUTPUT_STATE *output);
        static FMOD_RESULT F_CALLBACK startCallback                 (FMOD_OUTPUT_STATE *output);
        static FMOD_RESULT F_CALLBACK stopCallback                  (FMOD_OUTPUT_STATE *output);
        static FMOD_RESULT F_CALLBACK getHandleCallback             (FMOD_OUTPUT_STATE *output, void **handle);
        static FMOD_RESULT F_CALLBACK getPositionCallback           (FMOD_OUTPUT_STATE *output, unsigned int *pcm);
        static FMOD_RESULT F_CALLBACK lockCallback                  (FMOD_OUTPUT_STATE *output, unsigned int offset, unsigned int length, void **ptr1, void **ptr2, unsigned int *len1, unsigned int *len2);
    };
}

#endif  /* #ifdef FMOD_SUPPORT_SDL */

#endif

#endif
