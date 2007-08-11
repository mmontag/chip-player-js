/* Graoumf Tracker GTK module loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: gtk_load.c,v 1.1 2007-08-11 00:17:49 cmatsuoka Exp $
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

int gtk_load(FILE * f)
{
	struct xxm_event *event;
	int i, j, k;
	uint8 buffer[40];
	int rows, bits, c2spd, size;
	int ver, patmax;

	LOAD_INIT();

	fread(buffer, 4, 1, f);
	if (memcmp(buffer, "GTK", 3) && buffer[3] != 3)
		return -1;
	ver = buffer[3];
	fread(xmp_ctl->name, 32, 1, f);
	sprintf(xmp_ctl->type, "Graoumf Tracker v%d (GTK)", ver);
	fseek(f, 160, SEEK_CUR);	/* skip comments */

	xxh->ins = read16b(f);
	xxh->smp = xxh->ins;
	rows = read16b(f);
	xxh->chn = read16b(f);
	xxh->len = read16b(f);
	xxh->rst = read16b(f);
	//xxh->flg |= XXM_FLG_MODRNG;

	MODULE_INFO();

	reportv(0, "Instruments    : %d ", xxh->ins);
	reportv(1, "\n     Name                          Len   LBeg  LSiz  L Vol");

	INSTRUMENT_INIT();
	for (i = 0; i < xxh->ins; i++) {
		xxi[i] = calloc(sizeof(struct xxm_instrument), 1);
		fread(buffer, 28, 1, f);
		copy_adjust(xxih[i].name, buffer, 28);

		if (ver == 1) {
			read32b(f);
			xxs[i].len = read32b(f);
			xxs[i].lps = read32b(f);
			size = read32b(f);
			xxs[i].lpe = xxs[i].lps + size - 1;
			read16b(f);
			read16b(f);
			xxi[i][0].vol = 0x40;
			bits = 1;
		} else {
			fseek(f, 14, SEEK_CUR);
			read16b(f);		/* autobal */
			bits = read16b(f);	/* 1 = 8 bits, 2 = 16 bits */
			c2spd = read16b(f);
			c2spd_to_note(c2spd, &xxi[i][0].xpo, &xxi[i][0].fin);
			xxs[i].len = read32b(f);
			xxs[i].lps = read32b(f);
			size = read32b(f);
			xxs[i].lpe = xxs[i].lps + size - 1;
			xxi[i][0].vol = read32b(f) / 4;
			read8(f);
			xxi[i][0].fin = read8s(f);
		}

		xxih[i].nsm = !!xxs[i].len;
		xxi[i][0].sid = i;
		xxs[i].flg = bits > 1 ? WAVE_16_BITS : 0;

		if (strlen((char*)xxih[i].name) || (xxs[i].len > 1)) {
			if (V(1)) {
				report("\n[%2X] %-28.28s  %05x %05x %05x %c "
						"V%02x", i,
			 		xxih[i].name,
					xxs[i].len, xxs[i].lps, size,
					xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
					xxi[i][0].vol);
			} else {
				report(".");
			}
		}
	}
	reportv(0, "\n");

	for (i = 0; i < 256; i++)
		xxo[i] = read8(f);

	for (patmax = i = 0; i < xxh->len; i++) {
		if (xxo[i] > patmax)
			patmax = xxo[i];
	}

	xxh->pat = patmax + 1;
	xxh->trk = xxh->pat * xxh->chn;

	PATTERN_INIT();

	/* Read and convert patterns */
	reportv(0, "Stored patterns: %d ", xxh->pat);

	for (i = 0; i < xxh->pat; i++) {
		PATTERN_ALLOC(i);
		xxp[i]->rows = rows;
		TRACK_ALLOC(i);

		for (j = 0; j < xxp[i]->rows; j++) {
			for (k = 0; k < xxh->chn; k++) {
				event = &EVENT (i, k, j);

				event->note = read8(f);
				event->ins = read8(f);
				read8(f);
				read8(f);
			}
		}
		reportv(0, ".");
	}
	reportv(0, "\n");

	/* Read samples */
	reportv(0, "Stored samples : %d ", xxh->smp);
	for (i = 0; i < xxh->ins; i++) {
		if (xxs[i].len == 0)
			continue;
		xmp_drv_loadpatch(f, xxi[i][0].sid, xmp_ctl->c4rate, 0,
						&xxs[xxi[i][0].sid], NULL);
		reportv(0, ".");
	}
	reportv(0, "\n");

	return 0;
}
