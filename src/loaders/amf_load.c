/* DSMI Advanced Module Format loader for xmp
 * Copyright (C) 2005 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: amf_load.c,v 1.1 2005-02-19 13:14:45 cmatsuoka Exp $
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

static uint8 fx[] =
{
};


int amf_load(FILE * f)
{
	int c, r, i, j;
	struct xxm_event *event;
	uint8 buf[1024];
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

	xxp = calloc(sizeof(struct xxm_pattern *), xxh->pat);
	xxh->trk = 0;

	for (i = 0; i < xxh->pat; i++) {
		PATTERN_ALLOC(i);
		xxp[i]->rows = ver >= 0x0e ? read16l(f) : 64;
		for (j = 0; j < xxh->chn; j++) {
			uint16 t = read16l(f);
			if (t > xxh->trk)
				xxh->trk = t;
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

		if (ver < 0x0a) {
			xxs[i].lps = read16l(f);
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

	if (V(0)) report("\nStored tracks  : %d ", xxh->trk);
	xxt = calloc (sizeof (struct xxm_track *), xxh->trk);

	for (i = 0; i < xxh->trk; i++) {
		xxt[w] = calloc(sizeof(struct xxm_track) +
			sizeof(struct xxm_event) * 64, 1);
		xxt[w]->rows = 64;


	}

	return 0;
}
