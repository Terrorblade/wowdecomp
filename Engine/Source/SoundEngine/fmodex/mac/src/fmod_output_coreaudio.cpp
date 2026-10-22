#include "fmod_settings.h"

#ifdef FMOD_SUPPORT_COREAUDIO

#include "fmod_memory.h"
#include "fmod_output_coreaudio.h"
#include "fmod_soundi.h"
#include "fmod_string.h"
#include "fmod_systemi.h"

#include <string.h>

namespace FMOD
{

FMOD_OUTPUT_DESCRIPTION_EX coreaudiooutput;

FMOD_OUTPUT_DESCRIPTION_EX *OutputCoreAudio::getDescriptionEx()
{
    FMOD_memset(&coreaudiooutput, 0, sizeof(FMOD_OUTPUT_DESCRIPTION_EX));

    coreaudiooutput.name          = "FMOD CoreAudio Output";
    coreaudiooutput.version       = 0x00010100;
    coreaudiooutput.polling       = true;
    coreaudiooutput.getnumdrivers = &OutputCoreAudio::getNumDriversCallback;
    coreaudiooutput.getdrivername = &OutputCoreAudio::getDriverNameCallback;
    coreaudiooutput.getdrivercaps = &OutputCoreAudio::getDriverCapsCallback;
    coreaudiooutput.init          = &OutputCoreAudio::initCallback;
    coreaudiooutput.close         = &OutputCoreAudio::closeCallback;
    coreaudiooutput.start         = &OutputCoreAudio::startCallback;
    coreaudiooutput.stop          = &OutputCoreAudio::stopCallback;
    coreaudiooutput.getposition   = &OutputCoreAudio::getPositionCallback;
    coreaudiooutput.lock          = &OutputCoreAudio::lockCallback;

    /*
        Private members
    */
    coreaudiooutput.mType         = FMOD_OUTPUTTYPE_COREAUDIO;
    coreaudiooutput.mSize         = sizeof(OutputCoreAudio);

    return &coreaudiooutput;
}

static bool DeviceHasOutput(AudioDeviceID device)
{
    AudioObjectPropertyAddress address = { kAudioDevicePropertyStreams, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMain };
    UInt32 size = 0;

    return AudioObjectGetPropertyDataSize(device, &address, 0, NULL, &size) == noErr && size > 0;
}

FMOD_RESULT OutputCoreAudio::enumerate()
{
    if (mEnumerated)
    {
        return FMOD_OK;
    }

    mNumDevices = 0;

    AudioObjectPropertyAddress address = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    UInt32 size = 0;

    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, NULL, &size) == noErr && size)
    {
        AudioDeviceID devices[256];
        UInt32 count = size / sizeof(AudioDeviceID);

        if (count > 256)
        {
            count = 256;
        }

        size = count * sizeof(AudioDeviceID);

        if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, NULL, &size, devices) == noErr)
        {
            for (UInt32 i = 0; i < size / sizeof(AudioDeviceID) && mNumDevices < (int)(sizeof(mDevices) / sizeof(mDevices[0])); i++)
            {
                if (DeviceHasOutput(devices[i]))
                {
                    mDevices[mNumDevices++] = devices[i];
                }
            }
        }
    }

    mEnumerated = true;

    return FMOD_OK;
}

FMOD_RESULT OutputCoreAudio::getNumDrivers(int *numdrivers)
{
    FMOD_RESULT result = enumerate();

    if (result != FMOD_OK)
    {
        return result;
    }

    *numdrivers = mNumDevices + 1;

    return FMOD_OK;
}

FMOD_RESULT OutputCoreAudio::getDriverName(int driver, char *name, int namelen)
{
    FMOD_RESULT result = enumerate();

    if (result != FMOD_OK)
    {
        return result;
    }

    if (driver < 0 || driver > mNumDevices)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (driver == 0)
    {
        FMOD_strncpy(name, "System Default", namelen);
        return FMOD_OK;
    }

    AudioObjectPropertyAddress address = { kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    CFStringRef deviceName = NULL;
    UInt32 size = sizeof(deviceName);

    if (AudioObjectGetPropertyData(mDevices[driver - 1], &address, 0, NULL, &size, &deviceName) != noErr || !deviceName)
    {
        FMOD_strncpy(name, "Audio Device", namelen);
        return FMOD_OK;
    }

    if (!CFStringGetCString(deviceName, name, namelen, kCFStringEncodingUTF8))
    {
        FMOD_strncpy(name, "Audio Device", namelen);
    }

    CFRelease(deviceName);

    return FMOD_OK;
}

FMOD_RESULT OutputCoreAudio::getDriverCaps(int id, FMOD_CAPS *caps)
{
    *caps = (FMOD_CAPS)(*caps | FMOD_CAPS_OUTPUT_MULTICHANNEL);
    *caps = (FMOD_CAPS)(*caps | FMOD_CAPS_OUTPUT_FORMAT_PCM8);
    *caps = (FMOD_CAPS)(*caps | FMOD_CAPS_OUTPUT_FORMAT_PCM16);
    *caps = (FMOD_CAPS)(*caps | FMOD_CAPS_OUTPUT_FORMAT_PCM24);
    *caps = (FMOD_CAPS)(*caps | FMOD_CAPS_OUTPUT_FORMAT_PCM32);
    *caps = (FMOD_CAPS)(*caps | FMOD_CAPS_OUTPUT_FORMAT_PCMFLOAT);

    return FMOD_OK;
}

/*
    The stream format of FMOD's software mixer as CoreAudio linear PCM.
*/
static bool StreamFormat(int rate, int channels, FMOD_SOUND_FORMAT format, AudioStreamBasicDescription *description)
{
    memset(description, 0, sizeof(AudioStreamBasicDescription));
    description->mSampleRate = rate;
    description->mFormatID = kAudioFormatLinearPCM;
    description->mFramesPerPacket = 1;
    description->mChannelsPerFrame = channels;

    switch (format)
    {
        case FMOD_SOUND_FORMAT_PCM8:
            /* FMOD's 8-bit PCM is signed. */
            description->mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
            description->mBitsPerChannel = 8;
            break;
        case FMOD_SOUND_FORMAT_PCM16:
            description->mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
            description->mBitsPerChannel = 16;
            break;
        case FMOD_SOUND_FORMAT_PCM24:
            description->mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
            description->mBitsPerChannel = 24;
            break;
        case FMOD_SOUND_FORMAT_PCM32:
            description->mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
            description->mBitsPerChannel = 32;
            break;
        case FMOD_SOUND_FORMAT_PCMFLOAT:
            description->mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
            description->mBitsPerChannel = 32;
            break;
        default:
            return false;
    }

    description->mBytesPerFrame = description->mBitsPerChannel / 8 * channels;
    description->mBytesPerPacket = description->mBytesPerFrame;

    return true;
}

/*
    FMOD orders multichannel output like WAVE files: front left, front right, center, LFE, then back
    and side pairs.
*/
static AudioChannelLayoutTag ChannelLayout(int channels)
{
    switch (channels)
    {
        case 1:
            return kAudioChannelLayoutTag_Mono;
        case 2:
            return kAudioChannelLayoutTag_Stereo;
        case 4:
            return kAudioChannelLayoutTag_Quadraphonic;
        case 6:
            return kAudioChannelLayoutTag_WAVE_5_1_A;
        case 8:
            return kAudioChannelLayoutTag_WAVE_7_1;
        default:
            return kAudioChannelLayoutTag_DiscreteInOrder | channels;
    }
}

FMOD_RESULT OutputCoreAudio::init(int selecteddriver, FMOD_INITFLAGS flags, int *outputrate, int outputchannels, FMOD_SOUND_FORMAT *outputformat, int dspbufferlength, int dspnumbuffers, void *extradriverdata)
{
    Plugin::init(); /* Set up gGlobal - for debug / file / memory access by this plugin. */

    FLOG((FMOD_DEBUG_LEVEL_LOG, __FILE__, __LINE__, "OutputCoreAudio::init", "Initializing.\n"));

    FMOD_RESULT result = enumerate();

    if (result != FMOD_OK)
    {
        return result;
    }

    AudioStreamBasicDescription stream;

    if (!StreamFormat(*outputrate, outputchannels, *outputformat, &stream))
    {
        return FMOD_ERR_FORMAT;
    }

    mBytesPerFrame = stream.mBytesPerFrame;

    result = SoundI::getBytesFromSamples(dspbufferlength * dspnumbuffers, &mBufferLengthBytes, outputchannels, *outputformat);

    if (result != FMOD_OK)
    {
        return result;
    }

    mBuffer = (char *)FMOD_Memory_Calloc(mBufferLengthBytes);

    if (!mBuffer)
    {
        return FMOD_ERR_MEMORY;
    }

    __atomic_store_n(&mFramesPlayed, 0ULL, __ATOMIC_RELEASE);

    /*
        The default output unit follows the system's default device; a chosen device gets a HAL unit
        bound to it.
    */
    bool useDefault = selecteddriver <= 0 || selecteddriver > mNumDevices;

    AudioComponentDescription componentDescription;
    memset(&componentDescription, 0, sizeof(componentDescription));
    componentDescription.componentType = kAudioUnitType_Output;
    componentDescription.componentSubType = useDefault ? kAudioUnitSubType_DefaultOutput : kAudioUnitSubType_HALOutput;
    componentDescription.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent component = AudioComponentFindNext(NULL, &componentDescription);

    if (!component || AudioComponentInstanceNew(component, &mUnit) != noErr)
    {
        close();
        return FMOD_ERR_OUTPUT_INIT;
    }

    if (!useDefault)
    {
        AudioDeviceID device = mDevices[selecteddriver - 1];

        if (AudioUnitSetProperty(mUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &device, sizeof(device)) != noErr)
        {
            close();
            return FMOD_ERR_OUTPUT_INIT;
        }
    }

    if (AudioUnitSetProperty(mUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &stream, sizeof(stream)) != noErr)
    {
        close();
        return FMOD_ERR_OUTPUT_FORMAT;
    }

    if (outputchannels > 2)
    {
        AudioChannelLayout layout;
        memset(&layout, 0, sizeof(layout));
        layout.mChannelLayoutTag = ChannelLayout(outputchannels);
        AudioUnitSetProperty(mUnit, kAudioUnitProperty_AudioChannelLayout, kAudioUnitScope_Input, 0, &layout, sizeof(layout));
    }

    AURenderCallbackStruct callback;
    callback.inputProc = renderCallback;
    callback.inputProcRefCon = this;

    if (AudioUnitSetProperty(mUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &callback, sizeof(callback)) != noErr || AudioUnitInitialize(mUnit) != noErr)
    {
        close();
        return FMOD_ERR_OUTPUT_INIT;
    }

    FLOG((FMOD_DEBUG_LEVEL_LOG, __FILE__, __LINE__, "OutputCoreAudio::init", "Done.\n"));

    return FMOD_OK;
}

FMOD_RESULT OutputCoreAudio::close()
{
    if (mUnit)
    {
        AudioOutputUnitStop(mUnit);
        AudioUnitUninitialize(mUnit);
        AudioComponentInstanceDispose(mUnit);
        mUnit = NULL;
    }

    if (mBuffer)
    {
        FMOD_Memory_Free(mBuffer);
        mBuffer = NULL;
    }

    mEnumerated = false;

    return FMOD_OK;
}

FMOD_RESULT OutputCoreAudio::start()
{
    FMOD_RESULT result = OutputPolled::start();

    if (result != FMOD_OK)
    {
        return result;
    }

    if (!mUnit || AudioOutputUnitStart(mUnit) != noErr)
    {
        OutputPolled::stop();
        return FMOD_ERR_OUTPUT_DRIVERCALL;
    }

    mRunning = true;

    return FMOD_OK;
}

FMOD_RESULT OutputCoreAudio::stop()
{
    FMOD_RESULT result = OutputPolled::stop();

    if (result != FMOD_OK)
    {
        return result;
    }

    if (mUnit && mRunning)
    {
        AudioOutputUnitStop(mUnit);
    }

    mRunning = false;

    return FMOD_OK;
}

/*
    Plays frames from the ring buffer at the play cursor. The cursor counts every frame played, as
    WinMM reports its position; the polled mixer refills the blocks behind it.
*/
OSStatus OutputCoreAudio::renderCallback(void *userdata, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *timestamp, UInt32 bus, UInt32 frames, AudioBufferList *data)
{
    OutputCoreAudio *output = (OutputCoreAudio *)userdata;
    unsigned int ringFrames = output->mBytesPerFrame ? output->mBufferLengthBytes / output->mBytesPerFrame : 0;

    if (!output->mBuffer || !ringFrames || !data->mNumberBuffers)
    {
        for (UInt32 i = 0; i < data->mNumberBuffers; i++)
        {
            memset(data->mBuffers[i].mData, 0, data->mBuffers[i].mDataByteSize);
        }

        *flags |= kAudioUnitRenderAction_OutputIsSilence;
        return noErr;
    }

    unsigned long long played = __atomic_load_n(&output->mFramesPlayed, __ATOMIC_RELAXED);
    unsigned int position = (unsigned int)(played % ringFrames);
    char *out = (char *)data->mBuffers[0].mData;
    UInt32 remaining = frames;

    while (remaining)
    {
        UInt32 chunk = ringFrames - position < remaining ? ringFrames - position : remaining;
        memcpy(out, output->mBuffer + position * output->mBytesPerFrame, chunk * output->mBytesPerFrame);
        out += chunk * output->mBytesPerFrame;
        remaining -= chunk;
        position = 0;
    }

    __atomic_store_n(&output->mFramesPlayed, played + frames, __ATOMIC_RELEASE);

    return noErr;
}

FMOD_RESULT OutputCoreAudio::getPosition(unsigned int *pcm)
{
    *pcm = (unsigned int)__atomic_load_n(&mFramesPlayed, __ATOMIC_ACQUIRE);

    return FMOD_OK;
}

FMOD_RESULT OutputCoreAudio::lock(unsigned int offset, unsigned int length, void **ptr1, void **ptr2, unsigned int *len1, unsigned int *len2)
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
        *ptr2 = NULL;
        *len1 = length;
        *len2 = 0;
    }

    return FMOD_OK;
}

/*
    ==============================================================================================================

    CALLBACK INTERFACE

    ==============================================================================================================
*/

FMOD_RESULT F_CALLBACK OutputCoreAudio::getNumDriversCallback(FMOD_OUTPUT_STATE *output, int *numdrivers)
{
    return ((OutputCoreAudio *)output)->getNumDrivers(numdrivers);
}

FMOD_RESULT F_CALLBACK OutputCoreAudio::getDriverNameCallback(FMOD_OUTPUT_STATE *output, int id, char *name, int namelen)
{
    return ((OutputCoreAudio *)output)->getDriverName(id, name, namelen);
}

FMOD_RESULT F_CALLBACK OutputCoreAudio::getDriverCapsCallback(FMOD_OUTPUT_STATE *output, int id, FMOD_CAPS *caps)
{
    return ((OutputCoreAudio *)output)->getDriverCaps(id, caps);
}

FMOD_RESULT F_CALLBACK OutputCoreAudio::initCallback(FMOD_OUTPUT_STATE *output, int selecteddriver, FMOD_INITFLAGS flags, int *outputrate, int outputchannels, FMOD_SOUND_FORMAT *outputformat, int dspbufferlength, int dspnumbuffers, void *extradriverdata)
{
    return ((OutputCoreAudio *)output)->init(selecteddriver, flags, outputrate, outputchannels, outputformat, dspbufferlength, dspnumbuffers, extradriverdata);
}

FMOD_RESULT F_CALLBACK OutputCoreAudio::closeCallback(FMOD_OUTPUT_STATE *output)
{
    return ((OutputCoreAudio *)output)->close();
}

FMOD_RESULT F_CALLBACK OutputCoreAudio::startCallback(FMOD_OUTPUT_STATE *output)
{
    return ((OutputCoreAudio *)output)->start();
}

FMOD_RESULT F_CALLBACK OutputCoreAudio::stopCallback(FMOD_OUTPUT_STATE *output)
{
    return ((OutputCoreAudio *)output)->stop();
}

FMOD_RESULT F_CALLBACK OutputCoreAudio::getPositionCallback(FMOD_OUTPUT_STATE *output, unsigned int *pcm)
{
    return ((OutputCoreAudio *)output)->getPosition(pcm);
}

FMOD_RESULT F_CALLBACK OutputCoreAudio::lockCallback(FMOD_OUTPUT_STATE *output, unsigned int offset, unsigned int length, void **ptr1, void **ptr2, unsigned int *len1, unsigned int *len2)
{
    return ((OutputCoreAudio *)output)->lock(offset, length, ptr1, ptr2, len1, len2);
}

}

#endif  /* FMOD_SUPPORT_COREAUDIO */
