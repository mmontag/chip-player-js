/* Archimedes Tracker module loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: sym_load.c,v 1.1 2007-08-24 01:17:41 cmatsuoka Exp $
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
	int i, j, k;
	uint32 a, b;
	int ver;

	LOAD_INIT();

	a = read32b(f);
	b = read32b(f);

	if (a != 0x02011313 || b != 0x1412010B)		/* BASSTRAK */
		return -1;

	ver = read8(f);

	sprintf(xmp_ctl->type, "BASSTRAK v%d (Archimedes Symphony)", ver);

	xxh->chn = read8(f);
	xxh->len = read16l(f);
	xxh->pat = read16l(f);

	MODULE_INFO();

	INSTRUMENT_INIT();

	reportv(1, "     Name      Len  LBeg LEnd L Vol  ?? ?? ??\n");
	for (i = 0; i < xxh->ins; i++) {
		xxi[i] = calloc(sizeof(struct xxm_instrument), 1);

		if (V(1) && (strlen((char*)xxih[i].name) || (xxs[i].len > 1))) {
			report("[%2X] %-8.8s  %04x %04x %04x %c V%02x\n", i,
			       xxih[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe,
			       xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
			       xxi[i][0].vol);
		}
	}

	PATTERN_INIT();

	/* Read and convert patterns */
	reportv(0, "Stored patterns: %d ", xxh->pat);

	for (i = 0; i < xxh->pat; i++) {
		PATTERN_ALLOC(i);
		xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		for (j = 0; j < xxp[i]->rows; j++) {
			for (k = 0; k < xxh->chn; k++) {
				event = &EVENT (i, k, j);

			}
		}
		reportv(0, ".");
	}
	reportv(0, "\n");


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
