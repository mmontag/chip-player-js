/*
  MDXplay : PDX file parser

  Made by Daisuke Nagano <breeze.nagano@nifty.ne.jp>
  Jan.18.1999
 */

#include <stdio.h>
#include <stdlib.h>

#include "version.h"
#include "mdx.h"

/* ------------------------------------------------------------------ */
/* local functions */

static void store_pdx_data( PDX_DATA *, unsigned char *, int );
static void adpcm2pcm( int *, unsigned char *, long );

/* ------------------------------------------------------------------ */

PDX_DATA *mdx_open_pdx( unsigned char *buf, long length )
{
  PDX_DATA* pdx = NULL;

  pdx = (PDX_DATA *)malloc(sizeof(PDX_DATA));
  if (!pdx) {
    return NULL;
  }

  store_pdx_data( pdx, buf, (int)length );

  return pdx;
}

int mdx_close_pdx( PDX_DATA *pdx ) {
  int i;

  if ( !pdx ) return 1;

  for ( i=0 ; i<MDX_MAX_PDX_TONE_NUMBER*MDX_MAX_PDX_BANK_NUMBER ; i++ ) {
    if ( pdx->tone[i].data )
      free(pdx->tone[i].data);
    if ( pdx->tone[i].orig_data )
      free(pdx->tone[i].orig_data);
  }
  free( pdx );

  return 0;
}

/* ------------------------------------------------------------------ */

static void store_pdx_data( PDX_DATA *pdx, unsigned char *buf, int buflen ) {
  long address, size;
   long data_top = 0;
  int i,j,d;
  int ptr;
  int num;

  for ( i=0 ; i<MDX_MAX_PDX_TONE_NUMBER*MDX_MAX_PDX_BANK_NUMBER ; i++ ) {
    pdx->tone[i].data = NULL;
    pdx->tone[i].size = 0;
    pdx->tone[i].orig_data = NULL;
    pdx->tone[i].orig_size = 0;
  }

  data_top = buflen;
  ptr=0;
  num=0;
  while(1) {
    int iscontinue=FLAG_TRUE;

    for ( i=0 ; i<MDX_MAX_PDX_TONE_NUMBER ; i++,num++ ) {
      if (ptr+i*8 >= data_top) {
	  // table is done.
     	iscontinue = FLAG_FALSE;
		return;
      }

      address  = (long)buf[ptr+i*8+0]*256*256*256;
      address += (long)buf[ptr+i*8+1]*256*256;
      address += (long)buf[ptr+i*8+2]*256;
      address += (long)buf[ptr+i*8+3];

      size     = (long)buf[ptr+i*8+4]*256*256*256;
      size    += (long)buf[ptr+i*8+5]*256*256;
      size    += (long)buf[ptr+i*8+6]*256;
      size    += (long)buf[ptr+i*8+7];

      if ( address!=0 && address<=ptr+768 ) iscontinue=FLAG_FALSE;
      if ( address>0 && address < data_top ) data_top = address;

      if ( size<=0 || address<0) {
	pdx->tone[num].data = NULL;
	pdx->tone[num].size=0;
	continue;
      }

      /* 16bit pcm */
      pdx->tone[num].orig_data = (int *)malloc(sizeof(int)*size/2);
      if (!pdx->tone[num].orig_data) {
	  printf("%s Error : %d\n" ,__FILE__,__LINE__);
	goto error_end;
      }
      pdx->tone[num].orig_size = size/2;
      for ( j=0 ; j<pdx->tone[num].orig_size ; j++ ) {
	if (address+j*2+1<buflen) {
	  d = (int)(buf[address+j*2+0]<<8)+(int)buf[address+j*2+1];
	  if ( d>=0x8000 ) d-=0x10000;
	} else {
	  printf("%s Error : %d\n" ,__FILE__,__LINE__);
	  goto error_end;
	}
	pdx->tone[num].orig_data[j] = d*32;
      }

      if (size>=2*1024*1024) {
	/* raw 16bit pcm */
	pdx->tone[num].data = (int *)malloc(sizeof(int)*size/2);
	if (!pdx->tone[num].data) {
	  printf("%s Error : %d\n" ,__FILE__,__LINE__);
	  goto error_end;
	}
	pdx->tone[num].size = size/2;
	for ( j=0 ; j<pdx->tone[num].size ; j++ ) {
	  if (address+j*2+1<buflen) {
	    d = (int)(buf[address+j*2+0]<<8)+(int)buf[address+j*2+1];
	    if ( d>=0x8000 ) d-=0x10000;
	  } else {
	  printf("%s Error : %d\n" ,__FILE__,__LINE__);
	    goto error_end;
	  }
	  pdx->tone[num].data[j] = d*32;
	}
      } else {
	/* adpcm */
	pdx->tone[num].data = (int *)malloc(sizeof(int)*size*2);
	if (!pdx->tone[num].data) {
	  printf("%s Error : %d\n" ,__FILE__,__LINE__);
	  goto error_end;
	}
	pdx->tone[num].size = size*2;
	adpcm2pcm( pdx->tone[num].data, buf+address, size );
      }
    }
    ptr+=768;
    if ( iscontinue == FLAG_FALSE ||
	 num >= MDX_MAX_PDX_TONE_NUMBER*MDX_MAX_PDX_BANK_NUMBER ) 
      break;
  }

  return;

error_end:
  for ( i=0 ; i<MDX_MAX_PDX_TONE_NUMBER*MDX_MAX_PDX_BANK_NUMBER ; i++ ) {
    if ( pdx->tone[i].data != NULL ) {
      free(pdx->tone[i].data);
      pdx->tone[i].data = NULL;
    }
    if ( pdx->tone[i].orig_data != NULL ) {
      free(pdx->tone[i].orig_data);
      pdx->tone[i].orig_data = NULL;
    }
    pdx->tone[i].size = 0;
    pdx->tone[i].orig_size = 0;
  }
  return;
}

/* ------------------------------------------------------------------ */

typedef struct _adpcm_val {
  signed char slevel;
  signed int last_val;
} adpcm_val;

static const int scale_level[49]={
  16,17,19,21,23,25,28,
  31,34,37,41,45,50,55,
  60,66,73,80,88,97,107,
  118,130,143,157,173,190,209,
  230,253,279,307,337,371,408,
  449,494,544,598,658,724,796,
  876,963,1060,1166,1282,1411,1552 };

static const signed char level_chg[8]={ -1,-1,-1,-1,2,4,6,8 } ;

static int calc_pcm_val(adpcm_val* self, unsigned char adp ) {
 
  int d0;
  
  d0=((int)((adp&7)*2+1)*scale_level[(int)self->slevel])>>3;
  if (adp>7) d0=-d0;
  
  self->slevel+=level_chg[adp&7];
  if (self->slevel<0) self->slevel=0;
  if (self->slevel>48) self->slevel=48;

  self->last_val+=d0;
  if (self->last_val>1023) self->last_val=1023;
  if (self->last_val<-1024) self->last_val=-1024;

  return self->last_val*32;
}

static void adpcm2pcm( int *dst, unsigned char *src, long size ) {
  int i,j;
  adpcm_val adpcm;

  adpcm.slevel = 0;
  adpcm.last_val = 0;
 
  for ( i=0,j=0 ; j<size ; i+=2, j++ ) {
    dst[i+0] = calc_pcm_val( &adpcm,   src[j]    &0x0f );
    dst[i+1] = calc_pcm_val( &adpcm,  (src[j]>>4)&0x0f );
  }

  return;
}
