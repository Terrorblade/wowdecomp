#ifndef _FMOD_CODEC_MPEG_H
#define _FMOD_CODEC_MPEG_H

#include "fmod_settings.h"

#ifdef FMOD_SUPPORT_MPEG

#include "fmod_codeci.h"

#ifdef FMOD_SUPPORT_MPEG_SONYDECODER
    #include "../ps3/lib/sony/mp3declib.h"
#endif

#ifndef PLATFORM_PS3
    #define USE_ISPOW_TABLE
#endif

namespace FMOD
{
    /*
        DEFINITIONS
    */

    #define SBLIMIT					32
    #define SSLIMIT					18
    #define SCALE_BLOCK             12

    #define	MPG_MD_STEREO           0
    #define	MPG_MD_JOINT_STEREO     1
    #define	MPG_MD_DUAL_CHANNEL     2
    #define	MPG_MD_MONO             3

    #define	MAXFRAMESIZE 1792
    
    /*
        STRUCTURE DEFINITIONS
    */

    struct al_table 
    {
      short bits;
      short d;
    };

    typedef struct
    {
        struct al_table *alloc;
        int stereo;
        int jsbound;
        int II_sblimit;
        int lsf;
        int mpeg25;
        int header_change;
        int lay;
        int error_protection;
        int bitrate_index;
        int sampling_frequency;
        int padding;
        int extension;
        int mode;
        int mode_ext;
        int copyright;
        int original;
        int emphasis;
        int framesize; /* computed framesize */
    } MPEG_FRAME;

    struct gr_info_s 
    {
          int scfsi;
          unsigned part2_3_length;
          unsigned big_values;
          unsigned scalefac_compress;
          unsigned block_type;
          unsigned mixed_block_flag;
          unsigned table_select[3];
          unsigned subblock_gain[3];
          unsigned maxband[3];
          unsigned maxbandl;
          unsigned maxb;
          unsigned region1start;
          unsigned region2start;
          unsigned preflag;
          unsigned scalefac_scale;
          unsigned count1table_select;
          float *full_gain[3];
          float *pow2gain;
    };

    struct III_sideinfo
    {
	    unsigned main_data_begin;
	    unsigned private_bits;
	    
	    struct 
	    {
		    struct gr_info_s gr[2];
	    } ch[2];
    };

    struct bandInfoStruct
    {
	    int longIdx[23];
	    int longDiff[22];
	    int shortIdx[14];
	    int shortDiff[13];
    };

    class SyncPoint;

    struct bitstream_info 
    {
        int mBitIndex;
        unsigned char *mWordPointer;
    };

    struct CodecMPEG_MemoryBlock
    {
      #ifdef FMOD_SUPPORT_MPEG_SONYDECODER

        CellMP3Context      mContext1;
        CellMP3Context      mContext2;
        int                 mCount;
        int                 mStereo;
        char                mPad[4];

      #else

        int					mFrameSize;
        int					mFrameSizeOld;
	    MPEG_FRAME	        mFrame;
	    unsigned int 		mFrameHeader;
        unsigned int        mNumFrames;
	    unsigned int       *mFrameOffset;
	    int					mPcmPoint;
	    int					mLayer;
        unsigned char		mBSSpace[2][MAXFRAMESIZE+512];      /* 4608 */
	    int					mBSNum;
	    float				mSynthBuffs[2 * 576];               /* 4608 */
        int					mSynthBo;
	    bitstream_info		mBSI;
	    float				mBlock[2][2][SBLIMIT * SSLIMIT];    /* 9216 */
    	int                 mBlc[2];
      #endif
        unsigned char       mXingToc[100];
        bool                mHasXingNumFrames;
        bool                mHasXingToc;
    };

    class CodecMPEG : public Codec
    {
      public:       
        CodecMPEG_MemoryBlock  *mMemoryBlock;
        CodecMPEG_MemoryBlock  *mMemoryBlockMemory;

        SyncPoint          *mSyncPoint;
        int                 mNumSyncPoints;
        unsigned int        mPCMFrameLengthBytes;
        unsigned char      *mPCMBufferMemory;
        int                 mChannels;

        static int          gTabSel123[2][3][16];
        static int          gFreqs[9];

        static bool         gInitialized;

        #ifndef FMOD_SUPPORT_MPEG_SONYDECODER
        /*
            Static, global members used by all codecs 
        */
        static float        gDecWinMem[512+32+16];                /* +16 for alignment */
        static FMOD_ALIGN16(float gCos64[16]);
        static FMOD_ALIGN16(float gCos32[8]);
        static FMOD_ALIGN16(float gCos16[4]);
        static FMOD_ALIGN16(float gCos8[2]);
        static FMOD_ALIGN16(float gCos4[1]);
        static float       *gPnts[5];
        static int          gIntWinBase[];


        /*
            Layer 2 globals
        */
        static unsigned char    gGrp3Tab[32 * 3];
        static unsigned char    gGrp5Tab[128 * 3];
        static unsigned char    gGrp9Tab[1024 * 3];
        static float            gMulsTab[27][64];
        static struct al_table  gAlloc0[];
        static struct al_table  gAlloc1[];
        static struct al_table  gAlloc2[];
        static struct al_table  gAlloc3[];
        static struct al_table  gAlloc4[];

        /*
            Layer 3 globals
        */
        static float            gIsPowTable[8207];
        static float            gAaCa[8];
        static float            gAaCs[8];
        static float            gWin[4][36];
        static float            gWin1[4][36];
        static float            gGainPow2[256 + 118 + 4];
        static float            gCos6_1;
        static float            gCos6_2;
        static float            gTfCos36[9];
        static float            gTfCos12[3];
        static float            gCos9[3];
        static float            gCos18[3];
        static int              gLongLimit[9][23];
        static int              gShortLimit[9][14];
        static struct bandInfoStruct gBandInfo[9];
        static int              gMapBuf0[9][152];
        static int              gMapBuf1[9][156];
        static int              gMapBuf2[9][44];
        static int             *gMap[9][3];
        static int             *gMapEnd[9][3];
        static unsigned int     gN_SLen2[512];	/* MPEG 2.0 slen for 'normal' mode */
        static unsigned int     gI_SLen2[256];	/* MPEG 2.0 slen for intensity stereo */
        static float            gTan1_1[16];
        static float            gTan2_1[16];
        static float            gTan1_2[16];
        static float            gTan2_2[16];
        static float            gPow1_1[2][16];
        static float            gPow2_1[2][16];
        static float            gPow1_2[2][16];
        static float            gPow2_2[2][16];

        static FMOD_RESULT  makeTables(int scaleval);
        static FMOD_RESULT  initLayer2();
        static FMOD_RESULT  initLayer3(int down_sample_sblimit);
        #endif

        static FMOD_RESULT  initAll();
        
        #ifdef FMOD_SUPPORT_MPEG_SPU
        static FMOD_RESULT  closeAll();
        #endif

        /*
            Per mpeg member functions
        */
        FMOD_RESULT         decodeHeader(void *in, int *samplerate, int *channels, int *framesize);
        FMOD_RESULT         decodeXingHeader(unsigned char *in, unsigned char *toc, unsigned int *numframes);
        FMOD_RESULT         decodeFrame(unsigned char *in, void *out, unsigned int *outlen);
        FMOD_RESULT         decodeLayer2(void *out, unsigned int *outlen);
        FMOD_RESULT         decodeLayer3(void *out, unsigned int *outlen);
        FMOD_RESULT         decodeLayer3(unsigned char *in, void *out, unsigned int *outlen); /* Using Sony Decoder for PS3 */
        FMOD_RESULT         getPCMLength();
        FMOD_RESULT         resetFrame();
        FMOD_RESULT         getIIStuff();
        FMOD_RESULT         II_step_one(unsigned int *bit_alloc,int *scale);
        FMOD_RESULT         II_step_two(unsigned int *bit_alloc,float fraction[2][4][SBLIMIT],int *scale,int x1);
        FMOD_RESULT         III_get_side_info_1(struct III_sideinfo *si, int stereo, int ms_stereo, int sfreq);
        FMOD_RESULT         III_get_side_info_2(struct III_sideinfo *si, int stereo, int ms_stereo, int sfreq);
        FMOD_RESULT         III_get_scale_factors_1(int *scf,struct gr_info_s *gr_info,  int *numbits);
        FMOD_RESULT         III_get_scale_factors_2(int *scf,struct gr_info_s *gr_info,int i_stereo, int *numbits);
        FMOD_RESULT         III_dequantize_sample(float xr[SBLIMIT][SSLIMIT],int *scf, struct gr_info_s *gr_info,int sfreq,int part2bits);
        FMOD_RESULT         III_dequantize_sample_ms(float xr[2][SBLIMIT][SSLIMIT], int *scf, struct gr_info_s *gr_info, int sfreq, int part2bits);
        FMOD_RESULT         III_i_stereo(float xr_buf[2][SBLIMIT][SSLIMIT], int *scalefac, struct gr_info_s *gr_info, int sfreq, int ms_stereo, int lsf);
        FMOD_RESULT         III_antialias(float xr[SBLIMIT][SSLIMIT], struct gr_info_s *gr_info);
        FMOD_RESULT         III_hybrid(float fsIn[SBLIMIT][SSLIMIT], float tsOut[SSLIMIT][SBLIMIT], int ch, struct gr_info_s *gr_info);

        FMOD_RESULT         synth(void *samples, float *bandPtr, int channels, int channelskip);
        static FMOD_RESULT  synthC(float *b0, int bo1, int channels, signed short *samples);

        unsigned int        getBits(int number_of_bits);
        unsigned int        getBitsFast(int number_of_bits);

        #ifndef FMOD_SUPPORT_MPEG_SONYDECODER
		FMOD_INLINE unsigned int get1Bit()
		{
			unsigned char rval;

			rval = *(mMemoryBlock->mBSI.mWordPointer) << mMemoryBlock->mBSI.mBitIndex;

			mMemoryBlock->mBSI.mBitIndex++;
			mMemoryBlock->mBSI.mWordPointer += (mMemoryBlock->mBSI.mBitIndex>>3);
			mMemoryBlock->mBSI.mBitIndex &= 7;

			return (unsigned int)(rval>>7);
		}
        FMOD_INLINE int getByte()
        {
            return *mMemoryBlock->mBSI.mWordPointer++;
        }
        FMOD_INLINE void backbits(int number_of_bits)
        {
              mMemoryBlock->mBSI.mBitIndex    -= number_of_bits;
              mMemoryBlock->mBSI.mWordPointer += (mMemoryBlock->mBSI.mBitIndex>>3);
              mMemoryBlock->mBSI.mBitIndex    &= 0x7;
        }
        #endif

        static void         dct64(float *out0,float *out1,float *samples);
        static void         dct36(float * inbuf, float * o1, float * o2, float * wintab, float * tsbuf);
        static void         dct12(float * in, float * rawout1, float * rawout2, float * wi, float * ts);

        /*
            Codec functions
        */
#ifndef PLATFORM_PS3_SPU
        FMOD_RESULT         openInternal(FMOD_MODE usermode, FMOD_CREATESOUNDEXINFO *userexinfo);
        FMOD_RESULT         closeInternal();
        FMOD_RESULT         soundCreateInternal(int subsound, FMOD_SOUND *sound);
#endif
        FMOD_RESULT         readInternal(void *buffer, unsigned int size, unsigned int *read);
        FMOD_RESULT         setPositionInternal(int subsound, unsigned int position, FMOD_TIMEUNIT postype);
        FMOD_RESULT         resetInternal() { return resetFrame(); }

      public:

#ifndef PLATFORM_PS3_SPU
        static FMOD_RESULT F_CALLBACK openCallback(FMOD_CODEC_STATE *codec, FMOD_MODE usermode, FMOD_CREATESOUNDEXINFO *userexinfo);
        static FMOD_RESULT F_CALLBACK closeCallback(FMOD_CODEC_STATE *codec);
        static FMOD_RESULT F_CALLBACK soundCreateCallback(FMOD_CODEC_STATE *codec, int subsound, FMOD_SOUND *sound);

        static FMOD_CODEC_DESCRIPTION_EX *getDescriptionEx();        
#endif
        static FMOD_RESULT F_CALLBACK readCallback(FMOD_CODEC_STATE *codec, void *buffer, unsigned int sizebytes, unsigned int *bytesread);
        static FMOD_RESULT F_CALLBACK setPositionCallback(FMOD_CODEC_STATE *codec, int subsound, unsigned int position, FMOD_TIMEUNIT postype);
        static FMOD_RESULT F_CALLBACK resetCallback(FMOD_CODEC_STATE *codec);
    };
}

#ifdef __cplusplus
extern "C" {
#endif

extern float *FMOD_Mpeg_DecWin;

void FMOD_Mpeg_Synth_FPU(float *b0, int bo1, int numchannels, short *samps);
void FMOD_Mpeg_Synth_FPU_Float(float *b0, int bo1, int numchannels, float *out);
void FMOD_Mpeg_DCT64(float *out0,float *out1,float *samples);

#ifdef __cplusplus
}
#endif


#endif  /* FMOD_SUPPORT_MPEG */

#endif


