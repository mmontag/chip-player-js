/* Extended Module Player
 * Copyright (C) 1996-2006 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: load.h,v 1.8 2006-02-12 19:38:09 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifndef __XMP_LOAD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xmpi.h"
#include "xxm.h"
#include "effects.h"
#include "driver.h"
#include "convert.h"


#ifndef __XMP_LOADERS_COMMON
static char tracker_name[80];
static char author_name[80];
#endif

void	set_xxh_defaults	(struct xxm_header *);
void	cvt_pt_event		(struct xxm_event *, uint8 *);
void	disable_continue_fx	(struct xxm_event *);


/* Endianism fixup */
#define FIX_ENDIANISM_16(x)	(x=((((x)&0xff00)>>8)|(((x)&0xff)<<8)))
#define FIX_ENDIANISM_32(x)	(x=(((x)&0xff000000)>>24)|(((x)&0xff0000)>>8)|\
				  (((x)&0xff00)<<8)|(((x)&0xff)<<24))
#ifdef WORDS_BIGENDIAN
#define L_ENDIAN16(x)	FIX_ENDIANISM_16(x)
#define L_ENDIAN32(x)	FIX_ENDIANISM_32(x)
#define B_ENDIAN16(x)	(x=x)
#define B_ENDIAN32(x)	(x=x)
#else
#define L_ENDIAN16(x)	(x=x)
#define L_ENDIAN32(x)	(x=x)
#define B_ENDIAN16(x)	FIX_ENDIANISM_16(x)
#define B_ENDIAN32(x)	FIX_ENDIANISM_32(x)
#endif


#define LOAD_INIT() { \
    fseek (f, 0, SEEK_SET); \
    *tracker_name = *author_name = 0; \
    med_vol_table = med_wav_table = NULL; \
    set_xxh_defaults (xxh); \
}

#define MODULE_INFO() { \
    if (xmp_ctl->verbose) { \
	if (*xmp_ctl->name) report ("Module title   : %s\n", xmp_ctl->name); \
        if (*xmp_ctl->type) report ("Module type    : %s\n", xmp_ctl->type); \
	if (*tracker_name) report ("Tracker name   : %s\n", tracker_name); \
	if (*author_name) report ("Author name    : %s\n", author_name); \
        if (xxh->len) report ("Module length  : %d patterns\n", xxh->len); \
    } \
}

#define INSTRUMENT_INIT() { \
    xxih = calloc (sizeof (struct xxm_instrument_header), xxh->ins); \
    xxim = calloc (sizeof (struct xxm_instrument_map), xxh->ins); \
    xxi = calloc (sizeof (struct xxm_instrument *), xxh->ins); \
    if (xxh->smp) { xxs = calloc (sizeof (struct xxm_sample), xxh->smp); }\
    xxae = calloc (sizeof (uint16 *), xxh->ins); \
    xxpe = calloc (sizeof (uint16 *), xxh->ins); \
    xxfe = calloc (sizeof (uint16 *), xxh->ins); \
}

#define PATTERN_INIT() { \
    xxt = calloc (sizeof (struct xxm_track *), xxh->trk); \
    xxp = calloc (sizeof (struct xxm_pattern *), xxh->pat + 1); \
}

#define PATTERN_ALLOC(x) { \
    xxp[x] = calloc (1, sizeof (struct xxm_pattern) + \
    sizeof (struct xxm_trackinfo) * (xxh->chn - 1)); \
}

#define TRACK_ALLOC(i) { \
    int j; \
    for (j = 0; j < xxh->chn; j++) { \
	xxp[i]->info[j].index = i * xxh->chn + j; \
	xxt[i * xxh->chn + j] = calloc (sizeof (struct xxm_track) + \
	    sizeof (struct xxm_event) * xxp[i]->rows, 1); \
	xxt[i * xxh->chn + j]->rows = xxp[i]->rows; \
    } \
}

#define read8s(x)	( (int8)((read8((x)) + 128) & 0xff) - 128 )

uint8	read8		(FILE *);
uint16	read16l		(FILE *);
uint16	read16b		(FILE *);
uint32	read24l		(FILE *);
uint32	read24b		(FILE *);
uint32	read32l		(FILE *);
uint32	read32b		(FILE *);

#endif
