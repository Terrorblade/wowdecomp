#ifndef _FMOD_OUTPUT_WASAPI_H
#define _FMOD_OUTPUT_WASAPI_H

#include "fmod_settings.h"

#ifdef FMOD_SUPPORT_WASAPI

#include "fmod_systemi.h"
#include "fmod_output_timer.h"
#include "fmod_os_misc.h"

// 0x0600 is Vista (safe since WASAPI output mode is only available on Vista)
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#include <Windows.h>

#include "wasapi\fmod_audioclient.h"
#include "wasapi\fmod_mmdeviceapi.h"

namespace FMOD
{
	typedef struct FMOD_WASAPIDriver
	{
		LPWSTR   id;
		char    *name;
	} FMOD_WASAPIDriver;

    class OutputWASAPI : public OutputTimer
    {
      private:

        bool                      mCoInitialized;
        bool                      mInitialised;
        bool                      mExclusiveMode;

        int                       mNumRenderDrivers;
        FMOD_WASAPIDriver         mRenderDrivers[FMOD_OUTPUT_MAXDRIVERS];
        IAudioClient             *mRenderAudioClient;
        IAudioClock              *mRenderAudioClock;
        IAudioRenderClient       *mRenderClient;
        BYTE                     *mRenderBuffer;
        UINT32                    mRenderBufferLength;

        int                       mNumCaptureDrivers;
        FMOD_WASAPIDriver         mCaptureDrivers[FMOD_OUTPUT_MAXDRIVERS];
        IAudioClient             *mCaptureAudioClient;
        IAudioCaptureClient      *mCaptureClient;
        BYTE                     *mRecordBuffer;
        int                       mRecordBlockAlign;
        float                     mRecordTimerPeriod;
        int                       mRecordWritePosition;
        bool                      mRecordStereoToMono;
        FMOD_OS_CRITICALSECTION  *mRecordCrit;
        MMRESULT                  mRecordTimerID;

        FMOD_RESULT            enumerateDefaultDevices      (IMMDeviceEnumerator *pEnumerator);
        FMOD_RESULT            enumerate                    ();

      public:

        static FMOD_OUTPUT_DESCRIPTION_EX *getDescriptionEx();

        FMOD_RESULT            getNumDrivers                (int *numdrivers);
        FMOD_RESULT            getDriverName                (int id, char *name, int namelen);
        FMOD_RESULT            getDriverCapsEx              (int id, FMOD_CAPS *caps, int *minfrequency, int *maxfrequency, FMOD_SPEAKERMODE *controlpanelspeakermode);
        FMOD_RESULT            initEx                       (int selecteddriver, FMOD_INITFLAGS flags, int *outputrate, int outputchannels, FMOD_SOUND_FORMAT *outputformat, FMOD_SPEAKERMODE *speakermode, int dspbufferlength, int dspnumbuffers, int max2dchannels, int max3dchannels, void *extradriverdata);
	    FMOD_RESULT            close                        ();
        FMOD_RESULT            getHandle                    (void **handle);
        FMOD_RESULT            start                        ();
        FMOD_RESULT            stop                         ();
        FMOD_RESULT            getPosition                  (unsigned int *pcm);
        FMOD_RESULT            lock                         (unsigned int offset, unsigned int length, void **ptr1, void **ptr2, unsigned int *len1, unsigned int *len2);
        FMOD_RESULT            unlock                       (void *ptr1, void *ptr2, unsigned int len1, unsigned int len2);

#ifdef FMOD_SUPPORT_RECORDING
        FMOD_RESULT            recordUpdate                 ();
        FMOD_RESULT            recordGetNumDrivers          (int *numdrivers);
        FMOD_RESULT            recordGetDriverInfo          (int id, char *name, int namelen);
        FMOD_RESULT            recordGetDriverCaps          (int id, FMOD_CAPS *caps, int *minfrequency, int *maxfrequency);
        FMOD_RESULT            recordStart                  (int id, Sound *sound, bool loop);
        FMOD_RESULT            recordStop                   ();
        FMOD_RESULT            recordGetPosition            (unsigned int *pcm);
        FMOD_RESULT            recordLock                   (unsigned int offset, unsigned int length, void **ptr1, void **ptr2, unsigned int *len1, unsigned int *len2);
#endif

 static FMOD_RESULT F_CALLBACK getNumDriversCallback        (FMOD_OUTPUT_STATE *output, int *numdrivers);
 static FMOD_RESULT F_CALLBACK getDriverNameCallback        (FMOD_OUTPUT_STATE *output, int id, char *name, int namelen);
 static FMOD_RESULT F_CALLBACK getDriverCapsExCallback      (FMOD_OUTPUT_STATE *output, int id, FMOD_CAPS *caps, int *minfrequency, int *maxfrequency, FMOD_SPEAKERMODE *controlpanelspeakermode);
 static FMOD_RESULT F_CALLBACK initExCallback               (FMOD_OUTPUT_STATE *output, int selecteddriver, FMOD_INITFLAGS flags, int *outputrate, int outputchannels, FMOD_SOUND_FORMAT *outputformat, FMOD_SPEAKERMODE *speakermode, int dspbufferlength, int dspnumbuffers, int max2dchannels, int max3dchannels, void *extradriverdata);
 static FMOD_RESULT F_CALLBACK closeCallback                (FMOD_OUTPUT_STATE *output);
 static FMOD_RESULT F_CALLBACK startCallback                (FMOD_OUTPUT_STATE *output);
 static FMOD_RESULT F_CALLBACK stopCallback                 (FMOD_OUTPUT_STATE *output);
 static FMOD_RESULT F_CALLBACK getHandleCallback            (FMOD_OUTPUT_STATE *output, void **handle);
 static FMOD_RESULT F_CALLBACK getPositionCallback          (FMOD_OUTPUT_STATE *output, unsigned int *pcm);
 static FMOD_RESULT F_CALLBACK lockCallback                 (FMOD_OUTPUT_STATE *output, unsigned int offset, unsigned int length, void **ptr1, void **ptr2, unsigned int *len1, unsigned int *len2);
 static FMOD_RESULT F_CALLBACK unlockCallback               (FMOD_OUTPUT_STATE *output, void *ptr1, void *ptr2, unsigned int len1, unsigned int len2);

#ifdef FMOD_SUPPORT_RECORDING
 static void CALLBACK          recordTimerCallback          (UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);
 static FMOD_RESULT F_CALLBACK recordGetNumDriversCallback  (FMOD_OUTPUT_STATE *output, int *numdrivers);
 static FMOD_RESULT F_CALLBACK recordGetDriverInfoCallback  (FMOD_OUTPUT_STATE *output, int id, char *name, int namelen);
 static FMOD_RESULT F_CALLBACK recordGetDriverCapsCallback  (FMOD_OUTPUT_STATE *output, int id, FMOD_CAPS *caps, int *minfrequency, int *maxfrequency);
 static FMOD_RESULT F_CALLBACK recordStartCallback          (FMOD_OUTPUT_STATE *output, int id, FMOD_SOUND *sound, int loop);
 static FMOD_RESULT F_CALLBACK recordStopCallback           (FMOD_OUTPUT_STATE *output);
 static FMOD_RESULT F_CALLBACK recordGetPositionCallback    (FMOD_OUTPUT_STATE *output, unsigned int *pcm);
 static FMOD_RESULT F_CALLBACK recordLockCallback           (FMOD_OUTPUT_STATE *output, unsigned int offset, unsigned int length, void **ptr1, void **ptr2, unsigned int *len1, unsigned int *len2);
#endif
    };
}

#endif  /* FMOD_SUPPORT_WASAPI */

#endif  /* _FMOD_OUTPUT_WASAPI_H */
