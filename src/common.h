
#ifndef __XMP_COMMON_H
#define __XMP_COMMON_H

#ifdef __AROS__
#define __AMIGA__
#endif

/*
 * Sat, 15 Sep 2007 10:39:41 -0600
 * Reported by Jon Rafkind <workmin@ccs.neu.edu>
 * In megaman.xm there should be a tempo change at position 1 but in
 * xmp tempo and bpm remain the same.
 *
 * Claudio's fix: megaman has many (unused) samples, raise XMP_MAXPAT
 * from 256 to 1024. Otherwise, xmp ignores any event containing a sample
 * number above the limit.
 */

#define XMP_MAXROW	256		/* pattern loop stack size */
#define XMP_MAXVOL	(0x400 * 0x7fff)
#define XMP_EXTDRV	0xffff
#define XMP_MINLEN	0x1000
#define XMP_MAXVOC	64		/* max physical voices */

#include <stdio.h>
#include "xmp.h"


/* AmigaOS fixes by Chris Young <cdyoung@ntlworld.com>, Nov 25, 2007
 */
#if defined B_BEOS_VERSION
#  include <SupportDefs.h>
#elif defined __amigaos4__
#  include <exec/types.h>
#else
typedef signed char int8;
typedef signed short int int16;
typedef signed int int32;
typedef unsigned char uint8;
typedef unsigned short int uint16;
typedef unsigned int uint32;
#endif

#ifdef _MSC_VER				/* MSVC++6.0 has no long long */
typedef signed __int64 int64;
typedef unsigned __int64 uint64;
#elif !defined B_BEOS_VERSION		/*BeOS has its own int64 definition */
typedef unsigned long long uint64;
typedef signed long long int64;
#endif

/* Constants */
#define PAL_RATE	250.0		/* 1 / (50Hz * 80us)		  */
#define NTSC_RATE	208.0		/* 1 / (60Hz * 80us)		  */
#define C4_FREQ		130812		/* 440Hz / (2 ^ (21 / 12)) * 1000 */
#define C4_PAL_RATE	8287		/* 7093789.2 / period (C4) * 2	  */
#define C4_NTSC_RATE	8363		/* 7159090.5 / period (C4) * 2	  */

/* [Amiga] PAL color carrier frequency (PCCF) = 4.43361825 MHz */
/* [Amiga] CPU clock = 1.6 * PCCF = 7.0937892 MHz */

#define DEFAULT_AMPLIFY	1
#define DEFAULT_MIX	70

/* Global flags */
#define PATTERN_BREAK	0x0001 
#define PATTERN_LOOP	0x0002 
#define MODULE_ABORT	0x0004 
#define GLOBAL_VSLIDE	0x0010
#define ROW_MAX		0x0100

#define MSN(x)		(((x)&0xf0)>>4)
#define LSN(x)		((x)&0x0f)
#define SET_FLAG(a,b)	((a)|=(b))
#define RESET_FLAG(a,b)	((a)&=~(b))
#define TEST_FLAG(a,b)	!!((a)&(b))

#define TRACK_NUM(a,c)	m->mod.xxp[a]->index[c]
#define EVENT(a,c,r)	m->mod.xxt[TRACK_NUM((a),(c))]->event[r]

#ifdef _MSC_VER
#define _D_CRIT "  Error: "
#define _D_WARN "Warning: "
#define _D_INFO "   Info: "
#ifndef CLIB_DECL
#define CLIB_DECL
#endif
#ifdef _DEBUG
#ifndef ATTR_PRINTF
#define ATTR_PRINTF(x,y)
#endif
void CLIB_DECL _D(const char *text, ...) ATTR_PRINTF(1,2);
#else
void __inline CLIB_DECL _D(const char *text, ...) { do {} while (0); }
#endif

#elif defined ANDROID

#ifdef _DEBUG
#include <android/log.h>
#define _D_CRIT "  Error: "
#define _D_WARN "Warning: "
#define _D_INFO "   Info: "
#define _D(args...) do { \
	__android_log_print(ANDROID_LOG_DEBUG, "libxmp", args); \
	} while (0)
#else
#define _D(args...) do {} while (0)
#endif

#else

#ifdef _DEBUG
#define _D_INFO "\x1b[33m"
#define _D_CRIT "\x1b[31m"
#define _D_WARN "\x1b[36m"
#define _D(args...) do { \
	printf("\x1b[33m%s \x1b[37m[%s:%d] " _D_INFO, __PRETTY_FUNCTION__, \
		__FILE__, __LINE__); printf (args); printf ("\x1b[0m\n"); \
	} while (0)
#else
#define _D(args...) do {} while (0)
#endif

#endif	/* !_MSC_VER */


	/* Format quirks */
#define QUIRK_S3MLOOP	(1 << 0)	/* S3M loop mode */
#define QUIRK_ENVFADE	(1 << 1)	/* Fade at end of envelope */
#define QUIRK_INVLOOP	(1 << 2)	/* Enable effect EF invert loop */
#define QUIRK_FUNKIT	(1 << 3)	/* Enable effect EF funk it */
#define QUIRK_IGNWINS	(1 << 4)	/* Ignore invalid instrument */
#define QUIRK_INSPRI	(1 << 6)	/* Reset note for every new != ins */
#define QUIRK_CUTNWI	(1 << 7)	/* Cut only when note + invalid ins */
#define QUIRK_OINSVOL	(1 << 8)	/* XM old instrument mode */
#define QUIRK_OFSRST	(1 << 9)	/* Always reset sample offset */
#define QUIRK_FX9BUG	(1 << 10)	/* Protracker effect 9 bug emulation */
#define QUIRK_ST3GVOL	(1 << 11)	/* ST 3 weird global volume effect */
#define QUIRK_FINEFX	(1 << 12)	/* Enable 0xf/0xe for fine effects */
#define QUIRK_VSALL	(1 << 13)	/* Volume slides in all frames */
#define QUIRK_RTGINS	(1 << 14)	/* Retrig instrument on toneporta */
#define QUIRK_PBALL	(1 << 15)	/* Pitch bending in all frames */
#define QUIRK_PERPAT	(1 << 16)	/* Cancel persistent fx at pat start */
#define QUIRK_VOLPDN	(1 << 17)	/* Set priority to volume slide down */
#define QUIRK_UNISLD	(1 << 18)	/* Unified pitch slide/portamento */
#define QUIRK_SAVEINS	(1 << 19)	/* Always save instrument number */
#define QUIRK_ITVPOR	(1 << 20)	/* Disable fine bends in IT vol fx */
#define QUIRK_LINEAR	(1 << 21)	/* Linear periods */
#define QUIRK_MODRNG	(1 << 22)	/* Limit periods to MOD range */
#define QUIRK_INSVOL	(1 << 23)	/* Use instrument volume */
#define QUIRK_VIRTUAL	(1 << 24)	/* Enable virtual channels */
#define QUIRK_FILTER	(1 << 25)	/* Enable filter */

#define HAS_QUIRK(x)	(m->quirk & (x))


/* Format quirks */
#define QUIRKS_ST3		(QUIRK_IGNWINS | QUIRK_S3MLOOP | \
				 QUIRK_RTGINS | QUIRK_VOLPDN)
#define QUIRKS_FT2		(QUIRK_OINSVOL | QUIRK_CUTNWI | QUIRK_OFSRST)
#define QUIRKS_IT		(QUIRK_IGNWINS | QUIRK_INSPRI | \
				 QUIRK_ENVFADE | QUIRK_S3MLOOP | \
				 QUIRK_OFSRST | QUIRK_VOLPDN | \
				 QUIRK_RTGINS | QUIRK_SAVEINS | \
				 QUIRK_ITVPOR | QUIRK_FILTER | QUIRK_VIRTUAL)

/* DSP effects */
#define DSP_EFFECT_CUTOFF	0x02
#define DSP_EFFECT_RESONANCE	0x03
#define DSP_EFFECT_FILTER_B0	0xb0
#define DSP_EFFECT_FILTER_B1	0xb1
#define DSP_EFFECT_FILTER_B2	0xb2

/* Time factor */
#define DEFAULT_TIME_FACTOR	10.0
#define MED_TIME_FACTOR		2.64


struct ord_data {
	int bpm;
	int tempo;
	int gvl;
	int time;
	int start_row;
};



/* Context */

struct module_data {
	struct xmp_module mod;

	int time;			/* replay time in ms */
	char *dirname;			/* file dirname */
	char *basename;			/* file basename */
	char *filename;			/* Module file name */
	char *comment;			/* Comments, if any */
	int size;			/* File size */
	double rrate;			/* Replay rate */
	double time_factor;		/* Time conversion constant */
	int c4rate;			/* C4 replay rate */
	int volbase;			/* Volume base */
	int *vol_table;			/* Volume translation table */
	int quirk;			/* player quirks */
#define READ_EVENT_MOD	0
#define READ_EVENT_FT2	1
#define READ_EVENT_ST3	2
#define READ_EVENT_IT	3
	int read_event_type;
	struct ord_data xxo_info[XMP_MAX_MOD_LENGTH];

	char *instrument_path;

	uint8 **med_vol_table;		/* MED volume sequence table */
	uint8 **med_wav_table;		/* MED waveform sequence table */

	void *extra;			/* format-specific extra fields */

	const struct synth_info *synth;
	void *synth_chip;
};


struct player_data {
	int start;
	int ord;
	int pos;
	int row;
	int frame;
	int tempo;
	int bpm;
	double time;
	double frame_time;

	int loop_count;

	struct {			/* Global volume */
		int volume;
		int slide;
		int flag;
	} gvol;

	struct flow_control {
		int pbreak;
		int jump;
		int delay;
		int skip_fetch;		/* To emulate delay + break quirk */
		int jumpline;
		int loop_chn;
	
		struct pattern_loop {
			int start;
			int count;
		} *loop;
	
		int num_rows;
		int end_point;
	} flow;

	struct {
		int ord;
		int row;
		int num;
	} scan;

	struct channel_data *xc_data;

	char channel_mute[XMP_MAX_CHANNELS];

	struct virt_control {
		int num_tracks;		/* Number of tracks */
		int virt_channels;	/* Number of virtual channels */
		int virt_used;		/* Number of voices currently in use */
		int maxvoc;		/* Number of sound card voices */
		int chnvoc;		/* Number of voices per channel */
		int age;		/* Voice age control (?) */
	
		struct virt_channel {
			int count;
			int map;
		} *virt_channel;
	
		struct mixer_voice *voice_array;
	} virt;

	struct xmp_event inject_event[XMP_MAX_CHANNELS];
};

struct mixer_data {
	int freq;		/* sampling rate */
	int format;		/* sample format */
	int amplify;		/* amplification multiplier */
	int mix;		/* percentage of channel separation */
	char* buffer;		/* output buffer */
	int* buf32b;		/* temp buffer for 32 bit samples */
	int numvoc;		/* default softmixer voices number */
	int ticksize;
	int dtright;		/* anticlick control, right channel */
	int dtleft;		/* anticlick control, left channel */
	int pbase;		/* period base */
};

#include "list.h"

struct context_data {
	struct player_data p;
	struct mixer_data s;
	struct module_data m;
};


/* Prototypes */

char	*str_adj		(char *);
int	scan_module		(struct context_data *);

int8	read8s			(FILE *);
uint8	read8			(FILE *);
uint16	read16l			(FILE *);
uint16	read16b			(FILE *);
uint32	read24l			(FILE *);
uint32	read24b			(FILE *);
uint32	read32l			(FILE *);
uint32	read32b			(FILE *);

void	write8			(FILE *, uint8);
void	write16l		(FILE *, uint16);
void	write16b		(FILE *, uint16);
void	write32l		(FILE *, uint32);
void	write32b		(FILE *, uint32);

uint16	readmem16l		(uint8 *);
uint16	readmem16b		(uint8 *);
uint32	readmem32l		(uint8 *);
uint32	readmem32b		(uint8 *);

int	get_temp_dir		(char *, int);
#ifdef WIN32
int	mkstemp			(char *);
#endif

#endif /* __XMP_COMMON_H */
