/* Extended Module Player
 * Copyright (C) 1996-2006 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: ptm_load.c,v 1.3 2006-02-13 02:55:59 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "ptm.h"
#include "period.h"


/* PTM volume table formula (approximated):
 *
 *   64 * (10 ** ((23362 + 598 * (64 + 20 * log10 (x / 64)) - 64E3) / 20E3))
 */

static int ptm_vol[] =
{
     0,  5,  8, 10, 12, 14, 15, 17, 18, 20, 21, 22, 23, 25, 26,
    27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 37, 38, 39, 40,
    41, 42, 42, 43, 44, 45, 46, 46, 47, 48, 49, 49, 50, 51, 51,
    52, 53, 54, 54, 55, 56, 56, 57, 58, 58, 59, 59, 60, 61, 61,
    62, 63, 63, 64, 64
};


int ptm_load (FILE * f)
{
    int c, r, i, smp_ofs[256];
    struct xxm_event *event;
    struct ptm_file_header pfh;
    struct ptm_instrument_header pih;
    uint8 n, b;

    LOAD_INIT ();

    /* Load and convert header */

    fread(&pfh.name, 28, 1, f);		/* Song name */
    pfh.doseof = read8(f);		/* 0x1a */
    pfh.vermin = read8(f);		/* Minor version */
    pfh.vermaj = read8(f);		/* Major type */
    pfh.rsvd1 = read8(f);		/* Reserved */
    pfh.ordnum = read16l(f);		/* Number of orders (must be even) */
    pfh.insnum = read16l(f);		/* Number of instruments */
    pfh.patnum = read16l(f);		/* Number of patterns */
    pfh.chnnum = read16l(f);		/* Number of channels */
    pfh.flags = read16l(f);		/* Flags (set to 0) */
    pfh.rsvd2 = read16l(f);		/* Reserved */
    fread(&pfh.magic, 4, 1, f);		/* 'PTMF' */
    fread(&pfh.rsvd3, 16, 1, f);	/* Reserved */
    fread(&pfh.chset, 32, 1, f);	/* Channel settings */
    fread(&pfh.order, 256, 1, f);	/* Orders */
    for (i = 0; i < 128; i++)
	pfh.patseg[i] = read16l(f);

    if (strncmp ((char *) pfh.magic, "PTMF", 4))
	return -1;
    strcpy (xmp_ctl->name, (char *) pfh.name);
    xxh->len = pfh.ordnum;
    xxh->ins = pfh.insnum;
    xxh->pat = pfh.patnum;
    xxh->chn = pfh.chnnum;
    xxh->trk = xxh->pat * xxh->chn;
    xxh->smp = xxh->ins;
    xxh->tpo = 6;
    xxh->bpm = 125;
    memcpy (xxo, pfh.order, 256);
    for (i = 0; i < pfh.patnum; i++)
	L_ENDIAN16 (pfh.patseg[i]);

    xmp_ctl->c4rate = C4_NTSC_RATE;

    strncpy(xmp_ctl->name, (char *)pfh.name, 28);
    sprintf(xmp_ctl->type, "PTMF %d.%02x (PTM)",
	pfh.vermaj, pfh.vermin);

    MODULE_INFO ();

    INSTRUMENT_INIT ();

    /* Read and convert instruments and samples */

    if (V (1))
	    report (
"     Instrument name              Len   LBeg  LEnd  L Vol C4Spd\n");

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);

	pih.type = read8(f);			/* Sample type */
	fread(&pih.dosname, 12, 1, f);		/* DOS file name */
	pih.vol = read8(f);			/* Volume */
	pih.c4spd = read16l(f);			/* C4 speed */
	pih.smpseg = read16l(f);		/* Sample segment (not used) */
	pih.smpofs = read32l(f);		/* Sample offset */
	pih.length = read32l(f);		/* Length */
	pih.loopbeg = read32l(f);		/* Loop begin */
	pih.loopend = read32l(f);		/* Loop end */
	pih.gusbeg = read32l(f);		/* GUS begin address */
	pih.guslps = read32l(f);		/* GUS loop start address */
	pih.guslpe = read32l(f);		/* GUS loop end address */
	pih.gusflg = read8(f);			/* GUS loop flags */
	pih.rsvd1 = read8(f);			/* Reserved */
	fread(&pih.name, 28, 1, f);		/* Instrument name */
	fread(&pih.magic, 4, 1, f);		/* 'PTMS' */

	if ((pih.type & 3) != 1)
	    continue;
#if 0
	if (strncmp (pih.magic, "PTMS", 4))
	    return -2;
#endif
	smp_ofs[i] = pih.smpofs;
	xxih[i].nsm = !!(xxs[i].len = pih.length);
	xxs[i].lps = pih.loopbeg;
	xxs[i].lpe = pih.loopend;
	xxs[i].flg = pih.type & 0x04 ? WAVE_LOOPING : 0;
	xxs[i].flg |= pih.type & 0x08 ? WAVE_LOOPING | WAVE_BIDIR_LOOP : 0;
	xxs[i].flg |= pih.type & 0x10 ? WAVE_16_BITS : 0;
	xxi[i][0].vol = pih.vol;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	pih.magic[0] = 0;

	copy_adjust(xxih[i].name, pih.name, 24);

	if ((V (1)) && (strlen ((char *) pih.name) || xxs[i].len))
	    report ("[%2X] %-28.28s %05x%c%05x %05x %c V%02x %5d\n",
		i, pih.name, xxs[i].len, pih.type & 0x10 ? '+' : ' ', xxs[i].lps,
		xxs[i].lpe, xxs[i].flg & WAVE_LOOPING ? 'L' : ' ', xxi[i][0].vol,
		pih.c4spd);

	/* Convert C4SPD to relnote/finetune */
	c2spd_to_note (pih.c4spd, &xxi[i][0].xpo, &xxi[i][0].fin);
    }

    PATTERN_INIT ();

    /* Read patterns */
    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);
    for (i = 0; i < xxh->pat; i++) {
	if (!pfh.patseg[i])
	    continue;
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	fseek (f, 16L * pfh.patseg[i], SEEK_SET);
	r = 0;
	while (r < 64) {
	    fread (&b, 1, 1, f);
	    if (!b) {
		r++;
		continue;
	    }
	    c = b & PTM_CH_MASK;
	    if (c >= xxh->chn)
		continue;
	    event = &EVENT (i, c, r);
	    if (b & PTM_NI_FOLLOW) {
		fread (&n, 1, 1, f);
		switch (n) {
		case 255:
		    n = 0;
		    break;	/* Empty note */
		case 254:
		    n = 0x61;
		    break;	/* Key off */
		}
		event->note = n;
		fread (&n, 1, 1, f);
		event->ins = n;
	    }
	    if (b & PTM_FX_FOLLOWS) {
		fread (&n, 1, 1, f);
		event->fxt = n;
		fread (&n, 1, 1, f);
		event->fxp = n;
		switch (event->fxt) {
		case 0x0e:	/* Pan set */
		    if (MSN (event->fxp) == 0x8) {
			event->fxt = FX_SETPAN;
			event->fxp = LSN (event->fxp) << 4;
		    }
		    break;
		case 0x11:	/* Multi retrig */
		    event->fxt = FX_MULTI_RETRIG;
		    break;
		case 0x12:	/* Fine vibrato */
		    event->fxt = FX_VIBRATO;
		    break;
		case 0x13:
		case 0x14:
		case 0x15:
		case 0x16:	/* Note slide */
		case 0x17:	/* Reverse sample */
		    event->fxt = event->fxp = 0;
		    break;
		}
		if (event->fxt > 0x16)
		    event->fxt = event->fxp = 0;
	    }
	    if (b & PTM_VOL_FOLLOWS) {
		fread (&n, 1, 1, f);
		event->vol = n + 1;
	    }
	}
	if (V (0))
	    report (".");
    }

    if (V (0))
	report ("\nStored samples : %d ", xxh->smp);
    for (i = 0; i < xxh->smp; i++) {
	if (!xxs[i].len)
	    continue;
	fseek (f, smp_ofs[xxi[i][0].sid], SEEK_SET);
	/*xxs[xxi[i][0].sid].len--; */
	xmp_drv_loadpatch (f, xxi[i][0].sid, xmp_ctl->c4rate,
	    XMP_SMP_8BDIFF, &xxs[xxi[i][0].sid], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    xmp_ctl->vol_xlat = (int *) &ptm_vol;

    for (i = 0; i < xxh->chn; i++)
	xxc[i].pan = pfh.chset[i] << 4;

    return 0;
}
