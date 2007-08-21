/* Scream Tracker 3 module loader for xmp
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: s3m_load.c,v 1.10 2007-08-21 12:38:32 cmatsuoka Exp $
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

/*
 * From http://code.pui.ch/2007/02/18/turn-demoscene-modules-into-mp3s/
 * The only flaw I noticed [in xmp] is a problem with portamento in Purple
 * Motion's second reality soundtrack (1:06-1:17)
 *
 * Claudio's note: that's a dissonant beating between channels 6 and 7
 * starting at pos12, caused by pitchbending effect F25.
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
	FX_S3M_TEMPO,		/* Axx  Set speed to xx (the default is 06) */
	FX_JUMP,		/* Bxx  Jump to order xx (hexadecimal) */
	FX_BREAK,		/* Cxx  Break pattern to row xx (decimal) */
	FX_VOLSLIDE,		/* Dxy  Volume slide down by y/up by x */
	FX_PORTA_DN,		/* Exx  Slide down by xx */
	FX_PORTA_UP,		/* Fxx  Slide up by xx */
	FX_TONEPORTA,		/* Gxx  Tone portamento with speed xx */
	FX_VIBRATO,		/* Hxy  Vibrato with speed x and depth y */
	FX_TREMOR,		/* Ixy  Tremor with ontime x and offtime y */
	FX_ARPEGGIO,		/* Jxy  Arpeggio with halfnote additions */
	FX_VIBRA_VSLIDE,	/* Kxy  Dual command: H00 and Dxy */
	FX_TONE_VSLIDE,		/* Lxy  Dual command: G00 and Dxy */
	NONE,
	NONE,
	FX_OFFSET,		/* Oxy  Set sample offset */
	NONE,
	FX_MULTI_RETRIG,	/* Qxy  Retrig (+volumeslide) note */
	FX_TREMOLO,		/* Rxy  Tremolo with speed x and depth y */
	FX_S3M_EXTENDED,	/* Sxx  (misc effects) */
	FX_S3M_BPM,		/* Txx  Tempo = xx (hex) */
	FX_FINE4_VIBRA,		/* Uxx  Fine vibrato */
	FX_GLOBALVOL,		/* Vxx  Set global volume */
	NONE,
	NONE,
	NONE,
	NONE
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
    uint8 n, b, x8;

    LOAD_INIT ();

    fread(&sfh.name, 28, 1, f);		/* Song name */
    read8(f);				/* 0x1a */
    sfh.type = read8(f);		/* File type */
    read16l(f);				/* Reserved */
    sfh.ordnum = read16l(f);		/* Number of orders (must be even) */
    sfh.insnum = read16l(f);		/* Number of instruments */
    sfh.patnum = read16l(f);		/* Number of patterns */
    sfh.flags = read16l(f);		/* Flags */
    sfh.version = read16l(f);		/* Tracker ID and version */
    sfh.ffi = read16l(f);		/* File format information */
    fread(&sfh.magic, 4, 1, f);		/* 'SCRM' */
    sfh.gv = read8(f);			/* Global volume */
    sfh.is = read8(f);			/* Initial speed */
    sfh.it = read8(f);			/* Initial tempo */
    sfh.mv = read8(f);			/* Master volume */
    sfh.uc = read8(f);			/* Ultra click removal */
    sfh.dp = read8(f);			/* Default pan positions if 0xfc */
    fread(&sfh.rsvd2, 8, 1, f);		/* Reserved */
    sfh.special = read16l(f);		/* Ptr to special custom data */
    fread(sfh.chset, 32, 1, f);		/* Channel settings */

    if (strncmp ((char *) sfh.magic, "SCRM", 4))
	return -1;

    copy_adjust((uint8 *)xmp_ctl->name, sfh.name, 28);

    /* Load and convert header */
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
	if (sfh.chset[i] == S3M_CH_OFF)
	    continue;

	xxh->chn = i + 1;

	if (sfh.mv & 0x80) {	/* stereo */
		int x = sfh.chset[i] & S3M_CH_PAN;
		xxc[i].pan = (x & 0x0f) < 8 ? 0x00 : 0xff;
	} else {
		xxc[i].pan = 0x80;
	}
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

    for (i = 0; i < xxh->ins; i++)
	pp_ins[i] = read16l(f);
 
    for (i = 0; i < xxh->pat; i++)
	pp_pat[i] = read16l(f);

    /* Default pan positions */

    for (i = 0, sfh.dp -= 0xfc; !sfh.dp /* && n */ && (i < 32); i++) {
	uint8 x = read8(f);
	if (x & S3M_PAN_SET)
	    xxc[i].pan = (x << 4) & 0xff;
	else
	    xxc[i].pan = sfh.mv % 0x80 ? 0x30 + 0xa0 * (i & 1) : 0x80;
    }

    xmp_ctl->c4rate = C4_NTSC_RATE;
    xmp_ctl->fetch |= XMP_CTL_FINEFX;

    strcpy(xmp_ctl->type, "SCRM (S3M)");
    if (sfh.version == 0x1300)
	xmp_ctl->fetch |= XMP_CTL_VSALL;

    switch (sfh.version >> 12) {
    case 1:
	strcpy(tracker_name, "Scream Tracker");
	xmp_ctl->fetch |= XMP_CTL_ST3GVOL;
	break;
    case 2:
	strcpy(tracker_name, "Imago Orpheus");
	break;
    case 3:
	strcpy(tracker_name, "Impulse Tracker");
	break;
    default:
	snprintf(tracker_name, 80, "unknown (%d) version",
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
	pat_len = read16l(f) - 2;

	/* Used to be (--pat_len >= 0). Replaced by Rudolf Cejka
	 * <cejkar@dcse.fee.vutbr.cz>, fixes hunt.s3m
	 * ftp://us.aminet.net/pub/aminet/mods/8voic/s3m_hunt.lha
	 */
	while (r < xxp[i]->rows) {
	    b = read8(f);

	    if (b == S3M_EOR) {
		r++;
		continue;
	    }

	    c = b & S3M_CH_MASK;
	    event = c >= xxh->chn ? &dummy : &EVENT (i, c, r);

	    if (b & S3M_NI_FOLLOW) {
		switch(n = read8(f)) {
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
		event->ins = read8(f);
		pat_len -= 2;
	    }

	    if (b & S3M_VOL_FOLLOWS) {
		event->vol = read8(f) + 1;
		pat_len--;
	    }

	    if (b & S3M_FX_FOLLOWS) {
		event->fxt = read8(f);
		event->fxp = read8(f);
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
	report ("Stereo enabled : %s\n", sfh.mv & 0x80 ? "yes" : "no");
	report ("Pan settings   : %s\n", sfh.dp ? "no" : "yes");
    }

    INSTRUMENT_INIT ();

    /* Read and convert instruments and samples */

    if (V (0))
	report ("Instruments    : %d ", xxh->ins);

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	fseek (f, pp_ins[i] * 16, SEEK_SET);
	x8 = read8(f);
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;

	if (x8 >= 2) {
	    /* OPL2 FM instrument */

	    fread(&sah.dosname, 12, 1, f);	/* DOS file name */
	    fread(&sah.rsvd1, 3, 1, f);		/* 0x00 0x00 0x00 */
	    fread(&sah.reg, 12, 1, f);		/* Adlib registers */
	    sah.vol = read8(f);
	    sah.dsk = read8(f);
	    read16l(f);
	    sah.c2spd = read16l(f);		/* C 4 speed */
	    read16l(f);
	    fread(&sah.rsvd4, 12, 1, f);	/* Reserved */
	    fread(&sah.name, 28, 1, f);		/* Instrument name */
	    fread(&sah.magic, 4, 1, f);		/* 'SCRI' */

	    if (strncmp ((char *)sah.magic, "SCRI", 4))
		return -2;
	    sah.magic[0] = 0;

	    copy_adjust(xxih[i].name, sah.name, 24);

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

	fread(&sih.dosname, 13, 1, f);		/* DOS file name */
	sih.memseg = read16l(f);		/* Pointer to sample data */
	sih.length = read32l(f);		/* Length */
	sih.loopbeg = read32l(f);		/* Loop begin */
	sih.loopend = read32l(f);		/* Loop end */
	sih.vol = read8(f);			/* Volume */
	sih.rsvd1 = read8(f);			/* Reserved */
	sih.pack = read8(f);			/* Packing type (not used) */
	sih.flags = read8(f);		/* Loop/stereo/16bit samples flags */
	sih.c2spd = read16l(f);			/* C 4 speed */
	sih.rsvd2 = read16l(f);			/* Reserved */
	fread(&sih.rsvd3, 4, 1, f);		/* Reserved */
	sih.int_gp = read16l(f);		/* Internal - GUS pointer */
	sih.int_512 = read16l(f);		/* Internal - SB pointer */
	sih.int_last = read32l(f);		/* Internal - SB index */
	fread(&sih.name, 28, 1, f);		/* Instrument name */
	fread(&sih.magic, 4, 1, f);		/* 'SCRS' */

	if ((x8 == 1) && strncmp((char *)sih.magic, "SCRS", 4))
	    return -2;

	xxih[i].nsm = !!(xxs[i].len = sih.length);
	xxs[i].lps = sih.loopbeg;
	xxs[i].lpe = sih.loopend;

	xxs[i].flg = sih.flags & 1 ? WAVE_LOOPING : 0;
	xxs[i].flg |= sih.flags & 4 ? WAVE_16_BITS : 0;
	xxi[i][0].vol = sih.vol;
	sih.magic[0] = 0;

	copy_adjust(xxih[i].name, sih.name, 24);

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

