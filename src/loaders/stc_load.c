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

#include "load.h"
#include "period.h"
#include "spectrum.h"

/* ZX Spectrum Sound Tracker loader
 * Sound Tracker written by Jarek Burczynski (Bzyk), 1990
 */

static int stc_test(FILE *, char *, const int);
static int stc_load(struct xmp_context *, FILE *, const int);


struct xmp_loader_info stc_loader = {
	"STC",
	"Sound Tracker",
	stc_test,
	stc_load
};


#define MAX_PAT 32

struct stc_ord {
	int pattern;
	int height;
};

struct stc_pat {
	int ch[3];
};

static int stc_test(FILE * f, char *t, const int start)
{
	int pos_ptr, orn_ptr, pat_ptr;
	int i, len, max_pat;
	//char buf[8];

	fseek(f, start, SEEK_SET);

	if (read8(f) > 0x20)			/* Check tempo */
		return -1;

	pos_ptr = read16l(f);			/* Positions pointer */
	orn_ptr = read16l(f);			/* Ornaments pointer */
	pat_ptr = read16l(f);			/* Patterns pointer */

	if (pos_ptr < 138 || orn_ptr < 138 || pat_ptr < 138)
		return -1;

	fseek(f, start + pos_ptr, SEEK_SET);
	len = read8(f) + 1;

	for (max_pat = i = 0; i < len; i++) {
		int pat = read8(f);
		if (pat > MAX_PAT)		/* Check orders */
			return -1;
		if (pat > max_pat)
			max_pat = pat;
		read8(f);
	}

	fseek(f, pat_ptr, SEEK_SET);

	for (i = 0; i < max_pat; i++) {
		int num = read8(f);		/* Check track pointers */
		if (num != (i + 1))
			return -1;
		read16l(f);
		read16l(f);
		read16l(f);
	}

	if (read8(f) != 0xff)
		return -1;

	return 0;
}

static int stc_load(struct xmp_context *ctx, FILE * f, const int start)
{
	struct xmp_player_context *p = &ctx->p;
	struct xmp_mod_context *m = &p->m;
	struct xxm_event *event /*, *noise*/;
	int i, j;
	uint8 buf[100];
	int pos_ptr, orn_ptr, pat_ptr;
	struct stc_ord stc_ord[256];
	struct stc_pat stc_pat[MAX_PAT];
	int num, flag, orn;
	int *decoded;
	struct spectrum_extra *se;

	LOAD_INIT();

	m->xxh->tpo = read8(f);		/* Tempo */
	pos_ptr = read16l(f);		/* Positions pointer */
	orn_ptr = read16l(f);		/* Ornaments pointer */
	pat_ptr = read16l(f);		/* Patterns pointer */

	fread(buf, 18, 1, f);		/* Title */
	copy_adjust((uint8 *)m->name, (uint8 *)buf, 18);
	strcpy(m->type, "STC (ZX Spectrum Sound Tracker)");

	read16l(f);			/* Size */

	/* Read orders */

	fseek(f, pos_ptr, SEEK_SET);
	m->xxh->len = read8(f) + 1;

	for (num = i = 0; i < m->xxh->len; i++) {
		stc_ord[i].pattern = read8(f);
		stc_ord[i].height = read8s(f);
		//printf("%d %d -- ", stc_ord[i].pattern, stc_ord[i].height);

		for (flag = j = 0; j < i; j++) {
			if (stc_ord[i].pattern == stc_ord[j].pattern &&
				stc_ord[i].height == stc_ord[j].height)
			{
				m->xxo[i] = m->xxo[j];
				flag = 1;
				break;
			}
		}
		if (!flag) {
			m->xxo[i] = num++;
		}
		//printf("%d\n", m->xxo[i]);
	}

	m->xxh->chn = 3;
	m->xxh->pat = num;
	m->xxh->trk = m->xxh->pat * m->xxh->chn;
	m->xxh->ins = 15;
	m->xxh->smp = m->xxh->ins;
	orn = (pat_ptr - orn_ptr) / 33;

	MODULE_INFO();

	/* Read patterns */

	PATTERN_INIT();

	fseek(f, pat_ptr, SEEK_SET);
	decoded = calloc(m->xxh->pat, sizeof(int));
	reportv(ctx, 0, "Stored patterns: %d ", m->xxh->pat);

	for (i = 0; i < MAX_PAT; i++) {
		if (read8(f) == 0xff)
			break;
		stc_pat[i].ch[0] = read16l(f);
		stc_pat[i].ch[1] = read16l(f);
		stc_pat[i].ch[2] = read16l(f);
	}

	for (i = 0; i < m->xxh->len; i++) {		/* pattern */
		int src = stc_ord[i].pattern - 1;
		int dest = m->xxo[i];
		int trans = stc_ord[i].height;

		if (decoded[dest])
			continue;

		//printf("%d/%d) Read pattern %d -> %d\n", i, m->xxh->len, src, dest);

		PATTERN_ALLOC(dest);
		m->xxp[dest]->rows = 64;
		TRACK_ALLOC(dest);

		for (j = 0; j < 3; j++) {		/* row */
			int row = 0;
			int x;
			int rowinc = 0;
	
			fseek(f, stc_pat[src].ch[j], SEEK_SET);

			do {
				for (;;) {
					x = read8(f);

					if (x == 0xff)
						break;
	
//printf("pat %d, channel %d, row %d, x = %02x\n", dest, j, row, x);
					event = &EVENT(dest, j, row);
				
					if (x <= 0x5f) {
						event->note = x + 6 + trans;
						row += 1 + rowinc;
						break;
					}

					if (x <= 0x6f) {
						event->ins = x - 0x5f - 1;
					} else if (x <= 0x7f) {
						/* ornament*/
						event->fxt = FX_SYNTH_0;
						event->fxp = x - 0x70;
					} else if (x == 0x80) {
						event->note = XMP_KEY_OFF;
						row += 1 + rowinc;
						break;
					} else if (x == 0x81) {
						/* ? */
						row += 1 + rowinc;
					} else if (x == 0x82) {
						/* disable ornament/envelope */
						event->fxt = FX_SYNTH_0;
						event->fxp = 0;
						event->f2t = FX_SYNTH_2;
						event->f2p = 0;
					} else if (x <= 0x8e) {
						/* envelope */
						event->fxt = FX_SYNTH_0 +
							x - 0x80;      /* R13 */
						event->fxp = read8(f); /* R11 */
						event->f2t = FX_SYNTH_1;
						event->f2p = read8(f); /* R12 */
					} else {
						rowinc = x - 0xa1;
					}
				}
			} while (x != 0xff);
		}

		decoded[dest] = 1;

		reportv(ctx, 0, ".");
	}
	reportv(ctx, 0, "\n");

	free(decoded);

	/* Read instruments */

	INSTRUMENT_INIT();

	fseek(f, 27, SEEK_SET);

	reportv(ctx, 0, "Instruments    : %d ", m->xxh->ins);
	for (i = 0; i < m->xxh->ins; i++) {
		struct spectrum_sample ss;

		memset(&ss, 0, sizeof (struct spectrum_sample));
		m->xxi[i] = calloc(sizeof(struct xxm_instrument), 1);
		m->xxih[i].nsm = 1;
		m->xxi[i][0].vol = 0x40;
		m->xxi[i][0].pan = 0x80;
		m->xxi[i][0].xpo = -1;
		m->xxi[i][0].sid = i;

		fread(buf, 1, 99, f);

		if (buf[97] == 0) {
			ss.loop = 32;
			ss.length = 33;
		} else {
			ss.loop = buf[97] - 1;
			if (ss.loop > 31)
				ss.loop = 31;
			ss.length = buf[97] + buf[98];
			if (ss.length > 32)
				ss.length = 32;
			if (ss.length == 0)
				ss.length = 1;
			if (ss.loop >= ss.length)
				ss.loop = ss.length - 1;

			if (ss.length < 32) {
				ss.length += 33 - (ss.loop + 1);
				ss.loop = 32;
			}
		}

		/* Read sample ticks */

		for (j = 0; j < 31; j++) {
			struct spectrum_stick *sst = &ss.stick[j];
			uint8 *chdata = &buf[1 + j * 3];
			
			memset(sst, 0, sizeof (struct spectrum_stick));

			if (~chdata[1] & 0x80) {
				sst->flags |= SPECTRUM_FLAG_MIXNOISE;
				sst->noise_env_inc = chdata[1] & 0x1f;

				if (sst->noise_env_inc & 0x10)
					sst->noise_env_inc |= 0xf0;
			}

			if (~chdata[1] & 0x40)
				sst->flags |= SPECTRUM_FLAG_MIXTONE;

			sst->vol = chdata[0] & 0x0f;

			sst->tone_inc = (((int)(chdata[0] & 0xf0)) << 4) |
						chdata[2];

			if (~chdata[1] & 0x20)
				sst->tone_inc = -sst->tone_inc;

			sst->flags |= SPECTRUM_FLAG_ENVELOPE;

			/*if (j != 0) {
				reportv(ctx, 1, "               ");
			}
			reportv(ctx, 1, "%02X %c%c%c %c%03x %x\n", j,
				sst->flags & SPECTRUM_FLAG_MIXTONE ? 'T' : 't',
				sst->flags & SPECTRUM_FLAG_MIXNOISE ? 'N' : 'n',
				sst->flags & SPECTRUM_FLAG_ENVELOPE ? 'E' : 'e',
				sst->tone_inc >= 0 ? '+' : '-',
				sst->tone_inc >= 0 ?
					sst->tone_inc : -sst->tone_inc,
				sst->vol);*/
			
		}

		xmp_drv_loadpatch(ctx, f, i, 0, XMP_SMP_SPECTRUM, NULL,
								(char *)&ss);

		reportv(ctx, 0, ".");
	}
	reportv(ctx, 0, "\n");
	
	/* Read ornaments */

	fseek(f, orn_ptr, SEEK_SET);
	m->extra = calloc(1, sizeof (struct spectrum_extra));
	se = m->extra;

	reportv(ctx, 0, "Ornaments      : %d ", orn);
	for (i = 0; i < orn; i++) {
		int index;
		struct spectrum_ornament *so;

		index = read8(f);		

		so = &se->ornament[index];
		so->length = 32;
		so->loop = 31;

		for (j = 0; j < 32; j++) {
			so->val[j] = read8s(f);
		}

		reportv(ctx, 0, ".");
	}
	reportv(ctx, 0, "\n");

	for (i = 0; i < 4; i++) {
		m->xxc[i].pan = 0x80;
		m->xxc[i].flg = XXM_CHANNEL_SYNTH;
	}
	
	m->synth = &synth_spectrum;

	return 0;
}
