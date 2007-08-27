/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: driver.h,v 1.8 2007-08-27 01:42:52 cmatsuoka Exp $
 */

#ifndef __XMP_DRIVER_H
#define __XMP_DRIVER_H

#include "xmpi.h"

#define XMP_PATCH_FM	-1

#if !defined(DRIVER_OSS_SEQ) && !defined(DRIVER_OSS_MIX)

#define GUS_PATCH 1

struct patch_info {
    unsigned short key;
    short device_no;			/* Synthesizer number */
    short instr_no;			/* Midi pgm# */
    unsigned int mode;
    int len;				/* Size of the wave data in bytes */
    int loop_start, loop_end;		/* Byte offsets from the beginning */
    unsigned int base_freq;
    unsigned int base_note;
    unsigned int high_note;
    unsigned int low_note;
    int panning;			/* -128=left, 127=right */
    int detuning;
    int volume;
    char data[1];			/* The waveform data starts here */
};

#endif

/* Sample flags */
#define XMP_SMP_DIFF		0x0001	/* Differential */
#define XMP_SMP_UNS		0x0002	/* Unsigned */
#define XMP_SMP_8BDIFF		0x0004
#define XMP_SMP_7BIT		0x0008
#define XMP_SMP_NOLOAD		0x0010	/* Get from buffer, don't load */
#define XMP_SMP_8X		0x0020
#define XMP_SMP_BIGEND		0x0040	/* Big-endian */
#define XMP_SMP_VIDC		0x0080	/* Archimedes VIDC logarithmic */
#define XMP_SMP_LZW13		0x0100	/* DSymphony LZW packed samples */

#define XMP_ACT_CUT		XXM_NNA_CUT
#define XMP_ACT_CONT		XXM_NNA_CONT
#define XMP_ACT_OFF		XXM_NNA_OFF
#define XMP_ACT_FADE		XXM_NNA_FADE

#define XMP_CHN_ACTIVE		0x100
#define XMP_CHN_DUMB		-1

#define parm_init() for (parm = ctl->parm; *parm; parm++) { \
	token = strtok (*parm, ":="); token = strtok (NULL, "");
#define parm_end() }
#define parm_error() { \
	fprintf (stderr, "xmp: incorrect parameters in -D %s\n", *parm); \
	exit (-4); }
#define chkparm1(x,y) { \
	if (!strcmp (*parm,x)) { \
	    if (token == NULL) parm_error ()  else { y; } } }
#define chkparm2(x,y,z,w) { if (!strcmp (*parm,x)) { \
	if (2 > sscanf (token, y, z, w)) parm_error (); } }


/* PROTOTYPES */

void	xmp_drv_register	(struct xmp_drv_info *);
int	xmp_drv_open		(struct xmp_control *);
int	xmp_drv_set		(struct xmp_control *);
void	xmp_drv_close		(void);
int	xmp_drv_on		(int);
void	xmp_drv_off		(void);
int	xmp_drv_mutelloc	(int);
void	xmp_drv_mute		(int, int);
int	xmp_drv_flushpatch	(int);
int	xmp_drv_writepatch	(struct patch_info *);
int	xmp_drv_setpatch	(int, int, int, int, int, int, int, int);
int	xmp_drv_cvt8bit		(void);
int	xmp_drv_crunch		(struct patch_info **, unsigned int);
void	xmp_drv_setsmp		(int, int);
void	xmp_drv_setnna		(int, int);
void	xmp_drv_pastnote	(int, int);
void	xmp_drv_retrig		(int);
void	xmp_drv_setvol		(int, int);
void	xmp_drv_voicepos	(int, int);
void	xmp_drv_setbend		(int, int);
void	xmp_drv_setpan		(int, int);
void	xmp_drv_seteffect	(int, int, int);
int	xmp_drv_cstat		(int);
void	xmp_drv_resetchannel	(int);
void	xmp_drv_reset		(void);
double	xmp_drv_sync		(double);
int	xmp_drv_getmsg		(void);
void	xmp_drv_stoptimer	(void);
void	xmp_drv_setsmp		(int, int);
void	xmp_drv_clearmem	(void);
void	xmp_drv_starttimer	(void);
void	xmp_drv_echoback	(int);
void	xmp_drv_bufwipe		(void);
void	xmp_drv_bufdump		(void);
int	xmp_drv_loadpatch 	(FILE *, int, int, int, struct xxm_sample *,
				 char *);

void xmp_init_drivers (void);
struct xmp_drv_info *xmp_drv_array (void);

#endif /* __XMP_DRIVER_H */
