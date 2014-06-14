/*
  MDXplayer :  YM2151 emulator access routines

  Made by Daisuke Nagano <breeze.nagano@nifty.ne.jp>
  Apr.16.1999
 */

#include "mdxmini.h"

#ifndef _MDX2151_H_
#define _MDX2151_H_

/* APIs */

extern int ym2151_open( void );
extern int ym2151_reg_init( MDX_DATA *, songdata * );
extern void ym2151_shutdown( songdata * );

extern void ym2151_set_logging( int flag, songdata * );

extern void ym2151_all_note_off( songdata * );
extern void ym2151_note_on( int, int, songdata * );
extern void ym2151_note_off( int, songdata * );
extern void ym2151_set_pan( int, int, songdata * );
extern void ym2151_set_volume( int, int, songdata * );
extern void ym2151_set_detune( int, int, songdata * );
extern void ym2151_set_portament( int, int, songdata * );
extern void ym2151_set_noise_freq( int, songdata * );
extern void ym2151_set_voice( int, VOICE_DATA *, songdata * );
extern void ym2151_set_reg( int, int, songdata *data );

extern void ym2151_set_plfo( int, int, int, int, int, songdata * );
extern void ym2151_set_alfo( int, int, int, int, int, songdata * );
extern void ym2151_set_lfo_delay( int, int, songdata * );

extern void ym2151_set_hlfo( int, int, int, int, int, int, songdata * );
extern void ym2151_set_hlfo_onoff( int, int, songdata * );

extern void ym2151_set_freq_volume( int, songdata * );
extern void ym2151_set_master_volume( int, songdata * );

#endif /* _MDX2151_H_ */
