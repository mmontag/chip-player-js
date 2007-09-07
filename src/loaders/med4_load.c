/* MED4 loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: med4_load.c,v 1.5 2007-09-07 12:10:26 cmatsuoka Exp $
 */

/*
 * MED 2.13 is in Fish disk #424 and has a couple of demo modules, get it
 * from ftp://ftp.funet.fi/pub/amiga/fish/401-500/ff424. Alex Van Starrex's
 * HappySong MED4 is in ff401. MED 3.00 is in ff476.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include "load.h"

#define MAGIC_MED4	MAGIC4('M','E','D',4)
#define MAGIC_MEDV	MAGIC4('M','E','D','V')


static inline uint8 read4(FILE *f)
{
	static uint8 b = 0;
	static int ctl = 0;
	uint8 ret;

	if (ctl & 0x01) {
		ret = b & 0x0f;
	} else {
		b = read8(f);
		ret = b >> 4;
	}

	ctl ^= 0x01;

	return ret;
}

static inline uint16 read12b(FILE *f)
{
	uint32 a, b, c;

	a = read4(f);
	b = read4(f);
	c = read4(f);

	return (a << 8) | (b << 4) | c;
}

int med4_load(FILE * f)
{
	int i, j, k;
	uint32 m, mask;
	int transp;
	int pos, ver;
	uint8 trkvol[16];
	struct xxm_event *event;

	LOAD_INIT();

	if (read32b(f) !=  MAGIC_MED4)
		return -1;

	pos = ftell(f);
	fseek(f, -11, SEEK_END);
	ver = read32b(f) == MAGIC_MEDV ? 3 : 2;
	fseek(f, pos, SEEK_SET);

	sprintf(xmp_ctl->type, "MED4 (MED %s)", ver > 2 ? "3.00" : "2.10");

	xxh->ins = xxh->smp = 32;
	INSTRUMENT_INIT();

	m = read8(f);
	for (mask = i = 0; i < 8; i++, m <<= 1) {
		if (m & 0x80) {
			mask <<= 8;
			mask |= read8(f);
		}
	}

	/* read instrument names */
	for (i = 0; i < 32; i++, mask <<= 1) {
		uint8 c, size, buf[40];

		xxi[i] = calloc(sizeof(struct xxm_instrument), 1);

		if (~mask & 0x80000000)
			continue;

		c = read8(f);

		size = read8(f);
		for (j = 0; j < size; j++)
			buf[j] = read8(f);
		buf[j] = 0;

		if (c == 0x4f)
			read8(f);
		if (c == 0x4c) {
			read32b(f);
			read8(f);
		}
		if (c == 0x4d) {
			read32b(f);
			read8(f);
		}
		if (c == 0x6c)
			read32b(f);

		copy_adjust(xxih[i].name, buf, 32);
//printf("%d = %s\n", i, xxih[i].name);
	}

	xxh->chn = 4;
	xxh->pat = read16b(f);
	xxh->trk = xxh->chn * xxh->pat;

	xxh->len = read16b(f);
	fread(xxo, 1, xxh->len, f);
	xxh->bpm = read16b(f);
        transp = read8s(f);
        read8s(f);
        read8s(f);
	xxh->tpo = read8(f);

	fseek(f, 20, SEEK_CUR);

	fread(trkvol, 1, 16, f);
	read8(f);		/* master vol */

	MODULE_INFO();

	reportv(0, "Play transpose : %d\n", transp);

	for (i = 0; i < 32; i++)
		xxi[i][0].xpo = transp;

	PATTERN_INIT();

	/* Load and convert patterns */
	reportv(0, "Stored patterns: %d ", xxh->pat);

	for (i = 0; i < xxh->pat; i++) {
		int size, plen;
		uint8 ctl, chmsk;
		uint32 linemsk0, fxmsk0, linemsk1, fxmsk1, x;

		PATTERN_ALLOC(i);
		xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		size = read8(f);	/* pattern control block */
		read16b(f);		/* 0x3f 0x04 */
		plen = read16b(f);
		ctl = read8(f);

		printf("size = %02x\n", size);
		printf("pen  = %04x\n", plen);
		printf("ctl  = %02x\n", ctl);

		linemsk0 = fxmsk0 = ctl & 0x80 ? 0xffffffff : 0x00000000;
		linemsk1 = fxmsk1 = ctl & 0x80 ? 0xffffffff : 0x00000000;

		if (~ctl & 0x80) {
			if (~ctl & 0x40)
				linemsk0 = read32b(f);
			if (~ctl & 0x10)
				fxmsk0   = read32b(f);
		}
		if (~ctl & 0x08) {
			if (~ctl & 0x04)
				linemsk1 = read32b(f);
			if (~ctl & 0x01)
				fxmsk1   = read32b(f);
		}

		printf("linemsk0 = %08x\n", linemsk0);
		printf("fxmsk0   = %08x\n", fxmsk0);
		printf("linemsk1 = %08x\n", linemsk1);
		printf("fxmsk1   = %08x\n", fxmsk1);

		read8(f);		/* 0xff */

		for (j = 0; j < 32; j++, linemsk0 <<= 1, fxmsk0 <<= 1) {
			if (linemsk0 & 0x80000000) {
				chmsk = read4(f);
				for (k = 0; k < 4; k++, chmsk <<= 1) {
					event = &EVENT(i, k, j);

					if (chmsk & 0x08) {
						x = read12b(f);
						event->note = x >> 4;
						if (event->note)
							event->note += 36;
						event->ins  = x & 0x0f;
					}
				}
			}

			if (fxmsk0 & 0x80000000) {
				chmsk = read4(f);
				for (k = 0; k < 4; k++, chmsk <<= 1) {
					event = &EVENT(i, k, j);

					if (chmsk & 0x08) {
						x = read12b(f);
						event->fxt = x >> 8;
						event->fxp = x & 0xff;
					}
				}
			}

			printf("%03d ", j);
			for (k = 0; k < 4; k++) {
				event = &EVENT(i, k, j);
				if (event->note)
					printf("%03d", event->note);
				else
					printf("---");
				printf(" %1x%1x%02x ",
					event->ins, event->fxt, event->fxp);
			}
			printf("\n");
		}

		for (j = 32; j < 64; j++, linemsk1 <<= 1, fxmsk1 <<= 1) {
			if (linemsk1 & 0x80000000) {
				chmsk = read4(f);
				for (k = 0; k < 4; k++, chmsk <<= 1) {
					event = &EVENT(i, k, j);

					if (chmsk & 0x08) {
						x = read12b(f);
						event->note = x >> 4;
						event->ins  = x & 0x0f;
					}
				}
			}

			if (fxmsk1 & 0x80000000) {
				chmsk = read4(f);
				for (k = 0; k < 4; k++, chmsk <<= 1) {
					event = &EVENT(i, k, j);

					if (chmsk & 0x08) {
						x = read12b(f);
						event->fxt = x >> 8;
						event->fxp = x & 0xff;
					}
				}
			}
		}

		reportv(0, ".");
	}
	reportv(0, "\n");

	/* Load samples */

	reportv(0, "Instruments    : %d ", xxh->ins);
	reportv(1, "\n     Instrument name                  Len  LBeg LEnd L Vol");

	mask = read32b(f);
	for (i = 0; i < 32; i++, mask <<= 1) {
		if (~mask & 0x80000000)
			continue;

		xxs[i].len = read32b(f);
		if (read16b(f))		/* type */
			continue;

		xxih[i].nsm = !!(xxs[i].len);

		reportv(1, "\n[%2X] %-32.32s %04x %04x %04x %c V%02x ",
			i, xxih[i].name, xxs[i].len, xxs[i].lps,
			xxs[i].lpe,
			xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
			xxi[i][0].vol);

		xmp_drv_loadpatch(f, xxi[i][0].sid, xmp_ctl->c4rate, 0,
				  &xxs[xxi[i][0].sid], NULL);
		reportv(0, ".");
	}
	reportv(0, "\n");

	return 0;
}
