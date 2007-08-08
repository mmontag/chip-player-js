/* TCB Tracker module loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: tcb_load.c,v 1.2 2007-08-08 21:24:34 cmatsuoka Exp $
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
 * we only have victor2.mod to test, so we'll support only the older
 * format --claudio
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"

int tcb_load(FILE * f)
{
	struct xxm_event *event;
	int i, j, k;
	uint8 buffer[10];

	LOAD_INIT();

	fread(buffer, 8, 1, f);

	if (memcmp(buffer, "AN COOL.", 8) && memcmp(buffer, "AN COOL!", 8))
		return -1;

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

//printf("offset = %x\n", ftell(f));
	read16b(f);	/* ? */
	for (i = 0; i < 5; i++)
		read16b(f);
	for (i = 0; i < 5; i++)
		read16b(f);
	for (i = 0; i < 5; i++)
		read16b(f);

	PATTERN_INIT();

//printf("offset = %x\n", ftell(f));
	/* Read and convert patterns */
	if (V(0))
		report("Stored patterns: %d ", xxh->pat);

	for (i = 0; i < xxh->pat; i++) {
		PATTERN_ALLOC(i);
		xxp[i]->rows = 64;
		TRACK_ALLOC(i);

			for (j = 0; j < 64; j++) {
				for (k = 0; k < 4; k++) {
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
				}
		
		}

		if (V(0))
			report(".");
	}

	if (V(0))
		report("\n");

	read16b(f);
	read16b(f);

//printf("offset = %x\n", ftell(f));
	/* Read instrument data */
	if (V(1))
		report
		    ("     Name      Len  LBeg LEnd L Vol\n");

	for (i = 0; i < xxh->ins; i++) {
		xxi[i][0].vol = read8(f) / 4;
		read8(f);
		read8(f);
		read8(f);
	}

	read32b(f);

	for (i = 0; i < xxh->ins; i++) {
		xxs[i].len = read32b(f);
		read32b(f);
	}

	//for (i = 0; i < xxh->ins; i++)
	//	read32b(f);

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
			report("[%2X] %-8.8s  %04x %04x %04x %c V%02x\n", i,
			       xxih[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe,
			       xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
			       xxi[i][0].vol);
		}
	}

//printf("offset = %x\n", ftell(f));
	/* Read samples */
	if (V(0))
		report("Stored samples : %d ", xxh->smp);
	for (i = 0; i < xxh->ins; i++) {
//printf("offset sample %d = %x\n", i, ftell(f));
		xmp_drv_loadpatch(f, xxi[i][0].sid, xmp_ctl->c4rate,
				XMP_SMP_UNS, &xxs[xxi[i][0].sid], NULL);
		if (V(0))
			report(".");
	}
	if (V(0))
		report("\n");

	return 0;
}
