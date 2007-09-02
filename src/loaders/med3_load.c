/* MED3 loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * Pattern unpacking code by Teijo Kinnunen, 1990
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: med3_load.c,v 1.3 2007-09-02 06:14:49 cmatsuoka Exp $
 */

/*
 * MED 2.00 is in Fish disk #349 and has a couple of demo modules, get it
 * from ftp://ftp.funet.fi/pub/amiga/fish/301-400/ff349
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include "load.h"

#define MAGIC_MED3	MAGIC4('M','E','D',3)
#define MASK		0x80000000

#define M0F_LINEMSK0F	0x01
#define M0F_LINEMSK1F	0x02
#define M0F_FXMSK0F	0x04
#define M0F_FXMSK1F	0x08
#define M0F_LINEMSK00	0x10
#define M0F_LINEMSK10	0x20
#define M0F_FXMSK00	0x40
#define M0F_FXMSK10	0x80



/*
 * From the MED 2.00 file loading/saving routines by Teijo Kinnunen, 1990
 */

static uint8 get_nibble(uint8 *mem, uint16 *nbnum)
{
	uint8 *mloc = mem + (*nbnum / 2),res;

	if(*nbnum & 0x1)
		res = *mloc & 0x0f;
	else
		res = *mloc >> 4;
	(*nbnum)++;

	return res;
}

static uint16 get_nibbles(uint8 *mem,uint16 *nbnum,uint8 nbs)
{
	uint16 res = 0;

	while (nbs--) {
		res <<= 4;
		res |= get_nibble(mem,nbnum);
	}

	return res;
}

static void unpack_block(uint16 bnum, uint8 *from)
{
	struct xxm_event *event;
	uint32 linemsk0 = *((uint32 *)from), linemsk1 = *((uint32 *)from + 1);
	uint32 fxmsk0 = *((uint32 *)from + 2), fxmsk1 = *((uint32 *)from + 3);
	uint32 *lmptr = &linemsk0, *fxptr = &fxmsk0;
	uint16 fromn = 0, lmsk;
	uint8 *fromst = from + 16, bcnt, *tmpto;
	uint8 *patbuf, *to;
	int i, trkn = xxh->chn;

	from += 16;
	patbuf = to = malloc(2048);
	assert(to);

	for (i = 0; i < 64; i++) {
		if (i == 32) {
			lmptr = &linemsk1;
			fxptr = &fxmsk1;
		}

		if (*lmptr & MASK) {
			lmsk = get_nibbles(fromst, &fromn, (uint8)(trkn / 4));
			lmsk <<= (16 - trkn);
			tmpto = to;

			for (bcnt = 0; bcnt < trkn; bcnt++) {
				if (lmsk & 0x8000) {
					*tmpto = (uint8)get_nibbles(fromst,
						&fromn,2);
					*(tmpto + 1) = (get_nibble(fromst,
							&fromn) << 4);
				}
				lmsk <<= 1;
				tmpto += 3;
			}
		}

		if (*fxptr & MASK) {
			lmsk = get_nibbles(fromst,&fromn,(uint8)(trkn / 4));
			lmsk <<= (16 - trkn);
			tmpto = to;

			for (bcnt = 0; bcnt < trkn; bcnt++) {
				if (lmsk & 0x8000) {
					*(tmpto+1) |= get_nibble(fromst,
							&fromn);
					*(tmpto+2) = (uint8)get_nibbles(fromst,
							&fromn,2);
				}
				lmsk <<= 1;
				tmpto += 3;
			}
		}
		to += 3 * trkn;
		*lmptr <<= 1;
		*fxptr <<= 1;
	}

	//printf("=== %d ===\n", bnum);

	for (i = 0; i < 64; i++) {
		int j;
		//printf("%02x] ", i);
		for (j = 0; j < 4; j++) {
			event = &EVENT(bnum, j, i);

			/*printf("%02x %02x %02x  ",
				patbuf[i * 3 * 4 + j * 3 + 0],
				patbuf[i * 3 * 4 + j * 3 + 1],
				patbuf[i * 3 * 4 + j * 3 + 2]); */

			event->note = patbuf[i * 12 + j * 3 + 0];
			if (event->note)
				event->note += 36;
			event->ins  = patbuf[i * 12 + j * 3 + 1] >> 4;
			if (event->ins)
				event->ins++;
			event->fxt  = patbuf[i * 12 + j * 3 + 1] & 0x0f;
			event->fxp  = patbuf[i * 12 + j * 3 + 2];

			switch(event->fxt) {
			case 0x00:	/* arpeggio */
			case 0x01:	/* slide up */
			case 0x02:	/* slide down */
				break;
			case 0x03:	/* vibrato */
				event->fxt = FX_VIBRATO;
				break;
			case 0x0c:	/* set volume (BCD) */
				event->fxp = MSN(event->fxp) * 10 +
							LSN(event->fxp);
				break;
			case 0x0d:	/* volume slides */
				event->fxt = FX_VOLSLIDE;
				break;
			case 0x0f:	/* tempo/break */
				if (event->fxp == 0)
					event->fxt = FX_BREAK;
				break;
			default:
				event->fxp = event->fxt = 0;
			}
		}

		//printf("\n");
	}

	//printf("\n");
	free(patbuf);
}




int med3_load(FILE * f)
{
	int i, j;
	uint32 mask;
	int transp;

	LOAD_INIT();

	if (read32b(f) !=  MAGIC_MED3)
		return -1;

	strcpy(xmp_ctl->type, "MED3 (MED 2.00)");

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
	}

	xxh->chn = 4;
	xxh->pat = read16b(f);
	xxh->trk = xxh->chn * xxh->pat;

	xxh->len = read16b(f);
	fread(xxo, 1, xxh->len, f);
	xxh->tpo = read16b(f);
	transp = read8s(f);
	read8(f);			/* flags */
	read16b(f);			/* sliding */
	read32b(f);			/* jumping mask */
	fseek(f, 16, SEEK_CUR);		/* rgb */

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
	
	MODULE_INFO();

	PATTERN_INIT();

	/* Load and convert patterns */
	reportv(0, "Stored patterns: %d ", xxh->pat);

	for (i = 0; i < xxh->pat; i++) {
		uint32 *conv;
		uint8 b, tracks;
		uint16 convsz;

		PATTERN_ALLOC(i);
		xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		tracks = read8(f);

		b = read8(f);
		convsz = read16b(f);
//printf("tracks = %d, b = %02x, convsz = %d\n", tracks, b, convsz);
		conv = calloc(1, convsz + 16);
		assert(conv);

                if (b & M0F_LINEMSK00)
			*conv = 0L;
                else if (b & M0F_LINEMSK0F)
			*conv = 0xffffffff;
                else
			*conv = read32b(f);

                if (b & M0F_LINEMSK10)
			*(conv + 1) = 0L;
                else if (b & M0F_LINEMSK1F)
			*(conv + 1) = 0xffffffff;
                else
			*(conv + 1) = read32b(f);

                if (b & M0F_FXMSK00)
			*(conv + 2) = 0L;
                else if (b & M0F_FXMSK0F)
			*(conv + 2) = 0xffffffff;
                else
			*(conv + 2) = read32b(f);

                if (b & M0F_FXMSK10)
			*(conv + 3) = 0L;
                else if (b & M0F_FXMSK1F)
			*(conv + 3) = 0xffffffff;
                else
			*(conv + 3) = read32b(f);

		fread(conv + 4, 1, convsz, f);

                unpack_block(i, (uint8 *)conv);

		free(conv);

		reportv(0, ".");
	}
	reportv(0, "\n");

	/* Load samples */

	reportv(0, "Instruments    : %d ", xxh->ins);
	reportv(1, "\n     Instrument name                  Len  LBeg LEnd L Vol");

	mask = read32b(f);
	for (i = 0; i < 32; i++, mask <<= 1) {
		if (~mask & MASK)
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
