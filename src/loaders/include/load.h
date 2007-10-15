/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: load.h,v 1.18 2007-10-15 00:25:26 cmatsuoka Exp $
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
#include "loader.h"


#ifndef __XMP_LOADERS_COMMON
static char author_name[80];
#endif

char	*copy_adjust		(uint8 *, uint8 *, int);
int	test_name		(uint8 *, int);
void	read_title		(FILE *, char *, int);
void	set_xxh_defaults	(struct xxm_header *);
void	cvt_pt_event		(struct xxm_event *, uint8 *);
void	disable_continue_fx	(struct xxm_event *);

#define MAGIC4(a,b,c,d) \
    (((uint32)(a)<<24)|((uint32)(b)<<16)|((uint32)(c)<<8)|(d))

#define LOAD_INIT() do { \
    fseek (f, 0, SEEK_SET); \
    *author_name = 0; \
    med_vol_table = med_wav_table = NULL; \
    set_xxh_defaults (xxh); \
} while (0)

#define MODULE_INFO() do { \
    if (xmp_ctl->verbose) { \
	if (*xmp_ctl->name) report ("Module title   : %s\n", xmp_ctl->name); \
        if (*xmp_ctl->type) report ("Module type    : %s\n", xmp_ctl->type); \
	if (*author_name) report ("Author name    : %s\n", author_name); \
        if (xxh->len) report ("Module length  : %d patterns\n", xxh->len); \
    } \
} while (0)

#define INSTRUMENT_INIT() do { \
    xxih = calloc (sizeof (struct xxm_instrument_header), xxh->ins); \
    xxim = calloc (sizeof (struct xxm_instrument_map), xxh->ins); \
    xxi = calloc (sizeof (struct xxm_instrument *), xxh->ins); \
    if (xxh->smp) { xxs = calloc (sizeof (struct xxm_sample), xxh->smp); }\
    xxae = calloc (sizeof (uint16 *), xxh->ins); \
    xxpe = calloc (sizeof (uint16 *), xxh->ins); \
    xxfe = calloc (sizeof (uint16 *), xxh->ins); \
} while (0)

#define PATTERN_INIT() do { \
    xxt = calloc (sizeof (struct xxm_track *), xxh->trk); \
    xxp = calloc (sizeof (struct xxm_pattern *), xxh->pat + 1); \
} while (0)

#define PATTERN_ALLOC(x) do { \
    xxp[x] = calloc (1, sizeof (struct xxm_pattern) + \
    sizeof (struct xxm_trackinfo) * (xxh->chn - 1)); \
} while (0)

#define TRACK_ALLOC(i) do { \
    int j; \
    for (j = 0; j < xxh->chn; j++) { \
	xxp[i]->info[j].index = i * xxh->chn + j; \
	xxt[i * xxh->chn + j] = calloc (sizeof (struct xxm_track) + \
	    sizeof (struct xxm_event) * xxp[i]->rows, 1); \
	xxt[i * xxh->chn + j]->rows = xxp[i]->rows; \
    } \
} while (0)

#endif
