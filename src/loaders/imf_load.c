/* Extended Module Player
 * Copyright (C) 1996-2006 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: imf_load.c,v 1.2 2006-02-12 16:58:48 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Loader for Imago Orpheus modules based on the format description
 * written by Lutz Roeder.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "imf.h"
#include "period.h"

#define NONE 0xff
#define FX_IMF_FPORTA_UP 0xfe
#define FX_IMF_FPORTA_DN 0xfd

static uint8 arpeggio_val[32];

/* Effect conversion table */
static uint8 fx[] =
{
    NONE,		FX_S3M_TEMPO,
    FX_TEMPO,		FX_TONEPORTA,
    FX_TONE_VSLIDE,	FX_VIBRATO,
    FX_VIBRA_VSLIDE,	NONE /* fine vibrato */,
    FX_TREMOLO,		FX_ARPEGGIO,
    FX_SETPAN,		FX_PANSLIDE,
    FX_VOLSET,		FX_VOLSLIDE,
    FX_F_VSLIDE,	FX_FINETUNE,
    FX_NSLIDE_UP,	FX_NSLIDE_DN,
    FX_PORTA_UP,	FX_PORTA_DN,
    FX_IMF_FPORTA_UP,	FX_IMF_FPORTA_DN,
    NONE /*FX_FLT_CUTOFF*/,	NONE /*FX_FLT_RESN*/,
    FX_OFFSET,		NONE /* fine offset */,
    FX_KEYOFF,		FX_MULTI_RETRIG,
    FX_TREMOR,		FX_JUMP,
    FX_BREAK,		FX_GLOBALVOL,
    FX_G_VOLSLIDE,	FX_EXTENDED,
    FX_CHORUS,		FX_REVERB
};


/* Effect translation */
static void xlat_fx (int c, uint8 *fxt, uint8 *fxp)
{
    uint8 h = MSN (*fxp), l = LSN (*fxp);

    switch (*fxt = fx[*fxt]) {
    case FX_ARPEGGIO:			/* Arpeggio */
	if (*fxp)
	    arpeggio_val[c] = *fxp;
	else
	    *fxp = arpeggio_val[c];
	break;
    case FX_TEMPO:
	if (*fxp < 0x21)
	    *fxp = 0x21;
	break;
    case FX_S3M_TEMPO:
	if (*fxp <= 0x20)
	    *fxt = FX_TEMPO;
	break;
    case FX_IMF_FPORTA_UP:
	*fxt = FX_PORTA_UP;
	if (*fxp < 0x30)
	    *fxp = LSN (*fxp >> 2) | 0xe0;
	else
	    *fxp = LSN (*fxp >> 4) | 0xf0;
	break;
    case FX_IMF_FPORTA_DN:
	*fxt = FX_PORTA_DN;
	if (*fxp < 0x30)
	    *fxp = LSN (*fxp >> 2) | 0xe0;
	else
	    *fxp = LSN (*fxp >> 4) | 0xf0;
	break;
    case FX_EXTENDED:			/* Extended effects */
	switch (h) {
	case 0x1:			/* Set filter */
	case 0x2:			/* Undefined */
	case 0x4:			/* Undefined */
	case 0x6:			/* Undefined */
	case 0x7:			/* Undefined */
	case 0x9:			/* Undefined */
	case 0xe:			/* Ignore envelope */
	case 0xf:			/* Invert loop */
	    *fxp = *fxt = 0;
	    break;
	case 0x3:			/* Glissando */
	    *fxp = l | (EX_GLISS << 4);
	    break;
	case 0x5:			/* Vibrato waveform */
	    *fxp = l | (EX_VIBRATO_WF << 4);
	    break;
	case 0x8:			/* Tremolo waveform */
	    *fxp = l | (EX_TREMOLO_WF << 4);
	    break;
	case 0xa:			/* Pattern loop */
	    *fxp = l | (EX_PATTERN_LOOP << 4);
	    break;
	case 0xb:			/* Pattern delay */
	    *fxp = l | (EX_PATT_DELAY << 4);
	    break;
	case 0xc:
	    if (l == 0)
		*fxt = *fxp = 0;
	}
	break;
    case NONE:				/* No effect */
	*fxt = *fxp = 0;
	break;
    }
}


int imf_load (FILE *f)
{
    int c, r, i, j;
    struct xxm_event *event = 0, dummy;
    struct imf_header ih;
    struct imf_instrument ii;
    struct imf_sample is;
    int pat_len, smp_num;
    uint8 n, b;

    LOAD_INIT ();

    /* Load and convert header */
    fread(&ih.name, 32, 1, f);
    ih.len = read16l(f);
    ih.pat = read16l(f);
    ih.ins = read16l(f);
    ih.flg = read16l(f);
    fread(&ih.unused1, 8, 1, f);
    ih.tpo = read8(f);
    ih.bpm = read8(f);
    ih.vol = read8(f);
    ih.amp = read8(f);
    fread(&ih.unused2, 8, 1, f);
    fread(&ih.magic, 4, 1, f);

    for (i = 0; i < 32; i++) {
	fread(&ih.chn[i].name, 12, 1, f);
	ih.chn[i].status = read8(f);
	ih.chn[i].pan = read8(f);
	ih.chn[i].chorus = read8(f);
	ih.chn[i].reverb = read8(f);
    }

    fread(&ih.pos, 256, 1, f);

    if (ih.magic[0] != 'I' || ih.magic[1] != 'M' || ih.magic[2] != '1' ||
	ih.magic[3] != '0')
	return -1;

    copy_adjust((uint8 *)xmp_ctl->name, (uint8 *)ih.name, 32);

    xxh->len = ih.len;
    xxh->ins = ih.ins;
    xxh->smp = 1024;
    xxh->pat = ih.pat;

    if (ih.flg & 0x01)
	xxh->flg |= XXM_FLG_LINEAR;

    xxh->tpo = ih.tpo;
    xxh->bpm = ih.bpm;

    sprintf (xmp_ctl->type, "IM10 (Imago Orpheus)");

    MODULE_INFO ();

    for (xxh->chn = i = 0; i < 32; i++) {
	if (ih.chn[i].status != 0x00)
	    xxh->chn = i + 1;
	else
	    continue;
	xxc[i].pan = ih.chn[i].pan;
	xxc[i].cho = ih.chn[i].chorus;
	xxc[i].rvb = ih.chn[i].reverb;
	xxc[i].flg |= XXM_CHANNEL_FX;
    }
    xxh->trk = xxh->pat * xxh->chn;

    memcpy (xxo, ih.pos, xxh->len);
    for (i = 0; i < xxh->len; i++)
	if (xxo[i] == 0xff)
	    xxo[i]--;

    xmp_ctl->c4rate = C4_NTSC_RATE;
    xmp_ctl->fetch |= XMP_CTL_FINEFX;

    PATTERN_INIT ();

    /* Read patterns */

    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

    memset (arpeggio_val, 0, 32);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);

	pat_len = read16l(f) - 4;
	xxp[i]->rows = read16l(f);
	TRACK_ALLOC (i);

	r = 0;

	while (--pat_len >= 0) {
	    fread (&b, 1, 1, f);

	    if (b == IMF_EOR) {
		r++;
		continue;
	    }

	    c = b & IMF_CH_MASK;
	    event = c >= xxh->chn ? &dummy : &EVENT (i, c, r);

	    if (b & IMF_NI_FOLLOW) {
		n = read8(f);
		switch (n) {
		case 255:
		case 160:	/* ??!? */
		    n = 0x61;
		    break;	/* Key off */
		default:
		    n = 1 + 12 * MSN (n) + LSN (n);
		}

		event->note = n;
		event->ins = read8(f);
		pat_len -= 2;
	    }
	    if (b & IMF_FX_FOLLOWS) {
		event->fxt = read8(f);
		event->fxp = read8(f);
		xlat_fx (c, &event->fxt, &event->fxp);
		pat_len -= 2;
	    }
	    if (b & IMF_F2_FOLLOWS) {
		event->f2t = read8(f);
		event->f2p = read8(f);
		xlat_fx (c, &event->f2t, &event->f2p);
		pat_len -= 2;
	    }
	}

	if (V (0))
	    report (".");
    }

    if (V (0))
	report ("\n");

    INSTRUMENT_INIT ();

    /* Read and convert instruments and samples */

    if (V (0))
	report ("Instruments    : %d ", xxh->ins);

    if (V (1))
	report (
"\n     Instrument name                NSm Fade Env Smp# Len   Start End   C2Spd");

    for (smp_num = i = 0; i < xxh->ins; i++) {

	fread(&ii.name, 32, 1, f);
	fread(&ii.map, 120, 1, f);
	fread(&ii.unused, 8, 1, f);
	for (i = 0; i < 32; i++)
		ii.vol_env[i] = read16l(f);
	for (i = 0; i < 32; i++)
		ii.pan_env[i] = read16l(f);
	for (i = 0; i < 32; i++)
		ii.pitch_env[i] = read16l(f);
	for (i = 0; i < 3; i++) {
	    ii.env[i].npt = read8(f);
	    ii.env[i].sus = read8(f);
	    ii.env[i].lps = read8(f);
	    ii.env[i].lpe = read8(f);
	    ii.env[i].flg = read8(f);
	    fread(&ii.env[i].unused, 3, 1, f);
	}
	ii.fadeout = read16l(f);
	ii.nsm = read16l(f);
	fread(&ii.magic, 4, 1, f);

	if (strncmp((char *)ii.magic, "II10", 4))
	    return -2;

        if (ii.nsm)
 	    xxi[i] = calloc (sizeof (struct xxm_instrument), ii.nsm);

	xxih[i].nsm = ii.nsm;

	str_adj ((char *) ii.name);
	strncpy ((char *) xxih[i].name, ii.name, 24);

	memcpy (xxim[i].ins, ii.map, 96);

	if (V (1) && (strlen ((char *) ii.name) || ii.nsm))
	    report ("\n[%2X] %-31.31s %2d %4x %c%c%c ",
		i, ii.name, ii.nsm, ii.fadeout,
		ii.env[0].flg & 0x01 ? 'V' : '-',
		'-', '-');

	xxih[i].aei.npt = ii.env[0].npt;
	xxih[i].aei.sus = ii.env[0].sus;
	xxih[i].aei.lps = ii.env[0].lps;
	xxih[i].aei.lpe = ii.env[0].lpe;
	xxih[i].aei.flg = ii.env[0].flg & 0x01 ? XXM_ENV_ON : 0;
	xxih[i].aei.flg |= ii.env[0].flg & 0x02 ? XXM_ENV_SUS : 0;
	xxih[i].aei.flg |= ii.env[0].flg & 0x04 ?  XXM_ENV_LOOP : 0;

	if (xxih[i].aei.npt)
	    xxae[i] = calloc (4, xxih[i].aei.npt);

	for (j = 0; j < xxih[i].aei.npt; j++) {
	    xxae[i][j * 2] = ii.vol_env[j * 2];
	    xxae[i][j * 2 + 1] = ii.vol_env[j * 2 + 1];
	}

	for (j = 0; j < ii.nsm; j++, smp_num++) {

	    fread(&is.name, 13, 1, f);
	    fread(&is.unused1, 3, 1, f);
	    is.len = read32l(f);
	    is.lps = read32l(f);
	    is.lpe = read32l(f);
	    is.rate = read32l(f);
	    is.vol = read8(f);
	    is.pan = read8(f);
	    fread(&is.unused2, 14, 1, f);
	    is.flg = read8(f);
	    fread(&is.unused3, 5, 1, f);
	    is.ems = read16l(f);
	    is.dram = read32l(f);
	    fread(&is.magic, 4, 1, f);

	    xxi[i][j].sid = smp_num;
	    xxi[i][j].vol = is.vol;
	    xxi[i][j].pan = is.pan;
	    xxs[smp_num].len = is.len;
	    xxs[smp_num].lps = is.lps;
	    xxs[smp_num].lpe = is.lpe;
	    xxs[smp_num].flg = is.flg & 1 ? WAVE_LOOPING : 0;
	    xxs[smp_num].flg |= is.flg & 4 ? WAVE_16_BITS : 0;

	    if (V (1)) {
		if (j)
		    report("\n\t\t\t\t\t\t ");
		report ("[%02x] %05x %05x %05x %5d ",
		    j, is.len, is.lps, is.lpe, is.rate);
	    }
	    c2spd_to_note (is.rate, &xxi[i][j].xpo, &xxi[i][j].fin);

	    if (!xxs[smp_num].len)
		continue;

	    xmp_drv_loadpatch (f, xxi[i][j].sid, xmp_ctl->c4rate, 0,
		&xxs[xxi[i][j].sid], NULL);

	    if (V (0))
		report (".");
	}
    }
    xxh->smp = smp_num;
    xxs = realloc (xxs, sizeof (struct xxm_sample) * xxh->smp);

    if (V (0))
	report ("\n");

    xmp_ctl->fetch |= XMP_MODE_ST3;

    return 0;
}
