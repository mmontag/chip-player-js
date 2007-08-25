/* Archimedes Tracker module loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: sym_load.c,v 1.5 2007-08-25 21:35:55 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"


static void fix_effect(struct xxm_event *e, int parm)
{
	switch (e->fxt) {
	case 0x03:		/* 03 xyy Tone Portamento */
	case 0x04:		/* 04 xyz Vibrato */
	case 0x05:		/* 05 xyz Tone Portamento + Volume Slide */
	case 0x06:		/* 06 xyz Vibrato + Volume Slide */
	case 0x07:		/* 07 xyz Tremolo */
		e->fxp = parm;
		break;
	case 0x0b:		/* 0B xxx Position Jump */
	case 0x0c:		/* 0C xyy Set Volume */
	case 0x0d:		/* 0D xyy Pattern Break */
	case 0x0f:		/* 0F xxx Set Speed */
		e->fxp = parm;
		break;
	default:
		e->fxp = 0;
	}
}


int sym_load(FILE * f)
{
	struct xxm_event *event;
	int i, j;
	uint32 a, b;
	int ver, sn[64];

	LOAD_INIT();

	a = read32b(f);
	b = read32b(f);

	if (a != 0x02011313 || b != 0x1412010B)		/* BASSTRAK */
		return -1;

	ver = read8(f);

	sprintf(xmp_ctl->type, "BASSTRAK v%d (Archimedes Symphony)", ver);

	xxh->chn = read8(f);
	xxh->len = xxh->pat = read16l(f);
	xxh->trk = read16l(f);	/* Symphony patterns are actually tracks */
	read24l(f);

	xxh->ins = xxh->smp = 63;

	INSTRUMENT_INIT();

	for (i = 0; i < xxh->ins; i++) {
		xxi[i] = calloc(sizeof(struct xxm_instrument), 1);

		sn[i] = read8(f);	/* sample name length */

		if (~sn[i] & 0x80)
			xxs[i].len = read24l(f) << 1;
	}

	a = read8(f);			/* track name length */
	fread(xmp_ctl->name, 1, a, f);
	fseek(f, 8, SEEK_CUR);		/* skip effects table */

	MODULE_INFO();

	PATTERN_INIT();

	/* Sequence */
	a = read8(f);			/* packing */
	reportv(0, "Packed sequence: %s\n", a ? "yes" : "no");
	for (i = 0; i < xxh->len; i++) {
		PATTERN_ALLOC(i);
		xxp[i]->rows = 64;
		for (j = 0; j < xxh->chn; j++) {
			xxp[i]->info[j].index = read16l(f);			
		}
		xxo[i] = i;
	}

	/* Read and convert patterns */

	a = read8(f);
	reportv(0, "Packed tracks  : %s\n", a ? "yes" : "no");
	reportv(0, "Stored tracks  : %d ", xxh->trk);

	for (i = 0; i < xxh->trk; i++) {
		xxt[i] = calloc(sizeof(struct xxm_track) +
				sizeof(struct xxm_event) * 64 - 1, 1);
		xxt[i]->rows = 64;

		for (j = 0; j < xxt[i]->rows; j++) {
			event = &xxt[i]->event[j];
			int parm;

			b = read32l(f);
			event->note = b & 0x0000003f;
			if (event->note)
				event->note += 36;
			event->ins = (b & 0x00001fc0) >> 6;
			event->fxt = (b & 0x000fc000) >> 14;
			parm = (b & 0xfff00000) >> 20;

			fix_effect(event, parm);
		}
		if (V(0) && (i % xxh->chn) == 0)
			report(".");
	}
	reportv(0, "\n");

	/* Load and convert instruments */

	reportv(1, "     Instrument Name                  Len   LBeg  LEnd  L Vol\n");
	for (i = 0; i < xxh->ins; i++) {
		uint8 buf[128];

		memset(buf, 0, 128);
		fread(buf, 1, sn[i] & 0x7f, f);
		copy_adjust(xxih[i].name, buf, 32);

		if (~sn[i] & 0x80) {
			int looplen;

			xxs[i].lps = read24l(f) << 1;
			looplen = read24l(f) << 1;
			if (looplen > 2)
				xxs[i].flg |= WAVE_LOOPING;
			xxs[i].lpe = xxs[i].lps + looplen;
			xxih[i].nsm = 1;
			xxi[i][0].vol = read8(f);
			xxi[i][0].fin = (int8)(read8(f) << 4);
			xxi[i][0].sid = i;
			a = read8(f);
	
			if (a != 0) abort();
		}

		if (V(1) && (strlen((char*)xxih[i].name) || (xxs[i].len > 1))) {
			report("[%2X] %-32.32s %05x %05x %05x %c V%02x\n", i,
			       xxih[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe,
			       xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
			       xxi[i][0].vol);
		}

		if (~sn[i] & 0x80) {
			xmp_drv_loadpatch(f, xxi[i][0].sid, xmp_ctl->c4rate,
				XMP_SMP_VIDC, &xxs[xxi[i][0].sid], NULL);
		}
	}

	return 0;
}
