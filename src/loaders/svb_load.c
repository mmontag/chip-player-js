/* Silverball MASI PSM loader for xmp
 * Copyright (C) 2005 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: svb_load.c,v 1.5 2007-08-04 20:08:15 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "period.h"

/* FIXME: effects translation */

int svb_load (FILE * f)
{
	int c, r, i;
	struct xxm_event *event;
	uint8 buf[1024];
	uint32 p_ord, p_chn, p_pat, p_ins;
	uint32 p_smp[64];
 
	LOAD_INIT ();

	fread (buf, 1, 4, f);

	if (buf[0] != 'P' || buf[1] != 'S' || buf[2] != 'M' || buf[3] != 0xfe)
		return -1;

	fread(buf, 1, 60, f);
	strncpy(xmp_ctl->name, (char *)buf, 32);
	sprintf (xmp_ctl->type, "Silverball MASI (PSM)");

	read8(f);
	read8(f);
	read8(f);

	xxh->tpo = read8(f);
	xxh->bpm = read8(f);
	read8(f);
	read16l(f);
	xxh->len = read16l(f);
	xxh->pat = read16l(f);
	xxh->ins = read16l(f);
	xxh->chn = read16l(f);
	read16l(f);			/* channels used */
	xxh->smp = xxh->ins;
	xxh->trk = xxh->pat * xxh->chn;

	p_ord = read32l(f);
	p_chn = read32l(f);
	p_pat = read32l(f);
	p_ins = read32l(f);

	MODULE_INFO ();

	fseek(f, p_ord, SEEK_SET);
	fread(xxo, 1, xxh->len, f);

	fseek(f, p_chn, SEEK_SET);
	fread(buf, 1, 16, f);

	INSTRUMENT_INIT ();

	if (V(1)) report("     Sample name            Len  LBeg LEnd L Vol C2Spd\n");

	fseek(f, p_ins, SEEK_SET);
	for (i = 0; i < xxh->ins; i++) {
		uint16 flags, c2spd;

		xxi[i] = calloc (sizeof (struct xxm_instrument), 1);

		fread(buf, 1, 13, f);
		fread(buf, 1, 22, f);
		strncpy((char *)xxih[i].name, (char *)buf, 22);
		str_adj((char *)xxih[i].name);
		read16l(f);
		p_smp[i] = read32l(f);
		read32l(f);
		read8(f);
		flags = read16l(f);
		xxs[i].len = read32l(f); 
		xxs[i].lps = read32l(f);
		xxs[i].lpe = read32l(f);
		read8(f);
		xxi[i][0].vol = read8(f);
		c2spd = read16l(f);

		c2spd = 8363 * c2spd / 8448;

		xxi[i][0].pan = 0x80;
		xxi[i][0].sid = i;
		xxih[i].nsm = !!xxs[i].len;
		xxs[i].flg = xxs[i].lpe > 0 ? WAVE_LOOPING : 0;
		c2spd_to_note(c2spd, &xxi[i][0].xpo, &xxi[i][0].fin);

		if (V(1) && (strlen((char *)xxih[i].name) || (xxs[i].len > 1))) {
			report ("[%2X] %-22.22s %04x %04x %04x %c V%02x %5d\n",
				i, xxih[i].name, xxs[i].len, xxs[i].lps,
				xxs[i].lpe, xxs[i].flg & WAVE_LOOPING ?
				'L' : ' ', xxi[i][0].vol, c2spd);
		}
	}
	

	PATTERN_INIT ();

	if (V(0)) report ("Stored patterns: %d ", xxh->pat);

	fseek(f, p_pat, SEEK_SET);
	for (i = 0; i < xxh->pat; i++) {
		int len;
		uint8 b, rows, chan;

		len = read16l(f);
		rows = read8(f);
		chan = read8(f);
		len -= 4;

		PATTERN_ALLOC (i);
		xxp[i]->rows = rows;
		TRACK_ALLOC (i);

		for (r = 0; r < rows; r++) {
			while (len > 0) {
				b = read8(f);
				len--;

				if (b == 0)
					break;
	
				c = b & 0x0f;
	
				event = &EVENT(i, c, r);
	
				if (b & 0x80) {
					event->note = read8(f) + 24 + 1;
					event->ins = read8(f);
					len -= 2;
				}
	
				if (b & 0x40) {
					event->vol = read8(f) + 1;
					len--;
				}
	
				if (b & 0x20) {
					event->fxt = read8(f);
					event->fxp = read8(f);
					len -= 2;
/* printf("p%d r%d c%d: %02x %02x\n", i, r, c, event->fxt, event->fxp); */
				}
			}
		}

		if (len > 0)
			fseek(f, len, SEEK_CUR);

		if (V(0)) report(".");
	}
	if (V(0)) report("\n");


	/* Read samples */

	if (V(0)) report ("Stored samples : %d ", xxh->smp);

	for (i = 0; i < xxh->ins; i++) {
		fseek(f, p_smp[i], SEEK_SET);
		xmp_drv_loadpatch(f, xxi[i][0].sid, xmp_ctl->c4rate,
			XMP_SMP_DIFF, &xxs[xxi[i][0].sid], NULL);
		if (V(0)) report (".");
	}
	if (V(0)) report("\n");

	return 0;
}

