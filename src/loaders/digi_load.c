/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Based on the DIGI Booster player v1.6 by Tap (Tomasz Piasta), with the
 * help of Louise Heimann <louise.heimann@softhome.net>. The following
 * DIGI Booster effects are _NOT_ recognized by this player:
 *
 * 8xx robot
 * e00 filter off
 * e01 filter on
 * e30 backwd play sample
 * e31 backwd play sample+loop
 * e50 channel off
 * e51 channel on
 * e8x sample offset 2
 * e9x retrace
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"

struct digi_header {
    uint8 id[20];		/* ID: "DIGI Booster module\0" */
    uint8 vstr[4];		/* Version string: "Vx.y" */
    uint8 ver;			/* Version hi-nibble.lo-nibble */
    uint8 chn;			/* Number of channels */
    uint8 pack;			/* PackEnable */
    uint8 unknown[19];		/* ??!? */
    uint8 pat;			/* Number of patterns */
    uint8 len;			/* Song length */
    uint8 ord[128];		/* Orders */
    uint32 slen[31];		/* Sample length for 31 samples */
    uint32 sloop[31];		/* Sample loop start for 31 samples */
    uint32 sllen[31];		/* Sample loop length for 31 samples */
    uint8 vol[31];		/* Instrument volumes */
    uint8 fin[31];		/* Finetunes */
    uint8 title[32];		/* Song name */
    uint8 insname[31][30];	/* Instrument names */
} PACKED;


int digi_load (FILE * f)
{
    struct xxm_event *event = 0;
    struct digi_header dh;
    uint8 digi_event[4], chn_table[64];
    uint16 w;
    int i, j, k, c;

    LOAD_INIT ();

    fread (&dh, 1, sizeof (dh), f);

    if (strncmp ((char *) dh.id, "DIGI Booster module", 19))
	return -1;

    xxh->ins = 31;
    xxh->smp = xxh->ins;
    xxh->pat = dh.pat + 1;
    xxh->chn = dh.chn;
    xxh->trk = xxh->pat * xxh->chn;
    xxh->len = dh.len + 1;
    xxh->flg |= XXM_FLG_MODRNG;

    strncpy (xmp_ctl->name, dh.title, 32);
    sprintf (xmp_ctl->type, "DIGI Booster %-4.4s", dh.vstr);

    MODULE_INFO ();
 
    for (i = 0; i < xxh->len; i++)
	xxo[i] = dh.ord[i];
 
    INSTRUMENT_INIT ();

    /* Read and convert instruments and samples */

    if (V (1))
	report ("     Sample name                    Len  LBeg LEnd L Vol\n");

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	B_ENDIAN32 (dh.slen[i]);
	B_ENDIAN32 (dh.sloop[i]);
	B_ENDIAN32 (dh.sllen[i]);
	xxih[i].nsm = !!(xxs[i].len = dh.slen[i]);
	xxs[i].lps = dh.sloop[i];
	xxs[i].lpe = dh.sloop[i] + dh.sllen[i];
	xxs[i].flg = xxs[i].lpe > 0 ? WAVE_LOOPING : 0;
	xxi[i][0].vol = dh.vol[i];
	xxi[i][0].fin = dh.fin[i];
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	strncpy ((char *) xxih[i].name, dh.insname[i], 30);
	str_adj ((char *) xxih[i].name);
	if (V (1) &&
	    (strlen ((char *) xxih[i].name) || (xxs[i].len > 1))) {
	    report ("[%2X] %-30.30s %04x %04x %04x %c V%02x\n", i,
		xxih[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe, xxs[i].flg
		& WAVE_LOOPING ? 'L' : ' ', xxi[i][0].vol);
	}
    }

    PATTERN_INIT ();

    /* Read and convert patterns */
    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	TRACK_ALLOC (i);

	if (dh.pack) {
	    fread (&w, 2, 1, f);
	    B_ENDIAN16 (w);
	    w = (w - 64) >> 2;
	    fread (chn_table, 1, 64, f);
	} else {
	    w = 64 * xxh->chn;
	    memset (chn_table, 0xff, 64);
	}

	for (j = 0; j < 64; j++) {
	    for (c = 0, k = 0x80; c < xxh->chn; c++, k >>= 1) {
	        if (chn_table[j] & k) {
		    fread (digi_event, 4, 1, f);
		    event = &EVENT (i, c, j);
	            cvt_pt_event (event, digi_event);
		    switch (event->fxt) {
		    case 0x08:		/* Robot */
			event->fxt = event->fxp = 0;
			break;
		    case 0x0e:
			switch (MSN (event->fxp)) {
			case 0x00:
			case 0x03:
			case 0x08:
			case 0x09:
			    event->fxt = event->fxp = 0;
			    break;
			case 0x04:
			    event->fxt = 0x0c;
			    event->fxp = 0x00;
			    break;
			}
		    }
		    w--;
		}
	    }
	}

	if (w)
	    report ("WARNING! Corrupted file (w = %d)", w);

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

    /* xmp_ctl->fetch |= 0; */

    return 0;
}
