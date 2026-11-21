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

#ifdef USE_NLG

#include "nlg.h"
extern NLGCTX *nlgctx;

#endif


/* ------------------------------------------------------------------ */
/* export symbols */

int mdx_parse_mml_ym2151( MDX_DATA *, PDX_DATA *, songdata * );
int mdx_init_track_work_area_ym2151( songdata * );

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

  /* Song loop bookkeeping (see mdx_get_loop_*). The region is measured from the
     tracks that actually loop: a track that never loops (a one-shot) must not
     gate the song's loop count, or the min-over-tracks stalls at 0 until that
     track ends and misreads its dropout as the first loop. */
  int64_t track_first_us[MDX_MAX_TRACK_NUMBER];  /* per-track 1st loop, us */
  int64_t track_second_us[MDX_MAX_TRACK_NUMBER]; /* per-track 2nd loop, us */
  int64_t loop_first_us;   /* elapsed at the 1st loop (intro + loop) */
  int64_t loop_second_us;  /* elapsed at the 2nd loop (intro + 2 * loop) */
  int loops_per_section;   /* per-track counters one joint section is worth (L / pmax) */
  int gate_reduced;        /* the gate is not the raw per-track minimum, so the length
                              parse (which resolves this only afterwards) must be
                              re-measured on the grid the gate actually uses */
  int loop_region_bad;     /* no usable region: the getters report none */
} mdxmml_ym2151_instances;

/* ------------------------------------------------------------------ */
/* local functions */

static int  set_new_event( int, songdata *, int muted );
static void set_tempo( int, songdata * );
static void note_off( int, songdata * );
static void note_on( int, int, songdata * );
static void set_volume( int, int, songdata * );
static void inc_volume( int, songdata * );
static void dec_volume( int, songdata * );
static void set_phase( int, int, songdata * );
static void set_keyoff( int, songdata * );
static void set_voice( int, int, songdata * );
static void set_quantize( int, int, songdata * );
static void set_detune( int, int, int, songdata * );
static void set_portament( int, int, int, songdata * );
static void set_freq( int, int, songdata * );
static void set_opm_reg( int, int, int, songdata * );
static void set_fade_out( int, songdata * );
static void send_sync( int, songdata * );
static void recv_sync( int, songdata * );

static void set_plfo_onoff( int, int, songdata * );
static void set_plfo( int, int, int, int, int, int, songdata * );
static void set_alfo_onoff( int, int, songdata * );
static void set_alfo( int, int, int, int, int, int, songdata * );
static void set_hlfo_onoff( int, int, songdata * );
static void set_hlfo( int, int, int, int, int, int, songdata * );
static void set_lfo_delay( int, int, songdata * );

static void do_quantize( int, int, songdata * );

/* ------------------------------------------------------------------ */
/* class interface */

extern void* _get_mdxmml_ym2151(songdata *data);

static mdxmml_ym2151_instances*
__get_instances(songdata *data)
{
  return (mdxmml_ym2151_instances*)_get_mdxmml_ym2151(data);
}

#define __GETSELF(data)  mdxmml_ym2151_instances* self = __get_instances(data);

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
mdx_parse_mml_ym2151( MDX_DATA *orig_mdx, PDX_DATA *orig_pdx, songdata *data )
{
  int i;
  long count;
  int all_track_finished;
  int fade_out_wait;
  int master_volume;
  int infinite_loops;

  __GETSELF(data);

  self->mdx = orig_mdx;
  self->pdx = orig_pdx;

  mdx_init_track_work_area_ym2151(data);

  self->pcm8_disable=FLAG_TRUE;
  if ( pcm8_open(self->mdx, data)==0 ) {
    self->pcm8_disable=FLAG_FALSE;
  }

  if (!ym2151_reg_init( self->mdx, data )) {
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
    ym2151_set_master_volume( master_volume * self->mdx->fm_volume / 127, data );
    pcm8_set_master_volume( master_volume * self->mdx->pcm_volume / 127, data );

    all_track_finished=FLAG_TRUE;
    infinite_loops = 32767; /* large enough */
    for ( i=0 ; i<self->mdx->tracks ; i++ ) {

      if ( self->mdx->track[i].waiting_sync == FLAG_TRUE )
	{ continue; }

      count = self->mdx->track[i].counter;
      if ( count < 0 ) { continue; } /* this track has finished */
      all_track_finished=FLAG_FALSE;

      self->mdx->track[i].gate--;
      if ( self->mdx->track[i].gate == 0 ) { note_off( i, data ); }

      if ( i<8 ) {
	ym2151_set_freq_volume( i, data ); /* do portament, lfo, detune */
      }

      count--;
      while ( count == 0 ) {
	count=set_new_event( i, data, 0 );
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

	do_pcm8(NULL,-1, data);
    
	}

  ym2151_all_note_off(data);
  pcm8_close(data);
  ym2151_shutdown(data);

  return 0;
}




void*
mdx_parse_mml_ym2151_async_initialize(MDX_DATA *orig_mdx, PDX_DATA *orig_pdx, songdata *data)
{
  __GETSELF(data);

  self->mdx = orig_mdx;
  self->pdx = orig_pdx;

  mdx_init_track_work_area_ym2151(data);

  self->pcm8_disable=FLAG_TRUE;
  if ( pcm8_open(self->mdx, data)==0 ) {
    self->pcm8_disable=FLAG_FALSE;
  }

  if (!ym2151_reg_init( self->mdx, data )) {
    /* failed to initialize opm! */
    return NULL;
  }

  /* start parsing */

  self->all_track_finished=FLAG_FALSE;
  self->fade_out=0;
  self->fade_out_wait=0;
  self->master_volume=127;

  return (void *)self;
}

void
mdx_parse_mml_ym2151_async_finalize(songdata *data)
{
  ym2151_all_note_off(data);
  pcm8_close(data);
  ym2151_shutdown(data);
}

/* The quantity the fade gate compares against max_infinite_loops: how many joint
   sections the song has finished.

   The engine's own quantity is a per-track loop count, and one section is
   several repeats of a shorter track, so a limiting count compared against raw
   counters fires early. Two things have to come out of each counter:

   - its repeats per section (L / p), and
   - the loops it ran *before the joint start*. The region starts at the last
     track to enter its loop (see the resolver), so a track that entered earlier
     has that many loops already on the books, and its count alone reads as
     sections it has not finished, so the fade can begin a whole section early.

   Tracks with no measured period (a one-shot that never loops) contribute their
   raw count, as they always did. */
static int
loop_gate_value(mdxmml_ym2151_instances *self, int infinite_loops)
{
  int64_t L, intro;
  int i, gate = -1;

  if (self->loop_region_bad)
    return infinite_loops;
  L = self->loop_second_us - self->loop_first_us;
  intro = self->loop_first_us - L;   /* the getters' anchor, minus one section */
  if (L <= 0)
    return infinite_loops;

  for (i = 0; i < self->mdx->tracks; i++) {
    int64_t p, s;
    if (self->mdx->track[i].waiting_sync == FLAG_TRUE) continue;
    if (self->mdx->track[i].counter < 0) continue;   /* finished */
    p = self->track_second_us[i] - self->track_first_us[i];
    if (self->track_second_us[i] <= 0 || p <= 0) {
      s = self->mdx->track[i].infinite_loop_times;   /* never loops: raw count */
    } else {
      int64_t r = (L + p / 2) / p;    /* this track's repeats in one section */
      int64_t start, pre, n;
      if (r <= 0) r = 1;
      start = self->track_first_us[i] - p;           /* where its loop began */
      pre = intro > start ? (intro - start) / p : 0; /* loops before the joint start */
      n = self->mdx->track[i].infinite_loop_times - pre;
      if (n < 0) n = 0;
      s = n / r;
    }
    if (gate < 0 || s < gate) gate = (int)s;
  }
  return gate < 0 ? infinite_loops : gate;
}

int
mdx_parse_mml_ym2151_async(songdata *data, int track_mute_mask)
{
  int i;
  long count;
  int infinite_loops;
  __GETSELF(data)

  pcm8_clear_buffer_flush_flag(data);
 loop:
  if (self->all_track_finished==FLAG_TRUE) {
    return FLAG_FALSE;
  }

  if ( self->fade_out > 0 ) {
    if ( self->master_volume <= 0 ) { self->all_track_finished = FLAG_TRUE; return FLAG_FALSE; }
    if ( self->fade_out_wait==0 ) { self->fade_out_wait = self->fade_out; }
    self->fade_out_wait--;
    if ( self->fade_out_wait==0 ) { self->master_volume--; }
    if ( self->master_volume==0 ) { self->all_track_finished = FLAG_TRUE; return FLAG_FALSE; }
  }
  ym2151_set_master_volume( self->master_volume * self->mdx->fm_volume / 127, data );
  pcm8_set_master_volume( self->master_volume * self->mdx->pcm_volume / 127, data );

  self->all_track_finished=FLAG_TRUE;
  infinite_loops = 32767; /* large enough */

  for ( i=0 ; i<self->mdx->tracks ; i++ ) {

    if ( self->mdx->track[i].waiting_sync == FLAG_TRUE )
      { continue; }

    count = self->mdx->track[i].counter;
    if ( count < 0 ) { continue; } /* this track has finished */
    self->all_track_finished=FLAG_FALSE;
    
    self->mdx->track[i].gate--;
    if ( self->mdx->track[i].gate == 0 ) { note_off( i, data ); }
    
    if ( i<8 ) {
      ym2151_set_freq_volume( i, data ); /* do portament, lfo, detune */
    }

    count--;
    while ( count == 0 ) {
      count=set_new_event( i, data, track_mute_mask & (1 << i) );
    }

    self->mdx->track[i].counter = count;
    if ( infinite_loops > self->mdx->track[i].infinite_loop_times ) {
      infinite_loops = self->mdx->track[i].infinite_loop_times;
    }
  }

  if ( self->mdx->max_infinite_loops > 0 ) {
    if ( loop_gate_value(self, infinite_loops) >= self->mdx->max_infinite_loops ) {
      self->fade_out = self->mdx->fade_out_speed;
    }
  }

  /* Per-track loop times: when each track completes its 1st and 2nd loop. The
     song-wide region is resolved from these after the parse, using only tracks
     that loop at all (mdx_parse_mml_ym2151_async_get_length). A one-shot track
     is excluded, so its end cannot masquerade as a loop crossing -- the reason
     the min-over-tracks approach mislabels the boundary (a one-shot sits at
     count 0 and holds the minimum until it ends). */
  for ( i = 0; i < self->mdx->tracks; i++ ) {
    int c = self->mdx->track[i].infinite_loop_times;
    if ( c >= 1 && self->track_first_us[i] == 0 )
      self->track_first_us[i] = self->mdx->elapsed_time;
    if ( c >= 2 && self->track_second_us[i] == 0 )
      self->track_second_us[i] = self->mdx->elapsed_time;
  }

  /* timer count */

  self->mdx->total_count++;
  self->mdx->elapsed_time += 1000*1024*(256 - self->mdx->tempo)/4000;

  return FLAG_TRUE;
}

/* Rewind the song to the start of a parse (or of playback). mdx_get_length may
   measure twice -- see its comment -- and mdx_parse_mml_ym2151_async leaves the
   tracks mid-song; this is the same reinit the single-pass version did once at
   the end. Returns 0 when the OPM registers fail to initialize, as that did. */
static int
mdx_parse_mml_ym2151_async_rewind(songdata *data)
{
  __GETSELF(data)

  ym2151_all_note_off(data);
  mdx_init_track_work_area_ym2151(data);
  pcm8_init(data);

  if (!ym2151_reg_init( self->mdx, data )) {
    /* failed to initialize opm! */
    return 0;
  }

  self->all_track_finished = FLAG_FALSE;
  self->fade_out_wait = 0;
  self->master_volume = 127;

  return 1;
}

int mdx_parse_mml_ym2151_async_get_length(songdata *data)
{
  int next,sec,i;
  
  __GETSELF(data)
  
  /* A fresh measurement: the parse below may run on a reused context (the app
     keeps one and reopens it per song), so clear last time's loop times. */
  memset(self->track_first_us, 0, sizeof self->track_first_us);
  memset(self->track_second_us, 0, sizeof self->track_second_us);
  self->loop_first_us = 0;
  self->loop_second_us = 0;
  self->loops_per_section = 0;
  self->gate_reduced = 0;
  self->loop_region_bad = 0;

  next = 1;
  while(next && self->mdx->elapsed_time < (1200 * 1000000))
  {
	next = mdx_parse_mml_ym2151_async(data, 0);
  }
  
  sec = (int)self->mdx->elapsed_time / 1000000;

  /* Resolve the loop region from the tracks that loop. The region is the mix's
     repeat, and its two numbers are different in kind: the length L is the joint
     period -- the smallest multiple of the longest track period that every
     looping track also hits -- while the start is the last track to *enter* its
     loop section, which is that track's first crossing less its own period, not
     the crossing itself. Subtracting L from a crossing only cancels when the
     tracks share a period, so a mix whose loop sections start apart, or whose
     periods need k > 1 to meet (4s and 6s: L is 12), put the band early, over
     audio that does not repeat. Tracks that loop together differ by a tick or
     two, so a small tolerance absorbs the measurement slack instead of demanding
     equality. If no small common multiple exists the tracks are incommensurate:
     report no region (the getters return 0 and the app shows the blind-loop UI)
     rather than fold the head into a made-up boundary -- a wrong band is worse
     than none. Every looping track must also have reached its 2nd loop, or its
     period is not measurable. */
  {
    int64_t intro = 0, pmax = 0, tol = 0, L = 0;
    int loopers = 0, all_second = 1, periods = 0, k = 0;
    for ( i = 0; i < self->mdx->tracks; i++ ) {
      int64_t p;
      if ( self->mdx->track[i].infinite_loop_times <= 0 )
        continue;
      loopers++;
      if ( self->track_second_us[i] == 0 ) {
        all_second = 0;
        continue;
      }
      p = self->track_second_us[i] - self->track_first_us[i];
      if ( periods == 0 || p > pmax ) pmax = p;
      periods++;
      /* Its first crossing less its own period: where this track's loop began. */
      if ( p > 0 && self->track_first_us[i] - p > intro )
        intro = self->track_first_us[i] - p;
    }
    tol = pmax / 50;            /* ~2% of the longest period */
    if ( tol < 200000 ) tol = 200000;  /* 200 ms floor */
    if ( periods > 0 ) {
      for ( k = 1; k <= 16 && L == 0; k++ ) {
        int64_t cand = pmax * k, ok = 1;
        for ( i = 0; i < self->mdx->tracks; i++ ) {
          int64_t p, r, d;
          if ( self->mdx->track[i].infinite_loop_times <= 0 ) continue;
          if ( self->track_second_us[i] == 0 ) continue;
          p = self->track_second_us[i] - self->track_first_us[i];
          if ( p <= 0 ) { ok = 0; break; }
          r = ( cand + p / 2 ) / p;
          d = cand - r * p; if ( d < 0 ) d = -d;
          if ( d > tol ) { ok = 0; break; }
        }
        if ( ok ) L = cand;
      }
    }
    /* loop_first is the anchor the getters subtract L from (see mdxmini.c), so
       the start they report is the intro above. */
    self->loop_first_us  = intro + L;
    self->loop_second_us = intro + 2 * L;
    /* One joint section is L / pmax of the engine's own counters -- the multiple
       the resolver just proved every looping track hits. Measured here, from the
       parse's own data: the live counters are 0 until a track's first boundary,
       so reading them instead would miss a longer-period track early on. */
    self->loops_per_section = ( pmax > 0 && L > 0 ) ? (int)(L / pmax) : 0;
    self->loop_region_bad =
      ( loopers == 0 || !all_second || periods == 0 || L <= 0 || intro + L <= 0 ) ? 1 : 0;
    /* Whether the gate is anything other than the raw per-track minimum: some
       track needs the section division, or ran loops before the joint start. The
       gate itself computes both every time; this is the same test, hoisted so the
       length parse below can know to re-measure on the gate's own grid. */
    if (!self->loop_region_bad) {
      for ( i = 0; i < self->mdx->tracks; i++ ) {
        int64_t p, r;
        if ( self->mdx->track[i].infinite_loop_times <= 0 ) continue;
        if ( self->track_second_us[i] == 0 ) continue;
        p = self->track_second_us[i] - self->track_first_us[i];
        if ( p <= 0 ) continue;
        r = ( L + p / 2 ) / p;
        if ( r > 1 || self->track_first_us[i] - p < intro ) {
          self->gate_reduced = 1;
          break;
        }
      }
    }
  }

  /* The parse above measured the length with the fade gate on the raw per-track
     minimum: the region is resolved only here, so loop_gate_value has nothing to
     reduce during that parse. When the gate is not that raw minimum it turns a
     section's worth (or a track's head start) early, so sec is short of the
     section end -- and MDXPlayer arms its end detector on sec, stopping the song
     mid-fade. Re-measure with the grid resolved, which is the quantity playback
     gates on, so sec is the length the engine plays. */
  if (!self->loop_region_bad && self->gate_reduced) {
    if (!mdx_parse_mml_ym2151_async_rewind(data))
      return 0;
    next = 1;
    while(next && self->mdx->elapsed_time < (1200 * 1000000))
    {
      next = mdx_parse_mml_ym2151_async(data, 0);
    }
    sec = (int)self->mdx->elapsed_time / 1000000;
  }

  /* stop and reinitialize for playback */
  if (!mdx_parse_mml_ym2151_async_rewind(data))
    return 0;

  return sec;
}

int
mdx_parse_mml_ym2151_make_samples(short *buffer,int buffer_size, songdata *data)
{
   do_pcm8(buffer,buffer_size, data);
   return 0;
}


int
mdx_parse_mml_get_tempo(void* in_self)
{
  mdxmml_ym2151_instances* self = (mdxmml_ym2151_instances *)in_self;

 return 1000*1024*(256 - self->mdx->tempo)/4000;
}

int64_t
mdx_parse_mml_get_loop_first_us(void* in_self)
{
  if (!in_self) return 0;
  if (((mdxmml_ym2151_instances *)in_self)->loop_region_bad) return 0;
  return ((mdxmml_ym2151_instances *)in_self)->loop_first_us;
}

int64_t
mdx_parse_mml_get_loop_second_us(void* in_self)
{
  if (!in_self) return 0;
  if (((mdxmml_ym2151_instances *)in_self)->loop_region_bad) return 0;
  return ((mdxmml_ym2151_instances *)in_self)->loop_second_us;
}

/* ------------------------------------------------------------------ */

#define __GETMDX(data) \
MDX_DATA* mdx; \
__GETSELF(data); \
mdx = self->mdx;

int
mdx_init_track_work_area_ym2151( songdata *data )
{
  int i;

  __GETMDX(data);

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
      ym2151_set_detune(i,0, data);
    mdx->track[i].portament     = 0;
    if ( i<8 )
      ym2151_set_portament(i,0, data);

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
    
  set_tempo(mdx->tempo, data);

  return 0;
}

static int
set_new_event( int t, songdata *songdata, int muted )
{
  int ptr;
  unsigned char *data;
  int count;
  int follower;
  __GETMDX(songdata);

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
    note_off(t, songdata);
    count = data[ptr]+1;
    mdx->track[t].gate = count+1;
    follower=0;

  } else if ( data[ptr] <= MDX_MAX_NOTE) { /* note */
    if (muted == 0) {
      note_on(t, data[ptr], songdata);
    }
    count = data[ptr+1]+1;
    do_quantize( t, count, songdata );
    follower = 1;

  } else {
    switch ( data[ptr] ) {

    case MDX_SET_TEMPO:
      set_tempo( data[ptr+1], songdata );
      follower = 1;
      break;

    case MDX_SET_OPM_REG:
      set_opm_reg( t, data[ptr+1], data[ptr+2], songdata );
      follower = 2;
      break;

    case MDX_SET_VOICE:
      set_voice( t, data[ptr+1], songdata );
      follower = 1;
      break;

    case MDX_SET_PHASE:
      set_phase( t, data[ptr+1], songdata );
      follower = 1;
      break;

    case MDX_SET_VOLUME:
      set_volume( t, data[ptr+1], songdata );
      follower = 1;
      break;

    case MDX_VOLUME_DEC:
      dec_volume( t, songdata );
      follower = 0;
      break;

    case MDX_VOLUME_INC:
      inc_volume( t, songdata );
      follower = 0;
      break;

    case MDX_SET_QUANTIZE:
      set_quantize( t, data[ptr+1], songdata );
      follower = 1;
      break;

    case MDX_SET_KEYOFF:
      set_keyoff( t, songdata );
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
      set_detune( t, data[ptr+1], data[ptr+2], songdata );
      follower = 2;
      break;

    case MDX_SET_PORTAMENT:
      set_portament( t, data[ptr+1], data[ptr+2], songdata );
      follower = 2;
      break;

    case MDX_DATA_END:
      if ( data[ptr+1] == 0x00 ) {
	count = -1;
	note_off(t, songdata);
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
      send_sync( data[ptr+1], songdata );
      follower = 1;
      break;

    case MDX_RECV_SYNC:
      recv_sync( t, songdata );
      follower = 0;
      count = 1;
      break;

    case MDX_SET_FREQ:
      set_freq( t, data[ptr+1], songdata );
      follower = 1;
      break;

    case MDX_SET_PLFO:
      if ( data[ptr+1] == 0x80 || data[ptr+1] == 0x81 ) {
	set_plfo_onoff( t, data[ptr+1]-0x80, songdata );
	follower = 1;
      } else {
	set_plfo( t,
		  data[ptr+1], data[ptr+2], data[ptr+3],
		  data[ptr+4], data[ptr+5], songdata );
	follower = 5;
      }
      break;

    case MDX_SET_ALFO:
      if ( data[ptr+1] == 0x80 || data[ptr+1] == 0x81 ) {
	set_alfo_onoff( t, data[ptr+1]-0x80, songdata );
	follower = 1;
      } else {
	set_alfo( t,
		  data[ptr+1], data[ptr+2], data[ptr+3],
		  data[ptr+4], data[ptr+5], songdata );
	follower = 5;
      }
      break;

    case MDX_SET_OPMLFO:
      if ( data[ptr+1] == 0x80 || data[ptr+1] == 0x81 ) {
	set_hlfo_onoff( t, data[ptr+1]-0x80, songdata );
	follower = 1;
      } else {
	set_hlfo( t,
		  data[ptr+1], data[ptr+2], data[ptr+3],
		  data[ptr+4], data[ptr+5], songdata );
	follower = 5;
      }
      break;

    case MDX_SET_LFO_DELAY:
      set_lfo_delay( t, data[ptr+1], songdata );
      follower = 1;
      break;

    case MDX_SET_PCM8_MODE:
      follower = 0;
      break;

    case MDX_FADE_OUT:
      if ( data[ptr+1]==0x00 ) {
	follower = 1;
	set_fade_out( 5, songdata );
      } else {
	follower = 2;
	set_fade_out( data[ptr+2], songdata );
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
set_tempo( int val, songdata *data )
{
  __GETMDX(data);

  if (val<2) return; /* workaround!!! */

  mdx->tempo = val;
    return;
}

static void
note_off( int track, songdata *data )
{
  __GETMDX(data);

  if ( mdx->track[track].keyoff_disable == FLAG_FALSE ) {
    mdx->track[track].note=-1;
    if ( track<8 )
      ym2151_note_off(track, data);
    else
      pcm8_note_off(track-8, data);
  }
  mdx->track[track].keyoff_disable = FLAG_FALSE;

  return;
}

static void
note_on( int track, int note, songdata *data )
{
  int last_note;
  PDX_DATA* pdx;
  __GETMDX(data);

  pdx = self->pdx;

  last_note = mdx->track[track].note;
  note -= MDX_MIN_NOTE;
  mdx->track[track].note = note;

  if ( mdx->track[track].phase != MDX_PAN_N ) {
    if ( track<8 ){
      ym2151_note_on(track, mdx->track[track].note, data);
    }
    else {
      if ( note<MDX_MAX_PDX_TONE_NUMBER && self->pcm8_disable == FLAG_FALSE && pdx != NULL ) {
	int n = note+mdx->track[track].voice*MDX_MAX_PDX_TONE_NUMBER;
	pcm8_note_on( track-8,
		      pdx->tone[n].data,
		      (int)pdx->tone[n].size, 
		      pdx->tone[n].orig_data,
		      (int)pdx->tone[n].orig_size, data ); 
      }
    }
  }

  return;
}

static void
do_quantize( int t, int count, songdata *data )
{
  int gate;

  __GETMDX(data);

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
set_phase( int track, int val, songdata *data )
{
  __GETMDX(data);

  mdx->track[track].phase = val;

  if ( track<8 )
    ym2151_set_pan(track, val, data);
  else
    pcm8_set_pan(val, data);

  if ( val == MDX_PAN_N ) {
    if ( track<8 )
      ym2151_note_off(track, data);
    else
      pcm8_note_off(track-8, data);
  }

  return;
}

static void
set_volume( int track, int val, songdata * data )
{
  int v;
  int m;
  __GETMDX(data);

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
    ym2151_set_volume( track, v, data );
  else
    pcm8_set_volume( track-8, v, data );

  return;
}

static void
inc_volume( int track, songdata *data )
{
  __GETMDX(data);

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
    ym2151_set_volume( track, mdx->track[track].volume, data );
  else
    pcm8_set_volume( track-8, mdx->track[track].volume, data );

  return;
}

static void
dec_volume( int track, songdata *data )
{
  __GETMDX(data);

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
    ym2151_set_volume( track, mdx->track[track].volume, data );
  else
    pcm8_set_volume( track-8, mdx->track[track].volume, data );

  return;
}

static void
set_keyoff( int track, songdata *data )
{
  __GETMDX(data);

  mdx->track[track].keyoff_disable = FLAG_TRUE;

  return;
}

static void
set_voice( int track, int val, songdata *data )
{
  __GETMDX(data);

  if ( track<8 && val >= MDX_MAX_VOICE_NUMBER ) return;
  if ( track>7 && val >= MDX_MAX_PDX_BANK_NUMBER ) return;
  mdx->track[track].voice = val;
  if ( track<8 ) {
    ym2151_set_voice( track, &mdx->voice[val], data );
  }

  return;
}

static void
set_quantize( int track, int val, songdata *data )
{
  __GETMDX(data);

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
set_detune( int track, int v1, int v2, songdata *data )
{
  int v;
  __GETMDX(data);

  if ( track<8 ) {
    v = v1*256 + v2;
    if ( v1 >= 0x80 ) v = v-0x10000;
    mdx->track[track].detune = v;
    ym2151_set_detune( track, v, data );
  }

  return;
}

static void
set_portament( int track, int v1, int v2, songdata *data )
{
  int v;
  __GETMDX(data);

  if ( track<8 ) {
    v = v1*256 + v2;
    if ( v1 >= 0x80 ) v = v - 0x10000;
    mdx->track[track].portament = v;
    ym2151_set_portament( track, v, data );
  }

  return;
}

static void
set_freq( int track, int val, songdata *data )
{
  __GETMDX(data);

  if ( track >= 8 ) {
    pcm8_set_pcm_freq( track-8, val, data );
  }
  else {
    ym2151_set_noise_freq( val, data );
  }
  return;
}

static void
set_fade_out( int speed, songdata *data )
{
  __GETSELF(data);

  self->fade_out = speed+1;
  return;
}

static void
set_opm_reg( int track, int v1, int v2, songdata *data )
{
  __GETMDX(data);

  if ( track<8 ) {
    ym2151_set_reg( v1, v2, data );
  }
  if ( v1 == 0x12 ) {
      set_tempo(v2, data);
  }

  return;
}

static void
send_sync( int track, songdata *data )
{
  __GETMDX(data);

  mdx->track[track].waiting_sync = FLAG_FALSE;
  return;
}

static void
recv_sync( int track, songdata *data )
{
  __GETMDX(data);

  mdx->track[track].waiting_sync = FLAG_TRUE;
  return;
}

static void
set_plfo_onoff( int track, int onoff, songdata *data )
{
  __GETMDX(data);

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
		     mdx->track[track].p_lfo_depth,
         data );
  }

  return;
}

static void
set_plfo( int track, int v1, int v2, int v3, int v4, int v5, songdata *data )
{
  int t,d;
  __GETMDX(data);

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
		     mdx->track[track].p_lfo_depth,
         data );
  }

  return;
}

static void
set_alfo_onoff( int track, int onoff, songdata *data )
{
  __GETMDX(data);

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
		     mdx->track[track].a_lfo_depth,
         data );
  }
  return;
}

static void
set_alfo( int track, int v1, int v2, int v3, int v4, int v5, songdata *data )
{
  int t,d;
  __GETMDX(data);

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
		     mdx->track[track].a_lfo_depth,
         data );
  }

  return;
}

static void
set_hlfo_onoff( int track, int onoff, songdata *data )
{
  __GETMDX(data);

  if ( onoff == 0 ) {
    mdx->track[track].h_lfo_flag = FLAG_FALSE;
  }
  else {
    mdx->track[track].h_lfo_flag = FLAG_TRUE;
  }

  if ( track < 8 ) {
    ym2151_set_hlfo_onoff( track, mdx->track[track].h_lfo_flag, data );
  }

  return;
}

static void
set_hlfo( int track, int v1, int v2, int v3, int v4, int v5, songdata *data )
{
  int wa, sp, pd, ad, ps, as, sy;
  __GETMDX(data);

  wa = v1&0x03;
  sp = v2;
  pd = v3 & 0x7f;
  ad = v4;
  ps = (v5/16) & 0x07;
  as = v5 & 0x03;
  sy = (v1/64) & 0x01;

  /*fprintf(stderr,"%d %d %d %d\n", track, v1, v2, v3);fflush(stderr);*/
  if ( track < 8 ) {
    ym2151_set_hlfo( track, v1, v2, v3, v4, v5, data );
  }

  return;
}

static void
set_lfo_delay( int track, int delay, songdata *data )
{
  __GETMDX(data);

  mdx->track[track].lfo_delay = delay;

  if ( track < 8 ) {
    ym2151_set_lfo_delay( track, delay, data );
  }
  return;
}
