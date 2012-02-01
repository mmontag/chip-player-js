/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include "load.h"
#include "iff.h"

#define MAGIC_FORM	MAGIC4('F','O','R','M')
#define MAGIC_EMOD	MAGIC4('E','M','O','D')


static int emod_test (FILE *, char *, const int);
static int emod_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info emod_loader = {
    "EMOD",
    "Quadra Composer",
    emod_test,
    emod_load
};

static int emod_test(FILE *f, char *t, const int start)
{
    if (read32b(f) != MAGIC_FORM)
	return -1;

    read32b(f);

    if (read32b(f) != MAGIC_EMOD)
	return -1;

    read_title(f, t, 0);

    return 0;
}


static uint8 *reorder;


static void get_emic(struct xmp_context *ctx, int size, FILE *f)
{
    struct xmp_mod_context *m = &ctx->m;
    int i, ver;

    ver = read16b(f);
    fread(m->mod.name, 1, 20, f);
    fseek(f, 20, SEEK_CUR);
    m->mod.xxh->bpm = read8(f);
    m->mod.xxh->ins = read8(f);
    m->mod.xxh->smp = m->mod.xxh->ins;

    m->mod.xxh->flg |= XXM_FLG_MODRNG;

    snprintf(m->mod.type, XMP_NAMESIZE, "EMOD v%d (Quadra Composer)", ver);
    MODULE_INFO();

    INSTRUMENT_INIT();

    for (i = 0; i < m->mod.xxh->ins; i++) {
	m->mod.xxi[i].sub = calloc(sizeof (struct xxm_subinstrument), 1);

	read8(f);		/* num */
	m->mod.xxi[i].sub[0].vol = read8(f);
	m->mod.xxs[i].len = 2 * read16b(f);
	fread(m->mod.xxi[i].name, 1, 20, f);
	m->mod.xxs[i].flg = read8(f) & 1 ? XMP_SAMPLE_LOOP : 0;
	m->mod.xxi[i].sub[0].fin = read8(f);
	m->mod.xxs[i].lps = 2 * read16b(f);
	m->mod.xxs[i].lpe = m->mod.xxs[i].lps + 2 * read16b(f);
	read32b(f);		/* ptr */

	m->mod.xxi[i].nsm = 1;
	m->mod.xxi[i].sub[0].pan = 0x80;
	m->mod.xxi[i].sub[0].sid = i;

	_D(_D_INFO "[%2X] %-20.20s %05x %05x %05x %c V%02x %+d",
		i, m->mod.xxi[i].name, m->mod.xxs[i].len, m->mod.xxs[i].lps,
		m->mod.xxs[i].lpe, m->mod.xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		m->mod.xxi[i].sub[0].vol, m->mod.xxi[i].sub[0].fin >> 4);
    }

    read8(f);			/* pad */
    m->mod.xxh->pat = read8(f);

    m->mod.xxh->trk = m->mod.xxh->pat * m->mod.xxh->chn;

    PATTERN_INIT();

    reorder = calloc(1, 256);

    for (i = 0; i < m->mod.xxh->pat; i++) {
	reorder[read8(f)] = i;
	PATTERN_ALLOC(i);
	m->mod.xxp[i]->rows = read8(f) + 1;
	TRACK_ALLOC(i);
	fseek(f, 20, SEEK_CUR);		/* skip name */
	read32b(f);			/* ptr */
    }

    m->mod.xxh->len = read8(f);

    _D(_D_INFO "Module length: %d", m->mod.xxh->len);

    for (i = 0; i < m->mod.xxh->len; i++)
	m->mod.xxo[i] = reorder[read8(f)];
}


static void get_patt(struct xmp_context *ctx, int size, FILE *f)
{
    struct xmp_mod_context *m = &ctx->m;
    int i, j, k;
    struct xxm_event *event;
    uint8 x;

    _D(_D_INFO "Stored patterns: %d", m->mod.xxh->pat);

    for (i = 0; i < m->mod.xxh->pat; i++) {
	for (j = 0; j < m->mod.xxp[i]->rows; j++) {
	    for (k = 0; k < m->mod.xxh->chn; k++) {
		event = &EVENT(i, k, j);
		event->ins = read8(f);
		event->note = read8(f) + 1;
		if (event->note != 0)
		    event->note += 36;
		event->fxt = read8(f) & 0x0f;
		event->fxp = read8(f);

		/* Fix effects */
		switch (event->fxt) {
		case 0x04:
		    x = event->fxp;
		    event->fxp = (x & 0xf0) | ((x << 1) & 0x0f);
		    break;
		case 0x09:
		    event->fxt <<= 1;
		    break;
		case 0x0b:
		    x = event->fxt;
		    event->fxt = 16 * (x / 10) + x % 10;
		    break;
		}
	    }
	}
    }
}


static void get_8smp(struct xmp_context *ctx, int size, FILE *f)
{
    struct xmp_mod_context *m = &ctx->m;
    int i;

    _D(_D_INFO, "Stored samples : %d ", m->mod.xxh->smp);

    for (i = 0; i < m->mod.xxh->smp; i++) {
	xmp_drv_loadpatch(ctx, f, i, 0, &m->mod.xxs[i], NULL);
    }
}


static int emod_load(struct xmp_context *ctx, FILE *f, const int start)
{
    struct xmp_mod_context *m = &ctx->m;

    LOAD_INIT();

    read32b(f);		/* FORM */
    read32b(f);
    read32b(f);		/* EMOD */

    /* IFF chunk IDs */
    iff_register("EMIC", get_emic);
    iff_register("PATT", get_patt);
    iff_register("8SMP", get_8smp);

    /* Load IFF chunks */
    while (!feof(f))
	iff_chunk(ctx, f);

    iff_release();
    free(reorder);

    return 0;
}
