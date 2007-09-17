/* Desktop Tracker module loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: dtt_load.c,v 1.6 2007-09-17 19:18:33 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"

#define MAGIC_DskT	MAGIC4('D','s','k','T')
#define MAGIC_DskS	MAGIC4('D','s','k','S')


int dtt_load(FILE *f)
{
	struct xxm_event *event;
	int i, j, k;
	uint8 buf[100];
	uint32 flags;
	uint32 pofs[256];
	uint8 plen[256];
	int sdata[64];

	LOAD_INIT();

	if (read32b(f) != MAGIC_DskT)
		return -1;

	strcpy(xmp_ctl->type, "Desktop Tracker");

	fread(buf, 1, 64, f);
	strncpy(xmp_ctl->name, (char *)buf, XMP_DEF_NAMESIZE);
	fread(buf, 1, 64, f);
	strncpy(author_name, (char *)buf, XMP_DEF_NAMESIZE);
	
	flags = read32l(f);
	xxh->chn = read32l(f);
	xxh->len = read32l(f);
	fread(buf, 1, 8, f);
	xxh->tpo = read32l(f);
	xxh->rst = read32l(f);
	xxh->pat = read32l(f);
	xxh->ins = xxh->smp = read32l(f);
	xxh->trk = xxh->pat * xxh->chn;
	
	fread(xxo, 1, (xxh->len + 3) & ~3L, f);

	MODULE_INFO();

	for (i = 0; i < xxh->pat; i++)
		pofs[i] = read32l(f);

	fread(plen, 1, ((xxh->pat + 3) >> 2) << 2, f);

	INSTRUMENT_INIT();

	/* Read instrument names */
	reportv(1, "     Name                              Len  LBeg LEnd L Vol\n");
	for (i = 0; i < xxh->ins; i++) {
		int c2spd, looplen;

		xxi[i] = calloc(sizeof(struct xxm_instrument), 1);
		read8(f);			/* note */
		xxi[i][0].vol = read8(f) >> 1;
		xxi[i][0].pan = 0x80;
		read16l(f);			/* not used */
		c2spd = read32l(f);		/* period? */
		read32l(f);			/* sustain start */
		read32l(f);			/* sustain length */
		xxs[i].lps = read32l(f);
		looplen = read32l(f);
		xxs[i].flg = looplen > 0 ? WAVE_LOOPING : 0;
		xxs[i].lpe = xxs[i].lps + looplen;
		xxs[i].len = read32l(f);
		fread(buf, 1, 32, f);
		copy_adjust(xxih[i].name, (uint8 *)buf, 32);
		sdata[i] = read32l(f);

		xxih[i].nsm = !!(xxs[i].len);
		xxi[i][0].sid = i;

		if (V(1) && (strlen((char*)xxih[i].name) || (xxs[i].len > 1))) {
			report("[%2X] %-32.32s  %04x %04x %04x %c V%02x\n",
				i, xxih[i].name, xxs[i].len,
				xxs[i].lps, xxs[i].lpe,
				xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
				xxi[i][0].vol, c2spd);
		}
	}

	PATTERN_INIT();

	/* Read and convert patterns */
	reportv(0, "Stored patterns: %d ", xxh->pat);

	for (i = 0; i < xxh->pat; i++) {
		PATTERN_ALLOC(i);
		xxp[i]->rows = plen[i];
		TRACK_ALLOC(i);

		fseek(f, pofs[i], SEEK_SET);

		for (j = 0; j < xxp[i]->rows; j++) {
			for (k = 0; k < xxh->chn; k++) {
				uint32 x;

				event = &EVENT (i, k, j);
				x = read32l(f);

				event->ins  = (x & 0x0000003f);
				event->note = (x & 0x00000fc0) >> 6;
				event->fxt  = (x & 0x0001f000) >> 12;

				if (event->note)
					event->note += 36;

				/* sorry, we only have room for two effects */
				if (x & (0x1f << 17)) {
					event->f2p = (x & 0x003e0000) >> 17;
					x = read32l(f);
					event->fxp = (x & 0x000000ff);
					event->f2p = (x & 0x0000ff00) >> 8;
				} else {
					event->fxp = (x & 0xfc000000) >> 18;
				}
			}
		}
		reportv(0, ".");
	}
	reportv(0, "\n");

	/* Read samples */
	reportv(0, "Stored samples : %d ", xxh->smp);
	for (i = 0; i < xxh->ins; i++) {
		fseek(f, sdata[i], SEEK_SET);
		xmp_drv_loadpatch(f, xxi[i][0].sid, xmp_ctl->c4rate,
				XMP_SMP_VIDC, &xxs[xxi[i][0].sid], NULL);
		reportv(0, ".");
	}
	reportv(0, "\n");

	return 0;
}
