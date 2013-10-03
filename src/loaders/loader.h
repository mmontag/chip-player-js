#ifndef XMP_LOADER_H
#define XMP_LOADER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "effects.h"
#include "format.h"
#include "hio.h"

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
#define SAMPLE_FLAG_HSC		0x2000	/* HSC Adlib synth instrument */
#define SAMPLE_FLAG_SPECTRUM	0x4000	/* Spectrum synth instrument */

#define SAMPLE_FLAG_SYNTH	(SAMPLE_FLAG_ADLIB | SAMPLE_FLAG_SPECTRUM)

int instrument_init(struct xmp_module *);
int subinstrument_alloc(struct xmp_module *, int);
int pattern_init(struct xmp_module *);
int pattern_alloc(struct xmp_module *, int);
int track_alloc(struct xmp_module *, int, int);
int tracks_in_pattern_alloc(struct xmp_module *, int);
int pattern_tracks_alloc(struct xmp_module *, int, int);
char *instrument_name(struct xmp_module *, int, uint8 *, int);

char *copy_adjust(char *, uint8 *, int);
int test_name(uint8 *, int);
void read_title(HIO_HANDLE *, char *, int);
void set_xxh_defaults(struct xmp_module *);
void decode_protracker_event(struct xmp_event *, uint8 *);
void decode_noisetracker_event(struct xmp_event *, uint8 *);
void disable_continue_fx(struct xmp_event *);
int check_filename_case(char *, char *, char *, int);
void get_instrument_path(struct module_data *, char *, int);
void set_type(struct module_data *, char *, ...);
int load_sample(struct module_data *, HIO_HANDLE *, int, struct xmp_sample *, void *);

extern uint8 ord_xlat[];
extern const int arch_vol_table[];

#define MAGIC4(a,b,c,d) \
    (((uint32)(a)<<24)|((uint32)(b)<<16)|((uint32)(c)<<8)|(d))

#define LOAD_INIT() do { \
    hio_seek(f, start, SEEK_SET); \
} while (0)

#define MODULE_INFO() do { \
    D_(D_WARN "Module title: \"%s\"", m->mod.name); \
    D_(D_WARN "Module type: %s", m->mod.type); \
} while (0)

#define INSTRUMENT_INIT() do { \
    mod->xxi = calloc(sizeof (struct xmp_instrument), mod->ins); \
    if (mod->smp) { mod->xxs = calloc (sizeof (struct xmp_sample), mod->smp); }\
} while (0)

#define PATTERN_INIT() do { \
    mod->xxt = calloc(sizeof (struct xmp_track *), mod->trk); \
    mod->xxp = calloc(sizeof (struct xmp_pattern *), mod->pat + 1); \
} while (0)

#define PATTERN_ALLOC(x_) do { \
    mod->xxp[x_] = calloc(1, sizeof (struct xmp_pattern) + \
	sizeof (int) * (mod->chn - 1)); \
} while (0)

#define TRACK_ALLOC(x_) do { \
    int i_; \
    for (i_ = 0; i_ < mod->chn; i_++) { \
	int t_ = (x_) * mod->chn + i_; \
	mod->xxp[x_]->index[i_] = t_; \
	mod->xxt[t_] = calloc (sizeof (struct xmp_track) + \
	    sizeof (struct xmp_event) * (mod->xxp[x_]->rows - 1), 1); \
	mod->xxt[t_]->rows = mod->xxp[x_]->rows; \
    } \
} while (0)

#endif
