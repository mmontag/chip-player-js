/* Oktalyzer module loader for xmp
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: okt_load.c,v 1.6 2007-10-12 04:15:19 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Based on the format description written by Harald Zappe.
 * Additional information about Oktalyzer modules from Bernardo
 * Innocenti's XModule 3.4 sources.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "iff.h"


#define OKT_MODE8 0x00		/* 7 bit samples */
#define OKT_MODE4 0x01		/* 8 bit samples */
#define OKT_MODEB 0x02		/* Both */

#define NONE 0xff

static int mode[36];
static int idx[36];
static int pattern = 0;
static int sample = 0;


static int fx[] = {
    NONE,
    FX_PORTA_UP,	/*  1 */
    FX_PORTA_DN,	/*  2 */
    NONE,
    NONE,
    NONE,
    NONE,
    NONE,
    NONE,
    NONE,
    FX_OKT_ARP3,	/* 10 */
    FX_OKT_ARP4,	/* 11 */
    FX_OKT_ARP5,	/* 12 */
    FX_NSLIDE_DN,	/* 13 */
    NONE,
    NONE, /* Filter */	/* 15 */
    NONE,
    FX_NSLIDE_UP,	/* 17 */
    NONE,
    NONE,
    NONE,
    FX_F_NSLIDE_DN,	/* 21 */
    NONE,
    NONE,
    NONE,
    FX_JUMP,		/* 25 */
    NONE,
    NONE, /* Release */	/* 27 */
    FX_TEMPO,		/* 28 */
    NONE,
    FX_F_NSLIDE_UP,	/* 30 */
    FX_VOLSET		/* 31 */
};


static void get_cmod (int size, FILE *f)
{
    int i, j, k;

    xxh->chn = 0;
    for (i = 0; i < 4; i++) {
	j = read16b(f);
	for (k = !!j; k >= 0; k--) {
	    xxc[xxh->chn].pan = (((i + 1) / 2) % 2) * 0xff;
	    xxh->chn++;
	}
    }
}


static void get_samp(int size, FILE *f)
{
    int i, j;
    int looplen;

    /* Should be always 36 */
    xxh->ins = size / 32;  /* sizeof(struct okt_instrument_header); */
    xxh->smp = xxh->ins;

    INSTRUMENT_INIT ();

    reportv(1, "     Instrument name      Len   Lbeg  Lend  L Vol Mod\n");
    for (j = i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);

	fread(xxih[i].name, 1, 20, f);
	str_adj((char *)xxih[i].name);

	/* Sample size is always rounded down */
	xxs[i].len = read32b(f) & ~1;
	xxs[i].lps = read16b(f);
	looplen = read16b(f);
	xxs[i].lpe = xxs[i].lps + looplen;
	xxi[i][0].vol = read16b(f);
	mode[i] = read16b(f);

	xxih[i].nsm = !!(xxs[i].len);
	xxs[i].flg = looplen > 2 ? WAVE_LOOPING : 0;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = j;

	idx[j] = i;

	if ((V(1)) && (strlen ((char *) xxih[i].name) || (xxs[i].len > 1)))
	    report ("[%2X] %-20.20s %05x %05x %05x %c V%02x M%02x\n", i,
		xxih[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe, xxs[i].flg
		& WAVE_LOOPING ? 'L' : ' ', xxi[i][0].vol, mode[i]);
	if (xxih[i].nsm)
	    j++;
    }
}


static void get_spee(int size, FILE *f)
{
    xxh->tpo = read16b(f);
    xxh->bpm = 125;
}


static void get_slen(int size, FILE *f)
{
    xxh->pat = read16b(f);
    xxh->trk = xxh->pat * xxh->chn;
}


static void get_plen(int size, FILE *f)
{
    xxh->len = read16b(f);
    reportv(0, "Module length  : %d patterns\n", xxh->len);
}


static void get_patt (int size, FILE *f)
{
    fread(xxo, 1, xxh->len, f);
}


static void get_pbod (int size, FILE *f)
{
    int j;
    uint16 rows;
    struct xxm_event *event;

    if (pattern >= xxh->pat)
	return;

    if (!pattern) {
	PATTERN_INIT ();
	reportv(0, "Stored patterns: %d ", xxh->pat);
    }

    rows = read16b(f);

    PATTERN_ALLOC (pattern);
    xxp[pattern]->rows = rows;
    TRACK_ALLOC (pattern);

    for (j = 0; j < rows * xxh->chn; j++) {
	uint8 note, ins;

	event = &EVENT(pattern, j % xxh->chn, j / xxh->chn);
	memset(event, 0, sizeof(struct xxm_event));

	note = read8(f);
	ins = read8(f);

	if (note) {
	    event->note = 36 + note;
	    event->ins = 1 + ins;
	}

	event->fxt = fx[read8(f)];
	event->fxp = read8(f);

	if ((event->fxt == FX_VOLSET) && (event->fxp > 0x40)) {
	    if (event->fxp <= 0x50) {
		event->fxt = FX_VOLSLIDE;
		event->fxp -= 0x40;
	    } else if (event->fxp <= 0x60) {
		event->fxt = FX_VOLSLIDE;
		event->fxp = (event->fxp - 0x50) << 4;
	    } else if (event->fxp <= 0x70) {
		event->fxt = FX_EXTENDED;
		event->fxp = (EX_F_VSLIDE_DN << 4) | (event->fxp - 0x60);
	    } else if (event->fxp <= 0x80) {
		event->fxt = FX_EXTENDED;
		event->fxp = (EX_F_VSLIDE_UP << 4) | (event->fxp - 0x70);
	    }
	}
	if (event->fxt == FX_ARPEGGIO)	/* Arpeggio fixup */
	    event->fxp = (((24 - MSN (event->fxp)) % 12) << 4) | LSN (event->fxp);
	if (event->fxt == NONE)
	    event->fxt = event->fxp = 0;
    }
    reportv(0, ".");
    pattern++;
}


static void get_sbod (int size, FILE *f)
{
    int flags = 0;
    int i;

    if (sample >= xxh->ins)
	return;

    if (!sample && V(0))
	report ("\nStored samples : %d ", xxh->smp);

    i = idx[sample];
    if (mode[i] == OKT_MODE8 || mode[i] == OKT_MODEB)
	flags = XMP_SMP_7BIT;

    xmp_drv_loadpatch(f, sample, xmp_ctl->c4rate, flags, &xxs[i], NULL);

    reportv(0, ".");

    sample++;
}


int okt_load (FILE *f)
{
    char magic[8];

    LOAD_INIT ();

    /* Check magic */
    fread (magic, 1, 8, f);
    if (strncmp (magic, "OKTASONG", 8))
	return -1;

    pattern = sample = 0;

    /* IFF chunk IDs */
    iff_register ("CMOD", get_cmod);
    iff_register ("SAMP", get_samp);
    iff_register ("SPEE", get_spee);
    iff_register ("SLEN", get_slen);
    iff_register ("PLEN", get_plen);
    iff_register ("PATT", get_patt);
    iff_register ("PBOD", get_pbod);
    iff_register ("SBOD", get_sbod);

    strcpy (xmp_ctl->type, "OKT (Oktalyzer)");

    MODULE_INFO ();

    /* Load IFF chunks */
    while (!feof (f))
	iff_chunk (f);

    iff_release ();

    reportv(0, "\n");

    return 0;
}
