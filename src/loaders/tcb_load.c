/* TCB Tracker module loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: tcb_load.c,v 1.10 2007-10-13 23:21:26 cmatsuoka Exp $
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


static int tcb_test(FILE *, char *);
static int tcb_load(FILE *);

struct xmp_loader_info tcb_loader = {
	"TCB",
	"TCB Tracker",
	tcb_test,
	tcb_load
};

static int tcb_test(FILE *f, char *t)
{
	uint8 buffer[10];

	fread(buffer, 8, 1, f);
	if (memcmp(buffer, "AN COOL.", 8) && memcmp(buffer, "AN COOL!", 8))
		return -1;

	read_title(f, t, 0);

	return 0;
}

static int tcb_load(FILE *f)
{
	struct xxm_event *event;
	int i, j, k;
	uint8 buffer[10];
	int base_offs, soffs[16];
	uint8 unk1[16], unk2[16], unk3[16];

	LOAD_INIT();

	fread(buffer, 8, 1, f);

	sprintf(xmp_ctl->type, "%-8.8s (TCB Tracker)", buffer);

	read16b(f);	/* ? */
	xxh->pat = read16b(f);
	xxh->ins = 16;
	xxh->smp = xxh->ins;
	xxh->chn = 4;
	xxh->trk = xxh->pat * xxh->chn;
	xxh->flg |= XXM_FLG_MODRNG;

	read16b(f);	/* ? */

	for (i = 0; i < 128; i++)
		xxo[i] = read8(f);

	xxh->len = read8(f);
	read8(f);	/* ? */
	read16b(f);	/* ? */

	MODULE_INFO();

	INSTRUMENT_INIT();

	/* Read instrument names */
	for (i = 0; i < xxh->ins; i++) {
		xxi[i] = calloc(sizeof(struct xxm_instrument), 1);
		fread(buffer, 8, 1, f);
		copy_adjust(xxih[i].name, buffer, 8);
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
	reportv(0, "Stored patterns: %d ", xxh->pat);

	for (i = 0; i < xxh->pat; i++) {
		PATTERN_ALLOC(i);
		xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		for (j = 0; j < xxp[i]->rows; j++) {
			for (k = 0; k < xxh->chn; k++) {
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
		reportv(0, ".");
	}
	reportv(0, "\n");

	base_offs = ftell(f);
	read32b(f);	/* remaining size */

	/* Read instrument data */
	reportv(1, "     Name      Len  LBeg LEnd L Vol  ?? ?? ??\n");

	for (i = 0; i < xxh->ins; i++) {
		xxi[i][0].vol = read8(f) / 2;
		xxi[i][0].pan = 0x80;
		unk1[i] = read8(f);
		unk2[i] = read8(f);
		unk3[i] = read8(f);
	}


	for (i = 0; i < xxh->ins; i++) {
		soffs[i] = read32b(f);
		xxs[i].len = read32b(f);
	}

	read32b(f);
	read32b(f);
	read32b(f);
	read32b(f);

	for (i = 0; i < xxh->ins; i++) {
		xxih[i].nsm = !!(xxs[i].len);
		xxs[i].lps = 0;
		xxs[i].lpe = 0;
		xxs[i].flg = xxs[i].lpe > 0 ? WAVE_LOOPING : 0;
		xxi[i][0].fin = 0;
		xxi[i][0].pan = 0x80;
		xxi[i][0].sid = i;

		if (V(1) && (strlen((char*)xxih[i].name) || (xxs[i].len > 1))) {
			report("[%2X] %-8.8s  %04x %04x %04x %c "
						"V%02x  %02x %02x %02x\n",
				i, xxih[i].name,
				xxs[i].len, xxs[i].lps, xxs[i].lpe,
				xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
				xxi[i][0].vol, unk1[i], unk2[i], unk3[i]);
		}
	}

	/* Read samples */
	reportv(0, "Stored samples : %d ", xxh->smp);
	for (i = 0; i < xxh->ins; i++) {
		fseek(f, base_offs + soffs[i], SEEK_SET);
		xmp_drv_loadpatch(f, xxi[i][0].sid, xmp_ctl->c4rate,
				XMP_SMP_UNS, &xxs[xxi[i][0].sid], NULL);
		reportv(0, ".");
	}
	reportv(0, "\n");

	return 0;
}
