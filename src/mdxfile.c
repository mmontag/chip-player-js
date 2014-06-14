/*
  MDXplay : MDX file parser

  Made by Daisuke Nagano <breeze.nagano@nifty.ne.jp>
  Jan.13.1999

  reference : mdxform.doc  ( KOUNO Takeshi )
            : MXDRVWIN.pas ( monlight@tkb.att.ne.jp )
 */

#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include "version.h"
#include "mdx.h"

/* ------------------------------------------------------------------ */

static void*
__alloc_mdxwork(void)
{
  MDX_DATA* mdx = NULL;
  mdx = (MDX_DATA *)malloc(sizeof(MDX_DATA));
  if (mdx) {
    memset((void *)mdx, 0, sizeof(MDX_DATA));
  }
  return mdx;
}

static int
__load_file(MDX_DATA* mdx, char* fnam)
{
  FILE* fp = NULL;
  unsigned char* buf = NULL;
  int len = 0;
  int result = 0;

  fp = fopen( fnam, "rb" );
  if ( fp == NULL ) {
    return FLAG_FALSE;
  }

  fseek(fp, 0, SEEK_END);
  len = (int)ftell(fp);
  fseek(fp, 0, SEEK_SET);

  buf = (unsigned char *)malloc(sizeof(unsigned char)*(len+16));
  memset(buf, 0, len);

  if (!buf) {
    fclose(fp);
    return FLAG_FALSE;
  }


  result = (int)fread( buf, 1, len, fp );
  fclose(fp);
	


  if (result!=len) {
    free(buf);
    return FLAG_FALSE;
  }

  mdx->length = len;
  mdx->data = buf;

  return FLAG_TRUE;
}

MDX_DATA *mdx_open_mdx( char *name ) {

  int i,j;
  int ptr;
  unsigned char *buf;
  MDX_DATA *mdx;

  /* allocate work area */

  mdx = __alloc_mdxwork();
  if ( mdx == NULL ) return NULL;

  /* data read */
  if (!__load_file(mdx, name)) {
    goto error_end;
  }

  /* title parsing */

  for ( i=0 ; i<MDX_MAX_TITLE_LENGTH ; i++ ) {
    mdx->data_title[i] = '\0';
  }
  i=0;
  ptr=0;
  buf = mdx->data;
  mdx->data_title[i]=0;
  if (mdx->length<3) {
    goto error_end;
  }
  while(1) {
    if ( buf[ptr+0] == 0x0d &&
	 buf[ptr+1] == 0x0a &&
	 buf[ptr+2] == 0x1a ) break;

    mdx->data_title[i++]=buf[ptr++];  /* warning! this text is SJIS */
    if ( i>=MDX_MAX_TITLE_LENGTH ) i--;
    if ( ptr > mdx->length ) return NULL;
  }
  mdx->data_title[i++]=0;


  /* pdx name */

  ptr+=3;
  for ( i=0 ; i<MDX_MAX_PDX_FILENAME_LENGTH ; i++ ) {
    mdx->pdx_name[i]='\0';
  }
  i=0;
  j=0;
  mdx->haspdx=FLAG_FALSE;
  while(1) {
    if ( buf[ptr] == 0x00 ) break;

    mdx->haspdx=FLAG_TRUE;
    mdx->pdx_name[i++] = buf[ptr++];
    if ( strcasecmp( ".pdx", (char *)(buf+ptr-1) )==0 ) j=1;
    if ( i>= MDX_MAX_PDX_FILENAME_LENGTH ) i--;
    if ( ptr > mdx->length ) goto error_end;
  }
  if ( mdx->haspdx==FLAG_TRUE && j==0 ) {
    mdx->pdx_name[i+0] = '.';
    mdx->pdx_name[i+1] = 'p';
    mdx->pdx_name[i+2] = 'd';
    mdx->pdx_name[i+3] = 'x';
  }

  /* get voice data offset */

  ptr++;
  mdx->base_pointer = ptr;
  mdx->voice_data_offset =
    (unsigned int)buf[ptr+0]*256 +
    (unsigned int)buf[ptr+1] + mdx->base_pointer;

  if ( mdx->voice_data_offset > mdx->length ) goto error_end;

   /* get MML data offset */

  mdx->mml_data_offset[0] =
    (unsigned int)buf[ptr+2+0] * 256 +
    (unsigned int)buf[ptr+2+1] + mdx->base_pointer;
  if ( mdx->mml_data_offset[0] > mdx->length ) goto error_end;

  if ( buf[mdx->mml_data_offset[0]] == MDX_SET_PCM8_MODE ) {
    mdx->ispcm8mode = 1;
    mdx->tracks = 16;
  } else {
    mdx->ispcm8mode = 0;
    mdx->tracks = 9;
  }

  for ( i=0 ; i<mdx->tracks ; i++ ) {
    mdx->mml_data_offset[i] =
      (unsigned int)buf[ptr+i*2+2+0] * 256 +
      (unsigned int)buf[ptr+i*2+2+1] + mdx->base_pointer;
    if ( mdx->mml_data_offset[i] > mdx->length ) goto error_end;
  }


  /* init. configuration */

  mdx->is_use_pcm8 = FLAG_TRUE;
  mdx->is_use_fm   = FLAG_TRUE;
  mdx->is_use_opl3 = FLAG_TRUE;

  i = strlen(VERSION_TEXT1);
  if ( i > MDX_VERSION_TEXT_SIZE ) i=MDX_VERSION_TEXT_SIZE;
  strncpy( (char *)mdx->version_1, VERSION_TEXT1, i );
  i = strlen(VERSION_TEXT2);
  if ( i > MDX_VERSION_TEXT_SIZE ) i=MDX_VERSION_TEXT_SIZE;
  strncpy( (char *)mdx->version_2, VERSION_TEXT2, i );

  return mdx;

error_end:
  if (mdx) {
    if (mdx->data) {
      free(mdx->data);
      mdx->data = NULL;
    }

    free(mdx);
  }
  return NULL;
}

int mdx_close_mdx ( MDX_DATA *mdx ) {

  if ( mdx == NULL ) return 1;

  if ( mdx->data != NULL ) free(mdx->data);
  free(mdx);

  return 0;
}


#ifndef HAVE_SUPPORT_DUMP_VOICES
# define dump_voices(a,b) (1)
#else
static void
dump_voices(MDX_DATA* mdx, int num)
{
  int sum = 0;
  int i=0;

  fprintf(stdout, "( @%03d, \n", num);
  fprintf(stdout, "#\t AR  D1R  D2R   RR   SL   TL   KS  MUL  DT1  DT2  AME\n");
  for ( i=0 ; i<4 ; i++ ) {
    fprintf(stdout, "\t%3d, %3d, %3d, %3d, %3d, %3d, %3d, %3d, %3d, %3d, %3d,\n",
	    mdx->voice[num].ar[i],
	    mdx->voice[num].d1r[i],
	    mdx->voice[num].d2r[i],
	    mdx->voice[num].rr[i],
	    mdx->voice[num].sl[i],
	    mdx->voice[num].tl[i],
	    mdx->voice[num].ks[i],
	    mdx->voice[num].mul[i],
	    mdx->voice[num].dt1[i],
	    mdx->voice[num].dt2[i],
	    mdx->voice[num].ame[i] );
  }
  fprintf(stdout, "#\tCON   FL   SM\n");
  fprintf(stdout, "\t%3d, %3d, %3d )\n",
	  mdx->voice[num].con,
	  mdx->voice[num].fl,
	  mdx->voice[num].slot_mask );
  
  fprintf(stdout, "[ F0 7D 10 %02X ", num);
  sum = mdx->voice[num].v0;
  for ( i=0 ; i<4 ; i++ ) {
    fprintf(stdout, "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X ",
	    mdx->voice[num].ar[i],
	    mdx->voice[num].d1r[i],
	    mdx->voice[num].d2r[i],
	    mdx->voice[num].rr[i],
	    mdx->voice[num].sl[i],
	    mdx->voice[num].tl[i],
	    mdx->voice[num].ks[i],
	    mdx->voice[num].mul[i],
	    mdx->voice[num].dt1[i],
	    mdx->voice[num].dt2[i],
	    mdx->voice[num].ame[i] );
    sum += mdx->voice[num].v1[i] + mdx->voice[num].v2[i] + mdx->voice[num].v3[i] + mdx->voice[num].v4[i] + mdx->voice[num].v5[i] + mdx->voice[num].v6[i];
  }
  fprintf(stdout, "%02X %02X %02X ",
	  mdx->voice[num].con,
	  mdx->voice[num].fl,
	  mdx->voice[num].slot_mask );
  
  fprintf(stdout, "%02X F7 ]\n", 0x80-(sum%0x7f));
  fprintf(stdout, "\n");
}
#endif

int mdx_get_voice_parameter( MDX_DATA *mdx ) {

  int i;
  int ptr;
  int num;
  unsigned char *buf;

  ptr = mdx->voice_data_offset;
  buf = mdx->data;

  while ( ptr < mdx->length ) {

    if ( mdx->length-ptr < 27 ) break;

    num = buf[ptr++];
    if ( num >= MDX_MAX_VOICE_NUMBER ) return 1;

    mdx->voice[num].v0 = buf[ptr];

    mdx->voice[num].con = buf[ptr  ]&0x07;
    mdx->voice[num].fl  = (buf[ptr++] >> 3)&0x07;
    mdx->voice[num].slot_mask = buf[ptr++];

    /* DT1 & MUL */
    for ( i=0 ; i<4 ; i++ ) {
      mdx->voice[num].v1[i] = buf[ptr];

      mdx->voice[num].mul[i] = buf[ptr] & 0x0f;
      mdx->voice[num].dt1[i] = (buf[ptr] >> 4)&0x07;
      ptr++;
    }
    /* TL */
    for ( i=0 ; i<4 ; i++ ) {
      mdx->voice[num].v2[i] = buf[ptr];

      mdx->voice[num].tl[i] = buf[ptr];
      ptr++;
    }
    /* KS & AR */
    for ( i=0 ; i<4 ; i++ ) {
      mdx->voice[num].v3[i] = buf[ptr];

      mdx->voice[num].ar[i] = buf[ptr] & 0x1f;
      mdx->voice[num].ks[i] = (buf[ptr] >> 6)&0x03;
      ptr++;
    }
    /* AME & D1R */
    for ( i=0 ; i<4 ; i++ ) {
      mdx->voice[num].v4[i] = buf[ptr];

      mdx->voice[num].d1r[i] = buf[ptr] & 0x1f;
      mdx->voice[num].ame[i] = (buf[ptr] >> 7)&0x01;
      ptr++;
    }
    /* DT2 & D2R */
    for ( i=0 ; i<4 ; i++ ) {
      mdx->voice[num].v5[i] = buf[ptr];

      mdx->voice[num].d2r[i] = buf[ptr] & 0x1f;
      mdx->voice[num].dt2[i] = (buf[ptr] >> 6)&0x03;
      ptr++;
    }
    /* SL & RR */
    for ( i=0 ; i<4 ; i++ ) {
      mdx->voice[num].v6[i] = buf[ptr];

      mdx->voice[num].rr[i] = buf[ptr] & 0x0f;
      mdx->voice[num].sl[i] = (buf[ptr] >> 4)&0x0f;
      ptr++;
    }

    /* if ( mdx->dump_voice == FLAG_TRUE ) {
       dump_voices(mdx, num);
    } */
  }

  return 0;
}

/* ------------------------------------------------------------------ */

int mdx_output_titles( MDX_DATA *mdx ) {

  // unsigned char *message;

  return 0;
}

