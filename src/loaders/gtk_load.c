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


static int gtk_test(FILE *, char *, const int);
static int gtk_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info gtk_loader = {
	"GTK",
	"Graoumf Tracker",
	gtk_test,
	gtk_load
};

static int gtk_test(FILE * f, char *t, const int start)
{
	char buf[4];

	if (fread(buf, 1, 4, f) < 4)
		return -1;

	if (memcmp(buf, "GTK", 3) || buf[3] > 4)
		return -1;

	read_title(f, t, 32);

	return 0;
}

static int gtk_load(struct xmp_context *ctx, FILE *f, const int start)
{
	struct xmp_mod_context *m = &ctx->m;
	struct xmp_event *event;
	int i, j, k;
	uint8 buffer[40];
	int rows, bits, c2spd, size;
	int ver, patmax;

	LOAD_INIT();

	fread(buffer, 4, 1, f);
	ver = buffer[3];
	fread(m->mod.name, 32, 1, f);
	set_type(m, "GTK v%d (Graoumf Tracker)", ver);
	fseek(f, 160, SEEK_CUR);	/* skip comments */

	m->mod.xxh->ins = read16b(f);
	m->mod.xxh->smp = m->mod.xxh->ins;
	rows = read16b(f);
	m->mod.xxh->chn = read16b(f);
	m->mod.xxh->len = read16b(f);
	m->mod.xxh->rst = read16b(f);

	MODULE_INFO();

	_D(_D_INFO "Instruments    : %d ", m->mod.xxh->ins);

	INSTRUMENT_INIT();
	for (i = 0; i < m->mod.xxh->ins; i++) {
		m->mod.xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
		fread(buffer, 28, 1, f);
		copy_adjust(m->mod.xxi[i].name, buffer, 28);

		if (ver == 1) {
			read32b(f);
			m->mod.xxs[i].len = read32b(f);
			m->mod.xxs[i].lps = read32b(f);
			size = read32b(f);
			m->mod.xxs[i].lpe = m->mod.xxs[i].lps + size - 1;
			read16b(f);
			read16b(f);
			m->mod.xxi[i].sub[0].vol = 0x40;
			m->mod.xxi[i].sub[0].pan = 0x80;
			bits = 1;
			c2spd = 8363;
		} else {
			fseek(f, 14, SEEK_CUR);
			read16b(f);		/* autobal */
			bits = read16b(f);	/* 1 = 8 bits, 2 = 16 bits */
			c2spd = read16b(f);
			c2spd_to_note(c2spd, &m->mod.xxi[i].sub[0].xpo, &m->mod.xxi[i].sub[0].fin);
			m->mod.xxs[i].len = read32b(f);
			m->mod.xxs[i].lps = read32b(f);
			size = read32b(f);
			m->mod.xxs[i].lpe = m->mod.xxs[i].lps + size - 1;
			m->mod.xxi[i].sub[0].vol = read16b(f) / 4;
			read8(f);
			m->mod.xxi[i].sub[0].fin = read8s(f);
		}

		m->mod.xxi[i].nsm = !!m->mod.xxs[i].len;
		m->mod.xxi[i].sub[0].sid = i;
		m->mod.xxs[i].flg = size > 2 ? XMP_SAMPLE_LOOP : 0;

		if (bits > 1) {
			m->mod.xxs[i].flg |= XMP_SAMPLE_16BIT;
			m->mod.xxs[i].len >>= 1;
			m->mod.xxs[i].lps >>= 1;
			m->mod.xxs[i].lpe >>= 1;
		}

		_D(_D_INFO "[%2X] %-28.28s  %05x%c%05x %05x %c "
						"V%02x F%+03d %5d", i,
			 	m->mod.xxi[i].name,
				m->mod.xxs[i].len,
				bits > 1 ? '+' : ' ',
				m->mod.xxs[i].lps,
				size,
				m->mod.xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
				m->mod.xxi[i].sub[0].vol, m->mod.xxi[i].sub[0].fin,
				c2spd);
	}

	for (i = 0; i < 256; i++)
		m->mod.xxo[i] = read16b(f);

	for (patmax = i = 0; i < m->mod.xxh->len; i++) {
		if (m->mod.xxo[i] > patmax)
			patmax = m->mod.xxo[i];
	}

	m->mod.xxh->pat = patmax + 1;
	m->mod.xxh->trk = m->mod.xxh->pat * m->mod.xxh->chn;

	PATTERN_INIT();

	/* Read and convert patterns */
	_D(_D_INFO "Stored patterns: %d", m->mod.xxh->pat);

	for (i = 0; i < m->mod.xxh->pat; i++) {
		PATTERN_ALLOC(i);
		m->mod.xxp[i]->rows = rows;
		TRACK_ALLOC(i);

		for (j = 0; j < m->mod.xxp[i]->rows; j++) {
			for (k = 0; k < m->mod.xxh->chn; k++) {
				event = &EVENT (i, k, j);

				event->note = read8(f);
				event->ins = read8(f);
				event->fxt = read8(f);
				event->fxp = read8(f);
				if (ver >= 4) {
					event->vol = read8(f);
				}

				/* Ignore extended effects */
				if (event->fxt > 0x0f || event->fxt == 0x0e ||
						event->fxt == 0x0c) {
					event->fxt = 0;
					event->fxp = 0;
				}
			}
		}
	}

	/* Read samples */
	_D(_D_INFO "Stored samples: %d", m->mod.xxh->smp);

	for (i = 0; i < m->mod.xxh->ins; i++) {
		if (m->mod.xxs[i].len == 0)
			continue;
		xmp_drv_loadpatch(ctx, f, m->mod.xxi[i].sub[0].sid, 0,
					&m->mod.xxs[m->mod.xxi[i].sub[0].sid], NULL);
	}

	return 0;
}
