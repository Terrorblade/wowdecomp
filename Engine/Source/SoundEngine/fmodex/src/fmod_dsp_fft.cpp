#include "fmod_settings.h"

#include "fmod_dsp_fft.h"

#ifdef FMOD_SUPPORT_GETSPECTRUM

#include "fmod.h"
#include "fmod_3d.h"
#include "fmod_dsp_fft.h"

namespace FMOD
{

/*
[
	[DESCRIPTION]
 
	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
// VERIFY: 093c489f manual
// OFFSET: 0x00918D30
DSPFFT::DSPFFT()
{
#ifdef USECOSTAB
    int count;

    for (count = 0; count < DSPFFT_COSTABSIZE; count++)
    {
        mCosTab[count] = (float)FMOD_COS(FMOD_PI_2 * (float)count / (float)DSPFFT_COSTABSIZE);
    }
#endif
}

#ifdef USECOSTAB

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
FMOD_INLINE const float DSPFFT::cosine(float x)
{
    int y;

    x *= DSPFFT_TABLERANGE;
    y = (int)x;
    if (y < 0)
    {
        y = -y; 
    }

    y &= DSPFFT_TABLEMASK;
    switch (y >> DSPFFT_COSTABBITS)
    {
        case 0 : return  mCosTab[y]; 
        case 1 : return -mCosTab[(DSPFFT_COSTABSIZE - 1) - (y - (DSPFFT_COSTABSIZE * 1))]; 
        case 2 : return -mCosTab[                          (y - (DSPFFT_COSTABSIZE * 2))]; 
        case 3 : return  mCosTab[(DSPFFT_COSTABSIZE - 1) - (y - (DSPFFT_COSTABSIZE * 3))]; 
    }

    return 0.0f;
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
FMOD_INLINE const float DSPFFT::sine(float x)
{
    return cosine(x - 0.25f);
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
FMOD_INLINE const unsigned int DSPFFT::reverse(unsigned int val, int bits)
{
    unsigned int retn = 0;
  
    while (bits--)
    {
        retn <<= 1;
        retn |= (val & 1);
        val >>= 1;
    }
  
    return (retn);
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
// VERIFY: 3c7b15cd manual
// OFFSET: 0x00918D70
FMOD_RESULT DSPFFT::process(int bits)
{
    int    count, count2, count3;
    unsigned        i1;		/* going to right shift this */
    int             i2, i3, i4, y;
    int             fftlen = 1 << bits;
    float           a1, a2, b1, b2, z1, z2;
    float           oneoverN= 1.0f / fftlen;

    i1 = fftlen / 2;
    i2 = 1;
    
    /* perform the butterfly's */

    for (count = 0; count < bits; count++)
    {
        i3 = 0;
        i4 = i1;

        for (count2 = 0; count2 < i2; count2++)
	    {
	        y  = reverse(i3 / (int)i1, bits);

            z1 =  cosine((float)y * oneoverN);
            z2 =   -sine((float)y * oneoverN);

	        for (count3 = i3; count3 < i4; count3++)
	        {
	            a1 = mFFTBuffer[count3].re;
	            a2 = mFFTBuffer[count3].im;

	            b1 = (z1 * mFFTBuffer[count3+i1].re) - (z2 * mFFTBuffer[count3+i1].im);
	            b2 = (z2 * mFFTBuffer[count3+i1].re) + (z1 * mFFTBuffer[count3+i1].im);

	            mFFTBuffer[count3].re = a1 + b1;
	            mFFTBuffer[count3].im = a2 + b2;

	            mFFTBuffer[count3+i1].re = a1 - b1;
	            mFFTBuffer[count3+i1].im = a2 - b2;
	        }

	        i3 += (i1 << 1);
	        i4 += (i1 << 1);
	    }

        i1 >>= 1;
        i2 <<= 1;
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
// VERIFY: 88e9109b manual
// OFFSET: 0x009190A0
FMOD_RESULT DSPFFT::getSpectrum(float *pcmbuffer, unsigned int pcmposition, unsigned int pcmlength, float *spectrum, int length, int channel, int numchannels, FMOD_DSP_FFT_WINDOW windowtype)
{
    int count, bits, bitslength, nyquist;
	
    bitslength = length;
    bits = 0;
    while (bitslength > 1)
    {
        bitslength >>= 1;
        bits++;
    }

    /*
        Convert the PCM data into complex data.
    */
    switch (windowtype)
    {
        case FMOD_DSP_FFT_WINDOW_TRIANGLE:
        {
            for (count = 0; count < length; count++)
            {
                float window;
                float percent = (float)count / (float)length;

                window = (percent * 2.0f) - 1.0f;   /* -1.0 to 0.0 to 1.0 */
                window = FMOD_ABS(window);          /*  1.0 to 0.0 to 1.0 */
                window = 1.0f - window;             /*  0.0 to 1.0 to 1.0 */

                mFFTBuffer[count].re = pcmbuffer[(pcmposition * numchannels) + channel] * window;
                mFFTBuffer[count].re /= (float)length;
                mFFTBuffer[count].im = 0.00000001f;  /* Giving it a very small number besides 0 avoids domain exceptions = big cpu usage */

                pcmposition++;
                if (pcmposition >= pcmlength)
                {
                    pcmposition = 0;
                }
            }
            break;
        }
        case FMOD_DSP_FFT_WINDOW_HAMMING:
        {
            for (count = 0; count < length; count++)
            {
                float window;
                float percent = (float)count / (float)length;

                window = 0.54f - (0.46f * cosine(percent) );

                mFFTBuffer[count].re = pcmbuffer[(pcmposition * numchannels) + channel] * window;
                mFFTBuffer[count].re /= (float)length;
                mFFTBuffer[count].im = 0.00000001f;  /* Giving it a very small number besides 0 avoids domain exceptions = big cpu usage */

                pcmposition++;
                if (pcmposition >= pcmlength)
                {
                    pcmposition = 0;
                }
            }
            break;
        }
        case FMOD_DSP_FFT_WINDOW_HANNING:
        {
            for (count = 0; count < length; count++)
            {
                float window;
                float percent = (float)count / (float)length;

                window = 0.5f *  (1.0f  - cosine(percent) );

                mFFTBuffer[count].re = pcmbuffer[(pcmposition * numchannels) + channel] * window;
                mFFTBuffer[count].re /= (float)length;
                mFFTBuffer[count].im = 0.00000001f;  /* Giving it a very small number besides 0 avoids domain exceptions = big cpu usage */

                pcmposition++;
                if (pcmposition >= pcmlength)
                {
                    pcmposition = 0;
                }
            }
            break;
        }
        case FMOD_DSP_FFT_WINDOW_BLACKMAN:
        {
            for (count = 0; count < length; count++)
            {
                float window;
                float percent = (float)count / (float)length;

                window = 0.42f - (0.5f  * cosine(percent) ) + (0.08f * cosine(2.0f * percent) );

                mFFTBuffer[count].re = pcmbuffer[(pcmposition * numchannels) + channel] * window;
                mFFTBuffer[count].re /= (float)length;
                mFFTBuffer[count].im = 0.00000001f;  /* Giving it a very small number besides 0 avoids domain exceptions = big cpu usage */

                pcmposition++;
                if (pcmposition >= pcmlength)
                {
                    pcmposition = 0;
                }
            }
            break;
        }
        case FMOD_DSP_FFT_WINDOW_BLACKMANHARRIS:
        {
            float a0 = 0.35875f;
            float a1 = 0.48829f;
            float a2 = 0.14128f;
            float a3 = 0.01168f;

            for (count = 0; count < length; count++)
            {
                float window;
                float percent = (float)count / (float)length;
 
                window = a0 - (a1 * cosine(1.0f * percent) ) +
                              (a2 * cosine(2.0f * percent) ) -
                              (a3 * cosine(3.0f * percent) );

                mFFTBuffer[count].re = pcmbuffer[(pcmposition * numchannels) + channel] * window;
                mFFTBuffer[count].re /= (float)length;
                mFFTBuffer[count].im = 0.00000001f;  /* Giving it a very small number besides 0 avoids domain exceptions = big cpu usage */

                pcmposition++;
                if (pcmposition >= pcmlength)
                {
                    pcmposition = 0;
                }
            }
            break;
        }
        case FMOD_DSP_FFT_WINDOW_RECT:
        default:
        {
            for (count = 0; count < length; count++)
            {
                mFFTBuffer[count].re = pcmbuffer[(pcmposition * numchannels) + channel];
                mFFTBuffer[count].re /= (float)length;
                mFFTBuffer[count].im = 0.00000001f;  /* Giving it a very small number besides 0 avoids domain exceptions = big cpu usage */

                pcmposition++;
                if (pcmposition >= pcmlength)
                {
                    pcmposition = 0;
                }
            }
            break;
        }
    };
    
    /*
        Do the FFT
    */
    process(bits);

    /*
        Now prepare the data into a readable format
    */
    nyquist = length / 2;
    for (count=0; count < nyquist-1; count++)
	{
        float magnitude;
        int n = count;

        n = reverse(n, bits);
        
        #ifdef FMOD_NO_FPU
        {
            FMOD_VECTOR v;

            v.x = mFFTBuffer[n].re;
            v.y = mFFTBuffer[n].im;
            v.z = 0.0f;
            
            magnitude = FMOD_Vector_GetLengthFast(&v);
        }
        #else
        magnitude = FMOD_SQRT((mFFTBuffer[n].re * mFFTBuffer[n].re) + (mFFTBuffer[n].im * mFFTBuffer[n].im));
        #endif

    //  phaseangle = (float)atan2(v.x, v.y);

        magnitude *= 2.5f;      /* This brings it roughly to the same db level as the input.  Not sure why though */

        if (magnitude > 1.0f)
        {
            magnitude = 1.0f;
        }

        spectrum[count] = magnitude;
    }

	return FMOD_OK;
}

}

#endif

