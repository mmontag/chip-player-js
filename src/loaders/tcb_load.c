/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/*
 * From http://www.tscc.de/ucm24/tcb2pro.html:
 * There are two different TCB-Tracker module formats. Older format and
 * newer format. They have different headers "AN COOL." and "AN COOL!".
 *
 * We only support the old format --claudio
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"


static int tcb_test(FILE *, char *, const int);
static int tcb_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info tcb_loader = {
	"TCB",
	"TCB Tracker",
	tcb_test,
	tcb_load
};

static int tcb_test(FILE *f, char *t, const int start)
{
	uint8 buffer[10];

	if (fread(buffer, 1, 8, f) < 8)
		return -1;
	if (memcmp(buffer, "AN COOL.", 8) && memcmp(buffer, "AN COOL!", 8))
		return -1;

	read_title(f, t, 0);

	return 0;
}

static int tcb_load(struct xmp_context *ctx, FILE *f, const int start)
{
	struct xmp_mod_context *m = &ctx->m;
	struct xmp_event *event;
	int i, j, k;
	uint8 buffer[10];
	int base_offs, soffs[16];
	uint8 unk1[16], unk2[16], unk3[16];

	LOAD_INIT();

	fread(buffer, 8, 1, f);

	set_type(m, "%-8.8s (TCB Tracker)", buffer);

	read16b(f);	/* ? */
	m->mod.pat = read16b(f);
	m->mod.ins = 16;
	m->mod.smp = m->mod.ins;
	m->mod.chn = 4;
	m->mod.trk = m->mod.pat * m->mod.chn;
	m->mod.flg |= XXM_FLG_MODRNG;

	read16b(f);	/* ? */

	for (i = 0; i < 128; i++)
		m->mod.xxo[i] = read8(f);

	m->mod.len = read8(f);
	read8(f);	/* ? */
	read16b(f);	/* ? */

	MODULE_INFO();

	INSTRUMENT_INIT();

	/* Read instrument names */
	for (i = 0; i < m->mod.ins; i++) {
		m->mod.xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
		fread(buffer, 8, 1, f);
		copy_adjust(m->mod.xxi[i].name, buffer, 8);
	}

	read16b(f);	/* ? */
	for (i = 0; i < 5; i++)
		read16b(f);
	for (i = 0; i < 5; i++)
		read16b(f);
	for (i = 0; i < 5; i++)
		read16b(f);

	PATTERN_INIT();

	/* Read and convert patterns */
	_D(_D_INFO "Stored patterns: %d ", m->mod.pat);

	for (i = 0; i < m->mod.pat; i++) {
		PATTERN_ALLOC(i);
		m->mod.xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		for (j = 0; j < m->mod.xxp[i]->rows; j++) {
			for (k = 0; k < m->mod.chn; k++) {
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
	}

	base_offs = ftell(f);
	read32b(f);	/* remaining size */

	/* Read instrument data */

	for (i = 0; i < m->mod.ins; i++) {
		m->mod.xxi[i].sub[0].vol = read8(f) / 2;
		m->mod.xxi[i].sub[0].pan = 0x80;
		unk1[i] = read8(f);
		unk2[i] = read8(f);
		unk3[i] = read8(f);
	}


	for (i = 0; i < m->mod.ins; i++) {
		soffs[i] = read32b(f);
		m->mod.xxs[i].len = read32b(f);
	}

	read32b(f);
	read32b(f);
	read32b(f);
	read32b(f);

	for (i = 0; i < m->mod.ins; i++) {
		m->mod.xxi[i].nsm = !!(m->mod.xxs[i].len);
		m->mod.xxs[i].lps = 0;
		m->mod.xxs[i].lpe = 0;
		m->mod.xxs[i].flg = m->mod.xxs[i].lpe > 0 ? XMP_SAMPLE_LOOP : 0;
		m->mod.xxi[i].sub[0].fin = 0;
		m->mod.xxi[i].sub[0].pan = 0x80;
		m->mod.xxi[i].sub[0].sid = i;

		_D(_D_INFO "[%2X] %-8.8s  %04x %04x %04x %c "
						"V%02x  %02x %02x %02x\n",
				i, m->mod.xxi[i].name,
				m->mod.xxs[i].len, m->mod.xxs[i].lps, m->mod.xxs[i].lpe,
				m->mod.xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
				m->mod.xxi[i].sub[0].vol, unk1[i], unk2[i], unk3[i]);
	}

	/* Read samples */

	_D(_D_INFO "Stored samples: %d", m->mod.smp);

	for (i = 0; i < m->mod.ins; i++) {
		fseek(f, start + base_offs + soffs[i], SEEK_SET);
		load_patch(ctx, f, m->mod.xxi[i].sub[0].sid,
				XMP_SMP_UNS, &m->mod.xxs[m->mod.xxi[i].sub[0].sid], NULL);
	}

	return 0;
}
