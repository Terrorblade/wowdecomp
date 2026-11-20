#include "fmod_settings.h"

#include "fmod.h"
#include "fmod_3d.h"
#ifdef FMOD_SUPPORT_NONBLOCKING
#include "fmod_async.h"
#endif
#include "fmod_autocleanup.h"
#include "fmod_channeli.h"
#include "fmod_channel_real.h"
#include "fmod_channel_stream.h"
#include "fmod_channel_software.h"
#include "fmod_codec_user.h"
#include "fmod_codeci.h"
#include "fmod_debug.h"
#include "fmod_dsp_chorus.h"
#ifdef FMOD_SUPPORT_DSPCODEC
#include "fmod_dsp_codec.h"
#endif

#ifndef FMOD_STATICFORPLUGINS
#include "fmod_dsp_distortion.h"
#include "fmod_dsp_echo.h"
#include "fmod_dsp_fft.h"
#include "fmod_dsp_filter.h"
#include "fmod_dsp_flange.h"
#include "fmod_dsp_highpass.h"
#include "fmod_dsp_itecho.h"
#include "fmod_dsp_compressor.h"
#include "fmod_dsp_sfxreverb.h"
#include "fmod_dsp_lowpass.h"
#include "fmod_dsp_lowpass2.h"
#include "fmod_dsp_lowpass_simple.h"
#include "fmod_dsp_normalize.h"
#include "fmod_dsp_oscillator.h"
#include "fmod_dsp_parameq.h"
#include "fmod_dsp_pitchshift.h"
#include "fmod_dsp_reverb.h"
#endif

#include "fmod_dsp_resampler.h"
#ifndef PLATFORM_PS3
#include "fmod_dsp_soundcard.h"
#endif
#ifdef FMOD_SUPPORT_VSTPLUGIN
#include "fmod_dsp_vstplugin.h"
#endif
#include "fmod_dsp_wavetable.h"
#include "fmod_dspi.h"
#include "fmod_file.h"
#include "fmod_file_cdda.h"
#include "fmod_file_disk.h"
#include "fmod_file_memory.h"
#include "fmod_file_net.h"
#include "fmod_file_null.h"
#include "fmod_file_user.h"
#include "fmod_localcriticalsection.h"
#include "fmod_memory.h"
#include "fmod_metadata.h"
#include "fmod_net.h"
#include "fmod_output_emulated.h"
#include "fmod_output_polled.h"
#include "fmod_output_software.h"
#include "fmod_os_cdda.h"
#include "fmod_os_output.h"
#include "fmod_pluginfactory.h"
#include "fmod_sample_software.h"
#include "fmod_soundi.h"
#include "fmod_sound_sample.h"

#ifdef FMOD_SUPPORT_GEOMETRY
#include "fmod_geometryi.h"
#endif
#ifdef FMOD_SUPPORT_STREAMING
#include "fmod_sound_stream.h"
#endif
#include "fmod_string.h"
#include "fmod_stringw.h"
#include "fmod_systemi.h"
#ifdef FMOD_SUPPORT_OPENAL
#include "fmod_output_openal.h"
#endif
#include "fmod_downmix.h"

/*
    Codecs
*/
#ifdef FMOD_SUPPORT_AAC
#include "fmod_codec_aac.h"
#endif
#ifdef FMOD_SUPPORT_AIFF
#include "fmod_codec_aiff.h"
#endif
#ifdef FMOD_SUPPORT_ASF
#include "fmod_codec_asf.h"
#endif
#ifdef FMOD_SUPPORT_AT3
#include "fmod_codec_at3.h"
#endif
#ifdef FMOD_SUPPORT_CDDA
#include "fmod_codec_cdda.h"
#endif
#ifdef FMOD_SUPPORT_DLS
#include "fmod_codec_dls.h"
#endif
#ifdef FMOD_SUPPORT_FLAC
#include "fmod_codec_flac.h"
#endif
#ifdef FMOD_SUPPORT_GCADPCM_DSP
#include "fmod_codec_dsp.h"
#endif
#ifdef FMOD_SUPPORT_FSB
#include "fmod_codec_fsb.h"
#endif
#ifdef FMOD_SUPPORT_IT
#include "fmod_codec_it.h"
#endif
#ifdef FMOD_SUPPORT_MOD
#include "fmod_codec_midi.h"
#endif
#ifdef FMOD_SUPPORT_MOD
#include "fmod_codec_mod.h"
#endif
#ifdef FMOD_SUPPORT_MPEG
#include "fmod_codec_mpeg.h"
#endif
#ifdef FMOD_SUPPORT_MPEGPSP
#include "fmod_codec_mpegpsp.h"
#endif
#ifdef FMOD_SUPPORT_OGGVORBIS
#include "fmod_codec_oggvorbis.h"
#endif
#ifdef FMOD_SUPPORT_PLAYLIST
#include "fmod_codec_playlist.h"
#endif
#ifdef FMOD_SUPPORT_RAW
#include "fmod_codec_raw.h"
#endif
#ifdef FMOD_SUPPORT_SF2
#include "fmod_codec_sf2.h"
#endif
#ifdef FMOD_SUPPORT_S3M
#include "fmod_codec_s3m.h"
#endif
#ifdef FMOD_SUPPORT_TAGS
#include "fmod_codec_tag.h"
#endif
#ifdef FMOD_SUPPORT_TREMOR
#include "fmod_codec_tremor.h"
#endif
#ifdef FMOD_SUPPORT_VAG
    #if defined(PLATFORM_PS2) || defined(PLATFORM_PSP)
    #include "fmod_codec_vag.h"
    #else
    #include "fmod_codec_swvag.h"
    #endif
#endif
#ifdef FMOD_SUPPORT_WAV
#include "fmod_codec_wav.h"
#endif
#ifdef FMOD_SUPPORT_XM
#include "fmod_codec_xm.h"
#endif
#ifdef FMOD_SUPPORT_XMA
#include "fmod_codec_xma.h"
#endif
#ifdef FMOD_SUPPORT_XWMA
#include "fmod_codec_xwma.h"
#endif

#ifdef FMOD_SUPPORT_IMAADPCM
#include "fmod_codec_wav_imaadpcm.h"
#endif

/*
    Output types
*/
#ifdef FMOD_SUPPORT_NOSOUND
#include "fmod_output_nosound.h"
#endif
#ifdef FMOD_SUPPORT_NOSOUND_NRT
#include "fmod_output_nosound_nrt.h"
#endif
#ifdef FMOD_SUPPORT_WAVWRITER
#include "fmod_output_wavwriter.h"
#endif
#ifdef FMOD_SUPPORT_WAVWRITER_NRT
#include "fmod_output_wavwriter_nrt.h"
#endif

/*
    Profiler modules
*/

#include "fmod_speakermap.h"    /* Must be after mmreg include from codec wav */

namespace FMOD
{

FMOD_OS_CRITICALSECTION *SystemI::gSoundListCrit      = 0;

/*
[
	[DESCRIPTION]
    Returns a pointer to the instance of system object with the specified id

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: f3f43be4 auto
// OFFSET: 0x008D2800
FMOD_RESULT SystemI::getInstance(unsigned int id, SystemI **sys)
{
    SystemI *current;

    if (sys)
    {
        *sys = 0;
    }

    current = SAFE_CAST(SystemI, gGlobal->gSystemHead->getNext());
    while (current != gGlobal->gSystemHead)
    {
        if (current->mIndex == id)
        {
            if (sys)
            {
                *sys = current;
            }
            return FMOD_OK;
        }

        current = SAFE_CAST(SystemI, current->getNext());
    }

    return FMOD_ERR_INVALID_PARAM;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: b22da821 auto
// OFFSET: 0x008D2850
FMOD_RESULT SystemI::validate(System *system, SystemI **systemi)
{
    FMOD::SystemI *sys = (FMOD::SystemI *)system;

    if (!system)
    {
        return FMOD_ERR_INVALID_HANDLE;
    }

    if (!systemi)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (!FMOD::gGlobal->gSystemHead->exists(sys))
    {
        return FMOD_ERR_INVALID_HANDLE;
    }

    *systemi = sys;

    return FMOD_OK;
}


#ifdef FMOD_SUPPORT_STREAMING

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 033e4f7a auto
// OFFSET: 0x008D5200
void SystemI::streamThread(void *data)
{
    SystemI *system = (SystemI *)data;

    system->updateStreams();
}


/*
    ====================================================================================

    PRIVATE MEMBER FUNCTIONS

    ====================================================================================
*/


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 32885d29 auto
// OFFSET: 0x008D2890
FMOD_RESULT SystemI::updateStreams()
{
    mStreamTimeStamp.stampIn();

    FMOD_OS_CriticalSection_Enter(mStreamListCrit);
    {
        mStreamListChannelCurrent = mStreamListChannelHead.getNext();
        while (mStreamListChannelCurrent != &mStreamListChannelHead)
        {
            ChannelStream *channelstream = (ChannelStream *)mStreamListChannelCurrent->getData();

            mStreamListChannelNext = mStreamListChannelCurrent->getNext();

            FMOD_OS_CriticalSection_Leave(mStreamListCrit);

            FMOD_OS_CriticalSection_Enter(mStreamUpdateCrit);
            if (!channelstream->mFinished)
            {
                channelstream->updateStream();
            }
            FMOD_OS_CriticalSection_Leave(mStreamUpdateCrit);

            FMOD_OS_CriticalSection_Enter(mStreamListCrit);
            mStreamListChannelCurrent = mStreamListChannelNext;
        }
        mStreamListChannelNext = 0;
    }
    FMOD_OS_CriticalSection_Leave(mStreamListCrit);

    FMOD_OS_CriticalSection_Enter(mStreamListCrit);
    {
        LinkedListNode *current = mStreamListSoundHead.getNext();
        while (current != &mStreamListSoundHead)
        {
            Stream *stream = (Stream *)current->getData();

            if (stream->mChannel && stream->mChannel->mFinished)
            {
                stream->mFlags |= FMOD_SOUND_FLAG_FINISHED;
            }

            current = current->getNext();
        }
    }
    FMOD_OS_CriticalSection_Leave(mStreamListCrit);

    mStreamTimeStamp.stampOut(95);

    return FMOD_OK;
}

#endif


/*
[
	[DESCRIPTION]
    Updates virtual voices, and any per voice calculations, ie emulated voice calcs, 3d calcs or geometry.

	[PARAMETERS]
    'delta' Delta in milliseconds from last frame.
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 95e4b2f5 manual
// OFFSET: 0x008D29C0
FMOD_RESULT SystemI::updateChannels(int delta)
{
    int       emulatedused;
    ChannelI *current;

    current = SAFE_CAST(ChannelI, mChannelUsedListHead.getNext());
    while (current != &mChannelUsedListHead)
    {
        ChannelI    *next = SAFE_CAST(ChannelI, current->getNext());
        bool         playing = false;

        current->isPlaying(&playing);
        if (playing)
        {
            FMOD_MODE mode;

            current->update(delta);

            current->getMode(&mode);
            if (!(mode & FMOD_VIRTUAL_PLAYFROMSTART))
            {
                current->mFlags &= ~CHANNELI_FLAG_JUSTWENTVIRTUAL;
            }
        }

        current->isPlaying(&playing);
        if (!playing)
        {
            current->stopEx(true, true, true, true, true, true, false);
        }

        current = next;
    }

    if (mEmulated)
    {
        mEmulated->mChannelPool->getChannelsUsed(&emulatedused);

        if (emulatedused)
        {
            LinkedListNode *emucurrent, *realcurrent[CHANNELREAL_TYPE_MAX];
            int             count;

            emucurrent = mChannelSortedListHead.getNext();

            for (count = 0; count < CHANNELREAL_TYPE_MAX; count++)
            {
                realcurrent[count] = &mChannelSortedListHead;
            }

            do
            {
                FMOD_RESULT     result = FMOD_OK;
                ChannelI       *emuchannel = 0;
                ChannelReal    *emuchannelreal [FMOD_CHANNEL_MAXREALSUBCHANNELS] = { 0 };
                ChannelI       *realchannel    [FMOD_CHANNEL_MAXREALSUBCHANNELS] = { 0 };
                ChannelReal    *realchannelreal[FMOD_CHANNEL_MAXREALSUBCHANNELS] = { 0 };
                FMOD_MODE       mode;
                int             realchannelsneeded, realchannelsfound, channelsfound;
                bool            isvirtual;

                isvirtual = false;
                do
                {
                    emuchannel = (ChannelI *)emucurrent->getData();
                    if (!emuchannel)
                    {
                        break;
                    }

                    result = emuchannel->isVirtual(&isvirtual);
                    if (isvirtual && !(emuchannel->mFlags & CHANNELI_FLAG_FORCEVIRTUAL))
                    {
                        break;
                    }

                    emucurrent = emucurrent->getNext();

                } while (emucurrent != &mChannelSortedListHead);

                if (emucurrent == &mChannelSortedListHead ||
                    emucurrent == realcurrent[0] ||
                    emucurrent == realcurrent[1] ||
                    emucurrent == realcurrent[2] ||
                    result != FMOD_OK)
                {
                    break;
                }

                channelsfound = 0;
                realchannelsfound = 0;

                result = emuchannel->getRealChannel(emuchannelreal, 0);
                if (result != FMOD_OK)
                {
                    break;
                }

                mode = emuchannelreal[0]->mMode;

                if (emuchannelreal[0]->mSound)
                {
    #ifdef FMOD_SUPPORT_STREAMING
                    if (emuchannelreal[0]->mSound->isStream())
                    {
                        Stream *stream = SAFE_CAST(Stream, emuchannelreal[0]->mSound);
                        Sample *sample = stream->mSample;

                        realchannelsneeded = sample->mNumSubSamples;
                    }
                    else
    #endif
                    {
                        Sample *sample = SAFE_CAST(Sample, emuchannelreal[0]->mSound);

                        realchannelsneeded = sample->mNumSubSamples;
                    }

                    if (realchannelsneeded < 1)
                    {
                        realchannelsneeded = 1;
                    }
                }
                else if (emuchannelreal[0]->mDSP)
                {
                    realchannelsneeded = 1;
                }
                else
                {
                    return FMOD_ERR_INTERNAL;
                }

                if (realchannelsfound < realchannelsneeded)
                {
                    int found = 0;
                    ChannelReal *tempchannelreal[FMOD_CHANNEL_MAXREALSUBCHANNELS] = { 0 };

                    if (mode & FMOD_HARDWARE)
                    {
                        if (emuchannelreal[0]->mSound)
                        {
                            result = mOutput->getFreeChannel(mode, tempchannelreal, realchannelsneeded, emuchannelreal[0]->mSound->mChannels, &found);
                        }
                        else
                        {
                            return FMOD_ERR_INTERNAL;
                        }
                    }
                    else
                    {
    #ifdef FMOD_SUPPORT_SOFTWARE
                        if (mode & FMOD_CREATECOMPRESSEDSAMPLE)
                        {
                            Sample *sample = SAFE_CAST(Sample, emuchannelreal[0]->mSound);

                            if (0)
                            {
                            }
    #ifdef FMOD_SUPPORT_DSPCODEC
                            #ifdef FMOD_SUPPORT_MPEG
                            else if (sample->mFormat == FMOD_SOUND_FORMAT_MPEG)
                            {
                                result = mDSPCodecPool_MPEG.areAnyFree();
                            }
                            #endif
                            #ifdef FMOD_SUPPORT_IMAADPCM
                            else if (sample->mFormat == FMOD_SOUND_FORMAT_IMAADPCM)
                            {
                                result = mDSPCodecPool_ADPCM.areAnyFree();
                            }
                            #endif
    #endif
                            else
                            {
                                result = FMOD_ERR_FORMAT;
                            }
                        }
                        else
                        {
                            result = FMOD_OK;
                        }

                        if (result == FMOD_OK)
                        {
                            if (emuchannelreal[0]->mSound)
                            {
                                result = mSoftware->getFreeChannel(mode, tempchannelreal, realchannelsneeded, emuchannelreal[0]->mSound->mChannels, &found);
                            }
                            else if (emuchannelreal[0]->mDSP)
                            {
                                result = mSoftware->getFreeChannel(mode, tempchannelreal, realchannelsneeded, 1, &found);
                            }
                            else
                            {
                                return FMOD_ERR_INTERNAL;
                            }
                        }
    #else
                        result = FMOD_ERR_NEEDSSOFTWARE;
    #endif
                    }

                    for (count = 0; count < found; count++)
                    {
                        realchannelreal[realchannelsfound] = tempchannelreal[count];
                        realchannelsfound ++;
                    }
                }

    tryformore:
                if (realchannelsfound < realchannelsneeded)
                {
                    CHANNELREAL_TYPE realtype;

                    if (mOutput->mChannelPool == mOutput->mChannelPool3D)
                    {
                        realtype = mode & FMOD_SOFTWARE ? CHANNELREAL_TYPE_SW : CHANNELREAL_TYPE_HW;
                    }
                    else
                    {
                        realtype = mode & FMOD_SOFTWARE ? CHANNELREAL_TYPE_SW : mode & FMOD_3D ? CHANNELREAL_TYPE_HW3D : CHANNELREAL_TYPE_HW2D;
                    }

                    if (realcurrent[realtype] == &mChannelSortedListHead)
                    {
                        realcurrent[realtype] = realcurrent[realtype]->getPrev();
                    }

                    do
                    {
                        if (realcurrent[realtype] == emucurrent)
                        {
                            result = FMOD_ERR_CHANNEL_ALLOC;
                            break;
                        }

                        realchannel[channelsfound] = (ChannelI *)realcurrent[realtype]->getData();
                        if (realcurrent[realtype] == &mChannelSortedListHead)
                        {
                            result = FMOD_ERR_CHANNEL_ALLOC;
                            break;
                        }

                        realchannel[channelsfound]->isVirtual(&isvirtual);
                        if (!isvirtual)
                        {
                            FMOD_MODE        currentmode;
                            CHANNELREAL_TYPE currentrealtype;

                            realchannel[channelsfound]->getMode(&currentmode);

                            if (mOutput->mChannelPool == mOutput->mChannelPool3D)
                            {
                                currentrealtype = currentmode & FMOD_SOFTWARE ? CHANNELREAL_TYPE_SW : CHANNELREAL_TYPE_HW;
                            }
                            else
                            {
                                currentrealtype = currentmode & FMOD_SOFTWARE ? CHANNELREAL_TYPE_SW : currentmode & FMOD_3D ? CHANNELREAL_TYPE_HW3D : CHANNELREAL_TYPE_HW2D;
                            }

                            if (realtype == currentrealtype)
                            {
                                if (realtype == CHANNELREAL_TYPE_SW && mode & FMOD_CREATECOMPRESSEDSAMPLE)
                                {
                                    Sample *sample = SAFE_CAST(Sample, emuchannelreal[0]->mSound);

                                    if (0)
                                    {
                                    }
    #ifdef FMOD_SUPPORT_DSPCODEC
                                    #ifdef FMOD_SUPPORT_MPEG
                                    else if (sample->mFormat == FMOD_SOUND_FORMAT_MPEG)
                                    {
                                        result = mDSPCodecPool_MPEG.areAnyFree();
                                    }
                                    #endif
                                    #ifdef FMOD_SUPPORT_IMAADPCM
                                    else if (sample->mFormat == FMOD_SOUND_FORMAT_IMAADPCM)
                                    {
                                        result = mDSPCodecPool_ADPCM.areAnyFree();
                                    }
                                    #endif
    #endif
                                    else
                                    {
                                        result = FMOD_ERR_FORMAT;
                                    }

                                    if (result == FMOD_OK)
                                    {
                                        break;
                                    }

                                    if (currentmode & FMOD_CREATECOMPRESSEDSAMPLE)
                                    {
                                        ChannelReal *tempcr[FMOD_CHANNEL_MAXREALSUBCHANNELS] = { 0 };

                                        realchannel[channelsfound]->getRealChannel(tempcr, 0);

                                        Sample *currentsample = SAFE_CAST(Sample, tempcr[0]->mSound);

                                        if (currentsample->mFormat == sample->mFormat)
                                        {
                                            result = FMOD_OK;
                                            break;
                                        }
                                    }
                                }
                                else
                                {
                                    result = FMOD_OK;
                                    break;
                                }
                            }
                        }

                        realcurrent[realtype] = realcurrent[realtype]->getPrev();

                    } while (1);

                    if (result == FMOD_OK)
                    {
                        int found = 0;
                        ChannelReal *tempchannelreal[FMOD_CHANNEL_MAXREALSUBCHANNELS] = { 0 };

                        realchannel[channelsfound]->getRealChannel(tempchannelreal, &found);

                        for (count = 0; count < found; count++)
                        {
                            realchannelreal[realchannelsfound] = tempchannelreal[count];
                            realchannelsfound ++;
                        }

                        channelsfound++;

                        if (realchannelsfound < realchannelsneeded)
                        {
                            realcurrent[realtype] = realcurrent[realtype]->getPrev();
                            goto tryformore;
                        }
                    }
                    else
                    {
                        realchannel[channelsfound] = 0;
                    }
                }

                if (realchannelsfound >= realchannelsneeded)
                {
                    FMOD_CHANNEL_INFO emuinfo;
                    FMOD_CHANNEL_INFO realinfo[FMOD_CHANNEL_MAXREALSUBCHANNELS];
                    FMOD_MODE         mode;
                    int               emulatedtoswap = channelsfound;

                    emuchannel->getMode(&mode);
                    if (mode & FMOD_VIRTUAL_PLAYFROMSTART)
                    {
                        current->mFlags &= ~CHANNELI_FLAG_JUSTWENTVIRTUAL;
                    }

                    if (channelsfound)
                    {
                        for (count = 0; count < channelsfound; count++)
                        {
                            realchannel[count]->getChannelInfo(&realinfo[count]);
                            realchannel[count]->stopEx(false, false, false, true, false, false, false);
                        }

                        for (count = realchannelsneeded; count < realchannelsfound; count++)
                        {
                            realchannelreal[count]->mFlags &= ~(CHANNELREAL_FLAG_IN_USE | CHANNELREAL_FLAG_ALLOCATED | CHANNELREAL_FLAG_PLAYING | CHANNELREAL_FLAG_PAUSED | CHANNELREAL_FLAG_PAUSEDFORSETPOS | CHANNELREAL_FLAG_ENDDELAY);
                            realchannelreal[count]->mFlags |=  CHANNELREAL_FLAG_STOPPED;
                        }
                    }
                    else
                    {
                        emuchannel->getChannelInfo(&emuinfo);
                        emuchannel->stopEx(false, false, false, true, false, false, true);

        #ifdef FMOD_SUPPORT_STREAMING
                        if (emuinfo.mSound && emuinfo.mSound->mMode & FMOD_CREATESTREAM)
                        {
                            Stream *stream = SAFE_CAST(Stream, emuinfo.mSound);
                            ChannelStream *channelstream = stream->mChannel;

                            emuchannel->mRealChannel[0] = channelstream;

                            channelstream->mNumRealChannels = realchannelsneeded;
                            for (count = 0; count < realchannelsneeded; count++)
                            {
                                channelstream->mRealChannel[count] = realchannelreal[count];
                                channelstream->mRealChannel[count]->mSubChannelIndex = count;
                            }
                        }
                        else
        #endif
                        {
                            emuchannel->mNumRealChannels = realchannelsneeded;
                            for (count = 0; count < realchannelsneeded; count++)
                            {
                                emuchannel->mRealChannel[count] = realchannelreal[count];
                                emuchannel->mRealChannel[count]->mSubChannelIndex = count;
                            }
                        }

                        if (emuinfo.mDSP)
                        {
                            emuchannel->play(emuinfo.mDSP, true, false, false);
                        }
                        else
                        {
                            emuchannel->play(emuinfo.mSound, true, false, false);
                        }

                        emuchannel->setChannelInfo(&emuinfo);
                        emuchannel->setChannelGroup(emuchannel->mChannelGroup);
                        emuchannel->setPaused(emuinfo.mPaused);
                    }

                    for (count = 0; count < emulatedtoswap; count++)
                    {
                        mEmulated->getFreeChannel(mode, emuchannelreal, 1, 1, 0);

                        realchannel[count]->mNumRealChannels = 1;
                        realchannel[count]->mRealChannel[0] = emuchannelreal[0];

                        if (realinfo[count].mSound)
                        {
                            realchannel[count]->play(realinfo[count].mSound->mSubSampleParent, true, false, false);
                            realchannel[count]->setChannelInfo(&realinfo[count]);
                            realchannel[count]->setChannelGroup(realchannel[count]->mChannelGroup);
                            realchannel[count]->setPaused(realinfo[count].mPaused);
                        }
                        else if (realinfo[count].mDSP)
                        {
                            realchannel[count]->play(realinfo[count].mDSP, true, false, false);
                            realchannel[count]->setChannelInfo(&realinfo[count]);
                            realchannel[count]->setChannelGroup(realchannel[count]->mChannelGroup);
                            realchannel[count]->setPaused(realinfo[count].mPaused);
                        }

                        emucurrent = emucurrent->getNext();
                    }
                }
            } while (1);
        }
    }

    return FMOD_OK;
}

#ifndef FMOD_STATICFORPLUGINS
/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: e5360052 manual
// OFFSET: 0x008D3230
FMOD_RESULT SystemI::updateSoundGroups(int delta)
{
    SoundGroupI *soundgroup;
    bool         mute = false;

    soundgroup = (SoundGroupI *)mSoundGroupUsedHead.getNext();
    while (soundgroup != &mSoundGroupUsedHead)
    {
        SoundGroupI *next = (SoundGroupI *)soundgroup->getNext();

        soundgroup->mPlayCount = 0;
        soundgroup->getNumPlaying(&soundgroup->mNumPlaying);

        if (soundgroup->mNumPlaying)
        {
            if (soundgroup->mMaxAudibleBehavior == FMOD_SOUNDGROUP_BEHAVIOR_MUTE)
            {
                mute = true;
            }
        }
        else
        {
            soundgroup->removeNode();
            soundgroup->addAfter(&mSoundGroupFreeHead);
        }

        soundgroup = next;
    }

    if (mute)
    {
        LinkedListNode *current = mChannelSortedListHead.getNext();

        while (current != &mChannelSortedListHead)
        {
            ChannelI       *channel = (ChannelI *)current->getData();
            LinkedListNode *next = current->getNext();

            if (channel->mRealChannel[0] && channel->mRealChannel[0]->mSound)
            {
                SoundGroupI *soundgroup = channel->mRealChannel[0]->mSound->mSoundGroup;

                if (soundgroup)
                {
                    if (soundgroup->mMaxAudibleBehavior == FMOD_SOUNDGROUP_BEHAVIOR_MUTE && soundgroup->mMaxAudible >= 0)
                    {
                        soundgroup->mPlayCount++;
                        if (soundgroup->mPlayCount > soundgroup->mMaxAudible)
                        {
                            channel->mFadeTarget = 0.0f;
                        }
                        else
                        {
                            channel->mFadeTarget = 1.0f;
                        }
                    }

                    if (channel->mFadeVolume != channel->mFadeTarget)
                    {
                        if (soundgroup->mFadeSpeed < 0.001)
                        {
                            channel->mFadeVolume = channel->mFadeTarget;
                        }
                        else
                        {
                            if (channel->mFadeVolume < channel->mFadeTarget)
                            {
                                channel->mFadeVolume += ((float)delta / (soundgroup->mFadeSpeed * 1000.0f));
                                if (channel->mFadeVolume > channel->mFadeTarget)
                                {
                                    channel->mFadeVolume = channel->mFadeTarget;
                                }
                            }
                            if (channel->mFadeVolume > channel->mFadeTarget)
                            {
                                channel->mFadeVolume -= ((float)delta / (soundgroup->mFadeSpeed * 1000.0f));
                                if (channel->mFadeVolume < channel->mFadeTarget)
                                {
                                    channel->mFadeVolume = channel->mFadeTarget;
                                }
                            }
                        }

                        channel->setVolume(channel->mVolume, false);
                    }
                }
            }

            current = next;
        }
    }

    return FMOD_OK;
}
#endif


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: e7822623 manual
// OFFSET: 0x008D5210
FMOD_RESULT SystemI::findChannel(FMOD_CHANNELINDEX id, SoundI *sound, ChannelI **channel)
{
    ChannelI       *chan = 0;
    int             count, subchannels, found;
    ChannelReal    *realchannel[FMOD_CHANNEL_MAXREALSUBCHANNELS] = { 0 };
    FMOD_RESULT     result;
    FMOD_MODE       mode;

    if (!channel)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    result = sound->getMode(&mode);
    if (result != FMOD_OK)
    {
        return result;
    }

    if (sound->mNumSubSounds && !(mode & FMOD_CREATESTREAM))
    {
        return FMOD_ERR_SUBSOUNDS;
    }

    if (mode & FMOD_UNIQUE)
	{
        result = stopSound(sound);
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    if (id == FMOD_CHANNEL_REUSE)
    {
        chan = *channel;
        if (!chan)
        {
            id = FMOD_CHANNEL_FREE;
        }
        else
        {
            if (!chan->mRealChannel[0] || chan->mRealChannel[0]->mFlags & CHANNELREAL_FLAG_STOPPED)
            {
                id = (FMOD_CHANNELINDEX)chan->mIndex;
            }
            else
            {
                chan->stopEx(false, false, false, true, true, true, false);
            }
        }
    }

    if (id != FMOD_CHANNEL_REUSE)
    {
        *channel = 0;

        if (id != FMOD_CHANNEL_FREE)
        {
            chan = &mChannel[id];
            chan->stop();
        }
        else
        {
            if (!mChannelFreeListHead.isEmpty())
            {
                chan = SAFE_CAST(ChannelI, mChannelFreeListHead.getNext());
            }
            else
            {
                LinkedListNode *node;

                if (mChannelSortedListHead.isEmpty())
                {
                    return FMOD_ERR_CHANNEL_ALLOC;
                }

                node = mChannelSortedListHead.getPrev();
                chan = (ChannelI *)node->getData();

                chan->stopEx(true, false, true, true, true, true, false);
            }
        }

        chan->removeNode();
        chan->addBefore(&mChannelUsedListHead);
    }

#ifdef FMOD_SUPPORT_STREAMING
    if (mode & FMOD_CREATESTREAM)
    {
        Stream *stream = SAFE_CAST(Stream, sound);
        Sample *sample = SAFE_CAST(Sample, stream->mSample);

        if (sample)
        {
            subchannels = sample->mNumSubSamples;
        }
        else
        {
            subchannels = 1;
        }
    }
    else
#endif
    {
        Sample *sample = SAFE_CAST(Sample, sound);

        subchannels = sample->mNumSubSamples;
    }

    if (!subchannels)
    {
        subchannels = 1;
    }

    if ((mode & FMOD_SOFTWARE || mOutputType == FMOD_OUTPUTTYPE_OPENAL) && mode & FMOD_CREATECOMPRESSEDSAMPLE)
    {
        if (0)
        {
        }
#ifdef FMOD_SUPPORT_DSPCODEC
        #ifdef FMOD_SUPPORT_MPEG
        else if (sound->mFormat == FMOD_SOUND_FORMAT_MPEG)
        {
            result = mDSPCodecPool_MPEG.areAnyFree();
        }
        #endif
        #ifdef FMOD_SUPPORT_IMAADPCM
        else if (sound->mFormat == FMOD_SOUND_FORMAT_IMAADPCM)
        {
            result = mDSPCodecPool_ADPCM.areAnyFree();
        }
        #endif
#endif
        else
        {
            result = FMOD_ERR_FORMAT;
        }
    }

    if (mode & FMOD_SOFTWARE || !mOutput->mDescription.createsample)
	{
        if (result == FMOD_OK)
        {
            result = mSoftware->getFreeChannel(mode, realchannel, subchannels, sound->mChannels, &found);
        }
    }
    else
    {
		result = mOutput->getFreeChannel(mode, realchannel, subchannels, sound->mChannels, &found);
    }

    if (result != FMOD_OK || found != subchannels)
    {
        subchannels = 1;

        result = mEmulated->getFreeChannel(mode, realchannel, subchannels, sound->mChannels, 0);
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    if (realchannel[0]->mOutput == mEmulated)
    {
        chan->mFlags |= CHANNELI_FLAG_JUSTWENTVIRTUAL;
    }

#ifdef FMOD_SUPPORT_STREAMING
    if (mode & FMOD_CREATESTREAM)
    {
        if (realchannel[0]->mOutput == mEmulated)
        {
            chan->mNumRealChannels = 1;
            chan->mRealChannel[0] = realchannel[0];
            chan->mRealChannel[0]->mSubChannelIndex = 0;
        }
        else
        {
            Stream *stream = SAFE_CAST(Stream, sound);
            ChannelStream *channelstream = stream->mChannel;

            chan->mRealChannel[0] = channelstream;
            chan->mNumRealChannels = 1;

            channelstream->mNumRealChannels = subchannels;
            channelstream->mSubChannelIndex = 0;

            for (count = 0; count < subchannels; count++)
            {
                channelstream->mRealChannel[count] = realchannel[count];
                channelstream->mRealChannel[count]->mSubChannelIndex = count;
            }
        }
    }
    else
#endif
    {
        chan->mNumRealChannels = subchannels;

        for (count = 0; count < subchannels; count++)
        {
            chan->mRealChannel[count] = realchannel[count];
            chan->mRealChannel[count]->mSubChannelIndex = count;
            chan->mRealChannel[count]->mNumRealChannels = 1;
        }
    }

    *channel = chan;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 92579bf1 manual
// OFFSET: 0x008D3420
FMOD_RESULT SystemI::findChannel(FMOD_CHANNELINDEX id, DSPI *dsp, ChannelI **channel)
{
    ChannelI       *chan = 0;
    ChannelReal    *realchannel[FMOD_CHANNEL_MAXREALSUBCHANNELS] = { 0 };
    FMOD_RESULT     result;

    if (!channel)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (id == FMOD_CHANNEL_REUSE)
    {
        chan = *channel;
        if (!chan)
        {
            id = FMOD_CHANNEL_FREE;
        }
        else
        {
            if (chan->mRealChannel[0]->mFlags & CHANNELREAL_FLAG_STOPPED)
            {
                id = (FMOD_CHANNELINDEX)chan->mIndex;
            }
            else
            {
                chan->stopEx(false, false, false, true, false, true, false);
            }
        }
    }

    if (id != FMOD_CHANNEL_REUSE)
    {
        *channel = 0;

        if (id != FMOD_CHANNEL_FREE)
        {
            chan = &mChannel[id];
            chan->stop();
        }
        else
        {
            if (!mChannelFreeListHead.isEmpty())
            {
                chan = SAFE_CAST(ChannelI, mChannelFreeListHead.getNext());
            }
            else
            {
                LinkedListNode *node;

                if (mChannelSortedListHead.isEmpty())
                {
                    return FMOD_ERR_CHANNEL_ALLOC;
                }

                node = mChannelSortedListHead.getPrev();
                chan = (ChannelI *)node->getData();

                chan->stopEx(true, false, true, true, false, true, false);
            }
        }

        chan->removeNode();
        chan->addBefore(&mChannelUsedListHead);
    }

    result = mSoftware->getFreeChannel(FMOD_SOFTWARE, realchannel, 1, 1, 0);
    if (result != FMOD_OK)
    {
        result = mEmulated->getFreeChannel(FMOD_SOFTWARE, realchannel, 1, 1, 0);
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    chan->mRealChannel[0] = realchannel[0];
    chan->mNumRealChannels = 1;

    *channel = chan;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 361aa6a2 manual
// OFFSET: 0x008D5590
FMOD_RESULT SystemI::createSample(FMOD_MODE mode, FMOD_CODEC_WAVEFORMAT *waveformat, Sample **sample_out)
{
    FMOD_RESULT  result;
    Output      *output;
    Sample      *sample = 0;
    int          channels, subsamples, count;
    unsigned int loopstart = 0, loopend = 0;

    if (!sample_out)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (waveformat->lengthpcm == (unsigned int)-1)
    {
        return FMOD_ERR_MEMORY;
    }

    if (!(mode & (FMOD_HARDWARE | FMOD_SOFTWARE)))
    {
        int num2d, num3d;

        mode |= (waveformat->mode & (FMOD_HARDWARE | FMOD_SOFTWARE | FMOD_2D | FMOD_3D));

        getHardwareChannels(&num2d, &num3d, 0);

        if (mode & FMOD_HARDWARE)
        {
            if (mode & FMOD_3D)
            {
                if (!num3d)
                {
                    mode &= ~FMOD_HARDWARE;
                    mode |= FMOD_SOFTWARE;
                }
            }
            else
            {
                if (!num2d)
                {
                    mode &= ~FMOD_HARDWARE;
                    mode |= FMOD_SOFTWARE;
                }
            }
        }
    }

    if (mode & FMOD_SOFTWARE)
    {
        if (!mSoftware)
        {
            return FMOD_ERR_NEEDSSOFTWARE;
        }

        mode &= ~FMOD_HARDWARE;
        output = mSoftware;
    }
    else
    {
        mode |= FMOD_HARDWARE;
        output = mOutput;
    }

    if (!(mode & (FMOD_2D | FMOD_3D)))
    {
        mode |= (waveformat->mode & (FMOD_2D | FMOD_3D));
    }
    if (mode & FMOD_3D)
    {
        mode &= ~FMOD_2D;
    }
    else
    {
        mode |= FMOD_2D;
    }

    if (!(mode & (FMOD_LOOP_OFF | FMOD_LOOP_NORMAL | FMOD_LOOP_BIDI)))
    {
        mode |= (waveformat->mode & (FMOD_LOOP_OFF | FMOD_LOOP_NORMAL | FMOD_LOOP_BIDI));
    }
    if (mode & FMOD_LOOP_NORMAL)
    {
        mode &= ~FMOD_LOOP_OFF;
        mode &= ~FMOD_LOOP_BIDI;
        mode |= FMOD_LOOP_NORMAL;
    }
    else if (mode & FMOD_LOOP_BIDI)
    {
        mode &= ~FMOD_LOOP_OFF;
        mode |= FMOD_LOOP_BIDI;
    }
    else
    {
        mode |= FMOD_LOOP_OFF;
    }

    if (waveformat->mode & FMOD_CREATECOMPRESSEDSAMPLE)
    {
        mode |= FMOD_CREATECOMPRESSEDSAMPLE;
        mode &= ~FMOD_CREATESAMPLE;
    }

    channels = waveformat->channels;

    if (channels > FMOD_CHANNEL_MAXREALSUBCHANNELS && channels > (output->mDescription.getsamplemaxchannels ? output->mDescription.getsamplemaxchannels(output, mode, waveformat->format) : 1))
    {
        return FMOD_ERR_TOOMANYCHANNELS;
    }

    if (channels > 1 && channels > (output->mDescription.getsamplemaxchannels ? output->mDescription.getsamplemaxchannels(output, mode, waveformat->format) : 1))
    {
        subsamples = channels;

        if (*sample_out)
        {
            sample = *sample_out;
        }
        else
        {
            sample = FMOD_Object_Calloc(Sample);
            if (!sample)
            {
                return FMOD_ERR_MEMORY;
            }
        }

        sample->mNumSubSamples = channels;

        if (!sample->mName && !(mode & FMOD_LOWMEM))
        {
            sample->mName = (char *)FMOD_Memory_Calloc(FMOD_STRING_MAXNAMELEN);
            if (!sample->mName)
            {
                return FMOD_ERR_MEMORY;
            }
        }
        if (sample->mName)
        {
            FMOD_strcpy(sample->mName,  waveformat->name);
        }

        sample->mDefaultFrequency   = (float)waveformat->frequency;
        sample->mDefaultChannelMask = waveformat->channelmask;
        sample->mLength             = waveformat->lengthpcm;
        sample->mMode               = mode;
        sample->mLoopStart          = 0;
        sample->mLoopLength         = sample->mLength;
        sample->mFormat             = waveformat->format;
        sample->mCodec              = 0;
        sample->mChannels           = channels;
        sample->mType               = FMOD_SOUND_TYPE_USER;
        sample->mSystem             = this;
        sample->mMinDistance        = 1.0f     * mDistanceScale;
        sample->mMaxDistance        = 10000.0f * mDistanceScale;
        sample->mLockBuffer         = mMultiSubSampleLockBuffer.alloc(SOUND_READCHUNKSIZE);
        if (!sample->mLockBuffer)
        {
            return FMOD_ERR_MEMORY;
        }

        channels = 1;
    }
    else
    {
        subsamples = 1;
    }

    for (count = 0; count < subsamples; count++)
    {
        Sample *subsample = subsamples == 1 ? *sample_out : 0;
        FMOD_CODEC_WAVEFORMAT subsamplewaveformat;

        FMOD_memcpy(&subsamplewaveformat, waveformat, sizeof(FMOD_CODEC_WAVEFORMAT));
        subsamplewaveformat.channels = channels;

        if (output->mDescription.createsample)
        {
            output->readfrommixer = Output::mixCallback;

            result = output->mDescription.createsample(output, mode, &subsamplewaveformat, &subsample);
        }
        else
        {
            result = mSoftware->createSample(mode, &subsamplewaveformat, &subsample);
        }
        if (result != FMOD_OK)
        {
            return result;
        }

        if (subsamples > 1)
        {
            sample->mSubSample[count] = subsample;
        }
        else
        {
            sample = subsample;

            if (!sample->mName && !(mode & FMOD_LOWMEM))
            {
                sample->mName = (char *)FMOD_Memory_Calloc(FMOD_STRING_MAXNAMELEN);
                if (!sample->mName)
                {
                    return FMOD_ERR_MEMORY;
                }
            }
        }

        if (subsample->mName)
        {
            FMOD_strcpy(subsample->mName,  waveformat->name);
        }

        subsample->mDefaultFrequency   = (float)waveformat->frequency;
        subsample->mDefaultChannelMask = waveformat->channelmask;
        subsample->mMode               = mode;
        subsample->mLoopStart          = 0;
        subsample->mLoopLength         = subsample->mLength;
        subsample->mFormat             = waveformat->format;
        subsample->mChannels           = channels;
        subsample->mCodec              = 0;
        subsample->mType               = FMOD_SOUND_TYPE_USER;
        subsample->mSystem             = this;
        subsample->mMinDistance        = 1.0f     * mDistanceScale;
        subsample->mMaxDistance        = 10000.0f * mDistanceScale;
        subsample->mSubSampleParent    = sample;
    }

    loopstart = waveformat->loopstart;
    loopend   = waveformat->loopend;
    if (!loopend)
    {
        loopend = sample->mLength - 1;
    }

    sample->setLoopPoints(loopstart, FMOD_TIMEUNIT_PCM, loopend, FMOD_TIMEUNIT_PCM);

    *sample_out = sample;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: cc24bd03 manual
// OFFSET: 0x008D67D0
FMOD_RESULT SystemI::createSoundInternal(const char *name_or_data, FMOD_MODE mode_in, FMOD_CREATESOUNDEXINFO *exinfo, bool calledfromasync, SoundI **sound)
{
    FMOD_RESULT            result;
    SoundI               **subsound    = 0;
    SoundI                *soundi      = 0;
    Codec                 *codec       = 0;
    Metadata              *metadata    = 0;
    File                  *file        = 0;
    FMOD_CODEC_WAVEFORMAT  waveformat;
    bool                   netfile     = false;
    int                    count;
    int                    numsubsounds;
    int                    numcodecs;
    int                    maxchannels = 0;
    FMOD_MODE              originalmode = mode_in;
    LocalMemoryCounter     memcounter;

    if (!sound)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (!(mode_in & FMOD_NONBLOCKING))
    {
        *sound = 0;
    }

    if (exinfo && exinfo->cbsize != sizeof(FMOD_CREATESOUNDEXINFO))
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (mode_in & FMOD_CREATESTREAM)
    {
        mode_in &= ~FMOD_CREATESAMPLE;
        mode_in &= ~FMOD_CREATECOMPRESSEDSAMPLE;
    }
    else if (mode_in & FMOD_CREATECOMPRESSEDSAMPLE)
    {
        mode_in &= ~FMOD_CREATESAMPLE;
    }

    if (mode_in & FMOD_2D)
    {
        mode_in &= ~(FMOD_3D_HEADRELATIVE | FMOD_3D_WORLDRELATIVE | FMOD_3D_LOGROLLOFF | FMOD_3D_LINEARROLLOFF | FMOD_3D_CUSTOMROLLOFF);
    }
    if (mode_in & (FMOD_3D_HEADRELATIVE | FMOD_3D_WORLDRELATIVE | FMOD_3D_LOGROLLOFF | FMOD_3D_LINEARROLLOFF | FMOD_3D_CUSTOMROLLOFF))
    {
        mode_in |= FMOD_3D;
    }

    if (!(mode_in & FMOD_SOFTWARE))
    {
        int num2d, num3d;

        getHardwareChannels(&num2d, &num3d, 0);

        if (mode_in & FMOD_3D)
        {
            if (!num3d)
            {
                mode_in &= ~FMOD_HARDWARE;
                mode_in |= FMOD_SOFTWARE;
            }
        }
        else
        {
            if (!num2d)
            {
                mode_in &= ~FMOD_HARDWARE;
                mode_in |= FMOD_SOFTWARE;
            }
        }
    }

    if (mode_in & FMOD_OPENUSER || mode_in & FMOD_OPENRAW)
    {
        if (!exinfo)
        {
            return FMOD_ERR_INVALID_PARAM;
        }

        if (exinfo->format == FMOD_SOUND_FORMAT_NONE ||
            exinfo->numchannels <= 0 ||
            exinfo->defaultfrequency == 0)
        {
            return FMOD_ERR_INVALID_PARAM;
        }
    }

    if (mode_in & (FMOD_OPENMEMORY | FMOD_OPENMEMORY_POINT))
    {
        file = FMOD_Object_Alloc(MemoryFile);
        if (!file)
        {
            return FMOD_ERR_MEMORY;
        }

        file->mBlockSize = 0;
    }
    else if (mode_in & FMOD_OPENUSER)
    {
        file = FMOD_Object_Alloc(NullFile);
        if (!file)
        {
            return FMOD_ERR_MEMORY;
        }

        mode_in &= ~FMOD_OPENRAW;
    }
    else
    {
        if (exinfo && exinfo->useropen && exinfo->userclose && exinfo->userread && exinfo->userseek)
        {
            UserFile *userfile = FMOD_Object_Alloc(UserFile);
            if (!userfile)
            {
                return FMOD_ERR_MEMORY;
            }

            result = userfile->setUserCallbacks(exinfo->useropen, exinfo->userclose, exinfo->userread, exinfo->userseek);
            if (result != FMOD_OK)
            {
                return result;
            }

            file = userfile;
        }
        else if (File::gUsesUserCallbacks)
        {
            file = FMOD_Object_Alloc(UserFile);
            if (!file)
            {
                return FMOD_ERR_MEMORY;
            }
        }
#ifdef FMOD_SUPPORT_NET
        else if (!FMOD_strnicmp("http://",   name_or_data, 7) ||
                 !FMOD_strnicmp("http:\\\\", name_or_data, 7) ||
                 !FMOD_strnicmp("https://",  name_or_data, 8) ||
                 !FMOD_strnicmp("https:\\\\", name_or_data, 8) ||
                 !FMOD_strnicmp("mms://",    name_or_data, 6) ||
                 !FMOD_strnicmp("mms:\\\\",  name_or_data, 6))
        {
            file = FMOD_Object_Alloc(NetFile);
            if (!file)
            {
                return FMOD_ERR_MEMORY;
            }

            netfile = true;
        }
#endif
#ifdef FMOD_SUPPORT_CDDA
        else if (FMOD_OS_CDDA_IsDeviceName((char *)name_or_data))
        {
            CddaFile *cddafile = FMOD_Object_Alloc(CddaFile);
            if (!cddafile)
            {
                return FMOD_ERR_MEMORY;
            }

            cddafile->init((mode_in & FMOD_CDDA_FORCEASPI) ? true : false, (mode_in & FMOD_CDDA_JITTERCORRECT) ? true : false);

            file = cddafile;
        }
#endif
        else
        {
            file = FMOD_Object_Alloc(DiskFile);
            if (!file)
            {
                return FMOD_ERR_MEMORY;
            }
        }
    }

    file->mSystem = this;

    if (*sound && netfile)
    {
        (*sound)->mOpenState = FMOD_OPENSTATE_CONNECTING;
    }

    result = file->open(name_or_data, exinfo ? exinfo->length : 0, (mode_in & FMOD_UNICODE) ? true : false, exinfo ? exinfo->encryptionkey : 0);
    if (result != FMOD_OK)
    {
        FMOD_Memory_Free(file);
        return result;
    }

    if (*sound && netfile)
    {
        (*sound)->mOpenState = FMOD_OPENSTATE_BUFFERING;
    }

    result = file->setStartOffset(exinfo ? exinfo->fileoffset : 0);
    if (result != FMOD_OK)
    {
        return result;
    }

    result = mPluginFactory->getNumCodecs(&numcodecs);
    if (result != FMOD_OK)
    {
        return result;
    }

    for (count = 0; count < numcodecs; count++)
    {
        FMOD_CODEC_DESCRIPTION_EX *codecdesc;

        result = mPluginFactory->getCodec(count, &codecdesc);
        if (result != FMOD_OK)
        {
            continue;
        }

        if (exinfo)
        {
            int memberoffset = (int)((FMOD_UINT_NATIVE)&exinfo->suggestedsoundtype - (FMOD_UINT_NATIVE)exinfo);

            if (exinfo->cbsize > memberoffset && exinfo->suggestedsoundtype && !(mode_in & FMOD_OPENUSER))
            {
                if (codecdesc->mType != exinfo->suggestedsoundtype)
                {
                    continue;
                }
            }
        }

        if (mode_in & FMOD_OPENRAW)
        {
            if (codecdesc->mType != FMOD_SOUND_TYPE_RAW)
            {
                codecdesc = 0;
                continue;
            }
        }
        else if (mode_in & FMOD_OPENUSER)
        {
            if (codecdesc->mType != FMOD_SOUND_TYPE_USER)
            {
                codecdesc = 0;
                continue;
            }
        }
        else
        {
            if (codecdesc->mType == FMOD_SOUND_TYPE_RAW || codecdesc->mType == FMOD_SOUND_TYPE_USER)
            {
                codecdesc = 0;
                continue;
            }
        }

        result = mPluginFactory->createCodec(codecdesc, &codec);
        if (result != FMOD_OK)
        {
            if (result == FMOD_ERR_MEMORY)
            {
                return result;
            }
            continue;
        }

        codec->mFile           = file;
        codec->mMode           = mode_in;
        codec->mOriginalMode   = originalmode;
        codec->filehandle      = file;
        codec->filesize        = file->mLength;
        codec->mSystem         = this;
        codec->mFlags         |= FMOD_CODEC_ACCURATELENGTH;

        result = codec->mDescription.open(codec, mode_in, exinfo);
        if (result == FMOD_OK)
        {
            mode_in = codec->mMode;

            result = codec->mDescription.getwaveformat(codec, 0, &waveformat);
            if (result != FMOD_OK)
            {
                return result;
            }

            if (mode_in & FMOD_OPENUSER)
            {
                waveformat.format    = exinfo->format;
                waveformat.channels  = exinfo->numchannels;
                waveformat.frequency = exinfo->defaultfrequency;

                result = SoundI::getSamplesFromBytes(exinfo->length, &waveformat.lengthpcm, exinfo->numchannels, exinfo->format);
                if (result != FMOD_OK)
                {
                    return result;
                }

                waveformat.blockalign = codec->waveformat[0].blockalign = 1;
            }
            else if (mode_in & FMOD_OPENRAW)
            {
                unsigned int filesize;

                file->getSize(&filesize);

                waveformat.format    = codec->waveformat[0].format    = exinfo->format;
                waveformat.channels  = codec->waveformat[0].channels  = exinfo->numchannels;
                waveformat.frequency = codec->waveformat[0].frequency = exinfo->defaultfrequency;

                result = SoundI::getSamplesFromBytes(filesize, &waveformat.lengthpcm, exinfo->numchannels, exinfo->format);
                if (result != FMOD_OK)
                {
                    return result;
                }
            }

            maxchannels = waveformat.channels;
            codec->mBlockAlign = waveformat.blockalign;

            if (codec->numsubsounds && mode_in & FMOD_CREATESTREAM)
            {
                for (count = 0; count < codec->numsubsounds; count++)
                {
                    FMOD_CODEC_WAVEFORMAT waveformat2;

                    result = codec->mDescription.getwaveformat(codec, count, &waveformat2);
                    if (result != FMOD_OK)
                    {
                        return result;
                    }

                    if (waveformat2.channels > maxchannels)
                    {
                        maxchannels        = waveformat2.channels;
                        codec->mBlockAlign = waveformat2.blockalign;
                    }
                }
            }
            break;
        }

#ifdef FMOD_SUPPORT_TAGS
        if (codec->mType == FMOD_SOUND_TYPE_TAG && !metadata)
        {
            metadata = codec->mMetadata;
        }
#endif

        codec->mFile = 0;
        codec->mMetadata = 0;
        codec->release();
        codec = 0;

        if (result != FMOD_ERR_FORMAT && result != FMOD_ERR_FILE_EOF)
        {
            file->close();
            FMOD_Memory_Free(file);

            if (metadata)
            {
                metadata->release();
            }
            return result;
        }
    }

    if (!codec)
    {
        file->close();
        FMOD_Memory_Free(file);

        if (metadata)
        {
            metadata->release();
        }
        return FMOD_ERR_FORMAT;
    }

    numsubsounds = exinfo && exinfo->numsubsounds ? exinfo->numsubsounds : codec->numsubsounds;

    if (metadata)
    {
        if (codec->mMetadata)
        {
            codec->mMetadata->add(metadata);
            metadata->release();
        }
        else
        {
            codec->mMetadata = metadata;
        }
    }

    codec->getMetadataFromFile();

    if (numsubsounds > 0)
    {
        subsound = (SoundI **)FMOD_Memory_Calloc(numsubsounds * sizeof(SoundI *));
        if (!subsound)
        {
            return FMOD_ERR_MEMORY;
        }
    }

    result = codec->mDescription.getwaveformat(codec, 0, &waveformat);
    if (result != FMOD_OK)
    {
        return result;
    }

    if (waveformat.channels > mMaxInputChannels)
    {
        return FMOD_ERR_TOOMANYCHANNELS;
    }

    if (codec->mDescription.defaultasstream && !(mode_in & FMOD_CREATESAMPLE))
    {
        mode_in |= FMOD_CREATESTREAM;
    }

    if (codec->mType == FMOD_SOUND_TYPE_PLAYLIST)
    {
        SoundI *soundcontainer = 0;

        if (mode_in & FMOD_NONBLOCKING)
        {
            soundcontainer = *sound;
        }
        else
        {
            soundcontainer = FMOD_Object_Calloc(Sample);
            if (!soundcontainer)
            {
                return FMOD_ERR_MEMORY;
            }
        }

        if (!soundcontainer->mName && !(mode_in & FMOD_LOWMEM))
        {
            soundcontainer->mName = (char *)FMOD_Memory_Calloc(FMOD_STRING_MAXNAMELEN);
            if (!soundcontainer->mName)
            {
                return FMOD_ERR_MEMORY;
            }
        }

        soundcontainer->mSystem                  = this;
        soundcontainer->mPostReadCallback        = exinfo ? exinfo->pcmreadcallback : 0;
        soundcontainer->mPostSetPositionCallback = exinfo ? exinfo->pcmsetposcallback : 0;
        soundcontainer->mPostCallbackSound       = (FMOD_SOUND *)soundcontainer;
        soundcontainer->mType                    = codec->mType;
        soundcontainer->mCodec                   = codec;
        soundcontainer->mMinDistance             = 1.0f     * mDistanceScale;
        soundcontainer->mMaxDistance             = 10000.0f * mDistanceScale;
        soundcontainer->mSubSound                = 0;
        soundcontainer->mNumSubSounds            = 0;
        soundcontainer->mSubSoundIndex           = -1;
        soundcontainer->mUserData                = exinfo ? exinfo->userdata : 0;

        *sound = soundcontainer;
    }
    else
#ifdef FMOD_SUPPORT_STREAMING
    if (mode_in & FMOD_CREATESTREAM)
    {
        unsigned int          decodebuffersize;
        unsigned int          blocksize;
        Stream               *stream;
        Sample               *sample      = 0;
        FMOD_MODE             sample_mode = mode_in & ~(FMOD_CREATECOMPRESSEDSAMPLE);
        FMOD_CODEC_WAVEFORMAT waveformatstream;

        if (mode_in & FMOD_NONBLOCKING)
        {
            stream = SAFE_CAST(Stream, *sound);
        }
        else
        {
            stream = FMOD_Object_Calloc(Stream);
            if (!stream)
            {
                if (subsound)
                {
                    FMOD_Memory_Free(subsound);
                }
                codec->release();
                return FMOD_ERR_MEMORY;
            }
        }

        if (!stream->mName && !(mode_in & FMOD_LOWMEM))
        {
            stream->mName = (char *)FMOD_Memory_Calloc(FMOD_STRING_MAXNAMELEN);
            if (!stream->mName)
            {
                if (subsound)
                {
                    FMOD_Memory_Free(subsound);
                }
                codec->release();
                return FMOD_ERR_MEMORY;
            }
        }

        stream->mSubSound     = subsound;
        stream->mNumSubSounds = numsubsounds;
        stream->mCodec        = codec;
        stream->mSystem       = this;

        SoundI::getSamplesFromBytes(waveformat.blockalign, &blocksize, maxchannels, waveformat.format);   // VERIFY

        decodebuffersize = exinfo ? exinfo->decodebuffersize : 0;
        if (!decodebuffersize)
        {
            decodebuffersize  = 400;
            decodebuffersize *= waveformat.frequency;
            decodebuffersize /= 1000;
        }

        if (!blocksize)
        {
            blocksize = decodebuffersize;
        }

        if (blocksize < 256)
        {
            if (blocksize <= 0)
            {
                blocksize = 1;
            }
            blocksize = ((256 + (blocksize - 1)) / blocksize) * blocksize;
        }

        decodebuffersize /= blocksize;
        decodebuffersize *= blocksize;
        if (decodebuffersize <= blocksize)
        {
            decodebuffersize = blocksize * 2;
        }

        sample_mode &= ~FMOD_LOOP_OFF;
        sample_mode &= ~FMOD_LOOP_BIDI;
        sample_mode &= ~FMOD_OPENMEMORY_POINT;
        sample_mode |=  FMOD_LOOP_NORMAL;

        FMOD_memcpy(&waveformatstream, &waveformat, sizeof(FMOD_CODEC_WAVEFORMAT));

        if (exinfo && exinfo->speakermap)
        {
            if (exinfo->speakermap == FMOD_SPEAKERMAPTYPE_ALLMONO)
            {
                waveformatstream.channelmask = SPEAKER_ALLMONO;
            }
            else if (exinfo->speakermap == FMOD_SPEAKERMAPTYPE_ALLSTEREO)
            {
                waveformatstream.channelmask = SPEAKER_ALLSTEREO;
            }
        }

        waveformatstream.lengthpcm = decodebuffersize;
        waveformatstream.loopend   = decodebuffersize - 1;
        waveformatstream.channels  = maxchannels;
        waveformatstream.loopstart = 0;

        result = createSample(sample_mode, &waveformatstream, &sample);
        if (result != FMOD_OK)
        {
            return result;
        }

        sample->mPostReadCallback        = exinfo ? exinfo->pcmreadcallback : 0;
        sample->mPostSetPositionCallback = exinfo ? exinfo->pcmsetposcallback : 0;
        sample->mUserData                = exinfo ? exinfo->userdata : 0;
        sample->mPostCallbackSound       = (FMOD_SOUND *)stream;
        sample->mCodec                   = codec;

        if (sample->mNumSubSamples)
        {
            for (int count = 0; count < sample->mNumSubSamples; count++)
            {
                sample->mSubSample[count]->mCodec = codec;
            }
        }

        stream->mSample                  = sample;
        stream->mSubSoundIndex           = 0;
        stream->mLength                  = waveformat.lengthpcm;
        stream->mLengthBytes             = waveformat.lengthbytes;
        stream->mDefaultFrequency        = (float)waveformat.frequency;
        stream->mMode                    = sample_mode | FMOD_UNIQUE | FMOD_CREATESTREAM;
        stream->mMode                    = stream->mMode & ~FMOD_LOOP_NORMAL;
        stream->mFormat                  = waveformat.format;
        stream->mChannels                = maxchannels;
        stream->mSystem                  = this;
        stream->mPostReadCallback        = exinfo ? exinfo->pcmreadcallback : 0;
        stream->mPostSetPositionCallback = exinfo ? exinfo->pcmsetposcallback : 0;
        stream->mPostCallbackSound       = (FMOD_SOUND *)stream;
        stream->mBlockSize               = blocksize;
        stream->mType                    = codec->mType;
        stream->mMinDistance             = 1.0f     * mDistanceScale;
        stream->mMaxDistance             = 10000.0f * mDistanceScale;
        stream->mUserData                = exinfo ? exinfo->userdata : 0;

        if (stream->mName)
        {
            FMOD_strcpy(stream->mName, waveformat.name);
        }

        if (exinfo && exinfo->speakermap)
        {
            if (exinfo->speakermap == FMOD_SPEAKERMAPTYPE_ALLMONO)
            {
                sample->mDefaultChannelMask = SPEAKER_ALLMONO;
            }
            else if (exinfo->speakermap == FMOD_SPEAKERMAPTYPE_ALLSTEREO)
            {
                sample->mDefaultChannelMask = SPEAKER_ALLSTEREO;
            }
        }
        else
        {
            sample->mDefaultChannelMask = waveformat.channelmask;
        }

        stream->mMode &= ~(FMOD_2D | FMOD_3D | FMOD_HARDWARE | FMOD_SOFTWARE);
        stream->mMode |= (sample->mMode & (FMOD_2D | FMOD_3D | FMOD_HARDWARE | FMOD_SOFTWARE));

        if (waveformat.mode & FMOD_LOOP_OFF || mode_in & FMOD_LOOP_OFF)
        {
            stream->mMode |= FMOD_LOOP_OFF;
        }
        else if (waveformat.mode & FMOD_LOOP_NORMAL || mode_in & FMOD_LOOP_NORMAL)
        {
            stream->mMode |= FMOD_LOOP_NORMAL;
        }
        else
        {
            stream->mMode |= FMOD_LOOP_OFF;
        }

        stream->setLoopPoints(waveformat.loopstart, FMOD_TIMEUNIT_PCM, waveformat.loopend, FMOD_TIMEUNIT_PCM);

        if (!stream->mSoundGroupNode.getData())
        {
            result = stream->setSoundGroup(mSoundGroup);
            if (result != FMOD_OK)
            {
                return result;
            }
        }

        stream->mChannel = FMOD_Object_Calloc(ChannelStream);
        if (!stream->mChannel)
        {
            if (!(stream->mMode & FMOD_NONBLOCKING))
            {
                stream->release();
            }
            return FMOD_ERR_MEMORY;
        }

        FMOD_OS_CriticalSection_Enter(mStreamListCrit);
        {
            stream->mStreamNode.setData(stream);
            stream->mStreamNode.addBefore(&mStreamListSoundHead);
        }
        FMOD_OS_CriticalSection_Leave(mStreamListCrit);

        if (numsubsounds > 0 && !(mode_in & FMOD_OPENUSER))
        {
            Stream *substream = FMOD_Object_Calloc(Stream);
            if (!substream)
            {
                if (!(stream->mMode & FMOD_NONBLOCKING))
                {
                    stream->release();
                }
                return FMOD_ERR_MEMORY;
            }

            stream->mSubSoundShared = substream;

            if (!(mode_in & FMOD_LOWMEM))
            {
                substream->mName = (char *)FMOD_Memory_Calloc(FMOD_STRING_MAXNAMELEN);
                if (!substream->mName)
                {
                    if (!(stream->mMode & FMOD_NONBLOCKING))
                    {
                        stream->release();
                    }
                    return FMOD_ERR_MEMORY;
                }
            }

            substream->mSample           = sample;
            substream->mSystem           = this;
            substream->mCodec            = codec;
            substream->mSubSoundIndex    = count;   // VERIFY
            substream->mChannel          = stream->mChannel;
            substream->mMode             = stream->mMode;
            substream->mType             = stream->mType;
            substream->mAsyncData        = stream->mAsyncData;
            substream->mMinDistance      = 1.0f     * mDistanceScale;
            substream->mMaxDistance      = 10000.0f * mDistanceScale;
            substream->mBlockSize        = blocksize;
            substream->mSubSoundParent   = stream;
            substream->mSubSoundShared   = substream;

            if (!substream->mSoundGroupNode.getData())
            {
                result = substream->setSoundGroup(mSoundGroup);
                if (result != FMOD_OK)
                {
                    if (!(stream->mMode & FMOD_NONBLOCKING))
                    {
                        stream->release();
                    }
                    return result;
                }
            }

            for (count = 0; count < numsubsounds; count++)
            {
                subsound[count] = substream;

                if (count < codec->numsubsounds)
                {
                    FMOD_CODEC_WAVEFORMAT waveformat2;

                    result = codec->mDescription.getwaveformat(codec, count, &waveformat2);
                    if (result != FMOD_OK)
                    {
                        return result;
                    }

                    if (substream->mName)
                    {
                        FMOD_strcpy(substream->mName,  waveformat2.name);
                    }

                    substream->mDefaultChannelMask = waveformat2.channelmask;
                    substream->mDefaultFrequency   = (float)waveformat2.frequency;
                    substream->mChannels           = waveformat2.channels;
                    substream->mFormat             = waveformat2.format;
                    substream->mLoopLength         = waveformat2.loopend - waveformat2.loopstart + 1;
                    substream->mLength             = waveformat2.lengthpcm;
                    substream->mLengthBytes        = waveformat2.lengthbytes;
                    substream->mLoopStart          = waveformat2.loopstart;

                    substream->setLoopPoints(waveformat2.loopstart, FMOD_TIMEUNIT_PCM, waveformat2.loopend, FMOD_TIMEUNIT_PCM);

                    if (codec->mDescription.soundcreate)
                    {
                        result = codec->mDescription.soundcreate(codec, count, (FMOD_SOUND *)substream);
                        if (result != FMOD_OK)
                        {
                            return result;
                        }
                    }
                }

                stream->mNumActiveSubSounds++;
            }

            if (codec->numsubsounds)
            {
                substream->updateSubSound(0, false);
            }

            if (!(mode_in & FMOD_LOWMEM) && !(exinfo && exinfo->initialsubsound))
            {
                stream->setSubSoundSentence(0, numsubsounds);
            }
        }
        else
        {
            if (codec->mDescription.soundcreate)
            {
                result = codec->mDescription.soundcreate(codec, 0, (FMOD_SOUND *)stream);
                if (result != FMOD_OK)
                {
                    return result;
                }
            }
        }

        if (exinfo)
        {
            switch (exinfo->initialseekpostype)
            {
                case 0:
                case FMOD_TIMEUNIT_PCM:
                {
                    break;
                }
                case FMOD_TIMEUNIT_MS:
                {
                    exinfo->initialseekposition = (unsigned int)((float)exinfo->initialseekposition * stream->mDefaultFrequency / 1000.0f);
                    exinfo->initialseekpostype = FMOD_TIMEUNIT_PCM;
                    break;
                }
                case FMOD_TIMEUNIT_PCMBYTES:
                {
                    stream->getSamplesFromBytes(exinfo->initialseekposition, &exinfo->initialseekposition);
                    exinfo->initialseekpostype = FMOD_TIMEUNIT_PCM;
                    break;
                }
                default:
                {
                    return FMOD_ERR_INVALID_PARAM;
                }
            }
        }

        if (exinfo && exinfo->initialsubsound && stream->mSubSound)
        {
            Stream *substream = 0;

            if (exinfo->initialsubsound < 0 || exinfo->initialsubsound >= stream->mNumSubSounds)
            {
                if (!(stream->mMode & FMOD_NONBLOCKING))
                {
                    stream->release();
                }
                return FMOD_ERR_INVALID_PARAM;
            }

            stream->mSubSoundIndex       = exinfo->initialsubsound;
            stream->mSubSoundListCurrent = exinfo->initialsubsound;

            substream = SAFE_CAST(Stream, stream->mSubSound[stream->mSubSoundIndex]);
            if (substream)
            {
                substream->mSubSoundIndex       = exinfo->initialsubsound;
                substream->mSubSoundListCurrent = exinfo->initialsubsound;

                if (substream->mSubSoundShared)
                {
                    result = substream->updateSubSound(exinfo->initialsubsound, false);
                    if (result != FMOD_OK)
                    {
                        if (!(stream->mMode & FMOD_NONBLOCKING))
                        {
                            stream->release();
                        }
                        return result;
                    }
				}

                stream->mInitialPosition = exinfo->initialseekposition;

                result = substream->setPosition(exinfo->initialseekposition, FMOD_TIMEUNIT_PCM);
                if (result != FMOD_OK)
                {
                    if (!(stream->mMode & FMOD_NONBLOCKING))
                    {
                        stream->release();
                    }
                    return result;
                }
            }
        }
        else
        {
            stream->mInitialPosition = exinfo ? exinfo->initialseekposition : 0;

            result = stream->setPosition(exinfo ? exinfo->initialseekposition : 0, FMOD_TIMEUNIT_PCM);
            if (result != FMOD_OK)
            {
                if (!(stream->mMode & FMOD_NONBLOCKING))
                {
                    stream->release();
                }
                return result;
            }
        }

        if (!(mode_in & FMOD_OPENONLY))
        {
            result = stream->flush();
            if (result != FMOD_OK)
            {
                if (!(stream->mMode & FMOD_NONBLOCKING))
                {
                    stream->release();
                }
                return result;
            }
        }

        {
            unsigned int sizebytes = 0;
            unsigned int buffersize = mStreamFileBufferSize;
            FMOD_TIMEUNIT buffersizetype = mStreamFileBufferSizeType;

            if (buffersizetype == FMOD_TIMEUNIT_RAWBYTES)
            {
                sizebytes = buffersize;
            }
            else
            {
                unsigned int pcm = buffersize;

                if (buffersizetype == FMOD_TIMEUNIT_MS)
                {
                    pcm = buffersize * (unsigned int)stream->mDefaultFrequency / 1000;
                }
                else if (buffersizetype == FMOD_TIMEUNIT_PCMBYTES)
                {
                    stream->getSamplesFromBytes(buffersize, &pcm);
                }

                if (stream->mLength == (unsigned int)-1 || file->mLength == (unsigned int)-1)
                {
                    stream->getBytesFromSamples(pcm, &sizebytes);
                }
                else
                {
                    float frac;

                    if (stream->mLengthBytes)
                    {
                        frac = (float)stream->mLengthBytes / (float)stream->mLength;
                    }
                    else
                    {
                        frac = (float)file->mLength / (float)stream->mLength;
                    }

                    sizebytes = (unsigned int)(frac * (float)pcm);
                }
            }

            if (!(mode_in & FMOD_OPENUSER))
            {
                result = file->enableDoubleBuffer(sizebytes);
                if (result != FMOD_OK)
                {
                    if (!(stream->mMode & FMOD_NONBLOCKING))
                    {
                        stream->release();
                    }
                    return result;
                }
            }
        }

        *sound = stream;
    }
    else
#endif
    {
        Sample *sample = 0, *samplecontainer = 0;
        bool realtimesample = false;

        if (numsubsounds > 0)
        {
            if (mode_in & FMOD_NONBLOCKING)
            {
                samplecontainer = SAFE_CAST(Sample, *sound);
            }
            else
            {
                samplecontainer = FMOD_Object_Calloc(Sample);
                if (!samplecontainer)
                {
                    return FMOD_ERR_MEMORY;
                }
            }

            if (!samplecontainer->mName && !(mode_in & FMOD_LOWMEM))
            {
                samplecontainer->mName = (char *)FMOD_Memory_Calloc(FMOD_STRING_MAXNAMELEN);
                if (!samplecontainer->mName)
                {
                    return FMOD_ERR_MEMORY;
                }
            }

            samplecontainer->mSystem                  = this;
            samplecontainer->mPostReadCallback        = exinfo ? exinfo->pcmreadcallback : 0;
            samplecontainer->mPostSetPositionCallback = exinfo ? exinfo->pcmsetposcallback : 0;
            samplecontainer->mPostCallbackSound       = (FMOD_SOUND *)samplecontainer;
            samplecontainer->mType                    = codec->mType;
            samplecontainer->mCodec                   = codec;
            samplecontainer->mMinDistance             = 1.0f     * mDistanceScale;
            samplecontainer->mMaxDistance             = 10000.0f * mDistanceScale;
            samplecontainer->mSubSound                = subsound;
            samplecontainer->mNumSubSounds            = numsubsounds;
            samplecontainer->mSubSoundIndex           = -1;
            samplecontainer->mUserData                = exinfo ? exinfo->userdata : 0;
        }

        for (count = 0; (count < (numsubsounds == 0 ? 1 : numsubsounds)) && (count < (numsubsounds == 0 ? 1 : codec->numsubsounds)); count++)
        {
            FMOD_MODE sample_mode = mode_in;
            FMOD_CODEC_WAVEFORMAT waveformat2;

            result = codec->mDescription.getwaveformat(codec, count, &waveformat2);
            if (result != FMOD_OK)
            {
                return result;
            }

            if (!numsubsounds && (sample_mode & FMOD_NONBLOCKING))
            {
                sample = SAFE_CAST(Sample, *sound);
            }
            else
            {
                sample = 0;
            }

            if (numsubsounds > 0 && exinfo && exinfo->inclusionlist)
            {
                bool found = false;
                int count2;

                for (count2 = 0; count2 < exinfo->inclusionlistnum; count2++)
                {
                    if (exinfo->inclusionlist[count2] == count)
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    continue;
                }
            }

            if (exinfo && exinfo->speakermap)
            {
                if (exinfo->speakermap == FMOD_SPEAKERMAPTYPE_ALLMONO)
                {
                    waveformat2.channelmask = SPEAKER_ALLMONO;
                }
                else if (exinfo->speakermap == FMOD_SPEAKERMAPTYPE_ALLSTEREO)
                {
                    waveformat2.channelmask = SPEAKER_ALLSTEREO;
                }
            }

            if (mode_in & FMOD_CREATECOMPRESSEDSAMPLE)
            {
                if (waveformat2.format == FMOD_SOUND_FORMAT_MPEG ||
                    waveformat2.format == FMOD_SOUND_FORMAT_IMAADPCM ||
                    waveformat2.format == FMOD_SOUND_FORMAT_XMA)
                {
                    realtimesample = true;
                }
                else
                {
                    sample_mode &= ~FMOD_CREATECOMPRESSEDSAMPLE;
                }
            }

            if (!realtimesample && mode_in & FMOD_OPENMEMORY_POINT)
            {
                result = codec->canPointTo();
                if (result != FMOD_OK)
                {
                    return result;
                }
            }

            result = createSample(sample_mode, &waveformat2, &sample);
            if (result != FMOD_OK)
            {
                if (samplecontainer)
                {
                    samplecontainer->release();
                }
                else if (codec)
                {
                    codec->release();
                }
                return result;
            }

            sample->mSubSoundIndex  = count;
            sample->mSubSoundParent = samplecontainer;
            sample->mType           = codec->mType;
            sample->mCodec          = codec;
            sample->mMinDistance    = 1.0f     * mDistanceScale;
            sample->mMaxDistance    = 10000.0f * mDistanceScale;
            sample->mUserData       = exinfo ? exinfo->userdata : 0;

            if (sample->mNumSubSamples)
            {
                for (int count = 0; count < sample->mNumSubSamples; count++)
                {
                    sample->mSubSample[count]->mCodec         = codec;
                    sample->mSubSample[count]->mSubSoundIndex = sample->mSubSoundIndex;
                }
            }

            if (!sample->mSoundGroupNode.getData())
            {
                result = sample->setSoundGroup(mSoundGroup);
                if (result != FMOD_OK)
                {
                    return result;
                }
            }

            if (!numsubsounds && count == 0)
            {
                sample->mPostReadCallback        = exinfo ? exinfo->pcmreadcallback : 0;
                sample->mPostSetPositionCallback = exinfo ? exinfo->pcmsetposcallback : 0;
                sample->mPostCallbackSound       = (FMOD_SOUND *)sample;
            }

            if (codec->mDescription.soundcreate)
            {
                result = codec->mDescription.soundcreate(codec, count, (FMOD_SOUND *)sample);
                if (result != FMOD_OK)
                {
                    return result;
                }
            }

            result = codec->reset();
            if (result != FMOD_OK)
            {
                return result;
            }

            if (waveformat2.mode & FMOD_CREATECOMPRESSEDSAMPLE || sample_mode & FMOD_CREATECOMPRESSEDSAMPLE)
            {
                result = codec->setPosition(count, 0, FMOD_TIMEUNIT_RAWBYTES);
            }
            else
            {
                result = codec->setPosition(count, 0, FMOD_TIMEUNIT_PCM);
            }
            if (result != FMOD_OK)
            {
                return result;
            }

            if (sample->mPostSetPositionCallback)
            {
                if (numsubsounds > 0)
                {
                    samplecontainer->mPostSetPositionCallback((FMOD_SOUND *)samplecontainer, count, 0, FMOD_TIMEUNIT_PCM);
                }
                else
                {
                    sample->mPostSetPositionCallback((FMOD_SOUND *)sample, 0, 0, FMOD_TIMEUNIT_PCM);
                }
            }

            if (sample_mode & FMOD_OPENMEMORY_POINT)
            {
                result = sample->setBufferData((char *)name_or_data + codec->mFile->mCurrentPosition);
                if (result != FMOD_OK)
                {
                    if (samplecontainer)
                    {
                        sample->release();
                        samplecontainer->release();
                    }
                    else if (codec)
                    {
                        sample->release();
                    }
                    return result;
                }
            }
            else if (!(sample_mode & FMOD_OPENONLY))
            {
                unsigned int samplelength, read;

                if (waveformat2.mode & FMOD_CREATECOMPRESSEDSAMPLE || sample_mode & FMOD_CREATECOMPRESSEDSAMPLE)
                {
                    samplelength = waveformat2.lengthbytes;

                    if (codec->mType != FMOD_SOUND_TYPE_FSB && codec->mType != FMOD_SOUND_TYPE_XMA)
                    {
                        samplelength -= codec->mSrcDataOffset;
                    }
                }
                else
                {
                    samplelength = waveformat2.lengthpcm;
                }

                result = sample->read(0, samplelength, &read);
   	            if (result != FMOD_OK && result != FMOD_ERR_FILE_EOF)
                {
                    if (!(sample->mMode & FMOD_NONBLOCKING))
                    {
                        if (samplecontainer)
                        {
                            sample->release();
                            samplecontainer->release();
                        }
                        else if (codec)
                        {
                            sample->release();
                        }
                    }
                    return result;
                }

                if (samplelength != read && sample->mLoopLength == sample->mLength)
                {
                    sample->mLoopLength = read;
                }

                result = sample->setPositionInternal(0);
                if (result != FMOD_OK && result != FMOD_ERR_FILE_EOF)
                {
                    return result;
                }
            }

            if (numsubsounds > 0)
            {
                subsound[count] = sample;
                samplecontainer->mNumActiveSubSounds++;
            }
        }

        if (!(mode_in & FMOD_OPENONLY))
        {
            file->close();
            FMOD_Memory_Free(file);

            codec->mFile = 0;
        }

        if (numsubsounds > 0)
        {
            *sound = samplecontainer;
        }
        else
        {
            *sound = sample;
        }
    }

    soundi = SAFE_CAST(SoundI, *sound);

    FMOD_OS_CriticalSection_Enter(gSoundListCrit);
    {
        soundi->addAfter(&mSoundListHead);
    }
    FMOD_OS_CriticalSection_Leave(gSoundListCrit);

	if (result != FMOD_OK && result != FMOD_ERR_FILE_EOF)
    {
        if (!(mode_in & FMOD_NONBLOCKING))
        {
            *sound = 0;
        }
        return result;
    }

    if (soundi->mName && ((mode_in & FMOD_UNICODE && !((short *)soundi->mName)[0]) || !soundi->mName[0]))
    {
        bool usefilename = true;

        if (codec->mMetadata)
        {
            FMOD_TAG tag;

            result = codec->mMetadata->getTag("TITLE", 0, &tag);
            if (result == FMOD_OK)
            {
                FMOD_strncpy(soundi->mName, (char *)tag.data, FMOD_STRING_MAXNAMELEN);
                usefilename = false;

                codec->mMetadata->addTag(tag.type, "TITLE", tag.data, tag.datalen, tag.datatype, true);
            }
            else
            {
                result = codec->mMetadata->getTag("TT2", 0, &tag);
                if (result == FMOD_OK)
                {
                    FMOD_strncpy(soundi->mName, (char *)tag.data, FMOD_STRING_MAXNAMELEN);
                    usefilename = false;

                    codec->mMetadata->addTag(tag.type, "TT2", tag.data, tag.datalen, tag.datatype, true);
                }
            }
        }

        if (usefilename && !(mode_in & (FMOD_OPENMEMORY | FMOD_OPENMEMORY_POINT)) && name_or_data)
        {
            if (mode_in & FMOD_UNICODE)
            {
                for (count = FMOD_strlenW((short *)name_or_data); count >= 0; count--)
                {
                    if (((short *)name_or_data)[count] == L'\\' || ((short *)name_or_data)[count] == L'/')
                    {
                        count++;
                        break;
                    }
                }
                if (count < 0)
                {
                    count = 0;
                }

                FMOD_strncpyW((short *)soundi->mName, (short *)name_or_data + count, FMOD_STRING_MAXNAMELEN / 2);
            }
            else
            {
                for (count = FMOD_strlen(name_or_data); count >= 0; count--)
                {
                    if (name_or_data[count] == '\\' || name_or_data[count] == '/')
                    {
                        count++;
                        break;
                    }
                }
                if (count < 0)
                {
                    count = 0;
                }

                FMOD_strncpy(soundi->mName, (char *)name_or_data + count, FMOD_STRING_MAXNAMELEN);
            }
        }
    }

    if (!calledfromasync)
    {
        soundi->mOpenState = FMOD_OPENSTATE_READY;
    }

    soundi->mMemoryUsed += memcounter.getMemoryUsed();

    return FMOD_OK;
}



/*
    ====================================================================================

    PUBLIC MEMBER FUNCTIONS

    ====================================================================================
*/

#ifndef FMOD_STATICFORPLUGINS

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 06b9eea3 auto
// OFFSET: 0x008D35C0
FMOD_RESULT SystemI::setUpPlugins()
{
    FMOD_RESULT result;

    mPluginFactory = FMOD_Object_Alloc(FMOD::PluginFactory);
    if (!mPluginFactory)
    {
        return FMOD_ERR_MEMORY;
    }

    result = mPluginFactory->setSystem(this);
    if (result != FMOD_OK)
    {
        return result;
    }

    mPluginFactory->setPluginPath(mPluginPath);

    FMOD_OS_Output_Register(mPluginFactory);

    #ifdef FMOD_SUPPORT_WAVWRITER
    mPluginFactory->registerOutput(FMOD::OutputWavWriter::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_WAVWRITER_NRT
    mPluginFactory->registerOutput(FMOD::OutputWavWriter_NRT::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_NOSOUND
    mPluginFactory->registerOutput(FMOD::OutputNoSound::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_NOSOUND_NRT
    mPluginFactory->registerOutput(FMOD::OutputNoSound_NRT::getDescriptionEx());
    #endif

    #ifdef FMOD_SUPPORT_TAGS
    mPluginFactory->registerCodec(CodecTag::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_CDDA
    mPluginFactory->registerCodec(CodecCDDA::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_FSB
    mPluginFactory->registerCodec(CodecFSB::getDescriptionEx(), 0, &mFSBPluginHandle);
    #endif
    #ifdef FMOD_SUPPORT_VAG
    mPluginFactory->registerCodec(CodecVAG::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_WAV
    mPluginFactory->registerCodec(CodecWav::getDescriptionEx(), 0, &mWAVPluginHandle);
    #endif
    #ifdef FMOD_SUPPORT_OGGVORBIS
    mPluginFactory->registerCodec(CodecOggVorbis::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_AIFF
    mPluginFactory->registerCodec(CodecAIFF::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_FLAC
    mPluginFactory->registerCodec(CodecFLAC::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_MOD
    mPluginFactory->registerCodec(CodecMOD::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_S3M
    mPluginFactory->registerCodec(CodecS3M::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_XM
    mPluginFactory->registerCodec(CodecXM::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_IT
    mPluginFactory->registerCodec(CodecIT::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_MIDI
    mPluginFactory->registerCodec(CodecMIDI::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_DLS
    mPluginFactory->registerCodec(CodecDLS::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_ASF
    mPluginFactory->registerCodec(CodecASF::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_PLAYLIST
    mPluginFactory->registerCodec(CodecPlaylist::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_MPEG
    mPluginFactory->registerCodec(CodecMPEG::getDescriptionEx(), 0, &mMPEGPluginHandle);
    #endif
    #ifdef FMOD_SUPPORT_RAW
    mPluginFactory->registerCodec(CodecRaw::getDescriptionEx());
    #endif

    #ifdef FMOD_SUPPORT_OSCILLATOR
    mPluginFactory->registerDSP(DSPOscillator::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_LOWPASS
    mPluginFactory->registerDSP(DSPLowPass::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_LOWPASS2
    mPluginFactory->registerDSP(DSPLowPass2::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_LOWPASS_SIMPLE
    mPluginFactory->registerDSP(DSPLowPassSimple::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_HIGHPASS
    mPluginFactory->registerDSP(DSPHighPass::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_ECHO
    mPluginFactory->registerDSP(DSPEcho::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_FLANGE
    mPluginFactory->registerDSP(DSPFlange::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_DISTORTION
    mPluginFactory->registerDSP(DSPDistortion::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_NORMALIZE
    mPluginFactory->registerDSP(DSPNormalize::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_PARAMEQ
    mPluginFactory->registerDSP(DSPParamEq::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_PITCHSHIFT
    mPluginFactory->registerDSP(DSPPitchShift::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_CHORUS
    mPluginFactory->registerDSP(DSPChorus::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_FREEVERB
    mPluginFactory->registerDSP(DSPReverb::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_ITECHO
    mPluginFactory->registerDSP(DSPITEcho::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_COMPRESSOR
    mPluginFactory->registerDSP(DSPCompressor::getDescriptionEx());
    #endif
    #ifdef FMOD_SUPPORT_SFXREVERB
    mPluginFactory->registerDSP(DSPSfxReverb::getDescriptionEx());
    #endif

    #ifdef FMOD_SUPPORT_USERCODEC
    mPluginFactory->registerCodec(CodecUser::getDescriptionEx());
    #endif

    mPluginsLoaded = true;

    return FMOD_OK;
}

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 8df02627 manual
// OFFSET: 0x008D3980
FMOD_RESULT SystemI::sortSpeakerList()
{
    int count, count2;
    bool used[FMOD_SPEAKER_MAX];
    int outputchannels;

    if (mSpeakerMode == FMOD_SPEAKERMODE_RAW)
    {
        return FMOD_OK;
    }

    for (count = 0; count < FMOD_SPEAKER_MAX; count++)
    {
        mSpeakerList[count] =0;
    }
    FMOD_memset(used, 0, sizeof(bool) * FMOD_SPEAKER_MAX);

    outputchannels = mMaxOutputChannels;
    if (mSpeakerMode == FMOD_SPEAKERMODE_QUAD || mSpeakerMode == FMOD_SPEAKERMODE_PROLOGIC)
    {
        outputchannels = 6;
    }

    for (count = 0; count < outputchannels; count++)
    {
        int lowest = 361;

        for (count2 = 0; count2 < outputchannels; count2++)
        {
            if (mSpeaker[count2].mSpeaker == FMOD_SPEAKER_LOW_FREQUENCY)
            {
                continue;
            }
            if (!mSpeaker[count2].mActive)
            {
                continue;
            }
            if (mSpeakerMode == FMOD_SPEAKERMODE_QUAD && mSpeaker[count2].mSpeaker == FMOD_SPEAKER_FRONT_CENTER)
            {
                continue;
            }

            if (mSpeaker[count2].mXZAngle < lowest && !used[count2])
            {
                lowest = mSpeaker[count2].mXZAngle;
                mSpeakerList[count] = &mSpeaker[count2];
            }
        }

        if (mSpeakerList[count])
        {
            used[mSpeakerList[count]->mSpeaker] = true;
        }
    }

    return FMOD_OK;
}



#ifdef FMOD_SUPPORT_DSPCODEC
/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: cc61f75b auto
// OFFSET: 0x008D3A70
FMOD_RESULT SystemI::allocDSPCodec(FMOD_SOUND_FORMAT format, DSPCodec **dsp)
{
    if (0)
    {
    }
#ifdef FMOD_SUPPORT_MPEG
    else if (format == FMOD_SOUND_FORMAT_MPEG)
    {
        return mDSPCodecPool_MPEG.alloc(dsp);
    }
#endif
#ifdef FMOD_SUPPORT_IMAADPCM
    else if (format == FMOD_SOUND_FORMAT_IMAADPCM)
    {
        return mDSPCodecPool_ADPCM.alloc(dsp);
    }
#endif

    return FMOD_ERR_FORMAT;
}
#endif


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 56440937 manual
// OFFSET: 0x008D8A10
SystemI::SystemI()
{
    mInitialized                = false;
    mPluginsLoaded              = false;
    mOutputType                 = FMOD_OUTPUTTYPE_AUTODETECT;
    mOutput                     = 0;
#ifdef FMOD_SUPPORT_SOFTWARE
    mSoftware                   = 0;
#endif
    mEmulated                   = 0;
    mMainThreadID               = 0;
    mChannel                    = 0;
    mPluginFactory              = 0;
    mLastTimeStamp              = 0;
    mStreamFileBufferSize       = 16*1024;
    mStreamFileBufferSizeType   = FMOD_TIMEUNIT_RAWBYTES;
    mUserData                   = 0;
    mNumSoftwareChannels        = 64;
    mMinHardwareChannels2D      = 0;
    mMaxHardwareChannels2D      = 1000;
    mMinHardwareChannels3D      = 0;
    mMaxHardwareChannels3D      = 1000;
    mDSPBlockSize               = 1024;
    mDSPBufferSize              = mDSPBlockSize * 4;
#ifdef FMOD_SUPPORT_SOFTWARE
    mDSPTempBuff[0]             = 0;
    mDSPTempBuff[1]             = 0;
    mDSPTempBuffMem[0]          = 0;
    mDSPTempBuffMem[1]          = 0;
    mDSPTempBuffIndex           = 0;
    FMOD_memset(mDSPMixBuff, 0, sizeof(mDSPMixBuff));
#endif
    mMaxInputChannels           = DSP_DEFAULTLEVELS_IN;
    mMaxOutputChannels          = 0;

    set3DSpeakerPosition(FMOD_SPEAKER_FRONT_LEFT,    -1.0f,  1.0f);
    set3DSpeakerPosition(FMOD_SPEAKER_FRONT_RIGHT,    1.0f,  1.0f);
    set3DSpeakerPosition(FMOD_SPEAKER_FRONT_CENTER,   0.0f,  1.0f);
    set3DSpeakerPosition(FMOD_SPEAKER_LOW_FREQUENCY,  0.0f,  0.0f);
    set3DSpeakerPosition(FMOD_SPEAKER_BACK_LEFT,     -1.0f, -1.0f);
    set3DSpeakerPosition(FMOD_SPEAKER_BACK_RIGHT,     1.0f, -1.0f);
    set3DSpeakerPosition(FMOD_SPEAKER_SIDE_LEFT,     -1.0f,  0.0f);
    set3DSpeakerPosition(FMOD_SPEAKER_SIDE_RIGHT,     1.0f,  0.0f);

    mOutputFormat               = FMOD_SOUND_FORMAT_PCM16;
    setSpeakerMode(FMOD_SPEAKERMODE_STEREO);

    mOutputRate                 = 48000;
    mOutputHandle               = 0;
    mSelectedDriver             = 0;
    mRecordDriver               = 0;
    mResampleMethod             = FMOD_DSP_RESAMPLER_LINEAR;
    mNumListeners               = 1;
	mDistanceScale              = 1;
	mRolloffScale               = 1;
	mDopplerScale               = 1;

#ifdef FMOD_SUPPORT_MULTIREVERB
    mReverb3DActive           = false;
    FMOD_REVERB_PROPERTIES prop = FMOD_PRESET_OFF;
    setReverbAmbientProperties(&prop);
#endif

#ifdef FMOD_SUPPORT_GEOMETRY
 	mGeometryList         = 0;
    mGeometryMgr.mSystem = this;
#endif

    FMOD_memset(mPluginPath, 0, FMOD_STRING_MAXPATHLEN);

    mAdvancedSettings.maxXMAcodecs            = 0;
    mAdvancedSettings.maxADPCMcodecs          = 0;
    mAdvancedSettings.maxMPEGcodecs           = 0;
    mAdvancedSettings.maxPCMcodecs            = 0;
    mAdvancedSettings.max3DReverbDSPs         = FMOD_ADVANCEDSETTINGS_MAX3DREVERBDSPS;
    mAdvancedSettings.HRTFMinAngle            = 180;
    mAdvancedSettings.HRTFMaxAngle            = 360;
    mAdvancedSettings.HRTFFreq                = 4000;
    mAdvancedSettings.vol0virtualvol          = 0.0f;
    mAdvancedSettings.eventqueuesize          = 32;

    mMPEGPluginHandle = 0xFFFFFFFF;
    mFSBPluginHandle = 0xFFFFFFFF;
    mWAVPluginHandle = 0xFFFFFFFF;

#ifdef FMOD_SUPPORT_DSPCODEC
    #ifdef FMOD_SUPPORT_MPEG
    mDSPCodecPool_MPEG.mSystem = this;
    #endif
    #ifdef FMOD_SUPPORT_IMAADPCM
    mDSPCodecPool_ADPCM.mSystem = this;
    #endif
#endif

#ifdef FMOD_SUPPORT_ASIO
    mASIOSpeakerList[0]  = FMOD_SPEAKER_FRONT_LEFT;
    mASIOSpeakerList[1]  = FMOD_SPEAKER_FRONT_RIGHT;
    mASIOSpeakerList[2]  = FMOD_SPEAKER_FRONT_CENTER;
    mASIOSpeakerList[3]  = FMOD_SPEAKER_LOW_FREQUENCY;
    mASIOSpeakerList[4]  = FMOD_SPEAKER_BACK_LEFT;
    mASIOSpeakerList[5]  = FMOD_SPEAKER_BACK_RIGHT;
    mASIOSpeakerList[6]  = FMOD_SPEAKER_SIDE_LEFT;
    mASIOSpeakerList[7]  = FMOD_SPEAKER_SIDE_RIGHT;
    mASIOSpeakerList[8]  = FMOD_SPEAKER_FRONT_LEFT;
    mASIOSpeakerList[9]  = FMOD_SPEAKER_FRONT_RIGHT;
    mASIOSpeakerList[10] = FMOD_SPEAKER_FRONT_LEFT;
    mASIOSpeakerList[11] = FMOD_SPEAKER_FRONT_RIGHT;
    mASIOSpeakerList[12] = FMOD_SPEAKER_FRONT_LEFT;
    mASIOSpeakerList[13] = FMOD_SPEAKER_FRONT_RIGHT;
    mASIOSpeakerList[14] = FMOD_SPEAKER_FRONT_LEFT;
    mASIOSpeakerList[15] = FMOD_SPEAKER_FRONT_RIGHT;
#endif
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 8cf2ae70 auto
// OFFSET: 0x008D9A30
FMOD_RESULT SystemI::release()
{
    FMOD_RESULT result;

    if (mInitialized)
    {
        result = closeEx(false);
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    if (mPluginFactory)
    {
        mPluginFactory->release();
        mPluginFactory = 0;
    }

    removeNode();

    FMOD_Memory_Free(this);

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 476d4a43 auto
// OFFSET: 0x008D3AB0
FMOD_RESULT SystemI::setOutput(FMOD_OUTPUTTYPE outputtype)
{
    FMOD_RESULT result = FMOD_OK;
    int count, numoutputs;

    if (mInitialized)
	{
		return FMOD_ERR_INITIALIZED;
	}

    if (mOutput)
    {
        if (outputtype == mOutputType)
        {
            return FMOD_OK;
        }

        FMOD_Memory_Free(mOutput);
    }

    if (!mPluginsLoaded)
    {
        result = setUpPlugins();
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    result = mPluginFactory->getNumOutputs(&numoutputs);
    if (result != FMOD_OK)
    {
        return result;
    }

    if (outputtype == FMOD_OUTPUTTYPE_AUTODETECT)
    {
        FMOD_OS_Output_GetDefault(&outputtype);
    }

    for (count = 0; count < numoutputs; count++)
    {
        FMOD_OUTPUT_DESCRIPTION_EX *outputdesc = 0;

        result = mPluginFactory->getOutput(count, &outputdesc);
        if (result != FMOD_OK)
        {
            continue;
        }

        if (outputdesc->mType == outputtype)
        {
            result = mPluginFactory->createOutput(outputdesc, &mOutput);
            if (result != FMOD_OK)
            {
                return result;
            }

            mOutputHandle = count;
            mOutputType = mOutput->mDescription.mType;

            return FMOD_OK;
        }
    }

    return FMOD_ERR_PLUGIN_MISSING;
}

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: ef8dd8d0 auto
// OFFSET: 0x008D3BC0
FMOD_RESULT SystemI::getOutput(FMOD_OUTPUTTYPE *output)
{
    if (!output)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *output = mOutputType;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 30a8e5cd auto
// OFFSET: 0x008D3BF0
FMOD_RESULT SystemI::setSoftwareFormat(int samplerate, FMOD_SOUND_FORMAT format, int numoutputchannels, int maxinputchannels, FMOD_DSP_RESAMPLER resamplermethod)
{
	if (mInitialized)
	{
		return FMOD_ERR_INITIALIZED;
	}

    if (samplerate < 8000)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    mOutputRate     = samplerate;
    mOutputFormat   = format;
    mResampleMethod = resamplermethod;

    if (numoutputchannels)
    {
        mMaxOutputChannels = numoutputchannels;
        mSpeakerMode       = FMOD_SPEAKERMODE_RAW;
    }

    if (maxinputchannels > 0)
    {
        mMaxInputChannels = maxinputchannels;
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: a40b2aad auto
// OFFSET: 0x008D3C60
FMOD_RESULT SystemI::getNumDrivers(int *numdrivers)
{
    FMOD_RESULT result;

    if (!numdrivers)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    
    if (!mInitialized)
    {
        result = setOutput(mOutputType);
        if (result != FMOD_OK)
        {
            *numdrivers = 0;
            return result;
        }
    }

    if (mOutput->mDescription.getnumdrivers)
    {
#ifdef FMOD_SUPPORT_SOFTWARE
        mOutput->readfrommixer = Output::mixCallback;   /* Reset 'read only' variable in FMOD_OUTPUT_STATE in case the user changed it. */
#else
        mOutput->readfrommixer = 0;
#endif

        return mOutput->mDescription.getnumdrivers(mOutput, numdrivers);
    }

    *numdrivers = 0;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 24ca4efe auto
// OFFSET: 0x008D3CE0
FMOD_RESULT SystemI::getDriverName(int id, char *name, int namelen)
{
    FMOD_RESULT result;
    int numdrivers;

    result = getNumDrivers(&numdrivers);
    if (result != FMOD_OK)
    {
        return result;
    }

    if (id < 0 || id >= numdrivers)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (!mInitialized)
    {
        result = setOutput(mOutputType);
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    if (mOutput->mDescription.getdrivername)
    {
#ifdef FMOD_SUPPORT_SOFTWARE
        mOutput->readfrommixer = Output::mixCallback;   /* Reset 'read only' variable in FMOD_OUTPUT_STATE in case the user changed it. */
#else
        mOutput->readfrommixer = 0;
#endif

        return mOutput->mDescription.getdrivername(mOutput, id, name, namelen);
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: de328140 manual
// OFFSET: 0x008D3D70
FMOD_RESULT SystemI::getDriverCaps(int id, FMOD_CAPS *caps, int *minfrequency, int *maxfrequency, FMOD_SPEAKERMODE *controlpanelspeakermode)
{
    FMOD_RESULT      result;
    FMOD_CAPS        lcaps;
    FMOD_SPEAKERMODE lcontrolpanelspeakermode;
    int              lminfrequency;
    int              lmaxfrequency;
    int              lnum2dchannels;
    int              lnum3dchannels;
    int              ltotalchannels;
    int              numdrivers;

    if (mInitialized)
    {
        return FMOD_ERR_INITIALIZED;
    }

    result = getNumDrivers(&numdrivers);
    if (result != FMOD_OK)
    {
        return result;
    }

    if (id < 0 || id >= numdrivers)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (!mInitialized)
    {
        result = setOutput(mOutputType);
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    lcaps = FMOD_CAPS_NONE;
    lminfrequency = 0;
    lmaxfrequency = 0;
    lnum2dchannels = 0;
    lnum3dchannels = 0;
    ltotalchannels = 0;
    lcontrolpanelspeakermode = FMOD_SPEAKERMODE_STEREO;

    if (mOutput->mDescription.getdrivercapsex2)
    {
#ifdef FMOD_SUPPORT_SOFTWARE
        mOutput->readfrommixer = Output::mixCallback;   /* Reset 'read only' variable in FMOD_OUTPUT_STATE in case the user changed it. */
#else
        mOutput->readfrommixer = 0;
#endif
        result = mOutput->mDescription.getdrivercapsex2(mOutput, id, &lcaps, &lminfrequency, &lmaxfrequency, &lcontrolpanelspeakermode, &lnum2dchannels, &lnum3dchannels, &ltotalchannels);
        if (result != FMOD_OK)
        {
            return result;
        }
    }
    if (mOutput->mDescription.getdrivercapsex)
    {
#ifdef FMOD_SUPPORT_SOFTWARE
        mOutput->readfrommixer = Output::mixCallback;   /* Reset 'read only' variable in FMOD_OUTPUT_STATE in case the user changed it. */
#else
        mOutput->readfrommixer = 0;
#endif
        result = mOutput->mDescription.getdrivercapsex(mOutput, id, &lcaps, &lminfrequency, &lmaxfrequency, &lcontrolpanelspeakermode);
        if (result != FMOD_OK)
        {
            return result;
        }
    }
    else if (mOutput->mDescription.getdrivercaps)
    {
#ifdef FMOD_SUPPORT_SOFTWARE
        mOutput->readfrommixer = Output::mixCallback;   /* Reset 'read only' variable in FMOD_OUTPUT_STATE in case the user changed it. */
#else
        mOutput->readfrommixer = 0;
#endif
        result = mOutput->mDescription.getdrivercaps(mOutput, id, &lcaps);
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    mOutput->mNum2DChannelsFromCaps = lnum2dchannels;
    mOutput->mNum3DChannelsFromCaps = lnum3dchannels;
    mOutput->mTotalChannelsFromCaps = ltotalchannels;

    if (caps)
    {
        *caps = lcaps;
    }
    if (minfrequency)
    {
        *minfrequency = lminfrequency;
    }
    if (maxfrequency)
    {
        *maxfrequency = lmaxfrequency;
    }
    if (controlpanelspeakermode)
    {
        *controlpanelspeakermode = lcontrolpanelspeakermode;
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: aba9797d auto
// OFFSET: 0x008D3F30
FMOD_RESULT SystemI::setDriver(int driver)
{
    FMOD_RESULT        result       = FMOD_OK;
    int                numdrivers   = 0;

    if (mInitialized)
	{
		return FMOD_ERR_INITIALIZED;
	}

    result = getNumDrivers(&numdrivers);
    if (result != FMOD_OK)
    {
        return result;
    }

    if (driver < -1 || driver >= numdrivers)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (driver == -1)
    {
        driver = 0;
    }

    mSelectedDriver = driver;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getDriver(int *driver)
{
    if (!driver)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *driver = mSelectedDriver;

    return FMOD_OK;
}




/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 828c662e auto
// OFFSET: 0x008D3F90
FMOD_RESULT SystemI::setHardwareChannels(int min2d, int max2d, int min3d, int max3d)
{
	if (mInitialized)
	{
		return FMOD_ERR_INITIALIZED;
	}

    if (min2d < 0 || max2d < 0 || min3d < 0 || max3d < 0)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    mMinHardwareChannels2D = min2d;
    mMaxHardwareChannels2D = max2d;
    mMinHardwareChannels3D = min3d;
    mMaxHardwareChannels3D = max3d;

    return FMOD_OK;
}

#endif

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 76b9bcda auto
// OFFSET: 0x008D3FF0
FMOD_RESULT SystemI::getHardwareChannels(int *num2d, int *num3d, int *total)
{
    int numhw3d = 0, numhw2d = 0, numhwtotal = 0;

#ifndef FMOD_STATICFORPLUGINS

    if (mInitialized)
    {
        if (mOutput)
        {
            FMOD_RESULT result;
        
            if (mOutput->mChannelPool)
            {
                result = mOutput->mChannelPool->getNumChannels(&numhw2d);
                if (result != FMOD_OK)
                {
                    return result;
                }
            }

            if (mOutput->mChannelPool3D)
            {
                result = mOutput->mChannelPool3D->getNumChannels(&numhw3d);
                if (result != FMOD_OK)
                {
                    return result;
                }
            }
        }
        numhwtotal = numhw3d + numhw2d;
    }
    else
    {
        FMOD_RESULT result;
        result = setOutput(mOutputType);
        if (result != FMOD_OK)
        {
            return result;
        }

        numhw2d    = mOutput->mNum2DChannelsFromCaps;
        numhw3d    = mOutput->mNum3DChannelsFromCaps;
        numhwtotal = mOutput->mTotalChannelsFromCaps;
    }
#endif

    if (num3d)
    {
        *num3d = numhw3d;
    }

    if (num2d)
    {
        *num2d = numhw2d;
    }

    if (total)
    {
        *total = numhwtotal;
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: dd7ee6ee auto
// OFFSET: 0x008D40A0
FMOD_RESULT SystemI::setSoftwareChannels(int numsoftwarechannels)
{
    if (numsoftwarechannels < 0)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (mInitialized)
    {
        return FMOD_ERR_INITIALIZED;
    }

    mNumSoftwareChannels = numsoftwarechannels;

    return FMOD_OK;
}



/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getSoftwareChannels(int *numsoftwarechannels)
{
    if (!numsoftwarechannels)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *numsoftwarechannels = mNumSoftwareChannels;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 61c61349 auto
// OFFSET: 0x008D40D0
FMOD_RESULT SystemI::setDSPBufferSize(unsigned int bufferlength, int numbuffers)
{
	if (mInitialized)
	{
		return FMOD_ERR_INITIALIZED;
	}

    #if defined(PLATFORM_PS3) || defined(PLATFORM_WINDOWS_PS3MODE)

    return FMOD_ERR_UNSUPPORTED;

    #else

    if (bufferlength < 1 || numbuffers < 2)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    mDSPBlockSize  = bufferlength;
    mDSPBufferSize = bufferlength * numbuffers;

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

	[SEE_ALSO]
]
*/
// VERIFY: 17c82fc0 auto
// OFFSET: 0x008D4110
FMOD_RESULT SystemI::getDSPBufferSize(unsigned int *bufferlength, int *numbuffers)
{
    if (bufferlength)
    {
        *bufferlength = mDSPBlockSize;
    }
    if (numbuffers)
    {
        *numbuffers = mDSPBufferSize / mDSPBlockSize;
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::setFileSystem(FMOD_FILE_OPENCALLBACK useropen, FMOD_FILE_CLOSECALLBACK userclose, FMOD_FILE_READCALLBACK userread, FMOD_FILE_SEEKCALLBACK userseek, int blockalign)
{
    UserFile::gOpenCallback  = useropen;
    UserFile::gCloseCallback = userclose;
    UserFile::gReadCallback  = userread;
    UserFile::gSeekCallback  = userseek;

    File::gUsesUserCallbacks = (useropen && userclose && userread && userseek);

    if (blockalign >= 0)
    {
        File::gBufferSize = blockalign;
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::attachFileSystem(FMOD_FILE_OPENCALLBACK useropen, FMOD_FILE_CLOSECALLBACK userclose, FMOD_FILE_READCALLBACK userread, FMOD_FILE_SEEKCALLBACK userseek)
{
    mOpenCallback  = useropen;
    mCloseCallback = userclose;
    mReadCallback  = userread;
    mSeekCallback  = userseek;

    return FMOD_OK;
}

#ifndef FMOD_STATICFORPLUGINS


/*
[
	[DESCRIPTION]
    Sets advanced features like configuring memory and cpu usage for FMOD_CREATEREALTIMESAMPLE usage.

	[PARAMETERS]
    'settings'      Pointer to FMOD_ADVANCEDSETTINGS structure.
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
    FMOD_ADVANCEDSETTINGS
    SystemI::getAdvancedSettings
]
*/
FMOD_RESULT SystemI::setAdvancedSettings(FMOD_ADVANCEDSETTINGS *settings)
{
    int memberoffset;

    if (!settings)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    memberoffset = (int)((FMOD_UINT_NATIVE)&settings->maxXMAcodecs - (FMOD_UINT_NATIVE)settings);
    
    if (settings->cbsize <= memberoffset)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (settings->maxADPCMcodecs < 0 || settings->maxADPCMcodecs > 65535)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    if (settings->maxMPEGcodecs < 0 || settings->maxMPEGcodecs > 65535)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    if (settings->maxXMAcodecs < 0 || settings->maxXMAcodecs > 65535)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    if (settings->maxPCMcodecs < 0 || settings->maxPCMcodecs > 65535)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    {
        FMOD_RESULT result;

        result = FMOD_CHECKFLOAT(settings->HRTFMinAngle);
        if (result != FMOD_OK)
        {
            return result;
        }
        result = FMOD_CHECKFLOAT(settings->HRTFMaxAngle);
        if (result != FMOD_OK)
        {
            return result;
        }
        result = FMOD_CHECKFLOAT(settings->HRTFFreq);
        if (result != FMOD_OK)
        {
            return result;
        }
        result = FMOD_CHECKFLOAT(settings->vol0virtualvol);
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    if (settings->HRTFMinAngle < 0 || settings->HRTFMinAngle > 360)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    if (settings->HRTFMaxAngle < settings->HRTFMinAngle || settings->HRTFMaxAngle > 360.0f)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    if (settings->HRTFFreq < 0.0001f)
    {
        settings->HRTFFreq = mAdvancedSettings.HRTFFreq;
    }
    else if (settings->HRTFFreq < 10.0f || settings->HRTFFreq > 22050.0f)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    if (settings->vol0virtualvol < 0.0f)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    if (settings->eventqueuesize < 0 || settings->eventqueuesize > 65535)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    if (settings->ASIONumChannels > DSP_MAXLEVELS_OUT || settings->ASIONumChannels < 0)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    FMOD_memcpy(&mAdvancedSettings, settings, settings->cbsize);

#ifdef FMOD_SUPPORT_ASIO
    if (settings->ASIOSpeakerList)
    {
        int count;

        for (count = 0; count < settings->ASIONumChannels; count++)
        {
            mASIOSpeakerList[count] = settings->ASIOSpeakerList[count];
        }
    }
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

	[SEE_ALSO]
    FMOD_ADVANCEDSETTINGS
    SystemI::setAdvancedSettings
]
*/
FMOD_RESULT SystemI::getAdvancedSettings(FMOD_ADVANCEDSETTINGS *settings)
{
    int           usercbsize;
    int           asioNumChannels;
    FMOD_SPEAKER *speakerlist;

    if (!settings)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    if (settings->ASIONumChannels > DSP_MAXLEVELS_OUT || settings->ASIONumChannels < 0)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    
    usercbsize = settings->cbsize;
    speakerlist = settings->ASIOSpeakerList;
    asioNumChannels = settings->ASIONumChannels;

    FMOD_memcpy(settings, &mAdvancedSettings, usercbsize);

    settings->cbsize = usercbsize;
    settings->ASIOSpeakerList = speakerlist;
	#ifdef FMOD_SUPPORT_ASIO
    if (settings->ASIONumChannels > DSP_MAXLEVELS_OUT)
    {
        settings->ASIONumChannels = DSP_MAXLEVELS_OUT;
    }
    if (settings->ASIOSpeakerList)
    {
        int count;

        for (count = 0; count < asioNumChannels; count++)
        {
            settings->ASIOSpeakerList[count] = mASIOSpeakerList[count];
        }
    }
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

	[SEE_ALSO]
]
*/
// VERIFY: d4ac9675 auto
// OFFSET: 0x008D5A50
FMOD_RESULT SystemI::setSpeakerMode(FMOD_SPEAKERMODE speakermode)
{
	if (mInitialized)
	{
		return FMOD_ERR_INITIALIZED;
	}

    mSpeakerMode = speakermode;

    switch (mSpeakerMode)
    {
        case FMOD_SPEAKERMODE_RAW:
        {
            return FMOD_OK;
        }
        case FMOD_SPEAKERMODE_MONO:
        {
            mMaxOutputChannels = 1;
            break;
        }
        case FMOD_SPEAKERMODE_STEREO:
        {
            mMaxOutputChannels = 2;
            set3DSpeakerPosition(FMOD_SPEAKER_FRONT_LEFT,    -1.0f,  0.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_FRONT_RIGHT,    1.0f,  0.0f);
            break;
        }
        case FMOD_SPEAKERMODE_QUAD:
        {
            mMaxOutputChannels = 4;
            set3DSpeakerPosition(FMOD_SPEAKER_FRONT_LEFT,    -1.0f,  1.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_FRONT_RIGHT,    1.0f,  1.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_BACK_LEFT,     -1.0f, -1.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_BACK_RIGHT,     1.0f, -1.0f);
            break;
        }
        case FMOD_SPEAKERMODE_SURROUND:
        {
            mMaxOutputChannels = 5;
            set3DSpeakerPosition(FMOD_SPEAKER_FRONT_LEFT,    -1.0f,  1.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_FRONT_RIGHT,    1.0f,  1.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_FRONT_CENTER,   0.0f,  1.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_BACK_LEFT,     -1.0f, -1.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_BACK_RIGHT,     1.0f, -1.0f);
            break;
        }
        case FMOD_SPEAKERMODE_5POINT1:
        {
            mMaxOutputChannels = 6;
            set3DSpeakerPosition(FMOD_SPEAKER_FRONT_LEFT,    -1.0f,  1.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_FRONT_RIGHT,    1.0f,  1.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_FRONT_CENTER,   0.0f,  1.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_BACK_LEFT,     -1.0f, -1.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_BACK_RIGHT,     1.0f, -1.0f);
            break;
        }
        case FMOD_SPEAKERMODE_7POINT1:
        {
            mMaxOutputChannels = 8;
            set3DSpeakerPosition(FMOD_SPEAKER_FRONT_LEFT,    -1.0f,  1.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_FRONT_RIGHT,    1.0f,  1.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_FRONT_CENTER,   0.0f,  1.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_BACK_LEFT,     -1.0f, -1.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_BACK_RIGHT,     1.0f, -1.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_SIDE_LEFT,     -1.0f,  0.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_SIDE_RIGHT,     1.0f,  0.0f);
            break;
        }
        case FMOD_SPEAKERMODE_PROLOGIC:
        {
            mMaxOutputChannels = 2;
            set3DSpeakerPosition(FMOD_SPEAKER_FRONT_LEFT,    -1.0f,  1.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_FRONT_RIGHT,    1.0f,  1.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_FRONT_CENTER,   0.0f,  1.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_BACK_LEFT,     -1.0f, -1.0f);
            set3DSpeakerPosition(FMOD_SPEAKER_BACK_RIGHT,     1.0f, -1.0f);
            break;
        }
        default:
        {
            break;
        }
    }

    return sortSpeakerList();
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 5dbd4751 auto
// OFFSET: 0x008D4150
FMOD_RESULT SystemI::setCallback(FMOD_SYSTEM_CALLBACKTYPE type, FMOD_SYSTEM_CALLBACK callback)
{
    if (type < 0 || type >= FMOD_SYSTEM_CALLBACKTYPE_MAX)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    mCallback[type] = callback;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::setPluginPath(const char *path)
{
	if (mInitialized)
	{
		return FMOD_ERR_INITIALIZED;
	}

    if (FMOD_strlen(path) >= FMOD_STRING_MAXPATHLEN)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    FMOD_strncpy(mPluginPath, path, FMOD_STRING_MAXPATHLEN);

    if (mPluginFactory)
    {
        mPluginFactory->setPluginPath(mPluginPath);
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::loadPlugin(const char *filename, FMOD_PLUGINTYPE *plugintype, int *index)
{
#ifdef FMOD_SUPPORT_DLLS
    FMOD_RESULT result;

    if (!mPluginsLoaded)
    {
        result = setUpPlugins();
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    return mPluginFactory->loadPlugin(filename, plugintype, index);
#else
    return FMOD_ERR_UNSUPPORTED;
#endif
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getNumPlugins(FMOD_PLUGINTYPE type, int *numplugins)
{
    FMOD_RESULT result;

    if (!numplugins)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (!mPluginsLoaded)
    {
        result = setUpPlugins();
        if (result != FMOD_OK)
        {
            return result;
        }
    }
            
    switch (type)
    {
        case FMOD_PLUGINTYPE_OUTPUT:
        {
            mPluginFactory->getNumOutputs(numplugins);
            break;
        }
        case FMOD_PLUGINTYPE_CODEC:
        {
            mPluginFactory->getNumCodecs(numplugins);
            break;
        }
        case FMOD_PLUGINTYPE_DSP:
        {
            mPluginFactory->getNumDSPs(numplugins);
            break;
        }
        default:
        {
            return FMOD_ERR_INVALID_PARAM;
        }
    }

    return FMOD_OK;
}



/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getPluginInfo(FMOD_PLUGINTYPE plugintype, int index, char *name, int namelen, unsigned int *version)
{
    FMOD_RESULT result;

    if (!mPluginsLoaded)
    {
        result = setUpPlugins();
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    switch (plugintype)
    {
        case FMOD_PLUGINTYPE_OUTPUT:
        {
            FMOD_OUTPUT_DESCRIPTION_EX *outputdesc;

            result = mPluginFactory->getOutput(index, &outputdesc);
            if (result != FMOD_OK)
            {
                return result;
            }
            if (name)
            {
                FMOD_strncpy(name, outputdesc->name, namelen);
            }
            if (version)
            {
                *version = outputdesc->version;
            }
            break;
        }
        case FMOD_PLUGINTYPE_CODEC:
        {
            FMOD_CODEC_DESCRIPTION_EX *codecdesc;

            result = mPluginFactory->getCodec(index, &codecdesc);
            if (result != FMOD_OK)
            {
                return result;
            }
            if (name)
            {
                FMOD_strncpy(name, codecdesc->name, namelen);
            }
            if (version)
            {
                *version = codecdesc->version;
            }
            break;
        }
        case FMOD_PLUGINTYPE_DSP:
        {
            FMOD_DSP_DESCRIPTION_EX *dspdesc;

            result = mPluginFactory->getDSP(index, &dspdesc);
            if (result != FMOD_OK)
            {
                return result;
            }
            if (name)
            {
                FMOD_strncpy(name, dspdesc->name, namelen);
            }
            if (version)
            {
                *version = dspdesc->version;
            }
            break;
        }
        default:
        {
            return FMOD_ERR_INVALID_PARAM;
        }
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::unloadPlugin(FMOD_PLUGINTYPE plugintype, int index)
{
    FMOD_RESULT result;

    if (!mPluginsLoaded)
    {
        result = setUpPlugins();
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    return mPluginFactory->unloadPlugin(plugintype, index);
}




/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::setOutputByPlugin(int index)
{
    FMOD_RESULT                 result = FMOD_OK;
    FMOD_OUTPUT_DESCRIPTION_EX *outputdesc = 0;

    if (mInitialized)
	{
		return FMOD_ERR_INITIALIZED;
	}

    if (mOutput)
    {
        FMOD_Memory_Free(mOutput);
        mOutput = 0;
    }

    if (!mPluginsLoaded)
    {
        result = setUpPlugins();
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    result = mPluginFactory->getOutput(index, &outputdesc);
    if (result != FMOD_OK)
    {
        return result;
    }

    result = mPluginFactory->createOutput(outputdesc, &mOutput);
    if (result != FMOD_OK)
    {
        return result;
    }

    mOutputHandle = index;
    mOutputType   = mOutput->mDescription.mType;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getOutputByPlugin(int *index)
{
    if (!index)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *index = mOutputHandle;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/

class SystemInitCleanup
{
public:
    SystemInitCleanup()
        : mSystem(0), mSpeakerMode(FMOD_SPEAKERMODE_MAX), 
          mOutputFormat(FMOD_SOUND_FORMAT_MAX), mOutputRate(0),
          mSoundListCrit(0), mFileCrit(0), mAsyncCrit(0),
          mProfilerInitialized(false)
    { }

    void setSystem(SystemI *system) 
    { 
        mSystem = system; 
        mSpeakerMode  = system->mSpeakerMode;
        mOutputFormat = system->mOutputFormat;
        mOutputRate   = system->mOutputRate;
    }
    
    void profilerInitialized()                            { mProfilerInitialized = true; }
    void setSoundListCrit(FMOD_OS_CRITICALSECTION **crit) { mSoundListCrit = crit; }
    void setFileCrit(FMOD_OS_CRITICALSECTION **crit)      { mFileCrit = crit; }
    void setAsyncCrit(FMOD_OS_CRITICALSECTION **crit)     { mAsyncCrit = crit; }

    void cleanup(bool)
    {
        if(mSystem)
        {
            if(mSpeakerMode != FMOD_SPEAKERMODE_MAX)
            {
                mSystem->setSpeakerMode(mSpeakerMode);
            }
            if (mOutputFormat != FMOD_SOUND_FORMAT_MAX)
            {
                mSystem->mOutputFormat = mOutputFormat;
            }
            if (mOutputRate != 0)
            {
                mSystem->mOutputRate = mOutputRate;
            }
            mSystem->close();
            
        }
        if(mAsyncCrit)
        {
            FMOD_OS_CriticalSection_Free(*mAsyncCrit);
            *mAsyncCrit = 0;
        }
        if(mFileCrit)
        {
            FMOD_OS_CriticalSection_Free(*mFileCrit);
            *mFileCrit = 0;
        }
        if(mSoundListCrit)
        {
            FMOD_OS_CriticalSection_Free(*mSoundListCrit);
            *mSoundListCrit = 0;
        }
    }    

private:
    SystemI *mSystem;
    FMOD_SPEAKERMODE mSpeakerMode;
    FMOD_SOUND_FORMAT mOutputFormat;
    int mOutputRate;
    FMOD_OS_CRITICALSECTION **mSoundListCrit;
    FMOD_OS_CRITICALSECTION **mFileCrit;
    FMOD_OS_CRITICALSECTION **mAsyncCrit;
    bool mProfilerInitialized;
};


// VERIFY: e2a770ab manual
// OFFSET: 0x008D9AA0
FMOD_RESULT SystemI::init(int maxchannels, FMOD_INITFLAGS flags, void *extradriverdata)
{
    int count;
    FMOD_RESULT result;
    int channellimit = (int)(1 << CHANINDEX_BITS);

    if (maxchannels < 0 || maxchannels >= channellimit)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    result = closeEx(true);
    if (result != FMOD_OK)
    {
        return result;
    }

    mFlags = flags;

    FMOD_OS_Thread_GetCurrentID(&mMainThreadID);

    result = setOutput(mOutputType);
    if (result != FMOD_OK)
    {
        return result;
    }

    if (mOutput->mDescription.initex)
    {
        FMOD_SPEAKERMODE speakermode;

        mOutput->readfrommixer = Output::mixCallback;

        speakermode = mSpeakerMode;

        result = mOutput->mDescription.initex(mOutput, mSelectedDriver, flags, &mOutputRate, mMaxOutputChannels, &mOutputFormat, &speakermode, mDSPBlockSize, mDSPBufferSize / mDSPBlockSize, 0, 0, extradriverdata);
        if (result != FMOD_OK)
        {
            return result;
        }

        if (speakermode != mSpeakerMode)
        {
            setSpeakerMode(speakermode);
        }
    }
    else if (mOutput->mDescription.init)
    {
        mOutput->readfrommixer = Output::mixCallback;

        result = mOutput->mDescription.init(mOutput, mSelectedDriver, flags, &mOutputRate, mMaxOutputChannels, &mOutputFormat, mDSPBlockSize, mDSPBufferSize / mDSPBlockSize, extradriverdata);
        if (result != FMOD_OK)
        {
            return result;
        }
    }

#ifdef FMOD_SUPPORT_SOFTWARE
    if (!(mFlags & FMOD_INIT_SOFTWARE_DISABLE))
    {
        FMOD_DSP_DESCRIPTION    description;
        FMOD_DSP_DESCRIPTION_EX descriptionex;
        FMOD_SOUND_FORMAT       bufferformat;
        int                     bufferchannels;
        FMOD_SPEAKERMODE        speakermode;

        mSoftware = FMOD_Object_Alloc(OutputSoftware);
        if (!mSoftware)
        {
            closeEx(false);
            return FMOD_ERR_MEMORY;
        }

        mSoftware->mSystem = this;

        if (!mDSPCrit)
        {
            result = FMOD_OS_CriticalSection_Create(&mDSPCrit);
            if (result != FMOD_OK)
            {
                return result;
            }
        }

        if (!mDSPLockCrit)
        {
            result = FMOD_OS_CriticalSection_Create(&mDSPLockCrit);
            if (result != FMOD_OK)
            {
                return result;
            }
        }

        if (!mDSPConnectionCrit)
        {
            result = FMOD_OS_CriticalSection_Create(&mDSPConnectionCrit);
            if (result != FMOD_OK)
            {
                return result;
            }
        }

        if (!mDSPRequestCrit)
        {
            result = FMOD_OS_CriticalSection_Create(&mDSPRequestCrit);
            if (result != FMOD_OK)
            {
                return result;
            }
        }

        mConnectionRequestUsedHead.initNode();
        mConnectionRequestFreeHead.initNode();

        for (count = 0; count < FMOD_DSP_CONNECTION_REQUEST_MAX; count++)
        {
            DSPConnectionRequest *request = &mConnectionRequest[count];

            request->initNode();
            request->addBefore(&mConnectionRequestFreeHead);
        }

        result = getSoftwareFormat(0, &bufferformat, &bufferchannels, 0, 0, 0);
        if (result != FMOD_OK)
        {
            return result;
        }

        if (mMaxInputChannels < bufferchannels)
        {
            mMaxInputChannels = bufferchannels;
        }

        for (count = 0; count < 2; count++)
        {
            mDSPTempBuffMem[count] = (float *)FMOD_Memory_Calloc((mDSPBlockSize * (bufferchannels < mMaxInputChannels ? mMaxInputChannels : bufferchannels) * sizeof(float)) + 16);
            if (!mDSPTempBuffMem[count])
            {
                return FMOD_ERR_MEMORY;
            }
            mDSPTempBuff[count] = (float *)FMOD_ALIGNPOINTER(mDSPTempBuffMem[count], 16);
        }

        result = mDSPConnectionPool.init(this, 1 + (mNumSoftwareChannels * 2), bufferchannels < 2 ? 2 : bufferchannels, mMaxInputChannels);
        if (result != FMOD_OK)
        {
            return result;
        }

        FMOD_memset(&descriptionex, 0, sizeof(FMOD_DSP_DESCRIPTION_EX));
        FMOD_strcpy(descriptionex.name, "FMOD SoundCard Unit");
        descriptionex.version      = 0x00010100;
        descriptionex.channels     = bufferchannels;
        descriptionex.create       = 0;
        descriptionex.release      = 0;
        descriptionex.read         = 0;
        descriptionex.setposition  = 0;
        descriptionex.mCategory    = FMOD_DSP_CATEGORY_SOUNDCARD;
        descriptionex.mFormat      = bufferformat;

        result = createDSP(&descriptionex, &mDSPSoundCard);
        if (result != FMOD_OK)
        {
            return result;
        }

        mDSPSoundCard->setActive(true);

        FMOD_memset(&description, 0, sizeof(FMOD_DSP_DESCRIPTION));
        FMOD_strcpy(description.name, "FMOD ChannelGroup Target Unit");
        description.version      = 0x00010100;
        description.channels     = 0;
        description.create       = 0;
        description.release      = 0;
        description.read         = 0;
        description.setposition  = 0;

        result = createDSP(&description, &mDSPChannelGroupTarget);
        if (result != FMOD_OK)
        {
            return result;
        }

        mDSPChannelGroupTarget->setDefaults((float)mOutputRate, -1, -1, -1);
        mDSPChannelGroupTarget->setActive(true);

        result = getSpeakerMode(&speakermode);
        if (result != FMOD_OK)
        {
            return result;
        }

        result = mDSPSoundCard->addInput(mDSPChannelGroupTarget);
        if (result != FMOD_OK)
        {
            return result;
        }
    }
#endif

    result = createChannelGroup("FMOD master group", &mChannelGroup);
    if (result != FMOD_OK)
    {
        return result;
    }

    result = createSoundGroup("FMOD master group", &mSoundGroup);
    if (result != FMOD_OK)
    {
        return result;
    }

#ifdef FMOD_SUPPORT_SOFTWARE
    if (!(mFlags & FMOD_INIT_SOFTWARE_DISABLE))
    {
        result = mSoftware->init(mNumSoftwareChannels);
        if (result != FMOD_OK)
        {
            return result;
        }

        if (mOutput->mDescription.start)
        {
            mOutput->readfrommixer = Output::mixCallback;

            result = mOutput->mDescription.start(mOutput);
            if (result != FMOD_OK)
            {
                return result;
            }
        }
        else if (mOutput->mDescription.polling)
        {
            OutputPolled *outputpolled = SAFE_CAST(OutputPolled, mOutput);

            result = outputpolled->start();
            if (result != FMOD_OK)
            {
                return result;
            }
        }
    }
#endif

    if (maxchannels > 0)
    {
        mEmulated = FMOD_Object_Alloc(OutputEmulated);
        if (!mEmulated)
        {
            closeEx(false);
            return FMOD_ERR_MEMORY;
        }

        mEmulated->mSystem = this;

        result = mEmulated->init(maxchannels);
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    mNumChannels = maxchannels;

    if (maxchannels)
    {
        mChannel = (ChannelI *)FMOD_Memory_Calloc(sizeof(ChannelI) * maxchannels);
        if (!mChannel)
        {
            return FMOD_ERR_MEMORY;
        }

        for (count = 0; count < mNumChannels; count++)
        {
            new (&mChannel[count]) ChannelI(count, this);

            mChannel[count].addAfter(&mChannelFreeListHead);
            mChannel[count].setChannelGroup(mChannelGroup);
        }
    }

    if (!gSoundListCrit)
    {
        result = FMOD_OS_CriticalSection_Create(&gSoundListCrit);
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    if (!mReadDataCrit)
    {
        result = FMOD_OS_CriticalSection_Create(&mReadDataCrit);
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    if (!mMultiSubSampleLockBufferCrit)
    {
        result = FMOD_OS_CriticalSection_Create(&mMultiSubSampleLockBufferCrit);
        if (result != FMOD_OK)
        {
            return result;
        }
    }

#ifdef FMOD_SUPPORT_STREAMING
    if (!mStreamThreadActive && !(mFlags & FMOD_INIT_STREAM_FROM_UPDATE))
    {
        result = FMOD_OS_CriticalSection_Create(&mStreamRealchanCrit);
        if (result != FMOD_OK)
        {
            return result;
        }

        result = FMOD_OS_CriticalSection_Create(&mStreamUpdateCrit);
        if (result != FMOD_OK)
        {
            return result;
        }

        result = FMOD_OS_CriticalSection_Create(&mStreamListCrit);
        if (result != FMOD_OK)
        {
            return result;
        }

        result = mStreamThread.initThread("FMOD stream thread", streamThread, this, STREAM_THREADPRIORITY, 0, STREAM_STACKSIZE, false, 10);
        if (result != FMOD_OK)
        {
            return result;
        }

        mStreamThreadActive = true;
    }
#endif

    if (!File::gFileCrit)
    {
        result = FMOD_OS_CriticalSection_Create(&File::gFileCrit);
        if (result != FMOD_OK)
        {
            return result;
        }
    }

#ifdef FMOD_SUPPORT_DSPCODEC
    if (!(mFlags & FMOD_INIT_SOFTWARE_DISABLE))
    {
        #ifdef FMOD_SUPPORT_IMAADPCM
        if (mAdvancedSettings.maxADPCMcodecs)
        {
            result = mDSPCodecPool_ADPCM.init(FMOD_DSP_CATEGORY_DSPCODECADPCM, 64, mAdvancedSettings.maxADPCMcodecs);
            if (result != FMOD_OK)
            {
                return result;
            }

            for (count = 0; count < mDSPCodecPool_ADPCM.mNumDSPCodecs; count++)
            {
                DSPCodec *dspcodec = SAFE_CAST(DSPCodec, mDSPCodecPool_ADPCM.mPool[count]);
                CodecWav *wav      = (CodecWav *)dspcodec->mCodec;

                wav->mSrcFormat  = &wav->mSrcFormatMemory;
                wav->mReadBuffer = mDSPCodecPool_ADPCM.mReadBuffer;
                wav->mSrcFormat->Format.wFormatTag = WAVE_FORMAT_IMA_ADPCM;
            }
        }
        #endif

        #ifdef FMOD_SUPPORT_MPEG
        if (mAdvancedSettings.maxMPEGcodecs)
        {
            result = mDSPCodecPool_MPEG.init(FMOD_DSP_CATEGORY_DSPCODECMPEG, 1152, mAdvancedSettings.maxMPEGcodecs);
            if (result != FMOD_OK)
            {
                return result;
            }

            for (count = 0; count < mDSPCodecPool_MPEG.mNumDSPCodecs; count++)
            {
                DSPCodec *dspcodec = SAFE_CAST(DSPCodec, mDSPCodecPool_MPEG.mPool[count]);
                CodecMPEG *mpeg    = (CodecMPEG *)dspcodec->mCodec;

                mpeg->mSrcDataOffset    = 0;
                mpeg->mWaveFormatMemory = 0;
                mpeg->reset();
            }
        }
        #endif
    }
#endif

    {
        const FMOD_REVERB_PROPERTIES prop = FMOD_PRESET_OFF;

        result = mReverbGlobal.init(this, false);
        if (result != FMOD_OK)
        {
            return result;
        }

#ifdef FMOD_SUPPORT_REVERB
        result = setReverbProperties(&prop);
        if (result != FMOD_OK)
        {
            return result;
        }
#endif

#ifdef FMOD_SUPPORT_MULTIREVERB
        result = mReverb3D.init(this, false);
        if (result != FMOD_OK)
        {
            return result;
        }

        result = set3DReverbProperties(&prop);
        if (result != FMOD_OK)
        {
            return result;
        }
#endif
    }

    if (mFlags & FMOD_INIT_ENABLE_DSPNET)
    {
        mDSPNet.init();
    }

    mSpeakerLevelsPool.mSystem = this;

    mInitialized = true;

    gGlobal->gSystemInitCount++;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: dc7d2e9c auto
// OFFSET: 0x008D9A20
FMOD_RESULT SystemI::close()
{
    return closeEx(false);
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 8cd16740 manual
// OFFSET: 0x008D9550
FMOD_RESULT SystemI::closeEx(bool calledfrominit)
{
    FMOD_RESULT result;
    int count;

    recordStop();

    if (!calledfrominit)
    {
        for (count = 0; count < mNumChannels; count++)
        {
            Channel *channel;

            if (((System *)this)->getChannel(count, &channel) == FMOD_OK)
            {
                channel->stop();
            }
        }

        update();

    #ifdef FMOD_SUPPORT_STREAMING
        if (mStreamThreadActive)
        {
            mStreamThread.closeThread();
            mStreamThreadActive = false;

            FMOD_OS_CriticalSection_Free(mStreamRealchanCrit);
            mStreamRealchanCrit = 0;
            FMOD_OS_CriticalSection_Free(mStreamUpdateCrit);
            mStreamUpdateCrit = 0;
            FMOD_OS_CriticalSection_Free(mStreamListCrit);
            mStreamListCrit = 0;
        }
    #endif

        if (gGlobal->gSystemInitCount == 1 && mInitialized)
        {
            result = AsyncThread::shutDown();
            if (result != FMOD_OK)
            {
                return result;
            }

            if (gSoundListCrit)
            {
                FMOD_OS_CriticalSection_Free(gSoundListCrit);
                gSoundListCrit = 0;
            }

            result = File::shutDown();
            if (result != FMOD_OK)
            {
                return result;
            }
        }
    }

    if (mChannelGroup)
    {
        result = mChannelGroup->releaseInternal();
        if (result != FMOD_OK)
        {
            return result;
        }
        mChannelGroup = 0;
    }

    while (!mChannelGroupHead.isEmpty())
    {
        ChannelGroupI *channelgroup = (ChannelGroupI *)mChannelGroupHead.getNext();

        result = channelgroup->release();
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    if (mSoundGroup)
    {
        result = mSoundGroup->releaseInternal();
        if (result != FMOD_OK)
        {
            return result;
        }
        mSoundGroup = 0;
    }

    if (mOutput && !calledfrominit)
    {
        if (mOutput->mDescription.stop)
        {
            mOutput->readfrommixer = Output::mixCallback;
            mOutput->mDescription.stop(mOutput);
        }
        else if (mOutput->mDescription.polling)
        {
            OutputPolled *outputpolled = SAFE_CAST(OutputPolled, mOutput);

            outputpolled->stop();
        }

#ifdef FMOD_SUPPORT_SOFTWARE
        if (mDSPChannelGroupTarget)
        {
            mDSPChannelGroupTarget->release();
            mDSPChannelGroupTarget = 0;
        }

        if (mDSPSoundCard)
        {
            mDSPSoundCard->release();
            mDSPSoundCard = 0;
        }
#endif

#ifdef FMOD_SUPPORT_MULTIREVERB
        ReverbI *reverb_c = SAFE_CAST(ReverbI, mReverb3DHead.getNext());
        while (reverb_c != &mReverb3DHead)
        {
            ReverbI *reverb_tmp = reverb_c;

            reverb_c = SAFE_CAST(ReverbI, reverb_c->getNext());
            reverb_tmp->release(true);
        }
        mReverb3D.release(false);
#endif
        mReverbGlobal.release(false);

#ifdef FMOD_SUPPORT_SOFTWARE
        for (count = 0; count < 2; count++)
        {
            if (mDSPTempBuffMem[count])
            {
                FMOD_Memory_Free(mDSPTempBuffMem[count]);
                mDSPTempBuffMem[count] = 0;
                mDSPTempBuff[count] = 0;
            }
        }
#endif

        if (mOutput->mDescription.close)
        {
            mOutput->readfrommixer = Output::mixCallback;
            mOutput->mDescription.close(mOutput);
        }

        mOutput->release();
        mOutput = 0;
    }

#ifdef FMOD_SUPPORT_SOFTWARE
    if (mSoftware)
    {
        mSoftware->release();
        mSoftware = 0;
    }
#endif

    if (mEmulated)
    {
        mEmulated->release();
        mEmulated = 0;
    }

#ifdef FMOD_SUPPORT_DSPCODEC
    #ifdef FMOD_SUPPORT_MPEG
    result = mDSPCodecPool_MPEG.close();
    if (result != FMOD_OK)
    {
        return result;
    }
    #endif
    #ifdef FMOD_SUPPORT_IMAADPCM
    result = mDSPCodecPool_ADPCM.close();
    if (result != FMOD_OK)
    {
        return result;
    }
    #endif
#endif

    if (mChannel)
    {
        for (count = 0; count < mNumChannels; count++)
        {
            if (mChannel[count].mLevels)
            {
                FMOD_Memory_Free(mChannel[count].mLevels);
                mChannel[count].mLevels = 0;
            }
        }

        FMOD_Memory_Free(mChannel);
        mChannel = 0;
    }

    mChannelFreeListHead.initNode();

#ifdef FMOD_SUPPORT_SOFTWARE
    result = mDSPConnectionPool.close();
    if (result != FMOD_OK)
    {
        return result;
    }

    for (count = 0; count < FMOD_DSP_MAXTREEDEPTH; count++)
    {
        if (mDSPMixBuff[count])
        {
            FMOD_Memory_Free(mDSPMixBuff[count]);
            mDSPMixBuff[count] = 0;
        }
    }

    if (mDSPCrit)
    {
        result = FMOD_OS_CriticalSection_Free(mDSPCrit);
        if (result != FMOD_OK)
        {
            return result;
        }
        mDSPCrit = 0;
    }

    if (mDSPLockCrit)
    {
        result = FMOD_OS_CriticalSection_Free(mDSPLockCrit);
        if (result != FMOD_OK)
        {
            return result;
        }
        mDSPLockCrit = 0;
    }

    if (mDSPConnectionCrit)
    {
        result = FMOD_OS_CriticalSection_Free(mDSPConnectionCrit);
        if (result != FMOD_OK)
        {
            return result;
        }
        mDSPConnectionCrit = 0;
    }

    if (mDSPRequestCrit)
    {
        result = FMOD_OS_CriticalSection_Free(mDSPRequestCrit);
        if (result != FMOD_OK)
        {
            return result;
        }
        mDSPRequestCrit = 0;
    }
#endif

    if (mFlags & FMOD_INIT_ENABLE_DSPNET)
    {
        mDSPNet.release();
    }

    if (mMultiSubSampleLockBufferCrit)
    {
        FMOD_OS_CriticalSection_Free(mMultiSubSampleLockBufferCrit);
        mMultiSubSampleLockBufferCrit = 0;
    }

    if (mReadDataCrit)
    {
        FMOD_OS_CriticalSection_Free(mReadDataCrit);
        mReadDataCrit = 0;
    }

    mSpeakerLevelsPool.release();

    if (!calledfrominit && mInitialized)
    {
        gGlobal->gSystemInitCount--;
    }

    mInitialized = false;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 8dde5d74 manual
// OFFSET: 0x008D8FD0
FMOD_RESULT SystemI::update()
{
    FMOD_RESULT  result;
    unsigned int currenttimestamp;
    int          delta, count;
    bool         devicelistchanged;

    if (!mInitialized)
    {
        return FMOD_ERR_UNINITIALIZED;
    }

    mUpdateTimeStamp.stampIn();

    if (!mLastTimeStamp)
    {
        FMOD_OS_Time_GetMs(&mLastTimeStamp);
    }

    FMOD_OS_Time_GetMs(&currenttimestamp);

    if (currenttimestamp >= mLastTimeStamp)
    {
        delta = currenttimestamp - mLastTimeStamp;
    }
    else
    {
        delta = currenttimestamp;
    }

    mLastTimeStamp = currenttimestamp;

    if (mEmulated)
    {
        result = mEmulated->update();
        if (result != FMOD_OK)
        {
            return result;
        }
    }

#ifdef FMOD_SUPPORT_MULTIREVERB
    result = update3DReverbs();
    if (result != FMOD_OK)
    {
        return result;
    }
#endif

    result = updateChannels(delta);
    if (result != FMOD_OK)
    {
        return result;
    }

    result = updateSoundGroups(delta);
    if (result != FMOD_OK)
    {
        return result;
    }

#ifdef FMOD_SUPPORT_SOFTWARE
    if (!mSoftware)
#endif
    {
        FMOD_OS_Time_GetMs(&FMOD::gGlobal->gDSPClockTimeStamp);
        FMOD::gGlobal->gDSPClock.mHi += delta;
    }

    if (mOutput && mOutput->mDescription.update)
    {
        mUpdateTimeStamp.setPaused(true);

        mOutput->readfrommixer = Output::mixCallback;

        result = mOutput->mDescription.update(mOutput);
        if (result != FMOD_OK)
        {
            return result;
        }

        mUpdateTimeStamp.setPaused(false);
    }

    if (mCallback[FMOD_SYSTEM_CALLBACKTYPE_DEVICELISTCHANGED])
    {
        FMOD_OS_CheckDriverList(&devicelistchanged);
        if (devicelistchanged)
        {
            mCallback[FMOD_SYSTEM_CALLBACKTYPE_DEVICELISTCHANGED]((FMOD_SYSTEM *)this, FMOD_SYSTEM_CALLBACKTYPE_DEVICELISTCHANGED, 0, 0);
        }
    }

    for (count = 0; count < mNumListeners; count++)
    {
        mListener[count].mMoved = false;
        mListener[count].mRotated = false;
    }

#ifdef FMOD_SUPPORT_GEOMETRY
	mGeometryMgr.mMoved = false;
#endif

    result = AsyncThread::update();
    if (result != FMOD_OK)
    {
        return result;
    }

    mUpdateTimeStamp.stampOut(95);

#ifdef FMOD_SUPPORT_STREAMING
    if (mFlags & FMOD_INIT_STREAM_FROM_UPDATE)
    {
        updateStreams();
    }
#endif

    if (mFlags & FMOD_INIT_SYNCMIXERWITHUPDATE)
    {
        if (mOutput->mDescription.polling)
        {
            OutputPolled *outputpolled = (OutputPolled *)mOutput;

            outputpolled->wakeupThread();
        }
    }

    if (mFlags & FMOD_INIT_ENABLE_DSPNET)
    {
        mDSPNet.update(this, delta);
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 71ae7d4d manual
// OFFSET: 0x008D4180
FMOD_RESULT SystemI::set3DSettings(float dopplerscale, float distancescale, float rolloffscale)
{
    if (dopplerscale < 0)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    if (distancescale <= 0)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    if (rolloffscale < 0)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    mDopplerScale  = dopplerscale;
    mDistanceScale = distancescale;
    mRolloffScale  = rolloffscale;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::set3DNumListeners(int numlisteners)
{
    if (numlisteners < 1 || numlisteners > LISTENER_MAX)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    
    mNumListeners = numlisteners;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 29395d55 auto
// OFFSET: 0x008D41E0
FMOD_RESULT SystemI::set3DListenerAttributes(int listener, const FMOD_VECTOR *pos, const FMOD_VECTOR *vel, const FMOD_VECTOR *forward, const FMOD_VECTOR *up)
{
    if (listener < 0 || listener >= LISTENER_MAX)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (forward)
    {
        #ifdef FMOD_DEBUG
        {
            FMOD_RESULT result;
            float length;

            result = FMOD_CHECKFLOAT(forward->x);
            if (result != FMOD_OK)
            {
                return result;
            }
            result = FMOD_CHECKFLOAT(forward->y);
            if (result != FMOD_OK)
            {
                return result;
            }
            result = FMOD_CHECKFLOAT(forward->z);
            if (result != FMOD_OK)
            {
                return result;
            }

            length = FMOD_Vector_DotProduct(forward, forward);
            if (length < 0.9f || length > 1.1f)
            {
                return FMOD_ERR_INVALID_VECTOR;
            }
        }
        #endif

        if (mListener[listener].mLastFront.x != forward->x ||
            mListener[listener].mLastFront.y != forward->y ||
            mListener[listener].mLastFront.z != forward->z)
        {
            mListener[listener].mRotated = true;
        }
 
        mListener[listener].mLastFront = mListener[listener].mFront;
        mListener[listener].mFront     = *forward;
    }

    if (up)
    {
        #ifdef FMOD_DEBUG
        {
            FMOD_RESULT result;
            float length;

            result = FMOD_CHECKFLOAT(up->x);
            if (result != FMOD_OK)
            {
                return result;
            }
            result = FMOD_CHECKFLOAT(up->y);
            if (result != FMOD_OK)
            {
                return result;
            }
            result = FMOD_CHECKFLOAT(up->z);
            if (result != FMOD_OK)
            {
                return result;
            }

            length = FMOD_Vector_DotProduct(up, up);
            if (length < 0.9f || length > 1.1f)
            {
                return FMOD_ERR_INVALID_VECTOR;
            }
        }
        #endif

        if (mListener[listener].mLastUp.x != up->x ||
            mListener[listener].mLastUp.y != up->y ||
            mListener[listener].mLastUp.z != up->z)
        {
            mListener[listener].mRotated = true;
        }

        mListener[listener].mLastUp = mListener[listener].mUp;
        mListener[listener].mUp     = *up;
    }

    if (pos)
    {
        #ifdef FMOD_DEBUG
        {
            FMOD_RESULT result;

            result = FMOD_CHECKFLOAT(pos->x);
            if (result != FMOD_OK)
            {
                return result;
            }
            result = FMOD_CHECKFLOAT(pos->y);
            if (result != FMOD_OK)
            {
                return result;
            }
            result = FMOD_CHECKFLOAT(pos->z);
            if (result != FMOD_OK)
            {
                return result;
            }
        }
        #endif

        if (mListener[listener].mLastPosition.x != pos->x ||
            mListener[listener].mLastPosition.y != pos->y ||
            mListener[listener].mLastPosition.z != pos->z)
        {
            mListener[listener].mMoved = true;
        }

        mListener[listener].mPosition     = *pos;
        mListener[listener].mLastPosition = mListener[listener].mPosition;
    }

    if (vel)
    {
        #ifdef FMOD_DEBUG
        {
            FMOD_RESULT result;

            result = FMOD_CHECKFLOAT(vel->x);
            if (result != FMOD_OK)
            {
                return result;
            }
            result = FMOD_CHECKFLOAT(vel->y);
            if (result != FMOD_OK)
            {
                return result;
            }
            result = FMOD_CHECKFLOAT(vel->z);
            if (result != FMOD_OK)
            {
                return result;
            }
        }
        #endif

        if (mListener[listener].mLastVelocity.x != vel->x ||
            mListener[listener].mLastVelocity.y != vel->y ||
            mListener[listener].mLastVelocity.z != vel->z)
        {
            mListener[listener].mMoved = true;
        }

        mListener[listener].mLastVelocity = mListener[listener].mVelocity;
        mListener[listener].mVelocity = *vel;
    }

    {
        FMOD_VECTOR l_front, l_up;
        
        l_up    = mListener[listener].mUp;
        l_front = mListener[listener].mFront;

        if (mFlags & FMOD_INIT_3D_RIGHTHANDED)
        {
            l_up.z    = -l_up.z;
            l_front.z = -l_front.z;
        }

        #ifdef FMOD_DEBUG
        {
            float dot = FMOD_Vector_DotProduct(&l_up, &l_front);

            if (dot < -0.01f || dot > 0.01f)
            {
                return FMOD_ERR_INVALID_VECTOR;
            }
        }
        #endif

        FMOD_Vector_CrossProduct(&l_up, &l_front, &mListener[listener].mRight);
    }


    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: e83cc3fe auto
// OFFSET: 0x008D44A0
FMOD_RESULT SystemI::set3DRolloffCallback(FMOD_3D_ROLLOFFCALLBACK callback)
{
    mRolloffCallback = callback;

    return FMOD_OK;
}



/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: a8642a0c auto
// OFFSET: 0x008D44C0
FMOD_RESULT SystemI::set3DSpeakerPosition(FMOD_SPEAKER speaker, float x, float y, bool active)
{
    if (speaker < 0 || speaker >= FMOD_SPEAKER_MAX)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    mSpeaker[speaker].mSpeaker    = speaker;
    mSpeaker[speaker].mPosition.x = x;
    mSpeaker[speaker].mPosition.y = 0;
    mSpeaker[speaker].mActive     = active;
    mSpeaker[speaker].mPosition.z = y;
    mSpeaker[speaker].mXZAngle    = FMOD_Cart2Angle((int)(x * 256.0f), (int)(y * 256.0f));

    return sortSpeakerList();
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::get3DSpeakerPosition(FMOD_SPEAKER speaker, float *x, float *y, bool *active)
{
    if (speaker < 0 || speaker >= FMOD_SPEAKER_MAX)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (x)
    {
        *x = mSpeaker[speaker].mPosition.x;
    }
    if (y)
    {
        *y = mSpeaker[speaker].mPosition.z;
    }
    if (active)
    {
        *active = mSpeaker[speaker].mActive;
    }
    
    return FMOD_OK;
}

#endif   // #ifndef FMOD_STATICFORPLUGINS


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: ea1803b8 auto
// OFFSET: 0x008D4600
FMOD_RESULT SystemI::get3DNumListeners(int *numlisteners)
{
    if (!numlisteners)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *numlisteners = mNumListeners;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 11e44ef2 manual
// OFFSET: 0x008D4630
FMOD_RESULT SystemI::get3DListenerAttributes(int listener, FMOD_VECTOR *pos, FMOD_VECTOR *vel, FMOD_VECTOR *forward, FMOD_VECTOR *up)
{
    if (listener < 0 || listener >= LISTENER_MAX)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (pos)
    {
        *pos = mListener[listener].mPosition;
    }

    if (vel)
    {
        *vel = mListener[listener].mVelocity;
    }

    if (forward)
    {
        *forward = mListener[listener].mFront;
    }

    if (up)
    {
        *up = mListener[listener].mUp;
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: bd2ac38c auto
// OFFSET: 0x008D46F0
FMOD_RESULT SystemI::get3DSettings(float *dopplerscale, float *distancescale, float *rolloffscale)
{
    if (dopplerscale)
    {
        *dopplerscale = mDopplerScale;
    }
    if (distancescale)
    {
        *distancescale = mDistanceScale;
    }
    if (rolloffscale)
    {
        *rolloffscale = mRolloffScale;
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 1e72d656 auto
// OFFSET: 0x008D4730
FMOD_RESULT SystemI::getVersion(unsigned int *version)
{
    if (!version)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *version = FMOD_VERSION;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getOutputHandle(void **handle)
{
    if (!handle)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (!mOutput)
    {
        return FMOD_ERR_UNINITIALIZED;
    }

    if (mOutput->mDescription.gethandle)
    {
#ifdef FMOD_SUPPORT_SOFTWARE
        mOutput->readfrommixer = Output::mixCallback;   /* Reset 'read only' variable in FMOD_OUTPUT_STATE in case the user changed it. */
#else
        mOutput->readfrommixer = 0;
#endif

        return mOutput->mDescription.gethandle(mOutput, handle);
    }
    
    return FMOD_OK;
}


#ifndef FMOD_STATICFORPLUGINS

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getChannelsPlaying(int *channels)
{
    if (!channels)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    
    *channels = mChannelUsedListHead.count();

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getCPUUsage(float *dsp, float *stream, float *update, float *total)
{
    FMOD_RESULT result;
    float       totalcpu, usage;

    totalcpu = 0;

#ifdef FMOD_SUPPORT_SOFTWARE
    result = mDSPTimeStamp.getCPUUsage(&usage);
    if (result == FMOD_OK)
    {
        totalcpu += usage;

        if (dsp)
        {
            *dsp = usage;
        }
    }
#else
    if (dsp)
    {
        *dsp = 0;
    }
#endif

#ifdef FMOD_SUPPORT_STREAMING
    result = mStreamTimeStamp.getCPUUsage(&usage);
    if (result == FMOD_OK)
    {
        totalcpu += usage;

        if (stream)
        {
            *stream = usage;
        }
    }
#else
    if (stream)
    {
        *stream = 0;
    }
#endif


    result = mUpdateTimeStamp.getCPUUsage(&usage);
    if (result == FMOD_OK)
    {
        totalcpu += usage;

        if (update)
        {
            *update = usage;
        }
    }

    if (total)
    {
        *total = totalcpu;
    }
     
    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getSoundRAM(int *currentalloced, int *maxalloced, int *total)
{
    if (mOutput && mOutput->mDescription.getsoundram)
    {
#ifdef FMOD_SUPPORT_SOFTWARE
        mOutput->readfrommixer = Output::mixCallback;   /* Reset 'read only' variable in FMOD_OUTPUT_STATE in case the user changed it. */
#else
        mOutput->readfrommixer = 0;
#endif

        mOutput->mDescription.getsoundram(mOutput, currentalloced, maxalloced, total);
    }
    else
    {
        if (currentalloced)
        {
            *currentalloced = 0;
        }
        if (maxalloced)
        {
            *maxalloced = 0;
        }
        if (total)
        {
            *total = 0;
        }
    }

    return FMOD_OK;
}

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getNumCDROMDrives(int *numdrives)
{
#ifdef FMOD_SUPPORT_CDDA
    return FMOD_OS_CDDA_GetNumDevices(numdrives);

#else
    return FMOD_ERR_UNSUPPORTED;
#endif
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getCDROMDriveName(int drive, char *drivename, int drivenamelen, char *scsiname, int scsinamelen, char *devicename, int devicenamelen)
{
#ifdef FMOD_SUPPORT_CDDA
    return FMOD_OS_CDDA_GetDeviceName(drive, drivename, drivenamelen, scsiname, scsinamelen, devicename, devicenamelen);
#else
    return FMOD_ERR_UNSUPPORTED;
#endif
}



/*
[
	[DESCRIPTION]
    Retrieves the spectrum from the currently playing output signal.

	[PARAMETERS]
    'spectrumarray'     Pointer to an array of floats to receive spectrum data.  Data range is 0-1.  Decibels = 10.0f * (float)log10(val) * 2.0f;
    'numvalues'         Size of array in floating point values being passed to the function.  Must be a power of 2. (ie 128/256/512 etc).  Min = 64.  Max = 8192.
    'channeloffset'     Channel to analyze.  If the signal is multichannel (such as a stereo output), then this value represents which channel to analyze.  On a stereo signal 0 = left, 1 = right.
    'windowtype'        Pre-FFT window method.  This filters the PCM data before entering the spectrum analyzer to reduce transient frequency error for more accurate results.  See FMOD_DSP_FFT_WINDOW for different types of fft window techniques possible and for a more detailed explanation.
     
	[RETURN_VALUE]

	[REMARKS]
    The larger the windowsize, the more CPU the FFT will take.  Choose the right value to trade off between accuracy / speed.<br>
    The larger the windowsize, the more 'lag' the spectrum will seem to inherit.  This is because the window size stretches the analysis back in time to what was already played.  For example if the window size happened to be 44100 and the output rate was 44100 it would be analyzing the past second of data, and giving you the average spectrum over that time period.<br>
    If you are not displaying the result in dB, then the data may seem smaller than it should be.  To display it you may want to normalize the data - that is, find the maximum value in the resulting spectrum, and scale all values in the array by 1 / max.  (ie if the max was 0.5f, then it would become 1).<br>
    To get the spectrum for both channels of a stereo signal, call this function twice, once with channeloffset = 0, and again with channeloffset = 1.  Then add the spectrums together and divide by 2 to get the average spectrum for both channels.<br>

    [PLATFORMS]

	[SEE_ALSO]
    FMOD_DSP_FFT_WINDOW
]
*/
FMOD_RESULT SystemI::getSpectrum(float *spectrumarray, int numvalues, int channeloffset, FMOD_DSP_FFT_WINDOW windowtype)
{
#ifdef FMOD_SUPPORT_GETSPECTRUM
    FMOD_RESULT     result = FMOD_OK;
    #if defined(PLATFORM_PS3) || defined(PLATFORM_WINDOWS_PS3MODE)
    DSPI           *soundcard;
    #else
    DSPSoundCard   *soundcard;
    #endif
    float          *buffer;
    unsigned int    position, length;
    int             numchannels, windowsize;
    static DSPFFT   fft;

    if (!mDSPSoundCard)
    {
        return FMOD_ERR_INITIALIZATION;
    }
    
    #if defined(PLATFORM_PS3) || defined(PLATFORM_WINDOWS_PS3MODE)
    soundcard = SAFE_CAST(DSPI, mDSPSoundCard);
    #else
    soundcard = SAFE_CAST(DSPSoundCard, mDSPSoundCard);
    #endif
    if (!soundcard)
    {
        return FMOD_ERR_INITIALIZATION;
    }

    windowsize = numvalues * 2;

    if (windowsize != (1 <<  7) &&
        windowsize != (1 <<  8) &&
        windowsize != (1 <<  9) &&
        windowsize != (1 << 10) &&
        windowsize != (1 << 11) &&
        windowsize != (1 << 12) &&
        windowsize != (1 << 13) &&
        windowsize != (1 << 14))
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    result = getSoftwareFormat(0, 0, &numchannels, 0, 0, 0);
    if (result != FMOD_OK)
    {
        return result;
    }

    if (channeloffset >= numchannels)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    result = soundcard->startBuffering(16384);
    if (result != FMOD_OK)
    {
        return result;
    }

    result = soundcard->getHistoryBuffer(&buffer, &position, &length);
    if (result != FMOD_OK)
    {
        return result;
    }

    if (windowsize > (int)length)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
   
    position -= windowsize;
    if ((int)position < 0)
    {
        position += length;
    }

    mUpdateTimeStamp.stampIn();

    result = fft.getSpectrum(buffer, position, length, spectrumarray, windowsize, channeloffset, numchannels, windowtype);

    mUpdateTimeStamp.stampOut(95);

    return result;
#else
    return FMOD_ERR_NEEDSSOFTWARE;
#endif // FMOD_SUPPORT_GETSPECTRUM
}

#endif


/*
[
	[DESCRIPTION]

	[PARAMETERS]
     
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
    FMOD_DSP_FFT_WINDOW
]
*/
FMOD_RESULT SystemI::getWaveData(float *wavearray, int numvalues, int channeloffset)
{
#ifdef FMOD_SUPPORT_SOFTWARE
    FMOD_RESULT     result = FMOD_OK;
    #if defined(PLATFORM_PS3) || defined(PLATFORM_WINDOWS_PS3MODE)
    DSPI           *soundcard;
    #else
    DSPSoundCard   *soundcard;
    #endif
    float          *buffer;
    unsigned int    position, length;
    int             numchannels, count;

    if (!mDSPSoundCard)
    {
        return FMOD_ERR_INITIALIZATION;
    }

    #if defined(PLATFORM_PS3) || defined(PLATFORM_WINDOWS_PS3MODE)
    soundcard = SAFE_CAST(DSPI, mDSPSoundCard);
    #else
    soundcard = SAFE_CAST(DSPSoundCard, mDSPSoundCard);
    #endif
    if (!soundcard)
    {
        return FMOD_ERR_INITIALIZATION;
    }

    result = getSoftwareFormat(0, 0, &numchannels, 0, 0, 0);
    if (result != FMOD_OK)
    {
        return result;
    }

    if (channeloffset >= numchannels)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    result = soundcard->startBuffering(16384);
    if (result != FMOD_OK)
    {
        return result;
    }

    result = soundcard->getHistoryBuffer(&buffer, &position, &length);
    if (result != FMOD_OK)
    {
        return result;
    }

    if (numvalues > (int)length)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    
    position -= numvalues;
    if ((int)position < 0)
    {
        position += length;
    }

    for (count = 0; count < numvalues; count++)
    {
        wavearray[count] = buffer[position*numchannels+channeloffset];
        position++;
        if (position >= length)
        {
            position = 0;
        }
    }

    return result;
#else
    return FMOD_ERR_NEEDSSOFTWARE;
#endif
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: a83ed056 manual
// OFFSET: 0x008D91D0
FMOD_RESULT SystemI::createSound(const char *name_or_data, FMOD_MODE mode_in, FMOD_CREATESOUNDEXINFO *exinfo, SoundI **sound)
{
    FMOD_RESULT result;

	if (!mInitialized)
	{
		return FMOD_ERR_UNINITIALIZED;
	}

    if (!sound)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (!name_or_data && !(mode_in & FMOD_OPENUSER))
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (!(mode_in & FMOD_SOFTWARE))
    {
        if (!mOutput)
        {
            return FMOD_ERR_OUTPUT_NOHARDWARE;
        }
    }

    if (mode_in & FMOD_HARDWARE && mode_in & FMOD_OPENMEMORY_POINT && !(mode_in & FMOD_CREATESTREAM))
    {
        return FMOD_ERR_NEEDSSOFTWARE;
    }

    *sound = 0;

#ifdef FMOD_SUPPORT_NONBLOCKING
    if (mode_in & FMOD_NONBLOCKING)
    {
        SoundI            *soundi;
        LocalMemoryCounter memcounter;

#ifdef FMOD_SUPPORT_STREAMING
        if (mode_in & FMOD_CREATESTREAM)
        {
            Stream *stream = FMOD_Object_Calloc(Stream);
            if (!stream)
            {
                return FMOD_ERR_MEMORY;
            }
            *sound = stream;
        }
        else
#endif
        {
            Sample *sample = 0;
            int     hwchannels = 0;

            if (!(mode_in & FMOD_SOFTWARE))
            {
                getHardwareChannels(0, 0, &hwchannels);
            }

            if (hwchannels && mOutput->mDescription.createsample)
            {
                mOutput->readfrommixer = Output::mixCallback;

                result = mOutput->mDescription.createsample(mOutput, (FMOD_MODE)0, 0, &sample);
            }
            else
            {
                result = mSoftware->createSample((FMOD_MODE)0, 0, &sample);
            }
            if (result != FMOD_OK)
            {
                return result;
            }

            *sound = sample;
        }

        soundi = SAFE_CAST(SoundI, *sound);

        soundi->mAsyncData = (AsyncData *)FMOD_Memory_Calloc(sizeof(AsyncData));
        if (!soundi->mAsyncData)
        {
            return FMOD_ERR_MEMORY;
        }

        if (mode_in & (FMOD_OPENMEMORY | FMOD_OPENMEMORY_POINT))
        {
            soundi->mAsyncData->mNameData = (void *)name_or_data;
        }
        else if (name_or_data)
        {
            if (mode_in & FMOD_UNICODE)
            {
                FMOD_strncpyW((short *)soundi->mAsyncData->mName, (short *)name_or_data, FMOD_STRING_MAXNAMELEN);
            }
            else
            {
                FMOD_strncpy(soundi->mAsyncData->mName, name_or_data, FMOD_STRING_MAXNAMELEN);
            }
        }

        soundi->mMode       = mode_in;
        soundi->mSystem     = this;
        soundi->mOpenState  = FMOD_OPENSTATE_LOADING;
        soundi->mMemoryUsed = memcounter.getMemoryUsed();

        if (exinfo)
        {
            FMOD_memcpy(&soundi->mAsyncData->mExInfo, exinfo, sizeof(FMOD_CREATESOUNDEXINFO));
            soundi->mAsyncData->mExInfoExists = true;

            if (exinfo->initialsoundgroup)
            {
                soundi->setSoundGroup((FMOD::SoundGroupI *)exinfo->initialsoundgroup);
            }
        }
        else
        {
            soundi->mAsyncData->mExInfoExists = false;
        }

        result = AsyncThread::getAsyncThread(soundi);
        if (result != FMOD_OK)
        {
            soundi->mOpenState = FMOD_OPENSTATE_ERROR;
            soundi->release();
            *sound = 0;
            return result;
        }

        FMOD_OS_CriticalSection_Enter(soundi->mAsyncData->mThread->mCrit);
        {
            soundi->mAsyncData->mNode.setData(soundi);
            soundi->mAsyncData->mNode.addBefore(&soundi->mAsyncData->mThread->mHead);
        }
        FMOD_OS_CriticalSection_Leave(soundi->mAsyncData->mThread->mCrit);

        soundi->mAsyncData->mThread->mThread.wakeupThread();
    }
    else
#endif
    {
        result = createSoundInternal(name_or_data, mode_in, exinfo, false, sound);

        if (*sound && exinfo && exinfo->initialsoundgroup)
        {
            (*sound)->setSoundGroup((FMOD::SoundGroupI *)exinfo->initialsoundgroup);
        }
    }

    return result;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 363033dd manual
// OFFSET: 0x008D9520
FMOD_RESULT SystemI::createStream(const char *name_or_data, FMOD_MODE mode_in, FMOD_CREATESOUNDEXINFO *exinfo, SoundI **sound)
{
#ifdef FMOD_SUPPORT_STREAMING
    return createSound(name_or_data, mode_in | FMOD_CREATESTREAM, exinfo, sound);
#else
    return FMOD_ERR_UNSUPPORTED;
#endif
}



/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/                                    
FMOD_RESULT SystemI::createCodec(FMOD_CODEC_DESCRIPTION *description)
{   
    if (!description)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

#ifdef FMOD_SUPPORT_SOFTWARE
    if (mFlags & FMOD_INIT_SOFTWARE_DISABLE)
#endif
    {
        return FMOD_ERR_NEEDSSOFTWARE;
    }

#ifdef FMOD_SUPPORT_SOFTWARE
    {
        FMOD_RESULT result;
        FMOD_CODEC_DESCRIPTION_EX descriptionex;

        descriptionex.name            = description->name;
        descriptionex.version         = description->version;
        descriptionex.timeunits       = description->timeunits;
        descriptionex.defaultasstream = description->defaultasstream;
        descriptionex.open            = description->open;
        descriptionex.close           = description->close;
        descriptionex.read            = description->read;
        descriptionex.getlength       = description->getlength;
        descriptionex.setposition     = description->setposition;
        descriptionex.getposition     = description->getposition;
        descriptionex.soundcreate     = description->soundcreate;
        descriptionex.getwaveformat   = description->getwaveformat;
        descriptionex.mType           = FMOD_SOUND_TYPE_UNKNOWN;
        descriptionex.mSize           = sizeof(Codec);
        descriptionex.mModule         = 0;
        descriptionex.reset           = 0;

        result = mPluginFactory->registerCodec(&descriptionex);
        if (result != FMOD_OK)
        {
            return result;
        }

    }
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

	[SEE_ALSO]
]
*/                                    
// VERIFY: a7c065a9 auto
// OFFSET: 0x008D5CD0
FMOD_RESULT SystemI::createDSP(FMOD_DSP_DESCRIPTION *description, DSPI **dsp)
{   
    if (!dsp)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *dsp = 0;

    if (!description)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

#ifdef FMOD_SUPPORT_SOFTWARE
    if (mFlags & FMOD_INIT_SOFTWARE_DISABLE)
#endif
    {
        return FMOD_ERR_NEEDSSOFTWARE;
    }

#ifdef FMOD_SUPPORT_SOFTWARE
    {
        FMOD_RESULT result;
        FMOD_DSP_DESCRIPTION_EX descriptionex;

        FMOD_strcpy(descriptionex.name, description->name);
        descriptionex.version               = description->version;
        descriptionex.channels              = description->channels;
        descriptionex.create                = description->create;
        descriptionex.release               = description->release;
        descriptionex.reset                 = description->reset;
        descriptionex.read                  = description->read;
        descriptionex.setposition           = description->setposition;
                                            
        descriptionex.numparameters         = description->numparameters;
        descriptionex.paramdesc             = description->paramdesc;
        descriptionex.setparameter          = description->setparameter;
        descriptionex.getparameter          = description->getparameter;
        descriptionex.config                = description->config;
        descriptionex.configwidth           = description->configwidth;
        descriptionex.configheight          = description->configheight;
        descriptionex.userdata              = description->userdata;

        descriptionex.mSize                 = 0;
        descriptionex.mCategory             = FMOD_DSP_CATEGORY_FILTER;
        descriptionex.mFormat               = FMOD_SOUND_FORMAT_PCMFLOAT;
#ifdef FMOD_SUPPORT_DLLS
        descriptionex.mModule               = 0;
        descriptionex.mAEffect              = 0;
#endif
        descriptionex.mResamplerBlockLength = 0;
        descriptionex.mType                 = FMOD_DSP_TYPE_UNKNOWN;

        result = mPluginFactory->createDSP(&descriptionex, dsp);
        if (result != FMOD_OK)
        {
            return result;
        }

        (*dsp)->mSystem = this;
    }
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

	[SEE_ALSO]
]
*/                                    
// VERIFY: 7acea14b auto
// OFFSET: 0x008D5DE0
FMOD_RESULT SystemI::createDSP(FMOD_DSP_DESCRIPTION_EX *description, DSPI **dsp, bool allocate)
{
    FMOD_RESULT             result;
    FMOD_DSP_DESCRIPTION_EX descriptionex;

    if (!dsp)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (allocate)
    {
        *dsp = 0;
    }

    if (!description)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (mFlags & FMOD_INIT_SOFTWARE_DISABLE)
    {
        return FMOD_ERR_NEEDSSOFTWARE;
    }

    FMOD_strcpy(descriptionex.name, description->name);
    descriptionex.version               = description->version;
    descriptionex.channels              = description->channels;
    descriptionex.create                = description->create;
    descriptionex.release               = description->release;
    descriptionex.reset                 = description->reset;
    descriptionex.read                  = description->read;
    descriptionex.setposition           = description->setposition;
    descriptionex.numparameters         = description->numparameters;
    descriptionex.paramdesc             = description->paramdesc;
    descriptionex.setparameter          = description->setparameter;
    descriptionex.getparameter          = description->getparameter;
    descriptionex.config                = description->config;
    descriptionex.configwidth           = description->configwidth;
    descriptionex.configheight          = description->configheight;
    descriptionex.userdata              = description->userdata;
    descriptionex.mFormat               = description->mFormat;
    descriptionex.mType                 = description->mType;
    descriptionex.mSize                 = description->mSize;
    descriptionex.mCategory             = description->mCategory;
#ifdef FMOD_SUPPORT_DLLS
    descriptionex.mModule               = 0;
    descriptionex.mAEffect              = description->mAEffect;
#endif
    descriptionex.mResamplerBlockLength = description->mResamplerBlockLength;

    result = mPluginFactory->createDSP(&descriptionex, dsp);
    if (result != FMOD_OK)
    {
        return result;
    }

    (*dsp)->mSystem = this;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::createDSPByIndex(int index, DSPI **dsp)
{
#ifdef FMOD_SUPPORT_SOFTWARE
    FMOD_RESULT              result;
    FMOD_DSP_DESCRIPTION_EX *descriptionex = 0;

    if (!mPluginFactory)
    {
        return FMOD_ERR_UNINITIALIZED;
    }

    if (!dsp)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *dsp = 0;

    result = mPluginFactory->getDSP(index, &descriptionex);
    if (result != FMOD_OK)
    {
        return result;
    }

    return mPluginFactory->createDSP(descriptionex, dsp);
#else
    return FMOD_ERR_NEEDSSOFTWARE;
#endif
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 2b6c920c auto
// OFFSET: 0x008D5F10
FMOD_RESULT SystemI::createDSPByType(FMOD_DSP_TYPE type, DSPI **dsp)
{
    FMOD_RESULT result;
    int         count, numdsps;

    if (!mPluginFactory)
    {
        return FMOD_ERR_UNINITIALIZED;
    }

    if (!dsp)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *dsp = 0;

    if (type == FMOD_DSP_TYPE_MIXER)
    {
        FMOD_DSP_DESCRIPTION desc;

        FMOD_memset(&desc, 0, sizeof(FMOD_DSP_DESCRIPTION));
        FMOD_strcpy(desc.name, "FMOD Mixer unit");

        return createDSP(&desc, dsp);
    }

    result = mPluginFactory->getNumDSPs(&numdsps);
    if (result != FMOD_OK)
    {
        return result;
    }

    for (count = 0; count < numdsps; count++)
    {
        FMOD_DSP_DESCRIPTION_EX *descriptionex = 0;

        if (mPluginFactory->getDSP(count, &descriptionex) == FMOD_OK && descriptionex->mType == type)
        {
            return mPluginFactory->createDSP(descriptionex, dsp);
        }
    }

    return FMOD_ERR_PLUGIN_MISSING;
}
  


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 9815896e manual
// OFFSET: 0x008D6000
FMOD_RESULT SystemI::createChannelGroup(const char *name, ChannelGroupI **channelgroup)
{
    ChannelGroupI *newchannelgroup;

    if (!channelgroup)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    newchannelgroup = FMOD_Object_Calloc(ChannelGroupI);
    if (!newchannelgroup)
    {
        return FMOD_ERR_MEMORY;
    }

    newchannelgroup->addAfter(&mChannelGroupHead);
    newchannelgroup->mSystem = this;

    if (name)
    {
        newchannelgroup->mName = FMOD_strdup(name);
        if (!newchannelgroup->mName)
        {
            return FMOD_ERR_MEMORY;
        }
    }
    else
    {
        newchannelgroup->mName = 0;
    }

#ifdef FMOD_SUPPORT_SOFTWARE
    if (mSoftware)
    {
        FMOD_RESULT          result;
        FMOD_DSP_DESCRIPTION description;

        FMOD_strcpy(description.name, "ChannelGroup");
        if (name)
        {
            FMOD_strcat(description.name, ":");
            FMOD_strncat(description.name, name, 18);
        }
        description.version      = 0x00010100;
        description.channels     = 0;
        description.create       = 0;
        description.release      = 0;
        description.read         = 0;
        description.setposition  = 0;

        result = createDSP(&description, &newchannelgroup->mDSPHead);
        if (result != FMOD_OK)
        {
            return result;
        }

        newchannelgroup->mDSPHead->setDefaults((float)mOutputRate, -1, -1, -1);
        newchannelgroup->mDSPHead->setActive(true);

        result = mDSPChannelGroupTarget->addInputQueued(newchannelgroup->mDSPHead, false, 0, 0);
        if (result != FMOD_OK)
        {
            return result;
        }

        newchannelgroup->mDSPMixTarget = newchannelgroup->mDSPHead;
    }
#endif

    if (name && !FMOD_strcmp("music", name))
    {
        mOutput->mMusicChannelGroup = newchannelgroup;
    }

    *channelgroup = newchannelgroup;

    return FMOD_OK;
}


#ifndef FMOD_STATICFORPLUGINS
/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 064f2e9e auto
// OFFSET: 0x008D4750
FMOD_RESULT SystemI::createSoundGroup(const char *name, SoundGroupI **soundgroup)
{
    SoundGroupI *newsoundgroup;

    if (!soundgroup)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    newsoundgroup = FMOD_Object_Calloc(SoundGroupI);
    if (!newsoundgroup)
    {
        return FMOD_ERR_MEMORY;
    }

    newsoundgroup->addAfter(&mSoundGroupFreeHead);
    newsoundgroup->mSystem = this;

    if (name)
    {
        newsoundgroup->mName = FMOD_strdup(name);
        if (!newsoundgroup->mName)
        {
            return FMOD_ERR_MEMORY;
        }
    }
    else
    {
        newsoundgroup->mName = 0;
    }

    *soundgroup = newsoundgroup;

    return FMOD_OK;
}
#endif

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 38fa5b23 manual
// OFFSET: 0x008D61A0
FMOD_RESULT SystemI::playSound(FMOD_CHANNELINDEX channelid, SoundI *sound, bool paused, ChannelI **channel)
{
    FMOD_RESULT  result;
    ChannelI    *chan = 0;
    bool         mute = false;

    if (channel && channelid == FMOD_CHANNEL_REUSE)
    {
        ChannelI::validate((Channel *)*channel, &chan);
    }

    if (!sound)
    {
        if (channel)
        {
            *channel = 0;
        }
        return FMOD_ERR_INVALID_PARAM;
    }

    if (sound->mOpenState != FMOD_OPENSTATE_READY)
    {
        if (channel)
        {
            *channel = 0;
        }
        return FMOD_ERR_NOTREADY;
    }

    if (sound->mType == FMOD_SOUND_TYPE_PLAYLIST)
    {
        return FMOD_ERR_FORMAT;
    }

    if (sound->mSoundGroup && sound->mSoundGroup->mMaxAudible >= 0)
    {
        int numplaying;

        result = sound->mSoundGroup->getNumPlaying(&numplaying);
        if (result != FMOD_OK)
        {
            return result;
        }

        if (numplaying >= sound->mSoundGroup->mMaxAudible)
        {
            switch (sound->mSoundGroup->mMaxAudibleBehavior)
            {
                case FMOD_SOUNDGROUP_BEHAVIOR_FAIL:
                {
                    return FMOD_ERR_MAXAUDIBLE;
                }
                case FMOD_SOUNDGROUP_BEHAVIOR_MUTE:
                {
                    mute = true;
                    break;
                }
                case FMOD_SOUNDGROUP_BEHAVIOR_STEALLOWEST:
                {
                    LinkedListNode *current;

                    for (current = mChannelSortedListHead.getNext(); current != &mChannelSortedListHead; current = current->getNext())
                    {
                        ChannelI *channeli = (ChannelI *)current->getData();

                        if (channeli->mRealChannel[0] && channeli->mRealChannel[0]->mSound && channeli->mRealChannel[0]->mSound->mSoundGroup == sound->mSoundGroup)
                        {
                            chan = channeli;
                            channelid = FMOD_CHANNEL_REUSE;
                        }
                    }
                    break;
                }
                default:
                {
                    break;
                }
            };
        }
    }

    result = findChannel(channelid, sound, &chan);
    if (result != FMOD_OK)
    {
        if (channel)
        {
            *channel = 0;
        }
        return result;
    }

    result = chan->play(sound, paused, true, mute);
    if (result != FMOD_OK)
    {
        if (channel)
        {
            *channel = 0;
        }
        chan->stopEx(false, true, true, true, false, true, false);
        return result;
    }

    result = chan->updatePosition();
    if (result != FMOD_OK)
    {
        if (channel)
        {
            *channel = 0;
        }
        return result;
    }

    if (channelid == FMOD_CHANNEL_REUSE && *channel)
    {
        chan->mHandleCurrent = chan->mHandleOriginal;
    }
    else
    {
        result = chan->referenceStamp(true);
        if (result != FMOD_OK)
        {
            if (channel)
            {
                *channel = 0;
            }
            return result;
        }
    }

    if (channel)
    {
        *channel = (ChannelI *)((FMOD_UINT_NATIVE)chan->mHandleCurrent);
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 6f3d25da auto
// OFFSET: 0x008D47F0
FMOD_RESULT SystemI::playDSP(FMOD_CHANNELINDEX channelid, DSPI *dsp, bool paused, ChannelI **channel)
{
    FMOD_RESULT  result;
    ChannelI    *chan = 0;

    if (!dsp)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (channel)
    {
        if (channelid == FMOD_CHANNEL_REUSE)
        {
            ChannelI::validate((Channel *)*channel, &chan);
        }

        *channel = 0;
    }

    result = findChannel(channelid, dsp, &chan);
    if (result != FMOD_OK)
    {
        return result;
    }

    result = chan->play(dsp, paused, true, false);
    if (result != FMOD_OK)
    {
        chan->stopEx(false, true, true, true, false, true, false);
        return result;
    }

    result = chan->updatePosition();
    if (result != FMOD_OK)
    {
        return result;
    }

    if (channelid == FMOD_CHANNEL_REUSE && *channel)
    {
        chan->mHandleCurrent = chan->mHandleOriginal;
    }
    else
    {
        result = chan->referenceStamp(true);
        if (result != FMOD_OK)
        {
            if (channel)
            {
                *channel = 0;
            }
            return result;
        }
    }

    if (channel)
    {
        *channel = (ChannelI *)((FMOD_UINT_NATIVE)chan->mHandleCurrent);
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 99e40ce3 auto
// OFFSET: 0x008D48E0
FMOD_RESULT SystemI::getChannel(int id, ChannelI **channel)
{
    FMOD_UINT_NATIVE handle;

    if (!channel)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (id < 0 || id >= mNumChannels)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    handle = (mIndex << SYSTEMID_SHIFT) | ((id << CHANINDEX_SHIFT) & (CHANINDEX_MASK << CHANINDEX_SHIFT));
    *channel = (ChannelI *)handle;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: c0c22909 auto
// OFFSET: 0x008D4920
FMOD_RESULT SystemI::getMasterChannelGroup(ChannelGroupI **channelgroup)
{
    if (!channelgroup)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (!mChannelGroup)
    {
        *channelgroup = 0;
        return FMOD_ERR_UNINITIALIZED;
    }

    *channelgroup = mChannelGroup;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getMasterSoundGroup(SoundGroupI **soundgroup)
{
    if (!soundgroup)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (!mSoundGroup)
    {
        *soundgroup = 0;
        return FMOD_ERR_UNINITIALIZED;
    }

    *soundgroup = mSoundGroup;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::createReverb(ReverbI **reverb)
{
#ifdef FMOD_SUPPORT_MULTIREVERB
    FMOD_RESULT result;
    ReverbI *newreverb;
    
    /*
        initialise a new reverb instance
    */
    newreverb = (ReverbI*)FMOD_Object_Alloc(ReverbI);
    if (!newreverb)
    {
        return FMOD_ERR_MEMORY;
    }

    /*
        Initialise the reverb object.
    */
    result = newreverb->init(this, true, FMOD_REVERB_VIRTUAL);
    if (result != FMOD_OK)
    {
        FMOD_Memory_Free(newreverb);
        return result;
    }

    /*
        Add the reverb object to the system's channel reverb list
    */
    newreverb->addBefore(&mReverb3DHead);

    if (reverb)
    {
        *reverb = newreverb;
    }

    mReverbGlobal.setDisableIfNoEnvironment(false);

    mReverb3D.setDisableIfNoEnvironment(false);

    set3DReverbActive(true);

    return FMOD_OK;
#else
    return FMOD_ERR_UNSUPPORTED;
#endif
}


#ifdef FMOD_SUPPORT_MULTIREVERB
/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: cc85fcf5 auto
// OFFSET: 0x008D4950
unsigned int SystemI::count3DPhysicalReverbs()
{
    unsigned int count = 0;

    ReverbI* chanreverb_c = SAFE_CAST(ReverbI, mReverb3DHead.getNext());
    while (chanreverb_c != &mReverb3DHead)
    {
        if (chanreverb_c->getMode() == FMOD_REVERB_PHYSICAL)
        {
            ++count;
        }
        chanreverb_c = SAFE_CAST(ReverbI, chanreverb_c->getNext());
    }
    return count;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 1e436fa8 auto
// OFFSET: 0x008D4980
unsigned int SystemI::count3DVirtualReverbs()
{
    unsigned int count = 0;

    ReverbI* chanreverb_c = SAFE_CAST(ReverbI, mReverb3DHead.getNext());
    while (chanreverb_c != &mReverb3DHead)
    {
        if (chanreverb_c->getMode() == FMOD_REVERB_VIRTUAL)
        {
            ++count;
        }
        chanreverb_c = SAFE_CAST(ReverbI, chanreverb_c->getNext());
    }
    return count;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 939c571f auto
// OFFSET: 0x008D49B0
FMOD_RESULT SystemI::set3DReverbActive(bool state)
{
    mReverb3DActive = state;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 758fba40 auto
// OFFSET: 0x008D49D0
FMOD_RESULT SystemI::get3DReverbActive(bool *state)
{
    if (state)
    {
        *state = mReverb3DActive;
    }
    return FMOD_OK;
}

#endif

#ifndef FMOD_STATICFORPLUGINS

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 46676010 manual
// OFFSET: 0x008D49F0
FMOD_RESULT SystemI::setReverbProperties(const FMOD_REVERB_PROPERTIES *prop, bool force_create)
{
#ifdef FMOD_SUPPORT_REVERB
    FMOD_RESULT result;

    if (!prop)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

#ifdef FMOD_SUPPORT_SOFTWARE
    if (!mReverbGlobal.mDSP && (force_create || prop->Environment != -1))
    {
        result = mReverbGlobal.createDSP();
        if (result != FMOD_OK)
        {
            return result;
        }

        if (!mDSPChannelGroupTarget)
        {
            return FMOD_ERR_UNINITIALIZED;
        }

        result = mDSPChannelGroupTarget->addInput(mReverbGlobal.mDSP);
        if (result != FMOD_OK)
        {
            return result;
        }

        mReverbGlobal.setGain(1.0f);

        ChannelI *channel_c = SAFE_CAST(ChannelI, mChannelUsedListHead.getNext());
        while (channel_c != &mChannelUsedListHead)
        {
            FMOD_REVERB_CHANNELPROPERTIES cprops;

            result = channel_c->getReverbProperties(&cprops);
            if (result != FMOD_OK)
            {
                return result;
            }
            result = channel_c->setReverbProperties(&cprops);
            if (result != FMOD_OK)
            {
                return result;
            }

            channel_c = SAFE_CAST(ChannelI, channel_c->getNext());
        }

        mReverbGlobal.mDSP->setActive(true);
    }
#endif

    return mReverbGlobal.setProperties(prop);
#else
    return FMOD_ERR_UNSUPPORTED;
#endif
}

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getReverbProperties(FMOD_REVERB_PROPERTIES *prop)
{
    if (!prop)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    return mReverbGlobal.getProperties(prop);
}

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
#ifdef FMOD_SUPPORT_MULTIREVERB
// VERIFY: 4c51c797 manual
// OFFSET: 0x008D4AD0
FMOD_RESULT SystemI::set3DReverbProperties(const FMOD_REVERB_PROPERTIES *prop, bool force_create)
{
#if defined(FMOD_SUPPORT_REVERB) && defined(FMOD_SUPPORT_MULTIREVERB)
    FMOD_RESULT result;

    if (!prop)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

#ifdef FMOD_SUPPORT_SOFTWARE
    if (!mReverb3D.mDSP && (force_create || prop->Environment != -1))
    {
        result = mReverb3D.createDSP();
        if (result != FMOD_OK)
        {
            return result;
        }

        if (!mDSPChannelGroupTarget)
        {
            return FMOD_ERR_UNINITIALIZED;
        }

        result = mDSPChannelGroupTarget->addInput(mReverb3D.mDSP);
        if (result != FMOD_OK)
        {
            return result;
        }

        mReverb3D.setGain(1.0f);

        ChannelI *channel_c = SAFE_CAST(ChannelI, mChannelUsedListHead.getNext());
        while (channel_c != &mChannelUsedListHead)
        {
            FMOD_REVERB_CHANNELPROPERTIES cprops;

            result = channel_c->getReverbProperties(&cprops);
            if (result != FMOD_OK)
            {
                return result;
            }
            result = channel_c->setReverbProperties(&cprops);
            if (result != FMOD_OK)
            {
                return result;
            }

            channel_c = SAFE_CAST(ChannelI, channel_c->getNext());
        }

        mReverb3D.mDSP->setActive(true);
    }
#endif

    return mReverb3D.setProperties(prop);
#else
    return FMOD_ERR_UNSUPPORTED;
#endif
}
#endif

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
#ifdef FMOD_SUPPORT_MULTIREVERB
#endif

#endif

/*
[
	[DESCRIPTION]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 7b5a2d7c auto
// OFFSET: 0x008D4BB0
FMOD_RESULT SystemI::getDSPHead(DSPI **dsp)
{
#ifdef FMOD_SUPPORT_SOFTWARE
    if (!dsp)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (!mDSPSoundCard)
    {
        return FMOD_ERR_INTERNAL;
    }

    *dsp = mDSPSoundCard;

    return FMOD_OK;
#else
    return FMOD_ERR_NEEDSSOFTWARE;
#endif
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::addDSP(DSPI *dsp)
{
    FMOD_RESULT result;
    DSPI       *dsphead = 0;
    DSPI       *inputdsp;
    int         numinputs;

    if (!dsp)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    result = getDSPHead(&dsphead);
    if (result != FMOD_OK)
    {
        return result;
    }

    result = dsphead->getNumInputs(&numinputs);
    if (result != FMOD_OK)
    {
        return result;
    }
    if (numinputs > 1)
    {
        return FMOD_ERR_DSP_TOOMANYCONNECTIONS;
    }

    if (dsp->mFlags & FMOD_DSP_FLAG_USEDADDDSP)
    {
        result = dsp->remove();
    }
    else
    {
        result = dsp->disconnectFrom(0);
    }
    if (result != FMOD_OK)
    {
        return result;
    }

    if (dsphead->getInput(0, &inputdsp) != FMOD_OK)
    {
        inputdsp = 0;
    }
    else
    {
        result = dsphead->disconnectFrom(inputdsp);
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    result = dsphead->addInput(dsp);
    if (result != FMOD_OK)
    {
        return result;
    }

    if (inputdsp)
    {
        result = dsp->addInput(inputdsp);
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    dsp->reset();
    dsp->mFlags |= FMOD_DSP_FLAG_ACTIVE | FMOD_DSP_FLAG_USEDADDDSP;

    return FMOD_OK;
}



/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: bffe224c auto
// OFFSET: 0x008D4BE0
FMOD_RESULT SystemI::lockDSP()
{
#ifdef FMOD_SUPPORT_SOFTWARE
    FMOD_OS_CriticalSection_Enter(mDSPLockCrit);

    return FMOD_OK;
#else
    return FMOD_ERR_NEEDSSOFTWARE;
#endif
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 7fb78cd6 auto
// OFFSET: 0x008D4C00
FMOD_RESULT SystemI::unlockDSP()
{
#ifdef FMOD_SUPPORT_SOFTWARE
    FMOD_OS_CriticalSection_Leave(mDSPLockCrit);

    return FMOD_OK;
#else
    return FMOD_ERR_NEEDSSOFTWARE;
#endif
}



/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: cb13c751 auto
// OFFSET: 0x008D4C20
FMOD_RESULT SystemI::getRecordNumDrivers(int *numdrivers)
{
#ifdef FMOD_SUPPORT_RECORDING
	if (!mOutput)
	{
		return FMOD_ERR_UNINITIALIZED;
	}

    if (!numdrivers)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (mOutput->mDescription.record_getnumdrivers)
    {
#ifdef FMOD_SUPPORT_SOFTWARE
        mOutput->readfrommixer = Output::mixCallback;
#else
        mOutput->readfrommixer = 0;
#endif
        return mOutput->mDescription.record_getnumdrivers(mOutput, numdrivers);
    }

    *numdrivers = 0;

    return FMOD_OK;
#else
    return FMOD_ERR_UNSUPPORTED;
#endif
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: eb171e77 auto
// OFFSET: 0x008D6380
FMOD_RESULT SystemI::setRecordDriver(int driver)
{
#ifdef FMOD_SUPPORT_RECORDING
    FMOD_RESULT result;
    int         numdrivers;

    result = getRecordNumDrivers(&numdrivers);
    if (result != FMOD_OK)
    {
        return result;
    }

    if (driver < -1 || driver >= numdrivers)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (driver == -1)
    {
        driver = 0;
    }

    mRecordDriver = driver;

    return FMOD_OK;
#else
    return FMOD_ERR_UNSUPPORTED;
#endif
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getRecordDriver(int *driver)
{
#ifdef FMOD_SUPPORT_RECORDING
    if (!driver)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *driver = mRecordDriver;

    return FMOD_OK;
#else
    return FMOD_ERR_UNSUPPORTED;
#endif
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 63395b2e auto
// OFFSET: 0x008D4C90
FMOD_RESULT SystemI::getRecordDriverName(int id, char *name, int namelen)
{
#ifdef FMOD_SUPPORT_RECORDING
    FMOD_RESULT result      = FMOD_OK;
    int         numdrivers;

	if (!mOutput)
	{
		return FMOD_ERR_UNINITIALIZED;
	}

    result = getRecordNumDrivers(&numdrivers);
    CHECK_RESULT(result);

    if (id < 0 || id >= numdrivers)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (mOutput->mDescription.record_getdrivername)
    {
#ifdef FMOD_SUPPORT_SOFTWARE
        mOutput->readfrommixer = Output::mixCallback;   /* Reset 'read only' variable in FMOD_OUTPUT_STATE in case the user changed it. */
#else
        mOutput->readfrommixer = 0;
#endif

        return mOutput->mDescription.record_getdrivername(mOutput, id, name, namelen);
    }

    return FMOD_OK;

#else
    return FMOD_ERR_UNSUPPORTED;
#endif
}


/*
 [
     [DESCRIPTION]
     
     [PARAMETERS]
     
     [RETURN_VALUE]
     
     [REMARKS]
     
     [PLATFORMS]
     
     [SEE_ALSO]
     ]
 */
FMOD_RESULT SystemI::getRecordDriverCaps(int id, FMOD_CAPS *caps, int *minfrequency, int *maxfrequency)
{
#ifdef FMOD_SUPPORT_RECORDING    
    FMOD_RESULT result          = FMOD_OK;
    FMOD_CAPS   lcaps           = FMOD_CAPS_NONE;
    int         lminfrequency   = 0;
    int         lmaxfrequency   = 0;
    int         numdrivers      = 0;
    
	if (!mOutput)
	{
		return FMOD_ERR_UNINITIALIZED;
	}
    
    result = getRecordNumDrivers(&numdrivers);
    CHECK_RESULT(result);
    
    if (id < 0 || id >= numdrivers)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    
    if (mOutput->mDescription.record_getdrivercaps)
    {
#ifdef FMOD_SUPPORT_SOFTWARE
        mOutput->readfrommixer = Output::mixCallback;   /* Reset 'read only' variable in FMOD_OUTPUT_STATE in case the user changed it. */
#else
        mOutput->readfrommixer = 0;
#endif
        result = mOutput->mDescription.record_getdrivercaps(mOutput, id, &lcaps, &lminfrequency, &lmaxfrequency);
        CHECK_RESULT(result);
    }
    
    if (caps)
    {
        *caps = lcaps;
    }
    if (minfrequency)
    {
        *minfrequency = lminfrequency;
    }
    if (maxfrequency)
    {
        *maxfrequency = lmaxfrequency;
    }
    
    return FMOD_OK;
#else
    return FMOD_ERR_UNSUPPORTED;
#endif    
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 11f932c7 manual
// OFFSET: 0x008D63D0
FMOD_RESULT SystemI::recordStart(SoundI *sound, bool loop)
{
#ifdef FMOD_SUPPORT_RECORDING
    FMOD_RESULT result;

	if (!mInitialized)
	{
		return FMOD_ERR_UNINITIALIZED;
	}

	if (!sound || (sound->mMode & FMOD_CREATESTREAM))
	{
		return FMOD_ERR_INVALID_PARAM;
	}

	recordStop();

    mOutput->mRecordSound           = sound;
    mOutput->mRecordOffset          = 0;
    mOutput->mRecordLoop            = loop;
    mOutput->mRecordLastCursorPos   = 0;
    mOutput->mRecordBufferLength    = 0;
    mOutput->mRecordRate            = (int)sound->mDefaultFrequency;

    if (mOutput->mDescription.record_start)
    {
        mOutput->readfrommixer = Output::mixCallback;

        result = mOutput->mDescription.record_start(mOutput, mRecordDriver, (FMOD_SOUND *)sound, loop);
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    {
        unsigned int bufferlengthbytes = 0;

        SoundI::getBytesFromSamples(FMOD_RECORD_TEMPBUFFERSIZE, &bufferlengthbytes, sound->mChannels, FMOD_SOUND_FORMAT_PCMFLOAT);

        mOutput->mRecordTempBufferLength = FMOD_RECORD_TEMPBUFFERSIZE;
        mOutput->mRecordTempBuffer = (float *)FMOD_Memory_Calloc(bufferlengthbytes);
        if (!mOutput->mRecordTempBuffer)
        {
            return FMOD_ERR_MEMORY;
        }
    }

    mOutput->mRecordResample = false;

    if (mOutput->mRecordRate != sound->mDefaultFrequency)
    {
        unsigned int             blocklength;
        FMOD_DSP_DESCRIPTION_EX  description;
        DSPResampler            *resampler = &mOutput->mRecordResamplerDSP;

        blocklength = (unsigned int)(0.01f * mOutput->mRecordRate);
        blocklength /= 16;
        blocklength *= 16;

        FMOD_memset(&description, 0, sizeof(FMOD_DSP_DESCRIPTION_EX));

        description.channels               = sound->mChannels;
        description.mFormat                = FMOD_SOUND_FORMAT_PCMFLOAT;
        description.userdata               = mOutput;
        description.read                   = Output::recordResamplerReadCallback;
        description.mResamplerBlockLength  = blocklength;

        resampler->mSystem = this;
        resampler->alloc(&description);
        resampler->mTargetFrequency = (int)sound->mDefaultFrequency;
        resampler->setFrequency((float)mOutput->mRecordRate);

        mOutput->mRecordResample = true;
    }

    mOutput->mRecording = true;

	return FMOD_OK;
#else
    return FMOD_ERR_UNSUPPORTED;
#endif
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 89221d61 auto
// OFFSET: 0x008D4D20
FMOD_RESULT SystemI::recordStop()
{
#ifdef FMOD_SUPPORT_RECORDING
    FMOD_RESULT result = FMOD_OK;

	if (!mInitialized)
	{
		return FMOD_ERR_UNINITIALIZED;
	}

    FMOD_OS_CriticalSection_Enter(mDSPLockCrit);
    mOutput->mRecording = false;
    mOutput->mRecordSound = 0;
    FMOD_OS_CriticalSection_Leave(mDSPLockCrit);

    if (mOutput->mDescription.record_stop)
    {
        mOutput->readfrommixer = Output::mixCallback;

        result = mOutput->mDescription.record_stop(mOutput);
    }

    if (mOutput->mRecordTempBuffer)
    {
        FMOD_Memory_Free(mOutput->mRecordTempBuffer);
        mOutput->mRecordTempBuffer = 0;
        mOutput->mRecordTempBufferLength = 0;
    }

    return result;
#else
    return FMOD_ERR_UNSUPPORTED;
#endif
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 1fb4da68 auto
// OFFSET: 0x008D4DE0
FMOD_RESULT SystemI::getRecordPosition(unsigned int *position)
{
#ifdef FMOD_SUPPORT_RECORDING
    if (!mInitialized)
	{
		return FMOD_ERR_UNINITIALIZED;
	}

    *position = mOutput->mRecordOffset;

    return FMOD_OK;
#else
    return FMOD_ERR_UNSUPPORTED;
#endif
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: f15be579 auto
// OFFSET: 0x008D4E10
FMOD_RESULT SystemI::isRecording(bool *recording)
{
#ifdef FMOD_SUPPORT_RECORDING
    if (!mInitialized)
	{
		return FMOD_ERR_UNINITIALIZED;
	}

    if (!recording)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *recording = mOutput->mRecording;

    return FMOD_OK;
#else
    return FMOD_ERR_UNSUPPORTED;
#endif
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::createGeometry(int maxNumPolygons, int maxNumVertices, GeometryI **geometry)
{
#ifdef FMOD_SUPPORT_GEOMETRY
	FMOD_RESULT result;

    if (!geometry)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (maxNumPolygons <= 0)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    if (maxNumVertices <= 0)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    if (geometry == 0)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

 	*geometry = (GeometryI *)FMOD_Memory_Alloc(sizeof (GeometryI));
    if (!*geometry)
    {
        return FMOD_ERR_MEMORY;	
    }
    
    new (*geometry) GeometryI(&mGeometryMgr);
 	
	result = (*geometry)->alloc(maxNumPolygons, maxNumVertices);
	if (result != FMOD_OK)
    {
		return result;
    }
 
 	if (mGeometryList)
    {
 		(*geometry)->addBefore(mGeometryList);
    }

 	mGeometryList = *geometry;
 
	return FMOD_OK;
#else
	return FMOD_ERR_UNSUPPORTED;
#endif
} 

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::setGeometrySettings(float maxWorldSize)
{
#ifdef FMOD_SUPPORT_GEOMETRY
	FMOD_RESULT result;

	if (maxWorldSize <= 0.0f)
    {
		return FMOD_ERR_INVALID_PARAM;
    }

 	result = mGeometryMgr.setWorldSize(maxWorldSize);
	if (result != FMOD_OK)
    {
		return result;
    }
	
	if (mGeometryList)
	{
		// we have to remove everything from the tree and re-add
		// it when the world size changes
		GeometryI* current = mGeometryList;
		do
		{ 
			current->removeFromTree();
			current = SAFE_CAST(GeometryI, current->getNext());
		}
		while (current != mGeometryList);

		current = mGeometryList;
		do
		{ 
			result = current->setWorldSize(maxWorldSize); // re-adds automatically
			if (result != FMOD_OK)
            {
				return result;
            }

			current = SAFE_CAST(GeometryI, current->getNext());
		}
		while (current != mGeometryList);
	}

	return FMOD_OK;
#else
	return FMOD_ERR_UNSUPPORTED;
#endif
}

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getGeometrySettings(float *maxWorldSize)
{
#ifdef FMOD_SUPPORT_GEOMETRY
	if (!maxWorldSize)
    {
		return FMOD_ERR_INVALID_PARAM;
    }

 	*maxWorldSize = mGeometryMgr.getWorldSize(); 
	
    return FMOD_OK;
#else
	return FMOD_ERR_UNSUPPORTED;
#endif
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::loadGeometry(const void *data, int dataSize, GeometryI **geometry)
{ 
#ifdef FMOD_SUPPORT_GEOMETRY
	FMOD_RESULT result;

    if (!data)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    if (!geometry)
    {
        return FMOD_ERR_INVALID_PARAM;
    }
    *geometry = (GeometryI *)FMOD_Memory_Alloc(sizeof (GeometryI));
    if (!*geometry)
    {
        return FMOD_ERR_MEMORY;	
    }
    
    new (*geometry) GeometryI(&mGeometryMgr);
 	
	result = (*geometry)->load(data, dataSize);
	if (result != FMOD_OK)
    {
		return result;
    }
 
 	if (mGeometryList)
    {
 		(*geometry)->addBefore(mGeometryList);
    }

 	mGeometryList = *geometry;
    
    return FMOD_OK;

#else
	return FMOD_ERR_UNSUPPORTED;
#endif
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::setNetworkProxy(const char *proxy)
{
#ifdef FMOD_SUPPORT_NET
    return FMOD_Net_SetProxy(proxy);
#else
    return FMOD_ERR_UNSUPPORTED;
#endif
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getNetworkProxy(char *proxy, int proxylen)
{
#ifdef FMOD_SUPPORT_NET
    return FMOD_Net_GetProxy(proxy, proxylen);
#else
    return FMOD_ERR_UNSUPPORTED;
#endif
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::setNetworkTimeout(int timeout)
{
#ifdef FMOD_SUPPORT_NET
    return FMOD_Net_SetTimeout(timeout);
#else
    return FMOD_ERR_UNSUPPORTED;
#endif
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getNetworkTimeout(int *timeout)
{
#ifdef FMOD_SUPPORT_NET
    return FMOD_Net_GetTimeout(timeout);
#else
    return FMOD_ERR_UNSUPPORTED;
#endif
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::setStreamBufferSize(unsigned int filebuffersize, FMOD_TIMEUNIT filebuffersizetype)
{
    if (filebuffersize <= 0)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (filebuffersizetype != FMOD_TIMEUNIT_MS && 
        filebuffersizetype != FMOD_TIMEUNIT_PCM && 
        filebuffersizetype != FMOD_TIMEUNIT_PCMBYTES && 
        filebuffersizetype != FMOD_TIMEUNIT_RAWBYTES)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    mStreamFileBufferSize       = filebuffersize;
    mStreamFileBufferSizeType   = filebuffersizetype;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getStreamBufferSize(unsigned int *filebuffersize, FMOD_TIMEUNIT *filebuffersizetype)
{
    if (filebuffersize)
    {
        *filebuffersize = mStreamFileBufferSize;
    }
    if (filebuffersizetype)
    {
        *filebuffersizetype = mStreamFileBufferSizeType;
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

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::setUserData(void *userdata)
{
    mUserData = userdata;
    
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

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getUserData(void **userdata)
{
    if (!userdata)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *userdata = mUserData;
    
    return FMOD_OK;
}



/*
[
	[DESCRIPTION]
    Stops all playing channels using this sound.

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 8bbcfa45 manual
// OFFSET: 0x008D4E50
FMOD_RESULT SystemI::stopSound(SoundI *sound)
{
    ChannelI *current;
    bool      streamstopped = false;

#ifdef FMOD_SUPPORT_RECORDING
    if (mOutput && sound == mOutput->mRecordSound)
    {
        recordStop();
    }
#endif

#ifdef FMOD_SUPPORT_STREAMING
    if (sound->isStream())
    {
        FMOD_OS_CriticalSection_Enter(mStreamListCrit);

        if (!mStreamListChannelHead.isEmpty())
        {
            LinkedListNode *current;
            Stream *stream = SAFE_CAST(Stream, sound);

            current = mStreamListChannelHead.getNext();
            while (current != &mStreamListChannelHead)
            {
                ChannelStream  *channelstream = (ChannelStream *)current->getData();
                Stream         *currentstream = SAFE_CAST(Stream, channelstream->mSound);
                LinkedListNode *next = current->getNext();

                streamstopped = false;

                if (!currentstream)
                {
                    current = next;
                    continue;
                }

                if (currentstream == stream)
                {
                    FMOD_OS_CriticalSection_Leave(mStreamListCrit);
                    channelstream->mParent->stop();
                    streamstopped = true;
                    FMOD_OS_CriticalSection_Enter(mStreamListCrit);
                }
#ifdef FMOD_SUPPORT_SENTENCING
                else if (currentstream->mSubSoundList)
                {
                    if (currentstream->mSubSound[currentstream->mSubSoundList[currentstream->mSubSoundListCurrent]] == sound)
                    {
                        FMOD_OS_CriticalSection_Leave(mStreamListCrit);
                        channelstream->mParent->stop();
                        streamstopped = true;
                        FMOD_OS_CriticalSection_Enter(mStreamListCrit);
                    }
                }
#endif
                else if (currentstream->mSubSound)
                {
                    int count;

                    for (count = 0; count < currentstream->mNumSubSounds; count++)
                    {
                        if (currentstream->mSubSound[count] == sound)
                        {
                            FMOD_OS_CriticalSection_Leave(mStreamListCrit);
                            channelstream->mParent->stop();
                            streamstopped = true;
                            FMOD_OS_CriticalSection_Enter(mStreamListCrit);
                            break;
                        }
                    }
                }
                else if (currentstream->mSample == stream->mSample)
                {
                    FMOD_OS_CriticalSection_Leave(mStreamListCrit);
                    channelstream->mParent->stop();
                    streamstopped = true;
                    FMOD_OS_CriticalSection_Enter(mStreamListCrit);
                }

                current = next;
            }
        }

        FMOD_OS_CriticalSection_Leave(mStreamListCrit);
    }
#endif

    if (!streamstopped)
    {
        if (sound->mMode & FMOD_SOFTWARE)
        {
            FMOD_OS_CriticalSection_Enter(mDSPLockCrit);
        }

        current = SAFE_CAST(ChannelI, mChannelUsedListHead.getNext());
        while (current != &mChannelUsedListHead)
        {
            ChannelI *next = SAFE_CAST(ChannelI, current->getNext());

            if (current->mRealChannel[0])
            {
                SoundI *channelsound = 0;

                current->getCurrentSound(&channelsound);
                if (channelsound == sound)
                {
                    current->stop();
                }
            }

            current = next;
        }

        if (sound->mMode & FMOD_SOFTWARE)
        {
            FMOD_OS_CriticalSection_Leave(mDSPLockCrit);
        }
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 636c6830 auto
// OFFSET: 0x008D5020
FMOD_RESULT SystemI::stopDSP(DSPI *dsp)
{
    ChannelI *current;

    current = SAFE_CAST(ChannelI, mChannelUsedListHead.getNext());
    while (current != &mChannelUsedListHead)
    {
        ChannelI *next = SAFE_CAST(ChannelI, current->getNext());
        DSPI *channeldsp;

        current->getCurrentDSP(&channeldsp);
        if (channeldsp == dsp)
        {
            current->stop();
        }

        current = next;
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]
    On success, FMOD_OK is returned.
    On failure, an error code is returned.

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 7802e3cd auto
// OFFSET: 0x008D5060
FMOD_RESULT SystemI::getListenerObject(int listener, Listener **listenerobject)
{   
    if (!listenerobject || (listener < 0) || (listener >= mNumListeners))
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *listenerobject = &mListener[listener];

    return FMOD_OK;
}



#ifdef FMOD_SUPPORT_SOFTWARE
/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 4bc96865 manual
// OFFSET: 0x008D50A0
FMOD_RESULT SystemI::flushDSPConnectionRequests(bool calledfrommainthread)
{
    DSPConnectionRequest *request;

    FMOD_OS_CriticalSection_Enter(mDSPRequestCrit);

    if (mConnectionRequestFlushing)
    {
        FMOD_OS_CriticalSection_Leave(mDSPRequestCrit);
        return FMOD_OK;
    }

    mConnectionRequestFlushing = true;

    request = (DSPConnectionRequest *)mConnectionRequestUsedHead.getNext();
    while (request != &mConnectionRequestUsedHead)
    {
        switch (request->mRequest)
        {
            case DSPCONNECTION_REQUEST_ADDINPUT:
            {
                request->mThis->addInputInternal(request->mTarget, false, request->mConnection, 0);
                break;
            }
            case DSPCONNECTION_REQUEST_DISCONNECTFROM:
            {
                request->mThis->disconnectFromInternal(request->mTarget);
                break;
            }
            case DSPCONNECTION_REQUEST_DISCONNECTALLINPUTS:
            {
                request->mThis->disconnectAllInternal(true, false);
                break;
            }
            case DSPCONNECTION_REQUEST_DISCONNECTALLOUTPUTS:
            {
                request->mThis->disconnectAllInternal(false, true);
                break;
            }
            case DSPCONNECTION_REQUEST_DISCONNECTALL:
            {
                request->mThis->disconnectAllInternal(true, true);
                break;
            }
        }

        request->removeNode();
        request->addBefore(&mConnectionRequestFreeHead);

        request = (DSPConnectionRequest *)mConnectionRequestUsedHead.getNext();
    }

    mConnectionRequestFlushing = false;

    FMOD_OS_CriticalSection_Leave(mDSPRequestCrit);

    return FMOD_OK;
}
#endif


#ifndef FMOD_STATICFORPLUGINS
/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getGlobals(Global **global)
{
    if (!global)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *global = gGlobal;

    return FMOD_OK;
}

FMOD_RESULT F_API System_SetDebugLevel(unsigned int level)
{
#ifdef FMOD_DEBUG
    gGlobal->gDebugLevel = (FMOD_DEBUGLEVEL)level;
#endif
    return FMOD_OK;
}

unsigned int F_API System_GetDebugLevel()
{
#ifdef FMOD_DEBUG
    return (unsigned int )gGlobal->gDebugLevel;
#else
    return 0;
#endif
}

FMOD_RESULT F_API System_SetDebugMode(unsigned int mode)
{
#ifdef FMOD_DEBUG
    gGlobal->gDebugMode = (FMOD_DEBUGMODE)mode;
#endif
    return FMOD_OK;
}
#endif

#ifdef FMOD_SUPPORT_MULTIREVERB
/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
// VERIFY: 482d643e auto
// OFFSET: 0x008D65E0
FMOD_RESULT SystemI::update3DReverbs()
{
    float        t = 0.0f;
    bool         reverb3d_active;
    ReverbI     *reverb_c;
    FMOD_REVERB_STDPROPERTIES stdprops;

    FMOD_memset(&stdprops, 0, sizeof(FMOD_REVERB_STDPROPERTIES));

    for (reverb_c = SAFE_CAST(ReverbI, mReverb3DHead.getNext()); reverb_c != &mReverb3DHead; reverb_c = SAFE_CAST(ReverbI, reverb_c->getNext()))
    {
        float distance_gain, distance_coeff;
        bool active;

        reverb_c->getActive(&active);
        if (active)
        {
            reverb_c->calculateDistanceGain(&mListener[0].mPosition, &distance_gain, &distance_coeff);

#ifdef FMOD_SUPPORT_GEOMETRY
            if (distance_gain > 0.0f)
            {
                float direct_occlusion, reverb_occlusion;
                FMOD_RESULT  result;
                FMOD_VECTOR  pos;

                reverb_c->get3DAttributes(&pos, 0, 0);

                result = mGeometryMgr.lineTestAll(&mListener[0].mPosition, &pos, &direct_occlusion, &reverb_occlusion);
                if (result != FMOD_OK)
                {
                    return result;
                }

                distance_gain *= (1.0f - reverb_occlusion);
                distance_coeff *= (1.0f - reverb_occlusion);
            }
#endif

            if (reverb_c->getMode() == FMOD_REVERB_PHYSICAL)
            {
                if (distance_gain != reverb_c->getGain())
                {
                    reverb_c->setGain(distance_gain);
                }
            }
            else if (reverb_c->getMode() == FMOD_REVERB_VIRTUAL)
            {
                if (distance_coeff >= 0.001f)
                {
                    FMOD_REVERB_PROPERTIES reverbprops;

                    reverb_c->getProperties(&reverbprops);
                    ReverbI::sumProps(&stdprops, &reverbprops, distance_coeff);
                    t += distance_coeff;
                }
            }
        }
    }

    get3DReverbActive(&reverb3d_active);
    if (reverb3d_active)
    {
        FMOD_REVERB_PROPERTIES qprops;

        if (t < 1.0f)
        {
            FMOD_REVERB_PROPERTIES ambience;

            getReverbAmbientProperties(&ambience);

            if (ambience.Environment == -1)
            {
                ambience.Room = -10000;
                ReverbI::sumRoomProps(&stdprops, &ambience, 1.0f - t);
            }
            else
            {
                ReverbI::sumProps(&stdprops, &ambience, 1.0f - t);
            }
            t = 1.0f;
        }

        ReverbI::factorProps(&qprops, &stdprops, 1.0f/t);

        set3DReverbProperties(&qprops, true);
    }

    return FMOD_OK;
}

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::setReverbAmbientProperties(FMOD_REVERB_PROPERTIES* prop)
{
    if (!prop)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (prop->Environment != -1)
    {
        set3DReverbActive(true);
    }

    FMOD_memcpy(&mReverb3DAmbientProperties, prop, sizeof(FMOD_REVERB_PROPERTIES));

    return FMOD_OK;
}

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]

	[SEE_ALSO]
]
*/
FMOD_RESULT SystemI::getReverbAmbientProperties(FMOD_REVERB_PROPERTIES* prop)
{
    if (!prop)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    FMOD_memcpy(prop, &mReverb3DAmbientProperties, sizeof(FMOD_REVERB_PROPERTIES));

    return FMOD_OK;
}
#endif

unsigned int F_API System_GetDebugMode()
{
#ifdef FMOD_DEBUG
    return (unsigned int )gGlobal->gDebugMode;
#else
    return 0;
#endif
}


/*
[API]
[
	[DESCRIPTION]
    Callback for system events.

	[PARAMETERS]
	'system'        Pointer to a system handle.   Note this could be null if FMOD_SYSTEM_CALLBACKTYPE_THREADCREATED is triggered from the EventSystem.
    'type'          The type of callback.  Refer to FMOD_SYSTEM_CALLBACKTYPE.
    'commanddata1'  The first callback type specific data generated by the callback.  See remarks for meaning.
    'commanddata2'  The second callback type specific data generated by the callback.  See remarks for meaning.

	[RETURN_VALUE]

	[REMARKS]
    <u>C++ Users</u>.  Cast <b>FMOD_SYSTEM *</b> to <b>FMOD::System *</b> inside the callback and use as normal.<br>
    <br>
    <u>'commanddata1' and 'commanddata2' meanings.</u><br>
    These 2 values are set by the callback depending on what is happening in the callback and the type of callback.<br>
    <li><b>FMOD_SYSTEM_CALLBACKTYPE_DEVICELISTCHANGED</b><br>
        <i>commanddata1</i>: Always 0.<br>
        <i>commanddata2</i>: Always 0.<br>
    <br>
    <li><b>FMOD_SYSTEM_CALLBACKTYPE_MEMORYALLOCATIONFAILED</b><br>
        <i>commanddata1</i>: A string (char*) which represents the file and line number of the allocation inside FMOD.<br>
        <i>commanddata2</i>: The size (int) of the requested allocation.<br>
    <br>
    <li><b>FMOD_SYSTEM_CALLBACKTYPE_THREADCREATED</b><br>
        <i>commanddata1</i>: The handle of the created thread.  See notes below for thread handle types<br>
        <i>commanddata2</i>: A string (char*) which represents the name of the thread.<br>
    <br>
    <li><b>FMOD_SYSTEM_CALLBACKTYPE_BADDSPCONNECTION</b><br>
        <i>commanddata1</i>: Pointer to a FMOD::DSP object that was the target of the DSP connection.<br>
        <i>commanddata2</i>: Pointer to a FMOD::DSP object that was the source of the DSP connection.<br>
    <br>
    <li><b>FMOD_SYSTEM_CALLBACKTYPE_BADDSPLEVEL</b><br>
        <i>commanddata1</i>: Pointer to a FMOD::DSP object that was trying to exceed the DSP tree level maximum.<br>
        <i>commanddata2</i>: 0.<br>
    <br>
    
    <b>Note!</b>  For FMOD_SYSTEM_CALLBACKTYPE_DEVICELISTCHANGED, the user must call System::update for the callback to trigger! See FMOD_SYSTEM_CALLBACKTYPE for details.<br>
    <br>
    <b>Note!</b>  For FMOD_SYSTEM_CALLBACKTYPE_THREADCREATED, the handle that is returned (via commanddata1) is different on each platform.  The types to cast to are as follows.<br>
    <li>iPhone, Linux, Mac, Solaris : pthread_t
    <li>PS2 : int
    <li>PS3 : sys_ppu_thread_t
    <li>PSP : PSPThreadWrapper.   This is a custom struct you can define as typedef struct PSPThreadWrapper { SceUID    id; int     (*func)(void *param); void     *param; };
    <li>Wii : OSThread
    <li>Win32, Win64, Xbox360 : HANDLE
    <br>
    <br>
    <br>
    Here is an example of a system callback.<br>
    <br>
    <PRE>
    FMOD_RESULT F_CALLBACK systemcallback(FMOD_SYSTEM *system, FMOD_SYSTEM_CALLBACKTYPE type, void *commanddata1, void *commanddata2)
    {
    <ul>FMOD::System *sys = (FMOD::System *)system;
        <br>
        switch (type)
        {
        <ul>case FMOD_SYSTEM_CALLBACKTYPE_DEVICELISTCHANGED:
            {
            <ul>int numdrivers;
                <br>
                printf("NOTE : FMOD_SYSTEM_CALLBACKTYPE_DEVICELISTCHANGED occured.\n");
                <br>
                sys->getNumDrivers(&numdrivers);
                <br>
                printf("Numdevices = %d\n", numdrivers);
                break;
            </ul>}
            case FMOD_SYSTEM_CALLBACKTYPE_MEMORYALLOCATIONFAILED:
            {
            <ul>printf("ERROR : FMOD_SYSTEM_CALLBACKTYPE_MEMORYALLOCATIONFAILED occured.\n");
                printf("%s.\n", commanddata1);
                printf("%d bytes.\n", commanddata2);
                break;
            </ul>}
            case FMOD_SYSTEM_CALLBACKTYPE_THREADCREATED:
            {
            <ul>printf("NOTE : FMOD_SYSTEM_CALLBACKTYPE_THREADCREATED occured.\n");
                printf("Thread ID = %d\n", (int)commanddata1);
                printf("Thread Name = %s\n", (char *)commanddata2);
                break;
            </ul>}
            case FMOD_SYSTEM_CALLBACKTYPE_BADDSPCONNECTION:
            {
            <ul>FMOD::DSP *source = (FMOD::DSP *)commanddata1;
                FMOD::DSP *dest = (FMOD::DSP *)commanddata2;
                <br>
                printf("ERROR : FMOD_SYSTEM_CALLBACKTYPE_BADDSPCONNECTION occured.\n");
                if (source)
                {
                <ul>char name[256];
                    source->getInfo(name, 0,0,0,0);
                    printf("SOURCE = %s\n", name);
                </ul>}
                if (dest)
                {
                <ul>char name[256];
                    dest->getInfo(name, 0,0,0,0);
                    printf("DEST = %s\n", name);
                </ul>}
                break;
            </ul>}
            case FMOD_SYSTEM_CALLBACKTYPE_BADDSPLEVEL:
            {
            <ul>FMOD::DSP *source = (FMOD::DSP *)commanddata1;
                <br>
                printf("ERROR : FMOD_SYSTEM_CALLBACKTYPE_BADDSPLEVEL occured.\n");
                if (source)
                {
                <ul>char name[256];
                    source->getInfo(name, 0,0,0,0);
                    printf("SOURCE = %s\n", name);
                </ul>}
                break;
            </ul>}
        </ul>}
        <br>
        return FMOD_OK;    
    </ul>}
    </PRE>
    <br>
    [PLATFORMS]
    Win32, Win64, Linux, Linux64, Macintosh, Xbox, Xbox360, PlayStation 2, GameCube, PlayStation Portable, PlayStation 3, Wii, Solaris

	[SEE_ALSO]
    System::setCallback
    FMOD_SYSTEM_CALLBACKTYPE
    System::update
]
*/
/*
FMOD_RESULT F_CALLBACK FMOD_SYSTEM_CALLBACK(FMOD_SYSTEM *system, FMOD_SYSTEM_CALLBACKTYPE type, void* commanddata1, void* commanddata2)
{
#if 0
	 // here purely for documentation purposes.
#endif
}
*/



}
