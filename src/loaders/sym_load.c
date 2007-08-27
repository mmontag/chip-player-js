/* Digital Symphony module loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: sym_load.c,v 1.17 2007-08-27 18:41:58 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include "load.h"
#include "readlzw.h"


static void fix_effect(struct xxm_event *e, int parm)
{
	switch (e->fxt) {
	case 0x00:	/* 00 xyz Normal play or Arpeggio + Volume Slide Up */
	case 0x01:	/* 01 xyy Slide Up + Volume Slide Up */
	case 0x02:	/* 01 xyy Slide Up + Volume Slide Up */
		e->fxp = parm & 0xff;
		e->f2t = FX_VOLSLIDE_UP;
		e->f2p = parm >> 8;
		break;
	case 0x03:	/* 03 xyy Tone Portamento */
	case 0x04:	/* 04 xyz Vibrato */
	case 0x05:	/* 05 xyz Tone Portamento + Volume Slide */
	case 0x06:	/* 06 xyz Vibrato + Volume Slide */
	case 0x07:	/* 07 xyz Tremolo */
		e->fxp = parm;
		break;
	case 0x09:	/* 09 xxx Set Sample Offset */
		e->fxp = parm >> 1;
		break;
	case 0x0a:	/* 0A xyz Volume Slide + Fine Slide Up */
		e->fxp = parm & 0xff;
		e->f2t = FX_EXTENDED;
		e->f2p = (EX_F_PORTA_UP << 4) | ((parm & 0xf00) >> 8);
		break;
	case 0x0b:	/* 0B xxx Position Jump */
	case 0x0c:	/* 0C xyy Set Volume */
	case 0x0d:	/* 0D xyy Pattern Break */
	case 0x0f:	/* 0F xxx Set Speed */
		e->fxp = parm;
		break;
	case 0x11:	/* 11 xyy Fine Slide Up + Fine Volume Slide Up */
	case 0x12:	/* 12 xyy Fine Slide Down + Fine Volume Slide Up */
		e->fxt = 0;
		break;
	case 0x13:	/* 13 xxy Glissando Control */
		e->fxt = FX_EXTENDED;
		e->fxp = (EX_GLISS << 4) | (parm & 0x0f);
		break;
	case 0x14:	/* 14 xxy Set Vibrato Waveform */
		e->fxt = FX_EXTENDED;
		e->fxp = (EX_VIBRATO_WF << 4) | (parm & 0x0f);
		break;
	case 0x15:	/* 15 xxy Set Fine Tune */
		e->fxt = FX_EXTENDED;
		e->fxp = (EX_FINETUNE << 4) | (parm & 0x0f);
		break;
	case 0x16:	/* 16 xxx Jump to Loop */
		e->fxt = FX_EXTENDED;
		e->fxp = (EX_PATTERN_LOOP << 4) | (parm & 0x0f);
		break;
	case 0x17:	/* 17 xxy Set Tremolo Waveform */
		e->fxt = FX_EXTENDED;
		e->fxp = (EX_TREMOLO_WF << 4) | (parm & 0x0f);
		break;
	case 0x19:	/* 19 xxx Retrig Note */
		e->fxt = FX_EXTENDED;
		e->fxp = (EX_RETRIG << 4) | (parm & 0x0f);
		break;
	case 0x1a:	/* 1A xyy Fine Slide Up + Fine Volume Slide Down */
		e->fxt = FX_EXTENDED;
		e->fxp = (EX_F_PORTA_UP << 4) | (parm & 0x0f);
		break;
	case 0x1b:	/* 1B xyy Fine Slide Down + Fine Volume Slide Down */
		e->fxt = FX_EXTENDED;
		e->fxp = (EX_F_PORTA_DN << 4) | (parm & 0x0f);
		break;
	case 0x1c:	/* 1C xxx Note Cut */
		e->fxt = FX_EXTENDED;
		e->fxp = (EX_CUT << 4) | (parm & 0x0f);
		break;
	case 0x1d:	/* 1D xxx Note Delay */
		e->fxt = FX_EXTENDED;
		e->fxp = (EX_DELAY << 4) | (parm & 0x0f);
		break;
	case 0x1e:	/* 1E xxx Pattern Delay */
		e->fxt = FX_EXTENDED;
		e->fxp = (EX_PATT_DELAY << 4) | (parm & 0x0f);
		break;
	case 0x1f:	/* 1F xxy Invert Loop */
		e->fxt = 0;
		break;
	case 0x20:	/* 20 xyz Normal play or Arpeggio + Volume Slide Down */
	case 0x21:	/* 21 xyy Slide Up + Volume Slide Down */
	case 0x22:	/* 22 xyy Slide Down + Volume Slide Down */
		e->fxp = parm & 0xff;
		e->f2t = FX_VOLSLIDE_DN;
		e->f2p = parm >> 8;
		break;
	case 0x2a:	/* 2A xyz Volume Slide + Fine Slide Down */
	case 0x2b:	/* 2B xyy Line Jump */
	case 0x2f:	/* 2F xxx Set Tempo */
	case 0x30:	/* 30 xxy Set Stereo */
	case 0x31:	/* 31 xxx Song Upcall */
	case 0x32:	/* 32 xxx Unset Sample Repeat */
	default:
		e->fxt = 0;
	}
}

static uint32 readptr32l(uint8 *p)
{
	uint32 a, b, c, d;

	a = *p++;
	b = *p++;
	c = *p++;
	d = *p++;

	return (d << 24) | (c << 16) | (b << 8) | a;
}

static uint32 readptr16l(uint8 *p)
{
	uint32 a, b;

	a = *p++;
	b = *p++;

	return (b << 8) | a;
}

int sym_load(FILE * f)
{
	struct xxm_event *event;
	int i, j;
	uint32 a, b;
	int ver, infolen, sn[64];
	uint8 *buf;
	int size;

	LOAD_INIT();

	a = read32b(f);
	b = read32b(f);

	if (a != 0x02011313 || b != 0x1412010B)		/* BASSTRAK */
		return -1;

	ver = read8(f);

	sprintf(xmp_ctl->type, "BASSTRAK v%d (Digital Symphony)", ver);

	xxh->chn = read8(f);
	xxh->len = xxh->pat = read16l(f);
	xxh->trk = read16l(f);	/* Symphony patterns are actually tracks */
	infolen = read24l(f);

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

	/* PATTERN_INIT - alloc extra track*/
	xxt = calloc(sizeof (struct xxm_track *), xxh->trk + 1);
	xxp = calloc(sizeof (struct xxm_pattern *), xxh->pat + 1);

	/* Sequence */
	a = read8(f);			/* packing */
	assert(a == 0 || a == 1);
	reportv(0, "Packed sequence: %s\n", a ? "yes" : "no");

	size = xxh->len * xxh->chn * 2;
	buf = malloc(size);

	if (a) {
		read_lzw_dynamic(f, buf, 13, 0, size, size, XMP_LZW_QUIRK_DSYM);
	} else {
		fread(buf, 1, size, f);
	}

	for (i = 0; i < xxh->len; i++) {
		PATTERN_ALLOC(i);
		xxp[i]->rows = 64;
		for (j = 0; j < xxh->chn; j++) {
			int idx = 2 * (i * xxh->chn + j);
			xxp[i]->info[j].index = readptr16l(&buf[idx]);

			if (xxp[i]->info[j].index == 0x1000) /* empty track */
				xxp[i]->info[j].index = xxh->trk;

			//printf("%04x ", xxp[i]->info[j].index);
		}
		xxo[i] = i;
		//printf("\n");
	}
	free(buf);

	/* Read and convert patterns */

	a = read8(f);
	assert(a == 0 || a == 1);
	//reportv(0, "Packed tracks  : %s\n", a ? "yes" : "no");
	reportv(0, "Stored tracks  : %d ", xxh->trk);

	size = 64 * xxh->trk * 4;
	buf = malloc(size);

	if (a) {
		read_lzw_dynamic(f, buf, 13, 0, size, size, XMP_LZW_QUIRK_DSYM);
	} else {
		fread(buf, 1, size, f);
	}

	for (i = 0; i < xxh->trk; i++) {
		xxt[i] = calloc(sizeof(struct xxm_track) +
				sizeof(struct xxm_event) * 64 - 1, 1);
		xxt[i]->rows = 64;

		for (j = 0; j < xxt[i]->rows; j++) {
			int parm;

			event = &xxt[i]->event[j];

			b = readptr32l(&buf[4 * (i * 64 + j)]);
			event->note = b & 0x0000003f;
			if (event->note)
				event->note += 36;
			event->ins = (b & 0x00001fc0) >> 6;
			event->fxt = (b & 0x000fc000) >> 14;
			parm = (b & 0xfff00000) >> 20;

			fix_effect(event, parm);
		}
		if (V(0)) {
			if (i % xxh->chn == 0)
				report(a ? "c" : ".");
		}
	}
	reportv(0, "\n");

	free(buf);

	/* Extra track */
	xxt[i] = calloc(sizeof(struct xxm_track) +
				sizeof(struct xxm_event) * 64 - 1, 1);
	xxt[i]->rows = 64;

	/* Load and convert instruments */

	reportv(0, "Instruments    : %d ", xxh->ins);
	reportv(1, "\n     Instrument Name                  Len   LBeg  LEnd  L Vol");

	for (i = 0; i < xxh->ins; i++) {
		uint8 buf[128];

		memset(buf, 0, 128);
//printf("offset = %x\n", ftell(f));
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
		}

		if (V(1) && (strlen((char*)xxih[i].name) || (xxs[i].len > 1))) {
			report("\n[%2X] %-32.32s %05x %05x %05x %c V%02x ", i,
			       xxih[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe,
			       xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
			       xxi[i][0].vol);
		}

		if (sn[i] & 0x80)
			continue;

		a = read8(f);
		assert(a == 0 || a == 1);

		if (a) {
			uint8 *b = malloc(xxs[i].len);
			read_lzw_dynamic(f, b, 13, 0, xxs[i].len, xxs[i].len,
							XMP_LZW_QUIRK_DSYM);
			xmp_drv_loadpatch(NULL, xxi[i][0].sid, xmp_ctl->c4rate,
				XMP_SMP_NOLOAD, &xxs[xxi[i][0].sid], (char*)b);
			free(b);
			reportv(0, "c");
		} else {
			xmp_drv_loadpatch(f, xxi[i][0].sid, xmp_ctl->c4rate,
				XMP_SMP_VIDC, &xxs[xxi[i][0].sid], NULL);
			reportv(0, ".");
		}
	}
	reportv(0, "\n");

	return 0;
}
