/* MED2/MED3 loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * Pattern unpacking code by Teijo Kinnunen, 1990
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: med2_load.c,v 1.5 2007-09-01 14:44:11 cmatsuoka Exp $
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

#include <assert.h>
#include "load.h"

#define MAGIC_MED2	MAGIC4('M','E','D',2)
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

static uint8 get_nibble(uint8 *mem,uint16 *nbnum)
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

		if (*lmptr & 0x80000000) {
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

		if (*fxptr & 0x80000000) {
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

	free(patbuf);
}




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
		strcpy(xmp_ctl->type, "MED2 (MED 1.12)");
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
		uint32 *conv;
		uint8 b, tracks;
		uint16 convsz;

		PATTERN_ALLOC(i);
		xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		tracks = read8(f);

		b = read8(f);
		convsz = read16b(f);
		conv = calloc(1, convsz + 64);
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

		*(conv + 4) = read32b(f);

                unpack_block(i, (uint8 *)conv);

		free(conv);

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
