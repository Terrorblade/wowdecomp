#include "fmod_settings.h"

#ifdef FMOD_SUPPORT_XM

#include "fmod.h"

#include "fmod_channel_software.h"
#include "fmod_codec_xm.h"
#include "fmod_debug.h"
#include "fmod_dspi.h"
#include "fmod_file.h"
#include "fmod_localcriticalsection.h"
#include "fmod_outputi.h"
#include "fmod_memory.h"
#include "fmod_systemi.h"
#include "fmod_string.h"

#include <stdio.h>
#include <stdlib.h>

namespace FMOD
{


FMOD_CODEC_DESCRIPTION_EX xmcodec;


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
        return CodecXM::getDescriptionEx();
    }

#ifdef __cplusplus
}
#endif

#endif  /* PLUGIN_EXPORTS */


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
// VERIFY: 21ba71d4 manual
// OFFSET: 0x009075F0
FMOD_CODEC_DESCRIPTION_EX *CodecXM::getDescriptionEx()
{
    FMOD_memset(&xmcodec, 0, sizeof(FMOD_CODEC_DESCRIPTION_EX));

    xmcodec.name            = "FMOD XM Codec";
    xmcodec.version         = 0x00010100;
    xmcodec.timeunits       = (FMOD_TIMEUNIT)(FMOD_TIMEUNIT_PCM | FMOD_TIMEUNIT_MODORDER | FMOD_TIMEUNIT_MODROW | FMOD_TIMEUNIT_MODPATTERN);
    xmcodec.defaultasstream = 1;
    xmcodec.open            = &CodecXM::openCallback;
    xmcodec.close           = &CodecXM::closeCallback;
    xmcodec.read            = &CodecXM::readCallback;
    xmcodec.getlength       = &MusicSong::getLengthCallback;
    xmcodec.setposition     = &CodecXM::setPositionCallback;
    xmcodec.getposition     = &MusicSong::getPositionCallback;

    xmcodec.mType = FMOD_SOUND_TYPE_XM;
    xmcodec.mSize = sizeof(CodecXM);

    return &xmcodec;
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
// VERIFY: d23a93c6 auto
// OFFSET: 0x00905E30
FMOD_RESULT CodecXM::calculateLength()
{
    waveformat[0].lengthpcm = 0;

    play();

    while (!mFinished)
    {
        CodecXM::update(false);

        waveformat[0].lengthpcm += mMixerSamplesPerTick;
    }

    stop();

    return FMOD_OK;
}



#ifdef FMOD_NO_FPU
    #define FMUSIC_XMLINEARPERIOD2HZ(_per) (FMUSIC_LinearTable[(_per) % 768] >> ((_per) / 768))
#else
    #include <math.h>
    /*
        Frequency = 8363*2^((6*12*16*4 - Period) / (12*16*4));
    */
    #define FMUSIC_XMLINEARPERIOD2HZ(_per) ( (int)(8363.0f * FMOD_POW(2.0f, ((6.0f*12.0f*16.0f*4.0f - _per) / (float)(12*16*4)))) )
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
// VERIFY: 6e639ca4 auto
// OFFSET: 0x00903AA0
FMOD_RESULT CodecXM::spawnNewChannel(MusicChannel *cptr, MusicVirtualChannel *oldvcptr, MusicSample *sptr, MusicVirtualChannel **newvcptr)
{
    if (oldvcptr == &gDummyVirtualChannel) /* bugfix - fmod 3.11 - instrument starting with no channel was just ignoring settings. */
    {
   	    spawnNewVirtualChannel(cptr, sptr, newvcptr);
    }
    else
    {
        *newvcptr = oldvcptr;
    }

	if (!newvcptr)
	{
        *newvcptr             = oldvcptr;
/*
		newvcptr             = &gDummyVirtualChannel;
        newvcptr->channelid  = -1;
		newvcptr->cptr       = &FMUSIC_DummyChannel;
		newvcptr->sptr       = &gDummySample;
		newvcptr->cptr->sptr = &gDummySample;
*/
	}

    return FMOD_OK;
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
// VERIFY: e4429e47 manual
// OFFSET: 0x00903AE0
FMOD_RESULT CodecXM::updateFlags(MusicVirtualChannel *vcptr, MusicSample *sptr)
{   
    if (!(vcptr->mFrequency + vcptr->mFrequencyDelta)) 
    {
	    vcptr->mNoteControl &= ~FMUSIC_FREQ;	/* divide by 0 check */
    }

    if (vcptr->mNoteControl & FMUSIC_TRIGGER)		
    {
        FMOD_RESULT result;
        result = playSound(sptr, vcptr, false);
    }

    if (vcptr->mNoteControl & FMUSIC_VOLUME)	
    {
        #ifdef FMOD_NO_FPU
        unsigned int finalvol;

	    finalvol = (unsigned int)vcptr->mEnvVolume.mValue;                  /*  6 bits (   64) */
	    finalvol *= (unsigned int)(vcptr->mVolume + vcptr->mVolumeDelta);	/*  6 bits (   64) */
	    finalvol *= (unsigned int)vcptr->mFadeOutVolume;		    		/* 16 bits (65536) */
        finalvol >>= 11;
	    finalvol *= (unsigned int)mGlobalVolume;	            			/*  6 bits (   64) */
        finalvol >>= 15;
																            /* ==============  */
																            /* 34 bits         */
        finalvol >>= 1; /* gain */

        #else

	    float finalvol;
	    
	    finalvol = (float)vcptr->mEnvVolume.mValue;			/*  6 bits (   64) */
	    finalvol *= (vcptr->mVolume + vcptr->mVolumeDelta);	/*  6 bits (   64) */
	    finalvol *= vcptr->mFadeOutVolume;					/* 16 bits (65536) */
	    finalvol *= mGlobalVolume;	        				/*  6 bits (   64) */
											    		    /* ==============  */
												    	    /* 34 bits         */

	    /*
            Any half arsed compiler will convert this into 1 constant at compile time.
        */
   	    finalvol *= (1.0f / (64.0f * 64.0f * 65536.0f * 64.0f) * 0.5f);
        #endif
	    vcptr->mChannel.setVolume(finalvol);
    }
    if (vcptr->mNoteControl & FMUSIC_PAN)
    {
        int p;
	    float finalpan;
        
        p = vcptr->mPan - 128;
        if (p < 0)
        {
            p = -p;
        }

        finalpan = (float)(vcptr->mPan + ( (vcptr->mEnvPan.mValue-32) * ( (128-p)/32 ) ));
        finalpan = ((float)vcptr->mPan - 128.0f) * mPanSeparation;
	    
	    vcptr->mChannel.setPan(finalpan / 127.0f);
    }
    if (vcptr->mNoteControl & FMUSIC_FREQ)		
    {
        int finalfreq = vcptr->mFrequency + vcptr->mFrequencyDelta;

        if (finalfreq < 1)
        {
            finalfreq = 1;
        }

	    if (mMusicFlags & FMUSIC_XMFLAGS_LINEARFREQUENCY)
        {
		    finalfreq = FMUSIC_XMLINEARPERIOD2HZ(finalfreq);
        }
	    else
        {
		    finalfreq = period2HZ(finalfreq);
        }

        vcptr->mChannel.setFrequency((float)finalfreq);
    }
    if (vcptr->mNoteControl & FMUSIC_STOP)		
    {
	    vcptr->mChannel.stopEx(false, false, true, true, false, false, false);
	    vcptr->mSampleOffset = 0;	/* if this channel gets stolen it will be safe */
    }

    return FMOD_OK;
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
// VERIFY: e8a9fcc5 manual
// OFFSET: 0x00903C30
FMOD_RESULT MusicChannelXM::portamento()
{
    MusicVirtualChannel *vcptr = (MusicVirtualChannel *)mVirtualChannelHead.getNext();

	/*
        Slide pitch down if it needs too.
    */
	if (vcptr->mFrequency < mPortaTarget) 
	{
		vcptr->mFrequency += (int )mPortaSpeed << 2;
		if (vcptr->mFrequency > mPortaTarget) 
        {
			vcptr->mFrequency = mPortaTarget;
        }
	}

	/*
        Slide pitch up if it needs too.
    */
	else if (vcptr->mFrequency > mPortaTarget) 
	{
		vcptr->mFrequency -= (int )mPortaSpeed << 2;
		if (vcptr->mFrequency < mPortaTarget) 
        {
			vcptr->mFrequency = mPortaTarget;
        }
	}

	 /*
	 if (glissando[track]) 
	 {
	 }
     */

	 vcptr->mNoteControl |= FMUSIC_FREQ;

     return FMOD_OK;
}


/*
[
	[DESCRIPTION]
	to carry out a vibrato at a certain depth and speed            

	[PARAMETERS]
	track - the track number to do the vibrato too           
 
	[RETURN_VALUE]

	[REMARKS]
	AND'ing temp with 31 removes the sign bit giving the abs value

	[SEE_ALSO]
]
*/
// VERIFY: b3a46df2 auto
// OFFSET: 0x00903CA0
FMOD_RESULT MusicChannelXM::vibrato() 
{
	int delta;
	unsigned char temp;
    MusicVirtualChannel *vcptr = (MusicVirtualChannel *)mVirtualChannelHead.getNext();

	temp = (unsigned char)(mVibPos & 31);

	switch (mWaveControl & 3)
	{
		case 0: delta = gSineTable[temp];			/* sine */
				break;
		case 1: temp <<= 3;								/* ramp down */
				if (mVibPos < 0) 
                {
					temp=255-temp;
                }
				delta=temp;
				break;
		case 2: delta = 255;					/* square */
				break;
		case 3: delta = FMOD_RAND()&255;		/* random */
				break;
		default : 
				delta = 0;
				break;
	};

	delta *= mVibDepth;
	delta >>=7;
	delta <<=2;   /* we use 4*periods so make vibrato 4 times bigger */

	if (mVibPos >= 0) 
    {
		vcptr->mFrequencyDelta = -delta;
    }
	else
    {
		vcptr->mFrequencyDelta = delta;
    }
	
	vcptr->mNoteControl |= FMUSIC_FREQ;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]
	To carry out a tremolo at a certain depth and speed

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 35f930c4 auto
// OFFSET: 0x00903D40
FMOD_RESULT MusicChannelXM::tremolo() 
{
	unsigned char temp;
    MusicVirtualChannel *vcptr = (MusicVirtualChannel *)mVirtualChannelHead.getNext();

	temp = (unsigned char)(mTremoloPosition & 31);

	switch ((mWaveControl>>4) & 3)
	{
		case 0: vcptr->mVolumeDelta = gSineTable[temp];		/* sine */
				break;
		case 1: temp <<= 3;										/* ramp down */
				if (mTremoloPosition < 0) 
                {
					temp=255-temp;
                }
				vcptr->mVolumeDelta=temp;
				break;
		case 2: vcptr->mVolumeDelta = 255;							/* square */
				break;
		case 3: vcptr->mVolumeDelta = gSineTable[temp];		/* random (just use sine for now) */
				break;
	};

	vcptr->mVolumeDelta *= mTremoloDepth;
	vcptr->mVolumeDelta >>= 6;

	if (mTremoloPosition >= 0) 
	{
		if (vcptr->mVolume + vcptr->mVolumeDelta > 64) 
        {
			vcptr->mVolumeDelta = 64-vcptr->mVolume;
        }
	}
	else 
	{
		if ((short)(vcptr->mVolume-vcptr->mVolumeDelta) < 0) 
        {
			vcptr->mVolumeDelta = vcptr->mVolume;
        }
		vcptr->mVolumeDelta = -vcptr->mVolumeDelta;
	}

	mTremoloPosition += mTremoloSpeed;
	if (mTremoloPosition > 31) 
    {
		mTremoloPosition -=64;
    }

	vcptr->mNoteControl |= FMUSIC_VOLUME;

    return FMOD_OK;
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
// VERIFY: a4e41fe6 manual
// OFFSET: 0x00903E50
FMOD_RESULT CodecXM::processEnvelope(MusicEnvelopeState *env, MusicVirtualChannel *vcptr, int Inumpoints, unsigned short *points, int type, int loopstart, int loopend, unsigned char ISustain, unsigned char control)
{
	if (env->mPosition < Inumpoints) 
	{
		if (!env->mTick || env->mTick == points[env->mPosition<<1])	/* if we are at the correct tick for the position */
		{
			int currpos, nextpos;
			int currtick, nexttick;
			int currval, nextval, tickdiff;

            restartenv:

			/*
                Handle loop
            */
			if ((type & FMUSIC_ENVELOPE_LOOP) && env->mPosition == loopend) 
			{
				env->mPosition  = loopstart;
				env->mTick = points[env->mPosition <<1];
			}

			currpos	= env->mPosition;
			nextpos	= env->mPosition + 1;

			currtick = points[currpos<<1];				/* get tick at this point */
			nexttick = points[nextpos<<1];				/* get tick at next point */

			currval = points[(currpos<<1)+1] << 16;	    /* get val at this point << 16 */
			nextval	= points[(nextpos<<1)+1] << 16;	    /* get val at next point << 16 */

			/*
                If it is at the last position, abort the envelope and continue last val
            */
			if (env->mPosition == Inumpoints - 1) 
			{
				env->mValue = points[(currpos<<1)+1];
				env->mStopped = true;
				vcptr->mNoteControl |= control;
				return FMOD_OK;
			}

			/*
                Sustain
            */
			if ((type & FMUSIC_ENVELOPE_SUSTAIN) && currpos == ISustain && !vcptr->mKeyOff)
			{
				env->mValue = points[(currpos<<1)+1];
				vcptr->mNoteControl |= control;
				return FMOD_OK;
			}

			/*
                Interpolate 2 points to find delta step
            */
			tickdiff = nexttick - currtick;
			if (tickdiff) 
            {
				env->mDelta = (nextval-currval) / tickdiff;
            }
			else
            {
				env->mDelta = 0;
            }

			env->mFraction = currval;

			env->mPosition++;

            if (env->mTick == points[env->mPosition<<1] && env->mPosition < Inumpoints)
            {
                goto restartenv;
            }
		}
		else
        {
			env->mFraction += env->mDelta;				/* interpolate */
        }
	}

	env->mValue = env->mFraction >> 16;
	env->mTick++;

	vcptr->mNoteControl |= control;

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]
	to carry out a vibrato at a certain depth and speed            

	[PARAMETERS]
	track - the track number to do the vibrato too           
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 9e263abd manual
// OFFSET: 0x00903F70
FMOD_RESULT MusicChannelXM::instrumentVibrato(MusicInstrument *iptr) 
{
    MusicVirtualChannel *vcptr = (MusicVirtualChannel *)mVirtualChannelHead.getNext();
	int delta;

	switch (iptr->mVibratoType)
	{
		case 0: delta = (int)gFineSineTable[vcptr->mIVibPos];	/* sine */
				break;
		case 1: if (vcptr->mIVibPos < 128) 
                {
                    delta=64;                 /* square */
                }
				else 
                {
                    delta = -64;
                }
				break;
		case 2: delta = (128-((vcptr->mIVibPos+128)%256))>>1;
				break;
		case 3: delta = (128-(((256-vcptr->mIVibPos)+128)%256))>>1;
				break;
		default : 
				delta = 0;
				break;
	};

	delta *= iptr->mVibratoDepth;
	if (iptr->mVibratoSweep) 
    {
		delta = delta * vcptr->mIVibSweepPos / iptr->mVibratoSweep;
    }
	delta >>=6;

	vcptr->mFrequencyDelta += delta;
	
	vcptr->mIVibSweepPos++;
	if (vcptr->mIVibSweepPos > iptr->mVibratoSweep) 
    {
		vcptr->mIVibSweepPos = iptr->mVibratoSweep;
    }

	vcptr->mIVibPos += iptr->mVibratoRate;
	if (vcptr->mIVibPos > 255) 
    {
		vcptr->mIVibPos -= 256;
    }

	vcptr->mNoteControl |= FMUSIC_FREQ;
	
	return FMOD_OK;
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
// VERIFY: a408f2e6 auto
// OFFSET: 0x009040B0
FMOD_RESULT MusicChannelXM::processVolumeByte(unsigned char volume)
{
    MusicVirtualChannel *vcptr = (MusicVirtualChannel *)mVirtualChannelHead.getNext();

	if (volume >= 0x10 && volume <= 0x50) 
	{
		vcptr->mVolume = volume-0x10;
		vcptr->mNoteControl |= FMUSIC_VOLUME;
	}

	else
	{
		switch (volume >> 4) 
		{
			case 0x6 : 
			{
				vcptr->mVolume -= (volume & 0xF);
				if (vcptr->mVolume < 0) 
                {
                    vcptr->mVolume = 0;
                }
				vcptr->mNoteControl |= FMUSIC_VOLUME;
				break;
			}
			case 0x7 : 
			{
				vcptr->mVolume += (volume & 0xF);
				if (vcptr->mVolume > 0x40) 
                {
                    vcptr->mVolume = 0x40;
                }
				vcptr->mNoteControl |= FMUSIC_VOLUME;
				break;
			}
			case 0x8 :
			{
				vcptr->mVolume -= (volume & 0xF);
				if (vcptr->mVolume < 0) 
                {
                    vcptr->mVolume = 0;
                }
				vcptr->mNoteControl |= FMUSIC_VOLUME;
				break;
			}
			case 0x9 :
			{
				vcptr->mVolume += (volume & 0xF);
				if (vcptr->mVolume > 0x40) 
                {
                    vcptr->mVolume = 0x40;
                }
				vcptr->mNoteControl |= FMUSIC_VOLUME;
				break;
			}
			case 0xa :
			{
				mVibSpeed = (volume & 0xF);
				break;
			}
			case 0xb :
			{
				mVibDepth = (volume & 0xF);
				break;
			}
			case 0xc :
			{
				vcptr->mPan = (volume & 0xF) << 4;
				vcptr->mNoteControl |= FMUSIC_PAN;
				break;
			}
			case 0xd :
			{
				vcptr->mPan -= (volume & 0xF);
				vcptr->mNoteControl |= FMUSIC_PAN;
				break;
			}
			case 0xe :
			{
				vcptr->mPan += (volume & 0xF);
				vcptr->mNoteControl |= FMUSIC_PAN;
				break;
			}
			case 0xf :
			{
				if (volume & 0xF) 
                {
					mPortaSpeed = (volume & 0xF) << 4;
                }
				mPortaTarget = mPeriod;
				vcptr->mNoteControl &= ~FMUSIC_TRIGGER;
				break;
			}
		};
	}

    return FMOD_OK;
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
// VERIFY: 114dabc4 manual
// OFFSET: 0x00904220
FMOD_RESULT CodecXM::getAmigaPeriod(int note, int finetune, int *period)
{
	*period = gPeriodTable[note];

	/*
        Interpolate for finer tuning
    */
	if (finetune < 0 && note)
	{
		int diff = *period - gPeriodTable[note-1];
		diff *= -finetune;
		diff /= 128;
		*period -= diff;
	}
	else
	{
		int diff = gPeriodTable[note+1] - *period;
		diff *= finetune;
		diff /= 128;
		*period += diff;
	}

	return FMOD_OK;
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
// VERIFY: 1a0284b9 auto
// OFFSET: 0x00904270
FMOD_RESULT CodecXM::processNote(MusicNote *current, MusicChannelXM *cptr, MusicVirtualChannel *vcptr, MusicInstrument *iptr, MusicSample *sptr)
{
	/*
        PROCESS INSTRUMENT NUMBER
    */
	if (current->mNumber)
	{
		vcptr->mVolume	= sptr->mDefaultVolume;
		vcptr->mPan		= sptr->mDefaultPan;

		vcptr->mEnvVolume.mValue    = 64;
		vcptr->mEnvVolume.mPosition = 0;
		vcptr->mEnvVolume.mTick     = 0;
		vcptr->mEnvVolume.mDelta    = 0;

		vcptr->mEnvPan.mValue       = 32;
		vcptr->mEnvPan.mPosition    = 0;
		vcptr->mEnvPan.mTick        = 0;
		vcptr->mEnvPan.mDelta       = 0;

		vcptr->mFadeOutVolume       = 65536;
		vcptr->mEnvVolume.mStopped  = false;
		vcptr->mEnvPan.mStopped     = false;
		vcptr->mKeyOff		        = false;

		vcptr->mIVibSweepPos = 0;
		vcptr->mIVibPos = 0;

		/*
            Retrigger tremolo and vibrato waveforms
        */
		if ((cptr->mWaveControl & 0xF) < 4) 
        {
			cptr->mVibPos=0;
        }
		if ((cptr->mWaveControl >> 4) < 4) 
        {
			cptr->mTremoloPosition = 0;
        }
		cptr->mTremorPosition = 0;                    /* retrigger tremor count */

		vcptr->mNoteControl |= FMUSIC_VOLUME;
		vcptr->mNoteControl |= FMUSIC_PAN;
	}

	/*
        PROCESS VOLUME BYTE
    */
	if (current->mVolume) 
    {
		cptr->processVolumeByte(current->mVolume);
    }

	/*
        PROCESS KEY OFF
    */
	if (current->mNote == FMUSIC_KEYOFF || current->mEffect == FMUSIC_XM_KEYOFF) 
    {
		vcptr->mKeyOff = true;
    }

	/*
        PROCESS ENVELOPES
    */
	if (iptr->mVolumeType & FMUSIC_ENVELOPE_ON)
	{
		if (!vcptr->mEnvVolume.mStopped) 
        {
			processEnvelope(&vcptr->mEnvVolume, vcptr, iptr->mVolumeNumPoints, iptr->mVolumePoints, iptr->mVolumeType, iptr->mVolumeLoopStart, iptr->mVolumeLoopEnd, iptr->mVolumeSustain, FMUSIC_VOLUME);
        }
	}
	else if (vcptr->mKeyOff) 
    {
		vcptr->mEnvVolume.mValue = 0;
    }

	if (iptr->mPanType & FMUSIC_ENVELOPE_ON && !vcptr->mEnvPan.mStopped) 
    {
        processEnvelope(&vcptr->mEnvPan, vcptr, iptr->mPanNumPoints, iptr->mPanPoints, iptr->mPanType, iptr->mPanLoopStart, iptr->mPanLoopEnd, iptr->mPanSustain, FMUSIC_PAN);
    }

	/*
        PROCESS VOLUME FADEOUT
    */
	if (vcptr->mKeyOff) 
	{
		vcptr->mFadeOutVolume -= iptr->mVolumeFade;
		if (vcptr->mFadeOutVolume < 0) 
        {
			vcptr->mFadeOutVolume = 0;
        }
		vcptr->mNoteControl |= FMUSIC_VOLUME;
	}

    return FMOD_OK;
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
// VERIFY: fbdd7d58 manual
// OFFSET: 0x00904440
FMOD_RESULT CodecXM::updateNote(bool audible)
{
	MusicNote  *current;
	bool        breakflag = false;
	bool        jumpflag  = false;
	int         count;

	/* 
        Point our note pointer to the correct pattern buffer, and to the
        correct offset in this buffer indicated by row and number of channels
    */
	current = mPattern[mOrderList[mOrder]].mData + (mRow * mNumChannels);
	if (!current) 
    {
		return FMOD_OK;
    }

    if (mVisited)
    {
        if (mVisited[(mOrder * FMUSIC_MAXROWS) + mRow])
        {
            mFinished = true;
            return FMOD_OK;
        }
        mVisited[(mOrder * FMUSIC_MAXROWS) + mRow] = true;
    }

	/*
        Loop through each channel in the row until we have finished
    */
	for (count = 0; count < mNumChannels; count++,current++)
	{
		MusicChannelXM          *cptr  = 0;
		MusicVirtualChannel	    *vcptr = 0;
		MusicSample             *sptr  = 0;
		MusicInstrument         *iptr  = 0;
		unsigned char            paramx, paramy;
		int                      oldvolume, oldfreq, oldpan;
		bool                     porta = false;

		paramx = current->mEffectParam >> 4;			/* get effect param x */
		paramy = current->mEffectParam & 0xF;			/* get effect param y */

		cptr = (MusicChannelXM *)mMusicChannel[count];
		if (cptr->mVirtualChannelHead.isEmpty()) 
		{
			vcptr             = &gDummyVirtualChannel; /* no channels allocated yet */
			vcptr->mSample    = &gDummySample;
		}
        else
        {
            vcptr = (MusicVirtualChannel *)cptr->mVirtualChannelHead.getNext();
        }

		porta = (current->mEffect == FMUSIC_XM_PORTATO || current->mEffect == FMUSIC_XM_PORTATOVOLSLIDE);
		if (porta && vcptr == &gDummyVirtualChannel) 
        {
			porta = false;  /* Note hasn't been triggered yet */
        }

		/* first store note and instrument number if there was one */
		if (current->mNumber)
		{
			if (!porta)							/*  bugfix 3.20 (&& !porta) */
            {
				cptr->mInstrument = current->mNumber-1;	/* remember the Instrument # */
            }
//		    if (mInstCallback[current->mNumber] && mInstCallback[current->mNumber]->callback)
//            {
//				FMUSIC_CheckCallback(mod, FMUSIC_CALLBACK_INSTRUMENT, current->mNumber);
//            }
		}

		if (current->mNote && current->mNote != FMUSIC_KEYOFF && !porta) /* bugfix 3.20 (&& !porta) */
        {
			cptr->mNote = current->mNote-1;						/* remember the note */
        }

		if (cptr->mInstrument >= mNumInstruments)
		{
			iptr = &gDummyInstrument;
			sptr = &gDummySample;
			sptr->mSound = 0;
		}
		else
		{
			/*
                Set up some instrument and sample pointers
            */
			iptr = &mInstrument[cptr->mInstrument];
			if (iptr->mKeyMap[cptr->mNote] >= 16) 
            {
				sptr = &gDummySample;
            }
			else 
            {
				sptr = &iptr->mSample[iptr->mKeyMap[cptr->mNote]];
            }

			if (!porta)
            {
				vcptr->mSample = sptr;
            }
		}

		oldvolume = vcptr->mVolume;
		oldfreq   = vcptr->mFrequency;
		oldpan    = vcptr->mPan;

		/*
            If there is no more tremolo, set volume to volume + last tremolo delta
        */
		if (cptr->mRecentEffect == FMUSIC_XM_TREMOLO && current->mEffect != FMUSIC_XM_TREMOLO)
        {
			vcptr->mVolume += vcptr->mVolumeDelta;
        }
		cptr->mRecentEffect = current->mEffect;

		vcptr->mVolumeDelta = 0;
		vcptr->mNoteControl	= 0;

		/*
            PROCESS NOTE
        */
		if (current->mNote && current->mNote != FMUSIC_KEYOFF) 
		{
            if (!porta || vcptr == &gDummyVirtualChannel)
            {
			    spawnNewChannel(cptr, vcptr, sptr, &vcptr);
            }

			if (!vcptr)
			{
				vcptr             = &gDummyVirtualChannel; /* no channels allocated yet */
				vcptr->mSample    = &gDummySample;
			}

			/*
                Get note according to mRelative note
            */
			cptr->mRealNote = current->mNote + sptr->mRelative - 1;

			/*
                Get period according to mRealNote and finetune
            */
			if (mMusicFlags & FMUSIC_XMFLAGS_LINEARFREQUENCY) 
			{
				cptr->mPeriod = (10*12*16*4) - (cptr->mRealNote*16*4) - (sptr->mFineTune / 2);
			}
			else 
			{
				getAmigaPeriod(cptr->mRealNote, sptr->mFineTune, &cptr->mPeriod);
			}
			
			/*
                Frequency only changes if there are no portamento effects
            */
			if (!(current->mEffect == FMUSIC_XM_PORTATO || current->mEffect == FMUSIC_XM_PORTATOVOLSLIDE))
            {
				vcptr->mFrequency = cptr->mPeriod;
            }

			vcptr->mNoteControl = FMUSIC_TRIGGER;
		}

		vcptr->mFrequencyDelta	= 0;
		vcptr->mNoteControl		|= FMUSIC_FREQ;
		vcptr->mNoteControl		|= FMUSIC_VOLUME;

        processNote(current, cptr, vcptr, iptr, sptr);

		/*
            PROCESS TICK 0 EFFECTS
        */
		switch (current->mEffect) 
		{
			/*
                Not processed on tick 0
            */
			case FMUSIC_XM_ARPEGGIO : 
			{
				break;
			}
			case FMUSIC_XM_PORTAUP :
			{
				if (current->mEffectParam) 
                {
					cptr->mPortaUp = current->mEffectParam;
                }
				break;
			}
			case FMUSIC_XM_PORTADOWN :
			{
				if (current->mEffectParam) 
                {
					cptr->mPortaDown = current->mEffectParam;
                }
				break;
			}
			case FMUSIC_XM_PORTATO :
			{
				if (current->mEffectParam) 
                {
					cptr->mPortaSpeed = current->mEffectParam;
                }
				cptr->mPortaTarget = cptr->mPeriod;
				if (porta)
                {
                    vcptr->mNoteControl &= ~FMUSIC_TRIGGER;
                }
				vcptr->mNoteControl &= ~FMUSIC_FREQ;
				break;
			}
			case FMUSIC_XM_PORTATOVOLSLIDE : 
			{
				cptr->mPortaTarget = cptr->mPeriod;
				if (current->mEffectParam) 
                {
					cptr->mVolumeSlide = current->mEffectParam;
                }
				if (porta)
                {
                    vcptr->mNoteControl &= ~FMUSIC_TRIGGER;
                }
				vcptr->mNoteControl &= ~FMUSIC_FREQ;
				break;
			}
			case FMUSIC_XM_VIBRATO :
			{
				if (paramx) 
                {
					cptr->mVibSpeed = paramx;
                }
				if (paramy) 
                {
					cptr->mVibDepth = paramy;
                }
				cptr->vibrato();
				break;
			}
			case FMUSIC_XM_VIBRATOVOLSLIDE : 
			{
				if (current->mEffectParam) 
                {
					cptr->mVolumeSlide = current->mEffectParam;
                }
				cptr->vibrato();
				break;								/* not processed on tick 0 */
			}
			case FMUSIC_XM_TREMOLO : 
			{
				if (paramx) 
                {
					cptr->mTremoloSpeed = paramx;
                }
				if (paramy) 
                {
					cptr->mTremoloDepth = paramy;
                }
				break;
			}
			case FMUSIC_XM_SETPANPOSITION : 
			{
				vcptr->mPan = current->mEffectParam;
				vcptr->mNoteControl |= FMUSIC_PAN;
				break;
			}
			case FMUSIC_XM_SETSAMPLEOFFSET : 
			{
				unsigned int offset;

				if (current->mEffectParam) 
                {
					cptr->mSampleOffset = current->mEffectParam;
                }

				offset = cptr->mSampleOffset << 8;

				if (offset >= sptr->mLoopStart + sptr->mLoopLength) 
				{
					vcptr->mNoteControl &= ~FMUSIC_TRIGGER;
					vcptr->mNoteControl |= FMUSIC_STOP;
				}
				else 
                {
					vcptr->mSampleOffset = offset;
                }
				break;
			}
			case FMUSIC_XM_VOLUMESLIDE : 
			{
				if (current->mEffectParam) 
                {
					cptr->mVolumeSlide  = current->mEffectParam;
                }
				break;				
			}
			case FMUSIC_XM_PATTERNJUMP : /* --- 00 B00 : --- 00 D63 , should put us at ord=0, row=63 */
			{
				mNextOrder = current->mEffectParam;
				mNextRow = 0;
				if (mNextOrder >= mNumOrders) 
                {
					mNextOrder = 0;
					mFinished = true;
                }
				jumpflag = 1;
				break;
			}
			case FMUSIC_XM_SETVOLUME : 
			{
				vcptr->mVolume = current->mEffectParam;
				vcptr->mNoteControl |= FMUSIC_VOLUME;
				break;
			}
			case FMUSIC_XM_PATTERNBREAK : 
			{
				mNextRow = (paramx*10) + paramy;
				if (mNextRow > 63) 
                {
					mNextRow = 0;
                }
				if (!breakflag && !jumpflag) 
                {
					mNextOrder = mOrder+1;
                }
				if (mNextOrder >= mNumOrders) 
                {
					mNextOrder=0;
                }
				break;
			}
			case FMUSIC_XM_SPECIAL : 
			{
				switch (paramx) 
				{
					/*
                        Not processed on tick 0 / unsupported
                    */
					case FMUSIC_XM_RETRIG :
					case FMUSIC_XM_NOTECUT :
					case FMUSIC_XM_SETFILTER :
					case FMUSIC_XM_FUNKREPEAT :
					case FMUSIC_XM_SETGLISSANDO : 
					{
						break;
					}
					case FMUSIC_XM_FINEPORTAUP : 
					{
						if (paramy)			/* BUGFIX 3.30 - fineporta wasnt remembering parameter */
                        {
							cptr->mFinePortaUp = paramy;
                        }
						vcptr->mFrequency -= (cptr->mFinePortaUp << 2);
						break;
					}
					case FMUSIC_XM_FINEPORTADOWN :
					{
						if (paramy)			/* BUGFIX 3.30 - fineporta wasnt remembering parameter */
                        {
							cptr->mFinePortaDown = paramy;
                        }
						vcptr->mFrequency += (cptr->mFinePortaDown << 2);
						break;
					}
					case FMUSIC_XM_SETVIBRATOWAVE : 
					{
						cptr->mWaveControl &= 0xF0;
						cptr->mWaveControl |= paramy;
						break;
					}
					case FMUSIC_XM_SETFINETUNE :
					{
						sptr->mFineTune = paramy;
						break;
					}
					case FMUSIC_XM_PATTERNLOOP :
					{
						if (paramy == 0) 
                        {
							cptr->mPatternLoopRow = mRow;
                        }
						else 
						{
							if (!cptr->mPatternLoopNumber) 
                            {
								cptr->mPatternLoopNumber = paramy;
                            }
							else 
                            {
                                cptr->mPatternLoopNumber--;
                            }
							if (cptr->mPatternLoopNumber) 
                            {
                                int count2;

								mNextRow = cptr->mPatternLoopRow;

                                if (mVisited)
                                {
                                    for (count2 = cptr->mPatternLoopRow; count2 <= mRow; count2++)
                                    {
                                        mVisited[(mOrder * FMUSIC_MAXROWS) + count2] = false;
                                    }
                                }
                            }
						}
						break;
					}
					case FMUSIC_XM_SETTREMOLOWAVE : 
					{
						cptr->mWaveControl &= 0xF;
						cptr->mWaveControl |= (paramy<<4);
						break;
					}
					case FMUSIC_XM_SETPANPOSITION16 :
					{
						vcptr->mPan = paramy<<4;
						vcptr->mNoteControl |= FMUSIC_PAN;
						break;
					}
					case FMUSIC_XM_FINEVOLUMESLIDEUP :
					{
						if (paramy) 
                        {
							cptr->mFineVolumeSlideUp = paramy;
                        }

						vcptr->mVolume += cptr->mFineVolumeSlideUp;
						
						if (vcptr->mVolume > 64) 
                        {
							vcptr->mVolume = 64;
                        }

						vcptr->mNoteControl |= FMUSIC_VOLUME;
						break;
					}
					case FMUSIC_XM_FINEVOLUMESLIDEDOWN :
					{
						if (paramy) 
                        {
							cptr->mFineVolumeSlideUp = paramy;
                        }

						vcptr->mVolume -= cptr->mFineVolumeSlideUp;
						
						if (vcptr->mVolume < 0) 
                        {
							vcptr->mVolume = 0;
                        }

						vcptr->mNoteControl |= FMUSIC_VOLUME;
						break;
					}
					case FMUSIC_XM_NOTEDELAY : 
					{
						vcptr->mVolume = oldvolume;
						vcptr->mFrequency   = oldfreq;
						vcptr->mPan    = oldpan;
						vcptr->mNoteControl &= ~FMUSIC_FREQ;
						vcptr->mNoteControl &= ~FMUSIC_VOLUME;
						vcptr->mNoteControl &= ~FMUSIC_PAN;
						vcptr->mNoteControl &= ~FMUSIC_TRIGGER;
						break;
					}
					case FMUSIC_XM_PATTERNDELAY : 
					{
						mPatternDelay = paramy;
						mPatternDelay *= mSpeed;
						break;
					}
				};
				break;
			}
			case FMUSIC_XM_SETSPEED : 
			{
				if (current->mEffectParam < 0x20)
                {
                    if (current->mEffectParam)
                    {
					    mSpeed = current->mEffectParam;
                    }
                }
				else
                {
					setBPM(current->mEffectParam); 
                }
				break;
			}

			case FMUSIC_XM_SETGLOBALVOLUME : 
			{
				mGlobalVolume = current->mEffectParam;
				if (mGlobalVolume > 64) 
                {
					mGlobalVolume=64;
                }
				vcptr->mNoteControl |= FMUSIC_VOLUME;
				break;
			}
			case FMUSIC_XM_GLOBALVOLSLIDE :
			{
				if (current->mEffectParam) 
                {
					mGlobalVolumeSlide = current->mEffectParam;
                }
				break;
			}
			case FMUSIC_XM_SETENVELOPEPOS :
			{
				int currpos, nextpos;
				int currtick, nexttick;
				int currvol, nextvol, tickdiff;

				if (!(iptr->mVolumeType & FMUSIC_ENVELOPE_ON)) 
                {
                    break;
                }

				currpos = 0;
				
				/*
                    Search and reinterpolate new envelope position				
                */
				while (current->mEffectParam > iptr->mVolumePoints[(currpos+1)<<1] && 
                       currpos < iptr->mVolumeNumPoints) 
                {
                    currpos++;
                }

				vcptr->mEnvVolume.mPosition = currpos;

				/*
                    If it is at the last position, abort the envelope and continue last volume
                */
				if (vcptr->mEnvVolume.mPosition >= iptr->mVolumeNumPoints - 1) 
				{
					vcptr->mEnvVolume.mValue = iptr->mVolumePoints[((iptr->mVolumeNumPoints-1)<<1)+1];
					vcptr->mEnvVolume.mStopped = true;
					break;
				}
				
				vcptr->mEnvVolume.mStopped = false;
				vcptr->mEnvVolume.mTick = current->mEffectParam;

				nextpos	= vcptr->mEnvVolume.mPosition + 1;

				currtick = iptr->mVolumePoints[currpos<<1];				/* get tick at this point */
				nexttick = iptr->mVolumePoints[nextpos<<1];				/* get tick at next point */

				currvol = iptr->mVolumePoints[(currpos<<1)+1] << 16;	/* get VOL at this point << 16 */
				nextvol	= iptr->mVolumePoints[(nextpos<<1)+1] << 16;	/* get VOL at next point << 16 */

				/*
                    Interpolate 2 points to find delta step
                */
				tickdiff = nexttick - currtick;
				if (tickdiff) vcptr->mEnvVolume.mDelta = (nextvol-currvol) / tickdiff;
				else		  vcptr->mEnvVolume.mDelta = 0;

				tickdiff = vcptr->mEnvVolume.mTick - currtick;

				vcptr->mEnvVolume.mFraction = currvol + (vcptr->mEnvVolume.mDelta * tickdiff);
				vcptr->mEnvVolume.mValue = vcptr->mEnvVolume.mFraction >> 16;
				vcptr->mEnvVolume.mPosition++;
				break;
			}
			case FMUSIC_XM_PANSLIDE :
			{
				if (current->mEffectParam) 
				{
					cptr->mPanSlide = current->mEffectParam;
					vcptr->mNoteControl |= FMUSIC_PAN;
				}
				break;
			}
			case FMUSIC_XM_MULTIRETRIG:
			{
				if (current->mEffectParam) 
				{
					cptr->mRetrigX = paramx;
					cptr->mRetrigY = paramy;
				}
				break;
			}
			case FMUSIC_XM_TREMOR :
			{
				if (current->mEffectParam) 
				{
					cptr->mTremorOn = (paramx+1);
					cptr->mTremorOff = (paramy+1);
				}
				if (cptr->mTremorPosition >= cptr->mTremorOn) 
                {
					vcptr->mVolumeDelta = -vcptr->mVolume;
                }
				cptr->mTremorPosition++;
				if (cptr->mTremorPosition >= (cptr->mTremorOn + cptr->mTremorOff)) 
                {
					cptr->mTremorPosition = 0;
                }
				vcptr->mNoteControl |= FMUSIC_VOLUME;
				break;
			}
			case FMUSIC_XM_EXTRAFINEPORTA :
			{
				
				if (paramx == 1) 
				{
					if (paramy) 
                    {
						cptr->mXtraPortaUp = paramy;
                    }
					vcptr->mFrequency -= cptr->mXtraPortaUp;
				}
				else if (paramx == 2) 
				{
					if (paramy) 
                    {
						cptr->mXtraPortaDown = paramy;
                    }
					vcptr->mFrequency += cptr->mXtraPortaDown;
				}
				break;
			}
			case FMUSIC_XM_Z :
			{
//				FMUSIC_CheckCallback(mod, FMUSIC_CALLBACK_ZXX, current->mEffectParam);
				break;
			}
		};

		/*
            INSTRUMENT VIBRATO
        */
		cptr->instrumentVibrato(iptr);	/* this gets added to previous freqdeltas */
		updateFlags((MusicVirtualChannel *)cptr->mVirtualChannelHead.getNext(), sptr);
	}

    return FMOD_OK;
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
// VERIFY: c1dc2c44 auto
// OFFSET: 0x00904F00
FMOD_RESULT CodecXM::updateEffects() 
{
	MusicNote  *current;
	int 		count;

	current = mPattern[mOrderList[mOrder]].mData + (mRow*mNumChannels);

	if (!current) 
    {
		return FMOD_OK;
    }

	for (count=0; count < mNumChannels; count++,current++)
	{
		MusicChannelXM      *cptr  = 0;
		MusicVirtualChannel *vcptr = 0;
		MusicInstrument     *iptr  = 0;
		MusicSample         *sptr  = 0;
		unsigned char        effect, paramx, paramy;
		
		cptr = (MusicChannelXM *)mMusicChannel[count];
		
		if (cptr->mVirtualChannelHead.isEmpty()) 
        {
			vcptr = &gDummyVirtualChannel;   /* no channels allocated yet */
        }
        else
        {
            vcptr = (MusicVirtualChannel *)cptr->mVirtualChannelHead.getNext();
        }

		if (cptr->mInstrument >= mNumInstruments)
		{
			iptr = &gDummyInstrument;
			sptr = &gDummySample;
			sptr->mSound = 0;
		}
		else
		{
			iptr = &mInstrument[cptr->mInstrument];
			if (iptr->mKeyMap[cptr->mNote] >= 16) 
            {
				sptr = &gDummySample;
            }
			else 
            {
				sptr = &iptr->mSample[iptr->mKeyMap[cptr->mNote]];
            }

			if (!sptr)
            {
				sptr = &gDummySample;
            }
		}

		effect = current->mEffect;			/* grab the effect number */
		paramx = current->mEffectParam >> 4;		/* grab the effect parameter x */
		paramy = current->mEffectParam & 0xF;		/* grab the effect parameter y */

		vcptr->mVolumeDelta	= 0;				/* this is for tremolo / tremor etc */
		vcptr->mFrequencyDelta = 0;				/* this is for vibrato / arpeggio etc */
		vcptr->mNoteControl	= 0;

		/*
            PROCESS ENVELOPES
        */
		if (iptr->mVolumeType & FMUSIC_ENVELOPE_ON && !vcptr->mEnvVolume.mStopped)
        {
			processEnvelope(&vcptr->mEnvVolume, vcptr, iptr->mVolumeNumPoints, iptr->mVolumePoints, iptr->mVolumeType, iptr->mVolumeLoopStart, iptr->mVolumeLoopEnd, iptr->mVolumeSustain, FMUSIC_VOLUME);
        }
		if (iptr->mPanType & FMUSIC_ENVELOPE_ON && !vcptr->mEnvPan.mStopped) 
        {
			processEnvelope(&vcptr->mEnvPan, vcptr, iptr->mPanNumPoints, iptr->mPanPoints, iptr->mPanType, iptr->mPanLoopStart, iptr->mPanLoopEnd, iptr->mPanSustain, FMUSIC_PAN);
        }

		/*
            PROCESS VOLUME FADEOUT
        */
		if (vcptr->mKeyOff) 
		{
			vcptr->mFadeOutVolume -= iptr->mVolumeFade;
			if (vcptr->mFadeOutVolume < 0) 
            {
				vcptr->mFadeOutVolume = 0;
            }
			vcptr->mNoteControl |= FMUSIC_VOLUME;
		}

		switch (current->mVolume >> 4) 
		{
			case 0x6: 
			{
				vcptr->mVolume -= (current->mVolume & 0xF);
				if (vcptr->mVolume < 0) 
                {
					vcptr->mVolume = 0;
                }
				vcptr->mNoteControl |= FMUSIC_VOLUME;
				break;
			}
			case 0x7 :
			{
				vcptr->mVolume += (current->mVolume & 0xF);
				if (vcptr->mVolume > 0x40) 
                {
					vcptr->mVolume = 0x40;
                }
				vcptr->mNoteControl |= FMUSIC_VOLUME;
				break;
			}
			case 0xb :
			{
				cptr->mVibDepth = (current->mVolume & 0xF);
				cptr->vibrato();
				cptr->mVibPos += cptr->mVibSpeed;
				if (cptr->mVibPos > 31) 
                {
					cptr->mVibPos -= 64;
                }
				break;
			}
			case 0xd :
			{
				vcptr->mPan -= (current->mVolume & 0xF);
				vcptr->mNoteControl |= FMUSIC_PAN;
				break;
			}
			case 0xe :
			{
				vcptr->mPan += (current->mVolume & 0xF);
				vcptr->mNoteControl |= FMUSIC_PAN;
				break;
			}
			case 0xf :
			{
				cptr->portamento();
				break;
			}
		};


		switch(effect) 
		{
			case FMUSIC_XM_ARPEGGIO : 
			{
				if (current->mEffectParam > 0)
				{
					switch (mTick % 3) 
					{
						case 1: 
                        {
							if (mMusicFlags & FMUSIC_XMFLAGS_LINEARFREQUENCY) 
                            {
								vcptr->mFrequencyDelta = -paramx << 6;
                            }
							else 
                            {
                                int per1, per2;

                                getAmigaPeriod(cptr->mRealNote + paramx, sptr->mFineTune, &per1);
                                getAmigaPeriod(cptr->mRealNote,          sptr->mFineTune, &per2);

								vcptr->mFrequencyDelta = per1 - per2;
                            }
							break;
                        }
						case 2: 
                        {
							if (mMusicFlags & FMUSIC_XMFLAGS_LINEARFREQUENCY) 
                            {
								vcptr->mFrequencyDelta = -paramy << 6;
                            }
							else 
                            {
                                int per1, per2;

                                getAmigaPeriod(cptr->mRealNote + paramy, sptr->mFineTune, &per1);
                                getAmigaPeriod(cptr->mRealNote,          sptr->mFineTune, &per2);

								vcptr->mFrequencyDelta = per1 - per2;
                            }
							break;
                        }
					};
					vcptr->mNoteControl |= FMUSIC_FREQ;
				}
				break;
			}
			case FMUSIC_XM_PORTAUP : 
			{
				vcptr->mFrequencyDelta = 0;

				vcptr->mFrequency -= cptr->mPortaUp << 2; /* subtract freq */
				if (vcptr->mFrequency < 56) 
                {
					vcptr->mFrequency=56;  /* stop at B#8 */
                }
				vcptr->mNoteControl |= FMUSIC_FREQ;
				break;
			}
			case FMUSIC_XM_PORTADOWN : 
			{
				vcptr->mFrequencyDelta = 0;

				vcptr->mFrequency += cptr->mPortaDown << 2; /* subtract freq */
				vcptr->mNoteControl |= FMUSIC_FREQ;
				break;
			}
			case FMUSIC_XM_PORTATO : 
			{
				vcptr->mFrequencyDelta = 0;

				cptr->portamento();
				break;
			}
			case FMUSIC_XM_VIBRATO :
			{
				cptr->vibrato();
				cptr->mVibPos += cptr->mVibSpeed;
				if (cptr->mVibPos > 31) 
                {
					cptr->mVibPos -= 64;
                }
				break;
			}
			case FMUSIC_XM_PORTATOVOLSLIDE : 
			{
				vcptr->mFrequencyDelta = 0;

				cptr->portamento();

				paramx = cptr->mVolumeSlide >> 4;    /* grab the effect parameter x */
				paramy = cptr->mVolumeSlide & 0xF;   /* grab the effect parameter y */

				/* 
                    Slide up takes precedence over down 
                */
				if (paramx) 
				{
					vcptr->mVolume += paramx;
					if (vcptr->mVolume > 64)
                    { 
					    vcptr->mVolume = 64;
                    }
				}
				else if (paramy) 
				{
					vcptr->mVolume -= paramy;
					if (vcptr->mVolume < 0)
                    {
						vcptr->mVolume = 0;
                    }
				}

				vcptr->mNoteControl |= FMUSIC_VOLUME;
				break;
			}
			case FMUSIC_XM_VIBRATOVOLSLIDE : 
			{
				cptr->vibrato();
				cptr->mVibPos += cptr->mVibSpeed;
				if (cptr->mVibPos > 31)
                {
					cptr->mVibPos -= 64;
                }

				paramx = cptr->mVolumeSlide >> 4;    /* grab the effect parameter x */
				paramy = cptr->mVolumeSlide & 0xF;   /* grab the effect parameter y */

				/*
                    Slide up takes precedence over down
                */
				if (paramx) 
				{
					vcptr->mVolume += paramx;
					if (vcptr->mVolume > 64)
                    {
						vcptr->mVolume = 64;
                    }
				}
				else if (paramy) 
				{
					vcptr->mVolume -= paramy;
					if (vcptr->mVolume < 0)
                    {
						vcptr->mVolume = 0;
                    }
				}

				vcptr->mNoteControl |= FMUSIC_VOLUME;
				break;
			}
			case FMUSIC_XM_TREMOLO :
			{
				cptr->tremolo();
				break;
			}
			case FMUSIC_XM_VOLUMESLIDE : 
			{
				paramx = cptr->mVolumeSlide >> 4;    /* grab the effect parameter x */
				paramy = cptr->mVolumeSlide & 0xF;   /* grab the effect parameter y */
				
				/*
                    Slide up takes precedence over down
                */
				if (paramx) 
				{
					vcptr->mVolume += paramx;
					if (vcptr->mVolume > 64) 
                    {
						vcptr->mVolume = 64;
                    }
				}
				else if (paramy) 
				{
					vcptr->mVolume -= paramy;
					if (vcptr->mVolume < 0)  
                    {
						vcptr->mVolume = 0;
                    }
				}

				vcptr->mNoteControl |= FMUSIC_VOLUME;
				break;
			}

			/*
                Extended PT effects
            */
			case FMUSIC_XM_SPECIAL: 
			{
				switch (paramx) 
				{
					case FMUSIC_XM_NOTECUT: 
					{
						if (mTick==paramy) 
						{
							vcptr->mVolume = 0;
							vcptr->mNoteControl |= FMUSIC_VOLUME;
						}
						break;
					}
					case FMUSIC_XM_RETRIG : 
					{
						if (!paramy) 
                        {
							break; /* divide by 0 bugfix */
                        }
						if (!(mTick % paramy)) 
						{
							vcptr->mNoteControl |= FMUSIC_TRIGGER;
							vcptr->mNoteControl |= FMUSIC_VOLUME;
							vcptr->mNoteControl |= FMUSIC_FREQ;
						}
						break;
					}
					case FMUSIC_XM_NOTEDELAY : 
					{
						if (mTick == paramy) 
						{
							spawnNewChannel(cptr, vcptr, sptr, &vcptr);

							vcptr->mFrequency = cptr->mPeriod;
							vcptr->mNoteControl |= FMUSIC_FREQ;
							vcptr->mNoteControl |= FMUSIC_TRIGGER;
                            
                            processNote(current, cptr, vcptr, iptr, sptr);
						}
						else
						{
							vcptr->mNoteControl &= ~FMUSIC_VOLUME;
							vcptr->mNoteControl &= ~FMUSIC_FREQ;
							vcptr->mNoteControl &= ~FMUSIC_PAN;
							vcptr->mNoteControl &= ~FMUSIC_TRIGGER;
						}
						break;
					}
				};
				break;
			}

			case FMUSIC_XM_MULTIRETRIG : 
			{
				if (!cptr->mRetrigY) 
                {
					break; /* divide by 0 bugfix */
                }
				
				if (!(mTick % cptr->mRetrigY)) 
				{
					if (cptr->mRetrigX) 
					{
						switch (cptr->mRetrigX) 
						{
							case 1: vcptr->mVolume--;
									break;
							case 2: vcptr->mVolume -= 2;
									break;
							case 3: vcptr->mVolume -= 4;
									break;
							case 4: vcptr->mVolume -= 8;
									break;
							case 5: vcptr->mVolume -= 16;
									break;
							case 6: vcptr->mVolume = vcptr->mVolume * 2 / 3;
									break;
							case 7: vcptr->mVolume >>= 1;
									break;
							case 8: /* ? */
									break;
							case 9: vcptr->mVolume++;
									break;
							case 0xA: vcptr->mVolume += 2;
									break;
							case 0xB: vcptr->mVolume += 4;
									break;
							case 0xC: vcptr->mVolume += 8;
									break;
							case 0xD: vcptr->mVolume += 16;
									break;
							case 0xE: vcptr->mVolume = vcptr->mVolume * 3 / 2;
									break;
							case 0xF: vcptr->mVolume <<= 1;
									break;
						};
						if (vcptr->mVolume > 64)
                        { 
							vcptr->mVolume = 64;
                        }
						if (vcptr->mVolume < 0)  
                        {
							vcptr->mVolume = 0;
                        }
					}
					vcptr->mNoteControl |= FMUSIC_VOLUME;
					vcptr->mNoteControl |= FMUSIC_TRIGGER;
				}
				break;
			}
			case FMUSIC_XM_GLOBALVOLSLIDE : 
			{
				paramx = mGlobalVolumeSlide >> 4;    /* grab the effect parameter x */
				paramy = mGlobalVolumeSlide & 0xF;   /* grab the effect parameter y */
				
				/* 
                    Slide up takes precedence over down 
                */
				if (paramx) 
				{
					mGlobalVolume += paramx;
					if (mGlobalVolume > 64) 
                    {
                        mGlobalVolume = 64;
                    }
				}
				else if (paramy) 
				{
					mGlobalVolume -= paramy;
					if (mGlobalVolume < 0)
                    {
                        mGlobalVolume = 0;
                    }
				}
				break;
			}
			case FMUSIC_XM_PANSLIDE :
			{
				paramx = cptr->mPanSlide >> 4;    /* grab the effect parameter x */
				paramy = cptr->mPanSlide & 0xF;   /* grab the effect parameter y */
				
				/* 
                    Slide right takes precedence over left 
                */
				if (paramx) 
				{
					vcptr->mPan += paramx;
					if (vcptr->mPan > 255) 
                    {
                        vcptr->mPan = 255;
                    }
				}
				else if (paramy) 
				{
					vcptr->mPan -= paramy;
					if (vcptr->mPan < 0)
                    {
                        vcptr->mPan = 0;
                    }
				}

				vcptr->mNoteControl |= FMUSIC_PAN;
				break;
			}
			case FMUSIC_XM_TREMOR :
			{
				if (cptr->mTremorPosition >= cptr->mTremorOn) 
                {
                    vcptr->mVolumeDelta = -vcptr->mVolume;
                }
				cptr->mTremorPosition++;
				if (cptr->mTremorPosition >= cptr->mTremorOn + cptr->mTremorOff) 
                {
                    cptr->mTremorPosition = 0;
                }
				vcptr->mNoteControl |= FMUSIC_VOLUME;
				break;
			}
		};

		/*
            INSTRUMENT VIBRATO
        */
		cptr->instrumentVibrato(iptr);	/* this gets added to previous freqdeltas */
        updateFlags((MusicVirtualChannel *)cptr->mVirtualChannelHead.getNext(), sptr);
	}

    return FMOD_OK;
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
// VERIFY: 8a8cf10f manual
// OFFSET: 0x009059C0
FMOD_RESULT CodecXM::update(bool audible)
{
    //FLOG((FMOD_DEBUG_LEVEL_ERROR, __FILE__, __LINE__, "CodecXM::update", "tick %d row %d\n", mTick, mRow));
	
    if (mTick == 0)	/* new note */
	{
        if (mFinished && !mLooping)
        {
            stop();
        }
        else
        {
		    /* process any rows commands to set the next order/row */
		    if (mNextOrder >= 0) 
            {
			    mOrder = mNextOrder;
//			    FMUSIC_CheckCallback(mod, FMUSIC_CALLBACK_ORDER, (unsigned char)mOrder);
		        if (mNextOrder >= 0) 
                {
			        mOrder = mNextOrder;
                }
        	    mNextOrder = -1;
            }
		    if (mNextRow >= 0)   
            {
			    mRow = mNextRow;
//			    FMUSIC_CheckCallback(mod, FMUSIC_CALLBACK_ROW, (unsigned char)mRow);
		        if (mNextRow >= 0)   
                {
			        mRow = mNextRow;
                }
	            mNextRow = -1;
            }

		    updateNote(audible);					/* Update and play the note */

		    /*
                If there were no row commands
            */
		    if (mNextRow == -1)
		    {
			    mNextRow = mRow+1;
			    if (mNextRow >= mPattern[mOrderList[mOrder]].mRows)	/* if end of pattern */
			    { 
				    mNextOrder = mOrder+1;			/* so increment the order */
				    if (mNextOrder >= mNumOrders) 
				    {
					    mNextOrder = mRestart;
				    }
				    mNextRow = 0;						/* start at top of pattern */
			    }
		    }
        }
	}
	else if (audible)
    {
		updateEffects();					/* Else update the inbetween row effects */
    }

    if (mSpeed)
    {
	    mTick++;
	    if (mTick >= mSpeed + mPatternDelay)
	    {
		    mPatternDelay = 0;
		    mTick = 0;
	    }
    }
    else
    {
        mFinished = true;
    }

    mPCMOffset += mMixerSamplesPerTick;

    return FMOD_OK;
}

#ifdef FMOD_SUPPORT_VAG

static float EncodeVAG_f[5][2] =
{
    {    0.0f        , 0.0f             },
   	{  -60.0f / 64.0f, 0.0f             },
    { -115.0f / 64.0f, 52.0f / 64.0f    },
    {  -98.0f / 64.0f, 55.0f / 64.0f    },
    { -122.0f / 64.0f, 60.0f / 64.0f    } 
};

void CodecXM::EncodeVAG_FindPredict( short *samples, float *d_samples, int *predict_nr, int *shift_factor )
{
    int i, j;
    float buffer[28][5];
    float min = 1e10;
    float max[5];
    float ds;
    int min2;
    int shift_mask;
    static float _s_1 = 0.0f;                            // s[t-1]
    static float _s_2 = 0.0f;                            // s[t-2]
    float s_0, s_1, s_2;

    for ( i = 0; i < 5; i++ ) 
    {
        max[i] = 0.0f;
        s_1 = _s_1;
        s_2 = _s_2;
        for ( j = 0; j < 28; j ++ ) 
        {
            s_0 = (float) samples[j];                      // s[t-0]
            if ( s_0 > 30719.0f )
                s_0 = 30719.0f;
            if ( s_0 < - 30720.0f )
                s_0 = -30720.0f;
            ds = s_0 + s_1 * EncodeVAG_f[i][0] + s_2 * EncodeVAG_f[i][1];
            buffer[j][i] = ds;
            if ( FMOD_FABS( ds ) > max[i] )
                max[i] = FMOD_FABS( ds );
            s_2 = s_1;                                  // new s[t-2]
            s_1 = s_0;                                  // new s[t-1]
        }

        if ( max[i] < min ) 
        {
            min = max[i];
            *predict_nr = i;
        }
        if ( min <= 7 ) 
        {
            *predict_nr = 0;
            break;
        }
    }

    // store s[t-2] and s[t-1] in a static variable
    // these than used in the next function call

    _s_1 = s_1;
    _s_2 = s_2;

    for ( i = 0; i < 28; i++ )
        d_samples[i] = buffer[i][*predict_nr];

    min2 = ( int ) min;
    shift_mask = 0x4000;
    *shift_factor = 0;

    while( *shift_factor < 12 ) 
    {
        if ( shift_mask  & ( min2 + ( shift_mask >> 3 ) ) )
            break;
        (*shift_factor)++;
        shift_mask = shift_mask >> 1;
    }

}

void CodecXM::EncodeVAG_pack( float *d_samples, short *four_bit, int predict_nr, int shift_factor )
{
    float ds;
    int di;
    float s_0;
    static float s_1 = 0.0f;
    static float s_2 = 0.0f;
    int i;

    for ( i = 0; i < 28; i++ ) 
    {
        s_0 = d_samples[i] + s_1 * EncodeVAG_f[predict_nr][0] + s_2 * EncodeVAG_f[predict_nr][1];
        ds = s_0 * (float) ( 1 << shift_factor );

        di = ( (int) ds + 0x800 ) & 0xfffff000;

        if ( di > 32767 )
            di = 32767;
        if ( di < -32768 )
            di = -32768;

        four_bit[i] = (short) di;

        di = di >> shift_factor;
        s_2 = s_1;
        s_1 = (float) di - s_0;
    }
}

int CodecXM::EncodeVAG_pcm2vag(unsigned char *vag, short *wave, int sample_len, int wavebits)
{
    short *ptr;
    float d_samples[28];
    short four_bit[28];
    short temp_samples[28];
    int predict_nr;
    int shift_factor;
    int flags=0;
    int numframes, i, j, k;    
    unsigned char d;
    unsigned char *vag_ptr = vag;

    sample_len = sample_len * 8 / wavebits;

    for( i = 0; i < 16; i++ )
    {
        *vag_ptr = 0; vag_ptr++;
    }

    numframes = sample_len / 28;

    if ( sample_len % 28 )
    {
//        for ( j = sample_len % 28; j < 28; j++ )
//            wave[28*numframes+j] = 0;
        numframes++;
    }

    for ( j = 0; j < numframes; j++ )                                      
    {        
        if (wavebits == 8)
        {
            int count;
            signed char *ptr8 = (signed char *)wave + j * 28;

            for (count = 0; count < 28; count++)
            {
                temp_samples[count] = ptr8[count] << 8;
            }
            ptr = temp_samples;
        }
        else
        {
            ptr = wave + j * 28;
        }

        EncodeVAG_FindPredict( ptr, d_samples, &predict_nr, &shift_factor );
        EncodeVAG_pack( d_samples, four_bit, predict_nr, shift_factor );
        d = ( predict_nr << 4 ) | shift_factor;

        *vag_ptr++ = d;
        *vag_ptr++ = flags;

        for ( k = 0; k < 28; k += 2 ) 
        {
            d = ( ( four_bit[k+1] >> 8 ) & 0xf0 ) | ( ( four_bit[k] >> 12 ) & 0xf );
            *vag_ptr++ = d;
        }
        sample_len -= 28;
        if ( sample_len < 28 )
            flags = 1;
    }

    *vag_ptr++ = ( predict_nr << 4 ) | shift_factor;
    *vag_ptr++ = 7;

    for ( i = 0; i < 14; i++ )
    {
        *vag_ptr++ = 0;
    }

    //FLOG((FMOD_DEBUG_LEVEL_ERROR, __FILE__, __LINE__, "CodecXM::EncodeVAG_pcm2vag", "encoded len = %d.  numframes = %d\n", (vag_ptr-vag), numframes));

    return static_cast<int>((vag_ptr-vag));
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
// VERIFY: 4d518743 manual
// OFFSET: 0x00906270
FMOD_RESULT CodecXM::openInternal(FMOD_MODE usermode, FMOD_CREATESOUNDEXINFO *userexinfo)
{
	unsigned short	filenumpatterns=0;
	int 			count;
	unsigned int 	mainHDRsize, lengthbytes;
	char		    str[256];
    unsigned char   temp;
    FMOD_RESULT     result = FMOD_OK;

    if (!(mFile->mSeekable))
    {
        return FMOD_ERR_FORMAT;
    }

    init(FMOD_SOUND_TYPE_XM);

    FLOG((FMOD_DEBUG_LEVEL_LOG, __FILE__, __LINE__, "CodecXM::openInternal", "attempting to open as XM..\n"));

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

	/*
        Verify Format
    */
    result = mFile->seek(0, SEEK_SET);
    if (result != FMOD_OK)
    {
        return result;
    }

	result = mFile->read(str, 1, 17, 0);
    if (result != FMOD_OK)
    {
        return result;
    }

	if (FMOD_strncmp((char *)str, "Extended Module: ", 17))
	{
        FLOG((FMOD_DEBUG_LEVEL_LOG, __FILE__, __LINE__, "CodecXM::openInternal", "'Extended Module: ' ID check failed [%c%c%c%c]\n", str[0], str[1], str[2], str[3]));

		return FMOD_ERR_FORMAT;
	}

    FLOG((FMOD_DEBUG_LEVEL_LOG, __FILE__, __LINE__, "CodecXM::openInternal", "Loading XM header\n"));

	/*
        Set a few default values for this format
    */
    for (count = 0; count < MUSIC_MAXCHANNELS; count++)
    {
        mMusicChannel[count] = 0;
    }
	mPattern                = 0;
	mPanSeparation          = 1.0f;
	mMasterSpeed            = 1.0f;
	mDefaultSpeed			= 6;
	mDefaultBPM				= 125;
	mDefaultGlobalVolume    = 64;
	mNumPatterns			= 0;
	mRestart				= 0;
	mNumSamples		        = 0;

	result = mFile->read(mSongName, 1, 20, 0);
    if (result != FMOD_OK)
    {
        return result;
    }

	result = mFile->getByte(&temp);
    if (result != FMOD_OK)
    {
        return result;
    }
    if (temp != 0x1a)
    {
        return FMOD_ERR_FORMAT;
    }

	/*
        Skip tracker name and version number
    */
	result = mFile->seek(60, SEEK_SET);
    if (result != FMOD_OK)
    {
        return result;
    }
	result = mFile->read(&mainHDRsize, 4, 1, 0);
    if (result != FMOD_OK)
    {
        return result;
    }
	result = mFile->getWord(&mNumOrders);
    if (result != FMOD_OK)
    {
        return result;
    }
	result = mFile->getWord(&mRestart);
    if (result != FMOD_OK)
    {
        return result;
    }
	result = mFile->getWord(&mNumChannels);
    if (result != FMOD_OK)
    {
        return result;
    }
	result = mFile->getWord(&filenumpatterns);
    if (result != FMOD_OK)
    {
        return result;
    }
	result = mFile->getWord(&mNumInstruments);
    if (result != FMOD_OK)
    {
        return result;
    }
	result = mFile->getWord(&mMusicFlags);
    if (result != FMOD_OK)
    {
        return result;
    }
	result = mFile->getWord(&mDefaultSpeed);
    if (result != FMOD_OK)
    {
        return result;
    }
	result = mFile->getWord(&mDefaultBPM);
    if (result != FMOD_OK)
    {
        return result;
    }
	result = mFile->read(&mOrderList, 1, 256, 0);
    if (result != FMOD_OK)
    {
        return result;
    }

    result = metaData(FMOD_TAGTYPE_FMOD, "Number of channels", &mNumChannels, sizeof(mNumChannels), FMOD_TAGDATATYPE_INT, false);
    if (result != FMOD_OK)
    {
        return result;
    }

	/*
        ALLOCATE MUSIC CHANNELS
    */
    for (count = 0; count < mNumChannels; count++)
    {
        mMusicChannel[count] = FMOD_Object_Calloc(MusicChannelXM);
        if (!mMusicChannel[count])
        {
            return FMOD_ERR_MEMORY;
        }
    }

	/*
        Seek to patterndata
    */
	result = mFile->seek(60L+mainHDRsize, SEEK_SET);
    if (result != FMOD_OK)
    {
        return result;
    }

	mNumPatterns = 0;
	for (count=0; count<mNumOrders; count++)
	{
		if (mOrderList[count] >= mNumPatterns) 
        {
            mNumPatterns = mOrderList[count]+1;
        }
	}

    FLOG((FMOD_DEBUG_LEVEL_LOG, __FILE__, __LINE__, "CodecXM::openInternal", "Loading XM patterns\n"));

	/*
        Alloc pattern array (whatever is bigger.. filenumpatterns or mNumpatterns)
    */
	mNumPatternsMem = (mNumPatterns > filenumpatterns ? mNumPatterns : filenumpatterns);
	mPattern = (MusicPattern *)FMOD_Memory_Calloc(mNumPatternsMem * sizeof(MusicPattern));
	if (!mPattern) 
	{
		return FMOD_ERR_MEMORY;
	}

	/*
        Unpack and read patterns
    */
	for (count=0; count < filenumpatterns; count++) 
	{
		MusicPattern   *pptr;
		unsigned short	patternsize, rows;
		unsigned int 	patternHDRsize;

		pptr = &mPattern[count];

		result = mFile->read(&patternHDRsize, 4, 1, 0);
        if (result != FMOD_OK)
        {
            return result;
        }
		result = mFile->getByte();                       /* should be 0 */
        if (result != FMOD_OK)
        {
            return result;
        }
		result = mFile->getWord(&rows);
        if (result != FMOD_OK)
        {
            return result;
        }
        if (result != FMOD_OK)
        {
            return result;
        }
		result = mFile->getWord(&patternsize);
        if (result != FMOD_OK)
        {
            return result;
        }
        if (result != FMOD_OK)
        {
            return result;
        }

		pptr->mRows = rows;

		/*
            Allocate memory for pattern buffer
        */
		pptr->mData = (MusicNote *)FMOD_Memory_Calloc(mNumChannels * pptr->mRows * sizeof(MusicNote));	
		if (!pptr->mData)
		{
			return FMOD_ERR_MEMORY;
		}

		if (patternsize > 0) 
		{
			int count2;
			MusicNote *nptr;

			nptr = pptr->mData;

			for (count2=0; count2< (pptr->mRows * mNumChannels); count2++) 
			{
				unsigned char dat;

				result = mFile->getByte(&dat);
                if (result != FMOD_OK)
                {
                    return result;
                }
				if (dat & 0x80) 
				{
					if (dat & 1)  
                    {
                        result = mFile->getByte(&nptr->mNote);
                        if (result != FMOD_OK)
                        {
                            return result;
                        }
                    }
					if (dat & 2)  
                    {
                        result = mFile->getByte(&nptr->mNumber);
                        if (result != FMOD_OK)
                        {
                            return result;
                        }
                    }
					if (dat & 4)  
                    {
                        result = mFile->getByte(&nptr->mVolume);
                        if (result != FMOD_OK)
                        {
                            return result;
                        }
                    }
					if (dat & 8)  
                    {
                        result = mFile->getByte(&nptr->mEffect);
                        if (result != FMOD_OK)
                        {
                            return result;
                        }
                    }
					if (dat & 16) 
                    {   
                        result = mFile->getByte(&nptr->mEffectParam);
                        if (result != FMOD_OK)
                        {
                            return result;
                        }
                    }
				}
				else 
				{
					if (dat) 
                    {
						nptr->mNote = dat;
                    }
					result = mFile->getByte(&nptr->mNumber);
                    if (result != FMOD_OK)
                    {
                        return result;
                    }
					result = mFile->getByte(&nptr->mVolume);
                    if (result != FMOD_OK)
                    {
                        return result;
                    }
					result = mFile->getByte(&nptr->mEffect);
                    if (result != FMOD_OK)
                    {
                        return result;
                    }
					result = mFile->getByte(&nptr->mEffectParam);
                    if (result != FMOD_OK)
                    {
                        return result;
                    }
				}
				if (nptr->mNote == 97) 
                {
					nptr->mNote = FMUSIC_KEYOFF;
                }

				if (nptr->mNumber > 0x80)
                {
					nptr->mNumber = 0;
                }

				nptr++;
			}
		}
	}

	/*
        Allocate and clean out any extra patterns 
    */
	if (mNumPatterns > filenumpatterns)
	{
		for (count=filenumpatterns; count < mNumPatterns; count++) 
		{			
			MusicPattern *pptr;

			pptr = &mPattern[count];
			pptr->mRows = 64;

			/*
                Allocate memory for pattern buffer
            */
			pptr->mData = (MusicNote *)FMOD_Memory_Calloc(mNumChannels * pptr->mRows * sizeof(MusicNote));
			if (!pptr->mData)
			{
				return FMOD_ERR_MEMORY;
			}
		}
	}

    FLOG((FMOD_DEBUG_LEVEL_LOG, __FILE__, __LINE__, "CodecXM::openInternal", "Loading XM instruments\n"));

	/*
        Alloc instrument array
    */
	mInstrument = (MusicInstrument *)FMOD_Memory_Calloc((mNumInstruments) * sizeof(MusicInstrument));
	if (!mInstrument) 
	{
		return FMOD_ERR_MEMORY;
	}

	/*
        Load instrument information
    */
	for (count = 0; count < mNumInstruments; count++) 
	{
		unsigned int        count2;
		MusicInstrument	*iptr;
		unsigned int        instHDRsize;
		unsigned short      numsamples;
		unsigned char       *buff = 0;
		unsigned int        largestsample = 0;
		unsigned int        firstsampleoffset;

		/*
            Point a pointer to that particular instrument
        */
		iptr = &mInstrument[count];

        mFile->tell(&firstsampleoffset);

		result = mFile->read(&instHDRsize, 4, 1, 0);			/* instrument size */
        if (result != FMOD_OK)
        {
            break;
        }
		firstsampleoffset += instHDRsize;

		result = mFile->read(iptr->mName, 1, 22, 0); 			/* instrument name */
        if (result != FMOD_OK)
        {
            return result;
        }
		result = mFile->getByte();
        if (result != FMOD_OK)
        {
            return result;
        }
		result = mFile->getWord(&numsamples);
        if (result != FMOD_OK)
        {
            return result;
        }

		iptr->mNumSamples = numsamples;

		if (numsamples > 0) 
		{
			unsigned int sampHDRsize;

			result = mFile->read(&sampHDRsize, 4, 1, 0);		/* sampleheader size */
            if (result != FMOD_OK)
            {
                return result;
            }
			result = mFile->read(iptr->mKeyMap, 1, 96, 0);		/* sample numbers */
            if (result != FMOD_OK)
            {
                return result;
            }
			result = mFile->read(iptr->mVolumePoints, 2, 24, 0);	/* Volume Envelope (24 words) */
            if (result != FMOD_OK)
            {
                return result;
            }
			result = mFile->read(iptr->mPanPoints, 2, 24, 0);	/* Panning Envelope (24 words) */
            if (result != FMOD_OK)
            {
                return result;
            }

			result = mFile->getByte(&iptr->mVolumeNumPoints);
            if (result != FMOD_OK)
            {
                return result;
            }
			result = mFile->getByte(&iptr->mPanNumPoints);
            if (result != FMOD_OK)
            {
                return result;
            }
			result = mFile->getByte(&iptr->mVolumeSustain);
            if (result != FMOD_OK)
            {
                return result;
            }
			result = mFile->getByte(&iptr->mVolumeLoopStart);
            if (result != FMOD_OK)
            {
                return result;
            }
			result = mFile->getByte(&iptr->mVolumeLoopEnd);
            if (result != FMOD_OK)
            {
                return result;
            }
			result = mFile->getByte(&iptr->mPanSustain	   );
            if (result != FMOD_OK)
            {
                return result;
            }
			result = mFile->getByte(&iptr->mPanLoopStart	   );
            if (result != FMOD_OK)
            {
                return result;
            }
			result = mFile->getByte(&iptr->mPanLoopEnd	   );
            if (result != FMOD_OK)
            {
                return result;
            }
			result = mFile->getByte(&iptr->mVolumeType	);
            if (result != FMOD_OK)
            {
                return result;
            }
			result = mFile->getByte(&iptr->mPanType		   );
            if (result != FMOD_OK)
            {
                return result;
            }
			result = mFile->getByte(&iptr->mVibratoType	);
            if (result != FMOD_OK)
            {
                return result;
            }
			result = mFile->getByte(&iptr->mVibratoSweep	);
            if (result != FMOD_OK)
            {
                return result;
            }
			result = mFile->getByte(&iptr->mVibratoDepth	);
            if (result != FMOD_OK)
            {
                return result;
            }
			result = mFile->getByte(&iptr->mVibratoRate	);
            if (result != FMOD_OK)
            {
                return result;
            }
			result = mFile->getWord(&iptr->mVolumeFade);
            if (result != FMOD_OK)
            {
                return result;
            }
			iptr->mVolumeFade *= 2;		/* i DONT KNOW why i needed this.. it just made it work */

			if (iptr->mVolumeNumPoints < 2) 
            {
				iptr->mVolumeType = FMUSIC_ENVELOPE_OFF;
            }
			if (iptr->mPanNumPoints < 2) 
            {
				iptr->mPanType = FMUSIC_ENVELOPE_OFF;
            }

			/*
                Seek to first sample
            */
			result = mFile->seek(firstsampleoffset, SEEK_SET);
            if (result != FMOD_OK)
            {
                return result;
            }

            FLOG((FMOD_DEBUG_LEVEL_LOG, __FILE__, __LINE__, "CodecXM::openInternal", "Loading XM Instrument %s. %d samples\n", iptr->mName, numsamples));

			for (count2 = 0; count2 < numsamples; count2++) 
			{
				MusicSample	       *sptr;
                char                sample_name[22];
                FMOD_MODE           sample_mode;
                unsigned int        sample_length;
                int                 sample_channels;
                FMOD_SOUND_FORMAT   sample_format;
				unsigned char	    dat;

				sptr = &iptr->mSample[count2];
                FMOD_memset(sptr, 0, sizeof(MusicSample));
				mNumSamples++;

   				result = mFile->read(&sample_length, 4, 1, 0);
                if (result != FMOD_OK)
                {
                    return result;
                }
				result = mFile->read(&sptr->mLoopStart, 4, 1, 0);
                if (result != FMOD_OK)
                {
                    return result;
                }
				result = mFile->read(&sptr->mLoopLength, 4, 1, 0);
                if (result != FMOD_OK)
                {
                    return result;
                }
				result = mFile->getByte(&sptr->mDefaultVolume);
                if (result != FMOD_OK)
                {
                    return result;
                }
				result = mFile->getByte(&sptr->mFineTune);
                if (result != FMOD_OK)
                {
                    return result;
                }
                			
                /*
                    Type of sample
                */
	        	sample_mode             = FMOD_SOFTWARE | FMOD_2D;
                sample_channels         = 1;
                sample_format   = FMOD_SOUND_FORMAT_PCM8;
    
				result = mFile->getByte(&dat);
                if (result != FMOD_OK)
                {
                    return result;
                }
				if (dat & 1) 
				{
                    sample_mode |= FMOD_LOOP_NORMAL;
				}
				if (dat & 2) 
				{
                    sample_mode &= ~FMOD_LOOP_NORMAL;
                    sample_mode |= FMOD_LOOP_BIDI;
				}
				if (dat & 16)
                {
                    sample_format = FMOD_SOUND_FORMAT_PCM16;
                }
				if (dat & 32)
                {
                    sample_channels = 2;
                }

				if (!(sample_mode & FMOD_LOOP_NORMAL) && !(sample_mode & FMOD_LOOP_BIDI)) 
				{
					sptr->mLoopStart = 0;
					sptr->mLoopLength = sample_length;
				}

				if (!sptr->mLoopLength) 
				{
					sptr->mLoopStart = 0;
					sptr->mLoopLength = sample_length;

                    sample_mode &= ~FMOD_LOOP_NORMAL;
                    sample_mode &= ~FMOD_LOOP_BIDI;
                    sample_mode |=  FMOD_LOOP_OFF;
				}

                sptr->mRawLength  = sample_length;
				sptr->mLoopStart  = sptr->mLoopStart  / (sample_format == FMOD_SOUND_FORMAT_PCM8 ? 1 : 2) / sample_channels;
				sptr->mLoopLength = sptr->mLoopLength / (sample_format == FMOD_SOUND_FORMAT_PCM8 ? 1 : 2) / sample_channels;

				result = mFile->getByte(&sptr->mDefaultPan);
                if (result != FMOD_OK)
                {
                    return result;
                }
				result = mFile->getByte(&sptr->mRelative);
                if (result != FMOD_OK)
                {
                    return result;
                }

				/* reserved */
				result = mFile->getByte();
                if (result != FMOD_OK)
                {
                    return result;
                }

				/* sample name */
				result = mFile->read(sample_name, 1, 22, 0);
                if (result != FMOD_OK)
                {
                    return result;
                }

                {
                    char s[256];
         
                    sprintf(s, "Sample name %d", count);
         
                    result = metaData(FMOD_TAGTYPE_FMOD, s, sample_name, 28, FMOD_TAGDATATYPE_STRING, false);
                    if (result != FMOD_OK)
                    {
                        return result;
                    }
                }

               
				/*
                    ALLOCATE MEMORY FOR THE SAMPLE BUFFER
                */
                if (sample_length)
		        {
                    FMOD_CREATESOUNDEXINFO exinfo;

                    FMOD_memset(&exinfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));            
                    exinfo.cbsize           = sizeof(FMOD_CREATESOUNDEXINFO);
                    exinfo.length           = sample_length;
                    exinfo.numchannels      = 1;
                    exinfo.defaultfrequency = 44100;
                    exinfo.format           = sample_format;

                    #ifdef FMOD_SUPPORT_HARDWAREXM
                    if (usermode & FMOD_HARDWARE)
                    {
                        sample_mode &= ~FMOD_SOFTWARE;
                        sample_mode |=  FMOD_HARDWARE;

                        #if defined(PLATFORM_PS2) || defined(PLATFORM_PSP)
                        exinfo.format = FMOD_SOUND_FORMAT_VAG;

                        if (sample_mode & FMOD_LOOP_BIDI)
                        {
                            exinfo.length <<= 1;    /* fake bidi by doubling length and flipping 2nd half. */
                        }

                        if (sample_format == FMOD_SOUND_FORMAT_PCM16)
                        {
                            exinfo.length >>= 1;
                        }

                        if (exinfo.length < 28)
                        {
                            exinfo.length = 28;
                        }

                        exinfo.length /= 28;
                        exinfo.length *= 16;
                        
                        exinfo.length += 32;    /* Extra few rows for end? */
                        #endif
                    }
                    #endif

                    result = mSystem->createSound(0, sample_mode | FMOD_OPENUSER, &exinfo, &sptr->mSound);
                    if (result != FMOD_OK)
                    {
                        return result;
                    }

                    if (sample_mode & FMOD_LOOP_NORMAL || sample_mode & FMOD_LOOP_BIDI)
                    {
                        result = sptr->mSound->setLoopPoints(sptr->mLoopStart, FMOD_TIMEUNIT_PCM, sptr->mLoopStart + sptr->mLoopLength - 1, FMOD_TIMEUNIT_PCM);
                        if (result != FMOD_OK)
                        {
//                            return result;
                        }
                    }
		        }
			}
			
			/*
                Clear out the rest of the samples
            */
			for (;count2<16; count2++) 
            {
                iptr->mSample[count2].mSound = 0;
            }

			/*
                Load sample data
            */
			for (count2 = 0; count2 < numsamples; count2++) 
			{
				MusicSample        *sptr = &iptr->mSample[count2];
				unsigned int        lenbytes = 0;
                int                 channels;
                FMOD_SOUND_FORMAT   format;
                
                if (sptr->mSound)
                {
                    lenbytes = sptr->mRawLength;
                    result = sptr->mSound->getFormat(0, &format, &channels, 0);
                }

				if (lenbytes)
				{
                    #ifdef FMOD_SUPPORT_HARDWAREXM
                    if (usermode & FMOD_HARDWARE && sptr->mSound->mMode & FMOD_LOOP_BIDI && (lenbytes * 2) > largestsample)
                    {
                        buff = (unsigned char *)FMOD_Memory_ReAlloc(buff, (lenbytes * 2) + 16 + 56);
                        if (!buff) 
						{
							return FMOD_ERR_MEMORY;
						}
						largestsample = lenbytes * 2;
					}
                    else
                    #endif
                    if (lenbytes > largestsample)
                    {
						buff = (unsigned char *)FMOD_Memory_ReAlloc(buff, lenbytes + 16);
                        if (!buff) 
						{
							return FMOD_ERR_MEMORY;
						}
						largestsample = lenbytes;
                    }

					result = mFile->read(buff, 1, lenbytes, 0);
                    if (result != FMOD_OK && result != FMOD_ERR_FILE_EOF)
                    {
                        return result;
                    }

                    if (!FMOD_strncmp((char *)buff + 4, "OggS", 4))
                    {
                        FMOD_MODE              mode;
                        FMOD_CREATESOUNDEXINFO exinfo;

                        FMOD_memset(&exinfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));            
                        exinfo.cbsize           = sizeof(FMOD_CREATESOUNDEXINFO);
                        exinfo.length           = lenbytes;
                        exinfo.numchannels      = sptr->mSound->mChannels;
                        exinfo.defaultfrequency = (int)sptr->mSound->mDefaultFrequency;
                        exinfo.format           = sptr->mSound->mFormat;

                        mode     = sptr->mSound->mMode;

                        sptr->mSound->release();
                        sptr->mSound = 0;

                        mode &= ~FMOD_OPENUSER;
                        mode |=  FMOD_OPENMEMORY;

                        result = mSystem->createSound((char *)buff + 4, mode, &exinfo, &sptr->mSound);
                        if (result == FMOD_OK)
                        {
                            if (mode & FMOD_LOOP_NORMAL || mode & FMOD_LOOP_BIDI)
                            {
                                result = sptr->mSound->setLoopPoints(sptr->mLoopStart, FMOD_TIMEUNIT_PCM, sptr->mLoopStart + sptr->mLoopLength - 1, FMOD_TIMEUNIT_PCM);
                                if (result != FMOD_OK)
                                {
                                    return result;
                                }
                            }
                        }
                    }
                    else
                    {
                        void *ptr1, *ptr2;
                        unsigned int len1, len2;

                        #ifdef PLATFORM_ENDIAN_BIG
    	                if (format == FMOD_SOUND_FORMAT_PCM16)
                        {
			                unsigned int count3;
			                signed short *wptr = (signed short *)buff;

			                for (count3=0; count3 < lenbytes >> 1; count3++) 
                            {
				                wptr[count3] = FMOD_SWAPENDIAN_WORD(wptr[count3]);
                            }
                        }
                        #endif

                        result = sptr->mSound->lock(0, lenbytes, &ptr1, &ptr2, &len1, &len2);
                        if (result != FMOD_OK)
                        {
                            return result;
                        }

                        if (ptr1 && len1)
                        {
		                    int oldval, newval;
		                    unsigned int count3;

                            FMOD_memcpy(ptr1, buff, lenbytes);

		                    oldval = 0;
		                    if (format == FMOD_SOUND_FORMAT_PCM8)
		                    {
    		                    unsigned char *bptr = (unsigned char *)ptr1;
			                    for (count3 = 0; count3 < lenbytes; count3++) 
			                    {
				                    newval = *bptr + oldval;
				                    *bptr = (signed char)newval;
				                    oldval = newval;
				                    bptr++;
			                    }
		                    }
		                    else if (format == FMOD_SOUND_FORMAT_PCM16)
		                    {
    		                    unsigned short	*wptr = (unsigned short *)ptr1;
			                    for (count3 = 0; count3 < lenbytes>>1; count3++) 
			                    {
				                    newval = *wptr + oldval;
				                    *wptr = (short)newval;
				                    oldval = newval;
				                    wptr++;
			                    }
		                    }
                        }

                        result = sptr->mSound->unlock(ptr1, ptr2, len1, len2);
                        if (result != FMOD_OK)
                        {
                            return result;
                        }
                    }
                }
			}

			if (buff) 
            {
                FMOD_Memory_Free(buff);
            }
		}
		else 
		{
			/*
                Clear out the rest of the samples
            */
			for (count2=0; count2<16; count2++) 
            {
				iptr->mSample[count2].mSound = 0;
            }

			result = mFile->seek(firstsampleoffset, SEEK_SET);
            if (result != FMOD_OK)
            {
                return result;
            }
		}
	}

	/* 
        Alloc sample array 
    */
	mSample = 0;
	if (mNumSamples)
	{
		mSample = (MusicSample **)FMOD_Memory_Calloc(mNumSamples * sizeof(MusicSample *));
		if (!mSample) 
		{
			return FMOD_ERR_MEMORY;
		}
	}

	/*
        Set up the linear sample array by scanning instrument list and copying sample pointers into the list incrementally
    */
	{
		int numsamples=0,count2;
		
		for (count=0; count < mNumInstruments; count++) 
		{
			MusicInstrument	*iptr = &mInstrument[count];

			for (count2=0; count2 < iptr->mNumSamples; count2++) 
			{
				MusicSample *sptr = &iptr->mSample[count2];

				mSample[numsamples] = sptr;
				numsamples ++;
			}
		}
	}

    mWaveFormatMemory = (FMOD_CODEC_WAVEFORMAT *)FMOD_Memory_Calloc(sizeof(FMOD_CODEC_WAVEFORMAT));
    if (!mWaveFormatMemory)
    {
        return FMOD_ERR_MEMORY;
    }

    waveformat = mWaveFormatMemory;
    waveformat[0].lengthbytes = lengthbytes;

    /*
        Set up general codec parameters.
    */
    if (userexinfo && userexinfo->format != FMOD_SOUND_FORMAT_NONE)
    {
        waveformat[0].format = userexinfo->format;
    }
    #ifdef FMOD_SUPPORT_HARDWAREXM
    if (usermode & FMOD_HARDWARE)
    {
        waveformat[0].format      = FMOD_SOUND_FORMAT_NONE;
        waveformat[0].blockalign  = 0;
        waveformat[0].channels    = 0;
        mFlags |= FMOD_CODEC_HARDWAREMUSICVOICES;
    }
    #endif
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
    FMOD_strncpy(waveformat[0].name, mSongName, FMOD_STRING_MAXNAMELEN);

    result = mSystem->getSoftwareFormat(&waveformat[0].frequency, 0, 0, 0, 0, 0);
    if (result != FMOD_OK)
    {
        return result;
    }

	mSrcDataOffset  = 0;

    SoundI::getBytesFromSamples(1, (unsigned int *)&waveformat[0].blockalign, waveformat[0].channels, waveformat[0].format);

    /*
        Create a head unit that software channels connect to.
    */
    #ifdef FMOD_SUPPORT_HARDWAREXM
    if (!(usermode & FMOD_HARDWARE))
    #endif
    {
        FMOD_DSP_DESCRIPTION_EX descriptionex;
        
        FMOD_memset(&descriptionex, 0, sizeof(FMOD_DSP_DESCRIPTION_EX));
        FMOD_strcpy(descriptionex.name, "FMOD XM Target Unit");
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

    /*
        Create a pool of virtual channels.
    */
    {
        mNumVirtualChannels = mNumChannels;
        mVirtualChannel = (MusicVirtualChannel *)FMOD_Memory_Calloc(sizeof(MusicVirtualChannel) * mNumVirtualChannels);
        if (!mVirtualChannel)
        {
            return FMOD_ERR_MEMORY;
        }
        for (count = 0; count < mNumVirtualChannels; count++)
        {
            new (&mVirtualChannel[count]) MusicVirtualChannel;
        }
    }

    /*
        Create a pool of real channels.  2x the number of virtual channels for double buffer channel usage.
    */
    #ifdef FMOD_SUPPORT_HARDWAREXM
    if (usermode & FMOD_HARDWARE)
    {
        mChannelPool = mSystem->mOutput->mChannelPool;
    }
    else
    #endif
    {
        int numrealchannels = mNumVirtualChannels * 2;

        mChannelPool = FMOD_Object_Calloc(ChannelPool);
        if (!mChannelPool)
        {
            return FMOD_ERR_MEMORY;
        }

        result = mChannelPool->init(mSystem, 0, numrealchannels);
        if (result != FMOD_OK)
        {
            return result;
        }

        mChannelSoftware = (ChannelSoftware *)FMOD_Memory_Calloc(sizeof(ChannelSoftware) * numrealchannels);
        if (!mChannelSoftware)
        {
            return FMOD_ERR_MEMORY;
        }

        for (count = 0; count < numrealchannels; count++)
        {
            new (&mChannelSoftware[count]) ChannelSoftware;
            mChannelPool->setChannel(count, &mChannelSoftware[count], mDSPHead);
            mChannelSoftware[count].allowReverb(false);
        }
    }

    if ((usermode & FMOD_ACCURATETIME) || (usermode & FMOD_CREATESAMPLE))
    {
        mVisited = (bool *)FMOD_Memory_Calloc(sizeof(bool) * mNumOrders * FMUSIC_MAXROWS);
        if (!mVisited)
        {
            return FMOD_ERR_MEMORY;
        }

        calculateLength();
    }
    else
    {
        mVisited = 0;
        waveformat[0].lengthpcm = (unsigned int)-1;
    }

    numsubsounds  = 0;

    play();

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
// VERIFY: c348c0e8 auto
// OFFSET: 0x00905B00
FMOD_RESULT CodecXM::closeInternal()
{
    int count;

    stop();

    if (mSample)
    {
	    for (count = 0; count < mNumSamples; count++)
	    {
            if (mSample[count] && mSample[count]->mSound)
            {
                mSample[count]->mSound->release();
                mSample[count]->mSound = 0;
                mSample[count] = 0;
            }
        }

        FMOD_Memory_Free(mSample);
        mSample = 0;
    }

    if (mInstrument)
    {
        FMOD_Memory_Free(mInstrument);
        mInstrument = 0;
    }

    if (mVirtualChannel)
    {
        FMOD_Memory_Free(mVirtualChannel);
        mVirtualChannel = 0;
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

    if (mPattern)
    {
	    for (count = 0; count < mNumPatterns; count++)
	    {
            if (mPattern[count].mData)
            {
		        FMOD_Memory_Free(mPattern[count].mData);
                mPattern[count].mData = 0;
            }
        }

        FMOD_Memory_Free(mPattern);
        mPattern = 0;
    }

	for (count = 0; count < mNumChannels; count++)
	{
        if (mMusicChannel[count])
        {
		    FMOD_Memory_Free(mMusicChannel[count]);
            mMusicChannel[count] = 0;
        }
    }

    if (mVisited)
    {
        FMOD_Memory_Free(mVisited);
        mVisited = 0;
    }

    if (mDSPHead)
    {
        mDSPHead->release();
        mDSPHead = 0;
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
// VERIFY: 7dc697d6 manual
// OFFSET: 0x00905E80
FMOD_RESULT CodecXM::readInternal(void *buffer, unsigned int sizebytes, unsigned int *bytesread)
{
    FMOD_RESULT              result = FMOD_OK;
    unsigned int             numsamples;
    int                      numchannels;
    FMOD_OS_CRITICALSECTION *crit = mSystem->mDSPCrit;

    numchannels = waveformat[0].channels;

    SoundI::getSamplesFromBytes(sizebytes, &numsamples, numchannels, waveformat[0].format);

	if (mPlaying && mMasterSpeed)
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

            FMOD_OS_CriticalSection_Enter(crit);
            if (buffer)
			{
                result = mDSPHead->execute(outbuffer, &outbuffer, &read, numchannels, &numchannels, FMOD_SPEAKERMODE_STEREO_LINEAR);
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
// VERIFY: 4bffeec6 auto
// OFFSET: 0x00905D30
FMOD_RESULT CodecXM::setPositionInternal(int subsound, unsigned int position, FMOD_TIMEUNIT postype)
{
    if (postype == FMOD_TIMEUNIT_MODORDER)
    {
        play();
        mNextOrder = mOrder = position;

        return FMOD_OK;
    }
    else if (postype == FMOD_TIMEUNIT_PCM)
    {
        bool restarted = false;

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
            restarted = true;
        }

        while (mPCMOffset < position)
        {
            update(true);
        }

        if (restarted)
        {
            bool oldplaying = mPlaying;
            bool oldfinished = mFinished;
        
            stop();

	        mPlaying = oldplaying;
            mFinished = oldfinished;
        }

        return FMOD_OK;
    }

    return FMOD_ERR_FORMAT;
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
// VERIFY: f1fd6caf manual
// OFFSET: 0x009075B0
FMOD_RESULT F_CALLBACK CodecXM::openCallback(FMOD_CODEC_STATE *codec, FMOD_MODE usermode, FMOD_CREATESOUNDEXINFO *userexinfo)
{
    CodecXM *cxm = (CodecXM *)codec;

    return cxm->openInternal(usermode, userexinfo);
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
// VERIFY: 217634dd manual
// OFFSET: 0x00905DD0
FMOD_RESULT F_CALLBACK CodecXM::closeCallback(FMOD_CODEC_STATE *codec)
{
    CodecXM *cxm = (CodecXM *)codec;

    return cxm->closeInternal();
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
// VERIFY: d83b213b auto
// OFFSET: 0x00906240
FMOD_RESULT F_CALLBACK CodecXM::readCallback(FMOD_CODEC_STATE *codec, void *buffer, unsigned int sizebytes, unsigned int *bytesread)
{
    CodecXM *cxm = (CodecXM *)codec;

    return cxm->readInternal(buffer, sizebytes, bytesread);
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
// VERIFY: 5ed1ed98 auto
// OFFSET: 0x00905E00
FMOD_RESULT F_CALLBACK CodecXM::setPositionCallback(FMOD_CODEC_STATE *codec, int subsound, unsigned int position, FMOD_TIMEUNIT postype   )
{
    CodecXM *cxm = (CodecXM *)codec;

    return cxm->setPositionInternal(subsound, position, postype);
}


}

#endif


