/* Silverball MASI PSM loader for xmp
 * Copyright (C) 2005 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: svb_load.c,v 1.1 2005-02-21 12:28:34 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"


int svb_load (FILE * f)
{
	int c, r, i;
	struct xxm_event *event;
	uint8 buf[1024];
	uint32 p_ord, p_chn, p_pat, p_smp;
 
	LOAD_INIT ();

	fread (buf, 1, 4, f);

	if (buf[0] != 'P' || buf[1] != 'S' || buf[2] != 'M' || buf[3] != 0xfe)
		return -1;

	fread(buf, 1, 60, f);
	strncpy(xmp_ctl->name, buf, 32);
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
	p_smp = read32l(f);

	MODULE_INFO ();

#if 0
 
    /* Read orders */
    for (i = 0; i < xxh->len; i++) {
	fread (&xxo[i], 1, 1, f);
	fseek (f, 4, SEEK_CUR);
    }
 
    INSTRUMENT_INIT ();

    /* Read and convert instruments and samples */

    if (V (1))
	report ("     Sample name    Len  LBeg LEnd L Vol C2Spd\n");

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	fseek (f, pp_ins[i] << 4, SEEK_SET);
	fread (&sih, 1, sizeof (struct svb_instrument_header), f);
	L_ENDIAN32 (sih.length);
	L_ENDIAN32 (sih.loopbeg);
	L_ENDIAN32 (sih.loopend);
	L_ENDIAN16 (sih.c2spd);
	xxih[i].nsm = !!(xxs[i].len = sih.length);
	xxs[i].lps = sih.loopbeg;
	xxs[i].lpe = sih.loopend;
	if (xxs[i].lpe == 0xffff)
	    xxs[i].lpe = 0;
	xxs[i].flg = xxs[i].lpe > 0 ? WAVE_LOOPING : 0;
	xxi[i][0].vol = sih.vol;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	strncpy ((char *) xxih[i].name, sih.name, 12);
	str_adj ((char *) xxih[i].name);
	if (V (1) &&
	    (strlen ((char *) xxih[i].name) || (xxs[i].len > 1))) {
	    report ("[%2X] %-14.14s %04x %04x %04x %c V%02x %5d\n", i,
		xxih[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe, xxs[i].flg
		& WAVE_LOOPING ? 'L' : ' ', xxi[i][0].vol, sih.c2spd);
	}

	sih.c2spd = 8363 * sih.c2spd / 8448;
	c2spd_to_note (sih.c2spd, &xxi[i][0].xpo, &xxi[i][0].fin);
    }

    PATTERN_INIT ();

    /* Read and convert patterns */
    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	TRACK_ALLOC (i);

	if (!pp_pat[i])
	    continue;

	fseek (f, pp_pat[i] * 16, SEEK_SET);
	if (broken)
	    fseek (f, 2, SEEK_CUR);

	for (r = 0; r < 64; ) {
	    fread (&b, 1, 1, f);

	    if (b == S3M_EOR) {
		r++;
		continue;
	    }

	    c = b & S3M_CH_MASK;
	    event = c >= xxh->chn ? &dummy : &EVENT (i, c, r);

	    if (b & S3M_NI_FOLLOW) {
		fread (&n, 1, 1, f);

		switch (n) {
		case 255:
		    n = 0;
		    break;	/* Empty note */
		case 254:
		    n = 0x61;
		    break;	/* Key off */
		default:
		    n = 25 + 12 * MSN (n) + LSN (n);
		}

		event->note = n;
		fread (&n, 1, 1, f);
		event->ins = n;
	    }

	    if (b & S3M_VOL_FOLLOWS) {
		fread (&n, 1, 1, f);
		event->vol = n + 1;
	    }

	    if (b & S3M_FX_FOLLOWS) {
		fread (&n, 1, 1, f);
		event->fxt = fx[n];
		fread (&n, 1, 1, f);
		event->fxp = n;
		switch (event->fxt) {
		case FX_TEMPO:
		    event->fxp = MSN (event->fxp);
		    break;
		case FX_NONE:
		    event->fxp = event->fxt = 0;
		    break;
		}
	    }
	}

	if (V (0))
	    report (".");
    }

    if (V (0))
	report ("\n");

    /* Read samples */
    if (V (0))
	report ("Stored samples : %d ", xxh->smp);
    for (i = 0; i < xxh->ins; i++) {
	xmp_drv_loadpatch (f, xxi[i][0].sid, xmp_ctl->c4rate, 0,
	    &xxs[xxi[i][0].sid], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    xmp_ctl->fetch |= XMP_CTL_VSALL | XMP_MODE_ST3;
#endif

    return 0;
}

