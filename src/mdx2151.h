/*
  MDXplayer :  YM2151 emulator access routines

  Made by Daisuke Nagano <breeze.nagano@nifty.ne.jp>
  Apr.16.1999
 */

#ifndef _MDX2151_H_
#define _MDX2151_H_

/* APIs */

extern int ym2151_open( void );
extern int ym2151_reg_init( MDX_DATA * );
extern void ym2151_shutdown(void);

extern void ym2151_all_note_off( void );
extern void ym2151_note_on( int, int );
extern void ym2151_note_off( int );
extern void ym2151_set_pan( int, int );
extern void ym2151_set_volume( int, int );
extern void ym2151_set_detune( int, int );
extern void ym2151_set_portament( int, int );
extern void ym2151_set_noise_freq( int );
extern void ym2151_set_voice( int, VOICE_DATA * );
extern void ym2151_set_reg( int, int );

extern void ym2151_set_plfo( int, int, int, int, int );
extern void ym2151_set_alfo( int, int, int, int, int );
extern void ym2151_set_lfo_delay( int, int );

extern void ym2151_set_hlfo( int, int, int, int, int, int );
extern void ym2151_set_hlfo_onoff( int, int );

extern void ym2151_set_freq_volume( int );
extern void ym2151_set_master_volume( int );

#endif /* _MDX2151_H_ */
