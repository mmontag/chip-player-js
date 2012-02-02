
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
#include <signal.h>
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

#ifdef HAVE_STRLCPY
#define strcpy strlcpy
#endif

#ifdef HAVE_STRLCAT
#define strcat strlcat
#endif

/* Constants */
#define PAL_RATE	250.0		/* 1 / (50Hz * 80us)		  */
#define NTSC_RATE	208.0		/* 1 / (60Hz * 80us)		  */
#define C4_FREQ		130812		/* 440Hz / (2 ^ (21 / 12)) * 1000 */
#define C4_PAL_RATE	8287		/* 7093789.2 / period (C4) * 2	  */
#define C4_NTSC_RATE	8363		/* 7159090.5 / period (C4) * 2	  */

/* [Amiga] PAL color carrier frequency (PCCF) = 4.43361825 MHz */
/* [Amiga] CPU clock = 1.6 * PCCF = 7.0937892 MHz */

#define DEFAULT_AMPLIFY	0

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

struct xmp_ord_info {
	int bpm;
	int tempo;
	int gvl;
	int time;
	int start_row;
};



/* Context */

struct xmp_mod_context {
	struct xmp_module mod;

	int time;			/* replay time in ms */
	char *dirname;			/* file dirname */
	char *basename;			/* file basename */
	char *filename;			/* Module file name */
	char *comment;			/* Comments, if any */
	int size;			/* File size */
	double rrate;			/* Replay rate */
	int c4rate;			/* C4 replay rate */
	int volbase;			/* Volume base */
	int volume;			/* Global volume */
	int *vol_table;			/* Volume translation table */
	int flags;			/* Copy from options */
	int quirk;			/* Copy from options */
	struct xmp_ord_info xxo_info[XMP_MAXORD];

	uint8 **med_vol_table;		/* MED volume sequence table */
	uint8 **med_wav_table;		/* MED waveform sequence table */

	void *extra;			/* format-specific extra fields */

	struct xmp_synth_info *synth;
	void *chip;
};

struct flow_control {
	int pbreak;
	int jump;
	int delay;
	int skip_fetch;		/* To emulate delay + break quirk */
	int jumpline;
	int loop_chn;
	int* loop_start;
	int* loop_stack;
	int num_rows;
	int ord;
	int end_point;
	double playing_time;
};

struct xmp_player_context {
	double time;
	int pause;
	int pos;
	int row;
	int frame;
	int tempo;
	int gvol_slide;
	int gvol_flag;
	int gvol_base;
	double tick_time;
	struct flow_control flow;
	struct channel_data *xc_data;
	int *fetch_ctl;
	int scan_ord;
	int scan_row;
	int scan_num;
	int bpm;
};

struct xmp_driver_context {
	int numtrk;			/* Number of tracks */
	int numchn;			/* Number of virtual channels */
	int curvoc;			/* Number of voices currently in use */
	int maxvoc;			/* Number of sound card voices */
	int chnvoc;			/* Number of voices per channel */
	int agevoc;			/* Voice age control (?) */

	int cmute_array[XMP_MAXCH];

	int *ch2vo_count;
	int *ch2vo_array;
	struct voice_info *voice_array;

	void *buffer;
	int size;
};

struct xmp_smixer_context {
	char* buffer;		/* output buffer */
	int* buf32b;		/* temp buffer for 32 bit samples */
	int numvoc;		/* default softmixer voices number */
	int mode;		/* mode = 0:OFF, 1:MONO, 2:STEREO */
	int resol;		/* resolution output 1:8bit, 2:16bit */
	int ticksize;
	int dtright;		/* anticlick control, right channel */
	int dtleft;		/* anticlick control, left channel */
};

struct xmp_context {
	struct xmp_options o;
	struct xmp_driver_context d;
	struct xmp_player_context p;
	struct xmp_smixer_context s;
	struct xmp_mod_context m;
};


/* Prototypes */

int	ulaw_encode		(int);
char	*str_adj		(char *);
int	_xmp_scan_module	(struct xmp_context *);

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
