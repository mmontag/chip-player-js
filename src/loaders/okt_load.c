/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
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

struct okt_instrument_header {
    uint8 name[20];		/* Intrument name */
    uint32 length;		/* Sample length */
    uint16 loop_start;		/* Loop start */
    uint16 looplen;		/* Loop length */
    uint16 volume;		/* Volume */
    uint16 mode;		/* 0x00=7bit, 0x01=8bit, 0x02=both */
} PACKED;

struct okt_event {
    uint8 note;			/* Note (1-36) */
    uint8 ins;			/* Intrument number */
    uint8 fxt;			/* Effect type */
    uint8 fxp;			/* Effect parameter */
} PACKED;


#define NONE 0xff

static int mode[36];
static int idx[36];
static int pattern = 0;
static int sample = 0;


static int fx[] =
{
    NONE, FX_PORTA_UP, FX_PORTA_DN, NONE, NONE, NONE, NONE, NONE,
    NONE, NONE, FX_ARPEGGIO, FX_ARPEGGIO, FX_ARPEGGIO, NONE, NONE,
    NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE,
    FX_JUMP, NONE, NONE, FX_TEMPO, NONE, NONE, FX_VOLSET
};


static void get_cmod (int size, uint16 * buffer)
{
    int i, k;

    xxh->chn = 0;
    for (i = 0; i < 4; i++) {
	B_ENDIAN16 (buffer[i]);
	for (k = !!buffer[i]; k >= 0; k--) {
	    xxc[xxh->chn].pan = (((i + 1) / 2) % 2) * 0xff;
	    xxh->chn++;
	}
    }
}


static void get_samp (int size, struct okt_instrument_header *buffer)
{
    int i, j;

    /* Should be always 36 */
    xxh->ins = size / sizeof (struct okt_instrument_header);
    xxh->smp = xxh->ins;

    INSTRUMENT_INIT ();

    for (j = i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	B_ENDIAN32 (buffer[i].length);
	B_ENDIAN16 (buffer[i].loop_start);
	B_ENDIAN16 (buffer[i].looplen);
	B_ENDIAN16 (buffer[i].volume);
	B_ENDIAN16 (buffer[i].mode);

	/* Sample size is always rounded down */
	xxs[i].len = buffer[i].length & ~1;

	idx[j] = i;
	mode[i] = buffer[i].mode;

	xxih[i].nsm = !!(xxs[i].len);
	xxs[i].lps = buffer[i].loop_start;
	xxs[i].lpe = buffer[i].loop_start + buffer[i].looplen;
	xxs[i].flg = buffer[i].looplen > 2 ? WAVE_LOOPING : 0;
	xxi[i][0].vol = buffer[i].volume;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = j;
	strncpy ((char *) xxih[i].name, buffer[i].name, 20);
	str_adj ((char *) xxih[i].name);
	if ((V (1)) && (strlen ((char *) xxih[i].name) || (xxs[i].len > 1)))
	    report ("[%2X] %-20.20s %05x %05x %05x %c V%02x M%02x\n", i,
		xxih[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe, xxs[i].flg
		& WAVE_LOOPING ? 'L' : ' ', xxi[i][0].vol, buffer[i].mode);
	if (xxih[i].nsm)
	    j++;
    }
}


static void get_spee (int size, uint16 *buffer)
{
    B_ENDIAN16 (*buffer);
    xxh->tpo = *buffer;
    xxh->bpm = 125;
}


static void get_slen (int size, uint16 *buffer)
{
    B_ENDIAN16 (*buffer);
    xxh->pat = *buffer;
    xxh->trk = xxh->pat * xxh->chn;
}


static void get_plen (int size, uint16 *buffer)
{
    B_ENDIAN16 (*buffer);
    xxh->len = *buffer;
    if (V (0))
	report ("Module length  : %d patterns\n", xxh->len);
}


static void get_patt (int size, uint8 *buffer)
{
    B_ENDIAN16 (*buffer);
    memcpy (xxo, buffer, xxh->len);
}


static void get_pbod (int size, void *buffer)
{
    int j;
    uint16 rows;
    char *p = buffer;
    struct xxm_event *event;
    struct okt_event *oe;

    if (pattern >= xxh->pat)
	return;

    if (!pattern) {
	PATTERN_INIT ();
	if (V (0))
	    report ("Stored patterns: %d ", xxh->pat);
    }
    rows = *(uint16 *) p;
    p += 2;
    B_ENDIAN16 (rows);
    PATTERN_ALLOC (pattern);
    xxp[pattern]->rows = rows;
    TRACK_ALLOC (pattern);
    for (j = 0; j < rows * xxh->chn; j++) {
	event = &EVENT (pattern, j % xxh->chn, j / xxh->chn);
	oe = (struct okt_event *) p;
	p += sizeof (struct okt_event);
	memset (event, 0, sizeof (struct xxm_event));
	if (oe->note) {
	    event->note = 36 + oe->note;
	    event->ins = 1 + oe->ins;
	}
	event->fxt = fx[oe->fxt];
	event->fxp = oe->fxp;
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
    if (V (0))
	report (".");
    pattern++;
}


static void get_sbod (int size, char *buffer)
{
    int flags;

    if (sample >= xxh->ins)
	return;

    if (!sample && V (0))
	report ("\nStored samples : %d ", xxh->smp);

    flags = XMP_SMP_NOLOAD;
    if (mode[idx[sample]] == OKT_MODE8)
	flags |= XMP_SMP_7BIT;
    if (mode[idx[sample]] == OKT_MODEB)
	flags |= XMP_SMP_7BIT;
    xmp_drv_loadpatch (NULL, sample, xmp_ctl->c4rate, flags,
	&xxs[idx[sample]], buffer);
    if (V (0))
	report (".");
    sample++;
}


int okt_load (FILE * f)
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

    strcpy (xmp_ctl->type, "Oktalyzer");

    MODULE_INFO ();

    /* Load IFF chunks */
    while (!feof (f))
	iff_chunk (f);

    iff_release ();

    if (V (0))
	report ("\n");

    return 0;
}
