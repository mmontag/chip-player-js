/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifndef __XMP_LOAD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "effects.h"
#include "convert.h"
#include "loader.h"

/* Sample flags */
#define SAMPLE_FLAG_DIFF	0x0001	/* Differential */
#define SAMPLE_FLAG_UNS		0x0002	/* Unsigned */
#define SAMPLE_FLAG_8BDIFF	0x0004
#define SAMPLE_FLAG_7BIT	0x0008
#define SAMPLE_FLAG_NOLOAD	0x0010	/* Get from buffer, don't load */
#define SAMPLE_FLAG_BIGEND	0x0040	/* Big-endian */
#define SAMPLE_FLAG_VIDC	0x0080	/* Archimedes VIDC logarithmic */
#define SAMPLE_FLAG_STEREO	0x0100	/* Interleaved stereo sample */
#define SAMPLE_FLAG_FULLREP	0x0200	/* Play full sample before looping */
#define SAMPLE_FLAG_ADLIB	0x1000	/* Adlib synth instrument */
#define SAMPLE_FLAG_SPECTRUM	0x2000	/* Spectrum synth instrument */

#define SAMPLE_FLAG_SYNTH	(SAMPLE_FLAG_ADLIB | SAMPLE_FLAG_SPECTRUM)


char *copy_adjust(char *, uint8 *, int);
int test_name(uint8 *, int);
void read_title(FILE *, char *, int);
void set_xxh_defaults(struct xmp_module *);
void cvt_pt_event(struct xmp_event *, uint8 *);
void disable_continue_fx(struct xmp_event *);
void clean_s3m_seq(struct xmp_module *, uint8 *);
int check_filename_case(char *, char *, char *, int);
void get_instrument_path(struct context_data *, char *, char *, int);
void set_type(struct module_data *, char *, ...);
int load_sample(struct context_data *, FILE *, int, int, struct xmp_sample *, void *);


extern uint8 ord_xlat[];
extern int arch_vol_table[];

#define MAGIC4(a,b,c,d) \
    (((uint32)(a)<<24)|((uint32)(b)<<16)|((uint32)(c)<<8)|(d))

#define LOAD_INIT() do { \
    fseek(f, start, SEEK_SET); \
    m->med_vol_table = m->med_wav_table = NULL; \
    set_xxh_defaults(&m->mod); \
} while (0)

#define MODULE_INFO() do { \
    _D(_D_WARN "Module title: \"%s\"", m->name); \
    _D(_D_WARN "Module type: %s", m->type); \
} while (0)

#define INSTRUMENT_INIT() do { \
    m->mod.xxi = calloc(sizeof (struct xmp_instrument), m->mod.ins); \
    if (m->mod.smp) { m->mod.xxs = calloc (sizeof (struct xmp_sample), m->mod.smp); }\
} while (0)

#define PATTERN_INIT() do { \
    m->mod.xxt = calloc(sizeof (struct xmp_track *), m->mod.trk); \
    m->mod.xxp = calloc(sizeof (struct xmp_pattern *), m->mod.pat + 1); \
} while (0)

#define PATTERN_ALLOC(x) do { \
    m->mod.xxp[x] = calloc(1, sizeof (struct xmp_pattern) + \
	sizeof (int) * (m->mod.chn - 1)); \
} while (0)

#define TRACK_ALLOC(i) do { \
    int j; \
    for (j = 0; j < m->mod.chn; j++) { \
	m->mod.xxp[i]->index[j] = i * m->mod.chn + j; \
	m->mod.xxt[i * m->mod.chn + j] = calloc (sizeof (struct xmp_track) + \
	    sizeof (struct xmp_event) * m->mod.xxp[i]->rows, 1); \
	m->mod.xxt[i * m->mod.chn + j]->rows = m->mod.xxp[i]->rows; \
    } \
} while (0)

#define INSTRUMENT_DEALLOC_ALL(i) do { \
    int k; \
    for (k = (i) - 1; k >= 0; k--) free(m->mod.xxi[k]); \
    free(m->mod.xxfe); \
    free(m->mod.xxpe); \
    free(m->mod.xxae); \
    if (m->mod.smp) free(m->mod.xxs); \
    free(m->mod.xxi); \
    free(m->mod.xxim); \
    free(m->mod.xxi); \
} while (0)

#define PATTERN_DEALLOC_ALL(x) do { \
    int k; \
    for (k = x; k >= 0; k--) free(m->mod.xxp[k]); \
    free(m->mod.xxp); \
} while (0)

#define TRACK_DEALLOC_ALL(i) do { \
    int j, k; \
    for (k = i; k >= 0; k--) { \
	for (j = 0; j < m->mod.chn; j++) \
	    free(m->mod.xxt[k * m->mod.chn + j]); \
    } \
    free(m->mod.xxt); \
} while (0)

#endif
