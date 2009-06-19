/* Extended Module Player
 * Copyright (C) 1996-2009 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "../prowizard/prowiz.h"

extern struct list_head *checked_format;

static int pw_test(FILE *, char *, const int);
static int pw_load(struct xmp_context *, FILE *, const int);

struct xmp_loader_info pw_loader = {
	"pw",
	"prowizard",
	pw_test,
	pw_load
};

#define BUF_SIZE 4096

static int pw_test(FILE *f, char *t, const int start)
{
	unsigned char *b;
	int extra;
	int s = BUF_SIZE;

	b = calloc(1, BUF_SIZE);

	while ((extra = pw_check(b, s)) > 0) {
		b = realloc(b, s + extra);
		fread(b + s, extra, 1, f);
		s += extra;
	}

	free(b);

	if (extra == 0) {
		struct pw_format *format;
		format = list_entry(checked_format, struct pw_format, list);
		if (format->enable)
			return 0;
	}

	return -1;
}

static int pw_load(struct xmp_context *ctx, FILE * f, const int start)
{
	return 0;
}




#if 0
static int flt_load(struct xmp_context *ctx, FILE *f, const int start)
{
    struct xmp_player_context *p = &ctx->p;
    struct xmp_mod_context *m = &p->m;
    struct xmp_options *o = &ctx->o;
    int i, j;
    struct xxm_event *event;
    struct mod_header mh;
    uint8 mod_event[4];
    char *tracker;
    char filename[1024];
    char buf[16];
    FILE *nt;
    int am_synth;

    LOAD_INIT();

    tracker = "";

    fread(&mh.name, 20, 1, f);
    for (i = 0; i < 31; i++) {
	fread(&mh.ins[i].name, 22, 1, f);	/* Instrument name */
	mh.ins[i].size = read16b(f);		/* Length in 16-bit words */
	mh.ins[i].finetune = read8(f);		/* Finetune (signed nibble) */
	mh.ins[i].volume = read8(f);		/* Linear playback volume */
	mh.ins[i].loop_start = read16b(f);	/* Loop start in 16-bit words */
	mh.ins[i].loop_size = read16b(f);	/* Loop size in 16-bit words */
    }
    mh.len = read8(f);
    mh.restart = read8(f);
    fread(&mh.order, 128, 1, f);
    fread(&mh.magic, 4, 1, f);

    if (mh.magic[3] == '4')
	m->xxh->chn = 4;
    else
	m->xxh->chn = 8;

    m->xxh->len = mh.len;
    m->xxh->rst = mh.restart;
    memcpy(m->xxo, mh.order, 128);

    for (i = 0; i < 128; i++) {
	if (m->xxh->chn > 4)
	    m->xxo[i] >>= 1;
	if (m->xxo[i] > m->xxh->pat)
	    m->xxh->pat = m->xxo[i];
    }

    m->xxh->pat++;

    m->xxh->trk = m->xxh->chn * m->xxh->pat;

    strncpy(m->name, (char *) mh.name, 20);
    sprintf(m->type, "%4.4s (%s)", mh.magic, tracker);
    MODULE_INFO();

    INSTRUMENT_INIT();

    reportv(ctx, 1, "     Instrument name        Len  LBeg LEnd L Vol Fin\n");

    for (i = 0; i < m->xxh->ins; i++) {
	m->xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	m->xxs[i].len = 2 * mh.ins[i].size;
	m->xxs[i].lps = 2 * mh.ins[i].loop_start;
	m->xxs[i].lpe = m->xxs[i].lps + 2 * mh.ins[i].loop_size;
	m->xxs[i].flg = mh.ins[i].loop_size > 1 ? WAVE_LOOPING : 0;
	m->xxi[i][0].fin = (int8)(mh.ins[i].finetune << 4);
	m->xxi[i][0].vol = mh.ins[i].volume;
	m->xxi[i][0].pan = 0x80;
	m->xxi[i][0].sid = i;
	m->xxih[i].nsm = !!(m->xxs[i].len);
	m->xxih[i].rls = 0xfff;

	if (m->xxs[i].flg & WAVE_LOOPING) {
	    if (m->xxs[i].lps == 0 && m->xxs[i].len > m->xxs[i].lpe)
		m->xxs[i].flg |= WAVE_PTKLOOP;
	}

	copy_adjust(m->xxih[i].name, mh.ins[i].name, 22);

	if (V(1)) {
	    if (*m->xxih[i].name || m->xxs[i].len > 2) {
	        report("[%2X] %-22.22s %04x %04x %04x %c V%02x %+d %c\n",
			i, m->xxih[i].name, m->xxs[i].len, m->xxs[i].lps,
			m->xxs[i].lpe, mh.ins[i].loop_size > 1 ? 'L' : ' ',
			m->xxi[i][0].vol, m->xxi[i][0].fin >> 4,
			m->xxs[i].flg & WAVE_PTKLOOP ? '!' : ' ');
	    }
	}
    }

    PATTERN_INIT();

    /* Load and convert patterns */
    reportv(ctx, 0, "Stored patterns: %d ", m->xxh->pat);

    for (i = 0; i < m->xxh->pat; i++) {
	PATTERN_ALLOC(i);
	m->xxp[i]->rows = 64;
	TRACK_ALLOC(i);
	for (j = 0; j < (64 * 4); j++) {
	    event = &EVENT(i, j % 4, j / 4);
	    fread(mod_event, 1, 4, f);
	    cvt_pt_event(event, mod_event);
	}
	reportv(ctx, 0, ".");
    }

    m->xxh->flg |= XXM_FLG_MODRNG;

    if (o->skipsmp)
	goto end;

    /* Load samples */

    reportv(ctx, 0, "\nStored samples : %d ", m->xxh->smp);
    for (i = 0; i < m->xxh->smp; i++) {
	if (m->xxs[i].len == 0) {
	    if (am_synth && is_am_instrument(nt, i)) {
		read_am_instrument(ctx, nt, i);
	    }
	    continue;
	}
	xmp_drv_loadpatch(ctx, f, m->xxi[i][0].sid, m->c4rate, 0,
					&m->xxs[m->xxi[i][0].sid], NULL);
	reportv(ctx, 0, ".");
    }
    reportv(ctx, 0, "\n");

end:
    /* unlink temp file */
    return 0;
}
#endif
