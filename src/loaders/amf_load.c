/* DSMI Advanced Module Format loader for xmp
 * Copyright (C) 2005 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: amf_load.c,v 1.5 2005-02-20 17:46:52 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Based on the format specification by Miodrag Vallat.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "period.h"


int amf_load(FILE * f)
{
	int i, j;
	struct xxm_event *event;
	uint8 buf[1024];
	int *trkmap, newtrk;
	int ver;

	LOAD_INIT();

	fread(buf, 1, 3, f);
	if (buf[0] != 'A' || buf[1] != 'M' || buf[2] != 'F')
		return -1;

	ver = read8(f);
	if (ver < 0x0a || ver > 0x0e)
		return -1;

	fread(buf, 1, 32, f);
	strncpy(xmp_ctl->name, buf, 32);
	sprintf(xmp_ctl->type, "DSMI (DMP) %d.%d", ver / 10, ver % 10);

	xxh->ins = read8(f);
	xxh->len = read8(f);
	xxh->trk = read16l(f);
	xxh->chn = read8(f);

	xxh->smp = xxh->ins;
	xxh->pat = xxh->len;

	if (ver == 0x0a)
		fread(buf, 1, 16, f);		/* channel remap table */

	if (ver >= 0x0d) {
		fread(buf, 1, 32, f);		/* panning table */
		for (i = 0; i < 32; i++) {
			xxc->pan = 0x80 + 2 * (int8)buf[i];
		}
		xxh->bpm = read8(f);
		xxh->tpo = read8(f);
	} else if (ver >= 0x0b) {
		fread(buf, 1, 16, f);
	}

	MODULE_INFO ();
 

	/* Orders */

	for (i = 0; i < xxh->len; i++)
		xxo[i] = i;

	if (V(0))
		report ("Stored patterns: %d ", xxh->pat);

	xxp = calloc(sizeof(struct xxm_pattern *), xxh->pat + 1);

	for (i = 0; i < xxh->pat; i++) {
		PATTERN_ALLOC(i);
		xxp[i]->rows = ver >= 0x0e ? read16l(f) : 64;
		for (j = 0; j < xxh->chn; j++) {
			uint16 t = read16l(f);
			xxp[i]->info[j].index = t;
		}
		if (V(0)) report (".");
	}
	printf("\n");


	/* Instruments */

	INSTRUMENT_INIT();

	if (V(1))
		report("     Sample name                      Len   LBeg  LEnd  L Vol C2Spd\n");

	for (i = 0; i < xxh->ins; i++) {
		uint8 b;
		int c2spd;

		xxi[i] = calloc(sizeof(struct xxm_instrument), 1);

		b = read8(f);
		xxih[i].nsm = b ? 1 : 0;

		fread(buf, 1, 32, f);
		strncpy(xxih[i].name, buf, 32);
		str_adj((char *) xxih[i].name);

		fread(buf, 1, 13, f);	/* sample name */
		read32l(f);		/* sample index */

		xxi[i][0].sid = i;
		xxi[i][0].pan = 0x80;
		xxs[i].len = read32l(f);
		c2spd = read16l(f);
		c2spd_to_note(c2spd, &xxi[i][0].xpo, &xxi[i][0].fin);
		xxi[i][0].vol = read8(f);

		if (ver <= 0x0a) {
			xxs[i].lps = read16l(f);
			xxs[i].lpe = xxs[i].len - 1;
		} else {
			xxs[i].lps = read32l(f);
			xxs[i].lpe = read32l(f);
		}
		xxs[i].flg = xxs[i].lps > 0 ? WAVE_LOOPING : 0;

		if (V(1) && (strlen((char *)xxih[i].name) || xxs[i].len)) {
			report ("[%2X] %-32.32s %05x %05x %05x %c V%02x %5d\n",
				i, xxih[i].name, xxs[i].len, xxs[i].lps,
				xxs[i].lpe, xxs[i].flg & WAVE_LOOPING ?
				'L' : ' ', xxi[i][0].vol, c2spd);
		}
	}
				

	/* Tracks */

	trkmap = calloc(sizeof(int), xxh->trk);
	newtrk = 0;

	for (i = 0; i < xxh->trk; i++) {		/* read track table */
		uint16 t;
		t = read16l(f);
		trkmap[i] = t - 1;
		if (t > newtrk) newtrk = t;
/*printf("%d -> %d\n", i, t);*/
	}

	for (i = 0; i < xxh->pat; i++) {		/* read track table */
		for (j = 0; j < xxh->chn; j++) {
			int k = xxp[i]->info[j].index - 1;
			xxp[i]->info[j].index = trkmap[k];
		}
	}

	xxh->trk = newtrk;
	free(trkmap);

	if (V(0)) report("Stored tracks  : %d ", xxh->trk);
	xxt = calloc (sizeof (struct xxm_track *), xxh->trk);

	for (i = 0; i < xxh->trk; i++) {
		uint8 t1, t2, t3;
		int size;

		xxt[i] = calloc(sizeof(struct xxm_track) +
			sizeof(struct xxm_event) * 64 - 1, 1);
		xxt[i]->rows = 64;

		size = read24l(f);
/*printf("TRACK %d SIZE %d\n", i, size);*/

		for (j = 0; j < size; j++) {
			t1 = read8(f);			/* row */
			t2 = read8(f);			/* type */
			t3 = read8(f);			/* parameter */
/*printf("track %d row %d: %02x %02x %02x\n", i, t1, t1, t2, t3);*/

			if (t1 == 0xff && t2 == 0xff && t3 == 0xff)
				break;

			event = &xxt[i]->event[t1];

			if (t2 < 0x7f) {		/* note */
				if (t2 > 12)
					event->note = t2 + 1 - 12;
			} else if (t2 == 0x7f) {	/* copy previous */
				memcpy(event, &xxt[i]->event[t1 - 1],
					sizeof(struct xxm_event));
			} else if (t2 == 0x80) {	/* instrument */
				event->ins = t3 + 1;
			} else  {			/* effects */
				uint8 fxp, fxt;

				fxp = fxt = 0;

				switch (t2) {
				case 0x81:
					fxt = FX_TEMPO;
					fxp = t3;
					break;
				case 0x82:
					if ((int8)t3 > 0) {
						fxt = FX_VOLSLIDE;
						fxp = t3 << 4;
					} else {
						fxt = FX_VOLSLIDE;
						fxp = -(int8)t3 & 0x0f;
					}
					break;
				case 0x83:
					/* set track vol -- later */
					break;
				case 0x84:
					if ((int8)t3 > 0) {
						fxt = FX_PORTA_DN;
						fxp = t3;
					} else {
						fxt = FX_PORTA_UP;
						fxp = -(int8)t3;
					}
					break;
				case 0x85:
					/* porta abs -- unknown */
					break;
				case 0x86:
					fxt = FX_TONEPORTA;
					fxp = t3;
					break;
				case 0x87:
					fxt = FX_TREMOR;
					fxp = t3;
					break;
				case 0x88:
					fxt = FX_ARPEGGIO;
					fxp = t3;
					break;
				case 0x89:
					fxt = FX_VIBRATO;
					fxp = t3;
					break;
				case 0x8a:
					if ((int8)t3 > 0) {
						fxt = FX_TONE_VSLIDE;
						fxp = t3 << 4;
					} else {
						fxt = FX_TONE_VSLIDE;
						fxp = -(int8)t3 & 0x0f;
					}
					break;
				case 0x8b:
					if ((int8)t3 > 0) {
						fxt = FX_VIBRA_VSLIDE;
						fxp = t3 << 4;
					} else {
						fxt = FX_VIBRA_VSLIDE;
						fxp = -(int8)t3 & 0x0f;
					}
					break;
				case 0x8c:
					fxt = FX_BREAK;
					fxp = t3;
					break;
				case 0x8d:
					fxt = FX_JUMP;
					fxp = (t3 >> 4) * 10 + (t3 & 0x0f);
					break;
				case 0x8e:
					/* sync -- unknown */
					break;
				case 0x8f:
					fxt = FX_EXTENDED;
					fxp = (EX_RETRIG << 4) | (t3 & 0x0f);
					break;
				case 0x90:
					fxt = FX_OFFSET;
					fxp = t3;
					break;
				case 0x91:
					if ((int8)t3 > 0) {
						fxt = FX_EXTENDED;
						fxp = (EX_F_VSLIDE_UP << 4) |
							(t3 & 0x0f);
					} else {
						fxt = FX_EXTENDED;
						fxp = (EX_F_VSLIDE_DN << 4) |
							(t3 & 0x0f);
					}
					break;
				case 0x92:
					if ((int8)t3 > 0) {
						fxt = FX_PORTA_DN;
						fxp = 0xf0 | (fxp & 0x0f);
					} else {
						fxt = FX_PORTA_UP;
						fxp = 0xf0 | (fxp & 0x0f);
					}
					break;
				case 0x93:
					fxt = FX_EXTENDED;
					fxp = (EX_DELAY << 4) | (t3 & 0x0f);
					break;
				case 0x94:
					fxt = FX_EXTENDED;
					fxp = (EX_CUT << 4) | (t3 & 0x0f);
					break;
				case 0x95:
					fxt = FX_TEMPO;
					if (t3 < 0x21)
						t3 = 0x21;
					fxp = t3;
					break;
				case 0x96:
					if ((int8)t3 > 0) {
						fxt = FX_PORTA_DN;
						fxp = 0xe0 | (fxp & 0x0f);
					} else {
						fxt = FX_PORTA_UP;
						fxp = 0xe0 | (fxp & 0x0f);
					}
					break;
				case 0x97:
					fxt = FX_SETPAN;
					fxp = 0x80 + 2 * (int8)t3;
					break;
				}

				event->fxt = fxt;
				event->fxp = fxp;
			}

		}
		if (V(0) & !(i % 4)) report(".");
	}
	if (V(0)) report("\n");


	/* Samples */

	if (V(0)) report ("Stored samples : %d ", xxh->smp);

	for (i = 0; i < xxh->ins; i++) {
		xmp_drv_loadpatch (f, xxi[i][0].sid, xmp_ctl->c4rate,
			XMP_SMP_UNS, &xxs[xxi[i][0].sid], NULL);
		if (V(0)) report (".");
	}
	if (V(0)) report ("\n");

	xmp_ctl->fetch |= XMP_CTL_FINEFX;

	return 0;
}
