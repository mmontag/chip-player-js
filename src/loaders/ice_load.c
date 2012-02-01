/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Loader for Soundtracker 2.6/Ice Tracker modules */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"

#define MAGIC_MTN_	MAGIC4('M','T','N',0)
#define MAGIC_IT10	MAGIC4('I','T','1','0')


static int ice_test (FILE *, char *, const int);
static int ice_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info ice_loader = {
    "MTN",
    "Soundtracker 2.6/Ice Tracker",
    ice_test,
    ice_load
};

static int ice_test(FILE *f, char *t, const int start)
{
    uint32 magic;

    fseek(f, start + 1464, SEEK_SET);
    magic = read32b(f);
    if (magic != MAGIC_MTN_ && magic != MAGIC_IT10)
	return -1;

    fseek(f, start + 0, SEEK_SET);
    read_title(f, t, 28);

    return 0;
}


struct ice_ins {
    char name[22];		/* Instrument name */
    uint16 len;			/* Sample length / 2 */
    uint8 finetune;		/* Finetune */
    uint8 volume;		/* Volume (0-63) */
    uint16 loop_start;		/* Sample loop start in file */
    uint16 loop_size;		/* Loop size / 2 */
};

struct ice_header {
    char title[20];
    struct ice_ins ins[31];	/* Instruments */
    uint8 len;			/* Size of the pattern list */
    uint8 trk;			/* Number of tracks */
    uint8 ord[128][4];
    uint32 magic;		/* 'MTN\0', 'IT10' */
};


static int ice_load(struct xmp_context *ctx, FILE *f, const int start)
{
    struct xmp_mod_context *m = &ctx->m;
    int i, j;
    struct xmp_event *event;
    struct ice_header ih;
    uint8 ev[4];

    LOAD_INIT();

    fread(&ih.title, 20, 1, f);
    for (i = 0; i < 31; i++) {
	fread(&ih.ins[i].name, 22, 1, f);
	ih.ins[i].len = read16b(f);
	ih.ins[i].finetune = read8(f);
	ih.ins[i].volume = read8(f);
	ih.ins[i].loop_start = read16b(f);
	ih.ins[i].loop_size = read16b(f);
    }
    ih.len = read8(f);
    ih.trk = read8(f);
    fread(&ih.ord, 128 * 4, 1, f);
    ih.magic = read32b(f);

    if (ih.magic == MAGIC_IT10)
        strcpy(m->mod.type, "IT10 (Ice Tracker)");
    else if (ih.magic == MAGIC_MTN_)
        strcpy(m->mod.type, "MTN (Soundtracker 2.6)");
    else
	return -1;

    m->mod.xxh->ins = 31;
    m->mod.xxh->smp = m->mod.xxh->ins;
    m->mod.xxh->pat = ih.len;
    m->mod.xxh->len = ih.len;
    m->mod.xxh->trk = ih.trk;

    strncpy (m->mod.name, (char *) ih.title, 20);
    MODULE_INFO();

    INSTRUMENT_INIT();

    for (i = 0; i < m->mod.xxh->ins; i++) {
	m->mod.xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
	m->mod.xxi[i].nsm = !!(m->mod.xxs[i].len = 2 * ih.ins[i].len);
	m->mod.xxs[i].lps = 2 * ih.ins[i].loop_start;
	m->mod.xxs[i].lpe = m->mod.xxs[i].lps + 2 * ih.ins[i].loop_size;
	m->mod.xxs[i].flg = ih.ins[i].loop_size > 1 ? XMP_SAMPLE_LOOP : 0;
	m->mod.xxi[i].sub[0].vol = ih.ins[i].volume;
	m->mod.xxi[i].sub[0].fin = ((int16)ih.ins[i].finetune / 0x48) << 4;
	m->mod.xxi[i].sub[0].pan = 0x80;
	m->mod.xxi[i].sub[0].sid = i;

	_D(_D_INFO "[%2X] %-22.22s %04x %04x %04x %c %02x %+01x",
		i, ih.ins[i].name, m->mod.xxs[i].len, m->mod.xxs[i].lps, m->mod.xxs[i].lpe,
		m->mod.xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ', m->mod.xxi[i].sub[0].vol,
		m->mod.xxi[i].sub[0].fin >> 4);
    }

    PATTERN_INIT();

    _D(_D_INFO "Stored patterns: %d", m->mod.xxh->pat);

    for (i = 0; i < m->mod.xxh->pat; i++) {
	PATTERN_ALLOC (i);
	m->mod.xxp[i]->rows = 64;
	for (j = 0; j < m->mod.xxh->chn; j++) {
	    m->mod.xxp[i]->index[j] = ih.ord[i][j];
	}
	m->mod.xxo[i] = i;
    }

    _D(_D_INFO "Stored tracks: %d", m->mod.xxh->trk);

    for (i = 0; i < m->mod.xxh->trk; i++) {
	m->mod.xxt[i] = calloc (sizeof (struct xmp_track) + sizeof
		(struct xmp_event) * 64, 1);
	m->mod.xxt[i]->rows = 64;
	for (j = 0; j < m->mod.xxt[i]->rows; j++) {
		event = &m->mod.xxt[i]->event[j];
		fread (ev, 1, 4, f);
		cvt_pt_event (event, ev);
	}
    }

    m->mod.xxh->flg |= XXM_FLG_MODRNG;

    /* Read samples */

    _D(_D_INFO "Stored samples: %d", m->mod.xxh->smp);

    for (i = 0; i < m->mod.xxh->ins; i++) {
	if (m->mod.xxs[i].len <= 4)
	    continue;
	load_patch(ctx, f, i, 0, &m->mod.xxs[i], NULL);
    }

    return 0;
}

