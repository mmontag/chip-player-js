/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: gdm_load.c,v 1.1 2007-11-20 23:38:19 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/*
 * Based on the GDM (General Digital Music) version 1.0 File Format
 * Specification - Revision 2 by MenTaLguY
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"

#define MAGIC_GDM	MAGIC4('G','D','M',0xfe)
#define MAGIC_GMFS	MAGIC4('G','M','F','S')

static char *fmt[] = {
	"?",
	"MOD",
	"MTM",
	"S3M",
	"669",
	"FAR",
	"ULT",
	"STM",
	"MED",
	"unknown"
};

static int gdm_test(FILE *, char *, const int);
static int gdm_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info gdm_loader = {
	"GDM",
	"Generic Digital Music",
	gdm_test,
	gdm_load
};

static int gdm_test(FILE *f, char *t, const int start)
{
	if (read32b(f) != MAGIC_GDM)
		return -1;

	fseek(f, start + 0x47, SEEK_SET);
	if (read32b(f) != MAGIC_GMFS)
		return -1;

	fseek(f, start + 4, SEEK_SET);
	read_title(f, t, 32);

	return 0;
}

static int gdm_load(struct xmp_context *ctx, FILE *f, const int start)
{
	struct xmp_player_context *p = &ctx->p;
	struct xmp_mod_context *m = &p->m;
	struct xxm_event *event;
	int i, j, k;
	int vermaj, vermin, tvmaj, tvmin, tracker;
	int origfmt, ord_ofs, pat_ofs, ins_ofs, smp_ofs;
	uint8 panmap[32];

	LOAD_INIT();

	read32b(f);		/* skip magic */
	fread(m->name, 1, 32, f);
	fread(m->author, 1, 32, f);

	fseek(f, 7, SEEK_CUR);

	vermaj = read8(f);
	vermin = read8(f);
	tracker = read16l(f);
	tvmaj = read8(f);
	tvmin = read8(f);

	if (tracker == 0) {
		sprintf(m->type, "GDM %d.%02d, (2GDM %d.%02d)",
					vermaj, vermin, tvmaj, tvmin);
	} else {
		sprintf(m->type, "GDM %d.%02d (unknown tracker %d.%02d)",
					vermaj, vermin, tvmaj, tvmin);
	}

	fread(panmap, 32, 1, f);

	m->xxh->gvl = read8(f);
	m->xxh->tpo = read8(f);
	m->xxh->bpm = read8(f);
	origfmt = read16l(f);
	ord_ofs = read32l(f);
	m->xxh->len = read8(f);
	pat_ofs = read32l(f);
	m->xxh->pat = read8(f);
	ins_ofs = read32l(f);
	smp_ofs = read32l(f);
	m->xxh->ins = m->xxh->smp = read8(f);
	
	MODULE_INFO();

	if (origfmt > 9)
		origfmt = 9;
	reportv(ctx, 0, "Orig format    : %s\n", fmt[origfmt]);

	fseek(f, start + ord_ofs, SEEK_SET);

	for (i = 0; i < m->xxh->len; i++)
		m->xxo[i] = read8(f);

	fseek(f, start + ins_ofs, SEEK_SET);

	INSTRUMENT_INIT();

#if 0
	for (i = 0; i < m->xxh->ins; i++) {
		m->xxi[i] = calloc(sizeof(struct xxm_instrument), 1);
		fread(buffer, 8, 1, f);
		copy_adjust(m->xxih[i].name, buffer, 8);
	}

	PATTERN_INIT();

	/* Read and convert patterns */
	reportv(ctx, 0, "Stored patterns: %d ", m->xxh->pat);

	for (i = 0; i < m->xxh->pat; i++) {
		PATTERN_ALLOC(i);
		m->xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		for (j = 0; j < m->xxp[i]->rows; j++) {
			for (k = 0; k < m->xxh->chn; k++) {
				int b;
				event = &EVENT (i, k, j);

				b = read8(f);
				if (b) {
					event->note = 12 * (b >> 4);
					event->note += (b & 0xf) + 24;
				}
				b = read8(f);
				event->ins = b >> 4;
				if (event->ins)
					event->ins += 1;
				if (b &= 0x0f) {
					switch (b) {
					case 0xd:
						event->fxt = FX_BREAK;
						event->fxp = 0;
						break;
					default:
						printf("---> %02x\n", b);
					}
				}
			}
		}
		reportv(ctx, 0, ".");
	}
	reportv(ctx, 0, "\n");

	base_offs = ftell(f);
	read32b(f);	/* remaining size */

	/* Read instrument data */
	reportv(ctx, 1, "     Name      Len  LBeg LEnd L Vol  ?? ?? ??\n");

	for (i = 0; i < m->xxh->ins; i++) {
		m->xxi[i][0].vol = read8(f) / 2;
		m->xxi[i][0].pan = 0x80;
		unk1[i] = read8(f);
		unk2[i] = read8(f);
		unk3[i] = read8(f);
	}


	for (i = 0; i < m->xxh->ins; i++) {
		soffs[i] = read32b(f);
		m->xxs[i].len = read32b(f);
	}

	for (i = 0; i < m->xxh->ins; i++) {
		m->xxih[i].nsm = !!(m->xxs[i].len);
		m->xxs[i].lps = 0;
		m->xxs[i].lpe = 0;
		m->xxs[i].flg = m->xxs[i].lpe > 0 ? WAVE_LOOPING : 0;
		m->xxi[i][0].fin = 0;
		m->xxi[i][0].pan = 0x80;
		m->xxi[i][0].sid = i;

		if (V(1) && (strlen((char*)m->xxih[i].name) || (m->xxs[i].len > 1))) {
			report("[%2X] %-8.8s  %04x %04x %04x %c "
						"V%02x  %02x %02x %02x\n",
				i, m->xxih[i].name,
				m->xxs[i].len, m->xxs[i].lps, m->xxs[i].lpe,
				m->xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
				m->xxi[i][0].vol, unk1[i], unk2[i], unk3[i]);
		}
	}

	/* Read samples */
	reportv(ctx, 0, "Stored samples : %d ", m->xxh->smp);
	for (i = 0; i < m->xxh->ins; i++) {
		fseek(f, start + base_offs + soffs[i], SEEK_SET);
		xmp_drv_loadpatch(ctx, f, m->xxi[i][0].sid, m->c4rate,
				XMP_SMP_UNS, &m->xxs[m->xxi[i][0].sid], NULL);
		reportv(ctx, 0, ".");
	}
	reportv(ctx, 0, "\n");
#endif

	return 0;
}
