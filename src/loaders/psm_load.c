/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
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



static uint8 fx[] =
{
};


int psm_load (FILE *f)
{
	struct xxm_event *event;
	uint8 c, b, n, buf[256];
	uint8 songver, patver, flags;
	int ord_ofs, pan_ofs, pat_ofs, smp_ofs, com_ofs, *sdt_ofs;
	int patsize;
	int offset;
	int i;

	LOAD_INIT ();

	offset = ftell(f);

	fread(buf, 1, 4, f);
	if (buf[0] != 'P' || buf[1] != 'S' || buf[2] != 'M' || buf[3] != 0xfe)
		return -1;

	fread(buf, 1, 60, f);

	flags = read8(f);

	if (flags & 1) return -1;		/* Song not supported */

	strncpy (xmp_ctl->name, buf, 59);
	sprintf (xmp_ctl->type, "Protracker Studio Module (PSM)");
	/* sprintf (tracker_name, ""); */

	if (~flags & 2)
		xxh->flg |= XXM_FLG_MODRNG;

	songver = read8(f);			/* song version */
	patver = read8(f);			/* pattern version */
	xxh->tpo = read8(f);
	xxh->bpm = read8(f);
	xxh->gvl = read8(f) << 2;
	read16l(f);				/* length */
	xxh->len = read16l(f);			/* number of orders */
	xxh->pat = read16l(f);
	xxh->ins = read16l(f);
	xxh->chn = read16l(f);
	read16l(f);				/* channels to process */

	xxh->trk = xxh->pat * xxh->chn;
	xxh->smp = xxh->ins;

	/* xmp_ctl->c4rate = C4_NTSC_RATE; */

	MODULE_INFO ();

	ord_ofs = read32l(f);
	pan_ofs = read32l(f);
	pat_ofs = read32l(f);
	smp_ofs = read32l(f);
	com_ofs = read32l(f);
	patsize = read32l(f);

	sdt_ofs = malloc(xxh->ins * sizeof(int));
	
	/* Read orders */

	fseek(f, offset + ord_ofs, SEEK_SET);
	fread (&xxo[i], 1, xxh->len, f);
	

	/* Read patterns */

	fseek(f, offset + pat_ofs, SEEK_SET);

	PATTERN_INIT();

	if (V(0))
		report ("Stored patterns: %d ", xxh->pat);

	for (i = 0; i < xxh->pat; i++) {
		int r, x;

		PATTERN_ALLOC (i);

		x = read16l(f);
		read8(f);		/* number of rows */
		read8(f);		/* number of channels */

		xxp[i]->rows = 64;
		TRACK_ALLOC (i);

		for (r = 0; r < 64; r++) {
			b = read8(f);

			if (b == 0)		/* end of row */
				continue;

			c = b & 0x1f;		/* channel */

			if (b & 0x80) {
				n = read8(f);
				event->note = n;
				n = read8(f);
				event->ins = n;
			}

			if (b & 0x40) {
				n = read8(f);
				event->vol = n;		/* + 1 */
			}

			if (b & 0x20) {
				n = read8(f);
				event->fxt = fx[n];
				n = read8(f);
				event->fxp = n;
			}
		}

		if (V (0))
			report (".");

	}

	/* Load instruments */
 
	fseek(f, offset + pat_ofs, SEEK_SET);

	INSTRUMENT_INIT();

	if (V(1))
		report ("     Sample name    Len  LBeg LEnd L Vol C2Spd\n");

	for (i = 0; i < xxh->ins; i++) {
		int c2spd;

		xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
		fread (buf, 1, 37, f);		/* name + description */
		sdt_ofs[i] = read32l(f);
		read32l(f);			/* memory offset */
		read16l(f);			/* sample number */
		flags = read8(f);
		xxs[i].len = read32l(f);
		xxih[i].nsm = !!xxs[i].len;
		xxs[i].lps = read32l(f);
		xxs[i].lpe = read32l(f);
		xxs[i].flg = xxs[i].lpe > 0 ? WAVE_LOOPING : 0;
		xxi[i][0].fin = read8(f);
		xxi[i][0].vol = read8(f);
		xxi[i][0].pan = 0x80;
		xxi[i][0].sid = i;

		c2spd = read16l(f);
		c2spd_to_note(c2spd, &xxi[i][0].xpo, &xxi[i][0].fin);

		strncpy((char *)xxih[i].name, buf, 12);
		str_adj((char *)xxih[i].name);
		if (V(1) && (strlen((char *)xxih[i].name)||(xxs[i].len > 1))) {
			report ("[%2X] %-14.14s %04x %04x %04x %c V%02x %5d\n",
			i, xxih[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe,
			xxs[i].flg & WAVE_LOOPING ? 'L' : ' ', xxi[i][0].vol,
			c2spd);
		}
	}

	if (V(0))
		report ("\n");

	/* Read samples */
	if (V (0))
		report ("Stored samples : %d ", xxh->smp);

	for (i = 0; i < xxh->ins; i++) {
		fseek(f, offset + sdt_ofs[i], SEEK_SET);
		xmp_drv_loadpatch(f, xxi[i][0].sid, xmp_ctl->c4rate,
			XMP_SMP_DIFF, &xxs[xxi[i][0].sid], NULL);

		if (V (0))
			report (".");
	}

	if (V (0))
		report ("\n");

	xmp_ctl->fetch |= XMP_CTL_VSALL | XMP_MODE_ST3;

	return 0;
}
