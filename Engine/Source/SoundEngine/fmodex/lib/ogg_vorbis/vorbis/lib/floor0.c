/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2007             *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: floor backend 0 implementation
 last mod: $Id: floor0.c 13293 2007-07-24 00:09:47Z xiphmont $

 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ogg/ogg.h>
#include "vorbis/codec.h"
#include "codec_internal.h"
#include "registry.h"
#include "lpc.h"
#include "lsp.h"
#include "codebook.h"
#include "scales.h"
#include "misc.h"
#include "os.h"

#include "misc.h"
#include <stdio.h>

typedef struct {
  int ln;
  int  m;
  int **linearmap;
  int  n[2];

  vorbis_info_floor0 *vi;

  ogg_int32_t bits;
  ogg_int32_t frames;
} vorbis_look_floor0;


/***********************************************/

// VERIFY: dfead52f auto
// OFFSET: 0x00471E90
static void floor0_free_info(vorbis_info_floor *i){
  vorbis_info_floor0 *info=(vorbis_info_floor0 *)i;
  if(info){
    FMOD_memset(info,0,sizeof(*info));
    _ogg_free(info);
  }
}

// VERIFY: 64c0bba2 auto
// OFFSET: 0x00471EC0
static void floor0_free_look(vorbis_look_floor *i){
  vorbis_look_floor0 *look=(vorbis_look_floor0 *)i;
  if(look){

    if(look->linearmap){

      if(look->linearmap[0])_ogg_free(look->linearmap[0]);
      if(look->linearmap[1])_ogg_free(look->linearmap[1]);

      _ogg_free(look->linearmap);
    }
    FMOD_memset(look,0,sizeof(*look));
    _ogg_free(look);
  }
}

// VERIFY: c702c4bd manual
// OFFSET: 0x00471F50
static vorbis_info_floor *floor0_unpack (vorbis_info *vi,oggpack_buffer *opb){
  codec_setup_info     *ci=vi->codec_setup;
  int j;

  vorbis_info_floor0 *info=_ogg_malloc(sizeof(*info));
  if(!info)return(NULL);
  info->order=FMOD_oggpack_read(opb,8);
  info->rate=FMOD_oggpack_read(opb,16);
  info->barkmap=FMOD_oggpack_read(opb,16);
  info->ampbits=FMOD_oggpack_read(opb,6);
  info->ampdB=FMOD_oggpack_read(opb,8);
  info->numbooks=FMOD_oggpack_read(opb,4)+1;

  if(info->order<1)goto err_out;
  if(info->rate<1)goto err_out;
  if(info->barkmap<1)goto err_out;
  if(info->numbooks<1)goto err_out;

  for(j=0;j<info->numbooks;j++){
    info->books[j]=FMOD_oggpack_read(opb,8);
    if(info->books[j]<0 || info->books[j]>=ci->books)goto err_out;
  }
  return(info);

 err_out:
  floor0_free_info(info);
  return(NULL);
}

/* initialize Bark scale and normalization lookups.  We could do this
   with static tables, but Vorbis allows a number of possible
   combinations, so it's best to do it computationally.

   The below is authoritative in terms of defining scale mapping.
   Note that the scale depends on the sampling rate as well as the
   linear block and mapping sizes */

// VERIFY: 00fff5cc manual
// OFFSET: 0x00472050
static int floor0_map_lazy_init(vorbis_block      *vb,
                                vorbis_info_floor *infoX,
                                vorbis_look_floor0 *look){
  if(!look->linearmap[vb->W]){
    vorbis_dsp_state   *vd=vb->vd;
    vorbis_info        *vi=vd->vi;
    codec_setup_info   *ci=vi->codec_setup;
    vorbis_info_floor0 *info=(vorbis_info_floor0 *)infoX;
    int W=vb->W;
    int n=ci->blocksizes[W]/2,j;

    /* we choose a scaling constant so that:
       floor(bark(rate/2-1)*C)=mapped-1
     floor(bark(rate/2)*C)=mapped */
    float scale=look->ln/toBARK(info->rate/2.f);

    /* the mapping from a linear scale to a smaller bark scale is
       straightforward.  We do *not* make sure that the linear mapping
       does not skip bark-scale bins; the decoder simply skips them and
       the encoder may do what it wishes in filling them.  They're
       necessary in some mapping combinations to keep the scale spacing
       accurate */
    look->linearmap[W]=_ogg_malloc((n+1)*sizeof(**look->linearmap));
    if(!look->linearmap[W])return(OV_EMEMORY);
    for(j=0;j<n;j++){
      int val=(int)(floor( toBARK((info->rate/2.f)/n*j)
                     *scale)); /* bark numbers represent band edges */
      if(val>=look->ln)val=look->ln-1; /* guard against the approximation */
      look->linearmap[W][j]=val;
    }
    look->linearmap[W][j]=-1;
    look->n[W]=n;
  }
  return(0);
}

// VERIFY: c2ffc530 auto
// OFFSET: 0x004721C0
static vorbis_look_floor *floor0_look(vorbis_dsp_state *vd,
                                      vorbis_info_floor *i){
  vorbis_info_floor0 *info=(vorbis_info_floor0 *)i;
  vorbis_look_floor0 *look=_ogg_calloc(1,sizeof(*look));
  if(!look)return(NULL);
  look->m=info->order;
  look->ln=info->barkmap;
  look->vi=info;

  look->linearmap=_ogg_calloc(2,sizeof(*look->linearmap));
  if(!look->linearmap)return(NULL);

  return look;
}

// VERIFY: c1702ef2 auto
// OFFSET: 0x00472220
static void *floor0_inverse1(vorbis_block *vb,vorbis_look_floor *i){
  vorbis_look_floor0 *look=(vorbis_look_floor0 *)i;
  vorbis_info_floor0 *info=look->vi;
  int j,k;

  int ampraw=FMOD_oggpack_read(&vb->opb,info->ampbits);
  if(ampraw>0){ /* also handles the -1 out of data case */
    ogg_int32_t maxval=(1<<info->ampbits)-1;
    float amp=(float)ampraw/maxval*info->ampdB;
    int booknum=FMOD_oggpack_read(&vb->opb,_FMOD_ilog(info->numbooks));

    if(booknum!=-1 && booknum<info->numbooks){ /* be paranoid */
      codec_setup_info  *ci=vb->vd->vi->codec_setup;
      codebook *b=ci->fullbooks+info->books[booknum];
      float last=0.f;

      /* the additional b->dim is a guard against any possible stack
         smash; b->dim is provably more than we can overflow the
         vector */
      float *lsp=_FMOD_vorbis_block_alloc(vb,sizeof(*lsp)*(look->m+b->dim+1));
      if(!lsp)return(NULL);

      for(j=0;j<look->m;j+=b->dim)
        if(FMOD_vorbis_book_decodev_set(b,lsp+j,&vb->opb,b->dim)==-1)goto eop;
      for(j=0;j<look->m;){
        for(k=0;k<b->dim;k++,j++)lsp[j]+=last;
        last=lsp[j-1];
      }

      lsp[look->m]=amp;
      return(lsp);
    }
  }
 eop:
  return(NULL);
}

// VERIFY: 055daa1b manual
// OFFSET: 0x00472360
static int floor0_inverse2(vorbis_block *vb,vorbis_look_floor *i,
                           void *memo,float *out){
  vorbis_look_floor0 *look=(vorbis_look_floor0 *)i;
  vorbis_info_floor0 *info=look->vi;

  if(floor0_map_lazy_init(vb,info,look))return(OV_EMEMORY);

  if(memo){
    float *lsp=(float *)memo;
    float amp=lsp[look->m];

    /* take the coefficients back to a spectral envelope curve */
    FMOD_vorbis_lsp_to_curve(out,
                        look->linearmap[vb->W],
                        look->n[vb->W],
                        look->ln,
                        lsp,look->m,amp,(float)info->ampdB);
    return(1);
  }
  FMOD_memset(out,0,sizeof(*out)*look->n[vb->W]);
  return(0);
}

/* export hooks */
const vorbis_func_floor FMOD_floor0_exportbundle={
  NULL,&floor0_unpack,&floor0_look,&floor0_free_info,
  &floor0_free_look,&floor0_inverse1,&floor0_inverse2
};
