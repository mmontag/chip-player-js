/* Archimedes Tracker module loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: sym_load.c,v 1.4 2007-08-25 01:11:57 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"

int sym_load(FILE * f)
{
	struct xxm_event *event;
	int i, j;
	uint32 a, b;
	int ver, sn[64];

	LOAD_INIT();

	a = read32b(f);
	b = read32b(f);

	if (a != 0x02011313 || b != 0x1412010B)		/* BASSTRAK */
		return -1;

	ver = read8(f);

	sprintf(xmp_ctl->type, "BASSTRAK v%d (Archimedes Symphony)", ver);

	xxh->chn = read8(f);
	xxh->len = xxh->pat = read16l(f);
	xxh->trk = read16l(f);	/* Symphony patterns are actually tracks */
	read24l(f);

	xxh->ins = xxh->smp = 63;

	INSTRUMENT_INIT();

	reportv(1, "     Name      Len  LBeg LEnd L Vol  ?? ?? ??\n");
	for (i = 0; i < xxh->ins; i++) {
		xxi[i] = calloc(sizeof(struct xxm_instrument), 1);

		sn[i] = read8(f);	/* sample name length */

		if (~sn[i] & 0x80)
			xxs[i].len = read24l(f);
	}

	a = read8(f);			/* track name length */
	fread(xmp_ctl->name, 1, a, f);
	fseek(f, 8, SEEK_CUR);		/* skip effects table */

	MODULE_INFO();

	PATTERN_INIT();

	/* Sequence */
	a = read8(f);			/* packing */
	reportv(0, "Packed sequence: %s\n", a ? "yes" : "no");
	for (i = 0; i < xxh->len; i++) {
		PATTERN_ALLOC(i);
		xxp[i]->rows = 64;
		for (j = 0; j < xxh->chn; j++) {
			xxp[i]->info[j].index = read16l(f);			
		}

		xxo[i] = i;
	}

	/* Read and convert patterns */
	reportv(0, "Stored tracks  : %d ", xxh->trk);
	for (i = 0; i < xxh->trk; i++) {
		xxt[i] = calloc(sizeof(struct xxm_track) +
				sizeof(struct xxm_event) * 64 - 1, 1);
		xxt[i]->rows = 64;

		a = read8(f);

		for (j = 0; j < xxp[i]->rows; j++) {
			event = &xxt[i]->event[j];

			b = read32l(f);
			event->note = b & 0x0000003f;
			if (event->note)
				event->note += 36;
			event->ins = (b & 0x00001fc0) >> 6;
			event->fxt = (b & 0x000fc000) >> 14;
			event->fxp = (b & 0xfff00000) >> 24;
		}
		reportv(0, "%c", a ? 'c' : '.');
	}
	reportv(0, "\n");




	for (i = 0; i < xxh->ins; i++) {
		if (V(1) && (strlen((char*)xxih[i].name) || (xxs[i].len > 1))) {
			report("[%2X] %-8.8s  %04x %04x %04x %c V%02x\n", i,
			       xxih[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe,
			       xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
			       xxi[i][0].vol);
		}
	}

	PATTERN_INIT();




	/* Read samples */
	reportv(0, "Stored samples : %d ", xxh->smp);
	for (i = 0; i < xxh->ins; i++) {
		/*xmp_drv_loadpatch(f, xxi[i][0].sid, xmp_ctl->c4rate,
				XMP_SMP_UNS, &xxs[xxi[i][0].sid], NULL);*/
		reportv(0, ".");
	}
	reportv(0, "\n");

	return 0;
}
