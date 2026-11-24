/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2009             *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: residue backend 0, 1 and 2 implementation
 last mod: $Id: res0.c 16227 2009-07-08 06:58:46Z xiphmont $

 ********************************************************************/

/* Slow, slow, slow, simpleminded and did I mention it was slow?  The
   encode/decode loops are coded for clarity and performance is not
   yet even a nagging little idea lurking in the shadows.  Oh and BTW,
   it's slow. */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ogg/ogg.h>
#include "vorbis/codec.h"
#include "codec_internal.h"
#include "registry.h"
#include "codebook.h"
#include "misc.h"
#include "os.h"

#if defined(TRAIN_RES) || defined (TRAIN_RESAUX)
#include <stdio.h>
#endif

typedef struct {
  vorbis_info_residue0 *info;

  int         parts;
  int         stages;
  codebook   *fullbooks;
  codebook   *phrasebook;
  codebook ***partbooks;

  int         partvals;
  int       **decodemap;

  ogg_int32_t      postbits;
  ogg_int32_t      phrasebits;
  ogg_int32_t      frames;

#if defined(TRAIN_RES) || defined(TRAIN_RESAUX)
  int        train_seq;
  ogg_int32_t      *training_data[8][64];
  float      training_max[8][64];
  float      training_min[8][64];
  float     tmin;
  float     tmax;
#endif

} vorbis_look_residue0;

// VERIFY: bb8ddb13 auto
// OFFSET: 0x00470AC0
void FMOD_res0_free_info(vorbis_info_residue *i){
  vorbis_info_residue0 *info=(vorbis_info_residue0 *)i;
  if(info){
    FMOD_memset(info,0,sizeof(*info));
    _ogg_free(info);
  }
}

// VERIFY: 2f76a5b3 auto
// OFFSET: 0x00470AF0
void FMOD_res0_free_look(vorbis_look_residue *i){
  int j;
  if(i){

    vorbis_look_residue0 *look=(vorbis_look_residue0 *)i;

#ifdef TRAIN_RES
    {
      int j,k,l;
      for(j=0;j<look->parts;j++){
        /*fprintf(stderr,"partition %d: ",j);*/
        for(k=0;k<8;k++)
          if(look->training_data[k][j]){
            char buffer[80];
            FILE *of;
            codebook *statebook=look->partbooks[j][k];

            /* long and short into the same bucket by current convention */
            sprintf(buffer,"res_part%d_pass%d.vqd",j,k);
            of=fopen(buffer,"a");

            for(l=0;l<statebook->entries;l++)
              fprintf(of,"%d:%ld\n",l,look->training_data[k][j][l]);

            fclose(of);

            /*fprintf(stderr,"%d(%.2f|%.2f) ",k,
              look->training_min[k][j],look->training_max[k][j]);*/

            _ogg_free(look->training_data[k][j]);
            look->training_data[k][j]=NULL;
          }
        /*fprintf(stderr,"\n");*/
      }
    }
    fprintf(stderr,"min/max residue: %g::%g\n",look->tmin,look->tmax);

    /*fprintf(stderr,"residue bit usage %f:%f (%f total)\n",
            (float)look->phrasebits/look->frames,
            (float)look->postbits/look->frames,
            (float)(look->postbits+look->phrasebits)/look->frames);*/
#endif


    /*vorbis_info_residue0 *info=look->info;

    fprintf(stderr,
            "%ld frames encoded in %ld phrasebits and %ld residue bits "
            "(%g/frame) \n",look->frames,look->phrasebits,
            look->resbitsflat,
            (look->phrasebits+look->resbitsflat)/(float)look->frames);

    for(j=0;j<look->parts;j++){
      ogg_int32_t acc=0;
      fprintf(stderr,"\t[%d] == ",j);
      for(k=0;k<look->stages;k++)
        if((info->secondstages[j]>>k)&1){
          fprintf(stderr,"%ld,",look->resbits[j][k]);
          acc+=look->resbits[j][k];
        }

      fprintf(stderr,":: (%ld vals) %1.2fbits/sample\n",look->resvals[j],
              acc?(float)acc/(look->resvals[j]*info->grouping):0);
    }
    fprintf(stderr,"\n");*/

    for(j=0;j<look->parts;j++)
      if(look->partbooks[j])_ogg_free(look->partbooks[j]);
    _ogg_free(look->partbooks);
    for(j=0;j<look->partvals;j++)
      _ogg_free(look->decodemap[j]);
    _ogg_free(look->decodemap);

    FMOD_memset(look,0,sizeof(*look));
    _ogg_free(look);
  }
}

static int ilog(unsigned int v){
  int ret=0;
  while(v){
    ret++;
    v>>=1;
  }
  return(ret);
}

static int icount(unsigned int v){
  int ret=0;
  while(v){
    ret+=v&1;
    v>>=1;
  }
  return(ret);
}

#if 0
void FMOD_res0_pack(vorbis_info_residue *vr,oggpack_buffer *opb){
  vorbis_info_residue0 *info=(vorbis_info_residue0 *)vr;
  int j,acc=0;
  FMOD_oggpack_write(opb,info->begin,24);
  FMOD_oggpack_write(opb,info->end,24);

  FMOD_oggpack_write(opb,info->grouping-1,24);  /* residue vectors to group and
                                             code with a partitioned book */
  FMOD_oggpack_write(opb,info->partitions-1,6); /* possible partition choices */
  FMOD_oggpack_write(opb,info->groupbook,8);  /* group huffman book */

  /* secondstages is a bitmask; as encoding progresses pass by pass, a
     bitmask of one indicates this partition class has bits to write
     this pass */
  for(j=0;j<info->partitions;j++){
    if(ilog(info->secondstages[j])>3){
      /* yes, this is a minor hack due to not thinking ahead */
      FMOD_oggpack_write(opb,info->secondstages[j],3);
      FMOD_oggpack_write(opb,1,1);
      FMOD_oggpack_write(opb,info->secondstages[j]>>3,5);
    }else
      FMOD_oggpack_write(opb,info->secondstages[j],4); /* trailing zero */
    acc+=icount(info->secondstages[j]);
  }
  for(j=0;j<acc;j++)
    FMOD_oggpack_write(opb,info->booklist[j],8);

}
#endif

/* vorbis_info is for range checking */
// VERIFY: 48abc639 auto
// OFFSET: 0x00470BC0
vorbis_info_residue *FMOD_res0_unpack(vorbis_info *vi,oggpack_buffer *opb){
  int j,acc=0;
  vorbis_info_residue0 *info=_ogg_calloc(1,sizeof(*info));
  codec_setup_info     *ci=vi->codec_setup;

  if (!info)
  {
    goto errout;
  }

  info->begin=FMOD_oggpack_read(opb,24);
  info->end=FMOD_oggpack_read(opb,24);
  info->grouping=FMOD_oggpack_read(opb,24)+1;
  info->partitions=FMOD_oggpack_read(opb,6)+1;
  info->groupbook=FMOD_oggpack_read(opb,8);

  for(j=0;j<info->partitions;j++){
    int cascade=FMOD_oggpack_read(opb,3);
    if(FMOD_oggpack_read(opb,1))
      cascade|=(FMOD_oggpack_read(opb,5)<<3);
    info->secondstages[j]=cascade;

    acc+=icount(cascade);
  }
  for(j=0;j<acc;j++)
    info->booklist[j]=FMOD_oggpack_read(opb,8);

  if(info->groupbook>=ci->books)goto errout;
  for(j=0;j<acc;j++)
    if(info->booklist[j]>=ci->books)goto errout;

  return(info);
 errout:
  FMOD_res0_free_info(info);
  return(NULL);
}

// VERIFY: 33e87374 manual
// OFFSET: 0x00470D20
vorbis_look_residue *FMOD_res0_look(vorbis_dsp_state *vd,
                               vorbis_info_residue *vr){
  vorbis_info_residue0 *info=(vorbis_info_residue0 *)vr;
  vorbis_look_residue0 *look=_ogg_calloc(1,sizeof(*look));
  codec_setup_info     *ci=vd->vi->codec_setup;
  int j,k,acc=0;
  int dim;
  int maxstage=0;

  if (!look)
  {
    return(NULL);
  }

  look->info=info;

  look->parts=info->partitions;
  look->fullbooks=ci->fullbooks;
  look->phrasebook=ci->fullbooks+info->groupbook;
  dim=look->phrasebook->dim;

  look->partbooks=_ogg_calloc(look->parts,sizeof(*look->partbooks));
  if (!look->partbooks)
  {
    return(NULL);
  }

  for(j=0;j<look->parts;j++){
    int stages=ilog(info->secondstages[j]);
    if(stages){
      if(stages>maxstage)maxstage=stages;
      look->partbooks[j]=_ogg_calloc(stages,sizeof(*look->partbooks[j]));
      if (!look->partbooks[j])
      {
            return 0;
      }
      for(k=0;k<stages;k++)
        if(info->secondstages[j]&(1<<k)){
          look->partbooks[j][k]=ci->fullbooks+info->booklist[acc++];
#ifdef TRAIN_RES
          look->training_data[k][j]=_ogg_calloc(look->partbooks[j][k]->entries,
                                           sizeof(***look->training_data));
#endif
        }
    }
  }

  look->partvals=(int)(rint(pow((float)look->parts,(float)dim)));

  look->stages=maxstage;
  look->decodemap=_ogg_malloc(look->partvals*sizeof(*look->decodemap));
  if (!look->decodemap)
  {
        return 0;
  }
  for(j=0;j<look->partvals;j++){
    ogg_int32_t val=j;
    ogg_int32_t mult=look->partvals/look->parts;
    look->decodemap[j]=_ogg_malloc(dim*sizeof(*look->decodemap[j]));
    if (!look->decodemap[j])
    {
        return 0;
    }
    for(k=0;k<dim;k++){
      ogg_int32_t deco=val/mult;
      val-=deco*mult;
      mult/=look->parts;
      look->decodemap[j][k]=deco;
    }
  }
#if defined(TRAIN_RES) || defined (TRAIN_RESAUX)
  {
    static int train_seq=0;
    look->train_seq=train_seq++;
  }
#endif
  return(look);
}

#if 0
/* break an abstraction and copy some code for performance purposes */
static int local_book_besterror(codebook *book,float *a){
  int dim=book->dim,i,k,o;
  int best=0;
  encode_aux_threshmatch *tt=book->c->thresh_tree;

  /* find the quant val of each scalar */
  for(k=0,o=dim;k<dim;++k){
    float val=a[--o];
    i=tt->threshvals>>1;

    if(val<tt->quantthresh[i]){
      if(val<tt->quantthresh[i-1]){
        for(--i;i>0;--i)
          if(val>=tt->quantthresh[i-1])
            break;
      }
    }else{

      for(++i;i<tt->threshvals-1;++i)
        if(val<tt->quantthresh[i])break;

    }

    best=(best*tt->quantvals)+tt->quantmap[i];
  }
  /* regular lattices are easy :-) */

  if(book->c->lengthlist[best]<=0){
    const static_codebook *c=book->c;
    int i,j;
    float bestf=0.f;
    float *e=book->valuelist;
    best=-1;
    for(i=0;i<book->entries;i++){
      if(c->lengthlist[i]>0){
        float this=0.f;
        for(j=0;j<dim;j++){
          float val=(e[j]-a[j]);
          this+=val*val;
        }
        if(best==-1 || this<bestf){
          bestf=this;
          best=i;
        }
      }
      e+=dim;
    }
  }

  if(best>-1){
    float *ptr=book->valuelist+best*dim;
    for(i=0;i<dim;i++)
      *a++ -= *ptr++;
  }

  return(best);
}

static int _encodepart(oggpack_buffer *opb,float *vec, int n,
                       codebook *book,ogg_int32_t *acc){
  int i,bits=0;
  int dim=book->dim;
  int step=n/dim;

  for(i=0;i<step;i++){
    int entry=local_book_besterror(book,vec+i*dim);

#ifdef TRAIN_RES
    if(entry>0)
      acc[entry]++;
#endif

    bits+=FMOD_vorbis_book_encode(book,entry,opb);

  }

  return(bits);
}
#endif

static ogg_int32_t **_FMOD_01class(vorbis_block *vb,vorbis_look_residue *vl,
                       float **in,int ch){
  ogg_int32_t i,j,k;
  vorbis_look_residue0 *look=(vorbis_look_residue0 *)vl;
  vorbis_info_residue0 *info=look->info;

  /* move all this setup out later */
  int samples_per_partition=info->grouping;
  int possible_partitions=info->partitions;
  int n=info->end-info->begin;

  int partvals=n/samples_per_partition;
  ogg_int32_t **partword=_FMOD_vorbis_block_alloc(vb,ch*sizeof(*partword));
  float scale=100.0f/samples_per_partition;

  if (!partword)
  {
    return(NULL);
  }

  /* we find the partition type for each partition of each
     channel.  We'll go back and do the interleaved encoding in a
     bit.  For now, clarity */

  for(i=0;i<ch;i++){
    partword[i]=_FMOD_vorbis_block_alloc(vb,n/samples_per_partition*sizeof(*partword[i]));
    if (!partword[i])
    {
      return(NULL);
    }
    FMOD_memset(partword[i],0,n/samples_per_partition*sizeof(*partword[i]));
  }

  for(i=0;i<partvals;i++){
    int offset=i*samples_per_partition+info->begin;
    for(j=0;j<ch;j++){
      float max=0.;
      float ent=0.;
      for(k=0;k<samples_per_partition;k++){
        if(FMOD_ogg_fabs(in[j][offset+k])>max)max=FMOD_ogg_fabs(in[j][offset+k]);
        ent+=FMOD_ogg_fabs(FMOD_ogg_rint(in[j][offset+k]));
      }
      ent*=scale;

      for(k=0;k<possible_partitions-1;k++)
        if(max<=info->classmetric1[k] &&
           (info->classmetric2[k]<0 || (int)ent<info->classmetric2[k]))
          break;

      partword[j][i]=k;
    }
  }

#ifdef TRAIN_RESAUX
  {
    FILE *of;
    char buffer[80];

    for(i=0;i<ch;i++){
      sprintf(buffer,"resaux_%d.vqd",look->train_seq);
      of=fopen(buffer,"a");
      for(j=0;j<partvals;j++)
        fprintf(of,"%ld, ",partword[i][j]);
      fprintf(of,"\n");
      fclose(of);
    }
  }
#endif
  look->frames++;

  return(partword);
}

#if 0
/* designed for stereo or other modes where the partition size is an
   integer multiple of the number of channels encoded in the current
   submap */
static ogg_int32_t **_2class(vorbis_block *vb,vorbis_look_residue *vl,float **in,
                      int ch){
  ogg_int32_t i,j,k,l;
  vorbis_look_residue0 *look=(vorbis_look_residue0 *)vl;
  vorbis_info_residue0 *info=look->info;

  /* move all this setup out later */
  int samples_per_partition=info->grouping;
  int possible_partitions=info->partitions;
  int n=info->end-info->begin;

  int partvals=n/samples_per_partition;
  ogg_int32_t **partword=_FMOD_vorbis_block_alloc(vb,sizeof(*partword));

#if defined(TRAIN_RES) || defined (TRAIN_RESAUX)
  FILE *of;
  char buffer[80];
#endif

  partword[0]=_FMOD_vorbis_block_alloc(vb,n*ch/samples_per_partition*sizeof(*partword[0]));
  FMOD_memset(partword[0],0,n*ch/samples_per_partition*sizeof(*partword[0]));

  for(i=0,l=info->begin/ch;i<partvals;i++){
    float magmax=0.f;
    float angmax=0.f;
    for(j=0;j<samples_per_partition;j+=ch){
      if(FMOD_ogg_fabs(in[0][l])>magmax)magmax=FMOD_ogg_fabs(in[0][l]);
      for(k=1;k<ch;k++)
        if(FMOD_ogg_fabs(in[k][l])>angmax)angmax=FMOD_ogg_fabs(in[k][l]);
        l++;
    }

    for(j=0;j<possible_partitions-1;j++)
      if(magmax<=info->classmetric1[j] &&
         angmax<=info->classmetric2[j])
        break;

    partword[0][i]=j;

  }

#ifdef TRAIN_RESAUX
  sprintf(buffer,"resaux_%d.vqd",look->train_seq);
  of=fopen(buffer,"a");
  for(i=0;i<partvals;i++)
    fprintf(of,"%ld, ",partword[0][i]);
  fprintf(of,"\n");
  fclose(of);
#endif

  look->frames++;

  return(partword);
}

static int _01forward(oggpack_buffer *opb,
                      vorbis_block *vb,vorbis_look_residue *vl,
                      float **in,int ch,
                      ogg_int32_t **partword,
                      int (*encode)(oggpack_buffer *,float *,int,
                                    codebook *,ogg_int32_t *)){
  ogg_int32_t i,j,k,s;
  vorbis_look_residue0 *look=(vorbis_look_residue0 *)vl;
  vorbis_info_residue0 *info=look->info;

  /* move all this setup out later */
  int samples_per_partition=info->grouping;
  int possible_partitions=info->partitions;
  int partitions_per_word=look->phrasebook->dim;
  int n=info->end-info->begin;

  int partvals=n/samples_per_partition;
  ogg_int32_t resbits[128];
  ogg_int32_t resvals[128];

#ifdef TRAIN_RES
  for(i=0;i<ch;i++)
    for(j=info->begin;j<end;j++){
      if(in[i][j]>look->tmax)look->tmax=in[i][j];
      if(in[i][j]<look->tmin)look->tmin=in[i][j];
    }
#endif

  FMOD_memset(resbits,0,sizeof(resbits));
  FMOD_memset(resvals,0,sizeof(resvals));

  /* we code the partition words for each channel, then the residual
     words for a partition per channel until we've written all the
     residual words for that partition word.  Then write the next
     partition channel words... */

  for(s=0;s<look->stages;s++){

    for(i=0;i<partvals;){

      /* first we encode a partition codeword for each channel */
      if(s==0){
        for(j=0;j<ch;j++){
          ogg_int32_t val=partword[j][i];
          for(k=1;k<partitions_per_word;k++){
            val*=possible_partitions;
            if(i+k<partvals)
              val+=partword[j][i+k];
          }

          /* training hack */
          if(val<look->phrasebook->entries)
            look->phrasebits+=FMOD_vorbis_book_encode(look->phrasebook,val,opb);
#if 0 /*def TRAIN_RES*/
          else
            fprintf(stderr,"!");
#endif

        }
      }

      /* now we encode interleaved residual values for the partitions */
      for(k=0;k<partitions_per_word && i<partvals;k++,i++){
        ogg_int32_t offset=i*samples_per_partition+info->begin;

        for(j=0;j<ch;j++){
          if(s==0)resvals[partword[j][i]]+=samples_per_partition;
          if(info->secondstages[partword[j][i]]&(1<<s)){
            codebook *statebook=look->partbooks[partword[j][i]][s];
            if(statebook){
              int ret;
              ogg_int32_t *accumulator=NULL;

#ifdef TRAIN_RES
              accumulator=look->training_data[s][partword[j][i]];
              {
                int l;
                float *samples=in[j]+offset;
                for(l=0;l<samples_per_partition;l++){
                  if(samples[l]<look->training_min[s][partword[j][i]])
                    look->training_min[s][partword[j][i]]=samples[l];
                  if(samples[l]>look->training_max[s][partword[j][i]])
                    look->training_max[s][partword[j][i]]=samples[l];
                }
              }
#endif

              ret=encode(opb,in[j]+offset,samples_per_partition,
                         statebook,accumulator);

              look->postbits+=ret;
              resbits[partword[j][i]]+=ret;
            }
          }
        }
      }
    }
  }

  /*{
    ogg_int32_t total=0;
    ogg_int32_t totalbits=0;
    fprintf(stderr,"%d :: ",vb->mode);
    for(k=0;k<possible_partitions;k++){
    fprintf(stderr,"%ld/%1.2g, ",resvals[k],(float)resbits[k]/resvals[k]);
    total+=resvals[k];
    totalbits+=resbits[k];
    }

    fprintf(stderr,":: %ld:%1.2g\n",total,(double)totalbits/total);
    }*/

  return(0);
}
#endif

/* a truncated packet here just means 'stop working'; it's not an error */
// VERIFY: 4e341123 manual
// OFFSET: 0x00470F70
static int _FMOD_01inverse(vorbis_block *vb,vorbis_look_residue *vl,
                      float **in,int ch,
                      ogg_int32_t (*decodepart)(codebook *, float *,
                                         oggpack_buffer *,int)){

  ogg_int32_t i,j,k,l,s;
  vorbis_look_residue0 *look=(vorbis_look_residue0 *)vl;
  vorbis_info_residue0 *info=look->info;

  /* move all this setup out later */
  int samples_per_partition=info->grouping;
  int partitions_per_word=look->phrasebook->dim;
  int max=vb->pcmend>>1;
  int end=(info->end<max?info->end:max);
  int n=end-info->begin;

  if(n>0){
    int partvals=n/samples_per_partition;
    int partwords=(partvals+partitions_per_word-1)/partitions_per_word;
    int ***partword=alloca(ch*sizeof(*partword));

    for(j=0;j<ch;j++)
    {
      partword[j]=_FMOD_vorbis_block_alloc(vb,partwords*sizeof(*partword[j]));
      if (!partword[j])
      {
        return(OV_EMEMORY);
      }
    }      

    for(s=0;s<look->stages;s++){

      /* each loop decodes on partition codeword containing
         partitions_per_word partitions */
      for(i=0,l=0;i<partvals;l++){
        if(s==0){
          /* fetch the partition word for each channel */
          for(j=0;j<ch;j++){
            int temp=FMOD_vorbis_book_decode(look->phrasebook,&vb->opb);

            if(temp==-1)goto eopbreak;
            partword[j][l]=look->decodemap[temp];
            if(partword[j][l]==NULL)goto errout;
          }
        }

        /* now we decode residual values for the partitions */
        for(k=0;k<partitions_per_word && i<partvals;k++,i++)
          for(j=0;j<ch;j++){
            ogg_int32_t offset=info->begin+i*samples_per_partition;
            if(info->secondstages[partword[j][l][k]]&(1<<s)){
              codebook *stagebook=look->partbooks[partword[j][l][k]][s];
              if(stagebook){
                if(decodepart(stagebook,in[j]+offset,&vb->opb,
                              samples_per_partition)==-1)goto eopbreak;
              }
            }
          }
      }
    }
  }
 errout:
 eopbreak:
  return(0);
}

#if 0
/* residue 0 and 1 are just slight variants of one another. 0 is
   interleaved, 1 is not */
ogg_int32_t **FMOD_res0_class(vorbis_block *vb,vorbis_look_residue *vl,
                  float **in,int *nonzero,int ch){
  /* we encode only the nonzero parts of a bundle */
  int i,used=0;
  for(i=0;i<ch;i++)
    if(nonzero[i])
      in[used++]=in[i];
  if(used)
    /*return(_FMOD_01class(vb,vl,in,used,_interleaved_testhack));*/
    return(_FMOD_01class(vb,vl,in,used));
  else
    return(0);
}

int FMOD_res0_forward(vorbis_block *vb,vorbis_look_residue *vl,
                 float **in,float **out,int *nonzero,int ch,
                 ogg_int32_t **partword){
  /* we encode only the nonzero parts of a bundle */
  int i,j,used=0,n=vb->pcmend/2;
  for(i=0;i<ch;i++)
    if(nonzero[i]){
      if(out)
        for(j=0;j<n;j++)
          out[i][j]+=in[i][j];
      in[used++]=in[i];
    }
  if(used){
    int ret=_01forward(vb,vl,in,used,partword,
                      _interleaved_encodepart);
    if(out){
      used=0;
      for(i=0;i<ch;i++)
        if(nonzero[i]){
          for(j=0;j<n;j++)
            out[i][j]-=in[used][j];
          used++;
        }
    }
    return(ret);
  }else{
    return(0);
  }
}
#endif

// VERIFY: 9a20a7f6 auto
// OFFSET: 0x00471180
int FMOD_res0_inverse(vorbis_block *vb,vorbis_look_residue *vl,
                 float **in,int *nonzero,int ch){
  int i,used=0;
  for(i=0;i<ch;i++)
    if(nonzero[i])
      in[used++]=in[i];
  if(used)
    return(_FMOD_01inverse(vb,vl,in,used,FMOD_vorbis_book_decodevs_add));
  else
    return(0);
}

#if 0
int FMOD_res1_forward(oggpack_buffer *opb,vorbis_block *vb,vorbis_look_residue *vl,
                 float **in,float **out,int *nonzero,int ch,
                 ogg_int32_t **partword){
  int i,j,used=0,n=vb->pcmend/2;
  for(i=0;i<ch;i++)
    if(nonzero[i]){
      if(out)
        for(j=0;j<n;j++)
          out[i][j]+=in[i][j];
      in[used++]=in[i];
    }

  if(used){
    int ret=_01forward(opb,vb,vl,in,used,partword,_encodepart);
    if(out){
      used=0;
      for(i=0;i<ch;i++)
        if(nonzero[i]){
          for(j=0;j<n;j++)
            out[i][j]-=in[used][j];
          used++;
        }
    }
    return(ret);
  }else{
    return(0);
  }
}
#endif

ogg_int32_t **FMOD_res1_class(vorbis_block *vb,vorbis_look_residue *vl,
                  float **in,int *nonzero,int ch){
  int i,used=0;
  for(i=0;i<ch;i++)
    if(nonzero[i])
      in[used++]=in[i];
  if(used)
    return(_FMOD_01class(vb,vl,in,used));
  else
    return(0);
}

// VERIFY: 62b4ad74 auto
// OFFSET: 0x004711E0
int FMOD_res1_inverse(vorbis_block *vb,vorbis_look_residue *vl,
                 float **in,int *nonzero,int ch){
  int i,used=0;
  for(i=0;i<ch;i++)
    if(nonzero[i])
      in[used++]=in[i];
  if(used)
    return(_FMOD_01inverse(vb,vl,in,used,FMOD_vorbis_book_decodev_add));
  else
    return(0);
}

#if 0
ogg_int32_t **FMOD_res2_class(vorbis_block *vb,vorbis_look_residue *vl,
                  float **in,int *nonzero,int ch){
  int i,used=0;
  for(i=0;i<ch;i++)
    if(nonzero[i])used++;
  if(used)
    return(_2class(vb,vl,in,ch));
  else
    return(0);
}

/* res2 is slightly more different; all the channels are interleaved
   into a single vector and encoded. */

int FMOD_res2_forward(oggpack_buffer *opb,
                 vorbis_block *vb,vorbis_look_residue *vl,
                 float **in,float **out,int *nonzero,int ch,
                 ogg_int32_t **partword){
  ogg_int32_t i,j,k,n=vb->pcmend/2,used=0;

  /* don't duplicate the code; use a working vector hack for now and
     reshape ourselves into a single channel res1 */
  /* ugly; reallocs for each coupling pass :-( */
  float *work=_FMOD_vorbis_block_alloc(vb,ch*n*sizeof(*work));
  
  for(i=0;i<ch;i++){
    float *pcm=in[i];
    if(nonzero[i])used++;
    for(j=0,k=i;j<n;j++,k+=ch)
      work[k]=pcm[j];
  }

  if(used){
    int ret=_01forward(opb,vb,vl,&work,1,partword,_encodepart);
    /* update the sofar vector */
    if(out){
      for(i=0;i<ch;i++){
        float *pcm=in[i];
        float *sofar=out[i];
        for(j=0,k=i;j<n;j++,k+=ch)
          sofar[j]+=pcm[j]-work[k];

      }
    }
    return(ret);
  }else{
    return(0);
  }
}
#endif

/* duplicate code here as speed is somewhat more important */
// VERIFY: a328a8fa manual
// OFFSET: 0x00471240
int FMOD_res2_inverse(vorbis_block *vb,vorbis_look_residue *vl,
                 float **in,int *nonzero,int ch){
  ogg_int32_t i,k,l,s;
  vorbis_look_residue0 *look=(vorbis_look_residue0 *)vl;
  vorbis_info_residue0 *info=look->info;

  /* move all this setup out later */
  int samples_per_partition=info->grouping;
  int partitions_per_word=look->phrasebook->dim;
  int max=(vb->pcmend*ch)>>1;
  int end=(info->end<max?info->end:max);
  int n=end-info->begin;

  if(n>0){
    int partvals=n/samples_per_partition;
    int partwords=(partvals+partitions_per_word-1)/partitions_per_word;
    int **partword=_FMOD_vorbis_block_alloc(vb,partwords*sizeof(*partword));
    if (!partword)
    {
        return(OV_EMEMORY);
    }

    for(i=0;i<ch;i++)if(nonzero[i])break;
    if(i==ch)return(0); /* no nonzero vectors */

    for(s=0;s<look->stages;s++){
      for(i=0,l=0;i<partvals;l++){

        if(s==0){
          /* fetch the partition word */
          int temp=FMOD_vorbis_book_decode(look->phrasebook,&vb->opb);
          if(temp==-1)goto eopbreak;
          partword[l]=look->decodemap[temp];
          if(partword[l]==NULL)goto errout;
        }

        /* now we decode residual values for the partitions */
        for(k=0;k<partitions_per_word && i<partvals;k++,i++)
          if(info->secondstages[partword[l][k]]&(1<<s)){
            codebook *stagebook=look->partbooks[partword[l][k]][s];

            if(stagebook){
              if(FMOD_vorbis_book_decodevv_add(stagebook,in,
                                          i*samples_per_partition+info->begin,ch,
                                          &vb->opb,samples_per_partition)==-1)
                goto eopbreak;
            }
          }
      }
    }
  }
 errout:
 eopbreak:
  return(0);
}


const vorbis_func_residue FMOD_residue0_exportbundle={
  NULL,
  &FMOD_res0_unpack,
  &FMOD_res0_look,
  &FMOD_res0_free_info,
  &FMOD_res0_free_look,
  NULL,
  NULL,
  &FMOD_res0_inverse
};

const vorbis_func_residue FMOD_residue1_exportbundle={
  NULL, //&res0_pack,
  &FMOD_res0_unpack,
  &FMOD_res0_look,
  &FMOD_res0_free_info,
  &FMOD_res0_free_look,
  NULL, //&res1_class,
  NULL, //&res1_forward,
  &FMOD_res1_inverse
};

const vorbis_func_residue FMOD_residue2_exportbundle={
  NULL, //&res0_pack,
  &FMOD_res0_unpack,
  &FMOD_res0_look,
  &FMOD_res0_free_info,
  &FMOD_res0_free_look,
  NULL, //&res2_class,
  NULL, //&res2_forward,
  &FMOD_res2_inverse
};
