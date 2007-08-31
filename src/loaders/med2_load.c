/* MED2/MED3 loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: med2_load.c,v 1.3 2007-08-31 22:22:10 cmatsuoka Exp $
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
	uint32 ver, flags, mask, m;

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
	for (i = 0, m = mask; i < 32; i++, m <<= 1) {
		xxi[i][0].vol = m & MASK ? read8(f) : 0;
		xxi[i][0].pan = 0x80;
		xxi[i][0].fin = 0;
		xxi[i][0].sid = i;
	}

	fseek(f, 8, SEEK_CUR);	/* skip something */

	xxh->pat = read16b(f);
	xxh->len = read16b(f);
	xxh->chn = 4;
	xxh->trk = xxh->chn * xxh->pat;

	fread(xxo, 1, xxh->len, f);

	/* read instrument loops */
	for (i = 0; i < 32; i++) {
		xxs[i].lps = xxih[i].name[0] ? read16l(f) : 0;
	}

	/* read instrument loop length */
	for (i = 0; i < 32; i++) {
		uint32 lsiz = xxih[i].name[0] ? read16l(f) : 0;
		xxs[i].len = xxs[i].lps + lsiz;
		xxs[i].lpe = xxs[i].lps + lsiz;
		xxs[i].flg = lsiz > 1 ? WAVE_LOOPING : 0;
		xxih[i].nsm = !!(xxs[i].len);
	}
	
#if 0
	xxh->pat = read16b(f);
	fread(xxo, 1, 100, f);
	xxh->len = read16b(f);
	xxh->tpo = read16b(f);
	xxh->trk = xxh->chn * xxh->pat;

	if (ver == MAGIC_MED2) {
		/* MED2 header */
		strcpy(xmp_ctl->type, "MED2");
		strcpy(tracker_name, "MED 1.11/1.12");
		flags = read16b(f);
		read16b(f);		/* sliding (5 or 6) */
		read32b(f);		/* jump mask */
		fseek(f, 16, SEEK_CUR);	/* skip rgb */
	} else {
		/* MED3 header */
		strcpy(xmp_ctl->type, "MED3");
		strcpy(tracker_name, "MED 2.00");
		read8(f);	/* playtranspose */
		flags = read8(f);
		read16b(f);		/* sliding (5 or 6) */
		read32b(f);		/* jump mask */
		fseek(f, 16, SEEK_CUR);	/* skip rgb */
		fseek(f, 32, SEEK_CUR);	/* skip MIDI channels */
		fseek(f, 32, SEEK_CUR);	/* skip MIDI presets */
	}
#endif

	MODULE_INFO();

	reportv(0, "Instruments    : %d\n", xxh->ins);
	reportv(1, "     Instrument name                  Len  LBeg LEnd L Vol Fin\n");

	for (i = 0; i < xxh->ins; i++) {
		if ((V(1)) && (strlen((char *)xxih[i].name) || xxs[i].len > 2))
			report("[%2X] %-32.32s %04x %04x %04x %c V%02x %+d\n",
			       i, xxih[i].name, xxs[i].len, xxs[i].lps,
			       xxs[i].lpe,
			       xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
			       xxi[i][0].vol, (char)xxi[i][0].fin >> 4);
	}

	PATTERN_INIT();

	/* Load and convert patterns */
	reportv(0, "Stored patterns: %d ", xxh->pat);

#if 0
	for (i = 0; i < xxh->pat; i++) {
		PATTERN_ALLOC(i);
		xxp[i]->rows = 64;
		TRACK_ALLOC(i);
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

		if (V(0))
			report(".");
	}

	xxh->flg |= XXM_FLG_MODRNG;

	/* Load samples */

	if (V(0))
		report("\nStored samples : %d ", xxh->smp);
	for (i = 0; i < xxh->smp; i++) {
		if (!xxs[i].len)
			continue;
		xmp_drv_loadpatch(f, xxi[i][0].sid, xmp_ctl->c4rate, 0,
				  &xxs[xxi[i][0].sid], NULL);
		if (V(0))
			report(".");
	}
	if (V(0))
		report("\n");
#endif

	return 0;
}
