/*
  MDXplayer :  YM2151 emulator access routines

  Made by Daisuke Nagano <breeze.nagano@nifty.ne.jp>
  Apr.16.1999
  
  independent from U*IX environment version by BKC
  Apr.??.2009
 */

/* ------------------------------------------------------------------ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "version.h"
#include "mdx.h"
#include "mdx2151.h"
#include "ym2151.h"
#include "pcm8.h"

#include "class.h"

#ifdef USE_NLG

#include "nlg.h"
extern NLGCTX *nlgctx;

#endif

/* ------------------------------------------------------------------ */

typedef struct _YM2151_LFO {

  int flag;

  int form;
  int clock;
  int depth;
  int depth2;

} YM2151_LFO;

typedef struct _DETUNE_VAL {

  int octave;
  int scale;
  int detune;

} DETUNE_VAL;

typedef struct _OPM_WORK {

  int        hlfo_flag;
  int        hlfo_val;
  int        lfo_delay;
  YM2151_LFO plfo;
  YM2151_LFO alfo;

  long       portament;
  DETUNE_VAL detune;
  int        note;
  int        note_on;
  int        volume;
  int        pan;
  int        total_level[4];
  int        step;
  int        lfo_step;
  int        freq_reg[3];
  int        algorithm;
  int        slot_mask;

} OPM_WORK;

/* ------------------------------------------------------------------ */

static const int ym2151_note[] ={
  0,1,2,4,5,6,8,9,10,12,13,14
};

static const int is_vol_set[8][4]={
  {0,0,0,1},
  {0,0,0,1},
  {0,0,0,1},
  {0,0,0,1},
  {0,0,1,1},
  {0,1,1,1},
  {0,1,1,1},
  {1,1,1,1}
};

static const int master_vol_table[128] = {
    0, 18, 28, 36, 42, 46, 51, 54, 57, 60, 62, 65, 67, 69, 70, 72,
   74, 75, 77, 78, 79, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 90,
   91, 92, 93, 93, 94, 95, 96, 96, 97, 97, 98, 99, 99,100,100,101,
  102,102,103,103,104,104,105,105,105,106,106,107,107,108,108,109,
  109,109,110,110,111,111,111,112,112,112,113,113,113,114,114,114,
  115,115,115,116,116,116,117,117,117,117,118,118,118,119,119,119,
  119,120,120,120,120,121,121,121,122,122,122,122,122,123,123,123,
  123,124,124,124,124,125,125,125,125,125,126,126,126,126,127,127
};

/* ------------------------------------------------------------------ */
/* local instances */

typedef struct _mdx2151_instances {
  int        ym2151_register_map[256];

  YM2151_LFO hlfo;
  int        hlfo_sync;
  OPM_WORK   opm[MDX_MAX_FM_TRACKS];

  int        ym2151_enable;
  int        master_volume;
  int        nlg_logging;

  void*      YM2151_Instance;
} mdx2151_instances;

/* ------------------------------------------------------------------ */
/* local functions */

static void freq_write( int, songdata * );
static void volume_write( int, songdata * );

static void reg_write( int, int, songdata * );
static int  reg_read( int, songdata * );

/* ------------------------------------------------------------------ */
/* class interface */

static mdx2151_instances*
__get_instances(songdata *data)
{
  return (mdx2151_instances*)_get_mdx2151(data);
}

#define __GETSELF(data)  mdx2151_instances* self = __get_instances(data);

void*
_mdx2151_initialize(void)
{
  mdx2151_instances* self = NULL;

  self = (mdx2151_instances *)malloc(sizeof(mdx2151_instances));
  if (!self) {
    return NULL;
  }
  memset(self, 0, sizeof(mdx2151_instances));

  self->ym2151_enable = FLAG_FALSE;
  self->master_volume = 127;
  self->YM2151_Instance = NULL;
  self->nlg_logging = 1;

  return self;
}

void
_mdx2151_finalize(void* in_self)
{
  mdx2151_instances* self = (mdx2151_instances *)in_self;
  if (self) {
    free(self);
  }
}

/* ------------------------------------------------------------------ */
/* implementations */

void*
ym2151_instance(songdata *data)
{
  __GETSELF(data);
  return self->YM2151_Instance;
}

int
ym2151_reg_init( MDX_DATA *mdx, songdata *data )
{
  int i;
  __GETSELF(data);

  if ( mdx->is_use_fm == FLAG_FALSE ) {
    self->ym2151_enable = FLAG_FALSE;
    return FLAG_TRUE;
  }

  /* Init OPM Emulator */
  /*
    number of OPM = 1
    clock         = 4MHz
    sample rate   = PCM8_MASTER_PCM_RATE ( typically 44.1 kHz )
    sample bits   = 16 bit
    */

  self->YM2151_Instance = YM2151Init( 1, 4000*1000, mdx->dsp_speed );
  if (!self->YM2151_Instance) {
    return FLAG_FALSE;
  }
  YM2151ResetChip( ym2151_instance(data) );

  self->ym2151_enable = FLAG_TRUE;
  self->master_volume = 127;

  for ( i=0 ; i<8 ; i++ ) {
    reg_write( 0x08, 0*8 + i, data );    /* KON */
  }
  reg_write( 0x0f, 0, data );            /* NE, NFREQ */
  reg_write( 0x18, 0, data );            /* LFRQ */
  reg_write( 0x19, 0*128 + 0, data );    /* AMD */
  reg_write( 0x19, 1*128 + 0, data );    /* AMD */
  reg_write( 0x1b, 0*64  + 0, data );    /* CT, W */

  for ( i=0 ; i<8 ; i++ ) {
    reg_write( 0x20+i, 3*64 + 0*8 +0, data ); /* LR, FL, CON */
    reg_write( 0x28+i, 0*16 + 0, data );      /* OCT, NOTE */
    reg_write( 0x30+i, 0, data );             /* KF */
    reg_write( 0x38+i, 0*16 + 0, data );      /* PMS, AMS */
  }
  for ( i=0 ; i<0x20 ; i++ ) {
    reg_write( 0x40+i, 0*16 + 0, data );      /* DT1, MUL */
    reg_write( 0x60+i, 0, data );             /* TL */
    reg_write( 0x80+i, 0*64 + 0, data );      /* KS, AR */
    reg_write( 0xa0+i, 0*128 + 0, data );     /* AMS, D1R */
    reg_write( 0xc0+i, 0*64 + 0, data );      /* DT2, D2R */
    reg_write( 0xe0+i, 0*16 + 0, data );      /* D1L, RR */
  }

  for ( i=0 ; i<MDX_MAX_FM_TRACKS ; i++ ) {

    OPM_WORK *o = &(self->opm[i]);

    o->note          = 0;
    o->note_on       = 0;
    o->step          = 0;
    o->lfo_step      = 0;
    o->pan           = 3;
    o->detune.octave = 0;
    o->detune.scale  = 0;
    o->detune.detune = 0;
    o->portament     = 0;
    o->freq_reg[0]   =-1;
    o->freq_reg[1]   =-1;
    o->freq_reg[2]   =-1;
    o->algorithm     = 0;
    o->volume        = 0;
    o->lfo_delay     = 0;

    o->hlfo_flag     = FLAG_FALSE;
    o->hlfo_val      = 0;

    o->plfo.flag     = FLAG_FALSE;
    o->plfo.form     = 0;
    o->plfo.clock    = 0;
    o->plfo.depth    = 0;

    o->alfo.flag     = FLAG_FALSE;
    o->alfo.form     = 0;
    o->alfo.clock    = 0;
    o->alfo.depth    = 0;

    o->total_level[0]= 0;
    o->total_level[1]= 0;
    o->total_level[2]= 0;
    o->total_level[3]= 127;
  }

  self->hlfo_sync=FLAG_FALSE;

  self->hlfo.form   = 0;
  self->hlfo.clock  = 0;
  self->hlfo.depth  = 0;
  self->hlfo.depth2 = 0;

  return FLAG_TRUE;
}

void
ym2151_shutdown( songdata* data )
{
  __GETSELF(data);

  if (self->YM2151_Instance) {
    YM2151Shutdown(ym2151_instance(data));
  }
  self->YM2151_Instance = NULL;
}


void
ym2151_set_logging(int flag, songdata* data )
{
    __GETSELF(data);
    
    self->nlg_logging = flag;
}


void
ym2151_all_note_off( songdata *data )
{
  int i,j;

  for ( i=0 ; i<MDX_MAX_FM_TRACKS ; i++ ) {
    ym2151_note_off(i, data);
  }
  for ( i=0 ; i<MDX_MAX_FM_TRACKS ; i++ ) {
    reg_write( 0x08, 0+i, data );          /* KON */

    j=reg_read( 0x20+i, data );
    reg_write( 0x20+i, j&0x3f, data );     /* LR, FL, CON */
  }

  return;
}

void
ym2151_note_on( int track, int n, songdata *data )
{
  OPM_WORK *o;
  __GETSELF(data);

  o = &(self->opm[track]);

  if ( o->step != 0 ) {
    o->portament = 0;
  }
  if ( o->note_on == 0 ) { /* for software-LFO w/ tie */
    o->step        = 0;
    o->lfo_step    = 0;
  }
  o->note        = n;
  o->note_on     = 1;
  o->freq_reg[0] = -1;
  o->freq_reg[1] = -1;
  o->freq_reg[2] = -1;

  reg_write( 0x1b, self->hlfo.form, data );
  reg_write( 0x18, self->hlfo.clock, data );
  reg_write( 0x19, self->hlfo.depth, data );
  reg_write( 0x19, self->hlfo.depth2, data );

  if ( o->hlfo_flag == FLAG_TRUE && self->hlfo_sync == FLAG_TRUE ) {
    reg_write( 0x01, 0x02, data ); /* LFO SYNC */
    reg_write( 0x01, 0x00, data );
  }

  freq_write(track, data);
  volume_write(track, data);

  return;
}

void
ym2151_note_off(int track, songdata *data)
{
  __GETSELF(data);

  self->opm[track].note_on   = 0;
  self->opm[track].portament = 0;

  return;
}

void
ym2151_set_pan( int track, int val, songdata *data )
{
  __GETSELF(data);

  if ( val < 0 ) { val = 0; }
  if ( val > 3 ) { val = 3; }

  if ( self->opm[track].pan != val ) {
    int i,v;
    i = reg_read( 0x20 + track, data );
    v = i&0x3f;
    reg_write( 0x20 + track, v+(val<<6), data );  /* LR */
  }
  self->opm[track].pan = val;

  return;
}

void
ym2151_set_volume( int track, int val, songdata *data )
{
  __GETSELF(data);

  if ( val < 0 ) { val = 0; }
  if ( val > 127 ) { val = 127; }
  self->opm[track].volume = val;

  return;
}

void
ym2151_set_master_volume( int val, songdata *data )
{
  __GETSELF(data);

  if ( val < 0 ) { val = 0; }
  if ( val > 127 ) { val = 127; }
  self->master_volume = master_vol_table[val];

  return;
}

void
ym2151_set_detune( int track, int val, songdata *data )
{
  __GETSELF(data);

  self->opm[track].detune.octave = val/(64*12);
  self->opm[track].detune.scale  = (val/64)%12;
  self->opm[track].detune.detune = val%64;

  return;
}

void
ym2151_set_portament( int track, int val, songdata *data )
{
  __GETSELF(data);

  self->opm[track].portament = val;
  self->opm[track].step      = 0;

  return;
}

void
ym2151_set_noise_freq( int val, songdata *data )
{
  reg_write( 0x0f, val, data );

  return;
}

void
ym2151_set_lfo_delay( int track, int delay, songdata *data )
{
  __GETSELF(data);

  self->opm[track].lfo_delay = delay;

  return;
}

void
ym2151_set_plfo( int track, int flag, int form, int clock, int depth, songdata *data )
{
  __GETSELF(data);

  self->opm[track].plfo.flag  = flag;
  self->opm[track].plfo.form  = form;
  self->opm[track].plfo.clock = clock;
  self->opm[track].plfo.depth = depth;

  self->opm[track].lfo_step = 0;

  return;
}

void
ym2151_set_alfo( int track, int flag, int form, int clock, int depth, songdata *data )
{
  __GETSELF(data);

  self->opm[track].alfo.flag  = flag;
  self->opm[track].alfo.form  = form;
  self->opm[track].alfo.clock = clock;
  self->opm[track].alfo.depth = depth;

  return;
}

void
ym2151_set_hlfo( int track, int v1, int v2, int v3, int v4, int v5, songdata *data )
{
  __GETSELF(data);

  self->hlfo.form   = v1&0x03;
  self->hlfo.clock  = v2;
  self->hlfo.depth  = v3|0x80;
  self->hlfo.depth2 = v4&0x7f;

  reg_write( 0x1b, self->hlfo.form, data   ); /* WAVE FORM */
  reg_write( 0x18, self->hlfo.clock, data  ); /* LFRQ */
  reg_write( 0x19, self->hlfo.depth, data  ); /* PMD */
  reg_write( 0x19, self->hlfo.depth2, data ); /* AMD */

  if ( (v1&0x40) == 0 ) {
    self->hlfo_sync = FLAG_FALSE;
  } else {
    self->hlfo_sync = FLAG_TRUE;
  }

  self->opm[track].hlfo_flag = FLAG_TRUE;
  self->opm[track].hlfo_val  = v5;
  reg_write( 0x38 + track, self->opm[track].hlfo_val, data );   /* PMS, AMS */

  return;
}

void
ym2151_set_hlfo_onoff( int track, int flag, songdata *data )
{
  __GETSELF(data);
  self->opm[track].hlfo_flag = flag;

  if ( self->opm[track].hlfo_flag == FLAG_TRUE ) {
    reg_write( 0x38 + track, self->opm[track].hlfo_val, data );   /* PMS, AMS */
  } else {
    reg_write( 0x38 + track, 0, data );
  }

  return;
}

void
ym2151_set_voice( int track, VOICE_DATA *v, songdata *data )
{
  OPM_WORK *o;
  int i,j,r;
  __GETSELF(data);

  o = &(self->opm[track]);

  j = reg_read( 0x20+track, data );                               /* LR, FL, CON */
  reg_write( 0x20+track, (j&0xc0) + v->v0, data );
  o->algorithm = v->con;
  o->slot_mask = v->slot_mask;

  for ( i=0 ; i<4 ; i++ ) {
    r = track + i*8;

    reg_write( 0x40+r, v->v1[i], data );    /* DT1, MUL */
    reg_write( 0x80+r, v->v3[i], data );    /* KS, AR */
    reg_write( 0xa0+r, v->v4[i], data );    /* AME, D1R */
    reg_write( 0xc0+r, v->v5[i], data );    /* DT2, D2R */
    reg_write( 0xe0+r, v->v6[i], data );    /* SL, RR */

    o->total_level[i] = 127 - v->v2[i];
    if ( is_vol_set[o->algorithm][i] == 0 )
      reg_write( 0x60+r, v->v2[i]&0x7f, data );   /* TL */
    else
      reg_write( 0x60+r, 127, data );             /* TL */
  }

  return;
}

void
ym2151_set_reg( int adr, int val, songdata *data )
{
  __GETSELF(data);

  if ( adr < 0 || adr > 0xff ) { return; }

  if ( val < 0 ) { val = 0; }
  if ( val > 0xff ) { val = 0xff; }

  reg_write( adr, val, data );

  /* Total level */

  if ( adr >= 0x60 && adr < 0x80 ) {
    if ( val >=0 && val < 128 ) {
      self->opm[(adr-0x60)/4].total_level[(adr-0x60)%4] = 127-val;
    }
  }

  /* panpot */

  if ( adr >= 0x20 && adr < 0x28 ) {
    self->opm[adr-0x20].pan = (val&0xc0)>>6;
  }

  return;
}

/* ------------------------------------------------------------------ */

void
ym2151_set_freq_volume( int track, songdata *data )
{
  __GETSELF(data);

  self->opm[track].step++;
  self->opm[track].lfo_step++;
  freq_write( track, data );
  volume_write( track, data );

  return;
}

static
void freq_write( int track, songdata *data )
{
  OPM_WORK *o;
  int oct, scale, kf;
  int ofs_o, ofs_s, ofs_f;
  long long f;
  int c,d;
  int key;
  __GETSELF(data);

  o = &(self->opm[track]);

  /* detune jobs */

  ofs_o = o->detune.octave;  /* octave */
  ofs_s = o->detune.scale;   /* scale */
  ofs_f = o->detune.detune;  /* detune */
    
  /* portament jobs */
    
  if ( o->portament != 0 ) {
    f = o->portament * o->step / 256;
    
    ofs_f += f%64;          /* detune */
    c=0;
    while ( ofs_f > 63 ) { ofs_f-=64; c++; }
    while ( ofs_f <  0 ) { ofs_f+=64; c--; }
    
    ofs_s += c+(f/64)%12;   /* scale */
    c=0;
    while ( ofs_s > 11 ) { ofs_s-=12; c++; }
    while ( ofs_s <  0 ) { ofs_s+=12; c--; }
    
    ofs_o += c+f/(64*12);   /* octave */
  }
    
  /* soft-lfo jobs */

  d = o->lfo_step - o->lfo_delay;
  if ( o->plfo.flag == FLAG_TRUE && d >= 0 ) {
    int cl;
    int c;
    switch(o->plfo.form) {
    case 0:
    case 4:
      cl = d % o->plfo.clock;
      f  = o->plfo.depth * cl;
      break;

    case 1:
    case 3:
    case 5:
    case 7:
      cl = d % (o->plfo.clock*2);
      if ( cl<o->plfo.clock ) {
	f =  o->plfo.depth/2;
      } else {
	f = -(o->plfo.depth/2);
      }
      break;

    case 2:
    case 6:
      cl = d % (o->plfo.clock*2);
      c = cl % (o->plfo.clock/2);
      switch ( cl/(o->plfo.clock/2) ) {
      case 0:
	f = o->plfo.depth * c;
	break;
      case 1:
	f = o->plfo.depth * (o->plfo.clock/2 - c );
	break;
      case 2:
	f = -(o->plfo.depth * c);
	break;
      case 3:
	f = -(o->plfo.depth * (o->plfo.clock/2 - c));
	break;

      default:
	f = 0;
	break;
      }
      break;

    default:
      f=0;
      break;
    }

    f/=256;
    ofs_f += f%64;          /* detune */
    c=0;
    while ( ofs_f > 63 ) { ofs_f-=64; c++; }
    while ( ofs_f <  0 ) { ofs_f+=64; c--; }
    
    ofs_s += c+(f/64)%12;   /* scale */
    c=0;
    while ( ofs_s > 11 ) { ofs_s-=12; c++; }
    while ( ofs_s <  0 ) { ofs_s+=12; c--; }
    
    ofs_o += c+f/(64*12);   /* octave */
  }
  
  /* generate f-number */

  kf = ofs_f;
  c=0;
  while ( kf > 63 ) { kf -= 64; c++; }
  while ( kf <  0 ) { kf += 64; c--; }

  scale = c + (o->note%12) + ofs_s;
  c=0;
  while ( scale > 11 ) { scale-=12; c++; }
  while ( scale <  0 ) { scale+=12; c--; }

  scale = ym2151_note[scale];

  oct = c + (o->note/12) + ofs_o;
  if ( oct>7 ) { oct = 7; }
  else if ( oct<0 ) { oct = 0; }


  /* write to register */

  if ( o->note_on != 0 ) { key=o->slot_mask<<3; }
  else { key=0; }


  if ( o->freq_reg[0] != oct*16+scale ) {
    o->freq_reg[0] = oct*16+scale;
    reg_write( 0x28 + track, o->freq_reg[0], data );  /* OCT, NOTE */
  }

  if ( o->freq_reg[1] != kf*4 ) {
    o->freq_reg[1] = kf*4;
    reg_write( 0x30 + track, o->freq_reg[1], data );  /* KF */
  }

  if ( o->freq_reg[2] != key+track ) {
    o->freq_reg[2] = key+track;
    reg_write( 0x08, o->freq_reg[2], data );          /* KEY ON */
  }

  return;
}

static
void volume_write( int track, songdata *data )
{
  OPM_WORK *o;
  int i,r;
  int vol;
  __GETSELF(data);

  o = &(self->opm[track]);

  /* set volume */

  for ( i=0 ; i<4 ; i++ ) {
    r = track + i*8;
    if ( is_vol_set[o->algorithm][i]==0 ) continue;
    
    vol = self->master_volume * o->volume * o->total_level[i] / 127 / 127;
    if ( vol > 127 ) vol = 127;
    if ( vol < 0 ) vol = 0;
    vol = 127-vol;
    
    reg_write( 0x60+r, vol, data );                  /* TL */
  }

  return;
}

/* ------------------------------------------------------------------ */

/* register actions */

static void
reg_write( int adr, int val, songdata *data )
{
  __GETSELF(data);

  if ( adr > 0x0ff ) { return; }
  self->ym2151_register_map[adr] = val;

#ifdef USE_NLG
  if (self->nlg_logging)
  {
      WriteNLG_Data(nlgctx, CMD_OPM, adr, val);
  }
#endif
  if ( self->ym2151_enable == FLAG_TRUE ) {
    YM2151WriteReg( ym2151_instance(data), adr, val );
  }

  return;
}

static int
reg_read( int adr, songdata *data )
{
  __GETSELF(data);

  if ( adr > 0xff ) { return 0; }
  return self->ym2151_register_map[adr];
}
