/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: xmpi.h,v 1.14 2007-10-15 00:25:26 cmatsuoka Exp $
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
 * Claudio's fix: megaman has many (unused) samples, raise XMP_DEF_MAXPAT
 * from 256 to 1024. Otherwise, xmp ignores any event containing a sample
 * number above the limit.
 */

#define XMP_DEF_MAXPAT	0x400		/* max number of samples in driver */
#define XMP_DEF_MAXORD	0x100		/* max number of patterns in module */
#define XMP_DEF_MAXROW	0x100		/* pattern loop stack size */
#define XMP_DEF_MAXVOL	(0x400 * 0x7fff)
#define XMP_DEF_EXTDRV	0xffff
#define XMP_DEF_MINLEN	0x1000

#if defined (DRIVER_OSS_MIX) || defined (DRIVER_OSS_SEQ)
#   if defined (HAVE_SYS_SOUNDCARD_H)
#	include <sys/soundcard.h>
#   elif defined (HAVE_MACHINE_SOUNDCARD_H)
#	include <machine/soundcard.h>
#   endif
#else
#   undef USE_ISA_CARDS
#   define WAVE_16_BITS    0x01   /* bit 0 = 8 or 16 bit wave data. */
#   define WAVE_UNSIGNED   0x02   /* bit 1 = Signed - Unsigned data. */
#   define WAVE_LOOPING    0x04   /* bit 2 = looping enabled-1. */
#   define WAVE_BIDIR_LOOP 0x08   /* bit 3 = Set is bidirectional looping. */
#   define WAVE_LOOP_BACK  0x10   /* bit 4 = Set is looping backward. */
#endif

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

#define EVENT(p, c, r)	xxt[xxp[p]->info[c].index]->event[r]

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

#define V(x) (xmp_ctl->verbose > (x))

struct xmp_ord_info {
    int bpm;
    int tempo;
    int gvl;
    int time;
};

/* Externs */

extern struct xxm_header *xxh;
extern struct xxm_pattern **xxp;
extern struct xxm_track **xxt;
extern struct xxm_instrument_header *xxih;
extern struct xxm_instrument_map *xxim;
extern struct xxm_instrument **xxi;
extern struct xxm_sample *xxs;
extern uint16 **xxae;
extern uint16 **xxpe;
extern uint16 **xxfe;
extern struct xxm_channel xxc[64]; 
extern uint8 xxo[XMP_DEF_MAXORD];
extern int big_endian;

extern uint8 **med_vol_table;
extern uint8 **med_wav_table;

extern void (*xmp_event_callback)(unsigned long);

extern struct xmp_control *xmp_ctl;	/* built in control struct pointer */

/* Prototypes */

int	report			(char *, ...);
int	reportv			(int, char *, ...);
int	ulaw_encode		(int);
char	*str_adj		(char *);
int	xmpi_scan_module	(void);
int	xmpi_player_start	(void);
int	xmpi_tell_wait		(void);
int	xmpi_select_read	(int, int);
int	xmpi_read_rc		(struct xmp_control *);
void	xmpi_read_modconf	(struct xmp_control *, unsigned, unsigned);
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

#endif /* __XMPI_H */
