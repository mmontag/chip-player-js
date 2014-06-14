/*
  mdx.h  mdx data structure

  Made by Daisuke Nagano <breeze_nagano@nifty.ne.jp>
  Jan.14.1998 

 */

#include "mdxmini.h"

#ifndef _MDX_H_
#define _MDX_H_

/* ------------------------------------------------------------------ */

#define MDX_MAX_TRACK_NUMBER        16
#define MDX_MAX_TITLE_LENGTH        1024
#define MDX_MAX_VOICE_NUMBER        256
#define MDX_MAX_LOOP_DEPTH          1024
#define MDX_MAX_PDX_FILENAME_LENGTH 1024
#define MDX_MAX_PDX_TONE_NUMBER     96
#define MDX_MAX_PDX_BANK_NUMBER     16
#define MDX_PDX_PATH_ENV_NAME       "PDX_PATH"
#define MDX_VERSION_TEXT_SIZE       256

#define MDX_MAX_FM_TRACKS           8
#define MDX_MAX_PCM_TRACKS          8

#define MDX_EXTERNAL_WORK_SHMID     "__MDXPLAY_WORK_AREA__"

#define MDX_MAX_BROOK_TIME_OF_CATCH 3
#define MDX_WAIT_INTERVAL           2

#define FLAG_FALSE 0
#define FLAG_TRUE  1

#define MDX_PAN_N  0
#define MDX_PAN_L  1
#define MDX_PAN_R  2
#define MDX_PAN_C  3

/* ------------------------------------------------------------------ */

typedef struct _VOICE_DATA {
  int voice_number;
  int fl;              /* feed back */
  int con;             /* connection:algorithm */
  int slot_mask;       /* slot mask */
  int dt1[4];          /* detune 1 */
  int dt2[4];          /* detune 2 */
  int mul[4];          /* multiple */
  int tl[4];           /* total level */
  int ks[4];           /* key scale */
  int ar[4];           /* attack rate */
  int ame[4];          /* amplitude moduration enable */
  int d1r[4];          /* decay rate 1 */
  int d2r[4];          /* decay rate 2 ( sustine rate ) */
  int rr[4];           /* release rate */
  int sl[4];           /* sustine level */

  int v0;
  int v1[4];
  int v2[4];
  int v3[4];
  int v4[4];
  int v5[4];
  int v6[4];

} VOICE_DATA;

typedef struct _TRACK_WORK {
  int  current_mml_ptr;
  long counter;
  long gate;

  int voice;              /* current voice number */
  int volume;             /* current volume (0-127) */
  int volume_normal;      /* current_volume (0-15) */
  int note;               /* last note */
  int phase;              /* panpot */
  int quantize1;          /* quantize (q)*/
  int quantize2;          /* quantize (@q) */
  int detune;             /* detune */
  int portament;          /* portament */

  int loop_depth;
  int loop_counter[MDX_MAX_LOOP_DEPTH];
                          /* loop counter */

  int infinite_loop_times;
                          /* infinite loop count */

  int p_lfo_flag;         /* pitch lfo */
  int a_lfo_flag;         /* amplitude lfo */
  int h_lfo_flag;         /* hardware lfo */

  int p_lfo_form;         /* pitch lfo form */
  int p_lfo_clock;        /* pitch lfo clocks per a wave */
  int p_lfo_depth;        /* pitch lfo depth */

  int a_lfo_form;         /* amplitude lfo form */
  int a_lfo_clock;        /* amplitude lfo clocks per a wave */
  int a_lfo_depth;        /* amplitude lfo depth */

  int lfo_delay;          /* lfo delay ( only soft lfo ) */

  /* WAVE_FORM
     0 : SAW WAVE
     1 : SQUARE WAVE
     2 : TRIANGLE WAVE
     */

  int waiting_sync;       /* SYNC flag */

  int keyoff_disable;     /* keyoff mode */
  int last_volume_normal; /* volume mode */

} TRACK_WORK;

typedef struct _MDX_DATA {

  unsigned char version_1[MDX_VERSION_TEXT_SIZE];
  unsigned char version_2[MDX_VERSION_TEXT_SIZE];

  unsigned char *data;    /* data pointer */
  int            length;  /* data length */

  char data_title[MDX_MAX_TITLE_LENGTH];
  char pdx_dir[MDX_MAX_PDX_FILENAME_LENGTH];
  char pdx_name[MDX_MAX_PDX_FILENAME_LENGTH];
  int  base_pointer;
  int  voice_data_offset;
  int  mml_data_offset[MDX_MAX_TRACK_NUMBER];
  int  tracks;

  VOICE_DATA voice[MDX_MAX_VOICE_NUMBER];
  int dump_voice;

  int  haspdx;      /* if the data requires *.pdx file */
  int  pdx_enable;  /* is the PDX file is loaded / enabled ? */
  int  ispcm8mode;  /* is PCM8 mode (tarck number=16 ) ? */

  /* playing work area */

  int  tempo;             /* Timer B's value */
  long total_count;       /* total steps */
  long elapsed_time;      /* mili-second */

  TRACK_WORK track[MDX_MAX_TRACK_NUMBER];

  /* user configuration */

  int is_use_pcm8;
  int is_use_fm;
  int is_use_opl3;
  int is_use_ym2151;
  int is_use_fm_voice;
  int fm_wave_form;
  int fm_volume;
  int pcm_volume;
  int master_volume;
  int max_infinite_loops;
  int fade_out_speed;

  /* work parameter */

  int is_normal_exit;

  int fm_noise_vol;
  int fm_noise_freq;

  int is_output_to_stdout;
  int is_output_titles;

  int is_use_fragment;
  char *dsp_device;
  int dsp_speed;

  int is_output_to_stdout_in_wav;

  int is_use_reverb;
  float reverb_predelay;
  float reverb_roomsize;
  float reverb_damp;
  float reverb_width;
  float reverb_dry;
  float reverb_wet;

} MDX_DATA;

typedef struct _PDX_TONE {
  int *data;
  int *orig_data;
  long size;
  long orig_size;
} PDX_TONE;

typedef struct _PDX_DATA {

  PDX_TONE tone[MDX_MAX_PDX_TONE_NUMBER*MDX_MAX_PDX_BANK_NUMBER];

} PDX_DATA;

/* ------------------------------------------------------------------ */
/* functions */

extern MDX_DATA *mdx_open_mdx( char * );
extern PDX_DATA *mdx_open_pdx( unsigned char *, long length );
extern int mdx_close_mdx( MDX_DATA * );
extern int mdx_close_pdx( PDX_DATA * );
extern int mdx_get_voice_parameter( MDX_DATA * );
extern int mdx_parse_mml_ym2151( MDX_DATA *, PDX_DATA *, songdata * );
extern int mdx_init_track_work_area_ym2151( songdata * );
extern int mdx_parse_mml_opl3( MDX_DATA *, PDX_DATA * );
extern int mdx_init_track_work_area_opl3( void );
extern void error_end( char * );

extern void* mdx_parse_mml_ym2151_async_initialize(MDX_DATA* in_mdx, PDX_DATA* in_pdx, songdata *data);
extern int mdx_parse_mml_ym2151_async(songdata *data);
extern int mdx_parse_mml_ym2151_make_samples(short *buffer,int buffer_size, songdata *);
extern int mdx_parse_mml_get_tempo(void* in_self);

extern int mdx_parse_mml_ym2151_async_get_length(songdata *data);


extern void mdx_parse_mml_ym2151_async_finalize(songdata *);

extern int mdx_output_titles( MDX_DATA * );

/* ------------------------------------------------------------------ */
/* mdx data formats */

/* +n : follwing data number */

#define MDX_REST                        /* +0 : 0x00 - 0x7f */
#define MDX_NOTE                        /* +1 : 0x80 - 0xdf */
#define MDX_MIN_REST        0x00
#define MDX_MAX_REST        0x7f
#define MDX_MIN_NOTE        0x80
#define MDX_MAX_NOTE        0xdf
#define MDX_SET_TEMPO       0xff        /* +1 : @t command */
#define MDX_SET_OPM_REG     0xfe        /* +2 : y command */
#define MDX_SET_VOICE       0xfd        /* +1 : @ command */
#define MDX_SET_PHASE       0xfc        /* +1 : p command */
#define MDX_SET_VOLUME      0xfb        /* +1 :  v command(0x00 - 0x0f)
					        @v command(0x80 - 0xff) */
#define MDX_VOLUME_DEC      0xfa        /* +0 */
#define MDX_VOLUME_INC      0xf9        /* +0 */
#define MDX_SET_QUANTIZE    0xf8        /* +1 : q command */
#define MDX_SET_KEYOFF      0xf7        /* +0 */
#define MDX_REPEAT_START    0xf6        /* +2 */
#define MDX_REPEAT_END      0xf5        /* +2 */
#define MDX_REPEAT_BREAK    0xf4        /* +2 */
#define MDX_SET_DETUNE      0xf3        /* +2 : D command */
#define MDX_SET_PORTAMENT   0xf2        /* +2 : _ command */
#define MDX_DATA_END        0xf1        /* +2 */
#define MDX_KEY_ON_DELAY    0xf0        /* +1 : k command */
#define MDX_SEND_SYNC       0xef        /* +1 */
#define MDX_RECV_SYNC       0xee        /* +0 */
#define MDX_SET_FREQ        0xed        /* +1 */
#define MDX_SET_PLFO        0xec        /* +1(0x80,0x81) / +5 */
#define MDX_SET_ALFO        0xeb        /* +1(0x80,0x81) / +5 */
#define MDX_SET_OPMLFO      0xea        /* +1(0x80,0x81) / +5 */
#define MDX_SET_LFO_DELAY   0xe9        /* +1 : MD command */
#define MDX_SET_PCM8_MODE   0xe8        /* +0 : only A channel */
#define MDX_FADE_OUT        0xe7        /* +1(0x00) +2(0x01) : $FO command */
#define MDX_INVALID_DATA    0xe0        /* any undefined command means
					   stop playing */

#endif /*_MDX_H_*/
