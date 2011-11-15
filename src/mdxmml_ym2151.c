/*
  MDXplayer :  MML parser

  Made by Daisuke Nagano <breeze.nagano@nifty.ne.jp>
  Jan.15.1999

  reference : mdxform.doc  ( Takeshi Kouno )
            : MXDRVWIN.pas ( monlight@tkb.att.ne.jp )
 */

/* ------------------------------------------------------------------ */

#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include "version.h"
#include "mdx.h"
#include "mdx2151.h"
#include "pcm8.h"

#include "class.h"

/* ------------------------------------------------------------------ */
/* export symbols */

int mdx_parse_mml_ym2151( MDX_DATA *, PDX_DATA * );
int mdx_init_track_work_area_ym2151( void );

/* ------------------------------------------------------------------ */
/* local valiables */

static const int vol_conv[] = {
   85,  87,  90,  93,  95,  98, 101, 103,
  106, 109, 111, 114, 117, 119, 122, 125
};

/* ------------------------------------------------------------------ */
/* local instances */

typedef struct _mdxmml_ym2151_instances {
  MDX_DATA *mdx;
  PDX_DATA *pdx;

  int pcm8_disable;
  int fade_out;
  int is_sigint_caught;

  int all_track_finished;
  int fade_out_wait;
  int master_volume;
} mdxmml_ym2151_instances;

/* ------------------------------------------------------------------ */
/* local functions */

static int  set_new_event( int );
static void set_tempo( int );
static void note_off( int );
static void note_on( int, int );
static void set_volume( int, int );
static void inc_volume( int );
static void dec_volume( int );
static void set_phase( int, int );
static void set_keyoff( int );
static void set_voice( int, int );
static void set_quantize( int, int );
static void set_detune( int, int, int );
static void set_portament( int, int, int );
static void set_freq( int, int );
static void set_opm_reg( int, int, int );
static void set_fade_out( int );
static void send_sync( int );
static void recv_sync( int );

static void set_plfo_onoff( int, int );
static void set_plfo( int, int, int, int, int, int );
static void set_alfo_onoff( int, int );
static void set_alfo( int, int, int, int, int, int );
static void set_hlfo_onoff( int, int );
static void set_hlfo( int, int, int, int, int, int );
static void set_lfo_delay( int, int );

static void do_quantize( int, int );

/* ------------------------------------------------------------------ */
/* class interface */

extern void* _get_mdxmml_ym2151(void);

static mdxmml_ym2151_instances*
__get_instances(void)
{
  return (mdxmml_ym2151_instances*)_get_mdxmml_ym2151();
}

#define __GETSELF  mdxmml_ym2151_instances* self = __get_instances();

void*
_mdxmml_ym2151_initialize(void)
{
  mdxmml_ym2151_instances* self = NULL;

  self = (mdxmml_ym2151_instances *)malloc(sizeof(mdxmml_ym2151_instances));
  if (!self) {
    return NULL;
  }
  memset(self, 0, sizeof(mdxmml_ym2151_instances));

  self->mdx = NULL;
  self->pdx = NULL;
  self->pcm8_disable = FLAG_TRUE;
  self->fade_out = 0;
  self->is_sigint_caught = FLAG_FALSE;

  return self;
}

void
_mdxmml_ym2151_finalize(void* in_self)
{
  mdxmml_ym2151_instances* self = (mdxmml_ym2151_instances *)in_self;
  if (self) {
    free(self);
  }
}

/* ------------------------------------------------------------------ */
/* implementations */

int
mdx_parse_mml_ym2151( MDX_DATA *orig_mdx, PDX_DATA *orig_pdx )
{
  int i;
  long count;
  int all_track_finished;
  int fade_out_wait;
  int master_volume;
  int infinite_loops;

  __GETSELF;

  self->mdx = orig_mdx;
  self->pdx = orig_pdx;

  mdx_init_track_work_area_ym2151();

  self->pcm8_disable=FLAG_TRUE;
  if ( pcm8_open(self->mdx)==0 ) {
    self->pcm8_disable=FLAG_FALSE;
  }

  if (!ym2151_reg_init( self->mdx )) {
    /* failed to initialize opm! */
    return 1;
  }

  /* start parsing */

  all_track_finished=FLAG_FALSE;
  fade_out_wait=0;
  master_volume=127;

  while(all_track_finished==FLAG_FALSE) {

    if ( self->fade_out > 0 ) {
      if ( fade_out_wait==0 ) { fade_out_wait = self->fade_out; }
      fade_out_wait--;
      if ( fade_out_wait==0 ) { master_volume--; }
      if ( master_volume==0 ) { break; }
    }
    ym2151_set_master_volume( master_volume * self->mdx->fm_volume / 127 );
    pcm8_set_master_volume( master_volume * self->mdx->pcm_volume / 127 );

    all_track_finished=FLAG_TRUE;
    infinite_loops = 32767; /* large enough */
    for ( i=0 ; i<self->mdx->tracks ; i++ ) {

      if ( self->mdx->track[i].waiting_sync == FLAG_TRUE )
	{ continue; }

      count = self->mdx->track[i].counter;
      if ( count < 0 ) { continue; } /* this track has finished */
      all_track_finished=FLAG_FALSE;

      self->mdx->track[i].gate--;
      if ( self->mdx->track[i].gate == 0 ) { note_off( i ); }

      if ( i<8 ) {
	ym2151_set_freq_volume( i ); /* do portament, lfo, detune */
      }

      count--;
      while ( count == 0 ) {
	count=set_new_event( i );
      }

      self->mdx->track[i].counter = count;
      if ( infinite_loops > self->mdx->track[i].infinite_loop_times ) {
	infinite_loops = self->mdx->track[i].infinite_loop_times;
      }
    }

    if ( self->mdx->max_infinite_loops > 0 ) {
      if ( infinite_loops >= self->mdx->max_infinite_loops ) {
	self->fade_out = self->mdx->fade_out_speed;
      }
    }

    /* timer count */

    self->mdx->total_count++;
    self->mdx->elapsed_time += 1000*1024*(256 - self->mdx->tempo)/4000;

	do_pcm8(NULL,-1);
    
	}

  ym2151_all_note_off();
  pcm8_close();
  ym2151_shutdown();

  return 0;
}




void*
mdx_parse_mml_ym2151_async_initialize(MDX_DATA *orig_mdx, PDX_DATA *orig_pdx)
{
  __GETSELF;

  self->mdx = orig_mdx;
  self->pdx = orig_pdx;

  mdx_init_track_work_area_ym2151();

  self->pcm8_disable=FLAG_TRUE;
  if ( pcm8_open(self->mdx)==0 ) {
    self->pcm8_disable=FLAG_FALSE;
  }

  if (!ym2151_reg_init( self->mdx )) {
    /* failed to initialize opm! */
    return NULL;
  }

  /* start parsing */

  self->all_track_finished=FLAG_FALSE;
  self->fade_out_wait=0;
  self->master_volume=127;

  return (void *)self;
}

void
mdx_parse_mml_ym2151_async_finalize(void* self)
{
  ym2151_all_note_off();
  pcm8_close();
  ym2151_shutdown();
}

int
mdx_parse_mml_ym2151_async(void* in_self)
{
  int i;
  long count;
  int infinite_loops;
  mdxmml_ym2151_instances* self = (mdxmml_ym2151_instances *)in_self;

  pcm8_clear_buffer_flush_flag();
 loop:
  if (self->all_track_finished==FLAG_TRUE) {
    return FLAG_FALSE;
  }

  if ( self->fade_out > 0 ) {
    if ( self->fade_out_wait==0 ) { self->fade_out_wait = self->fade_out; }
    self->fade_out_wait--;
    if ( self->fade_out_wait==0 ) { self->master_volume--; }
    if ( self->master_volume==0 ) { return FLAG_FALSE; }
  }
  ym2151_set_master_volume( self->master_volume * self->mdx->fm_volume / 127 );
  pcm8_set_master_volume( self->master_volume * self->mdx->pcm_volume / 127 );

  self->all_track_finished=FLAG_TRUE;
  infinite_loops = 32767; /* large enough */

  for ( i=0 ; i<self->mdx->tracks ; i++ ) {

    if ( self->mdx->track[i].waiting_sync == FLAG_TRUE )
      { continue; }

    count = self->mdx->track[i].counter;
    if ( count < 0 ) { continue; } /* this track has finished */
    self->all_track_finished=FLAG_FALSE;
    
    self->mdx->track[i].gate--;
    if ( self->mdx->track[i].gate == 0 ) { note_off( i ); }
    
    if ( i<8 ) {
      ym2151_set_freq_volume( i ); /* do portament, lfo, detune */
    }

    count--;
    while ( count == 0 ) {
      count=set_new_event( i );
    }

    self->mdx->track[i].counter = count;
    if ( infinite_loops > self->mdx->track[i].infinite_loop_times ) {
      infinite_loops = self->mdx->track[i].infinite_loop_times;
    }
  }

  if ( self->mdx->max_infinite_loops > 0 ) {
    if ( infinite_loops >= self->mdx->max_infinite_loops ) {
      self->fade_out = self->mdx->fade_out_speed;
    }
  }

  /* timer count */

  self->mdx->total_count++;
  self->mdx->elapsed_time += 1000*1024*(256 - self->mdx->tempo)/4000;

  return FLAG_TRUE;
}

int mdx_parse_mml_ym2151_async_get_length(void* in_self)
{
  int next,sec;
  
  mdxmml_ym2151_instances* self = (mdxmml_ym2151_instances *)in_self;

  
  next = 1;
  while(next && self->mdx->elapsed_time < (1200 * 1000000))
  {
	next = mdx_parse_mml_ym2151_async(self);
  }
  
  sec = (int)self->mdx->elapsed_time / 1000000;

  /* stop */
  ym2151_all_note_off();

  /* reinitialize */ 
  mdx_init_track_work_area_ym2151();
  pcm8_init();
  
  if (!ym2151_reg_init( self->mdx )) {
    /* failed to initialize opm! */
    return 0;
  }
  
  
  /* start parsing */

  self->all_track_finished=FLAG_FALSE;
  self->fade_out_wait=0;
  self->master_volume=127;
  
  return sec;
}

int
mdx_parse_mml_ym2151_make_samples(short *buffer,int buffer_size)
{
   do_pcm8(buffer,buffer_size);
   return 0;
}


int
mdx_parse_mml_get_tempo(void* in_self)
{
  mdxmml_ym2151_instances* self = (mdxmml_ym2151_instances *)in_self;

 return 1000*1024*(256 - self->mdx->tempo)/4000;
}




/* ------------------------------------------------------------------ */

#define __GETMDX \
MDX_DATA* mdx; \
__GETSELF; \
mdx = self->mdx;

int
mdx_init_track_work_area_ym2151( void )
{
  int i;

  __GETMDX;

  self->fade_out = 0;

  mdx->tempo        = 200;
  mdx->total_count  = 0;
  mdx->elapsed_time = 0;

  mdx->fm_noise_vol = 0;
  mdx->fm_noise_freq = 0;

  for ( i=0 ; i<mdx->tracks ; i++ ) {
    mdx->track[i].counter = 1;
    mdx->track[i].gate = 1;
    mdx->track[i].current_mml_ptr = mdx->mml_data_offset[i];

    mdx->track[i].voice         = 0;
    mdx->track[i].volume        = 64;
    mdx->track[i].volume_normal = 8;
    mdx->track[i].note          = 0;
    mdx->track[i].phase         = MDX_PAN_C;
    mdx->track[i].quantize1     = 8;
    mdx->track[i].quantize2     = 0;
    mdx->track[i].detune        = 0;
    if ( i<8 )
      ym2151_set_detune(i,0);
    mdx->track[i].portament     = 0;
    if ( i<8 )
      ym2151_set_portament(i,0);

    mdx->track[i].loop_depth = 0;
    mdx->track[i].infinite_loop_times = 0;

    mdx->track[i].p_lfo_flag = FLAG_FALSE;
    mdx->track[i].a_lfo_flag = FLAG_FALSE;
    mdx->track[i].h_lfo_flag = FLAG_FALSE;

    mdx->track[i].p_lfo_form = 0;
    mdx->track[i].p_lfo_clock = 255;
    mdx->track[i].p_lfo_depth = 0;

    mdx->track[i].a_lfo_form = 0;
    mdx->track[i].a_lfo_clock = 255;
    mdx->track[i].a_lfo_depth = 0;

    mdx->track[i].waiting_sync = FLAG_FALSE;

    mdx->track[i].keyoff_disable     = FLAG_FALSE;
    mdx->track[i].last_volume_normal = FLAG_FALSE;
  }

  return 0;
}

static int
set_new_event( int t )
{
  int ptr;
  unsigned char *data;
  int count;
  int follower;
  __GETMDX;

  data = mdx->data;
  ptr = mdx->track[t].current_mml_ptr;
  count = 0;
  follower = 0;

#if 0
  if ( ptr+1 <= mdx->length && t>7 ) {
    fprintf(stderr,"%2d %2x %2x\n",t,data[ptr],data[ptr+1]);fflush(stderr);
  }
#endif

  if ( data[ptr] <= MDX_MAX_REST ) {  /* rest */
    note_off(t);
    count = data[ptr]+1;
    mdx->track[t].gate = count+1;
    follower=0;

  } else if ( data[ptr] <= MDX_MAX_NOTE ) { /* note */
    note_on( t, data[ptr]);
    count = data[ptr+1]+1;
    do_quantize( t, count );
    follower = 1;

  } else {
    switch ( data[ptr] ) {

    case MDX_SET_TEMPO:
      set_tempo( data[ptr+1] );
      follower = 1;
      break;

    case MDX_SET_OPM_REG:
      set_opm_reg( t, data[ptr+1], data[ptr+2] );
      follower = 2;
      break;

    case MDX_SET_VOICE:
      set_voice( t, data[ptr+1] );
      follower = 1;
      break;

    case MDX_SET_PHASE:
      set_phase( t, data[ptr+1] );
      follower = 1;
      break;

    case MDX_SET_VOLUME:
      set_volume( t, data[ptr+1] );
      follower = 1;
      break;

    case MDX_VOLUME_DEC:
      dec_volume( t );
      follower = 0;
      break;

    case MDX_VOLUME_INC:
      inc_volume( t );
      follower = 0;
      break;

    case MDX_SET_QUANTIZE:
      set_quantize( t, data[ptr+1] );
      follower = 1;
      break;

    case MDX_SET_KEYOFF:
      set_keyoff( t );
      follower = 0;
      break;

    case MDX_REPEAT_START:
      mdx->track[t].loop_counter[mdx->track[t].loop_depth] = data[ptr+1];
      if ( mdx->track[t].loop_depth < MDX_MAX_LOOP_DEPTH )
	mdx->track[t].loop_depth++;
      follower = 2;
      break;

    case MDX_REPEAT_END:
      if (--mdx->track[t].loop_counter[mdx->track[t].loop_depth-1] == 0 ) {
	if ( --mdx->track[t].loop_depth < 0 ) mdx->track[t].loop_depth=0;
      } else {
	if ( data[ptr+1] >= 0x80 ) {
	  ptr = ptr+2 - (0x10000-(data[ptr+1]*256 + data[ptr+2])) - 2;
	} else {
	  ptr = ptr+2 + data[ptr+1]*256 + data[ptr+2] - 2;
	}
      }
      follower = 2;
      break;

    case MDX_REPEAT_BREAK:
      if ( mdx->track[t].loop_counter[mdx->track[t].loop_depth-1] == 1 ) {
	if ( --mdx->track[t].loop_depth < 0 ) mdx->track[t].loop_depth=0;
	ptr = ptr+2 + data[ptr+1]*256 + data[ptr+2] -2 +2;
      }
      follower = 2;
      break;

    case MDX_SET_DETUNE:
      set_detune( t, data[ptr+1], data[ptr+2] );
      follower = 2;
      break;

    case MDX_SET_PORTAMENT:
      set_portament( t, data[ptr+1], data[ptr+2] );
      follower = 2;
      break;

    case MDX_DATA_END:
      if ( data[ptr+1] == 0x00 ) {
	count = -1;
	note_off(t);
	follower = 1;
      } else {
	ptr = ptr+2 - (0x10000-(data[ptr+1]*256 + data[ptr+2])) - 2;
	mdx->track[t].infinite_loop_times++;
	follower = 2;
      }
      break;
 
    case MDX_KEY_ON_DELAY:
      follower = 1;
      break;

    case MDX_SEND_SYNC:
      send_sync( data[ptr+1] );
      follower = 1;
      break;

    case MDX_RECV_SYNC:
      recv_sync( t );
      follower = 0;
      count = 1;
      break;

    case MDX_SET_FREQ:
      set_freq( t, data[ptr+1] );
      follower = 1;
      break;

    case MDX_SET_PLFO:
      if ( data[ptr+1] == 0x80 || data[ptr+1] == 0x81 ) {
	set_plfo_onoff( t, data[ptr+1]-0x80 );
	follower = 1;
      } else {
	set_plfo( t,
		  data[ptr+1], data[ptr+2], data[ptr+3],
		  data[ptr+4], data[ptr+5] );
	follower = 5;
      }
      break;

    case MDX_SET_ALFO:
      if ( data[ptr+1] == 0x80 || data[ptr+1] == 0x81 ) {
	set_alfo_onoff( t, data[ptr+1]-0x80 );
	follower = 1;
      } else {
	set_alfo( t,
		  data[ptr+1], data[ptr+2], data[ptr+3],
		  data[ptr+4], data[ptr+5] );
	follower = 5;
      }
      break;

    case MDX_SET_OPMLFO:
      if ( data[ptr+1] == 0x80 || data[ptr+1] == 0x81 ) {
	set_hlfo_onoff( t, data[ptr+1]-0x80 );
	follower = 1;
      } else {
	set_hlfo( t,
		  data[ptr+1], data[ptr+2], data[ptr+3],
		  data[ptr+4], data[ptr+5] );
	follower = 5;
      }
      break;

    case MDX_SET_LFO_DELAY:
      set_lfo_delay( t, data[ptr+1] );
      follower = 1;
      break;

    case MDX_SET_PCM8_MODE:
      follower = 0;
      break;

    case MDX_FADE_OUT:
      if ( data[ptr+1]==0x00 ) {
	follower = 1;
	set_fade_out( 5 );
      } else {
	follower = 2;
	set_fade_out( data[ptr+2] );
      }
      break;

    default:
      count = -1;
      break;
    }
  }

  ptr += 1+follower;
  mdx->track[t].current_mml_ptr = ptr;

  return count;
}


static void
set_tempo( int val )
{
  __GETMDX;

  if (val<2) return; /* workaround!!! */
  mdx->tempo = val;
  return;
}

static void
note_off( int track )
{
  __GETMDX;

  if ( mdx->track[track].keyoff_disable == FLAG_FALSE ) {
    mdx->track[track].note=-1;
    if ( track<8 )
      ym2151_note_off(track);
    else
      pcm8_note_off(track-8);
  }
  mdx->track[track].keyoff_disable = FLAG_FALSE;

  return;
}

static void
note_on( int track, int note )
{
  int last_note;
  PDX_DATA* pdx;
  __GETMDX;

  pdx = self->pdx;

  last_note = mdx->track[track].note;
  note -= MDX_MIN_NOTE;
  mdx->track[track].note = note;

  if ( mdx->track[track].phase != MDX_PAN_N ) {
    if ( track<8 ){
      ym2151_note_on(track, mdx->track[track].note);
    }
    else {
      if ( note<MDX_MAX_PDX_TONE_NUMBER && self->pcm8_disable == FLAG_FALSE && pdx != NULL ) {
	int n = note+mdx->track[track].voice*MDX_MAX_PDX_TONE_NUMBER;
	pcm8_note_on( track-8,
		      pdx->tone[n].data,
		      (int)pdx->tone[n].size, 
		      pdx->tone[n].orig_data,
		      (int)pdx->tone[n].orig_size ); 
      }
    }
  }

  return;
}

static void
do_quantize( int t, int count )
{
  int gate;

  __GETMDX;

  if ( mdx->track[t].keyoff_disable == FLAG_TRUE ) {
    gate = count+2;
    mdx->track[t].keyoff_disable = FLAG_FALSE;

  } else {
    if ( mdx->track[t].quantize2>0 ) {
      gate = count - mdx->track[t].quantize2;
      if ( gate <= 0 ) gate=1;

    } else {
      gate = count * mdx->track[t].quantize1/8;
    }
  }

  /* FIXME : we must tune up the note-off job */
  if ( gate+1 < count )
    gate++;

  mdx->track[t].gate = gate;

  return;
}

static void
set_phase( int track, int val )
{
  __GETMDX;

  mdx->track[track].phase = val;

  if ( track<8 )
    ym2151_set_pan(track, val);
  else
    pcm8_set_pan(val);

  if ( val == MDX_PAN_N ) {
    if ( track<8 )
      ym2151_note_off(track);
    else
      pcm8_note_off(track-8);
  }

  return;
}

static void
set_volume( int track, int val )
{
  int v;
  int m;
  __GETMDX;

  if ( val < 0x10 ) {
    mdx->track[track].volume_normal = val;
    v = vol_conv[val];
    m = FLAG_TRUE;
  }
  else if ( val >= 0x80 ) {
    v = 255-val;
    m = FLAG_FALSE;
  }
  else {
    v=0;
    m=FLAG_FALSE;
  }
  mdx->track[track].volume = v;
  mdx->track[track].last_volume_normal = m;

  if ( track<8 )
    ym2151_set_volume( track, v );
  else
    pcm8_set_volume( track-8, v );

  return;
}

static void
inc_volume( int track )
{
  __GETMDX;

  if ( mdx->track[track].last_volume_normal == FLAG_TRUE ) {
    if ( ++mdx->track[track].volume_normal > 0x0f ) {
      mdx->track[track].volume_normal = 0x0f;
    }
    mdx->track[track].volume = vol_conv[mdx->track[track].volume_normal];
  }
  else {
    if ( ++mdx->track[track].volume > 0x7f ) {
      mdx->track[track].volume = 0x7f;
    }
  }

  if ( track<8 )
    ym2151_set_volume( track, mdx->track[track].volume );
  else
    pcm8_set_volume( track-8, mdx->track[track].volume );

  return;
}

static void
dec_volume( int track )
{
  __GETMDX;

  if ( mdx->track[track].last_volume_normal == FLAG_TRUE) {
    if ( --mdx->track[track].volume_normal < 0 ) {
      mdx->track[track].volume_normal = 0;
    }
    mdx->track[track].volume = vol_conv[mdx->track[track].volume_normal];
  }
  else {
    if ( --mdx->track[track].volume < 0 ) {
      mdx->track[track].volume = 0;
    }
  }

  if ( track<8 )
    ym2151_set_volume( track, mdx->track[track].volume );
  else
    pcm8_set_volume( track-8, mdx->track[track].volume );

  return;
}

static void
set_keyoff( int track )
{
  __GETMDX;

  mdx->track[track].keyoff_disable = FLAG_TRUE;

  return;
}

static void
set_voice( int track, int val )
{
  __GETMDX;

  if ( track<8 && val >= MDX_MAX_VOICE_NUMBER ) return;
  if ( track>7 && val >= MDX_MAX_PDX_BANK_NUMBER ) return;
  mdx->track[track].voice = val;
  if ( track<8 ) {
    ym2151_set_voice( track, &mdx->voice[val] );
  }

  return;
}

static void
set_quantize( int track, int val )
{
  __GETMDX;

  if ( val < 9 ) {
    mdx->track[track].quantize1 = val;
    mdx->track[track].quantize2 = 0;
  }
  else {
    mdx->track[track].quantize1 = 8;
    mdx->track[track].quantize2 = 0x100-val;
  }

  return;
}

static void
set_detune( int track, int v1, int v2 )
{
  int v;
  __GETMDX;

  if ( track<8 ) {
    v = v1*256 + v2;
    if ( v1 >= 0x80 ) v = v-0x10000;
    mdx->track[track].detune = v;
    ym2151_set_detune( track, v );
  }

  return;
}

static void
set_portament( int track, int v1, int v2 )
{
  int v;
  __GETMDX;

  if ( track<8 ) {
    v = v1*256 + v2;
    if ( v1 >= 0x80 ) v = v - 0x10000;
    mdx->track[track].portament = v;
    ym2151_set_portament( track, v );
  }

  return;
}

static void
set_freq( int track, int val )
{
  __GETMDX;

  if ( track >= 8 ) {
    pcm8_set_pcm_freq( track-8, val );
  }
  else {
    ym2151_set_noise_freq( val );
  }
  return;
}

static void
set_fade_out( int speed )
{
  __GETSELF;

  self->fade_out = speed+1;
  return;
}

static void
set_opm_reg( int track, int v1, int v2 )
{
  __GETMDX;

  if ( track<8 ) {
    ym2151_set_reg( v1, v2 );
  }
  if ( v1 == 0x12 ) {
    mdx->tempo = v2;
  }

  return;
}

static void
send_sync( int track )
{
  __GETMDX;

  mdx->track[track].waiting_sync = FLAG_FALSE;
  return;
}

static void
recv_sync( int track )
{
  __GETMDX;

  mdx->track[track].waiting_sync = FLAG_TRUE;
  return;
}

static void
set_plfo_onoff( int track, int onoff )
{
  __GETMDX;

  if ( onoff == 0 ) {
    mdx->track[track].p_lfo_flag = FLAG_FALSE;
  }
  else {
    mdx->track[track].p_lfo_flag = FLAG_TRUE;
  }

  if ( track < 8) {
    ym2151_set_plfo( track,
		     mdx->track[track].p_lfo_flag,
		     mdx->track[track].p_lfo_form,
		     mdx->track[track].p_lfo_clock,
		     mdx->track[track].p_lfo_depth );
  }

  return;
}

static void
set_plfo( int track, int v1, int v2, int v3, int v4, int v5 )
{
  int t,d;
  __GETMDX;

  t = v2*256+v3;
  d = v4*256+v5;
  if ( d>=0x8000 ) {
    d = d-0x10000;
  }
  if ( v1 > 4  ) d*=256;
  /*fprintf(stderr,"%d %d %d %d\n", track, v1, t, d);fflush(stderr);*/
  mdx->track[track].p_lfo_form  = v1;
  mdx->track[track].p_lfo_clock = t;
  mdx->track[track].p_lfo_depth = d;
  mdx->track[track].p_lfo_flag  = FLAG_TRUE;

  if ( track < 8 ) {
    ym2151_set_plfo( track,
		     mdx->track[track].p_lfo_flag,
		     mdx->track[track].p_lfo_form,
		     mdx->track[track].p_lfo_clock,
		     mdx->track[track].p_lfo_depth );
  }

  return;
}

static void
set_alfo_onoff( int track, int onoff )
{
  __GETMDX;

  if ( onoff == 0 ) {
    mdx->track[track].a_lfo_flag = FLAG_FALSE;
  }
  else {
    mdx->track[track].a_lfo_flag = FLAG_TRUE;
  }

  if ( track < 8 ) {
    ym2151_set_alfo( track,
		     mdx->track[track].a_lfo_flag,
		     mdx->track[track].a_lfo_form,
		     mdx->track[track].a_lfo_clock,
		     mdx->track[track].a_lfo_depth );
  }
  return;
}

static void
set_alfo( int track, int v1, int v2, int v3, int v4, int v5 )
{
  int t,d;
  __GETMDX;

  t = v2*256+v3;
  d = v4*256+v5;
  if ( v4>=0x80 ) {
    d = d-0x10000;
  }
  mdx->track[track].a_lfo_form  = v1;
  mdx->track[track].a_lfo_clock = t;
  mdx->track[track].a_lfo_depth = d;
  mdx->track[track].a_lfo_flag  = FLAG_TRUE;

  if ( track < 8 ) {
    ym2151_set_alfo( track,
		     mdx->track[track].a_lfo_flag,
		     mdx->track[track].a_lfo_form,
		     mdx->track[track].a_lfo_clock,
		     mdx->track[track].a_lfo_depth );
  }

  return;
}

static void
set_hlfo_onoff( int track, int onoff )
{
  __GETMDX;

  if ( onoff == 0 ) {
    mdx->track[track].h_lfo_flag = FLAG_FALSE;
  }
  else {
    mdx->track[track].h_lfo_flag = FLAG_TRUE;
  }

  if ( track < 8 ) {
    ym2151_set_hlfo_onoff( track, mdx->track[track].h_lfo_flag );
  }

  return;
}

static void
set_hlfo( int track, int v1, int v2, int v3, int v4, int v5 )
{
  int wa, sp, pd, ad, ps, as, sy;
  __GETMDX;

  wa = v1&0x03;
  sp = v2;
  pd = v3 & 0x7f;
  ad = v4;
  ps = (v5/16) & 0x07;
  as = v5 & 0x03;
  sy = (v1/64) & 0x01;

  /*fprintf(stderr,"%d %d %d %d\n", track, v1, v2, v3);fflush(stderr);*/
  if ( track < 8 ) {
    ym2151_set_hlfo( track, v1, v2, v3, v4, v5 );
  }

  return;
}

static void
set_lfo_delay( int track, int delay )
{
  __GETMDX;

  mdx->track[track].lfo_delay = delay;

  if ( track < 8 ) {
    ym2151_set_lfo_delay( track, delay );
  }
  return;
}
