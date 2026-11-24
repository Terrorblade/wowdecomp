#include "fmod_settings.h"

#ifdef FMOD_SUPPORT_MPEG

#include "fmod_codec_mpeg.h"

#include <stdlib.h>
#include <math.h>

#ifdef PLATFORM_PS3_SPU
#include "fmod_common_spu.h"
#include "fmod_spu_printf.h"
#endif

namespace FMOD
{

int CodecMPEG::gTabSel123[2][3][16] = 
{
	{ 
		{0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,},
		{0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,},
		{0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,} 
	},
	{ 
		{0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,},
		{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,},
		{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,} 
	}
};

int CodecMPEG::gFreqs[9] = 
{ 
	44100, 
	48000, 
	32000,
    22050, 
	24000, 
	16000,
	11025,
	12000,
	8000 
};

#ifndef FMOD_SUPPORT_MPEG_SONYDECODER

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 45df0b72 auto
// OFFSET: 0x0092C240
unsigned int CodecMPEG::getBits(int number_of_bits)
{
	unsigned int rval;

	if(!number_of_bits)
		return 0;
	
	rval = mMemoryBlock->mBSI.mWordPointer[0];
	rval <<= 8;
	rval |= mMemoryBlock->mBSI.mWordPointer[1];
	rval <<= 8;
	rval |= mMemoryBlock->mBSI.mWordPointer[2];
	rval <<= mMemoryBlock->mBSI.mBitIndex;
	rval &= 0xffffff;

	mMemoryBlock->mBSI.mBitIndex += number_of_bits;

	rval >>= (24-number_of_bits);

	mMemoryBlock->mBSI.mWordPointer += (mMemoryBlock->mBSI.mBitIndex>>3);
	mMemoryBlock->mBSI.mBitIndex &= 7;
	
	return rval;
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
// VERIFY: daab91e7 manual
// OFFSET: 0x0092C2C0
unsigned int CodecMPEG::getBitsFast(int number_of_bits)
{
	unsigned int rval;

	rval   = mMemoryBlock->mBSI.mWordPointer[0];
	rval <<= 8;	
	rval  |= mMemoryBlock->mBSI.mWordPointer[1];
	rval <<= mMemoryBlock->mBSI.mBitIndex;
	rval  &= 0xffff;

	mMemoryBlock->mBSI.mBitIndex += number_of_bits;

	rval >>= (16-number_of_bits);

	mMemoryBlock->mBSI.mWordPointer += (mMemoryBlock->mBSI.mBitIndex>>3);
	mMemoryBlock->mBSI.mBitIndex &= 7;
	
	return rval;
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
// VERIFY: 79dd0f5f auto
// OFFSET: 0x0092C330
FMOD_RESULT CodecMPEG::decodeHeader(void *in, int *samplerate, int *channels, int *framesize)
{
#ifdef FMOD_SUPPORT_MPEG_SONYDECODER

	unsigned int head = 0;

	head = (unsigned char)*((unsigned char*)in + 0);
	head <<= 8;
	head |= (unsigned char)*((unsigned char*)in + 1);
	head <<= 8;
	head |= (unsigned char)*((unsigned char*)in + 2);
	head <<= 8;
	head |= (unsigned char)*((unsigned char*)in + 3);

	if ((head & 0xFFE00000) != 0xFFE00000)
    {
		return FMOD_ERR_FORMAT;
    }

    int lsf;
    int mpeg25;
    int lay;
    int sampling_frequency;
    int bitrate_index;
    int mode;
    int mode_ext;
    int padding;
    int jsbound;
    int _framesize;
    int II_sblimit;

	if( head & (1<<20) ) 
	{
		lsf = (head & (1<<19)) ? 0x0 : 0x1;
		mpeg25 = 0;
	}
    else 
    {
        if (head & (1<<19))
        {
            return FMOD_ERR_FORMAT;
        }

        lsf = 1;
        mpeg25 = 1;
    }
    
    lay = 4-((head>>17)&3);

	if (lay != 3)
    {
        /*
            Sony MP3 decoder only supports layer 3
        */
        if ((mFlags & FMOD_CODEC_FROMFSB) && lay == 2)
        {
		    return FMOD_ERR_FORMAT;
        }
    }

	//if (!mMemoryBlock->mLayer)
    {
		mMemoryBlock->mLayer = lay;
    }

    /*
	if (lay != mMemoryBlock->mLayer)
    {
		return FMOD_ERR_FORMAT;
    }
    */

    if( ((head>>10)&0x3) == 0x3) 
    {
		return FMOD_ERR_FORMAT;
    }

    if (mpeg25) 
    {
		sampling_frequency = 6 + ((head>>10)&0x3);
    }
    else
    {
		sampling_frequency = ((head>>10)&0x3) + (lsf*3);
    }

	if (samplerate)
    {
		*samplerate = gFreqs[sampling_frequency];
    }
#if 0
    /* This is a sort of corruption check but it can cause problems if the fsb has its frequency set to something other than the encoded freq. */

//    else if (gFreqs[mMemoryBlock->mFrame.sampling_frequency] != waveformat[0].frequency)
//    {
//        return FMOD_ERR_FORMAT;
//    }
#endif

	bitrate_index           = ((head>>12)&0xf);
    padding                 = ((head>>9)&0x1);
    mode                    = ((head>>6)&0x3);
    mode_ext                = ((head>>4)&0x3);
    mMemoryBlock->mStereo   = (mode == MPG_MD_MONO) ? 1 : 2;

    if (mode_ext == 1 || mode_ext == 3)
    {
        /*
            Sony decoder doesn't support Intensity Stereo
        */
        return FMOD_ERR_FORMAT;
    }

    if (!bitrate_index || bitrate_index == 0xF)
    {
        return FMOD_ERR_FORMAT;
    }

	if (channels)
    {
		*channels = mMemoryBlock->mStereo;
    }
    else if (mMemoryBlock->mStereo != waveformat[0].channels && !mChannels)
    {
        return FMOD_ERR_FORMAT;
    }

	_framesize  = (int)gTabSel123[lsf][2][bitrate_index] * 144000;
	_framesize /= gFreqs[sampling_frequency]<<(lsf);
	_framesize = _framesize + padding - 4;

	if (_framesize < 16)
    {
		return FMOD_ERR_FORMAT;
    }

//    mMemoryBlock->mFrameSize = mMemoryBlock->mFrame.framesize;

    if (framesize)
    {
	    *framesize = _framesize;

        if (mFlags & FMOD_CODEC_FROMFSB)
        {
            if (waveformat && waveformat->channels > 2)
            {
                *framesize += 4;    /* Temporarily add in header */
                *framesize += 15;
                *framesize &= ~15;
                *framesize -= 4;    /* Remove header size */
            }
            else if (lay == 3 && mFlags & FMOD_CODEC_PADDED)
            {
                *framesize += 4;    /* Temporarily add in header */
                *framesize += 1;
                *framesize &= ~1;
                *framesize -= 4;    /* Remove header size */
            }
            else if (lay == 3 && mFlags & FMOD_CODEC_PADDED4)
            {
                *framesize += 4;    /* Temporarily add in header */
                *framesize += 3;
                *framesize &= ~3;
                *framesize -= 4;    /* Remove header size */
            }
        }
    }

#else

	unsigned int head = 0;

	head = (unsigned char)*((unsigned char*)in + 0);
	head <<= 8;
	head |= (unsigned char)*((unsigned char*)in + 1);
	head <<= 8;
	head |= (unsigned char)*((unsigned char*)in + 2);
	head <<= 8;
	head |= (unsigned char)*((unsigned char*)in + 3);

	mMemoryBlock->mFrameHeader = head;

	if ((head & 0xFF000000) != 0xFF000000 || ((head >> 16) & 0xE0) != 0xE0)
    {
		return FMOD_ERR_FORMAT;
    }

	if( head & (1<<20) ) 
	{
		mMemoryBlock->mFrame.lsf = (head & (1<<19)) ? 0x0 : 0x1;
		mMemoryBlock->mFrame.mpeg25 = 0;
	}
    else 
    {
        mMemoryBlock->mFrame.lsf = 1;
        mMemoryBlock->mFrame.mpeg25 = 1;
    }
    
    mMemoryBlock->mFrame.lay = 4-((head>>17)&3);

	if (mMemoryBlock->mFrame.lay != 3	&& mMemoryBlock->mFrame.lay != 2)
    {
		return FMOD_ERR_FORMAT;
    }

	if (!mMemoryBlock->mLayer)
    {
		mMemoryBlock->mLayer = mMemoryBlock->mFrame.lay;
    }

	if (mMemoryBlock->mFrame.lay != mMemoryBlock->mLayer)
    {
		return FMOD_ERR_FORMAT;
    }

    if( ((head>>10)&0x3) == 0x3) 
    {
		return FMOD_ERR_FORMAT;
    }

    if (mMemoryBlock->mFrame.mpeg25) 
    {
		mMemoryBlock->mFrame.sampling_frequency = 6 + ((head>>10)&0x3);
    }
    else
    {
		mMemoryBlock->mFrame.sampling_frequency = ((head>>10)&0x3) + (mMemoryBlock->mFrame.lsf*3);
    }

	if (samplerate)
    {
		*samplerate = gFreqs[mMemoryBlock->mFrame.sampling_frequency];
    }
#if 0
    /* This is a sort of corruption check but it can cause problems if the fsb has its frequency set to something other than the encoded freq. */

//    else if (gFreqs[mMemoryBlock->mFrame.sampling_frequency] != waveformat[0].frequency)
//    {
//        return FMOD_ERR_FORMAT;
//    }
#endif

    mMemoryBlock->mFrame.error_protection = ((head>>16)&0x1)^0x1;
	mMemoryBlock->mFrame.bitrate_index    = ((head>>12)&0xf);
    mMemoryBlock->mFrame.padding          = ((head>>9)&0x1);
    mMemoryBlock->mFrame.extension        = ((head>>8)&0x1);
    mMemoryBlock->mFrame.mode             = ((head>>6)&0x3);
    mMemoryBlock->mFrame.mode_ext         = ((head>>4)&0x3);
    mMemoryBlock->mFrame.copyright        = ((head>>3)&0x1);
    mMemoryBlock->mFrame.original         = ((head>>2)&0x1);
    mMemoryBlock->mFrame.emphasis         = head & 0x3;
    mMemoryBlock->mFrame.stereo           = (mMemoryBlock->mFrame.mode == MPG_MD_MONO) ? 1 : 2;

	if (channels)
    {
		*channels = mMemoryBlock->mFrame.stereo;
    }
    else if (mMemoryBlock->mFrame.stereo != waveformat[0].channels && !mChannels)
    {
        return FMOD_ERR_FORMAT;
    }

    if (!mMemoryBlock->mFrame.bitrate_index)
    {
        return FMOD_ERR_FORMAT;
    }


	
/*	printf("2.5? %d , bitrate index = %d\n", mMemoryBlock->mFrame.mpeg25, mMemoryBlock->mFrame.bitrate_index); */

    switch (mMemoryBlock->mFrame.lay)
    {
        #ifdef FMOD_SUPPORT_MPEG_LAYER2
		case 2:
		{
			getIIStuff();
			mMemoryBlock->mFrame.jsbound = (mMemoryBlock->mFrame.mode == MPG_MD_JOINT_STEREO) ? (mMemoryBlock->mFrame.mode_ext<<2)+4 : mMemoryBlock->mFrame.II_sblimit;
			mMemoryBlock->mFrame.framesize = (int) gTabSel123[mMemoryBlock->mFrame.lsf][1][mMemoryBlock->mFrame.bitrate_index] * 144000;
			mMemoryBlock->mFrame.framesize /= gFreqs[mMemoryBlock->mFrame.sampling_frequency];
			mMemoryBlock->mFrame.framesize += mMemoryBlock->mFrame.padding - 4;
			break;
		}
        #endif
        #ifdef FMOD_SUPPORT_MPEG_LAYER3
		case 3:
		{
			mMemoryBlock->mFrame.framesize  = (int)gTabSel123[mMemoryBlock->mFrame.lsf][2][mMemoryBlock->mFrame.bitrate_index] * 144000;
			mMemoryBlock->mFrame.framesize /= gFreqs[mMemoryBlock->mFrame.sampling_frequency]<<(mMemoryBlock->mFrame.lsf);
			mMemoryBlock->mFrame.framesize = mMemoryBlock->mFrame.framesize + mMemoryBlock->mFrame.padding - 4;
			break; 
		}
        #endif
		default:
        {
			return FMOD_ERR_UNSUPPORTED;
        }
	};

	if (mMemoryBlock->mFrame.framesize < 16)
    {
		return FMOD_ERR_FORMAT;
    }

    mMemoryBlock->mFrameSize = mMemoryBlock->mFrame.framesize;

    if (framesize)
    {
	    *framesize = mMemoryBlock->mFrame.framesize;

        if (waveformat && waveformat->channels > 2)
        {
            *framesize += 4;    /* Temporarily add in header */
            *framesize += 15;
            *framesize &= ~15;
            *framesize -= 4;    /* Remove header size */
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

	[SEE_ALSO]
]
*/
// VERIFY: 05b0031d manual
// OFFSET: 0x0092C670
FMOD_RESULT CodecMPEG::decodeXingHeader(unsigned char *in, unsigned char *toc, unsigned int *numframes)
{
    int i, flags = 0;
    int id, mode;

    id         = (in[1] >> 3) & 1;
    mode       = (in[3] >> 6) & 3;

    /*
        Get offset of Xing header
    */
    if (id)  // mpeg1
    {        
        if (mode != 3)
        {
            in+=(32+4);
        }
        else
        {
            in+=(17+4);
        }
    }
    else        // mpeg2
    {
        if (mode != 3)
        {
            in+=(17+4);
        }
        else
        {
            in+=(9+4);
        }
    }

    /*
        Check if this is a Xing header
    */
    if (FMOD_strncmp((const char *)in, "Xing", 4))
    {
        return FMOD_ERR_FORMAT;
    }
    in+=4;

    flags =  *in++;
    flags <<= 8;
    flags |= *in++;
    flags <<= 8;
    flags |= *in++;
    flags <<= 8;
    flags |= *in++;

    if (flags & 0x0001 /*FRAMES_FLAG*/)
    {   
        if (numframes)
        {
            *numframes =  *in++;
            *numframes <<= 8;
            *numframes |= *in++;
            *numframes <<= 8;
            *numframes |= *in++;
            *numframes <<= 8;
            *numframes |= *in++;
        }

        mMemoryBlock->mHasXingNumFrames = true;
    }
    if (flags & 0x0004 /*TOC_FLAG*/)
    {
        if (toc)
        {
            for (i=0; i<100; i++)
            {
                toc[i] = *in++;
            }
        }

        mMemoryBlock->mHasXingToc = true;
    }

    return FMOD_OK;
}


/*
[
	[DESCRIPTION]
	This just unpacks one frame

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 908447f4 manual
// OFFSET: 0x0092C780
FMOD_RESULT CodecMPEG::decodeFrame(unsigned char *in, void *out, unsigned int *outlen)
{
    FMOD_RESULT result = FMOD_OK;

#ifndef FMOD_SUPPORT_MPEG_SONYDECODER
	/*
        First decode frame header
    */
	if (mMemoryBlock->mFrameSize == 0) 
	{
        result = decodeHeader(in, 0, 0, 0);
		if (result != FMOD_OK)
        {
			return result;
        }
	}

	mMemoryBlock->mBSI.mWordPointer = mMemoryBlock->mBSSpace[mMemoryBlock->mBSNum] + 512;
	mMemoryBlock->mBSNum            = (mMemoryBlock->mBSNum + 1) & 0x1;
	mMemoryBlock->mBSI.mBitIndex    = 0;

	FMOD_memcpy(mMemoryBlock->mBSI.mWordPointer, in+4, mMemoryBlock->mFrameSize);

	if(mMemoryBlock->mFrame.error_protection)
    {
		getBits(16);
    }

	switch (mMemoryBlock->mFrame.lay)
	{	
        #ifdef FMOD_SUPPORT_MPEG_LAYER2
		case 2: 
		{
            #ifdef PLATFORM_PS3_SPU
            /*
                Because this is called on the PPU in decodeHeader,
                we need to call it on the SPU to set up some pointers.
            */
            getIIStuff();
            #endif

			result = decodeLayer2(out, outlen);
			break;
		}
        #endif
        #ifdef FMOD_SUPPORT_MPEG_LAYER3
		case 3: 
		{
			result = decodeLayer3(out, outlen);
			break;
		}
        #endif
	};
  
    mMemoryBlock->mFrameSizeOld = mMemoryBlock->mFrameSize;
    mMemoryBlock->mFrameSize = 0;

#else
    result = decodeLayer3(in, out, outlen);
#endif

	return result;
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
// VERIFY: 72108d06 auto
// OFFSET: 0x0092C880
FMOD_RESULT CodecMPEG::resetFrame()
{
    if (mMemoryBlock)
    {
        int count;

        for (count = 0; count < (mChannels ? mChannels : 1); count++)
        {
            unsigned int *oldframeoffset = mMemoryBlock[count].mFrameOffset;
            unsigned int oldnumframes    = mMemoryBlock[count].mNumFrames;

            FMOD_memset(&mMemoryBlock[count], 0, sizeof(CodecMPEG_MemoryBlock));

            mMemoryBlock[count].mNumFrames    = oldnumframes;
            mMemoryBlock[count].mFrameOffset  = oldframeoffset;

            #ifndef FMOD_SUPPORT_MPEG_SONYDECODER

            mMemoryBlock[count].mSynthBo = 1;
            mMemoryBlock[count].mFrameSizeOld = -1;     

            #endif
        }
    }

    return FMOD_OK;
}

#ifndef FMOD_SUPPORT_MPEG_SONYDECODER
/*
[
	[DESCRIPTION]
	DCT Function

	[PARAMETERS]
	'out0'
	'out1'
	'b1'
	'b2'
	'samples'		Pointer to 16bit pcm output data 
 
	[RETURN_VALUE]
	void

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 485d574c auto
// OFFSET: 0x0092C930
void CodecMPEG::dct64(float *out0,float *out1,float *samples)
{
    float b1[32], b2[32];
	{
		float *costab = gPnts[0];

		b1[0x00] = samples[0x00] + samples[0x1F];
		b1[0x1F] = (samples[0x00] - samples[0x1F]) * costab[0x0];

		b1[0x01] = samples[0x01] + samples[0x1E];
		b1[0x1E] = (samples[0x01] - samples[0x1E]) * costab[0x1];

		b1[0x02] = samples[0x02] + samples[0x1D];
		b1[0x1D] = (samples[0x02] - samples[0x1D]) * costab[0x2];

		b1[0x03] = samples[0x03] + samples[0x1C];
		b1[0x1C] = (samples[0x03] - samples[0x1C]) * costab[0x3];

		b1[0x04] = samples[0x04] + samples[0x1B];
		b1[0x1B] = (samples[0x04] - samples[0x1B]) * costab[0x4];

		b1[0x05] = samples[0x05] + samples[0x1A];
		b1[0x1A] = (samples[0x05] - samples[0x1A]) * costab[0x5];

		b1[0x06] = samples[0x06] + samples[0x19];
		b1[0x19] = (samples[0x06] - samples[0x19]) * costab[0x6];

		b1[0x07] = samples[0x07] + samples[0x18];
		b1[0x18] = (samples[0x07] - samples[0x18]) * costab[0x7];

		b1[0x08] = samples[0x08] + samples[0x17];
		b1[0x17] = (samples[0x08] - samples[0x17]) * costab[0x8];

		b1[0x09] = samples[0x09] + samples[0x16];
		b1[0x16] = (samples[0x09] - samples[0x16]) * costab[0x9];

		b1[0x0A] = samples[0x0A] + samples[0x15];
		b1[0x15] = (samples[0x0A] - samples[0x15]) * costab[0xA];

		b1[0x0B] = samples[0x0B] + samples[0x14];
		b1[0x14] = (samples[0x0B] - samples[0x14]) * costab[0xB];

		b1[0x0C] = samples[0x0C] + samples[0x13];
		b1[0x13] = (samples[0x0C] - samples[0x13]) * costab[0xC];

		b1[0x0D] = samples[0x0D] + samples[0x12];
		b1[0x12] = (samples[0x0D] - samples[0x12]) * costab[0xD];

		b1[0x0E] = samples[0x0E] + samples[0x11];
		b1[0x11] = (samples[0x0E] - samples[0x11]) * costab[0xE];

		b1[0x0F] = samples[0x0F] + samples[0x10];
		b1[0x10] = (samples[0x0F] - samples[0x10]) * costab[0xF];
	}
	{
		float *costab = gPnts[1];

		b2[0x00] = b1[0x00] + b1[0x0F]; 
		b2[0x0F] = (b1[0x00] - b1[0x0F]) * costab[0];
		b2[0x01] = b1[0x01] + b1[0x0E]; 
		b2[0x0E] = (b1[0x01] - b1[0x0E]) * costab[1];
		b2[0x02] = b1[0x02] + b1[0x0D]; 
		b2[0x0D] = (b1[0x02] - b1[0x0D]) * costab[2];
		b2[0x03] = b1[0x03] + b1[0x0C]; 
		b2[0x0C] = (b1[0x03] - b1[0x0C]) * costab[3];
		b2[0x04] = b1[0x04] + b1[0x0B]; 
		b2[0x0B] = (b1[0x04] - b1[0x0B]) * costab[4];
		b2[0x05] = b1[0x05] + b1[0x0A]; 
		b2[0x0A] = (b1[0x05] - b1[0x0A]) * costab[5];
		b2[0x06] = b1[0x06] + b1[0x09]; 
		b2[0x09] = (b1[0x06] - b1[0x09]) * costab[6];
		b2[0x07] = b1[0x07] + b1[0x08]; 
		b2[0x08] = (b1[0x07] - b1[0x08]) * costab[7];

		b2[0x10] = b1[0x10] + b1[0x1F];
		b2[0x1F] = (b1[0x1F] - b1[0x10]) * costab[0];
		b2[0x11] = b1[0x11] + b1[0x1E];
		b2[0x1E] = (b1[0x1E] - b1[0x11]) * costab[1];
		b2[0x12] = b1[0x12] + b1[0x1D];
		b2[0x1D] = (b1[0x1D] - b1[0x12]) * costab[2];
		b2[0x13] = b1[0x13] + b1[0x1C];
		b2[0x1C] = (b1[0x1C] - b1[0x13]) * costab[3];
		b2[0x14] = b1[0x14] + b1[0x1B];
		b2[0x1B] = (b1[0x1B] - b1[0x14]) * costab[4];
		b2[0x15] = b1[0x15] + b1[0x1A];
		b2[0x1A] = (b1[0x1A] - b1[0x15]) * costab[5];
		b2[0x16] = b1[0x16] + b1[0x19];
		b2[0x19] = (b1[0x19] - b1[0x16]) * costab[6];
		b2[0x17] = b1[0x17] + b1[0x18];
		b2[0x18] = (b1[0x18] - b1[0x17]) * costab[7];
	}

	{
		float *costab = gPnts[2];

		b1[0x00] = b2[0x00] + b2[0x07];
		b1[0x07] = (b2[0x00] - b2[0x07]) * costab[0];
		b1[0x01] = b2[0x01] + b2[0x06];
		b1[0x06] = (b2[0x01] - b2[0x06]) * costab[1];
		b1[0x02] = b2[0x02] + b2[0x05];
		b1[0x05] = (b2[0x02] - b2[0x05]) * costab[2];
		b1[0x03] = b2[0x03] + b2[0x04];
		b1[0x04] = (b2[0x03] - b2[0x04]) * costab[3];

		b1[0x08] = b2[0x08] + b2[0x0F];
		b1[0x0F] = (b2[0x0F] - b2[0x08]) * costab[0];
		b1[0x09] = b2[0x09] + b2[0x0E];
		b1[0x0E] = (b2[0x0E] - b2[0x09]) * costab[1];
		b1[0x0A] = b2[0x0A] + b2[0x0D];
		b1[0x0D] = (b2[0x0D] - b2[0x0A]) * costab[2];
		b1[0x0B] = b2[0x0B] + b2[0x0C];
		b1[0x0C] = (b2[0x0C] - b2[0x0B]) * costab[3];

		b1[0x10] = b2[0x10] + b2[0x17];
		b1[0x17] = (b2[0x10] - b2[0x17]) * costab[0];
		b1[0x11] = b2[0x11] + b2[0x16];
		b1[0x16] = (b2[0x11] - b2[0x16]) * costab[1];
		b1[0x12] = b2[0x12] + b2[0x15];
		b1[0x15] = (b2[0x12] - b2[0x15]) * costab[2];
		b1[0x13] = b2[0x13] + b2[0x14];
		b1[0x14] = (b2[0x13] - b2[0x14]) * costab[3];

		b1[0x18] = b2[0x18] + b2[0x1F];
		b1[0x1F] = (b2[0x1F] - b2[0x18]) * costab[0];
		b1[0x19] = b2[0x19] + b2[0x1E];
		b1[0x1E] = (b2[0x1E] - b2[0x19]) * costab[1];
		b1[0x1A] = b2[0x1A] + b2[0x1D];
		b1[0x1D] = (b2[0x1D] - b2[0x1A]) * costab[2];
		b1[0x1B] = b2[0x1B] + b2[0x1C];
		b1[0x1C] = (b2[0x1C] - b2[0x1B]) * costab[3];
	}

	{
		float const cos0 = gPnts[3][0];
		float const cos1 = gPnts[3][1];

		b2[0x00] = b1[0x00] + b1[0x03];
		b2[0x03] = (b1[0x00] - b1[0x03]) * cos0;
		b2[0x01] = b1[0x01] + b1[0x02];
		b2[0x02] = (b1[0x01] - b1[0x02]) * cos1;

		b2[0x04] = b1[0x04] + b1[0x07];
		b2[0x07] = (b1[0x07] - b1[0x04]) * cos0;
		b2[0x05] = b1[0x05] + b1[0x06];
		b2[0x06] = (b1[0x06] - b1[0x05]) * cos1;

		b2[0x08] = b1[0x08] + b1[0x0B];
		b2[0x0B] = (b1[0x08] - b1[0x0B]) * cos0;
		b2[0x09] = b1[0x09] + b1[0x0A];
		b2[0x0A] = (b1[0x09] - b1[0x0A]) * cos1;

		b2[0x0C] = b1[0x0C] + b1[0x0F];
		b2[0x0F] = (b1[0x0F] - b1[0x0C]) * cos0;
		b2[0x0D] = b1[0x0D] + b1[0x0E];
		b2[0x0E] = (b1[0x0E] - b1[0x0D]) * cos1;

		b2[0x10] = b1[0x10] + b1[0x13];
		b2[0x13] = (b1[0x10] - b1[0x13]) * cos0;
		b2[0x11] = b1[0x11] + b1[0x12];
		b2[0x12] = (b1[0x11] - b1[0x12]) * cos1;

		b2[0x14] = b1[0x14] + b1[0x17];
		b2[0x17] = (b1[0x17] - b1[0x14]) * cos0;
		b2[0x15] = b1[0x15] + b1[0x16];
		b2[0x16] = (b1[0x16] - b1[0x15]) * cos1;

		b2[0x18] = b1[0x18] + b1[0x1B];
		b2[0x1B] = (b1[0x18] - b1[0x1B]) * cos0;
		b2[0x19] = b1[0x19] + b1[0x1A];
		b2[0x1A] = (b1[0x19] - b1[0x1A]) * cos1;

		b2[0x1C] = b1[0x1C] + b1[0x1F];
		b2[0x1F] = (b1[0x1F] - b1[0x1C]) * cos0;
		b2[0x1D] = b1[0x1D] + b1[0x1E];
		b2[0x1E] = (b1[0x1E] - b1[0x1D]) * cos1;
	}

	{
		float const cos0 = gPnts[4][0];

		b1[0x00] = b2[0x00] + b2[0x01];
		b1[0x01] = (b2[0x00] - b2[0x01]) * cos0;
		b1[0x02] = b2[0x02] + b2[0x03];
		b1[0x03] = (b2[0x03] - b2[0x02]) * cos0;
		b1[0x02] += b1[0x03];

		b1[0x04] = b2[0x04] + b2[0x05];
		b1[0x05] = (b2[0x04] - b2[0x05]) * cos0;
		b1[0x06] = b2[0x06] + b2[0x07];
		b1[0x07] = (b2[0x07] - b2[0x06]) * cos0;
		b1[0x06] += b1[0x07];
		b1[0x04] += b1[0x06];
		b1[0x06] += b1[0x05];
		b1[0x05] += b1[0x07];

		b1[0x08] = b2[0x08] + b2[0x09];
		b1[0x09] = (b2[0x08] - b2[0x09]) * cos0;
		b1[0x0A] = b2[0x0A] + b2[0x0B];
		b1[0x0B] = (b2[0x0B] - b2[0x0A]) * cos0;
		b1[0x0A] += b1[0x0B];

		b1[0x0C] = b2[0x0C] + b2[0x0D];
		b1[0x0D] = (b2[0x0C] - b2[0x0D]) * cos0;
		b1[0x0E] = b2[0x0E] + b2[0x0F];
		b1[0x0F] = (b2[0x0F] - b2[0x0E]) * cos0;
		b1[0x0E] += b1[0x0F];
		b1[0x0C] += b1[0x0E];
		b1[0x0E] += b1[0x0D];
		b1[0x0D] += b1[0x0F];

		b1[0x10] = b2[0x10] + b2[0x11];
		b1[0x11] = (b2[0x10] - b2[0x11]) * cos0;
		b1[0x12] = b2[0x12] + b2[0x13];
		b1[0x13] = (b2[0x13] - b2[0x12]) * cos0;
		b1[0x12] += b1[0x13];

		b1[0x14] = b2[0x14] + b2[0x15];
		b1[0x15] = (b2[0x14] - b2[0x15]) * cos0;
		b1[0x16] = b2[0x16] + b2[0x17];
		b1[0x17] = (b2[0x17] - b2[0x16]) * cos0;
		b1[0x16] += b1[0x17];
		b1[0x14] += b1[0x16];
		b1[0x16] += b1[0x15];
		b1[0x15] += b1[0x17];

		b1[0x18] = b2[0x18] + b2[0x19];
		b1[0x19] = (b2[0x18] - b2[0x19]) * cos0;
		b1[0x1A] = b2[0x1A] + b2[0x1B];
		b1[0x1B] = (b2[0x1B] - b2[0x1A]) * cos0;
		b1[0x1A] += b1[0x1B];

		b1[0x1C] = b2[0x1C] + b2[0x1D];
		b1[0x1D] = (b2[0x1C] - b2[0x1D]) * cos0;
		b1[0x1E] = b2[0x1E] + b2[0x1F];
		b1[0x1F] = (b2[0x1F] - b2[0x1E]) * cos0;
		b1[0x1E] += b1[0x1F];
		b1[0x1C] += b1[0x1E];
		b1[0x1E] += b1[0x1D];
		b1[0x1D] += b1[0x1F];
	}

	out0[0x10*16] = b1[0x00];
	out0[0x10*12] = b1[0x04];
	out0[0x10* 8] = b1[0x02];
	out0[0x10* 4] = b1[0x06];
	out0[0x10* 0] = b1[0x01];
	out1[0x10* 0] = b1[0x01];
	out1[0x10* 4] = b1[0x05];
	out1[0x10* 8] = b1[0x03];
	out1[0x10*12] = b1[0x07];

	b1[0x08] += b1[0x0C];
	out0[0x10*14] = b1[0x08];
	b1[0x0C] += b1[0x0a];
	out0[0x10*10] = b1[0x0C];
	b1[0x0A] += b1[0x0E];
	out0[0x10* 6] = b1[0x0A];
	b1[0x0E] += b1[0x09];
	out0[0x10* 2] = b1[0x0E];
	b1[0x09] += b1[0x0D];
	out1[0x10* 2] = b1[0x09];
	b1[0x0D] += b1[0x0B];
	out1[0x10* 6] = b1[0x0D];
	b1[0x0B] += b1[0x0F];
	out1[0x10*10] = b1[0x0B];
	out1[0x10*14] = b1[0x0F];

	b1[0x18] += b1[0x1C];
	out0[0x10*15] = b1[0x10] + b1[0x18];
	out0[0x10*13] = b1[0x18] + b1[0x14];
	b1[0x1C] += b1[0x1a];
	out0[0x10*11] = b1[0x14] + b1[0x1C];
	out0[0x10* 9] = b1[0x1C] + b1[0x12];
	b1[0x1A] += b1[0x1E];
	out0[0x10* 7] = b1[0x12] + b1[0x1A];
	out0[0x10* 5] = b1[0x1A] + b1[0x16];
	b1[0x1E] += b1[0x19];
	out0[0x10* 3] = b1[0x16] + b1[0x1E];
	out0[0x10* 1] = b1[0x1E] + b1[0x11];
	b1[0x19] += b1[0x1D];
	out1[0x10* 1] = b1[0x11] + b1[0x19];
	out1[0x10* 3] = b1[0x19] + b1[0x15];
	b1[0x1D] += b1[0x1B];
	out1[0x10* 5] = b1[0x15] + b1[0x1D];
	out1[0x10* 7] = b1[0x1D] + b1[0x13];
	b1[0x1B] += b1[0x1F];
	out1[0x10* 9] = b1[0x13] + b1[0x1B];
	out1[0x10*11] = b1[0x1B] + b1[0x17];
	out1[0x10*13] = b1[0x17] + b1[0x1F];
	out1[0x10*15] = b1[0x1F];
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
#ifndef FMOD_SUPPORT_MPEG_SIMD        /* SIMD based decoding */


 /* old WRITE_SAMPLE */
#define WRITE_SAMPLE(samples,sum) \
  if( (sum) > 32767.0f) { *(samples) = 0x7fff; } \
  else if( (sum) < -32768.0f) { *(samples) = -0x8000; } \
  else { *(samples) = (short)sum; }

FMOD_RESULT CodecMPEG::synthC(float *b0, int bo1, int channelskip, signed short *samples)
{
    int j;
    float *window = FMOD_Mpeg_DecWin + 16 - bo1;

    for (j=16;j;j--,b0+=0x10,window+=0x20,samples+=channelskip)
    {
        float sum;
        sum  = window[0x0] * b0[0x0];
        sum -= window[0x1] * b0[0x1];
        sum += window[0x2] * b0[0x2];
        sum -= window[0x3] * b0[0x3];
        sum += window[0x4] * b0[0x4];
        sum -= window[0x5] * b0[0x5];
        sum += window[0x6] * b0[0x6];
        sum -= window[0x7] * b0[0x7];
        sum += window[0x8] * b0[0x8];
        sum -= window[0x9] * b0[0x9];
        sum += window[0xA] * b0[0xA];
        sum -= window[0xB] * b0[0xB];
        sum += window[0xC] * b0[0xC];
        sum -= window[0xD] * b0[0xD];
        sum += window[0xE] * b0[0xE];
        sum -= window[0xF] * b0[0xF];
        sum *= 32767.0f;

        #if (defined(PLATFORM_WINDOWS) || defined(PLATFORM_XBOX)) && defined(PLATFORM_32BIT)
        {
            signed int val;
            __asm fld sum
            __asm fistp val
            if (val > 32767)   val = 32767;
            if (val < -32768)  val = 32768;
            *samples = (signed short)val;
        }
        #else
        WRITE_SAMPLE(samples,sum);
        #endif
    }

    {
        float sum;
        sum  = window[0x0] * b0[0x0];
        sum += window[0x2] * b0[0x2];
        sum += window[0x4] * b0[0x4];
        sum += window[0x6] * b0[0x6];
        sum += window[0x8] * b0[0x8];
        sum += window[0xA] * b0[0xA];
        sum += window[0xC] * b0[0xC];
        sum += window[0xE] * b0[0xE];
        sum *= 32767.0f;

        #if (defined(PLATFORM_WINDOWS) || defined(PLATFORM_XBOX)) && defined(PLATFORM_32BIT)
        {
            signed int val;
            __asm fld sum
            __asm fistp val
            if (val > 32767)   val = 32767;
            if (val < -32768)  val = 32768;
            *samples = (signed short)val;
        }
        #else
        WRITE_SAMPLE(samples,sum);
        #endif

        b0-=0x10,window-=0x20,samples+=channelskip;
    }
    window += bo1<<1;

    for (j=15;j;j--,b0-=0x10,window-=0x20,samples+=channelskip)
    {
        float sum;
        sum = -window[-0x1] * b0[0x0];
        sum -= window[-0x2] * b0[0x1];
        sum -= window[-0x3] * b0[0x2];
        sum -= window[-0x4] * b0[0x3];
        sum -= window[-0x5] * b0[0x4];
        sum -= window[-0x6] * b0[0x5];
        sum -= window[-0x7] * b0[0x6];
        sum -= window[-0x8] * b0[0x7];
        sum -= window[-0x9] * b0[0x8];
        sum -= window[-0xA] * b0[0x9];
        sum -= window[-0xB] * b0[0xA];
        sum -= window[-0xC] * b0[0xB];
        sum -= window[-0xD] * b0[0xC];
        sum -= window[-0xE] * b0[0xD];
        sum -= window[-0xF] * b0[0xE];
        sum -= window[-0x10] * b0[0xF];
        sum *= 32767.0f;

        #if (defined(PLATFORM_WINDOWS) || defined(PLATFORM_XBOX)) && defined(PLATFORM_32BIT)
        {
            signed int val;
            __asm fld sum
            __asm fistp val
            if (val > 32767)  val = 32767;
            if (val < -32768) val = 32768;
            *samples = (signed short)val;
        }
        #else
        WRITE_SAMPLE(samples,sum);
        #endif
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

	[SEE_ALSO]
]
*/
// VERIFY: 7c9bcf22 manual
// OFFSET: 0x0092D300
FMOD_RESULT CodecMPEG::synth(void *samples, float *bandPtr, int channels, int channelskip)
{	
	int		count;
	float	*b0,*buf;
	int		bo1,bob1,bob2;
	
	mMemoryBlock->mSynthBo--;
	mMemoryBlock->mSynthBo &= 0xf;

	bob1 = mMemoryBlock->mSynthBo & 0x1;
	bob2 = bob1 ^ 1;
	bo1  = mMemoryBlock->mSynthBo+bob2;

	if (!samples)
    {
		return FMOD_ERR_INVALID_PARAM;
    }
       
	for (count = 0 ; count < channels; count++)
	{				
		buf = &mMemoryBlock->mSynthBuffs[count * 576];
		b0 =  &buf[bob2 * 288];

        if (mMemoryBlock->mFrame.lay == 2)
        {
            #ifdef 	FMOD_SUPPORT_MPEG_SIMD
		    FMOD_Mpeg_DCT64(&buf[bob1 * 288]+((mMemoryBlock->mSynthBo+bob1)&0xf),b0+bo1, bandPtr+(count*4*SBLIMIT));
		    #else
		    dct64(&buf[bob1 * 288]+((mMemoryBlock->mSynthBo+bob1)&0xf),b0+bo1, bandPtr+(count*4*SBLIMIT));
		    #endif
        }
        if (mMemoryBlock->mFrame.lay == 3)
        {
            #ifdef FMOD_SUPPORT_MPEG_SIMD
    		FMOD_Mpeg_DCT64(&buf[bob1 * 288]+((mMemoryBlock->mSynthBo+bob1)&0xf),b0+bo1, bandPtr+(count*SSLIMIT*SBLIMIT));
    		#else
    		dct64(&buf[bob1 * 288]+((mMemoryBlock->mSynthBo+bob1)&0xf),b0+bo1, bandPtr+(count*SSLIMIT*SBLIMIT));
    		#endif
        }

        #if defined(FMOD_SUPPORT_MPEG_SIMD) || (defined(PLATFORM_WINDOWS) && defined(PLATFORM_32BIT))
		FMOD_Mpeg_Synth_FPU(b0, bo1, channelskip, (short *)samples + count);
        #else
		synthC(b0, bo1, channelskip, (short *)samples + count);
        #endif
	}

    return FMOD_OK;
}

#endif // #ifndef FMOD_SUPPORT_MPEG_SONYDECODER

}

#endif /* defined(FMOD_SUPPORT_MPEG) */


