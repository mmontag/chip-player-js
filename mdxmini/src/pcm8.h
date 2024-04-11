/*
  MDXplayer : PCM8 emulater

  Made by Daisuke Nagano <breeze.nagano@nifty.ne.jp>
  Jan.21.1999
 */

#include "mdxmini.h"

#ifndef _PCM8_H_
#define _PCM8_H_

/* ------------------------------------------------------------------ */

#define PCM8_MAX_NOTE        16
#define PCM8_MAX_VOLUME      128
#ifdef HAVE_SYMBIAN_SUPPORT
# define PCM8_MASTER_PCM_RATE 16000     /* Hz */
#else /* HAVE_SYMBIAN_SUPPORT */
# define PCM8_MASTER_PCM_RATE 44100      /* Hz */
#endif /* HAVE_SYMBIAN_SUPPORT */
#define PCM8_NUM_OF_BUFFERS  16         /* for SNDCTL_DSP_SETFRAGMENT */
#define PCM8_SYSTEM_RATE     1         /* milli-second */

#define YM2151EMU_VOLUME     750/1024    /* percentage of OPM volume */

/* ------------------------------------------------------------------ */

typedef struct _PCM8_WORK {
  int *ptr;           /* current sample pointer */
  int *top_ptr;       /* top sample point */
  int *end_ptr;       /* end sample point */
  int  volume;        /* volume ( 0-127 ) */
  int  freq;          /* sample frequency */

  int adpcm;          /* TRUE: adpcm data FALSE: 16bit data */

  int isloop;         /* is the sample looping? */

  int fnum; /* freq conversion parameter buffer */
  int snum; /* freq conversion parameter buffer */

} PCM8_WORK;

/* ------------------------------------------------------------------ */
/* functions */

extern int  pcm8_open( MDX_DATA *, songdata* );
extern int  pcm8_close( songdata * );
extern void pcm8_init( songdata * );
extern void pcm8_start( void );
extern void pcm8_stop( void );

extern void do_pcm8( short *buffer,int buffer_size, songdata * );
extern int  pcm8_get_buffer_size( songdata * );
extern int  pcm8_get_sample_size( songdata * );
extern int  pcm8_get_output_channels( songdata * );


extern int  pcm8_set_pcm_freq( int, int, songdata* );
extern int  pcm8_set_volume( int, int, songdata * );
extern int  pcm8_set_pan( int, songdata * );

extern int  pcm8_note_on( int, int *, int, int *, int, songdata * );
extern int  pcm8_note_off( int, songdata * );

extern int  pcm8_set_master_volume( int, songdata * );

// extern void pcm8_wait_for_pcm_write();

#endif /* _PCM8_H_ */
