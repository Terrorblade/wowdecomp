#if defined(WOWDECOMP_PLATFORM_POSIX)

#include "fmod_settings.h"

#ifdef FMOD_SUPPORT_DISTORTION

#include "fmod.h"
#include "fmod_dspi.h"
#include "fmod_dsp_distortion.h"
#include "fmod_systemi.h"
#include "fmod_os_misc.h"

#include <stdio.h>

namespace FMOD
{

FMOD_DSP_DESCRIPTION_EX dspdistortion;

#ifdef PLUGIN_EXPORTS

#ifdef __cplusplus
extern "C" {
#endif

    /*
        FMODGetDSPDescription is mandantory for every fmod plugin.  This is the symbol the registerplugin function searches for.
        Must be declared with F_API to make it export as stdcall.
    */
    F_DECLSPEC F_DLLEXPORT FMOD_DSP_DESCRIPTION_EX * F_API FMODGetDSPDescriptionEx()
    {
        return DSPDistortion::getDescriptionEx();
    }

#ifdef __cplusplus
}
#endif

#endif  /* PLUGIN_EXPORTS */

FMOD_DSP_PARAMETERDESC dspdistortion_param[1] =
{
    { 0.0f,     1.0f,   0.5, "Level",   "",     "Distortion value.  0.0 to 1.0.  Default = 0.5." },
};

#ifdef FMOD_SUPPORT_SIMD
extern "C"
{
    void FMOD_DSP_Distortion_SIMD(float * inbuffer, float * outbuffer, unsigned int length, int inchannels, int outchannels, float k);
}
#endif

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]
    Win32, Win64, Linux, Macintosh, XBox, PlayStation 2, GameCube

	[SEE_ALSO]
]
*/
FMOD_DSP_DESCRIPTION_EX *DSPDistortion::getDescriptionEx()
{
    FMOD_memset(&dspdistortion, 0, sizeof(FMOD_DSP_DESCRIPTION_EX));

    FMOD_strcpy(dspdistortion.name, "FMOD Distortion");
    dspdistortion.version       = 0x00010100;
    dspdistortion.create        = DSPDistortion::createCallback;
    dspdistortion.release       = DSPDistortion::releaseCallback;
    dspdistortion.reset         = DSPDistortion::resetCallback;
    dspdistortion.read          = DSPDistortion::readCallback;

    dspdistortion.numparameters = sizeof(dspdistortion_param) / sizeof(dspdistortion_param[0]);
    dspdistortion.paramdesc     = dspdistortion_param;
    dspdistortion.setparameter  = DSPDistortion::setParameterCallback;
    dspdistortion.getparameter  = DSPDistortion::getParameterCallback;

    dspdistortion.mType         = FMOD_DSP_TYPE_DISTORTION;
    dspdistortion.mCategory     = FMOD_DSP_CATEGORY_FILTER;
    dspdistortion.mSize         = sizeof(DSPDistortion);

    return &dspdistortion;
}



/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]
    Win32, Win64, Linux, Macintosh, XBox, PlayStation 2, GameCube

	[SEE_ALSO]
]
*/
FMOD_RESULT DSPDistortion::createInternal()
{
    int count;

    init();

    mSupportsSIMD = FMOD_OS_SupportsSIMD();
    
    for (count = 0; count < mDescription.numparameters; count++)
    {
        FMOD_RESULT result;
        
        result = setParameter(count, mDescription.paramdesc[count].defaultval);
        if (result != FMOD_OK)
        {
            return result;
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
    Win32, Win64, Linux, Macintosh, XBox, PlayStation 2, GameCube

	[SEE_ALSO]
]
*/
FMOD_RESULT DSPDistortion::releaseInternal()
{
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
    Win32, Win64, Linux, Macintosh, XBox, PlayStation 2, GameCube

	[SEE_ALSO]
]
*/
FMOD_RESULT DSPDistortion::resetInternal()
{
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
    Win32, Win64, Linux, Macintosh, XBox, PlayStation 2, GameCube

	[SEE_ALSO]
]
*/
FMOD_RESULT DSPDistortion::readInternal(float * inbuffer, float * outbuffer, unsigned int length, int inchannels, int outchannels)
{   
    float k;
    float level = mLevel;

    if (level < 1.0f)
    {
        k = 2.0f * level / (1.0f - level);
    }
    else
    {
        k = 2.0f * 0.9999f / (1.0f - 0.9999f);
    } 

    #ifdef FMOD_SUPPORT_SIMD
    if (mSupportsSIMD)
    {
        FMOD_DSP_Distortion_SIMD(inbuffer, outbuffer, length, inchannels, outchannels, k);
    }
    else
    #endif
    {
        unsigned int len;
        float *in = inbuffer;
        float *out = outbuffer;
        float kplus1 = k + 1.0f;

        len = (length * inchannels) >> 3;
        while (len)
        {
            out[0] = in[0] * kplus1 / (FMOD_FABS(in[0]) * k + 1.0f);
            out[1] = in[1] * kplus1 / (FMOD_FABS(in[1]) * k + 1.0f);
            out[2] = in[2] * kplus1 / (FMOD_FABS(in[2]) * k + 1.0f);
            out[3] = in[3] * kplus1 / (FMOD_FABS(in[3]) * k + 1.0f);
            out[4] = in[4] * kplus1 / (FMOD_FABS(in[4]) * k + 1.0f);
            out[5] = in[5] * kplus1 / (FMOD_FABS(in[5]) * k + 1.0f);
            out[6] = in[6] * kplus1 / (FMOD_FABS(in[6]) * k + 1.0f);
            out[7] = in[7] * kplus1 / (FMOD_FABS(in[7]) * k + 1.0f);
            in += 8;
            out += 8;
            len--;
        }

        len = (length * inchannels) & 7;
        while (len)
        {
            out[0] = in[0] * kplus1 / (FMOD_FABS(in[0]) * k + 1.0f);
            in++;
            out++;
            len--;
        }
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
    Win32, Win64, Linux, Macintosh, XBox, PlayStation 2, GameCube

	[SEE_ALSO]
]
*/
FMOD_RESULT DSPDistortion::setParameterInternal(int index, float value)
{
    mLevel = value;

    return FMOD_OK;
}

/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]
    Win32, Win64, Linux, Macintosh, XBox, PlayStation 2, GameCube

	[SEE_ALSO]
]
*/
FMOD_RESULT DSPDistortion::getParameterInternal(int index, float *value, char *valuestr)
{
    *value = mLevel;
    sprintf(valuestr, "%.02f", mLevel);

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



/*
    ==============================================================================================================

    CALLBACK INTERFACE

    ==============================================================================================================
*/


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]
    Win32, Win64, Linux, Macintosh, XBox, PlayStation 2, GameCube

	[SEE_ALSO]
]
*/
FMOD_RESULT F_CALLBACK DSPDistortion::createCallback(FMOD_DSP_STATE *dsp)
{
    DSPDistortion *distortion = (DSPDistortion *)dsp;

    return distortion->createInternal();
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]
    Win32, Win64, Linux, Macintosh, XBox, PlayStation 2, GameCube

	[SEE_ALSO]
]
*/
FMOD_RESULT F_CALLBACK DSPDistortion::releaseCallback(FMOD_DSP_STATE *dsp)
{
    DSPDistortion *distortion = (DSPDistortion *)dsp;

    return distortion->releaseInternal();
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]
    Win32, Win64, Linux, Macintosh, XBox, PlayStation 2, GameCube

	[SEE_ALSO]
]
*/
FMOD_RESULT F_CALLBACK DSPDistortion::resetCallback(FMOD_DSP_STATE *dsp)
{
    DSPDistortion *distortion = (DSPDistortion *)dsp;

    return distortion->resetInternal();
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]
    Win32, Win64, Linux, Macintosh, XBox, PlayStation 2, GameCube

	[SEE_ALSO]
]
*/
FMOD_RESULT F_CALLBACK DSPDistortion::readCallback(FMOD_DSP_STATE *dsp, float *inbuffer, float *outbuffer, unsigned int length, int inchannels, int outchannels)
{
    DSPDistortion *distortion = (DSPDistortion *)dsp;

    return distortion->readInternal(inbuffer, outbuffer, length, inchannels, outchannels);
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]
    Win32, Win64, Linux, Macintosh, XBox, PlayStation 2, GameCube

	[SEE_ALSO]
]
*/
FMOD_RESULT F_CALLBACK DSPDistortion::setParameterCallback(FMOD_DSP_STATE *dsp, int index, float value)
{
    DSPDistortion *distortion = (DSPDistortion *)dsp;

    return distortion->setParameterInternal(index, value);
}


/*
[
	[DESCRIPTION]

	[PARAMETERS]
 
	[RETURN_VALUE]

	[REMARKS]

    [PLATFORMS]
    Win32, Win64, Linux, Macintosh, XBox, PlayStation 2, GameCube

	[SEE_ALSO]
]
*/
FMOD_RESULT F_CALLBACK DSPDistortion::getParameterCallback(FMOD_DSP_STATE *dsp, int index, float *value, char *valuestr)
{
    DSPDistortion *distortion = (DSPDistortion *)dsp;

    return distortion->getParameterInternal(index, value, valuestr);
}




}

#endif

#endif
