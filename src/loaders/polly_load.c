/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */


/*
 * Polly Tracker is a tracker for the Commodore 64 written by Aleksi Eeben.
 * See http://aleksieeben.blogspot.com/2007/05/polly-tracker.html for more
 * information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"

static int polly_test(FILE *, char *, const int);
static int polly_load(struct xmp_context *, FILE *, const int);

struct xmp_loader_info polly_loader = {
	"POLLY",
	"Polly Tracer",
	polly_test,
	polly_load
};

#define NUM_PAT 0x1f
#define PAT_SIZE (64 * 4)
#define ORD_OFS (NUM_PAT * PAT_SIZE)
#define SMP_OFS (NUM_PAT * PAT_SIZE + 256)


static void decode_rle(uint8 *out, FILE *f, int size)
{
	int i;

	for (i = 0; i < size; ) {
		int x = read8(f);

		if (feof(f))
			return;

		if (x == 0xae) {
			int n,v;
			switch (n = read8(f)) {
			case 0x01:
				out[i++] = 0xae;
				break;
			default:
				v = read8(f);
				while (n-- && i < size)
					out[i++] = v;
			}
		} else {
			out[i++] = x;
		}
	}
}

static int polly_test(FILE *f, char *t, const int start)
{
	int i;
	uint8 *buf;

	if (read8(f) != 0xae)
		return -1;

	if ((buf = malloc(0x10000)) == NULL)
		return -1;

	decode_rle(buf, f, 0x10000);

	for (i = 0; i < 128; i++) {
		if (buf[ORD_OFS + i] != 0 && buf[ORD_OFS] < 0xe0) {
			free(buf);
			return -1;
		}
	}

	if (t)
		memcpy(t, buf + ORD_OFS + 160, 16);

	free(buf);

	return 0;
}

static int polly_load(struct xmp_context *ctx, FILE *f, const int start)
{
	struct xmp_mod_context *m = &ctx->m;
	struct xmp_event *event;
	uint8 *buf;
	int i, j, k;

	LOAD_INIT();

	read8(f);			/* skip 0xae */
	/*
	 * File is RLE-encoded, escape is 0xAE (Aleksi Eeben's initials).
	 * Actual 0xAE is encoded as 0xAE 0x01
	 */
	if ((buf = calloc(1, 0x10000)) == NULL)
		return -1;

	decode_rle(buf, f, 0x10000);

	for (i = 0; buf[ORD_OFS + i] != 0 && i < 128; i++)
		m->mod.xxo[i] = buf[ORD_OFS + i] - 0xe0;
	m->mod.len = i;

	memcpy(m->mod.name, buf + ORD_OFS + 160, 16);
	/* memcpy(m->author, buf + ORD_OFS + 176, 16); */
	set_type(m, "Polly Tracker");
	MODULE_INFO();

	m->mod.tpo = 0x03;
	m->mod.bpm = 0x7d * buf[ORD_OFS + 193] / 0x88;
#if 0
	for (i = 0; i < 1024; i++) {
		if ((i % 16) == 0) printf("\n");
		printf("%02x ", buf[ORD_OFS + i]);
	}
#endif

	m->mod.pat = 0;
	for (i = 0; i < m->mod.len; i++) {
		if (m->mod.xxo[i] > m->mod.pat)
			m->mod.pat = m->mod.xxo[i];
	}
	m->mod.pat++;
	
	m->mod.chn = 4;
	m->mod.trk = m->mod.pat * m->mod.chn;

	PATTERN_INIT();

	_D(_D_INFO "Stored patterns: %d", m->mod.pat);

	for (i = 0; i < m->mod.pat; i++) {
		PATTERN_ALLOC(i);
		m->mod.xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		for (j = 0; j < 64; j++) {
			for (k = 0; k < 4; k++) {
				uint8 x = buf[i * PAT_SIZE + j * 4 + k];
				event = &EVENT(i, k, j);
				if (x == 0xf0) {
					event->fxt = FX_BREAK;
					event->fxp = 0;
					continue;
				}
				event->note = LSN(x);
				if (event->note)
					event->note += 36;
				event->ins = MSN(x);
			}
		}
	}

	m->mod.ins = m->mod.smp = 15;
	INSTRUMENT_INIT();

	for (i = 0; i < 15; i++) {
		m->mod.xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
		m->mod.xxs[i].len = buf[ORD_OFS + 129 + i] < 0x10 ? 0 :
					256 * buf[ORD_OFS + 145 + i];
		m->mod.xxi[i].sub[0].fin = 0;
		m->mod.xxi[i].sub[0].vol = 0x40;
		m->mod.xxs[i].lps = 0;
		m->mod.xxs[i].lpe = 0;
		m->mod.xxs[i].flg = 0;
		m->mod.xxi[i].sub[0].pan = 0x80;
		m->mod.xxi[i].sub[0].sid = i;
		m->mod.xxi[i].nsm = !!(m->mod.xxs[i].len);
		m->mod.xxi[i].rls = 0xfff;

                _D(_D_INFO "[%2X] %04x %04x %04x %c V%02x",
                       		i, m->mod.xxs[i].len, m->mod.xxs[i].lps,
                        	m->mod.xxs[i].lpe, ' ', m->mod.xxi[i].sub[0].vol);
	}

	/* Convert samples from 6 to 8 bits */
	for (i = SMP_OFS; i < 0x10000; i++)
		buf[i] = buf[i] << 2;

	/* Read samples */
	_D(_D_INFO "Loading samples: %d", m->mod.ins);

	for (i = 0; i < m->mod.ins; i++) {
		if (m->mod.xxs[i].len == 0)
			continue;
		load_patch(ctx, NULL, m->mod.xxi[i].sub[0].sid,
				XMP_SMP_NOLOAD | XMP_SMP_UNS,
				&m->mod.xxs[m->mod.xxi[i].sub[0].sid],
				(char*)buf + ORD_OFS + 256 +
					256 * (buf[ORD_OFS + 129 + i] - 0x10));
	}

	free(buf);

	/* make it mono */
	for (i = 0; i < m->mod.chn; i++)
		m->mod.xxc[i].pan = 0x80;

	m->mod.flg |= XXM_FLG_MODRNG;

	return 0;
}
