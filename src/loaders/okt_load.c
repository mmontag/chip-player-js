/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Based on the format description written by Harald Zappe.
 * Additional information about Oktalyzer modules from Bernardo
 * Innocenti's XModule 3.4 sources.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "iff.h"


static int okt_test (FILE *, char *, const int);
static int okt_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info okt_loader = {
    "OKT",
    "Oktalyzer",
    okt_test,
    okt_load
};

static int okt_test(FILE *f, char *t, const int start)
{
    char magic[8];

    if (fread(magic, 1, 8, f) < 8)
	return -1;

    if (strncmp (magic, "OKTASONG", 8))
	return -1;

    read_title(f, t, 0);

    return 0;
}


#define OKT_MODE8 0x00		/* 7 bit samples */
#define OKT_MODE4 0x01		/* 8 bit samples */
#define OKT_MODEB 0x02		/* Both */

#define NONE 0xff

static int mode[36];
static int idx[36];
static int pattern = 0;
static int sample = 0;


static int fx[] = {
    NONE,
    FX_PORTA_UP,	/*  1 */
    FX_PORTA_DN,	/*  2 */
    NONE,
    NONE,
    NONE,
    NONE,
    NONE,
    NONE,
    NONE,
    FX_OKT_ARP3,	/* 10 */
    FX_OKT_ARP4,	/* 11 */
    FX_OKT_ARP5,	/* 12 */
    FX_NSLIDE_DN,	/* 13 */
    NONE,
    NONE, /* Filter */	/* 15 */
    NONE,
    FX_NSLIDE_UP,	/* 17 */
    NONE,
    NONE,
    NONE,
    FX_F_NSLIDE_DN,	/* 21 */
    NONE,
    NONE,
    NONE,
    FX_JUMP,		/* 25 */
    NONE,
    NONE, /* Release */	/* 27 */
    FX_TEMPO,		/* 28 */
    NONE,
    FX_F_NSLIDE_UP,	/* 30 */
    FX_VOLSET		/* 31 */
};


static void get_cmod(struct xmp_context *ctx, int size, FILE *f)
{ 
    struct xmp_mod_context *m = &ctx->m;
    int i, j, k;

    m->mod.xxh->chn = 0;
    for (i = 0; i < 4; i++) {
	j = read16b(f);
	for (k = !!j; k >= 0; k--) {
	    m->mod.xxc[m->mod.xxh->chn].pan = (((i + 1) / 2) % 2) * 0xff;
	    m->mod.xxh->chn++;
	}
    }
}


static void get_samp(struct xmp_context *ctx, int size, FILE *f)
{
    struct xmp_mod_context *m = &ctx->m;
    int i, j;
    int looplen;

    /* Should be always 36 */
    m->mod.xxh->ins = size / 32;  /* sizeof(struct okt_instrument_header); */
    m->mod.xxh->smp = m->mod.xxh->ins;

    INSTRUMENT_INIT();

    for (j = i = 0; i < m->mod.xxh->ins; i++) {
	m->mod.xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);

	fread(m->mod.xxi[i].name, 1, 20, f);
	str_adj((char *)m->mod.xxi[i].name);

	/* Sample size is always rounded down */
	m->mod.xxs[i].len = read32b(f) & ~1;
	m->mod.xxs[i].lps = read16b(f);
	looplen = read16b(f);
	m->mod.xxs[i].lpe = m->mod.xxs[i].lps + looplen;
	m->mod.xxi[i].sub[0].vol = read16b(f);
	mode[i] = read16b(f);

	m->mod.xxi[i].nsm = !!(m->mod.xxs[i].len);
	m->mod.xxs[i].flg = looplen > 2 ? XMP_SAMPLE_LOOP : 0;
	m->mod.xxi[i].sub[0].pan = 0x80;
	m->mod.xxi[i].sub[0].sid = j;

	idx[j] = i;

	_D(_D_INFO "[%2X] %-20.20s %05x %05x %05x %c V%02x M%02x\n", i,
		m->mod.xxi[i].name, m->mod.xxs[i].len, m->mod.xxs[i].lps,
		m->mod.xxs[i].lpe, m->mod.xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		m->mod.xxi[i].sub[0].vol, mode[i]);

	if (m->mod.xxi[i].nsm)
	    j++;
    }
}


static void get_spee(struct xmp_context *ctx, int size, FILE *f)
{
    struct xmp_mod_context *m = &ctx->m;

    m->mod.xxh->tpo = read16b(f);
    m->mod.xxh->bpm = 125;
}


static void get_slen(struct xmp_context *ctx, int size, FILE *f)
{
    struct xmp_mod_context *m = &ctx->m;

    m->mod.xxh->pat = read16b(f);
    m->mod.xxh->trk = m->mod.xxh->pat * m->mod.xxh->chn;
}


static void get_plen(struct xmp_context *ctx, int size, FILE *f)
{
    struct xmp_mod_context *m = &ctx->m;

    m->mod.xxh->len = read16b(f);
    _D(_D_INFO "Module length: %d", m->mod.xxh->len);
}


static void get_patt(struct xmp_context *ctx, int size, FILE *f)
{
    struct xmp_mod_context *m = &ctx->m;

    fread(m->mod.xxo, 1, m->mod.xxh->len, f);
}


static void get_pbod(struct xmp_context *ctx, int size, FILE *f)
{
    struct xmp_mod_context *m = &ctx->m;

    int j;
    uint16 rows;
    struct xmp_event *event;

    if (pattern >= m->mod.xxh->pat)
	return;

    if (!pattern) {
	PATTERN_INIT();
	_D(_D_INFO "Stored patterns: %d", m->mod.xxh->pat);
    }

    rows = read16b(f);

    PATTERN_ALLOC (pattern);
    m->mod.xxp[pattern]->rows = rows;
    TRACK_ALLOC (pattern);

    for (j = 0; j < rows * m->mod.xxh->chn; j++) {
	uint8 note, ins;

	event = &EVENT(pattern, j % m->mod.xxh->chn, j / m->mod.xxh->chn);
	memset(event, 0, sizeof(struct xmp_event));

	note = read8(f);
	ins = read8(f);

	if (note) {
	    event->note = 36 + note;
	    event->ins = 1 + ins;
	}

	event->fxt = fx[read8(f)];
	event->fxp = read8(f);

	if ((event->fxt == FX_VOLSET) && (event->fxp > 0x40)) {
	    if (event->fxp <= 0x50) {
		event->fxt = FX_VOLSLIDE;
		event->fxp -= 0x40;
	    } else if (event->fxp <= 0x60) {
		event->fxt = FX_VOLSLIDE;
		event->fxp = (event->fxp - 0x50) << 4;
	    } else if (event->fxp <= 0x70) {
		event->fxt = FX_EXTENDED;
		event->fxp = (EX_F_VSLIDE_DN << 4) | (event->fxp - 0x60);
	    } else if (event->fxp <= 0x80) {
		event->fxt = FX_EXTENDED;
		event->fxp = (EX_F_VSLIDE_UP << 4) | (event->fxp - 0x70);
	    }
	}
	if (event->fxt == FX_ARPEGGIO)	/* Arpeggio fixup */
	    event->fxp = (((24 - MSN (event->fxp)) % 12) << 4) | LSN (event->fxp);
	if (event->fxt == NONE)
	    event->fxt = event->fxp = 0;
    }
    pattern++;
}


static void get_sbod(struct xmp_context *ctx, int size, FILE *f)
{
    struct xmp_mod_context *m = &ctx->m;

    int flags = 0;
    int i;

    if (sample >= m->mod.xxh->ins)
	return;

    _D(_D_INFO "Stored samples: %d", m->mod.xxh->smp);

    i = idx[sample];
    if (mode[i] == OKT_MODE8 || mode[i] == OKT_MODEB)
	flags = XMP_SMP_7BIT;

    load_patch(ctx, f, sample, flags, &m->mod.xxs[i], NULL);

    sample++;
}


static int okt_load(struct xmp_context *ctx, FILE *f, const int start)
{
    struct xmp_mod_context *m = &ctx->m;

    LOAD_INIT();

    fseek(f, 8, SEEK_CUR);	/* OKTASONG */

    pattern = sample = 0;

    /* IFF chunk IDs */
    iff_register("CMOD", get_cmod);
    iff_register("SAMP", get_samp);
    iff_register("SPEE", get_spee);
    iff_register("SLEN", get_slen);
    iff_register("PLEN", get_plen);
    iff_register("PATT", get_patt);
    iff_register("PBOD", get_pbod);
    iff_register("SBOD", get_sbod);

    strcpy (m->mod.type, "OKT (Oktalyzer)");

    MODULE_INFO();

    /* Load IFF chunks */
    while (!feof(f))
	iff_chunk(ctx, f);

    iff_release();

    return 0;
}
