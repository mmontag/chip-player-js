/* Protracker module loader for xmp
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: mod_load.c,v 1.12 2007-09-18 12:18:45 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* This loader recognizes the following variants of the Protracker
 * module format:
 *
 * - Protracker M.K. and M!K!
 * - Protracker songs
 * - Noisetracker N.T. and M&K! (not tested)
 * - Fast Tracker 6CHN and 8CHN
 * - Fasttracker II/Take Tracker ?CHN and ??CH
 * - M.K. with ADPCM samples (MDZ)
 * - Mod's Grave M.K. w/ 8 channels (WOW)
 * - Atari Octalyser CD61 and CD81
 * - Digital Tracker FA04, FA06 and FA08
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <sys/types.h>

#include "load.h"
#include "mod.h"

struct {
    char *magic;
    char *id;
    int flag;
    char *tracker;
    int ch;
} mod_magic[] = {
    { "M.K.", "4 channel MOD", 0, "Protracker", 4 },
    { "M!K!", "4 channel MOD", 1, "Protracker", 4 },
    { "M&K!", "4 channel MOD", 1, "Noisetracker", 4 },
    { "N.T.", "4 channel MOD", 1, "Noisetracker", 4 },
    { "6CHN", "6 channel MOD", 0, "Fast Tracker", 6 },
    { "8CHN", "8 channel MOD", 0, "Fast Tracker", 8 },
    { "CD61", "6 channel MOD", 1, "Octalyser", 6 }, /* Atari STe/Falcon */
    { "CD81", "8 channel MOD", 1, "Octalyser", 8 }, /* Atari STe/Falcon */
    { "FA04", "4 channel MOD", 1, "Digital Tracker", 4 }, /* Atari Falcon */
    { "FA06", "6 channel MOD", 1, "Digital Tracker", 6 }, /* Atari Falcon */
    { "FA08", "8 channel MOD", 1, "Digital Tracker", 8 }, /* Atari Falcon */
    { "PWIZ", "Packed module", 1, "converted with ProWizard", 4 },
    { "", 0 }
};

static int module_load (FILE *, int);


int mod_load (FILE *f)
{
    return module_load (f, 0);
}


int ptdt_load (FILE *f)
{
    return module_load (f, 1);
}


static int is_st_ins (char *s)
{
    if (s[0] != 's' && s[0] != 'S')
	return 0;
    if (s[1] != 't' && s[1] != 'T')
	return 0;
    if (s[2] != '-' || s[5] != ':')
	return 0;
    if (!isdigit(s[3]) || !isdigit(s[4]))
	return 0;

    return 1;
}


static int module_load (FILE *f, int ptdt)
{
    int i, j;
    int smp_size, pat_size, wow, ptsong = 0;
    struct xxm_event *event;
    struct mod_header mh;
    uint8 mod_event[4];
    char *x, pathname[256] = "", *id = "", *tracker = "";
    int lps_mult = xmp_ctl->fetch & XMP_CTL_FIXLOOP ? 1 : 2;
    int detected = 0;
    char idbuffer[32];

    if (!ptdt)
	LOAD_INIT ();

    xxh->ins = 31;
    xxh->smp = xxh->ins;
    xxh->chn = ptdt ? 4 : 0;
    smp_size = 0;
    pat_size = 0;

    if (xxh->chn == 4)
	xxh->flg |= XXM_FLG_MODRNG;

    fread(&mh.name, 20, 1, f);
    for (i = 0; i < 31; i++) {
	fread(&mh.ins[i].name, 22, 1, f);	/* Instrument name */
	mh.ins[i].size = read16b(f);		/* Length in 16-bit words */
	mh.ins[i].finetune = read8(f);		/* Finetune (signed nibble) */
	mh.ins[i].volume = read8(f);		/* Linear playback volume */
	mh.ins[i].loop_start = read16b(f);	/* Loop start in 16-bit words */
	mh.ins[i].loop_size = read16b(f);	/* Loop size in 16-bit words */
    }
    mh.len = read8(f);
    mh.restart = read8(f);
    fread(&mh.order, 128, 1, f);
    fread(&mh.magic, 4, 1, f);

    for (i = 0; mod_magic[i].ch; i++) {
	if (!(strncmp ((char *) mh.magic, mod_magic[i].magic, 4))) {
	    xxh->chn = mod_magic[i].ch;
	    id = mod_magic[i].id;
	    tracker = mod_magic[i].tracker;
	    detected = mod_magic[i].flag;
	    break;
	}
    }

    /* Prowizard hack */
    if (memcmp(mh.magic, "PWIZ", 4) == 0) {
	int pos = ftell(f);
	fseek(f, -30, SEEK_END);
	fread(&mh.magic, 4, 1, f);
	fseek(f, 4, SEEK_CUR);
	fread(idbuffer, 1, 22, f);
	id = idbuffer;
	fseek(f, pos, SEEK_SET);
    } else if (!xxh->chn) {
	if (!strncmp ((char *) mh.magic + 2, "CH", 2) &&
	    isdigit (*mh.magic) && isdigit (mh.magic[1])) {
	    if ((xxh->chn = (*mh.magic - '0') *
		10 + mh.magic[1] - '0') > 32)
		return -1;
	} else if (!strncmp ((char *) mh.magic + 1, "CHN", 3) &&
	    isdigit (*mh.magic)) {
	    if (!(xxh->chn = (*mh.magic - '0')))
		return -1;
	} else
	    return -1;
	id = "Multichannel MOD";
	tracker = "TakeTracker/FastTracker II";
	detected = 1;
	xxh->flg &= ~XXM_FLG_MODRNG;
    }

    if (!ptdt)
	strncpy (xmp_ctl->name, (char *) mh.name, 20);

    xxh->len = mh.len;
    /* xxh->rst = mh.restart; */

    if (xxh->rst >= xxh->len)
	xxh->rst = 0;
    memcpy (xxo, mh.order, 128);

    for (i = 0; i < 128; i++) {
	/* This fixes dragnet.mod (garbage in the order list) */
	if (xxo[i] > 0x7f)
		break;
	if (xxo[i] > xxh->pat)
	    xxh->pat = xxo[i];
    }
    xxh->pat++;

    pat_size = 256 * xxh->chn * xxh->pat;

    INSTRUMENT_INIT ();

    for (i = 0; i < xxh->ins; i++) {
	smp_size += 2 * mh.ins[i].size;

	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxs[i].len = 2 * mh.ins[i].size;
	xxs[i].lps = lps_mult * mh.ins[i].loop_start;
	xxs[i].lpe = xxs[i].lps + 2 * mh.ins[i].loop_size;
	xxs[i].flg = (mh.ins[i].loop_size > 1 && xxs[i].lpe > 8) ?
		WAVE_LOOPING : 0;
	xxi[i][0].fin = (int8)(mh.ins[i].finetune << 4);
	xxi[i][0].vol = mh.ins[i].volume;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	xxih[i].nsm = !!(xxs[i].len);
	copy_adjust(xxih[i].name, mh.ins[i].name, 22);
    }

    /*
     * Experimental tracker-detection routine
     */ 

    if (detected)
	goto skip_test;

    /* Test for Flextrax modules
     *
     * FlexTrax is a soundtracker for Atari Falcon030 compatible computers.
     * FlexTrax supports the standard MOD file format (up to eight channels)
     * for compatibility reasons but also features a new enhanced module
     * format FLX. The FLX format is an extended version of the standard
     * MOD file format with support for real-time sound effects like reverb
     * and delay.
     */

    if (0x43c + xxh->pat * 4 * xxh->chn * 0x40 + smp_size < xmp_ctl->size) {
	int pos = ftell(f);
	fseek(f, 0x43c + xxh->pat * 4 * xxh->chn * 0x40 + smp_size, SEEK_SET);
	fread(idbuffer, 1, 4, f);
	fseek(f, pos, SEEK_SET);

	if (!memcmp(idbuffer, "FLEX", 4)) {
	    tracker = "Flextrax";
	    goto skip_test;
	}
    }

    /* Test for Mod's Grave WOW modules
     *
     * Stefan Danes <sdanes@marvels.hacktic.nl> said:
     * This weird format is identical to '8CHN' but still uses the 'M.K.' ID.
     * You can only test for WOW by calculating the size of the module for 8 
     * channels and comparing this to the actual module length. If it's equal, 
     * the module is an 8 channel WOW.
     */

    if ((wow = (!strncmp ((char *) mh.magic, "M.K.", 4) &&
		(0x43c + xxh->pat * 32 * 0x40 + smp_size == xmp_ctl->size)))) {
	xxh->chn = 8;
	id = "WOW M.K. 8 channel MOD";
	tracker = "Mod's Grave";
	goto skip_test;
    }

    /* Test for UNIC tracker modules
     *
     * From Gryzor's Pro-Wizard PW_FORMATS-Engl.guide:
     * ``The UNIC format is very similar to Protracker... At least in the
     * heading... same length : 1084 bytes. Even the "M.K." is present,
     * sometimes !! Maybe to disturb the rippers.. hehe but Pro-Wizard
     * doesn't test this only!''
     */

    else if (sizeof (struct mod_header) + xxh->pat * 0x300 +
	smp_size == xmp_ctl->size) {
	return -1;
    }

    /* Test for Protracker song files
     */
    else if ((ptsong = (!strncmp ((char *) mh.magic, "M.K.", 4) &&
		(0x43c + xxh->pat * 0x400 == xmp_ctl->size)))) {
	id = "4 channel song";
	tracker = "Protracker";
	goto skip_test;
    }

    /* Test Protracker-like files
     */
    if (xxh->chn == 4 && mh.restart == xxh->pat) {
	tracker = "Soundtracker";
    } else if (xxh->chn == 4 && mh.restart == 0x78) {
	tracker = "Noisetracker" /*" (0x78)"*/;
    } else if (mh.restart < 0x7f) {
	if (xxh->chn == 4)
	    tracker = "Noisetracker";
	else
	    tracker = "(unknown)";
	xxh->rst = mh.restart;
    }

    if (xxh->chn != 4 && mh.restart == 0x7f)
	tracker = "(unknown)";

    if (xxh->chn == 4 && mh.restart == 0x7f) {
	for (i = 0; i < 31; i++) {
	    if (mh.ins[i].loop_size == 0)
		break;
	}
	if (i < 31)
	    tracker = "Protracker clone";
    }

    if (mh.restart != 0x78 && mh.restart < 0x7f) {
	for (i = 0; i < 31; i++) {
	    if (mh.ins[i].loop_size == 0)
		break;
	}
	if (i == 31) {	/* All loops are size 2 or greater */
	    for (i = 0; i < 31; i++) {
		if (mh.ins[i].size == 1 && mh.ins[i].volume == 0) {
		    tracker = "Probably converted";
		    goto skip_test;
		}
	    }

	    for (i = 0; i < 31; i++) {
	        if (is_st_ins((char *)mh.ins[i].name))
		    break;
	    }
	    if (i == 31) {	/* No st- instruments */
	        for (i = 0; i < 31; i++) {
		    if (mh.ins[i].size == 0 && mh.ins[i].loop_size == 1) {
			switch (xxh->chn) {
			case 4:
		            tracker = "old Noisetracker/Octalyzer";
			    break;
			case 6:
			case 8:
		            tracker = "Octalyser";
			    break;
			default:
		            tracker = "Unknown. Email claudio!";
			}
		        goto skip_test;
		    }
	        }

		if (xxh->chn == 4) {
	    	    tracker = "Maybe Protracker";
		} else if (xxh->chn == 6 || xxh->chn == 8) {
	    	    tracker = "FastTracker 1.01?";
		} else {
	    	    tracker = "Unknown";
		}
	    }
	} else {	/* Has loops with 0 size */
	    for (i = 15; i < 31; i++) {
	        if (strlen((char *)mh.ins[i].name) || mh.ins[i].size > 0)
		    break;
	    }
	    if (i == 31 && is_st_ins((char *)mh.ins[14].name)) {
		tracker = "converted 15 instrument";
		goto skip_test;
	    }

	    /* Assume that Fast Tracker modules won't have ST- instruments */
	    for (i = 0; i < 31; i++) {
	        if (is_st_ins((char *)mh.ins[i].name))
		    break;
	    }
	    if (i < 31) {
		tracker = "Unknown (converted?)";
		goto skip_test;
	    }

	    if (xxh->chn == 4 || xxh->chn == 6 || xxh->chn == 8) {
	    	tracker = "Fast Tracker";
	        xxh->flg &= ~XXM_FLG_MODRNG;
		goto skip_test;
	    }

	    tracker = "No idea";		/* ??!? */
	}
    }

skip_test:

    xxh->trk = xxh->chn * xxh->pat;

    if (!ptdt) {
	snprintf(xmp_ctl->type, XMP_DEF_NAMESIZE, "%-.4s (%s)", mh.magic, id);
	strcpy(tracker_name, tracker);
	MODULE_INFO ();
    }

    if (V (1)) {
	report ("     Instrument name        Len  LBeg LEnd L Vol Fin\n");

	for (i = 0; i < xxh->ins; i++) {
	    if (V(3) || (strlen ((char *) xxih[i].name) || (xxs[i].len > 2)))
		report ("[%2X] %-22.22s %04x %04x %04x %c V%02x %+d\n",
		    i, xxih[i].name, xxs[i].len, xxs[i].lps,
		    xxs[i].lpe, (mh.ins[i].loop_size > 1 && xxs[i].lpe > 8) ?
		    'L' : ' ', xxi[i][0].vol, (char) xxi[i][0].fin >> 4);
	}
    }

    PATTERN_INIT ();

    /* Load and convert patterns */
    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	for (j = 0; j < (64 * xxh->chn); j++) {
	    event = &EVENT (i, j % xxh->chn, j / xxh->chn);
	    fread (mod_event, 1, 4, f);

	    cvt_pt_event (event, mod_event);

	    /* Special translation for e8 (set panning) effect.
	     * This is not an official Protracker effect but DMP uses
	     * it for panning, and a couple of modules follow this
	     * "standard".
	     */
	    if ((event->fxt == 0x0e) && ((event->fxp & 0xf0) == 0x80)) {
		event->fxt = FX_SETPAN;
		event->fxp <<= 4;
	    }
	}
	reportv(0, ".");
    }

    /* Load samples */

    if ((x = strrchr (xmp_ctl->filename, '/'))) {
	*x = 0;
	strcpy (pathname, xmp_ctl->filename);
	strcat (pathname, "/");
	*x = '/';
    }

    if (V (0))
	report ("\nStored samples : %d ", xxh->smp);
    for (i = 0; i < xxh->smp; i++) {
	if (!xxs[i].len)
	    continue;
	if (ptsong) {
	    FILE *s;
	    char sn[256];
	    snprintf(sn, XMP_DEF_NAMESIZE, "%s%s", pathname, xxih[i].name);
	
	    if ((s = fopen (sn, "rb"))) {
	        xmp_drv_loadpatch (s, xxi[i][0].sid, xmp_ctl->c4rate, 0,
		    &xxs[xxi[i][0].sid], NULL);
		if (V (0))
		    report (".");
	    }
	} else {
	    xmp_drv_loadpatch (f, xxi[i][0].sid, xmp_ctl->c4rate, 0,
	        &xxs[xxi[i][0].sid], NULL);
	    if (V (0))
		report (".");
	}
    }
    if (V (0))
	report ("\n");

    if (xxh->chn > 4)
	xmp_ctl->fetch |= XMP_MODE_FT2;

    return 0;
}
