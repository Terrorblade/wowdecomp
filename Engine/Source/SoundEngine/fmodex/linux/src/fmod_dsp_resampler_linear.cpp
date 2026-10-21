#if defined(WOWDECOMP_PLATFORM_LINUX)

#include "fmod_settings.h"

#include "fmod_dsp_resampler_linear.h"
#include "fmod_os_misc.h"

#define OO4GIG (1.0f / 4294967296.0f)

#ifdef FMOD_SUPPORT_SIMD
extern "C"
{
    void FMOD_Resampler_Linear_PCM16_Mono(float *out, int outlength, void *src, FMOD_UINT64P *position, FMOD_SINT64P *speed);
    void FMOD_Resampler_Linear_PCM16_Stereo(float *out, int outlength, void *src, FMOD_UINT64P *position, FMOD_SINT64P *speed);
}
#endif

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]
    FMOD_OK

	[REMARKS]

    [PLATFORMS]
    Win32, Win64, Linux, Macintosh, XBox, PlayStation 2, GameCube

	[SEE_ALSO]
]
*/
void FMOD_Resampler_Linear(float *out, int outlength, void *src, FMOD_SOUND_FORMAT srcformat, FMOD_UINT64P *position, FMOD_SINT64P *speed, int channels)
{
    float scale = 1.0f;
    bool  supportssimd;

    supportssimd = FMOD_OS_SupportsSIMD();
    
    switch (srcformat)
    {
        /*
            8 BIT
        */
        case FMOD_SOUND_FORMAT_PCM8:
        {
            signed char *inptr = (signed char *)src;

            scale /= (float)(1<<7);

            if (channels == 1)
            {
                float f;
                float a, b, r1,r2,r3,r4;
                int len;

                len = outlength >> 2;
                while (len)
                {
                    f = (float)position->mLo * OO4GIG;
                    a = (float)inptr[position->mHi + 0] * scale;
                    b = (float)inptr[position->mHi + 1] * scale;
                    r1 = (a * (1.0f - f)) + (b * f);
                    position->mValue += speed->mValue;

                    f = (float)position->mLo * OO4GIG;
                    a = (float)inptr[position->mHi + 0] * scale;
                    b = (float)inptr[position->mHi + 1] * scale;
                    r2 = (a * (1.0f - f)) + (b * f);
                    position->mValue += speed->mValue;

                    f = (float)position->mLo * OO4GIG;
                    a = (float)inptr[position->mHi + 0] * scale;
                    b = (float)inptr[position->mHi + 1] * scale;
                    r3 = (a * (1.0f - f)) + (b * f);
                    position->mValue += speed->mValue;

                    f = (float)position->mLo * OO4GIG;
                    a = (float)inptr[position->mHi + 0] * scale;
                    b = (float)inptr[position->mHi + 1] * scale;
                    r4 = (a * (1.0f - f)) + (b * f);
                    position->mValue += speed->mValue;

                    out[0] = r1;
                    out[1] = r2;
                    out[2] = r3;
                    out[3] = r4;
                    len--;
                    out+=4;
                } 

                len = outlength & 3;
                while (len)
                {
                    f = position->mLo * OO4GIG;
                    a = (float)inptr[position->mHi + 0] * scale;
                    b = (float)inptr[position->mHi + 1] * scale;
                    r1 = (a * (1.0f - f)) + (b * f);

                    *out++ = r1;

                    position->mValue += speed->mValue;
                    len--;
                } 
            }
            else if (channels == 2)
            {
                float f;
                float l_a,l_b, l_r1,l_r2,l_r3,l_r4;
                float r_a,r_b, r_r1,r_r2,r_r3,r_r4;
                int len;

                len = outlength >> 2;
                while (len)
                {
                    f = (float)position->mLo * OO4GIG;
                    l_a = (float)inptr[position->mHi * 2 + 0] * scale;
                    l_b = (float)inptr[position->mHi * 2 + 2] * scale;
                    r_a = (float)inptr[position->mHi * 2 + 1] * scale;
                    r_b = (float)inptr[position->mHi * 2 + 3] * scale;
                    l_r1 = (l_a * (1.0f - f)) + (l_b * f);
                    r_r1 = (r_a * (1.0f - f)) + (r_b * f);
                    position->mValue += speed->mValue;

                    f = (float)position->mLo * OO4GIG;
                    l_a = (float)inptr[position->mHi * 2 + 0] * scale;
                    l_b = (float)inptr[position->mHi * 2 + 2] * scale;
                    r_a = (float)inptr[position->mHi * 2 + 1] * scale;
                    r_b = (float)inptr[position->mHi * 2 + 3] * scale;
                    l_r2 = (l_a * (1.0f - f)) + (l_b * f);
                    r_r2 = (r_a * (1.0f - f)) + (r_b * f);
                    position->mValue += speed->mValue;

                    f = (float)position->mLo * OO4GIG;
                    l_a = (float)inptr[position->mHi * 2 + 0] * scale;
                    l_b = (float)inptr[position->mHi * 2 + 2] * scale;
                    r_a = (float)inptr[position->mHi * 2 + 1] * scale;
                    r_b = (float)inptr[position->mHi * 2 + 3] * scale;
                    l_r3 = (l_a * (1.0f - f)) + (l_b * f);
                    r_r3 = (r_a * (1.0f - f)) + (r_b * f);
                    position->mValue += speed->mValue;

                    f = (float)position->mLo * OO4GIG;
                    l_a = (float)inptr[position->mHi * 2 + 0] * scale;
                    l_b = (float)inptr[position->mHi * 2 + 2] * scale;
                    r_a = (float)inptr[position->mHi * 2 + 1] * scale;
                    r_b = (float)inptr[position->mHi * 2 + 3] * scale;
                    l_r4 = (l_a * (1.0f - f)) + (l_b * f);
                    r_r4 = (r_a * (1.0f - f)) + (r_b * f);
                    position->mValue += speed->mValue;

                    out[0] = l_r1;
                    out[1] = r_r1;
                    out[2] = l_r2;
                    out[3] = r_r2;
                    out[4] = l_r3;
                    out[5] = r_r3;
                    out[6] = l_r4;
                    out[7] = r_r4;

                    len--;
                    out+=8;
                } 

                len = outlength & 3;
                while (len)
                {
                    f = position->mLo * OO4GIG;
                    l_a = (float)inptr[position->mHi * 2 + 0] * scale;
                    l_b = (float)inptr[position->mHi * 2 + 2] * scale;
                    r_a = (float)inptr[position->mHi * 2 + 1] * scale;
                    r_b = (float)inptr[position->mHi * 2 + 3] * scale;
                    l_r1 = (l_a * (1.0f - f)) + (l_b * f);
                    r_r1 = (r_a * (1.0f - f)) + (r_b * f);

                    *out++ = l_r1;
                    *out++ = r_r1;

                    position->mValue += speed->mValue;
                    len--;
                } 
            }
            else
            {
                while (outlength)
                {
                    int count;
                    float f = position->mLo * OO4GIG;

                    for (count = 0; count < channels; count++)
                    {
                        float p0 = (float)inptr[((position->mHi + 0) * channels) + count] * scale;
                        float p1 = (float)inptr[((position->mHi + 1) * channels) + count] * scale;
                        float r;

                        r = (p0 * (1.0f - f)) + (p1 * f);
                        *out++ = r;
                    }
                    position->mValue += speed->mValue;
                    outlength--;
                } 
            }
            break;
        }

        /*
            16 BIT
        */
        case FMOD_SOUND_FORMAT_PCM16:
        {
            signed short *inptr = (signed short *)src;

            if (channels == 1)
            {
                #ifdef FMOD_SUPPORT_SIMD
                if (supportssimd)
                {
                    FMOD_Resampler_Linear_PCM16_Mono(out, outlength, src, position, speed);
                }
                else
                #endif
                {
                    float f;
                    float a, b, r1,r2,r3,r4;
                    int len;

                    scale /= (float)(1<<15);
                    len = outlength >> 2;
                    while (len)
                    {
                        f = (float)position->mLo * OO4GIG;
                        a = (float)inptr[position->mHi + 0] * scale;
                        b = (float)inptr[position->mHi + 1] * scale;
                        r1 = (a * (1.0f - f)) + (b * f);
                        position->mValue += speed->mValue;

                        f = (float)position->mLo * OO4GIG;
                        a = (float)inptr[position->mHi + 0] * scale;
                        b = (float)inptr[position->mHi + 1] * scale;
                        r2 = (a * (1.0f - f)) + (b * f);
                        position->mValue += speed->mValue;

                        f = (float)position->mLo * OO4GIG;
                        a = (float)inptr[position->mHi + 0] * scale;
                        b = (float)inptr[position->mHi + 1] * scale;
                        r3 = (a * (1.0f - f)) + (b * f);
                        position->mValue += speed->mValue;

                        f = (float)position->mLo * OO4GIG;
                        a = (float)inptr[position->mHi + 0] * scale;
                        b = (float)inptr[position->mHi + 1] * scale;
                        r4 = (a * (1.0f - f)) + (b * f);
                        position->mValue += speed->mValue;

                        out[0] = r1;
                        out[1] = r2;
                        out[2] = r3;
                        out[3] = r4;
                        len--;
                        out+=4;
                    } 

                    len = outlength & 3;
                    while (len)
                    {
                        f = position->mLo * OO4GIG;
                        a = (float)inptr[position->mHi + 0] * scale;
                        b = (float)inptr[position->mHi + 1] * scale;
                        r1 = (a * (1.0f - f)) + (b * f);

                        *out++ = r1;

                        position->mValue += speed->mValue;
                        len--;
                    }
                }
            }
            else if (channels == 2)
            {
                #ifdef FMOD_SUPPORT_SIMD
                if (supportssimd)
                {
                    FMOD_Resampler_Linear_PCM16_Stereo(out, outlength, src, position, speed);
                }
                else
                #endif
                {
                    float f;
                    float l_a,l_b, l_r1,l_r2,l_r3,l_r4;
                    float r_a,r_b, r_r1,r_r2,r_r3,r_r4;
                    int len;

                    scale /= (float)(1<<15);
                    len = outlength >> 2;

                    while (len)
                    {
                        f = (float)position->mLo * OO4GIG;
                        l_a = (float)inptr[position->mHi * 2 + 0] * scale;
                        l_b = (float)inptr[position->mHi * 2 + 2] * scale;
                        r_a = (float)inptr[position->mHi * 2 + 1] * scale;
                        r_b = (float)inptr[position->mHi * 2 + 3] * scale;
                        l_r1 = (l_a * (1.0f - f)) + (l_b * f);
                        r_r1 = (r_a * (1.0f - f)) + (r_b * f);
                        position->mValue += speed->mValue;

                        f = (float)position->mLo * OO4GIG;
                        l_a = (float)inptr[position->mHi * 2 + 0] * scale;
                        l_b = (float)inptr[position->mHi * 2 + 2] * scale;
                        r_a = (float)inptr[position->mHi * 2 + 1] * scale;
                        r_b = (float)inptr[position->mHi * 2 + 3] * scale;
                        l_r2 = (l_a * (1.0f - f)) + (l_b * f);
                        r_r2 = (r_a * (1.0f - f)) + (r_b * f);
                        position->mValue += speed->mValue;

                        f = (float)position->mLo * OO4GIG;
                        l_a = (float)inptr[position->mHi * 2 + 0] * scale;
                        l_b = (float)inptr[position->mHi * 2 + 2] * scale;
                        r_a = (float)inptr[position->mHi * 2 + 1] * scale;
                        r_b = (float)inptr[position->mHi * 2 + 3] * scale;
                        l_r3 = (l_a * (1.0f - f)) + (l_b * f);
                        r_r3 = (r_a * (1.0f - f)) + (r_b * f);
                        position->mValue += speed->mValue;

                        f = (float)position->mLo * OO4GIG;
                        l_a = (float)inptr[position->mHi * 2 + 0] * scale;
                        l_b = (float)inptr[position->mHi * 2 + 2] * scale;
                        r_a = (float)inptr[position->mHi * 2 + 1] * scale;
                        r_b = (float)inptr[position->mHi * 2 + 3] * scale;
                        l_r4 = (l_a * (1.0f - f)) + (l_b * f);
                        r_r4 = (r_a * (1.0f - f)) + (r_b * f);
                        position->mValue += speed->mValue;

                        out[0] = l_r1;
                        out[1] = r_r1;
                        out[2] = l_r2;
                        out[3] = r_r2;
                        out[4] = l_r3;
                        out[5] = r_r3;
                        out[6] = l_r4;
                        out[7] = r_r4;
                        len--;
                        out+=8;
                    } 

                    len = outlength & 3;
                    while (len)
                    {
                        f = position->mLo * OO4GIG;
                        l_a = (float)inptr[position->mHi * 2 + 0] * scale;
                        l_b = (float)inptr[position->mHi * 2 + 2] * scale;
                        r_a = (float)inptr[position->mHi * 2 + 1] * scale;
                        r_b = (float)inptr[position->mHi * 2 + 3] * scale;
                        l_r1 = (l_a * (1.0f - f)) + (l_b * f);
                        r_r1 = (r_a * (1.0f - f)) + (r_b * f);

                        out[0] = l_r1;
                        out[1] = r_r1;

                        position->mValue += speed->mValue;
                        len--;
                        out+=2;
                    } 
                }
            }
            else
            {
                scale /= (float)(1<<15);

                while (outlength)
                {
                    int count;
                    float f = position->mLo * OO4GIG;

                    for (count = 0; count < channels; count++)
                    {
                        float p0 = (float)inptr[((position->mHi + 0) * channels) + count] * scale;
                        float p1 = (float)inptr[((position->mHi + 1) * channels) + count] * scale;
                        float r;

                        r = (p0 * (1.0f - f)) + (p1 * f);
                        *out++ = r;
                    }
                    position->mValue += speed->mValue;
                    outlength--;
                } 
            }
            break;
        }

        /*
            24 BIT
        */
        case FMOD_SOUND_FORMAT_PCM24:
        {
            FMOD_INT24 *inptr = (FMOD_INT24 *)src;

            scale /= (float)(1<<23);

            if (channels == 1)
            {
                while (outlength)
                {
                    int count;
                    float r;
                    float f = position->mLo * OO4GIG;

                    for (count = 0; count < channels; count++)
                    {
                        FMOD_INT24 *s0 = &inptr[position->mHi + 0];
                        FMOD_INT24 *s1 = &inptr[position->mHi + 1];
                        float  p0 = (float)((int)(((unsigned int)s0->val[0] <<  8) | ((unsigned int)s0->val[1] << 16) | ((unsigned int)s0->val[2] << 24)) >> 8) * scale;
                        float  p1 = (float)((int)(((unsigned int)s1->val[0] <<  8) | ((unsigned int)s1->val[1] << 16) | ((unsigned int)s1->val[2] << 24)) >> 8) * scale;

                        r = (p0 * (1.0f - f)) + (p1 * f);

                        *out++ = r;
                    }
                    position->mValue += speed->mValue;
                    outlength--;
                } 
            }
            else
            {
                while (outlength)
                {
                    int count;
                    float r;
                    float f = position->mLo * OO4GIG;

                    for (count = 0; count < channels; count++)
                    {
                        FMOD_INT24 *s0 = &inptr[((position->mHi + 0) * channels) + count];
                        FMOD_INT24 *s1 = &inptr[((position->mHi + 1) * channels) + count];
                        float  p0 = (float)((int)(((unsigned int)s0->val[0] <<  8) | ((unsigned int)s0->val[1] << 16) | ((unsigned int)s0->val[2] << 24)) >> 8) * scale;
                        float  p1 = (float)((int)(((unsigned int)s1->val[0] <<  8) | ((unsigned int)s1->val[1] << 16) | ((unsigned int)s1->val[2] << 24)) >> 8) * scale;

                        r = (p0 * (1.0f - f)) + (p1 * f);

                        *out++ = r;
                    }
                    position->mValue += speed->mValue;
                    outlength--;
                } 
            }
            break;
        }

        /*
            32 BIT
        */
        case FMOD_SOUND_FORMAT_PCM32:
        {
            signed int *inptr = (signed int *)src;

            scale /= (float)(1<<31);

            if (channels == 1)
            {
                float f;
                float a, b, r1,r2,r3,r4;
                int len;

                len = outlength >> 2;
                while (len)
                {
                    f = (float)position->mLo * OO4GIG;
                    a = (float)inptr[position->mHi + 0] * scale;
                    b = (float)inptr[position->mHi + 1] * scale;
                    r1 = (a * (1.0f - f)) + (b * f);
                    position->mValue += speed->mValue;

                    f = (float)position->mLo * OO4GIG;
                    a = (float)inptr[position->mHi + 0] * scale;
                    b = (float)inptr[position->mHi + 1] * scale;
                    r2 = (a * (1.0f - f)) + (b * f);
                    position->mValue += speed->mValue;

                    f = (float)position->mLo * OO4GIG;
                    a = (float)inptr[position->mHi + 0] * scale;
                    b = (float)inptr[position->mHi + 1] * scale;
                    r3 = (a * (1.0f - f)) + (b * f);
                    position->mValue += speed->mValue;

                    f = (float)position->mLo * OO4GIG;
                    a = (float)inptr[position->mHi + 0] * scale;
                    b = (float)inptr[position->mHi + 1] * scale;
                    r4 = (a * (1.0f - f)) + (b * f);
                    position->mValue += speed->mValue;

                    out[0] = r1;
                    out[1] = r2;
                    out[2] = r3;
                    out[3] = r4;
                    len--;
                    out+=4;
                } 

                len = outlength & 3;
                while (len)
                {
                    f = position->mLo * OO4GIG;
                    a = (float)inptr[position->mHi + 0] * scale;
                    b = (float)inptr[position->mHi + 1] * scale;
                    r1 = (a * (1.0f - f)) + (b * f);

                    *out++ = r1;

                    position->mValue += speed->mValue;
                    len--;
                } 
            }
            else
            {
                while (outlength)
                {
                    int count;
                    float f = position->mLo * OO4GIG;

                    for (count = 0; count < channels; count++)
                    {
                        float p0 = (float)inptr[((position->mHi + 0) * channels) + count] * scale;
                        float p1 = (float)inptr[((position->mHi + 1) * channels) + count] * scale;
                        float r;

                        r = (p0 * (1.0f - f)) + (p1 * f);
                        *out++ = r;
                    }
                    position->mValue += speed->mValue;
                    outlength--;
                } 
            }
            break;
        }

        /*
            FLOATING POINT.
        */
        case FMOD_SOUND_FORMAT_PCMFLOAT:
        {
            float *inptr = (float *)src;

            if (channels == 1)
            {
                float f;
                float a, b, r1,r2,r3,r4;
                int len;

                len = outlength >> 2;
                while (len)
                {
                    f = (float)position->mLo * OO4GIG;
                    a = inptr[position->mHi + 0];
                    b = inptr[position->mHi + 1];
                    r1 = (a * (1.0f - f)) + (b * f);
                    position->mValue += speed->mValue;

                    f = (float)position->mLo * OO4GIG;
                    a = inptr[position->mHi + 0];
                    b = inptr[position->mHi + 1];
                    r2 = (a * (1.0f - f)) + (b * f);
                    position->mValue += speed->mValue;

                    f = (float)position->mLo * OO4GIG;
                    a = inptr[position->mHi + 0];
                    b = inptr[position->mHi + 1];
                    r3 = (a * (1.0f - f)) + (b * f);
                    position->mValue += speed->mValue;

                    f = (float)position->mLo * OO4GIG;
                    a = inptr[position->mHi + 0];
                    b = inptr[position->mHi + 1];
                    r4 = (a * (1.0f - f)) + (b * f);
                    position->mValue += speed->mValue;

                    out[0] = r1;
                    out[1] = r2;
                    out[2] = r3;
                    out[3] = r4;
                    len--;
                    out+=4;
                } 

                len = outlength & 3;
                while (len)
                {
                    f = position->mLo * OO4GIG;
                    a = inptr[position->mHi + 0];
                    b = inptr[position->mHi + 1];
                    r1 = (a * (1.0f - f)) + (b * f);

                    *out++ = r1;

                    position->mValue += speed->mValue;
                    len--;
                } 
            }
            else
            {
                while (outlength)
                {
                    int count;
                    float f = position->mLo * OO4GIG;

                    for (count = 0; count < channels; count++)
                    {
                        float p0 = inptr[((position->mHi + 0) * channels) + count];
                        float p1 = inptr[((position->mHi + 1) * channels) + count];
                        float r;

                        r = (p0 * (1.0f - f)) + (p1 * f);
                        *out++ = r;
                    }
                    position->mValue += speed->mValue;
                    outlength--;
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



#endif
