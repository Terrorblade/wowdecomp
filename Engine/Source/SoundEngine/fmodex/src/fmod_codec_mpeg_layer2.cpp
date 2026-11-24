#include "fmod_settings.h"

#ifdef FMOD_SUPPORT_MPEG

#ifdef FMOD_SUPPORT_MPEG_LAYER2

#include "fmod_codec_mpeg.h"
#include "fmod_types.h"

#include <math.h>

#ifndef PLATFORM_PS3_SPU
    #define USE_MULS_TABLE
#endif

namespace FMOD
{

#ifndef FMOD_SUPPORT_MPEG_SONYDECODER

unsigned char CodecMPEG::gGrp3Tab[32 * 3] = { 0, };   // used: 27 
unsigned char CodecMPEG::gGrp5Tab[128 * 3] = { 0, };  // used: 125
unsigned char CodecMPEG::gGrp9Tab[1024 * 3] = { 0, }; // used: 729


#ifdef USE_MULS_TABLE
    float   CodecMPEG::gMulsTab[27][64];	// also used by layer 1 

    #define gMuls(_x, _y) gMulsTab[_x][_y]
#else

	float gMulMul[27] = 
	{
		0.0f , -2.0f/3.0f , 2.0f/3.0f ,
		2.0f/7.0f , 2.0f/15.0f , 2.0f/31.0f, 2.0f/63.0f , 2.0f/127.0f , 2.0f/255.0f ,
		2.0f/511.0f , 2.0f/1023.0f , 2.0f/2047.0f , 2.0f/4095.0f , 2.0f/8191.0f ,
		2.0f/16383.0f , 2.0f/32767.0f , 2.0f/65535.0f ,
		-4.0f/5.0f , -2.0f/5.0f , 2.0f/5.0f, 4.0f/5.0f ,
		-8.0f/9.0f , -4.0f/9.0f , -2.0f/9.0f , 2.0f/9.0f , 4.0f/9.0f , 8.0f/9.0f 
	};

    #ifdef PLATFORM_PS3_SPU

    float gMuls(int _x, int _y) { return ((_y) == 63) ? 0.0f : (float)(gMulMul[(_x)] * FMOD_POW(2.0f, (3 - (int)(_y)) / 3.0f)); } /* Function call to make code smaller */

    #else

    #define gMuls(_x, _y) ((_y) == 63) ? 0.0f : (float)(gMulMul[(_x)] * FMOD_POW(2.0f, (3 - (int)(_y)) / 3.0f))

    #endif

#endif

/*
 * Layer 2 Alloc tables .. 
 * most other tables are calculated on program start (which is (of course)
 * not ISO-conform) .. 
 * Layer-3 huffman table is in huffman.h
 */

struct al_table CodecMPEG::gAlloc0[] = 
{
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},
	{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},
	{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},
	{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767} 
};

struct al_table CodecMPEG::gAlloc1[] = 
{
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},
	{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},
	{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},
	{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767} };

struct al_table CodecMPEG::gAlloc2[] = 
{
	{4,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},
	{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},
	{4,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},
	{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63} 
};

struct al_table CodecMPEG::gAlloc3[] = 
{
	{4,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},
	{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},
	{4,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},
	{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63} 
};

struct al_table CodecMPEG::gAlloc4[] = 
{
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
		{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
		{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
		{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
		{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
    {2,0},{5,3},{7,5},{10,9},
    {2,0},{5,3},{7,5},{10,9},
    {2,0},{5,3},{7,5},{10,9},
    {2,0},{5,3},{7,5},{10,9},
    {2,0},{5,3},{7,5},{10,9},
    {2,0},{5,3},{7,5},{10,9},
    {2,0},{5,3},{7,5},{10,9},
    {2,0},{5,3},{7,5},{10,9}  
};


#if defined(PLATFORM_PS3_SPU_STREAMDECODE) || !defined(PLATFORM_PS3_SPU)
/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 81c71921 manual
// OFFSET: 0x009317C0
FMOD_RESULT CodecMPEG::initLayer2()
{
	float mulmul[27] = 
	{
		0.0f , -2.0f/3.0f , 2.0f/3.0f ,
		2.0f/7.0f , 2.0f/15.0f , 2.0f/31.0f, 2.0f/63.0f , 2.0f/127.0f , 2.0f/255.0f ,
		2.0f/511.0f , 2.0f/1023.0f , 2.0f/2047.0f , 2.0f/4095.0f , 2.0f/8191.0f ,
		2.0f/16383.0f , 2.0f/32767.0f , 2.0f/65535.0f ,
		-4.0f/5.0f , -2.0f/5.0f , 2.0f/5.0f, 4.0f/5.0f ,
		-8.0f/9.0f , -4.0f/9.0f , -2.0f/9.0f , 2.0f/9.0f , 4.0f/9.0f , 8.0f/9.0f 
	};
	char base[3][9] = 
	{
		{ 1 , 0, 2 , } ,
		{ 17, 18, 0 , 19, 20 , } ,
		{ 21, 1, 22, 23, 0, 24, 25, 2, 26 } 
	};
	int			i,j,k,l,len;
	int	tablen[3] = { 3 , 5 , 9 };
	unsigned char *itable,*tables[3] = { gGrp3Tab , gGrp5Tab , gGrp9Tab };

	for(i=0;i<3;i++)
	{
		itable = tables[i];
		len = tablen[i];
		for(j=0;j<len;j++)
		{
			for(k=0;k<len;k++)
			{
				for(l=0;l<len;l++)
				{
					*itable++ = base[i][l];
					*itable++ = base[i][k];
					*itable++ = base[i][j];
				}
			}
		}
	}

#ifdef USE_MULS_TABLE
    {
    	float *		table;

	    for(k=0;k<27;k++)
	    {
		    float m=mulmul[k];
		    table = gMulsTab[k];
	    
		    for(j=3,i=0;i<63;i++,j--)
			    *table++ = (float)(m * FMOD_POW(2.0f, (3 - i) / 3.0f));
	    
		    *table++ = 0.0f;
	    }
    }
#endif


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
// VERIFY: f32f7658 manual
// OFFSET: 0x00931A60
FMOD_RESULT CodecMPEG::II_step_one(unsigned int *bit_alloc,int *scale)
{
    int				stereo   = mMemoryBlock->mFrame.stereo-1;
    int				sblimit  = mMemoryBlock->mFrame.II_sblimit;
    int				jsbound  = mMemoryBlock->mFrame.jsbound;
    int				sblimit2 = mMemoryBlock->mFrame.II_sblimit<<stereo;
    struct al_table *alloc1  = mMemoryBlock->mFrame.alloc;
    int				i;
    unsigned int scfsi_buf[64];
    unsigned int	*scfsi,*bita;
    int				sc,step;

    bita = bit_alloc;

    if(stereo)
    {
		for (i=jsbound;i;i--,alloc1+=((ptrdiff_t)1<<step))
		{
			*bita++ = (char)getBits(step=alloc1->bits);
			*bita++ = (char)getBits(step);
		}
		for (i=sblimit-jsbound;i;i--,alloc1+=((ptrdiff_t)1<<step))
		{
			bita[0] = (char) getBits(step=alloc1->bits);
			bita[1] = bita[0];
			bita+=2;
		}

		bita = bit_alloc;
		scfsi=scfsi_buf;

		for (i=sblimit2;i;i--)
			if (*bita++)
				*scfsi++ = (char) getBitsFast(2);
    }
    else // mono 
    {
		for (i=sblimit;i;i--,alloc1+=((ptrdiff_t)1<<step))
        {
			*bita++ = (char) getBits(step=alloc1->bits);
        }

		bita = bit_alloc;
		scfsi=scfsi_buf;
	
		for (i=sblimit;i;i--)
        {
			if (*bita++)
            {
				*scfsi++ = (char) getBitsFast(2);
            }
        }
    }

    bita = bit_alloc;
    scfsi=scfsi_buf;
    for (i=sblimit2;i;i--) 
    {
        if (*bita++)
        {
            switch (*scfsi++) 
            {
              case 0: 
                    *scale++ = getBitsFast(6);
                    *scale++ = getBitsFast(6);
                    *scale++ = getBitsFast(6);
                    break;
              case 1 : 
                    *scale++ = sc = getBitsFast(6);
                    *scale++ = sc;
                    *scale++ = getBitsFast(6);
                    break;
              case 2: 
                    *scale++ = sc = getBitsFast(6);
                    *scale++ = sc;
                    *scale++ = sc;
                    break;
              default:              // case 3 
                    *scale++ = getBitsFast(6);
                    *scale++ = sc = getBitsFast(6);
                    *scale++ = sc;
                    break;
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

	[SEE_ALSO]
]
*/
// VERIFY: b05f67c7 manual
// OFFSET: 0x00931C90
FMOD_RESULT CodecMPEG::II_step_two(unsigned int *bit_alloc,float fraction[2][4][SBLIMIT],int *scale,int x1)
{
    int				i,j,k,ba;
    int				stereo  = mMemoryBlock->mFrame.stereo;
    int				sblimit = mMemoryBlock->mFrame.II_sblimit;
    int				jsbound = mMemoryBlock->mFrame.jsbound;
    struct al_table *alloc2,*alloc1 = mMemoryBlock->mFrame.alloc;
    unsigned int *	bita=bit_alloc;
    int				d1,step;

	for (i=0;i<jsbound;i++,alloc1+=((ptrdiff_t)1<<step))
	{
		step = alloc1->bits;
		for (j=0;j<stereo;j++)
		{
            ba=*bita++;
			if ( ba ) 
			{
				k=(alloc2 = alloc1+ba)->bits;
				if( (d1=alloc2->d) < 0) 
				{
					float cm = gMuls(k, scale[x1]);

					fraction[j][0][i] = ((float) ((int)getBits(k) + d1)) * cm;
					fraction[j][1][i] = ((float) ((int)getBits(k) + d1)) * cm;
					fraction[j][2][i] = ((float) ((int)getBits(k) + d1)) * cm;
				}        
				else 
				{
					unsigned char *table[] = { 0,0,0, gGrp3Tab, 0, gGrp5Tab, 0, 0, 0, gGrp9Tab };
					unsigned int idx,m=scale[x1];
                    unsigned char *tab;

					idx = (unsigned int) getBits(k);
					tab = table[d1] + idx + idx + idx;
					
                    fraction[j][0][i] = gMuls(*tab++, m);
                    fraction[j][1][i] = gMuls(*tab++, m);
                    fraction[j][2][i] = gMuls(*tab, m);  
				}
				scale+=3;
			}
			else
            {
				fraction[j][0][i] = fraction[j][1][i] = fraction[j][2][i] = 0.0f;
            }
		}
	}


	for (i=jsbound;i<sblimit;i++,alloc1+=((ptrdiff_t)1<<step))
	{
		step = alloc1->bits;
		bita++;	// channel 1 and channel 2 bitalloc are the same 

        ba=*bita++;
		if (ba)
		{
			k=(alloc2 = alloc1+ba)->bits;
			if( (d1=alloc2->d) < 0)
			{
				float cm;
				cm = gMuls(k, scale[x1+3]);
				fraction[1][0][i] = (fraction[0][0][i] = (float) ((int)getBits(k) + d1) ) * cm;
				fraction[1][1][i] = (fraction[0][1][i] = (float) ((int)getBits(k) + d1) ) * cm;
				fraction[1][2][i] = (fraction[0][2][i] = (float) ((int)getBits(k) + d1) ) * cm;
				cm = gMuls(k, scale[x1]);
				fraction[0][0][i] *= cm; fraction[0][1][i] *= cm; fraction[0][2][i] *= cm;
			}
			else
			{
				unsigned char *table[] = { 0,0,0, gGrp3Tab, 0, gGrp5Tab, 0, 0, 0, gGrp9Tab };
				unsigned int idx,m1,m2;
                unsigned char *tab;

				m1 = scale[x1]; m2 = scale[x1+3];
				idx = (unsigned int) getBits(k);
				tab = table[d1] + idx + idx + idx;
				fraction[0][0][i] = gMuls(*tab, m1); fraction[1][0][i] = gMuls(*tab++, m2);
				fraction[0][1][i] = gMuls(*tab, m1); fraction[1][1][i] = gMuls(*tab++, m2);
				fraction[0][2][i] = gMuls(*tab, m1); fraction[1][2][i] = gMuls(*tab, m2);
			}
			scale+=6;
		}
		else 
		{
			fraction[0][0][i] = fraction[0][1][i] = fraction[0][2][i] =
			fraction[1][0][i] = fraction[1][1][i] = fraction[1][2][i] = 0.0f;
		}
    }

	for(i=sblimit;i<SBLIMIT;i++)
    {
		for (j=0;j<stereo;j++)
        {
			fraction[j][0][i] = fraction[j][1][i] = fraction[j][2][i] = 0.0f;
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

	[SEE_ALSO]
]
*/
// VERIFY: 0db3b09b auto
// OFFSET: 0x009320D0
FMOD_RESULT CodecMPEG::getIIStuff()
{
	int translate[3][2][16] = 
	{
		{ 
			{ 0,2,2,2,2,2,2,0,0,0,1,1,1,1,1,0 } ,
			{ 0,2,2,0,0,0,1,1,1,1,1,1,1,1,1,0 } 
		} ,
		{ 
			{ 0,2,2,2,2,2,2,0,0,0,0,0,0,0,0,0 } ,
			{ 0,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0 } } ,
		{ 
			{ 0,3,3,3,3,3,3,0,0,0,1,1,1,1,1,0 } ,
			{ 0,3,3,0,0,0,1,1,1,1,1,1,1,1,1,0 } 
		} 
	};

	int table,sblim;
	struct al_table *tables[5] = { gAlloc0, gAlloc1, gAlloc2, gAlloc3 , gAlloc4 };
	int sblims[5] = { 27 , 30 , 8, 12 , 30 };

	if (mMemoryBlock->mFrame.lsf)
    {
		table = 4;
    }
	else
    {
		table = translate[mMemoryBlock->mFrame.sampling_frequency][2-mMemoryBlock->mFrame.stereo][mMemoryBlock->mFrame.bitrate_index];
    }
  
	sblim = sblims[table];

	mMemoryBlock->mFrame.alloc = tables[table];
	mMemoryBlock->mFrame.II_sblimit = sblim;

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
// VERIFY: 7470f6b1 auto
// OFFSET: 0x00932380
FMOD_RESULT CodecMPEG::decodeLayer2(void *pcm_sample, unsigned int *outlen)
{
	int				i,j = 0;
	int				channels = mMemoryBlock->mFrame.stereo;
	float       	fraction[2][4][SBLIMIT]; // pick_table clears unused subbands 
	unsigned int	bit_alloc[64];
	int				scale[192];
    int             inc = SBLIMIT * sizeof(signed short);
    char           *pcm_start = (char *)pcm_sample;

	II_step_one(bit_alloc, scale);

	for (i=0;i<SCALE_BLOCK;i++) 
	{
		II_step_two(bit_alloc,fraction,scale,i>>2);

		for (j=0;j<3;j++) 
		{
			synth(pcm_sample, fraction[0][j], channels, mChannels ? mChannels : channels);

			pcm_sample = (char *)pcm_sample + inc * channels;
		}
	}

    *outlen = (unsigned int)((char *)pcm_sample - pcm_start);
	
	return FMOD_OK;
}


#endif //#ifndef FMOD_SUPPORT_MPEG_SONYDECODER

}

#endif // FMOD_SUPPORT_MPEG_LAYER2

#endif // FMOD_SUPPORT_MPEG

