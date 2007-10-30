/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: xmpi.h,v 1.43 2007-10-30 11:57:51 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifndef __XMPI_H
#define __XMPI_H

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

#define XMP_MAXPAT	1024		/* max number of samples in driver */
#define XMP_MAXORD	256		/* max number of patterns in module */
#define XMP_MAXROW	256		/* pattern loop stack size */
#define XMP_MAXVOL	(0x400 * 0x7fff)
#define XMP_EXTDRV	0xffff
#define XMP_MINLEN	0x1000
#define XMP_MAXCH	64		/* max virtual channels */
#define XMP_MAXVOC	64		/* max physical voices */

#if defined (DRIVER_OSS_MIX) || defined (DRIVER_OSS_SEQ)
#  if defined (HAVE_SYS_SOUNDCARD_H)
#    include <sys/soundcard.h>
#  elif defined (HAVE_MACHINE_SOUNDCARD_H)
#    include <machine/soundcard.h>
#  endif
#else
#  undef USE_ISA_CARDS
#  define WAVE_16_BITS    0x01	/* bit 0 = 8 or 16 bit wave data. */
#  define WAVE_UNSIGNED   0x02 	/* bit 1 = Signed - Unsigned data. */
#  define WAVE_LOOPING    0x04	/* bit 2 = looping enabled-1. */
#  define WAVE_BIDIR_LOOP 0x08	/* bit 3 = Set is bidirectional looping. */
#  define WAVE_LOOP_BACK  0x10	/* bit 4 = Set is looping backward. */
#endif

/* For emulation of Amiga Protracker-style sample loops: play entire
 * sample once before looping -- see menowantmiseria.mod
 */
#define WAVE_PTKLOOP	0x80	/* bit 7 = Protracker loop enable */
#define WAVE_FIRSTRUN	0x40	/* bit 6 = Protracker loop control */

#include <stdio.h>
#include <signal.h>

#ifdef B_BEOS_VERSION
#include <SupportDefs.h>
#else
typedef signed char int8;
typedef signed short int int16;
typedef signed int int32;
typedef unsigned char uint8;
typedef unsigned short int uint16;
typedef unsigned int uint32;
#endif

#include "xmp.h"
#include "xxm.h"

/* [from the comp.lang.c Answers to Frequently Asked Questions]
 *
 * 9.10:   My compiler is leaving holes in structures, which is wasting
 *	   space and preventing "binary" I/O to external data files.  Can I
 *	   turn off the padding, or otherwise control the alignment of
 *	   structs?
 *
 * A:	   Your compiler may provide an extension to give you this control
 *	   (perhaps a #pragma), but there is no standard method.  See also
 *	   question 17.2.
 */

/* Constants */
#define PAL_RATE	250.0		/* 1 / (50Hz * 80us)		  */
#define NTSC_RATE	208.0		/* 1 / (60Hz * 80us)		  */
#define C4_FREQ		130812		/* 440Hz / (2 ^ (21 / 12)) * 1000 */
#define C4_PAL_RATE	8287		/* 7093789.2 / period (C4) * 2	  */
#define C4_NTSC_RATE	8363		/* 7159090.5 / period (C4) * 2	  */

/* [Amiga] PAL color carrier frequency (PCCF) = 4.43361825 MHz */
/* [Amiga] CPU clock = 1.6 * PCCF = 7.0937892 MHz */

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

#define EVENT(a, c, r)	m->xxt[m->xxp[a]->info[c].index]->event[r]

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


struct xmp_ord_info {
	int bpm;
	int tempo;
	int gvl;
	int time;
};



/* Context */

#include "xxm.h"

struct xmp_mod_context {
	int verbosity;			/* verbosity level */
	char *dirname;			/* file dirname */
	char *basename;			/* file basename */
	char name[XMP_NAMESIZE];	/* module name */
	char type[XMP_NAMESIZE];	/* module type */
	char author[XMP_NAMESIZE];	/* module author */
	char *filename;			/* Module file name */
	int size;			/* File size */
	double rrate;			/* Replay rate */
	int c4rate;			/* C4 replay rate */
	int volbase;			/* Volume base */
	int volume;			/* Global volume */
	int *vol_xlat;			/* Volume translation table */
	int fetch;			/* Fetch mode (copy from flags) */

	struct xxm_header *xxh;		/* Header */
	struct xxm_pattern **xxp;	/* Patterns */
	struct xxm_track **xxt;		/* Tracks */
	struct xxm_instrument_header *xxih;	/* Instrument headers */
	struct xxm_instrument_map *xxim;	/* Instrument map */
	struct xxm_instrument **xxi;	/* Instruments */
	struct xxm_sample *xxs;		/* Samples */
	uint16 **xxae;			/* Amplitude envelope */
	uint16 **xxpe;			/* Pan envelope */
	uint16 **xxfe;			/* Pitch envelope */
	struct xxm_channel xxc[64];	/* Channel info */
	struct xmp_ord_info xxo_info[XMP_MAXORD];
	int xxo_fstrow[XMP_MAXORD];
	uint8 xxo[XMP_MAXORD];		/* Orders */

	uint8 **med_vol_table;		/* MED volume sequence table */
	uint8 **med_wav_table;		/* MED waveform sequence table */
};

struct flow_control {
	int pbreak;
	int jump;
	int delay;
	int jumpline;
	int row_cnt;
	int loop_chn;
	int* loop_row;
	int* loop_stack;
};

struct xmp_player_context {
	int pause;
	int pos;
	int tempo;
	int gvol_slide;
	int gvol_flag;
	int gvol_base;
	double tick_time;
	struct flow_control flow;
	struct xmp_channel *xc_data;
	int *fetch_ctl;
	int xmp_scan_ord, xmp_scan_row, xmp_scan_num, xmp_bpm;

	struct xmp_mod_context m;
};

struct xmp_driver_context {
	struct xmp_drv_info *driver;	/* Driver */
	char *description;		/* Driver description */
	char **help;			/* Driver help info */
	void (*callback)(void *, int);

	int memavl;			/* Memory availble in sound card */
	int numtrk;			/* Number of tracks */
	int numchn;			/* Number of virtual channels */
	int numvoc;			/* Number of voices currently in use */
	int numbuf;			/* Number of output buffers */

	int cmute_array[XMP_MAXCH];

	int *ch2vo_count;
	int *ch2vo_array;
	struct voice_info *voice_array;
	struct patch_info **patch_array;
};

struct xmp_context {
	struct xmp_options o;
	struct xmp_driver_context d;
	struct xmp_player_context p;
};


/* Externs */

extern void (*xmp_event_callback)(unsigned long);


/* Prototypes */

int	report			(char *, ...);
int	reportv			(struct xmp_context *, int, char *, ...);
int	ulaw_encode		(int);
char	*str_adj		(char *);
int	xmpi_scan_module	(struct xmp_context *);
int	xmpi_player_start	(struct xmp_context *);
int	xmpi_tell_wait		(void);
int	xmpi_select_read	(int, int);
int	xmpi_read_rc		(struct xmp_context *);
void	xmpi_read_modconf	(struct xmp_context *, unsigned, unsigned);
int	cksum			(FILE *);

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

#ifdef WIN32
int	mkstemp			(char *);
#endif

#endif /* __XMPI_H */
