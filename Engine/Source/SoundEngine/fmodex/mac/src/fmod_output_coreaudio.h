#ifndef _FMOD_OUTPUT_COREAUDIO_H
#define _FMOD_OUTPUT_COREAUDIO_H

#include "fmod_settings.h"

#ifdef FMOD_SUPPORT_COREAUDIO

#include "fmod_output_polled.h"

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>

namespace FMOD
{
    /*
        CoreAudio output, polled like the DirectSound and WinMM outputs: FMOD mixes blocks into a ring
        buffer behind a play cursor, and an output audio unit plays from the cursor, advancing it from
        its render callback. The audio unit takes FMOD's software format as is and converts to the
        device. Driver 0 is the system default output, followed by every output device.

        FMOD constructs outputs as OutputPolled in zeroed memory of the plugin's size, so members here
        need no construction and rely on starting at zero.
    */
    class OutputCoreAudio : public OutputPolled
    {
      private:

        AudioComponentInstance      mUnit;
        AudioDeviceID               mDevices[64];
        int                         mNumDevices;
        char                       *mBuffer;
        unsigned int                mBufferLengthBytes;
        unsigned int                mBytesPerFrame;
        unsigned long long          mFramesPlayed;      /* Frames played, read and written atomically. */
        bool                        mRunning;

        static OSStatus             renderCallback(void *userdata, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *timestamp, UInt32 bus, UInt32 frames, AudioBufferList *data);

      public:

        static FMOD_OUTPUT_DESCRIPTION_EX *getDescriptionEx();

        FMOD_RESULT                 enumerate();
        FMOD_RESULT                 getNumDrivers(int *numdrivers);
        FMOD_RESULT                 getDriverName(int driver, char *name, int namelen);
        FMOD_RESULT                 getDriverCaps(int id, FMOD_CAPS *caps);
        FMOD_RESULT                 init(int selecteddriver, FMOD_INITFLAGS flags, int *outputrate, int outputchannels, FMOD_SOUND_FORMAT *outputformat, int dspbufferlength, int dspnumbuffers, void *extradriverdata);
        FMOD_RESULT                 close();
        FMOD_RESULT                 start();
        FMOD_RESULT                 stop();
        FMOD_RESULT                 getPosition(unsigned int *pcm);
        FMOD_RESULT                 lock(unsigned int offset, unsigned int length, void **ptr1, void **ptr2, unsigned int *len1, unsigned int *len2);

        static FMOD_RESULT F_CALLBACK getNumDriversCallback(FMOD_OUTPUT_STATE *output, int *numdrivers);
        static FMOD_RESULT F_CALLBACK getDriverNameCallback(FMOD_OUTPUT_STATE *output, int id, char *name, int namelen);
        static FMOD_RESULT F_CALLBACK getDriverCapsCallback(FMOD_OUTPUT_STATE *output, int id, FMOD_CAPS *caps);
        static FMOD_RESULT F_CALLBACK initCallback         (FMOD_OUTPUT_STATE *output, int selecteddriver, FMOD_INITFLAGS flags, int *outputrate, int outputchannels, FMOD_SOUND_FORMAT *outputformat, int dspbufferlength, int dspnumbuffers, void *extradriverdata);
        static FMOD_RESULT F_CALLBACK closeCallback        (FMOD_OUTPUT_STATE *output);
        static FMOD_RESULT F_CALLBACK startCallback        (FMOD_OUTPUT_STATE *output);
        static FMOD_RESULT F_CALLBACK stopCallback         (FMOD_OUTPUT_STATE *output);
        static FMOD_RESULT F_CALLBACK getPositionCallback  (FMOD_OUTPUT_STATE *output, unsigned int *pcm);
        static FMOD_RESULT F_CALLBACK lockCallback         (FMOD_OUTPUT_STATE *output, unsigned int offset, unsigned int length, void **ptr1, void **ptr2, unsigned int *len1, unsigned int *len2);
    };
}

#endif  /* FMOD_SUPPORT_COREAUDIO */

#endif
