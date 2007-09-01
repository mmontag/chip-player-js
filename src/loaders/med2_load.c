/* MED2/MED3 loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: med2_load.c,v 1.4 2007-09-01 01:23:28 cmatsuoka Exp $
 */

/*
 * MED 1.12 is in Fish disk #255
 * MED 2.00 is in Fish disk #349 and has a couple of demo modules
 *
 * ftp://ftp.funet.fi/pub/amiga/fish/201-300/ff255
 * ftp://ftp.funet.fi/pub/amiga/fish/301-400/ff349
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"

#define MAGIC_MED2	MAGIC4('M','E','D',2)
#define MAGIC_MED3	MAGIC4('M','E','D',3)
#define MASK		0x80000000


int med2_load(FILE * f)
{
	int i, j;
	struct xxm_event *event;
	uint32 ver, mask;

	LOAD_INIT();

	ver = read32b(f);

	if (ver != MAGIC_MED2 && ver != MAGIC_MED3)
		return -1;

	xxh->ins = xxh->smp = 32;
	INSTRUMENT_INIT();

	/* read instrument names */
	for (i = 0; i < 32; i++) {
		uint8 c, buf[40];
		for (j = 0; j < 40; j++) {
			c = read8(f);
			buf[j] = c;
			if (c == 0)
				break;
		}
		copy_adjust(xxih[i].name, buf, 32);
		xxi[i] = calloc(sizeof(struct xxm_instrument), 1);
	}

	/* read instrument volumes */
	mask = read32b(f);
	for (i = 0; i < 32; i++, mask <<= 1) {
		xxi[i][0].vol = mask & MASK ? read8(f) : 0;
		xxi[i][0].pan = 0x80;
		xxi[i][0].fin = 0;
		xxi[i][0].sid = i;
	}

	/* read instrument loops */
	mask = read32b(f);
	for (i = 0; i < 32; i++, mask <<= 1) {
		xxs[i].lps = mask & MASK ? read16b(f) : 0;
	}

	/* read instrument loop length */
	mask = read32b(f);
	for (i = 0; i < 32; i++, mask <<= 1) {
		uint32 lsiz = mask & MASK ? read16b(f) : 0;
		xxs[i].len = xxs[i].lps + lsiz;
		xxs[i].lpe = xxs[i].lps + lsiz;
		xxs[i].flg = lsiz > 1 ? WAVE_LOOPING : 0;
		xxih[i].nsm = !!(xxs[i].len);
	}

	xxh->chn = 4;
	xxh->pat = read16b(f);
	xxh->len = read16b(f);
	fread(xxo, 1, xxh->len, f);
	xxh->tpo = read16b(f);

	xxh->trk = xxh->chn * xxh->pat;

	if (ver == MAGIC_MED2) {
		/* MED2 header */
		strcpy(xmp_ctl->type, "MED2 (MED 1.11)");
	} else {
		/* MED3 header */
		strcpy(xmp_ctl->type, "MED3 (MED 2.00)");

		/* read midi channels */
		mask = read32b(f);
		for (i = 0; i < 32; i++, mask <<= 1) {
			if (mask & MASK)
				read8(f);
		}
	
		/* read midi programs */
		mask = read32b(f);
		for (i = 0; i < 32; i++, mask <<= 1) {
			if (mask & MASK)
				read8(f);
		}
	}
	
	MODULE_INFO();

	reportv(0, "Instruments    : %d\n", xxh->ins);
	reportv(1, "     Instrument name                  Len  LBeg LEnd L Vol\n");

	for (i = 0; i < xxh->ins; i++) {
		if ((V(1)) && (strlen((char *)xxih[i].name) || xxs[i].len > 2))
			report("[%2X] %-32.32s %04x %04x %04x %c V%02x\n",
			       i, xxih[i].name, xxs[i].len, xxs[i].lps,
			       xxs[i].lpe,
			       xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
			       xxi[i][0].vol);
	}

	PATTERN_INIT();

	/* Load and convert patterns */
	reportv(0, "Stored patterns: %d ", xxh->pat);

	for (i = 0; i < xxh->pat; i++) {
		uint8 b;
		uint16 sz;
		uint32 x;

		PATTERN_ALLOC(i);
		xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		read8(f);

		b = read8(f);
		sz = read16b(f);

		if (b & 0x10) {
			x = 0;
		} else if (b & 0x01) {
			x = 0xffffffff;
		} else {
			x = read32b(f);
		}
	
#if 0
		for (j = 0; j < (64 * 4); j++) {
			event = &EVENT(i, j % 4, j / 4);
			fread(mod_event, 1, 4, f);
			cvt_pt_event(event, mod_event);
		}
		if (xxh->chn > 4) {
			for (j = 0; j < (64 * 4); j++) {
				event = &EVENT(i, (j % 4) + 4, j / 4);
				fread(mod_event, 1, 4, f);
				cvt_pt_event(event, mod_event);
			}
		}
#endif

		reportv(0, ".");
	}

	/* Load samples */

	if (V(0))
		report("\nStored samples : %d ", xxh->smp);
	for (i = 0; i < xxh->smp; i++) {
		if (!xxs[i].len)
			continue;
		xmp_drv_loadpatch(f, xxi[i][0].sid, xmp_ctl->c4rate, 0,
				  &xxs[xxi[i][0].sid], NULL);
		reportv(0, ".");
	}
	reportv(0, "\n");

	return 0;
}
