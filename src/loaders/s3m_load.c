/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/*
 * Tue, 30 Jun 1998 20:23:11 +0200
 * Reported by John v/d Kamp <blade_@dds.nl>:
 * I have this song from Purple Motion called wcharts.s3m, the global
 * volume was set to 0, creating a devide by 0 error in xmp. There should
 * be an extra test if it's 0 or not.
 *
 * Claudio's fix: global volume ignored
 */

/*
 * Sat, 29 Aug 1998 18:50:43 -0500 (CDT)
 * Reported by Joel Jordan <scriber@usa.net>:
 * S3M files support tempos outside the ranges defined by xmp (that is,
 * the MOD/XM tempo ranges).  S3M's can have tempos from 0 to 255 and speeds
 * from 0 to 255 as well, since these are handled with separate effects
 * unlike the MOD format.  This becomes an issue in such songs as Skaven's
 * "Catch that Goblin", which uses speeds above 0x1f.
 *
 * Claudio's fix: FX_S3M_TEMPO added. S3M supports speeds from 0 to 255 and
 * tempos from 32 to 255 (S3M speed == xmp tempo, S3M tempo == xmp BPM).
 */

/* Wed, 21 Oct 1998 15:03:44 -0500  Geoff Reedy <vader21@imsa.edu>
 * It appears that xmp has issues loading/playing a specific instrument
 * used in LUCCA.S3M.
 * (Fixed by Hipolito in xmp-2.0.0dev34)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "s3m.h"
#include "period.h"


#define NONE 0xff
#define FX_S3M_EXTENDED 0xfe


static uint16 *pp_ins;		/* Parapointers to instruments */
static uint16 *pp_pat;		/* Parapointers to patterns */
static uint8 arpeggio_val[32];

/* Effect conversion table */
static uint8 fx[] =
{
    NONE,
    FX_S3M_TEMPO,	FX_JUMP,
    FX_BREAK,		FX_VOLSLIDE,
    FX_PORTA_DN,	FX_PORTA_UP,
    FX_TONEPORTA,	FX_VIBRATO,
    FX_TREMOR,		FX_ARPEGGIO,
    FX_VIBRA_VSLIDE,	FX_TONE_VSLIDE,
    NONE,		NONE,
    FX_OFFSET,		NONE,
    FX_MULTI_RETRIG,	FX_TREMOLO,
    FX_S3M_EXTENDED,	FX_TEMPO,
    NONE,		FX_GLOBALVOL,
    NONE,		NONE,
    NONE,		NONE
};


/* Effect translation */
static void xlat_fx (int c, struct xxm_event *e)
{
    uint8 h = MSN (e->fxp), l = LSN (e->fxp);

    switch (e->fxt = fx[e->fxt]) {
    case FX_ARPEGGIO:			/* Arpeggio */
	if (e->fxp)
	    arpeggio_val[c] = e->fxp;
	else
	    e->fxp = arpeggio_val[c];
	break;
    case FX_TEMPO:	/* BPM */
	if (e->fxp && e->fxp < 0x21)
	    e->fxp = 0x21;
	break;
    case FX_S3M_TEMPO:
	if (e->fxp <= 0x20)
	    e->fxt = FX_TEMPO;
	break;
    case FX_S3M_EXTENDED:		/* Extended effects */
	e->fxt = FX_EXTENDED;
	switch (h) {
	case 0x1:			/* Glissando */
	    e->fxp = LSN (e->fxp) | (EX_GLISS << 4);
	    break;
	case 0x2:			/* Finetune */
	    e->fxp = LSN (e->fxp) | (EX_FINETUNE << 4);
	    break;
	case 0x3:			/* Vibrato wave */
	    e->fxp = LSN (e->fxp) | (EX_VIBRATO_WF << 4);
	    break;
	case 0x4:			/* Tremolo wave */
	    e->fxp = LSN (e->fxp) | (EX_TREMOLO_WF << 4);
	    break;
	case 0x5:
	case 0x6:
	case 0x7:
	case 0x9:
	case 0xa:			/* Ignore */
	    e->fxt = e->fxp = 0;
	    break;
	case 0x8:			/* Set pan */
	    e->fxt = FX_MASTER_PAN;
	    e->fxp = l << 4;
	    break;
	case 0xb:			/* Pattern loop */
	    e->fxp = LSN (e->fxp) | (EX_PATTERN_LOOP << 4);
	    break;
	case 0xc:
	    if (!l)
		e->fxt = e->fxp = 0;
	}
	break;
    case NONE:				/* No effect */
	e->fxt = e->fxp = 0;
	break;
    }
}


int s3m_load (FILE * f)
{
    int c, r, i, j;
    struct s3m_adlib_header sah;

    struct xxm_event *event = 0, dummy;
    struct s3m_file_header sfh;
    struct s3m_instrument_header sih;
    int pat_len;
    uint8 n, b, x8, tmp[80];
    uint16 x16;


    LOAD_INIT ();

    /* Load and convert header */
    fread (&sfh, 1, sizeof (sfh), f);
    if (strncmp ((char *) sfh.magic, "SCRM", 4))
	return -1;
    L_ENDIAN16 (sfh.ordnum);
    L_ENDIAN16 (sfh.insnum);
    L_ENDIAN16 (sfh.patnum);
    L_ENDIAN16 (sfh.ffi);
    L_ENDIAN16 (sfh.version);
    str_adj ((char *) sfh.name);
    strcpy (xmp_ctl->name, (char *) sfh.name);
    xxh->len = sfh.ordnum;
    xxh->ins = sfh.insnum;
    xxh->smp = xxh->ins;
    xxh->pat = sfh.patnum;
    pp_ins = calloc (2, xxh->ins);
    pp_pat = calloc (2, xxh->pat);
    if (sfh.flags & S3M_AMIGA_RANGE)
	xxh->flg |= XXM_FLG_MODRNG;
    if (sfh.flags & S3M_ST300_VOLS)
	xmp_ctl->fetch |= XMP_CTL_VSALL;
    /* xmp_ctl->volbase = 4096 / sfh.gv; */
    xxh->tpo = sfh.is;
    xxh->bpm = sfh.it;

    for (i = 0; i < 32; i++) {
	if (sfh.chset[i] != S3M_CH_OFF)
	    xxh->chn = i + 1;
	else
	    continue;
	xxc[i].pan = sfh.mv & 0x80 ?
		(((sfh.chset[i] & S3M_CH_PAN) >> 3) * 0xff) & 0xff : 0x80;
    }
    xxh->trk = xxh->pat * xxh->chn;

    fread (xxo, 1, xxh->len, f);

#if 0
    /* S3M skips pattern 0xfe */
    for (i = 0; i < (xxh->len - 1); i++)
	if (xxo[i] == 0xfe) {
	    memcpy (&xxo[i], &xxo[i + 1], xxh->len - i - 1);
	    xxh->len--;
	}
    while (xxh->len && xxo[xxh->len - 1] == 0xff)
	xxh->len--;
#endif

    for (i = 0; i < xxh->ins; i++) {
	fread (&pp_ins[i], 2, 1, f);
	L_ENDIAN16 (pp_ins[i]);
    }
    for (i = 0; i < xxh->pat; i++) {
	fread (&pp_pat[i], 2, 1, f);
	L_ENDIAN16 (pp_pat[i]);
    }

    /* Default pan positions */
    for (i = 0, sfh.dp -= 0xfc; !sfh.dp && n && (i < 32); i++) {
	fread (tmp, 1, 1, f);
	if (tmp[0] && S3M_PAN_SET)
	    xxc[i].pan = (tmp[0] << 4) & 0xff;
    }

    xmp_ctl->c4rate = C4_NTSC_RATE;
    xmp_ctl->fetch |= XMP_CTL_FINEFX;

    sprintf (xmp_ctl->type, "SCRM (S3M)");
    if (sfh.version == 0x1300)
	xmp_ctl->fetch |= XMP_CTL_VSALL;

    switch (sfh.version >> 12) {
    case 1:
	sprintf (tracker_name, "Scream Tracker");
	xmp_ctl->fetch |= XMP_CTL_ST3GVOL;
	break;
    case 2:
	sprintf (tracker_name, "Imago Orpheus");
	break;
    case 3:
	sprintf (tracker_name, "Impulse Tracker");
	break;
    default:
	sprintf (tracker_name, "unknown (%d) version",
	    sfh.version >> 12);
    }

    sprintf (tracker_name + strlen (tracker_name), " %d.%02x",
	(sfh.version & 0x0f00) >> 8, sfh.version & 0xff);

    MODULE_INFO ();

    PATTERN_INIT ();

    /* Read patterns */

    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

    memset (arpeggio_val, 0, 32);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	TRACK_ALLOC (i);

	if (!pp_pat[i])
	    continue;

	fseek (f, pp_pat[i] * 16, SEEK_SET);
	r = 0;
	fread (&x16, 2, 1, f);
	pat_len = x16;
	L_ENDIAN16 (pat_len);
	pat_len -= 2;

	/* Used to be (--pat_len >= 0). Replaced by Rudolf Cejka
	 * <cejkar@dcse.fee.vutbr.cz>, fixes hunt.s3m
	 * ftp://us.aminet.net/pub/aminet/mods/8voic/s3m_hunt.lha
	 */
	while (r < xxp[i]->rows) {
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
		    n = 1 + 12 * MSN (n) + LSN (n);
		}

		event->note = n;
		fread (&n, 1, 1, f);
		event->ins = n;
		pat_len -= 2;
	    }

	    if (b & S3M_VOL_FOLLOWS) {
		fread (&n, 1, 1, f);
		event->vol = n + 1;
		pat_len--;
	    }

	    if (b & S3M_FX_FOLLOWS) {
		fread (&n, 1, 1, f);
		event->fxt = n;
		fread (&n, 1, 1, f);
		event->fxp = n;
		xlat_fx (c, event);
		pat_len -= 2;
	    }
	}

	if (V (0))
	    report (".");
    }

    if (V (0))
	report ("\n");

    if (V (1)) {
	report ("Stereo enabled : %s\n", sfh.mv % 0x80 ? "yes" : "no");
	report ("Pan settings   : %s\n", sfh.dp ? "no" : "yes");
    }

    INSTRUMENT_INIT ();

    /* Read and convert instruments and samples */

    if (V (0))
	report ("Instruments    : %d ", xxh->ins);

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	fseek (f, pp_ins[i] * 16, SEEK_SET);
	fread (&x8, 1, 1, f);
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;

	if (x8 >= 2) {
	    /* OPL2 FM instrument */
	    fread (&sah, 1, sizeof (sah), f);
	    if (strncmp (sah.magic, "SCRI", 4))
		return -2;
	    L_ENDIAN16 (sah.c2spd);
	    sah.magic[0] = 0;
	    str_adj ((char *) sah.name);
	    strncpy ((char *) xxih[i].name, sah.name, 24);
	    xxih[i].nsm = 1;
	    xxi[i][0].vol = sah.vol;
	    c2spd_to_note (sah.c2spd, &xxi[i][0].xpo, &xxi[i][0].fin);
	    xxi[i][0].xpo += 12;
	    xmp_drv_loadpatch (f, xxi[i][0].sid, 0, 0, NULL, (char *) &sah.reg);
	    if (V (0)) {
	        if (V (1)) {
		    report ("\n[%2X] %-28.28s ", i, sah.name);
	            for (j = 0; j < 11; j++)
		        report ("%02x ", (uint8) sah.reg[j]);
		} else
		    report (".");
	    }

	    continue;
	}
	fread (&sih, 1, sizeof (sih), f);

	if ((x8 == 1) && strncmp (sih.magic, "SCRS", 4))
	    return -2;

	L_ENDIAN16 (sih.memseg);
	L_ENDIAN32 (sih.length);
	L_ENDIAN32 (sih.loopbeg);
	L_ENDIAN32 (sih.loopend);
	L_ENDIAN16 (sih.c2spd);

	xxih[i].nsm = !!(xxs[i].len = sih.length);
	xxs[i].lps = sih.loopbeg;
	xxs[i].lpe = sih.loopend;

	xxs[i].flg = sih.flags & 1 ? WAVE_LOOPING : 0;
	xxs[i].flg |= sih.flags & 4 ? WAVE_16_BITS : 0;
	xxi[i][0].vol = sih.vol;
	sih.magic[0] = 0;
	str_adj ((char *) sih.name);
	strncpy ((char *) xxih[i].name, sih.name, 24);

	if ((V (1)) && (strlen ((char *) sih.name) || xxs[i].len))
	    report ("\n[%2X] %-28.28s %04x%c%04x %04x %c V%02x %5d ",
		i, sih.name, xxs[i].len, xxs[i].flg & WAVE_16_BITS ?'+' :
		' ', xxs[i].lps, xxs[i].lpe, xxs[i].flg & WAVE_LOOPING ?
		'L' : ' ', xxi[i][0].vol, sih.c2spd);

	c2spd_to_note (sih.c2spd, &xxi[i][0].xpo, &xxi[i][0].fin);

	if (!xxs[i].len)
	    continue;
	fseek (f, 16L * sih.memseg, SEEK_SET);
	xmp_drv_loadpatch (f, xxi[i][0].sid, xmp_ctl->c4rate,
	    (sfh.ffi - 1) * XMP_SMP_UNS, &xxs[i], NULL);
	if (V (0))
	    report (".");
    }

    if (V (0))
	report ("\n");

    xmp_ctl->fetch |= XMP_MODE_ST3;

    return 0;
}
