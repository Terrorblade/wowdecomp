#include "fmod_settings.h"

#include "fmod_historybuffer_pool.h"
#include "fmod_systemi.h"

namespace FMOD
{

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
HistoryBufferPool::HistoryBufferPool()
{
    mBufferSize = 0;
    mMaxHistoryBuffers = 0;
    mBufferPool = 0;
    mMemoryBlock = 0;
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
FMOD_RESULT HistoryBufferPool::init(int multichannelbuffercount, int channelsperbuffer)
{
    mBufferSize = FMOD_HISTORYBUFFERLEN * sizeof(float);

#if !defined(PLATFORM_PS3_SPU) && (defined(PLATFORM_PS3) || defined(PLATFORM_WINDOWS_PS3MODE))
        mBufferSize += 16; /* for alignment */
#endif

    mMaxHistoryBuffers = multichannelbuffercount * channelsperbuffer;
    if (mMaxHistoryBuffers == 0)
    {
        return FMOD_OK;
    }

    /* allocate memory block */
    int totalsize = mMaxHistoryBuffers * mBufferSize;
    mMemoryBlock = FMOD_Memory_Alloc(totalsize);
    if (!mMemoryBlock)
    {
        release();
        return FMOD_ERR_MEMORY;
    }

    /* allocate array of BufferInfos to keep track of data */
    mBufferPool = (BufferInfo*)FMOD_Memory_Alloc(mMaxHistoryBuffers * sizeof(BufferInfo));
    if (!mBufferPool)
    {
        release();
        return FMOD_ERR_MEMORY;
    }

    int i;
    for (i = 0; i < mMaxHistoryBuffers; ++i)
    {
        mBufferPool[i].numchannels = 0;
        mBufferPool[i].buffermemory = (float *)((FMOD_UINT_NATIVE)mMemoryBlock + (mBufferSize * i));
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
FMOD_RESULT HistoryBufferPool::alloc(float **historybuffer, int numchannels)
{
    if (!historybuffer || !numchannels)
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *historybuffer = 0;

    /* find free space for this buffer */
    int bufferindex;
    for (bufferindex = 0; bufferindex < mMaxHistoryBuffers; ++bufferindex)
    {
        int count;
        for (count = bufferindex; (count < (bufferindex + numchannels)); count++)
        {
            if (mBufferPool[count].numchannels != 0 ||
                mMaxHistoryBuffers <= count)
            {
                bufferindex = count;
                break;
            }
        }
        if ((count - bufferindex) == numchannels)
        {
            break;
        }
    }

    if (bufferindex < mMaxHistoryBuffers)
    {
        int count;
        for (count = bufferindex; count < (bufferindex + numchannels); count++)
        {
            if (mMaxHistoryBuffers < count)
            {
                return FMOD_ERR_INTERNAL; //PAS
            }
            mBufferPool[count].numchannels = numchannels;
        }

        *historybuffer = mBufferPool[bufferindex].buffermemory;
        /* intialize requested memory block to zero */
        FMOD_memset(*historybuffer, 0, mBufferSize * numchannels);
    }
    else
    {
        /* not in pool */
        *historybuffer = (float*)FMOD_Memory_Calloc(mBufferSize * numchannels);
    }

    if (!*historybuffer)
    {
        return FMOD_ERR_MEMORY;
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
FMOD_RESULT HistoryBufferPool::free(float *historybuffer)
{    
    if (mMaxHistoryBuffers != 0)
    {
        float *poolstart = mBufferPool[0].buffermemory;
        float *poolend   = mBufferPool[mMaxHistoryBuffers - 1].buffermemory;

        /* check history buffer is in pool*/
        if (poolstart <= historybuffer && historybuffer <= poolend)
        {
            /* determine the bufferindex of this block */
            int bufferindex = static_cast<int>(((FMOD_UINT_NATIVE)historybuffer - (FMOD_UINT_NATIVE)poolstart) / mBufferSize);
            int numchannels = mBufferPool[bufferindex].numchannels;
            
            int count;
            for (count = bufferindex; count < (bufferindex + numchannels); count++)
            {
                if (mMaxHistoryBuffers < count ||
                    mBufferPool[count].numchannels != numchannels ||
                    poolend < mBufferPool[count].buffermemory ) 
                {
                    return FMOD_ERR_INTERNAL; //PAS
                }
                mBufferPool[count].numchannels = 0;
            }
            return FMOD_OK;
        }
    }

    /* not in pool */
    FMOD_Memory_Free(historybuffer);
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
FMOD_RESULT HistoryBufferPool::release()
{
    if (mBufferPool)
    {
        FMOD_Memory_Free(mBufferPool);
        mBufferPool = 0;
    }
    if (mMemoryBlock)
    {
        FMOD_Memory_Free(mMemoryBlock);
        mMemoryBlock = 0;
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

}
