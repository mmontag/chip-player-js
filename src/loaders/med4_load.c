/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: med4_load.c,v 1.22 2007-11-20 17:53:37 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
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


static int med4_test(FILE *, char *, const int);
static int med4_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info med4_loader = {
	"MED4",
	"MED 2.10",
	med4_test,
	med4_load
};

static int med4_test(FILE *f, char *t, const int start)
{
	if (read32b(f) !=  MAGIC_MED4)
		return -1;

	read_title(f, t, 0);

	return 0;
}


static int read4_ctl;

static void fix_effect(struct xxm_event *event)
{
	switch (event->fxt) {
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
		if (event->fxp == 0xff) {
			event->fxp = event->fxt = 0;
			event->vol = 1;
		} else if (event->fxp == 0xfe) {
			event->fxp = event->fxt = 0;
		} else if (event->fxp == 0xf1) {
			event->fxt = FX_EXTENDED;
			event->fxp = (EX_RETRIG << 4) | 3;
		} else if (event->fxp == 0xf2) {
			event->fxt = FX_EXTENDED;
			event->fxp = (EX_CUT << 4) | 3;
		} else if (event->fxp == 0xf3) {
			event->fxt = FX_EXTENDED;
			event->fxp = (EX_DELAY << 4) | 3;
		} else if (event->fxp > 10) {
			event->fxt = FX_S3M_BPM;
			event->fxp = 125 * event->fxp / 33;
		}
		break;
	default:
		event->fxp = event->fxt = 0;
	}
}

static inline uint8 read4(FILE *f)
{
	static uint8 b = 0;
	uint8 ret;

	if (read4_ctl & 0x01) {
		ret = b & 0x0f;
	} else {
		b = read8(f);
		ret = b >> 4;
	}

	read4_ctl ^= 0x01;

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

static int med4_load(struct xmp_context *ctx, FILE *f, const int start)
{
	struct xmp_player_context *p = &ctx->p;
	struct xmp_mod_context *m = &p->m;
	int i, j, k;
	uint32 m0, mask;
	int transp, masksz;
	int pos, vermaj, vermin;
	uint8 trkvol[16], buf[1024];
	struct xxm_event *event;
	int flags, hexvol = 0;

	LOAD_INIT();

	read32b(f);

	vermaj = 2;
	vermin = 10;

	/*
	 * Check if we have a MEDV chunk at the end of the file
	 */
	pos = ftell(f);
	fseek(f, 0, SEEK_END);
	fseek(f, -1023, SEEK_CUR);
	fread(buf, 1, 1024, f);
	fseek(f, start + pos, SEEK_SET);
	for (i = 0; i < 1012; i++) {
		if (!memcmp(buf + i, "MEDV\000\000\000\004", 8)) {
			vermaj = *(buf + i + 10);
			vermin = *(buf + i + 11);
			break;
		}
	}

	sprintf(m->type, "MED4 (MED %d.%02d)", vermaj, vermin);

	m0 = read8(f);

	mask = masksz = 0;
	for (i = 0; i < 8; i++, m0 <<= 1) {
		if (m0 & 0x80) {
			mask <<= 8;
			mask |= read8(f);
			masksz++;
		}
	}
	mask <<= (32 - masksz * 8);

	m->xxh->ins = m->xxh->smp = 32;
	INSTRUMENT_INIT();

	/* read instrument names */
	for (i = 0; i < 32; i++, mask <<= 1) {
		uint8 c, size, buf[40];
		uint16 loop_len;

		m->xxi[i] = calloc(sizeof(struct xxm_instrument), 1);

		if (~mask & 0x80000000)
			continue;

		c = read8(f);

		size = read8(f);
		for (j = 0; j < size; j++)
			buf[j] = read8(f);
		buf[j] = 0;

		m->xxs[i].lps = 0;
		loop_len = 0;
		m->xxi[i][0].vol = 0x40;

#if 0
		m->xxs[i].lps    = c & 0x01 ? 0    : read16b(f) << 1;
		loop_len         = c & 0x02 ? 0    : read16b(f) << 1;
		m->xxi[i][0].vol = c & 0x20 ? 0x40 : read8(f);
		m->xxi[i][0].xpo = c & 0x40 ? 0x00 : read8(f);
#endif

		if ((c & 0x01) == 0)
			m->xxs[i].lps = read16b(f) << 1;
		if ((c & 0x02) == 0)
			loop_len = read16b(f) << 1;
		if ((c & 0x30) == 0)
			m->xxi[i][0].vol = read8(f);

		m->xxs[i].lpe = m->xxs[i].lps + loop_len;
		if (loop_len > 0)
			m->xxs[i].flg |= WAVE_LOOPING;

		copy_adjust(m->xxih[i].name, buf, 32);
		//printf("%d = %s\n", i, m->xxih[i].name);
	}

	m->xxh->pat = read16b(f);
	m->xxh->len = read16b(f);
	fread(m->xxo, 1, m->xxh->len, f);
	m->xxh->bpm = 125 * read16b(f) / 33;
        transp = read8s(f);
        read8s(f);
        flags = read8s(f);
	m->xxh->tpo = read8(f);

	if (~flags & 0x20)	/* sliding */
		m->fetch |= XMP_CTL_VSALL | XMP_CTL_PBALL;

	if (flags & 0x10)	/* dec/hex volumes */
		hexvol = 1;	/* not implemented */

	/* This is just a guess... */
	if (vermaj == 2)	/* Happy.med has tempo 5 but loads as 6 */
		m->xxh->tpo = flags & 0x20 ? 5 : 6;

	fseek(f, 20, SEEK_CUR);

	fread(trkvol, 1, 16, f);
	read8(f);		/* master vol */

	MODULE_INFO();

	reportv(ctx, 0, "Play transpose : %d semitones\n", transp);

	for (i = 0; i < 32; i++)
		m->xxi[i][0].xpo += transp;

	read8(f);
	m->xxh->chn = read8(f);;
	fseek(f, -2, SEEK_CUR);
	m->xxh->trk = m->xxh->chn * m->xxh->pat;

	PATTERN_INIT();

	/* Load and convert patterns */
	reportv(ctx, 0, "Stored patterns: %d ", m->xxh->pat);

	for (i = 0; i < m->xxh->pat; i++) {
		int size, plen;
		uint8 ctl, chmsk, chn, rows;
		uint32 linemsk0, fxmsk0, linemsk1, fxmsk1, x;

		/*printf("\n===== PATTERN %d =====\n", i);
		printf("offset = %lx\n", ftell(f));*/

		size = read8(f);	/* pattern control block */
		chn = read8(f);
		rows = read8(f) + 1;
		plen = read16b(f);
		ctl = read8(f);

		PATTERN_ALLOC(i);
		m->xxp[i]->rows = rows;
		TRACK_ALLOC(i);

		linemsk0 = ctl & 0x80 ? ~0 : ctl & 0x40 ? 0 : read32b(f);
		fxmsk0   = ctl & 0x20 ? ~0 : ctl & 0x10 ? 0 : read32b(f);
		linemsk1 = ctl & 0x08 ? ~0 : ctl & 0x04 ? 0 : read32b(f);
		fxmsk1   = ctl & 0x02 ? ~0 : ctl & 0x01 ? 0 : read32b(f);

		/*printf("size = %02x\n", size);
		printf("chn  = %01x\n", chn);
		printf("rows = %01x\n", rows);
		printf("plen = %04x\n", plen);
		printf("ctl  = %02x\n", ctl);
		printf("linemsk0 = %08x\n", linemsk0);
		printf("fxmsk0   = %08x\n", fxmsk0);
		printf("linemsk1 = %08x\n", linemsk1);
		printf("fxmsk1   = %08x\n", fxmsk1);*/

		x = read8(f);		/* 0xff */
		//printf("blk end  = %02x\n\n", x);

		read4_ctl = 0;

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
						fix_effect(event);
					}
				}
			}

			/*printf("%03d ", j);
			for (k = 0; k < 4; k++) {
				event = &EVENT(i, k, j);
				if (event->note)
					printf("%03d", event->note);
				else
					printf("---");
				printf(" %1x%1x%02x ",
					event->ins, event->fxt, event->fxp);
			}
			printf("\n");*/
		}

		for (j = 32; j < 64; j++, linemsk1 <<= 1, fxmsk1 <<= 1) {
			if (linemsk1 & 0x80000000) {
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

			if (fxmsk1 & 0x80000000) {
				chmsk = read4(f);
				for (k = 0; k < 4; k++, chmsk <<= 1) {
					event = &EVENT(i, k, j);

					if (chmsk & 0x08) {
						x = read12b(f);
						event->fxt = x >> 8;
						event->fxp = x & 0xff;
						fix_effect(event);
					}
				}
			}

			/*printf("%03d ", j);
			for (k = 0; k < 4; k++) {
				event = &EVENT(i, k, j);
				if (event->note)
					printf("%03d", event->note);
				else
					printf("---");
				printf(" %1x%1x%02x ",
					event->ins, event->fxt, event->fxp);
			}
			printf("\n");*/
		}

		reportv(ctx, 0, ".");
	}
	reportv(ctx, 0, "\n");

	/* Load samples */

	reportv(ctx, 0, "Instruments    : %d ", m->xxh->ins);
	reportv(ctx, 1, "\n     Instrument name                  Len  LBeg LEnd L Vol Xpo");

	mask = read32b(f);

	read16b(f);

	//printf("instrument mask: %08x\n", mask);

	mask <<= 1;	/* no instrument #0 */
	for (i = 0; i < 32; i++, mask <<= 1) {
		if (~mask & 0x80000000)
			continue;

		read16b(f);
		read16b(f);
		m->xxs[i].len = read16b(f);

		m->xxi[i][0].sid = i;
		m->xxih[i].nsm = !!(m->xxs[i].len);

		reportv(ctx, 1, "\n[%2X] %-32.32s %04x %04x %04x %c V%02x %+03d ",
			i, m->xxih[i].name, m->xxs[i].len, m->xxs[i].lps,
			m->xxs[i].lpe,
			m->xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
			m->xxi[i][0].vol, m->xxi[i][0].xpo);

		xmp_drv_loadpatch(ctx, f, m->xxi[i][0].sid, m->c4rate, 0,
				  &m->xxs[m->xxi[i][0].sid], NULL);
		reportv(ctx, 0, ".");
	}
	reportv(ctx, 0, "\n");

	return 0;
}
