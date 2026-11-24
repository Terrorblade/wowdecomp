#include "fmod_settings.h"

#ifdef FMOD_SUPPORT_MIDI

#include "fmod.h"

#include "fmod_channel_software.h"
#include "fmod_codec_dls.h"
#include "fmod_codec_midi.h"
#include "fmod_debug.h"
#include "fmod_dspi.h"
#include "fmod_file.h"
#include "fmod_localcriticalsection.h"
#include "fmod_memory.h"
#include "fmod_systemi.h"
#include "fmod_string.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#if defined(PLATFORM_WINDOWS) || defined(PLATFORM_MAC)
    #include <sys/stat.h>
#endif

namespace FMOD
{


FMOD_CODEC_DESCRIPTION_EX midicodec;


#ifdef PLUGIN_EXPORTS

#ifdef __cplusplus
extern "C" {
#endif

    /*
        FMODGetCodecDescription is mandantory for every fmod plugin.  This is the symbol the registerplugin function searches for.
        Must be declared with F_API to make it export as stdcall.
    */
    F_DECLSPEC F_DLLEXPORT FMOD_CODEC_DESCRIPTION_EX * F_API FMODGetCodecDescriptionEx()
    {
        return CodecMIDI::getDescriptionEx();
    }

#ifdef __cplusplus
}
#endif

#endif  /* PLUGIN_EXPORTS */

const char *note[128] =
{
    "C0", "C#0", "D0", "D#0", "E0", "F0", "F#0", "G0", "G#0", "A0", "A#0", "B0",  
    "C1", "C#1", "D1", "D#1", "E1", "F1", "F#1", "G1", "G#1", "A1", "A#1", "B1",  
    "C2", "C#2", "D2", "D#2", "E2", "F2", "F#2", "G2", "G#2", "A2", "A#2", "B2",  
    "C3", "C#3", "D3", "D#3", "E3", "F3", "F#3", "G3", "G#3", "A3", "A#3", "B3",  
    "C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4", "A4", "A#4", "B4",  
    "C5", "C#5", "D5", "D#5", "E5", "F5", "F#5", "G5", "G#5", "A5", "A#5", "B5",  
    "C6", "C#6", "D6", "D#6", "E6", "F6", "F#6", "G6", "G#6", "A6", "A#6", "B6",  
    "C7", "C#7", "D7", "D#7", "E7", "F7", "F#7", "G7", "G#7", "A7", "A#7", "B7",  
    "C8", "C#8", "D8", "D#8", "E8", "F8", "F#8", "G8", "G#8", "A8", "A#8", "B8",  
    "C9", "C#9", "D9", "D#9", "E9", "F9", "F#9", "G9", "G#9", "A9", "A#9", "B9"
    "C10", "C#10", "D10", "D#10", "E10", "F10", "F#10", "G10", "G#10"
};

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
// VERIFY: 6fe7af46 manual
// OFFSET: 0x008FD340
FMOD_CODEC_DESCRIPTION_EX *CodecMIDI::getDescriptionEx()
{
    FMOD_memset(&midicodec, 0, sizeof(FMOD_CODEC_DESCRIPTION_EX));

    midicodec.name            = "FMOD MIDI Codec";
    midicodec.version         = 0x00010100;
    midicodec.timeunits       = FMOD_TIMEUNIT_PCM;
    midicodec.defaultasstream = 1;
    midicodec.open            = &CodecMIDI::openCallback;
    midicodec.close           = &CodecMIDI::closeCallback;
    midicodec.read            = &CodecMIDI::readCallback;
    midicodec.setposition     = &CodecMIDI::setPositionCallback;

    midicodec.mType = FMOD_SOUND_TYPE_MIDI;
    midicodec.mSize = sizeof(CodecMIDI);

    return &midicodec;
}


/*
    ==========================================================================================

    CODEC MIDI SUBCHANNEL CLASS 

    ==========================================================================================
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
FMOD_RESULT CodecMIDISubChannel::displayArticulators()
{
    int blockcount;

    for (blockcount = 0; blockcount < mNumArticulators; blockcount++)
    {
        const char *src = "?", *control = "?", *dest = "?", *trans = "?";
        DLS_CONNECTIONBLOCK *block = &mArticulator[blockcount];

        switch (block->usSource)
        {
            case CONN_SRC_NONE:
            {
                src = "SRC_NONE";
                break;
            }
            case CONN_SRC_LFO:
            {
                src = "SRC_LFO";
                break;
            }
            case CONN_SRC_KEYONVELOCITY:
            {
                src = "SRC_KEYONVELOCITY";
                break;
            }
            case CONN_SRC_KEYNUMBER:
            {
                src = "SRC_KEYNUMBER";
                break;
            }
            case CONN_SRC_EG1:
            {
                src = "SRC_EG1";
                break;
            }
            case CONN_SRC_EG2:
            {
                src = "SRC_EG2";
                break;
            }
            case CONN_SRC_PITCHWHEEL:
            {
                src = "SRC_PITCHWHEEL";
                break;
            }
            case CONN_SRC_CC1:
            {
                src = "SRC_CC1";
                break;
            }
            case CONN_SRC_CC7:
            {
                src = "SRC_CC7";
                break;
            }
            case CONN_SRC_CC10:
            {
                src = "SRC_CC10";
                break;
            }
            case CONN_SRC_CC11:
            {
                src = "SRC_CC11";
                break;
            }
        }


        switch (block->usControl)
        {
            case CONN_SRC_NONE:
            {
                control = "SRC_NONE";
                break;
            }
            case CONN_SRC_LFO:
            {
                control = "SRC_LFO";
                break;
            }
            case CONN_SRC_KEYONVELOCITY:
            {
                control = "SRC_KEYONVELOCITY";
                break;
            }
            case CONN_SRC_KEYNUMBER:
            {
                control = "SRC_KEYNUMBER";
                break;
            }
            case CONN_SRC_EG1:
            {
                control = "SRC_EG1";
                break;
            }
            case CONN_SRC_EG2:
            {
                control = "SRC_EG2";
                break;
            }
            case CONN_SRC_PITCHWHEEL:
            {
                control = "SRC_PITCHWHEEL";
                break;
            }
            case CONN_SRC_CC1:
            {
                control = "SRC_CC1";
                break;
            }
            case CONN_SRC_CC7:
            {
                control = "SRC_CC7";
                break;
            }
            case CONN_SRC_CC10:
            {
                control = "SRC_CC10";
                break;
            }
            case CONN_SRC_CC11:
            {
                control = "SRC_CC11";
                break;
            }
        }

        switch (block->usDestination)
        {
            case CONN_DST_NONE:
            {
                dest = "DST_NONE";
                break;
            }
            case CONN_DST_ATTENUATION:
            {
                dest = "DST_ATTENUATION";
                break;
            }
            case CONN_DST_RESERVED:
            {
                dest = "DST_RESERVED";
                break;
            }
            case CONN_DST_PITCH:
            {
                dest = "DST_PITCH";
                break;
            }
            case CONN_DST_PAN:
            {
                dest = "DST_PAN";
                break;
            }
            case CONN_DST_LFO_FREQUENCY:
            {
                dest = "DST_LFO_FREQUENCY";
                break;
            }
            case CONN_DST_LFO_STARTDELAY:
            {
                dest = "DST_LFO_STARTDELAY";
                break;
            }
            case CONN_DST_EG1_ATTACKTIME:
            {
                dest = "DST_EG1_ATTACKTIME";
                break;
            }
            case CONN_DST_EG1_DECAYTIME:
            {
                dest = "DST_EG1_DECAYTIME";
                break;
            }
            case CONN_DST_EG1_RESERVED:
            {
                dest = "DST_EG1_RESERVED";
                break;
            }
            case CONN_DST_EG1_RELEASETIME:
            {
                dest = "DST_EG1_RELEASETIME";
                break;
            }
            case CONN_DST_EG1_SUSTAINLEVEL:
            {
                dest = "DST_EG1_SUSTAINLEVEL";
                break;
            }
            case CONN_DST_EG2_ATTACKTIME:
            {
                dest = "DST_EG2_ATTACKTIME";
                break;
            }
            case CONN_DST_EG2_DECAYTIME:
            {
                dest = "DST_EG2_DECAYTIME";
                break;
            }
            case CONN_DST_EG2_RESERVED:
            {
                dest = "DST_EG2_RESERVED";
                break;
            }
            case CONN_DST_EG2_RELEASETIME:
            {
                dest = "DST_EG2_RELEASETIME";
                break;
            }
            case CONN_DST_EG2_SUSTAINLEVEL:
            {
                dest = "DST_EG2_SUSTAINLEVEL";
                break;
            }
        };

        if (block->usTransform == CONN_TRN_CONCAVE)
        {
            trans = "TRN_CONCAVE";
        }
        else
        {
            trans = "TRN_NONE";
        }

        printf("%2d | %-13s | %-8s | %-20s | %08x | %-10s\n", blockcount, src, control, dest, block->lScale, trans);
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
// VERIFY: d2e946ac auto
// OFFSET: 0x008F9FE0
FMOD_RESULT CodecMIDISubChannel::findArticulator(int src, int dest)
{
    int blockcount;

    for (blockcount = 0; blockcount < mNumArticulators; blockcount++)
    {
        DLS_CONNECTIONBLOCK *block = &mArticulator[blockcount];

        if (block->usSource == src && block->usDestination == dest)
        {
            return FMOD_OK;
        }
    }

    return FMOD_ERR_INVALID_PARAM;
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
// VERIFY: 799ddfa2 manual
// OFFSET: 0x008FA030
FMOD_RESULT CodecMIDISubChannel::articulateDest(CONN_SRC_FLAGS srcflags, int dest, int *usDestination)
{
    int blockcount;
    bool found = false;

    *usDestination = 0;

    for (blockcount = 0; blockcount < mNumArticulators; blockcount++)
    {
        DLS_CONNECTIONBLOCK *block = &mArticulator[blockcount];
        float  usSource = 1.0f;
        float  usControl = 1.0f;

        if (block->usDestination == dest)
        {
            switch (block->usSource)
            {
                case CONN_SRC_NONE:
                {
                    if (!(srcflags & CONN_SRC_FLAG_NONE))
                    {
                        continue;
                    }
                    usSource = 1.0f;
                    break;
                }
                case CONN_SRC_LFO:
                {
                    if (!(srcflags & CONN_SRC_FLAG_LFO))
                    {
                        continue;
                    }
                    break;
                }
                case CONN_SRC_KEYONVELOCITY:
                {
                    usSource = (float)mKeyOnVelocity / 128.0f;
                    if (!(srcflags & CONN_SRC_FLAG_KEYONVELOCITY))
                    {
                        continue;
                    }
                    break;
                }
                case CONN_SRC_KEYNUMBER:
                {
                    usSource = (float)mKeyOnKey / 128.0f;
                    if (!(srcflags & CONN_SRC_FLAG_KEYNUMBER))
                    {
                        continue;
                    }
                    break;
                }
                case CONN_SRC_EG1:
                {
                    if (!(srcflags & CONN_SRC_FLAG_EG1))
                    {
                        continue;
                    }
                    break;
                }
                case CONN_SRC_EG2:
                {
                    if (!(srcflags & CONN_SRC_FLAG_EG2))
                    {
                        continue;
                    }
                    usSource = 1.0f;
                    break;
                }
                case CONN_SRC_PITCHWHEEL:
                {
                    if (!(srcflags & CONN_SRC_FLAG_PITCHWHEEL))
                    {
                        continue;
                    }
                    break;
                }
                case CONN_SRC_CC1:
                {
                    if (!(srcflags & CONN_SRC_FLAG_MODWHEEL))
                    {
                        continue;
                    }
                    break;
                }
                case CONN_SRC_CC7:
                {
                    if (!(srcflags & CONN_SRC_FLAG_CHANNELVOL))
                    {
                        continue;
                    }
                    break;
                }
                case CONN_SRC_CC10:
                {
                    if (!(srcflags & CONN_SRC_FLAG_PAN))
                    {
                        continue;
                    }
                    break;
                }
                case CONN_SRC_CC11:
                {
                    if (!(srcflags & CONN_SRC_FLAG_EXPRESSION))
                    {
                        continue;
                    }
                    break;
                }
            };


            switch (block->usControl)
            {
                case CONN_SRC_NONE:
                {
                    usControl = 1.0f;
                    break;
                }
                case CONN_SRC_LFO:
                {
                    usControl = 1.0f;
                    break;
                }
                case CONN_SRC_KEYONVELOCITY:
                {
                    usControl = (float)mKeyOnVelocity / 128.0f;
                    break;
                }
                case CONN_SRC_KEYNUMBER:
                {
                    usControl = (float)mKeyOnKey / 128.0f;
                    break;
                }
                case CONN_SRC_EG1:
                {
                    break;
                }
                case CONN_SRC_EG2:
                {
                    break;
                }
                case CONN_SRC_PITCHWHEEL:
                {
                    break;
                }
                case CONN_SRC_CC1:
                {
                    usControl = (float)mParent->mModWheel / 128.0f;
                    break;
                }
                case CONN_SRC_CC7:
                {
                    usControl = (float)mParent->mVolume / 128.0f;
                    break;
                }
                case CONN_SRC_CC10:
                {
                    usControl = (float)mParent->mPan / 128.0f;
                    break;
                }
                case CONN_SRC_CC11:
                {
                    usControl = (float)mParent->mExpression / 128.0f;
                    break;
                }
            }

            if (block->usTransform == CONN_TRN_CONCAVE)
            {
    //            *usDestination = *usDestination + (int)usTransform(usSource * (usControl * (float)block->lScale))
            }
            else
            {
                *usDestination = *usDestination + (int)(usSource * (usControl * (float)block->lScale));
            }

            found = true;
        }
    }

    if (found)
    {
        return FMOD_OK;
    }

    return FMOD_ERR_INVALID_PARAM;
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

#define USETAB

#ifdef USETAB

/*
        int count = 0;
        for (val = -10.0f; val < 5.5f; val += ((5.5f - -10.0f) / 128.0f))
        {
            printf("%9.03ff, ", FMOD_POW(2.0f, val) * 1000.0f);
            count++;
            if (count && !(count % 4))
            {
                printf("\n");
            }
        }
*/
#define TIMECENTSTABLENUM 128
static float gTimeCentsTable[TIMECENTSTABLENUM] =
{
    0.977f,     1.062f,     1.155f,     1.256f,    1.366f,     1.486f,     1.616f,     1.757f,
    1.911f,     2.079f,     2.261f,     2.459f,    2.674f,     2.908f,     3.163f,     3.439f,
    3.741f,     4.068f,     4.424f,     4.812f,    5.233f,     5.691f,     6.190f,     6.732f,
    7.321f,     7.962f,     8.659f,     9.417f,   10.242f,    11.139f,    12.114f,    13.175f,
   14.328f,    15.583f,    16.947f,    18.431f,   20.045f,    21.800f,    23.709f,    25.785f,
   28.042f,    30.498f,    33.168f,    36.072f,   39.231f,    42.666f,    46.401f,    50.464f,
   54.883f,    59.688f,    64.915f,    70.598f,   76.780f,    83.503f,    90.814f,    98.766f,
  107.414f,   116.819f,   127.047f,   138.171f,  150.270f,   163.427f,   177.737f,   193.299f,
  210.224f,   228.631f,   248.650f,   270.421f,  294.099f,   319.850f,   347.856f,   378.314f,
  411.439f,   447.464f,   486.644f,   529.254f,  575.595f,   625.993f,   680.805f,   740.415f,
  805.245f,   875.752f,   952.432f,  1035.826f, 1126.522f,  1225.159f,  1332.433f,  1449.099f,
 1575.981f,  1713.972f,  1864.046f,  2027.260f, 2204.765f,  2397.812f,  2607.763f,  2836.096f,
 3084.422f,  3354.491f,  3648.207f,  3967.640f, 4315.043f,  4692.864f,  5103.767f,  5550.648f,
 6036.658f,  6565.222f,  7140.066f,  7765.244f, 8445.161f,  9184.612f,  9988.808f, 10863.418f,
11814.609f, 12849.085f, 13974.139f, 15197.702f,16528.398f, 17975.609f, 19549.537f, 21261.276f,
23122.893f, 25147.512f, 27349.404f, 29744.092f,32348.457f, 35180.858f, 38261.261f, 41611.381f
};

#endif

// VERIFY: a54d515d auto
// OFFSET: 0x008FA390
float CodecMIDISubChannel::getTimeCentsFromlScale(int lScale)
{
    if ((unsigned int)lScale == 0x80000000)
    {
        return 0;
    }

#ifdef USETAB
    {
        float val;
        int index;

        val = (float)lScale / (1200.0f*65536.0f);
        val += 10.0f;
        val *= (1.0f / ((5.0f - -10.0f) / 128.0f));
   
        if (val < 0)
        {
            val = 0;
        }
        if (val >= TIMECENTSTABLENUM)
        {
            val = TIMECENTSTABLENUM - 1;
        }

        index = (int)val;

        return gTimeCentsTable[index];
    }
#else
    return FMOD_POW(2.0f, lScale / (1200.0f*65536.0f) ) * 1000.0f;
#endif
}


// VERIFY: e4f97cd1 auto
// OFFSET: 0x008FA400
FMOD_RESULT CodecMIDISubChannel::setUpArticulators()
{
    mKeyOff        = false;
    mMiddleC       = 12800;   /* */
    mLFOStartDelay = 0;
    mLFOTime       = 0;
    mLFOFrequency  = 0;

    /*
        Reset volume envelope
    */
    mVolumeEnvelope.mPosition = CODEC_DLS_ENVPOINT_ATTACK;
    mVolumeEnvelope.mTime     = 0;
    mVolumeEnvelope.mSustain  = 0.0f;
    mVolumeEnvelope.mActive   = true;
    mVolumeEnvelope.mRange    = 0;
    mVolumeEnvelope.mPoint[CODEC_DLS_ENVPOINT_ATTACK].mTime         = 0.0f;
    mVolumeEnvelope.mPoint[CODEC_DLS_ENVPOINT_ATTACK].mSrcValue     = -96.0f;
    mVolumeEnvelope.mPoint[CODEC_DLS_ENVPOINT_ATTACK].mDestValue    = 0.0f;

    mVolumeEnvelope.mPoint[CODEC_DLS_ENVPOINT_DECAY].mTime          = 0.0f;
    mVolumeEnvelope.mPoint[CODEC_DLS_ENVPOINT_DECAY].mSrcValue      = 0.0f;
    mVolumeEnvelope.mPoint[CODEC_DLS_ENVPOINT_DECAY].mDestValue     = -96.0f;

    mVolumeEnvelope.mPoint[CODEC_DLS_ENVPOINT_RELEASE].mTime        = 0.0f;
    mVolumeEnvelope.mPoint[CODEC_DLS_ENVPOINT_RELEASE].mSrcValue    = 0.0f;
    mVolumeEnvelope.mPoint[CODEC_DLS_ENVPOINT_RELEASE].mDestValue   = -96.0f;

    /*
        Reset pitch envelope
    */
    mPitchEnvelope.mPosition = CODEC_DLS_ENVPOINT_ATTACK;
    mPitchEnvelope.mTime     = 0;
    mPitchEnvelope.mSustain  = 1.0f;
    mPitchEnvelope.mActive   = false;
    mPitchEnvelope.mRange    = 0;
    mPitchEnvelope.mPoint[CODEC_DLS_ENVPOINT_ATTACK].mTime       = 0.0f;   
    mPitchEnvelope.mPoint[CODEC_DLS_ENVPOINT_ATTACK].mSrcValue   = 0.0f;
    mPitchEnvelope.mPoint[CODEC_DLS_ENVPOINT_ATTACK].mDestValue  = 1.0f;

    mPitchEnvelope.mPoint[CODEC_DLS_ENVPOINT_DECAY].mTime        = 0.0f;   
    mPitchEnvelope.mPoint[CODEC_DLS_ENVPOINT_DECAY].mSrcValue    = 1.0f;
    mPitchEnvelope.mPoint[CODEC_DLS_ENVPOINT_DECAY].mDestValue   = 0.0f;

    mPitchEnvelope.mPoint[CODEC_DLS_ENVPOINT_RELEASE].mTime      = 0.0f;   
    mPitchEnvelope.mPoint[CODEC_DLS_ENVPOINT_RELEASE].mSrcValue  = 0.0f;
    mPitchEnvelope.mPoint[CODEC_DLS_ENVPOINT_RELEASE].mDestValue = 0.0f;

#if 0
    displayArticulators();
#endif
    
    /*
        Run through articulators.
    */   
    {   
        int lScale;

        /*
            LFO
        */
        if (articulateDest(CONN_SRC_FLAG_LFO, CONN_DST_ATTENUATION, &lScale) == FMOD_OK)
        {
            mTremoloScale = -(lScale / (65536.0f * 10.0f));
            mTremoloScale = 1.0f - FMOD_POW(10.0f, mTremoloScale / 20.0f);
        }
        if (articulateDest(CONN_SRC_FLAG_LFO, CONN_DST_PITCH, &lScale) == FMOD_OK)
        {
            mVibratoScale = (float)lScale / 65536.0f;
        }
        if (articulateDest(CONN_SRC_FLAG_LFO, CONN_DST_PAN, &lScale) == FMOD_OK)
        {
            mPanbrelloScale = lScale == 0x8000000 ? -96.0f : (1.0f - (lScale / (1000.0f * 65536.0f))) * -96.0f;
        }
        if (articulateDest(CONN_SRC_FLAG_COMMON, CONN_DST_LFO_FREQUENCY, &lScale) == FMOD_OK)
        {
            mLFOFrequency = FMOD_POW(2.0f, (((float)lScale / 65536.0f) - 6900.0f) / 1200.0f) * 440.0f;
        }
        if (articulateDest(CONN_SRC_FLAG_COMMON, CONN_DST_LFO_STARTDELAY, &lScale) == FMOD_OK)
        {
            mLFOStartDelay = getTimeCentsFromlScale(lScale);
        }

        /*
            Volume envelope
        */
        if (findArticulator(CONN_SRC_NONE, CONN_DST_EG1_ATTACKTIME) == FMOD_OK)
        {
            if (articulateDest(CONN_SRC_FLAG_COMMON, CONN_DST_EG1_ATTACKTIME, &lScale) == FMOD_OK)
            {
                mVolumeEnvelope.mPoint[CODEC_DLS_ENVPOINT_ATTACK].mTime = getTimeCentsFromlScale(lScale);
            }
        }
        if (findArticulator(CONN_SRC_NONE, CONN_DST_EG1_DECAYTIME) == FMOD_OK)
        {
            if (articulateDest(CONN_SRC_FLAG_COMMON, CONN_DST_EG1_DECAYTIME, &lScale) == FMOD_OK)
            {
                mVolumeEnvelope.mPoint[CODEC_DLS_ENVPOINT_DECAY].mTime = getTimeCentsFromlScale(lScale);
            }
        }
        if (findArticulator(CONN_SRC_NONE, CONN_DST_EG1_RELEASETIME) == FMOD_OK)
        {
            if (articulateDest(CONN_SRC_FLAG_COMMON, CONN_DST_EG1_RELEASETIME, &lScale) == FMOD_OK)
            {
                mVolumeEnvelope.mPoint[CODEC_DLS_ENVPOINT_RELEASE].mTime = getTimeCentsFromlScale(lScale);
            }
        }
        if (articulateDest(CONN_SRC_FLAG_COMMON, CONN_DST_EG1_SUSTAINLEVEL, &lScale) == FMOD_OK)
        {
            mVolumeEnvelope.mSustain = lScale == 0x8000000 ? -96.0f : (1.0f - (lScale / (1000.0f * 65536.0f))) * -96.0f;
        }

        /*
            Pitch envelope
        */
        if (articulateDest(CONN_SRC_FLAG_EG2, CONN_DST_PITCH, &lScale) == FMOD_OK)
        {
            mPitchEnvelope.mActive = true;
            mPitchEnvelope.mRange = (float)lScale / 65536.0f;

            if (findArticulator(CONN_SRC_NONE, CONN_DST_EG2_ATTACKTIME) == FMOD_OK)
            {
                if (articulateDest(CONN_SRC_FLAG_NONE, CONN_DST_EG2_ATTACKTIME, &lScale) == FMOD_OK)
                {
                    mPitchEnvelope.mPoint[CODEC_DLS_ENVPOINT_ATTACK].mTime = getTimeCentsFromlScale(lScale);
                }
            }
            if (findArticulator(CONN_SRC_NONE, CONN_DST_EG2_DECAYTIME) == FMOD_OK)
            {
                if (articulateDest(CONN_SRC_FLAG_NONE, CONN_DST_EG2_DECAYTIME, &lScale) == FMOD_OK)
                {
                    mPitchEnvelope.mPoint[CODEC_DLS_ENVPOINT_DECAY].mTime = getTimeCentsFromlScale(lScale);
                }
            }
            if (findArticulator(CONN_SRC_NONE, CONN_DST_EG2_RELEASETIME) == FMOD_OK)
            {
                if (articulateDest(CONN_SRC_FLAG_NONE, CONN_DST_EG2_RELEASETIME, &lScale) == FMOD_OK)
                {
                    mPitchEnvelope.mPoint[CODEC_DLS_ENVPOINT_RELEASE].mTime = getTimeCentsFromlScale(lScale);
                }
            }
            if (articulateDest(CONN_SRC_FLAG_NONE, CONN_DST_EG2_SUSTAINLEVEL, &lScale) == FMOD_OK)
            {
                mPitchEnvelope.mSustain = lScale == 0x8000000 ? 0 : lScale / (1000.0f * 65536.0f);
            }
        }


        /*
            Pan envelope?
        */
        if (articulateDest(CONN_SRC_FLAG_EG2, CONN_DST_PAN, &lScale) == FMOD_OK)
        {
            lScale = lScale;
        }
    }

    /*
        This helps remove clicks, but might not be a good idea.
    */
    if (mVolumeEnvelope.mPoint[CODEC_DLS_ENVPOINT_RELEASE].mTime < 50.0f)
    {
        mVolumeEnvelope.mPoint[CODEC_DLS_ENVPOINT_RELEASE].mTime = 50.0f;
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
// VERIFY: e83732ba auto
// OFFSET: 0x008FA810
FMOD_RESULT CodecMIDISubChannel::stop()
{
    mChannel.stopEx(false, false, true, true, false, false, false);
    mInstrument  = 0;
    mKeyOff      = false;
    mCurrentNote = -1;       
    removeNode();
    addAfter(&mMIDI->mChannelFreeListHead);

    return FMOD_OK;
}

#if 1
/*
float val;
int count = 0;

for (val = -96.0f; val <= 0; val += 0.5f)
{
    printf("%.04f ", FMOD_POW(10.0f, val/20.0f));
    if (count && !(count % 8))
    {
        printf("\n");
    }
    count++;
}
eg1=eg1;
*/

static float gDBToLinearTable[(96*2) + 1] =
{
    0.0000f, // -96db
    0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f,
    0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f,
    0.0000f, 0.0000f, 0.0000f, 0.0001f, 0.0001f, 0.0001f, 0.0001f, 0.0001f,
    0.0001f, 0.0001f, 0.0001f, 0.0001f, 0.0001f, 0.0001f, 0.0001f, 0.0001f,
    0.0001f, 0.0001f, 0.0001f, 0.0001f, 0.0001f, 0.0001f, 0.0001f, 0.0002f,
    0.0002f, 0.0002f, 0.0002f, 0.0002f, 0.0002f, 0.0002f, 0.0002f, 0.0003f,
    0.0003f, 0.0003f, 0.0003f, 0.0003f, 0.0003f, 0.0004f, 0.0004f, 0.0004f,
    0.0004f, 0.0004f, 0.0005f, 0.0005f, 0.0005f, 0.0006f, 0.0006f, 0.0006f,
    0.0007f, 0.0007f, 0.0007f, 0.0008f, 0.0008f, 0.0009f, 0.0009f, 0.0010f,
    0.0011f, 0.0011f, 0.0012f, 0.0013f, 0.0013f, 0.0014f, 0.0015f, 0.0016f,
    0.0017f, 0.0018f, 0.0019f, 0.0020f, 0.0021f, 0.0022f, 0.0024f, 0.0025f,
    0.0027f, 0.0028f, 0.0030f, 0.0032f, 0.0033f, 0.0035f, 0.0038f, 0.0040f,
    0.0042f, 0.0045f, 0.0047f, 0.0050f, 0.0053f, 0.0056f, 0.0060f, 0.0063f,
    0.0067f, 0.0071f, 0.0075f, 0.0079f, 0.0084f, 0.0089f, 0.0094f, 0.0100f,
    0.0106f, 0.0112f, 0.0119f, 0.0126f, 0.0133f, 0.0141f, 0.0150f, 0.0158f,
    0.0168f, 0.0178f, 0.0188f, 0.0200f, 0.0211f, 0.0224f, 0.0237f, 0.0251f,
    0.0266f, 0.0282f, 0.0299f, 0.0316f, 0.0335f, 0.0355f, 0.0376f, 0.0398f,
    0.0422f, 0.0447f, 0.0473f, 0.0501f, 0.0531f, 0.0562f, 0.0596f, 0.0631f,
    0.0668f, 0.0708f, 0.0750f, 0.0794f, 0.0841f, 0.0891f, 0.0944f, 0.1000f,
    0.1059f, 0.1122f, 0.1189f, 0.1259f, 0.1334f, 0.1413f, 0.1496f, 0.1585f,
    0.1679f, 0.1778f, 0.1884f, 0.1995f, 0.2113f, 0.2239f, 0.2371f, 0.2512f,
    0.2661f, 0.2818f, 0.2985f, 0.3162f, 0.3350f, 0.3548f, 0.3758f, 0.3981f,
    0.4217f, 0.4467f, 0.4732f, 0.5012f, 0.5309f, 0.5623f, 0.5957f, 0.6310f,
    0.6683f, 0.7079f, 0.7499f, 0.7943f, 0.8414f, 0.8913f, 0.9441f, 1.0000f  // 0db
};

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
// VERIFY: 771b045f manual
// OFFSET: 0x008FA880
FMOD_RESULT CodecMIDISubChannel::updateVolume()
{
    float eg1 = 1.0f, velocity, channelvol, expression, attenuation, tremolo = 1.0f;

    if (mVolumeEnvelope.mActive)
    {
        /*
            Handle a pre-mature keyoff.
        */
        if ((mKeyOff && !mParent->mDamperPedal) && mVolumeEnvelope.mPosition != CODEC_DLS_ENVPOINT_RELEASE)
        {
            CodecDLSEnvelopePoint *point;
            float src;

            point = &mVolumeEnvelope.mPoint[mVolumeEnvelope.mPosition];

            src = point->mSrcValue + (((point->mDestValue - point->mSrcValue) / point->mTime) * mVolumeEnvelope.mTime);

            if (mVolumeEnvelope.mPosition == CODEC_DLS_ENVPOINT_ATTACK)
            {
                /*
                    Turn the linear value into dB so it scales properly with the release segment which is logarithmic.
                */
                src = 1.0f - (src / -96.0f);                
                src = log10f(src) * 20.0f;
            }            
            if (mVolumeEnvelope.mPosition == CODEC_DLS_ENVPOINT_DECAY && src < mVolumeEnvelope.mSustain)
            {
                src = mVolumeEnvelope.mSustain;
            }

            mVolumeEnvelope.mPosition = CODEC_DLS_ENVPOINT_RELEASE;
            point = &mVolumeEnvelope.mPoint[mVolumeEnvelope.mPosition];

            mVolumeEnvelope.mTime = (src - point->mSrcValue) / ((point->mDestValue - point->mSrcValue) / point->mTime);
        }

        /*
            Increment through A/D/S/R stages if nescessary.
        */
        while (mVolumeEnvelope.mTime >= mVolumeEnvelope.mPoint[mVolumeEnvelope.mPosition].mTime && mVolumeEnvelope.mPosition < CODEC_DLS_ENVPOINT_MAX)
        {
            if (mVolumeEnvelope.mPosition == CODEC_DLS_ENVPOINT_DECAY && !(mKeyOff && !mParent->mDamperPedal))
            {
                mVolumeEnvelope.mTime = mVolumeEnvelope.mPoint[mVolumeEnvelope.mPosition].mTime;
                break;
            }
            else
            {
                mVolumeEnvelope.mTime -= mVolumeEnvelope.mPoint[mVolumeEnvelope.mPosition].mTime;
                mVolumeEnvelope.mPosition = (CODEC_DLS_ENVPOINT)(mVolumeEnvelope.mPosition + 1);
            }
        }
    
        /*
            If the release is completed, stop the sound.
        */
        if (mVolumeEnvelope.mPosition >= CODEC_DLS_ENVPOINT_MAX)
        {
            return stop();
        }
        /*
            Otherwise calculate the nescessary volume.
        */
        else
        {
            CodecDLSEnvelopePoint *point = &mVolumeEnvelope.mPoint[mVolumeEnvelope.mPosition];

            if (point->mTime > 0)
            {
                eg1 = point->mSrcValue + (((point->mDestValue - point->mSrcValue) / point->mTime) * mVolumeEnvelope.mTime);
            }
            else
            {
                eg1 = point->mSrcValue;
            }

            if (mVolumeEnvelope.mPosition == CODEC_DLS_ENVPOINT_DECAY && eg1 < mVolumeEnvelope.mSustain)
            {
                eg1 = mVolumeEnvelope.mSustain;
            }

            /*
                Attack segment is linear.
            */
            if (mVolumeEnvelope.mPosition == CODEC_DLS_ENVPOINT_ATTACK)
            {
                eg1 = 1.0f - (eg1 / -96.0f);
            }
            else
            {
                #ifdef USETAB

                eg1 = gDBToLinearTable[192 - (int)(eg1 * -2.0f)];

                #else
            
                eg1 = FMOD_POW(10.0f, eg1/20.0f);

                #endif
            }
        }
    }
    
    velocity    = (float)(mKeyOnVelocity          * mKeyOnVelocity         ) / (127.0f * 127.0f);
    channelvol  = (float)(mParent->mVolume        * mParent->mVolume       ) / (127.0f * 127.0f);
    expression  = (float)(mParent->mExpression    * mParent->mExpression   ) / (127.0f * 127.0f);

    if (mLFOTime >= mLFOStartDelay)
    {
        tremolo = 1.0f + (FMOD_SIN((mLFOTime - mLFOStartDelay) / 1000.0f * FMOD_PI2 * mLFOFrequency) * mTremoloScale);
        if (tremolo < 0.0f)
        {
            tremolo = 0.0f;
        }
        if (tremolo > 1.0f)
        {
            tremolo = 1.0f;
        }
    }

    attenuation = eg1 * velocity * channelvol * expression * mSampleAttenuation * tremolo;

    if (mVolumeEnvelope.mPosition == CODEC_DLS_ENVPOINT_RELEASE && attenuation < (1.0f / 1024.0f))
    {
        return stop();
    }

    mChannel.setVolume(attenuation);

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
// VERIFY: 74904710 manual
// OFFSET: 0x008FAB40
FMOD_RESULT CodecMIDISubChannel::updatePitch()
{
    float eg2 = 0;

    if (mPitchEnvelope.mActive)
    {
        /*
            Handle a pre-mature keyoff.
        */
        if ((mKeyOff && !mParent->mDamperPedal) && mPitchEnvelope.mPosition != CODEC_DLS_ENVPOINT_RELEASE)
        {
            CodecDLSEnvelopePoint *point;
            float src;

            point = &mPitchEnvelope.mPoint[mPitchEnvelope.mPosition];

            src = point->mSrcValue + (((point->mDestValue - point->mSrcValue) / point->mTime) * mPitchEnvelope.mTime);

            if (mPitchEnvelope.mPosition == CODEC_DLS_ENVPOINT_DECAY && src < mPitchEnvelope.mSustain)
            {
                src = mPitchEnvelope.mSustain;
            }

            mPitchEnvelope.mPosition = CODEC_DLS_ENVPOINT_RELEASE;
            point = &mPitchEnvelope.mPoint[mPitchEnvelope.mPosition];

            mPitchEnvelope.mTime = (src - point->mSrcValue) / ((point->mDestValue - point->mSrcValue) / point->mTime);
        }

        /*
            Increment through A/D/S/R stages if nescessary.
        */
        while (mPitchEnvelope.mTime >= mPitchEnvelope.mPoint[mPitchEnvelope.mPosition].mTime && mPitchEnvelope.mPosition < CODEC_DLS_ENVPOINT_MAX)
        {
            if (mPitchEnvelope.mPosition == CODEC_DLS_ENVPOINT_DECAY && mPitchEnvelope.mSustain > 0 && !(mKeyOff && !mParent->mDamperPedal))
            {
                mPitchEnvelope.mTime = mPitchEnvelope.mPoint[mPitchEnvelope.mPosition].mTime;
                break;
            }
            else
            {
                mPitchEnvelope.mTime -= mPitchEnvelope.mPoint[mPitchEnvelope.mPosition].mTime;
                mPitchEnvelope.mPosition = (CODEC_DLS_ENVPOINT)(mPitchEnvelope.mPosition + 1);
            }
        }
    
        if (mPitchEnvelope.mPosition >= CODEC_DLS_ENVPOINT_MAX)
        {
            eg2 = 0;
            mPitchEnvelope.mActive = false;
        }
        else
        {
            CodecDLSEnvelopePoint *point = &mPitchEnvelope.mPoint[mPitchEnvelope.mPosition];

            if (point->mTime == 0)
            {
                eg2 = point->mSrcValue;
            }
            else
            {
                eg2 = point->mSrcValue + (((point->mDestValue - point->mSrcValue) / point->mTime) * mPitchEnvelope.mTime);
            }

            if (mPitchEnvelope.mPosition == CODEC_DLS_ENVPOINT_DECAY && eg2 < mPitchEnvelope.mSustain)
            {
                eg2 = mPitchEnvelope.mSustain;
            }

            eg2 *= mPitchEnvelope.mRange;
        }
    }
    
    /*
        Calculate frequency based on envelope, pitch wheel, key number, fine tune and unity.
    */
    {
        float pw, kn, ft, unity, freq, pitch, vibrato, deffreq;
        float sensitivity = mParent->mPitchBendSensitivity / 256.0f * 100.0f;
    
        pw      = sensitivity * ((float)(mParent->mPitchBend) / 8192.0f);   /* Pitch wheel */
        kn      = (float)mMiddleC * (float)mKeyOnKey / 128.0f;              /* Key number */
        ft      = (float)mFineTune;                                         /* Fine tune */
        unity   = (float)mUnityNote * 100.0f;                               /* Unity */
    
        if (mLFOTime >= mLFOStartDelay)
        {
            vibrato = FMOD_SIN((mLFOTime - mLFOStartDelay) / 1000.0f * FMOD_PI2 * mLFOFrequency) * mVibratoScale;
        }
        else
        {
            vibrato = 0;
        }

        pitch = eg2 + pw + kn + ft - unity + vibrato;

        freq = FMOD_POW(2.0f, (pitch / 1200));
    
        mSound->getDefaults(&deffreq, 0, 0, 0);

        freq *= deffreq;

        //printf("freq %s : %s = %d\n", mInstrument->mName, note[mKeyOnKey], (int)freq);

        mChannel.setFrequency(freq);
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
// VERIFY: cffdb6bd auto
// OFFSET: 0x008FADD0
FMOD_RESULT CodecMIDISubChannel::updatePan()
{                                
    mChannel.setPan(((float)mParent->mPan / 64.0f) - 1.0f);

    return FMOD_OK;
}

/*
    ==========================================================================================

    CODEC MIDI CHANNEL CLASS 

    ==========================================================================================
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
// VERIFY: b28d471e auto
// OFFSET: 0x008FAE00
FMOD_RESULT CodecMIDIChannel::getSound(int key, SoundI **sound, CodecDLSInstrument **instrument, int *unitynote, int *finetune, int *sampleattenuation, bool *duplicateallowed, int *keygroup, int *numarticulators, DLS_CONNECTIONBLOCK **articulators)
{
    CodecDLS *dls = mTrack->mMIDI->mDLS;

    for (int count = 0; count < dls->mNumInstruments; count++)
    {
        CodecDLSInstrument *inst = &dls->mInstrument[count];

        if (inst->mHeader.Locale.ulBank == mBank && inst->mHeader.Locale.ulInstrument == mProgram)
        {
            FMOD_RESULT  result   = FMOD_OK;            
            unsigned int region   = 0; 
            int          sampleid = 0;
    
            *instrument = inst;

/*MGB       FLOG((FMOD_DEBUG_LEVEL_LOG, __FILE__, __LINE__, "CodecMIDIChannel::getSound", "Key = %s\n", note[key]));
            for (region = 0; region < inst->mHeader.cRegions; region++)
            {
                FLOG((FMOD_DEBUG_LEVEL_LOG, __FILE__, __LINE__, "CodecMIDIChannel::getSound", "%s (%d): Region %d: low %s to high %s\n", inst->mName, mIndex, region, note[inst->mRegion[region].mRegionHeader.RangeKey.usLow], note[inst->mRegion[region].mRegionHeader.RangeKey.usHigh]));
            }*/

            for (region = 0; region < inst->mHeader.cRegions; region++)
            {
                if (key >= inst->mRegion[region].mRegionHeader.RangeKey.usLow && key <= inst->mRegion[region].mRegionHeader.RangeKey.usHigh)
                {
                    sampleid = inst->mRegion[region].mWaveLink.ulTableIndex;
                    
                    // If present, use the wave sample in the region
                    if (inst->mRegion[region].mWaveSample.cbSize != 0)
                    {
                        *unitynote         = inst->mRegion[region].mWaveSample.usUnityNote;
                        *finetune          = inst->mRegion[region].mWaveSample.sFineTune;
                        *sampleattenuation = inst->mRegion[region].mWaveSample.lAttenuation;
                    }
                    // Otherwise, if present, use the wave sample referenced
                    else if (sampleid >= 0 && sampleid < dls->mNumSamples)
                    {
                        *unitynote         = dls->mSample[sampleid].mWaveSample.usUnityNote;
                        *finetune          = dls->mSample[sampleid].mWaveSample.sFineTune;
                        *sampleattenuation = dls->mSample[sampleid].mWaveSample.lAttenuation;
                    }
                    // Otherwise, use the default
                    else
                    {
                        *unitynote         = 60; // Middle C
                        *finetune          = 0;
                        *sampleattenuation = 0;
                    }

                    *keygroup          = inst->mRegion[region].mRegionHeader.usKeyGroup;
                    *numarticulators   = inst->mRegion[region].mNumConnectionBlocks;
                    *articulators      = inst->mRegion[region].mConnectionBlock;

                    if (inst->mRegion[region].mRegionHeader.fusOptions & F_RGN_OPTION_SELFNONEXCLUSIVE)
                    {
                        // *duplicateallowed = true;    /* What the hell?  Dont support this?  The only song that does use it i've seen sounds wrong when trying to allow each duplicate to play. */
                    }

                    break;
                }
            }

            result = mTrack->mMIDI->mDLSSound->getSubSound(sampleid, sound);
            if (result != FMOD_OK)
            {
                FLOG((FMOD_DEBUG_LEVEL_LOG, __FILE__, __LINE__, "CodecMIDIChannel::getSound", "Cannot find sample (%d) for instrument %s (%d): Key %s (%d)\n", sampleid, inst->mName, mIndex, note[key], key));
                return result;
            }
            
            if (mTrack->mMIDI->mSampleInclusionList && !*sound)
            {
                mTrack->mMIDI->mSampleInclusionList[sampleid] = true;   /* This is for the loading stage when it scans the midi to see what samples should be actually loaded from disk. */
            }

            if (inst->mNumConnectionBlocks && inst->mConnectionBlock)
            {
                *numarticulators = inst->mNumConnectionBlocks;
                *articulators = inst->mConnectionBlock;
            }

//MGB            FLOG((FMOD_DEBUG_LEVEL_LOG, __FILE__, __LINE__, "CodecMIDIChannel::getSound", "%s (%d): Using region %d, unity %s, fine tune %d, sample '%s'\n\n", inst->mName, mIndex, region, note[*unitynote], *finetune, dls->mSample[sampleid].mName));

            break;
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
// VERIFY: d334e56c manual
// OFFSET: 0x008FB560
FMOD_RESULT CodecMIDIChannel::process(unsigned char event, bool reuse, unsigned char runningdata, bool calledfromopen)
{
    FMOD_RESULT result;
    CodecMIDISubChannel *current;
    int bankchange = false;

    if (mIndex == 10 && mBank != MIDI_F_INSTRUMENT_DRUMS)
    {
        mBank = MIDI_F_INSTRUMENT_DRUMS;
    }

    switch (event & 0xF0)
    {
        case MIDI_VOICE_NOTEOFF:
        {
            unsigned char keyoffkey;

            if (reuse)
            {
                keyoffkey = runningdata;
            }
            else
            {
                result = mTrack->readByte(&keyoffkey);
                if (result != FMOD_OK)
                {
                    break;
                }
            }
            result = mTrack->readByte(&mKeyOffVelocity);
            if (result != FMOD_OK)
            {
                break;
            }

            if (calledfromopen)
            {
                break;
            }

            current = (CodecMIDISubChannel *)mChannelHead.getNext();
            while (current != &mChannelHead)
            {
                if (keyoffkey == current->mCurrentNote)
                {
                    current->mKeyOff = true;
                    current->mCurrentNote = -1;
                    break;
                }
                current = (CodecMIDISubChannel *)current->getNext();
            }

//            printf("NOTE OFF: Track %d, KEY %s: velocity %d\n", mIndex, keyoffkey < 128 ? note[keyoffkey]: "???", mKeyOffVelocity);
            break;
        }
        case MIDI_VOICE_NOTEON:
        {
            unsigned char        keyonkey;
            int                  unitynote = 0, finetune = 0, sampleattenuation = 0, keygroup = 0;
            bool                 duplicateallowed = false;
            SoundI              *sound = 0;
            CodecDLSInstrument  *instrument = 0;
            int                  numarticulators = 0;
            DLS_CONNECTIONBLOCK *articulators = 0;

            if (reuse)
            {
                keyonkey = runningdata;
            }
            else
            {
                result = mTrack->readByte(&keyonkey);
                if (result != FMOD_OK)
                {
                    break;
                }
            }

            result = mTrack->readByte(&mKeyOnVelocity);
            if (result != FMOD_OK)
            {
                break;
            }

            result = getSound(keyonkey, &sound, &instrument, &unitynote, &finetune, &sampleattenuation, &duplicateallowed, &keygroup, &numarticulators, &articulators);

            if (calledfromopen)
            {
                break;
            }

            if (result == FMOD_OK && sound)
            {
                if (mKeyOnVelocity)
                {
                    FMOD_RESULT  result;
                    ChannelReal *realchannel;

                    /*
                        If a playing note is in the same keygroup as the requested sound, kill it.
                    */
                    current = (CodecMIDISubChannel *)mChannelHead.getNext();
                    while (current != &mChannelHead)
                    {
                        CodecMIDISubChannel *next = (CodecMIDISubChannel *)current->getNext();

                        if (keygroup && keygroup == current->mKeyGroup)
                        {
                            current->stop();
                        }

                        current = next;
                    }

                    if (mTrack->mMIDI->mChannelFreeListHead.isEmpty())
                    {
                        float lowestvol = 9999.0f, lowestinreleasevol = 9999.0f;
                        int   count;
                        CodecMIDISubChannel *lowestinrelease = 0;
                        CodecMIDISubChannel *lowest = 0;

                        /*
                            Steal an existing channel.
                        */
                        for (count = 0; count < 16; count++)
                        {
                            /*
                                Find a channel in release phase below volume of 10
                            */
                            current = (CodecMIDISubChannel *)mTrack->mMIDI->mMIDIChannel[count].mChannelHead.getNext();
                            while (current != &mTrack->mMIDI->mMIDIChannel[count].mChannelHead)
                            {
                                float vol;

                                current->mChannel.getVolume(&vol);
                                if (current->mVolumeEnvelope.mPosition == CODEC_DLS_ENVPOINT_RELEASE && vol < lowestinreleasevol)
                                {
                                    lowestinreleasevol = vol;
                                    lowestinrelease    = current;
                                }
                                if (vol < lowestvol)
                                {
                                    lowestvol = vol;
                                    lowest    = current;
                                }

                                current = (CodecMIDISubChannel *)current->getNext();;
                            }
                        }

                        if (lowestinrelease)
                        {
                            lowestinrelease->stop();
                        }
                        else
                        {
                            lowest->stop();
                        }
                    }

                    current = (CodecMIDISubChannel *)mTrack->mMIDI->mChannelFreeListHead.getNext();
                    current->removeNode();
                    current->addAfter(&mChannelHead);

                    current->mCurrentNote       = keyonkey;
                    current->mParent            = this;
                    current->mSound             = sound;
                    current->mKeyOnKey          = keyonkey;
                    current->mUnityNote         = unitynote;
                    current->mFineTune          = finetune;
                    current->mKeyOnVelocity     = mKeyOnVelocity;

                    current->mInstrument        = instrument;
                    current->mKeyGroup          = keygroup;
                    current->mSampleAttenuation = FMOD_POW(10.0f, (float)sampleattenuation / (200.0f * 65536.0f) );
                    current->mNumArticulators   = numarticulators;
                    current->mArticulator       = articulators;

                    result = current->setUpArticulators();
                    if (result != FMOD_OK)
                    {
                        return result;
                    }

                    result = mTrack->mMIDI->mChannelPool->allocateChannel(&realchannel, FMOD_CHANNEL_FREE, 1, 0);
                    if (result == FMOD_OK)
                    {
//MGB                        FLOG((FMOD_DEBUG_LEVEL_LOG, __FILE__, __LINE__, "CodecMIDIChannel::process", "play %s : %s : Velocity = %d\n", instrument->mName, note[keyonkey], mKeyOnVelocity));

                        current->mChannel.mRealChannel[0] = realchannel;

                        result = current->mChannel.play(sound, true, true, false);
                        if (result != FMOD_OK)
                        {
                            return result;
                        }
                                       
                        result = current->updatePitch();
                        if (result != FMOD_OK)
                        {
                            return result;
                        }

                        result = current->updateVolume();
                        if (result != FMOD_OK)
                        {
                            return result;
                        }

                        result = current->updatePan();
                        if (result != FMOD_OK)
                        {
                            return result;
                        }

                        current->mChannel.setPaused(false);
                    }
                    else
                    {
                        printf("ran out of FMOD channels?????\n");
                    }
                }
                else
                {
                    current = (CodecMIDISubChannel *)mChannelHead.getNext();
                    while (current != &mChannelHead)
                    {
                        if (keyonkey == current->mCurrentNote)
                        {
                            current->mKeyOff = true;
                            current->mCurrentNote = -1;
                            break;
                        }

                        current = (CodecMIDISubChannel *)current->getNext();
                    }
                }
            }

//                printf("NOTE ON: Track %d (%d), delta %6d, KEY %s: velocity %d\n", mIndex, channel, delta, keyonkey < 128 ? note[keyonkey]: "???", mKeyOnVelocity);
            break;
        }
        case MIDI_VOICE_AFTERTOUCH:
        {
            if (reuse)
            {
                mAfterTouchKey = runningdata;
            }
            else
            {
                result = mTrack->readByte(&mAfterTouchKey);
                if (result != FMOD_OK)
                {
                    break;
                }
            }
            result = mTrack->readByte(&mAfterTouchPressure);
            if (result != FMOD_OK)
            {
                break;
            }
//                printf("AFTERTCH: Track %d (%d), delta %6d, KEY %s: velocity %d\n", mIndex, channel, delta, mAfterTouchKey < 128 ? note[mAfterTouchKey]: "???", mAfterTouchPressure);
            break;
        }
        case MIDI_VOICE_CONTROLLERCHANGE:
        {
            unsigned char number, value;

            if (reuse)
            {
                number = runningdata;
            }
            else
            {
                result = mTrack->readByte(&number);
                if (result != FMOD_OK)
                {
                    break;
                }
            }

            result = mTrack->readByte(&value);
            if (result != FMOD_OK)
            {
                break;
            }

            switch (number)
            {
                case MIDI_CONTROLLERCHANGE_BANKSELECT_MSB:
                {
                    if (mIndex != 10) /* Drum track.  Not allowed to do this.  (Program change yes, but bank select.  No. It should always be MIDI_F_INSTRUMENT_DRUMS */
                    {
                        mBank = (unsigned int)value << 8;
                    }
                    bankchange = true;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_MODULATIONWHEEL_MSB:
                {
                    mModWheel |= value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_BREATHCONTROL_MSB:
                {
                    break;
                }
                case MIDI_CONTROLLERCHANGE_FOOTCONTROLLER_MSB:
                {
                    break;
                }
                case MIDI_CONTROLLERCHANGE_PORTAMENTOTIME_MSB:
                {
                    break;
                }
                case MIDI_CONTROLLERCHANGE_DATAENTRY_MSB:
                {
                    switch (mRPN)
                    {
                        case MIDI_CONTROLLERCHANGE_RPN_PITCHBENDSENSITIVITY:
                        {
                            mPitchBendSensitivity = (value << 8);
                            break;
                        }
                    }
                    break;
                }
                case MIDI_CONTROLLERCHANGE_CHANNELVOLUME_MSB:
                {
                    mVolume = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_BALANCE_MSB:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_PAN_MSB:
                {
                    mPan = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_EXPRESSIONCONTROLLER_MSB:
                {
                    mExpression = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_EFFECTCONTROL1_MSB:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_EFFECTCONTROL2_MSB:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_GENERALPURPOSECONTROLLER1_MSB:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_GENERALPURPOSECONTROLLER2_MSB:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_GENERALPURPOSECONTROLLER3_MSB:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_GENERALPURPOSECONTROLLER4_MSB:
                {
                    value = value;
                    break;
                }

                case MIDI_CONTROLLERCHANGE_BANKSELECT_LSB:
                {
                    mBank |= (unsigned int)value;
                    bankchange = true;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_MODULATIONWHEEL_LSB:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_BREATHCONTROL_LSB:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_FOOTCONTROLLER_LSB:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_PORTAMENTOTIME_LSB:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_DATAENTRY_LSB:
                {
                    switch (mRPN)
                    {
                        case MIDI_CONTROLLERCHANGE_RPN_PITCHBENDSENSITIVITY:
                        {
                            mPitchBendSensitivity |= value;
                            break;
                        }
                    }
                    break;
                }
                case MIDI_CONTROLLERCHANGE_CHANNELVOLUME_LSB:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_BALANCE_LSB:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_PAN_LSB:
                {
                    value = value;    /* implement */
                    break;
                }
                case MIDI_CONTROLLERCHANGE_EXPRESSIONCONTROLLER_LSB:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_EFFECTCONTROL1_LSB:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_EFFECTCONTROL2_LSB:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_GENERALPURPOSECONTROLLER1_LSB:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_GENERALPURPOSECONTROLLER2_LSB:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_GENERALPURPOSECONTROLLER3_LSB:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_GENERALPURPOSECONTROLLER4_LSB:
                {
                    value = value;
                    break;
                }

                case MIDI_CONTROLLERCHANGE_DAMPERPEDALONOFF:
                {
                    if (value == 0)
                    {
                        mDamperPedal = false;
                    }
                    else
                    {
                        mDamperPedal = true;
                    }
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_PORTAMENTOONOFF:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_SUSTENUTOONOFF:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_SOFTPEDALONOFF:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_LEGATOFOOTSWITCH:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_HOLD2:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_SOUNDCONTROLLER1:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_SOUNDCONTROLLER2:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_SOUNDCONTROLLER3:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_SOUNDCONTROLLER4:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_SOUNDCONTROLLER5:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_SOUNDCONTROLLER6:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_SOUNDCONTROLLER7:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_SOUNDCONTROLLER8:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_SOUNDCONTROLLER9:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_SOUNDCONTROLLER10:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_GENERALPURPOSECONTROLLER5:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_GENERALPURPOSECONTROLLER6:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_GENERALPURPOSECONTROLLER7:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_GENERALPURPOSECONTROLLER8:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_PORTAMENTOCONTROL:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_EFFECTS1DEPTH:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_EFFECTS2DEPTH:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_EFFECTS3DEPTH:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_EFFECTS4DEPTH:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_EFFECTS5DEPTH:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_DATAENTRYINC:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_DATAENTRYDEC:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_NONREGISTEREDPARAMNUMBER_LSB:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_NONREGISTEREDPARAMNUMBER_MSB:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_REGISTEREDPARAMETERNUMBER_LSB:
                {
                    mRPN |= value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_REGISTEREDPARAMETERNUMBER_MSB:
                {
                    mRPN = value << 8;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_ALLSOUNDOFF:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_RESETALLCONTROLLERS:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_LOCALCONTROLONOFF:
                {
                    value = value;    /* implement */
                    break;
                }
                case MIDI_CONTROLLERCHANGE_ALLNOTESOFF:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_OMNIMODEOFF:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_OMNIMODEON:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_POLYMODEONOFF:
                {
                    value = value;
                    break;
                }
                case MIDI_CONTROLLERCHANGE_POLYMODEON:
                {
                    value = value;
                    break;
                }
            }

            break;
        }
        case MIDI_VOICE_PROGRAMCHANGE:
        {
            if (reuse)
            {
                mProgram = runningdata;
            }
            else
            {
                result = mTrack->readByte(&mProgram); 
                if (result != FMOD_OK)
                {
                    break;
                }
            }
   
            /*
                Check if the drum set actually exists in the DLS set.  If it doesn't, reset to the default set.
            */    
            if (mIndex == 10)
            {
                CodecDLS *dls = mTrack->mMIDI->mDLS;
                int count;
                bool found = false;

                for (count = 0; count < dls->mNumInstruments; count++)
                {
                    CodecDLSInstrument *inst = &dls->mInstrument[count];

                    if (inst->mHeader.Locale.ulBank == mBank && inst->mHeader.Locale.ulInstrument == mProgram)
                    {
                        found = true;
                    }
                }

                if (!found)
                {
                    mProgram = 0;
                }
            }

            break;
        }
        case MIDI_VOICE_CHANNELKEYPRESSURE:
        {
            if (reuse)
            {
                mChannelPressure = runningdata;
            }
            else
            {
                result = mTrack->readByte(&mChannelPressure);
                if (result != FMOD_OK)
                {
                    break;
                }
            }
            break;
        }
        case MIDI_VOICE_PITCHBEND:
        {
            unsigned char msb, lsb;

            if (reuse)
            {
                lsb = runningdata;
            }
            else
            {
                result = mTrack->readByte(&lsb);
                if (result != FMOD_OK)
                {
                    break;
                }
            }
            result = mTrack->readByte(&msb);
            if (result != FMOD_OK)
            {
                break;
            }

            mPitchBend = (((int)msb - 0x40) << 7) | (int)lsb;
            break;
        }
        default:
        {
//                    mOffset += length;
            event = event;
            break;
        }
    };

    /*
        Check if the new program + bank combo exists.
    */
    if (bankchange)
    {
        if (mBank)
        {
            CodecDLS *dls = mTrack->mMIDI->mDLS;
            int count;
            bool found = false;

            for (count = 0; count < dls->mNumInstruments; count++)
            {
                CodecDLSInstrument *inst = &dls->mInstrument[count];

                if (inst->mHeader.Locale.ulBank == mBank && inst->mHeader.Locale.ulInstrument == mProgram)
                {
                    found = true;
                }
            }

            if (!found)
            {
                mBank = 0;
            }
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
// VERIFY: 9f6a687c manual
// OFFSET: 0x008FB010
FMOD_RESULT CodecMIDIChannel::update()
{
    CodecMIDISubChannel *current;

    current = (CodecMIDISubChannel *)mChannelHead.getNext();
    while (current != &mChannelHead)
    {
        CodecMIDISubChannel *next = (CodecMIDISubChannel *)current->getNext();

        if (current->mInstrument)
        {
            current->updateVolume();
            current->mVolumeEnvelope.mTime += mTrack->mMIDI->mMillisecondsPerTick;

            current->updatePitch();
            current->mPitchEnvelope.mTime += mTrack->mMIDI->mMillisecondsPerTick;

            current->updatePan();
//            current->mPanEnvelope.mTime += mTrack->mMIDI->mMillisecondsPerTick;
    
            current->mLFOTime += mTrack->mMIDI->mMillisecondsPerTick;
        }

        current = next;
    }

    return FMOD_OK;
}


/*
    ==========================================================================================

    CODEC MIDI TRACK CLASS 

    ==========================================================================================
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
// VERIFY: b683318b manual
// OFFSET: 0x008FB0C0
FMOD_RESULT CodecMIDITrack::readVarLen(unsigned int *result)
{
    unsigned int   value       = 0;
    unsigned char  ch          = 0;

	do
	{
		if (mOffset >= mLength)
		{
			mFinished = true;
			return FMOD_ERR_FILE_EOF;
		}

		value = (value << 7) + ((ch = mData[mOffset++]) & 0x7F);

	} while (ch & 0x80); 

    *result = value;
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
// VERIFY: 211ef286 auto
// OFFSET: 0x008FB130
FMOD_RESULT CodecMIDITrack::readByte(unsigned char *result)
{
    if (mOffset >= mLength)
    {
        mFinished = true;
        return FMOD_ERR_FILE_EOF;
    }

    *result = mData[mOffset++];

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
// VERIFY: 6a8f360f auto
// OFFSET: 0x008FB160
FMOD_RESULT CodecMIDITrack::read(void *buff, int length)
{        
    if (mOffset >= mLength)
    {
        mFinished = true;
        return FMOD_ERR_FILE_EOF;
    }
    
    if (mOffset + length > mLength)
    {
        length = mLength - mOffset;
    }

    if (buff)
    {
        FMOD_memcpy(buff, mData + mOffset, length);
    }

    mOffset += length;

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
// VERIFY: d3a9b570 auto
// OFFSET: 0x008FB1B0
FMOD_RESULT CodecMIDITrack::addTag(const char *name, int length, bool calledfromopen)
{
    FMOD_RESULT result;
    void *data;

    if (!calledfromopen)
    {
        return read(0, length);
    }

    data = FMOD_Memory_Calloc(length);
    if (!data)
    {
        return FMOD_ERR_MEMORY;
    }

    result = read(data, length);
    if (result != FMOD_OK)
    {
        return result;
    }

    result = mMIDI->metaData(FMOD_TAGTYPE_MIDI, name, data, length, FMOD_TAGDATATYPE_STRING, false);

    FMOD_Memory_Free(data);

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
// VERIFY: 72520853 manual
// OFFSET: 0x008FBD90
FMOD_RESULT CodecMIDITrack::process(bool calledfromopen)
{
    unsigned char event;
    
    if (!mData)
    {
        return FMOD_OK;
    }

    if (!mFinished)
    {
        FMOD_RESULT result;

        while (mTick <= mMIDI->mTick)
        {
            if (!mReadDelta)
            {
                unsigned int delta;

                result = readVarLen(&delta);
                if (result != FMOD_OK)
                {
                    return result;
                }
                mTick += (float)delta;
                mReadDelta = false;
            }

            if (mTick > mMIDI->mTick)
            {
                mReadDelta = true;
                break;
            }

            mReadDelta = false;

            result = readByte(&event);
            if (result != FMOD_OK)
            {
                return result;
            }
    
            if (event >= 0xF0)
            {
                switch (event)
                {
                    case MIDI_SYSEXESCAPE:
                    {
                        unsigned int length;

                        result = readVarLen(&length);
                        if (result != FMOD_OK)
                        {
                            break;
                        }

                        break;
                    }
                    case MIDI_SYSEXEVENT:
                    {
                        unsigned int length;

                        result = readVarLen(&length);
                        if (result != FMOD_OK)
                        {
                            break;
                        }

                        mOffset += length;

                        break;
                    }
                    case MIDI_METAEVENT:
                    {
                        unsigned char type;
                        unsigned int length;

                        result = readByte(&type);
                        if (result != FMOD_OK)
                        {
                            break;
                        }

                        result = readVarLen(&length);
                        if (result != FMOD_OK)
                        {
                            break;
                        }

                        switch (type)
                        {
                            case MIDI_METAEVENT_SEQNUMBER:
                            {
                                if (!length)
                                {
                                    mMIDI->mSequenceNumber = mIndex;
                                }
                                else
                                {
                                    if (length > 4) /* Eh?  I dont know how but 1 midi did this */
                                    {
                                        result = read(0, length);
                                        if (result != FMOD_OK)
                                        {
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        result = read(&mMIDI->mSequenceNumber, length);
                                        if (result != FMOD_OK)
                                        {
                                            break;
                                        }
                                    }
                                }
                                break;
                            }
                            case MIDI_METAEVENT_TEXT:
                            {
                                result = addTag("Text", length, calledfromopen);
                                break;
                            }
                            case MIDI_METAEVENT_COPYRIGHT:
                            {
                                result = addTag("Copyright", length, calledfromopen);
                                break;
                            }
                            case MIDI_METAEVENT_NAME:
                            {
                                char s[256];

                                sprintf(s, "Track %d Name", mIndex);
                        
                                result = addTag(s, length, calledfromopen);
                                break;
                            }
                            case MIDI_METAEVENT_INSTRUMENT:
                            {
                                result = addTag("Instrument", length, calledfromopen);
                                break;
                            }
                            case MIDI_METAEVENT_LYRIC:
                            {
                                result = addTag("Lyric", length, calledfromopen);
                                break;
                            }
                            case MIDI_METAEVENT_MARKER:
                            {
                                result = addTag("Marker", length, calledfromopen);
                                break;
                            }
                            case MIDI_METAEVENT_CUEPOINT:
                            {
                                result = addTag("Cue Point", length, calledfromopen);
                                break;
                            }
                            case MIDI_METAEVENT_PATCHNAME:
                            {
                                result = addTag("Patch Name", length, calledfromopen);
                                break;
                            }
                            case MIDI_METAEVENT_PORTNAME:
                            {
                                result = addTag("Port Name", length, calledfromopen);
                                break;
                            }
                            case MIDI_METAEVENT_CHANNEL:
                            {
                                unsigned char dummy;
                                result = readByte(&dummy);
                                break;
                            }
                            case MIDI_METAEVENT_PORT:
                            {
                                result = readByte(&mPort);
                                break;
                            }
                            case MIDI_METAEVENT_ENDOFTRACK:
                            {
                                mFinished = true;
                                break;
                            }
                            case MIDI_METAEVENT_TEMPO:
                            {
                                unsigned char ttt[3];

                                result = read(&ttt[0], length);

                                mMIDI->mTempo =  ttt[0] << 16;
                                mMIDI->mTempo |= ttt[1] << 8;
                                mMIDI->mTempo |= ttt[2] << 0;

                                // Tempo is microseconds per MIDI quarter-note
                                mMIDI->mMillisecondsPerTick = ((float)mMIDI->mTempo / (float)mMIDI->mDivision) / 1000.0f;
                                mMIDI->mMixerSamplesPerTick = (int)(mMIDI->mMillisecondsPerTick / 1000.0f * mMIDI->waveformat[0].frequency);
                                mMIDI->mTimingScale = 1.0f;

                                if (mMIDI->mMixerSamplesPerTick < (unsigned int)MIDI_MINIMUMSAMPLEGRANULARITY)
                                {
                                    mMIDI->mTimingScale          = (float)MIDI_MINIMUMSAMPLEGRANULARITY / (float)mMIDI->mMixerSamplesPerTick;
                                    mMIDI->mMixerSamplesPerTick  = MIDI_MINIMUMSAMPLEGRANULARITY;
                                    mMIDI->mMillisecondsPerTick  = mMIDI->mTimingScale * mMIDI->mMillisecondsPerTick;
                                }

                                break;
                            }
                            case MIDI_METAEVENT_SMTPOFFSET:
                            {
                                result = readByte(&mMIDI->mSMTPOffsetHours);
                                if (result != FMOD_OK)
                                {
                                    break;
                                }
                                result = readByte(&mMIDI->mSMTPOffsetMinutes);
                                if (result != FMOD_OK)
                                {
                                    break;
                                }
                                result = readByte(&mMIDI->mSMTPOffsetSeconds);
                                if (result != FMOD_OK)
                                {
                                    break;
                                }
                                result = readByte(&mMIDI->mSMTPOffsetFrames);
                                if (result != FMOD_OK)
                                {
                                    break;
                                }
                                result = readByte(&mMIDI->mSMTPOffsetSubFrames);
                                if (result != FMOD_OK)
                                {
                                    break;
                                }
                                break;
                            }
                            case MIDI_METAEVENT_TIMESIGNATURE:
                            {
                                result = readByte(&mMIDI->mTimeSignatureNumerator);
                                if (result != FMOD_OK)
                                {
                                    break;
                                }
                                result = readByte(&mMIDI->mTimeSignatureDenominator);
                                if (result != FMOD_OK)
                                {
                                    break;
                                }
                                result = readByte(&mMIDI->mTimeSignatureMetronome);
                                if (result != FMOD_OK)
                                {
                                    break;
                                }
                                result = readByte(&mMIDI->mTimeSignatureNotated32nds);
                                if (result != FMOD_OK)
                                {
                                    break;
                                }
                                break;
                            }
                            case MIDI_METAEVENT_KEYSIGNATURE:
                            {
                                result = readByte(&mMIDI->mKeySignatureSF);
                                if (result != FMOD_OK)
                                {
                                    break;
                                }
                                result = readByte(&mMIDI->mKeySignatureMI);
                                if (result != FMOD_OK)
                                {
                                    break;
                                }
                                break;
                            }
                            case MIDI_METAEVENT_PROPRIETARY:
                            {
                                result = addTag("Proprietory Data", length, calledfromopen);
                                break;
                            }
                            default:
                            {
                                mOffset += length;
                                break;
                            }
                        };
                        break;
                    }
                    default:
                    {
        //                    mOffset += length;
                        event = event;
                        break;
                    }
                };
            }
            else
            {
                int midichannel;
                unsigned char runningdata = 0;
                bool reuse = false;

                if (event < 0x80)
                {
                    runningdata = event;
                    event = mEvent;
                    reuse = true;
                }

                midichannel = (event & 0x0F);
                
                mMIDI->mMIDIChannel[midichannel].mTrack = this;
                mMIDI->mMIDIChannel[midichannel].process(event, reuse, runningdata, calledfromopen);
            }
        
            mEvent = event;

            if (mFinished)
            {
                break;
            }
        }
    }

    return FMOD_OK;
}


/*
    ==========================================================================================

    CODEC MIDI CLASS 

    ==========================================================================================
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
// VERIFY: 5214ef3f auto
// OFFSET: 0x008FC3B0
FMOD_RESULT CodecMIDI::update(bool audible)
{
    int count;

    for (count = 0; count < mNumTracks; count++)
    {
        mTrack[count].process(false);
    }

    for (count = 0; count < 16; count++)
    {
        mMIDIChannel[count].update();
//MGB        FLOG((FMOD_DEBUG_LEVEL_LOG, __FILE__, __LINE__, "CodecMIDI::update", "Chan: %d -- Voices: %d\n", count, mMIDIChannel[count].mChannelHead.count()));
    }
//MGB    FLOG((FMOD_DEBUG_LEVEL_LOG, __FILE__, __LINE__, "CodecMIDI::update", "\n"));

    mTick += mTimingScale;
    
    mPCMOffset += mMixerSamplesPerTick;

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
// VERIFY: cde44d1f auto
// OFFSET: 0x008FB270
FMOD_RESULT CodecMIDI::play()
{
    int count;

    for (count = 0; count < mNumTracks; count++)
    {        
        mTrack[count].mOffset = 0;
        mTrack[count].mTick = 0;
        mTrack[count].mFinished = false;
        mTrack[count].mEvent = 0;
        mTrack[count].mReadDelta = 0;
    }

    mChannelFreeListHead.initNode();

    for (count = 0; count < mNumSubChannels; count++)
    {
        mMIDISubChannel[count].initNode();
        mMIDISubChannel[count].stop();
    }
    
    for (count = 0; count < 16; count++)
    {
        mMIDIChannel[count].mChannelHead.initNode();
        mMIDIChannel[count].mIndex                = count + 1;
        mMIDIChannel[count].mPan                  = 64;
        mMIDIChannel[count].mVolume               = 100;
        mMIDIChannel[count].mExpression           = 127;
        mMIDIChannel[count].mBank                 = 0;
        mMIDIChannel[count].mProgram              = 0;
        mMIDIChannel[count].mPitchBendSensitivity = 0x200;
        mMIDIChannel[count].mRPN                  = 0xFFFFFFFF;
        mMIDIChannel[count].mTrack                = 0;
    }

    mTick               = 0;
    mPCMOffset          = 0;
    mMixerSamplesLeft   = 0;

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
// VERIFY: a5b8f5db manual
// OFFSET: 0x008FC850
FMOD_RESULT CodecMIDI::openInternal(FMOD_MODE usermode, FMOD_CREATESOUNDEXINFO *userexinfo)
{
    FMOD_RESULT     result = FMOD_OK;
	int 			count;
    unsigned int    offset, lengthbytes;
	MTHD_CHUNK      midiheaderchunk;
    int             trackindex;

    init(FMOD_SOUND_TYPE_MIDI);

    FLOG((FMOD_DEBUG_LEVEL_LOG, __FILE__, __LINE__, "CodecMIDI::openInternal", "attempting to open as MIDI..\n"));

    /*
        Need to explicitly construct the ChannelGroupI here because CodecMIDI doesn't have a constructor
    */
    new (&mChannelGroup) ChannelGroupI();

	result = mFile->seek(0, SEEK_SET);
    if (result != FMOD_OK)
    {
        return result;
    }

	/*
        Get size of file in bytes
    */
	result = mFile->getSize(&lengthbytes);
    if (result != FMOD_OK)
    {
        return result;
    }

    offset = 0;

    result = mFile->read(&midiheaderchunk, 1, sizeof(MTHD_CHUNK), 0);
    if (result != FMOD_OK)
    {
        return result;
    }

#ifdef PLATFORM_ENDIAN_LITTLE
    midiheaderchunk.mChunk.mSize = FMOD_SWAPENDIAN_DWORD(midiheaderchunk.mChunk.mSize);
    midiheaderchunk.mFormat      = FMOD_SWAPENDIAN_WORD(midiheaderchunk.mFormat);
    midiheaderchunk.mDivision    = FMOD_SWAPENDIAN_WORD(midiheaderchunk.mDivision);
    midiheaderchunk.mNumTracks   = FMOD_SWAPENDIAN_WORD(midiheaderchunk.mNumTracks);
#endif

    if (FMOD_strncmp((const char *)midiheaderchunk.mChunk.mID, "MThd", 4))
    {
        FLOG((FMOD_DEBUG_LEVEL_LOG, __FILE__, __LINE__, "CodecMIDI::openInternal", "'HThd' ID check failed [%c%c%c%c]\n", midiheaderchunk.mChunk.mID[0], midiheaderchunk.mChunk.mID[1], midiheaderchunk.mChunk.mID[2], midiheaderchunk.mChunk.mID[3]));
        return FMOD_ERR_FORMAT;
	}

    mNumTracks           = midiheaderchunk.mNumTracks + 1;          /* Some midi files written out wrongly! */
    mMIDIFormat          = (MIDI_FORMAT)midiheaderchunk.mFormat;
    mDivision            = midiheaderchunk.mDivision;
    mTempo               = 60000 / 120 * 1000;
    mMillisecondsPerTick = ((float)mTempo / (float)mDivision) / 1000.0f;
    mTimingScale         = 1.0f;

    if (userexinfo && userexinfo->maxpolyphony)
    {
        mNumSubChannels = userexinfo->maxpolyphony;
    }
    else
    {
        mNumSubChannels = MIDI_DEFAULTSUBCHANNELS;
    }

    /*
        Load the DLS definitions and audio data so we can actually hear something.
    */
    {
        FMOD_CREATESOUNDEXINFO exinfo;
        char dlsname[FMOD_STRING_MAXPATHLEN];
        int list[1] = { -1 };

		FMOD_memset(dlsname, 0, FMOD_STRING_MAXPATHLEN);

        if (userexinfo && userexinfo->dlsname)
        {
            strncpy(dlsname, userexinfo->dlsname, FMOD_STRING_MAXPATHLEN);
        }
        #if defined(PLATFORM_WINDOWS)
        else
        {
			char *sysroot;
			
			sysroot = getenv("windir");
			if (sysroot)
			{
                struct _stat statbuf;

	            strncpy(dlsname, sysroot, FMOD_STRING_MAXPATHLEN);
		        strcat(dlsname, "/system32/drivers/gm.dls");

                if (_stat(dlsname, &statbuf))
                {
	                strncpy(dlsname, sysroot, FMOD_STRING_MAXPATHLEN);
    		        strcat(dlsname, "/system32/drivers/etc/gm.dls");

    		        if (_stat(dlsname, &statbuf))
                    {
                        return FMOD_ERR_PLUGIN_RESOURCE;
                    }
                }
			}
        }
        #endif
        #if defined(PLATFORM_MAC)
        else
        {
            struct stat statbuf;          

            strcat(dlsname, "/System/Library/Components/CoreAudio.component/Contents/Resources/gs_instruments.dls");
            
            if (stat(dlsname, &statbuf))
            {
                return FMOD_ERR_PLUGIN_RESOURCE; 
            }
        }
        #endif

        FMOD_memset(&exinfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));
        exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
        exinfo.inclusionlist = list;
        exinfo.inclusionlistnum = 1;

        result = mSystem->createSound(dlsname, FMOD_2D, &exinfo, &mDLSSound);
        if (result == FMOD_ERR_FILE_NOTFOUND)
        {
            return FMOD_ERR_PLUGIN_RESOURCE;
        }
        else if (result != FMOD_OK)
        {
            return result;
        }

        mDLS = (CodecDLS *)mDLSSound->mCodec;
    }

    /*
        Create and parse the MIDI Tracks.
    */
    mTrack = (CodecMIDITrack *)FMOD_Memory_Calloc(sizeof(CodecMIDITrack) * mNumTracks);
    if (!mTrack)
    {
        return FMOD_ERR_MEMORY;
    }
    
    mMIDISubChannel = (CodecMIDISubChannel *)FMOD_Memory_Calloc(sizeof(CodecMIDISubChannel) * mNumSubChannels);
    if (!mMIDISubChannel)
    {
        return FMOD_ERR_MEMORY;
    }

    for (count = 0; count < mNumSubChannels; count++)
    {
        mMIDISubChannel[count].initNode();
        mMIDISubChannel[count].mChannel.init();        
        mMIDISubChannel[count].mMIDI = this;
        mMIDISubChannel[count].mChannel.mChannelGroup = &mChannelGroup;
    }

    mWaveFormatMemory = (FMOD_CODEC_WAVEFORMAT *)FMOD_Memory_Calloc(sizeof(FMOD_CODEC_WAVEFORMAT));
    if (!mWaveFormatMemory)
    {
        return FMOD_ERR_MEMORY;
    }

    waveformat = mWaveFormatMemory;
    waveformat[0].lengthbytes = lengthbytes;

    trackindex = 0;

    offset += sizeof(MTHD_CHUNK);
    offset -= sizeof(MIDI_CHUNK);

    /*
        Decode chunks
    */
    do 
    {
        MIDI_CHUNK chunk;

		result = mFile->seek(offset + sizeof(MIDI_CHUNK), SEEK_SET);
        if (result != FMOD_OK)
        {
            return result;
        }

		result = mFile->read(&chunk, 1, sizeof(MIDI_CHUNK), 0);
        if (result != FMOD_OK)
        {
            return result;
        }

        #ifdef PLATFORM_ENDIAN_LITTLE
        chunk.mSize = FMOD_SWAPENDIAN_DWORD(chunk.mSize);
        #endif

        FLOG((FMOD_DEBUG_LEVEL_LOG, __FILE__, __LINE__, "CodecMIDI::openInternal","chunk: id %c%c%c%c size %d\n", chunk.mID[0],chunk.mID[1],chunk.mID[2],chunk.mID[3], chunk.mSize));

		/*
            DATA CHUNK
        */
		if (!FMOD_strncmp((const char *)chunk.mID, "MTrk", 4))
		{
            mTrack[trackindex].mData = (unsigned char *)FMOD_Memory_Calloc(chunk.mSize);
            if (!mTrack[trackindex].mData)
            {
                return FMOD_ERR_MEMORY;
            }

		    result = mFile->read(mTrack[trackindex].mData, 1, chunk.mSize, 0);
            if (result != FMOD_OK && result != FMOD_ERR_FILE_EOF)       /* allow truncated files :S */
            {
                return result;
            }

            mTrack[trackindex].mIndex       = trackindex;
            mTrack[trackindex].mLength      = chunk.mSize;
            mTrack[trackindex].mOffset      = 0;
            mTrack[trackindex].mMIDI        = this;
            trackindex++;    
		}
		else
		{
			mFile->seek(chunk.mSize, SEEK_CUR);
            if (result != FMOD_OK)
            {
                return result;
            }
		}

		offset += (chunk.mSize+sizeof(MIDI_CHUNK));

		if (chunk.mSize < 0)
        {
			break;
        }

		FLOG((FMOD_DEBUG_LEVEL_LOG, __FILE__, __LINE__, "CodecMIDI::openInternal", "offset = %d / %d\n", offset, waveformat[0].lengthbytes - sizeof(MIDI_CHUNK)));

	} while (offset < (waveformat[0].lengthbytes - sizeof(MIDI_CHUNK) - sizeof(MIDI_CHUNK)) && offset > 0);

    /*
        Set up general codec parameters.
    */
    if (userexinfo && userexinfo->format != FMOD_SOUND_FORMAT_NONE)
    {
        waveformat[0].format = userexinfo->format;
    }
    #ifndef PLATFORM_PS3
    else if (usermode & FMOD_SOFTWARE)
    {
        waveformat[0].format = FMOD_SOUND_FORMAT_PCMFLOAT;
    }
    #endif
    else
    {
        waveformat[0].format = FMOD_SOUND_FORMAT_PCM16;
    }

	waveformat[0].channels = 2;
    result = mSystem->getSoftwareFormat(&waveformat[0].frequency, 0, 0, 0, 0, 0);
    if (result != FMOD_OK)
    {
        return result;
    }

    SoundI::getBytesFromSamples(1, (unsigned int *)&waveformat[0].blockalign, waveformat[0].channels, waveformat[0].format);

    /*
        Create a default set of values for mixersamples per tick and ms per tick.
    */
    mMixerSamplesPerTick = (int)(mMillisecondsPerTick / 1000.0f * waveformat[0].frequency);
    if (mMixerSamplesPerTick < (unsigned int)MIDI_MINIMUMSAMPLEGRANULARITY)
    {
        mTimingScale         = (float)MIDI_MINIMUMSAMPLEGRANULARITY / (float)mMixerSamplesPerTick;
        mMixerSamplesPerTick = MIDI_MINIMUMSAMPLEGRANULARITY;
        mMillisecondsPerTick = mTimingScale * mMillisecondsPerTick;
    }

    /*
        Scan the midi to get the length and also to find what instruments are used.
    */   
    play();

    mSampleInclusionList = (bool *)FMOD_Memory_Calloc(sizeof(bool) * mDLSSound->mNumSubSounds);
    if (!mSampleInclusionList)
    {
        return FMOD_ERR_MEMORY;
    }

    {      
        const float ticksPerStep    = 10.0f;
        int         finishedCount   = 0;
        float       msPerTick       = 0;
        float       msPlayed        = 0;

        waveformat[0].lengthpcm = 0;
        
        // Quickly process the file untill all tracks report finished to determine the length
        while (finishedCount != mNumTracks)
        {
            finishedCount = 0;

            for (count = 0; count < mNumTracks; count++)
            {
                // Process each track and record finished (or invalid data) tracks
                mTrack[count].process(true);
                if (mTrack[count].mFinished || !mTrack[count].mData)
                {
                    finishedCount++;
                }
            }

            // Each slice process updates tempo, so calculate the time this slice would play for
            msPerTick = ((float)mTempo / (float)mDivision) / 1000.0f;
            msPlayed += msPerTick * ticksPerStep;
            mTick += ticksPerStep;
        }

        // Convert the time played into playback samples (time adjusted)
        waveformat[0].lengthpcm = (int)((msPlayed / 1000.0f) * waveformat[0].frequency);
    }

    // Determine the maximum number of channels played in the MIDI file
    {
        int totalNumChannels = 0;

        for (int channelIndex = 0; channelIndex < 16; channelIndex++)
        {
            // If the channel has been assigned a track, then this channel has played at least once
            if (mMIDIChannel[channelIndex].mTrack)
            {
                totalNumChannels++;
            }
        }

        result = metaData(FMOD_TAGTYPE_FMOD, "Number of channels", &totalNumChannels, sizeof(totalNumChannels), FMOD_TAGDATATYPE_INT, false);
        if (result != FMOD_OK)
        {
            return result;
        }
    }

    /*
        Now that the midi data has been scanned load the appropriate sounds.
    */
    for (count=0; count < mDLSSound->mNumSubSounds; count++)
    {
        if (mSampleInclusionList[count])
        {
            result = mDLSSound->loadSubSound(count, FMOD_SOFTWARE | FMOD_2D);
            if (result != FMOD_OK)
            {
                return result;
            }
        }
    }

    FMOD_Memory_Free(mSampleInclusionList);
    mSampleInclusionList = 0;

    for (count = 0; count < mDLS->mNumInstruments; count++)
    {
        FLOG((FMOD_DEBUG_LEVEL_LOG, __FILE__, __LINE__, "CodecMIDI::openInternal", "%3d: %-20s: Bank %08x Program %4d\n", count, mDLS->mInstrument[count].mName, mDLS->mInstrument[count].mHeader.Locale.ulBank, mDLS->mInstrument[count].mHeader.Locale.ulInstrument));
    }

    /*
        Create a head unit that software channels connect to.
    */
    {
        FMOD_DSP_DESCRIPTION_EX descriptionex;
        
        FMOD_memset(&descriptionex, 0, sizeof(FMOD_DSP_DESCRIPTION_EX));
        FMOD_strcpy(descriptionex.name, "FMOD MIDI Target Unit");
        descriptionex.version       = 0x00010100;
        descriptionex.channels      = waveformat[0].channels;
        descriptionex.mFormat       = waveformat[0].format;
        descriptionex.mCategory     = FMOD_DSP_CATEGORY_SOUNDCARD;

        result = mSystem->createDSP(&descriptionex, &mDSPHead);
        if (result != FMOD_OK)
        {
            return result;
        }

        mDSPHead->mDefaultFrequency = (float)waveformat[0].frequency;
    }
    
    mChannelGroup.mDSPHead = mDSPHead;
    mChannelGroup.mVolume  = 1.0f;

    /*
        Create a pool of real channels.
    */
    {
        mChannelPool = FMOD_Object_Calloc(ChannelPool);
        if (!mChannelPool)
        {
            return FMOD_ERR_MEMORY;
        }

        result = mChannelPool->init(mSystem, 0, mNumSubChannels);
        if (result != FMOD_OK)
        {
            return result;
        }

        mChannelSoftware = (ChannelSoftware *)FMOD_Memory_Calloc(sizeof(ChannelSoftware) * mNumSubChannels);
        if (!mChannelSoftware)
        {
            return FMOD_ERR_MEMORY;
        }

        for (count = 0; count < mNumSubChannels; count++)
        {
            new (&mChannelSoftware[count]) ChannelSoftware;
            mChannelPool->setChannel(count, &mChannelSoftware[count], mDSPHead);
            mChannelSoftware[count].allowReverb(false);
        }
    }

    play();

    /*
        Fill out base class members, also pointing to or allocating storage for them.
    */
    numsubsounds    = 0;

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
// VERIFY: 0fbf7f3f auto
// OFFSET: 0x008FB410
FMOD_RESULT CodecMIDI::closeInternal()
{
    int count;

    if (mTrack)
    {
        for (count = 0; count < mNumTracks; count++)
        {
            if (mTrack[count].mData)
            {
                FMOD_Memory_Free(mTrack[count].mData);
            }
        }
        FMOD_Memory_Free(mTrack);
    }

    if (mMIDISubChannel)
    {
        FMOD_Memory_Free(mMIDISubChannel);
        mMIDISubChannel = 0;
    }

    if (mChannelPool)
    {
        mChannelPool->release();
        mChannelPool = 0;
    }

    if (mChannelSoftware)
    {
        FMOD_Memory_Free(mChannelSoftware);
        mChannelSoftware = 0;
    }

    if (mDSPHead)
    {
        mDSPHead->release();
        mDSPHead = 0;
    }

    if (mDLSSound)
    {
        mDLSSound->release();
        mDLSSound = 0;
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
// VERIFY: b2af4bc8 auto
// OFFSET: 0x008FC430
FMOD_RESULT CodecMIDI::readInternal(void *buffer, unsigned int sizebytes, unsigned int *bytesread)
{
    FMOD_RESULT              result = FMOD_OK;
    unsigned int             numsamples;
    int                      numchannels;
    FMOD_OS_CRITICALSECTION *crit = mSystem->mDSPCrit;

    numchannels = waveformat[0].channels;
    FMOD_memset(buffer, 0, sizebytes);

    SoundI::getSamplesFromBytes(sizebytes, &numsamples, numchannels, waveformat[0].format);

	{
		unsigned int    mixedsofar = 0;
		unsigned int    mixedleft = mMixerSamplesLeft;
		unsigned int    samplestomix;
        char           *destptr = (char *)buffer;

		while (mixedsofar < numsamples)
		{
            unsigned int read, bytes;
            void        *outbuffer = destptr;

			if (!mixedleft)
			{
                result = update(true);
                if (result != FMOD_OK)
                {
                    return result;
                }

				samplestomix = mMixerSamplesPerTick;
				mixedleft = samplestomix;
			}
			else
            {
				samplestomix = mixedleft;
            }

			if (mixedsofar + samplestomix > numsamples)
            {
				samplestomix = numsamples - mixedsofar;
            }

            read = samplestomix;

            mSystem->flushDSPConnectionRequests();

            FMOD_OS_CriticalSection_Enter(crit);
            if (buffer)
			{
                result = mDSPHead->execute(outbuffer, &outbuffer, &read, numchannels, &numchannels, FMOD_SPEAKERMODE_STEREO);
                if (result != FMOD_OK)
                {
                    FMOD_OS_CriticalSection_Leave(crit);
                    return result;
                }

                mDSPHead->resetVisited();
            }
            FMOD_OS_CriticalSection_Leave(crit);

            SoundI::getBytesFromSamples(read, &bytes, numchannels, waveformat[0].format);

            if (destptr != outbuffer && buffer)
            {
                FMOD_memcpy(destptr, outbuffer, bytes);
            }

			mixedleft  -= read;
			destptr    += bytes;
			mixedsofar += read;
		}

		mMixerSamplesLeft = mixedleft;
	}

    if (bytesread)
    {
        *bytesread = sizebytes;
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
FMOD_RESULT CodecMIDI::setPositionInternal(int subsound, unsigned int position, FMOD_TIMEUNIT postype)
{
    FMOD_RESULT result = FMOD_OK;

    if (position == mPCMOffset)
    {
        return FMOD_OK;
    }
    
    /*
        Want to seek backwards, start from the start.
    */
    if (position < mPCMOffset)
    {
        play();
    }

    while (mPCMOffset < position)
    {
        update(true);
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
// VERIFY: 2d07abf3 manual
// OFFSET: 0x008FD300
FMOD_RESULT F_CALLBACK CodecMIDI::openCallback(FMOD_CODEC_STATE *codec, FMOD_MODE usermode, FMOD_CREATESOUNDEXINFO *userexinfo)
{
    CodecMIDI *cmidi = (CodecMIDI *)codec;

    return cmidi->openInternal(usermode, userexinfo);
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
// VERIFY: 67e5094a manual
// OFFSET: 0x008FB530
FMOD_RESULT F_CALLBACK CodecMIDI::closeCallback(FMOD_CODEC_STATE *codec)
{
    CodecMIDI *cmidi = (CodecMIDI *)codec;

    return cmidi->closeInternal();
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
// VERIFY: dcca0603 auto
// OFFSET: 0x008FC7D0
FMOD_RESULT F_CALLBACK CodecMIDI::readCallback(FMOD_CODEC_STATE *codec, void *buffer, unsigned int sizebytes, unsigned int *bytesread)
{
    CodecMIDI *cmidi = (CodecMIDI *)codec;

    return cmidi->readInternal(buffer, sizebytes, bytesread);
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
// VERIFY: bf628a84 manual
// OFFSET: 0x008FC800
FMOD_RESULT F_CALLBACK CodecMIDI::setPositionCallback(FMOD_CODEC_STATE *codec, int subsound, unsigned int position, FMOD_TIMEUNIT postype)
{
    CodecMIDI *cmidi = (CodecMIDI *)codec;

    return cmidi->setPositionInternal(subsound, position, postype);
}

}

#endif

