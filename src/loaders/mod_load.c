/* Protracker module loader for xmp
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: mod_load.c,v 1.22 2007-10-15 19:19:21 cmatsuoka Exp $
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
    int flag;
    char *tracker;
    int ch;
} mod_magic[] = {
    { "M.K.", 0, "Protracker", 4 },
    { "M!K!", 1, "Protracker", 4 },
    { "M&K!", 1, "Noisetracker", 4 },
    { "N.T.", 1, "Noisetracker", 4 },
    { "6CHN", 0, "Fast Tracker", 6 },
    { "8CHN", 0, "Fast Tracker", 8 },
    { "CD61", 1, "Octalyser", 6 },		/* Atari STe/Falcon */
    { "CD81", 1, "Octalyser", 8 },		/* Atari STe/Falcon */
    { "FA04", 1, "Digital Tracker", 4 },	/* Atari Falcon */
    { "FA06", 1, "Digital Tracker", 6 },	/* Atari Falcon */
    { "FA08", 1, "Digital Tracker", 8 },	/* Atari Falcon */
    { "PWIZ", 1, "", 4 },
    { "", 0 }
};


static int mod_test (FILE *, char *);
static int mod_load (struct xmp_mod_context *, FILE *);

struct xmp_loader_info mod_loader = {
    "MOD",
    "Noise/Fast/Protracker",
    mod_test,
    mod_load
};

static int mod_test(FILE *f, char *t)
{
    int i;
    char buf[4];

    fseek(f, 1080, SEEK_SET);
    fread(buf, 4, 1, f);

    for (i = 0; mod_magic[i].ch; i++) {
	if (!memcmp(buf, mod_magic[i].magic, 4))
	    break;
    }
    if (mod_magic[i].ch == 0)
	return -1;

    fseek(f, 0, SEEK_SET);
    read_title(f, t, 20);

    return 0;
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


static int mod_load(struct xmp_mod_context *m, FILE *f)
{
    int i, j;
    int smp_size, pat_size, wow, ptsong = 0;
    struct xxm_event *event;
    struct mod_header mh;
    uint8 mod_event[4];
    char *x, pathname[256] = "", *tracker = "";
    int lps_mult = xmp_ctl->fetch & XMP_CTL_FIXLOOP ? 1 : 2;
    int detected = 0;
    char magic[8], idbuffer[32];

    LOAD_INIT();

    m->xxh->ins = 31;
    m->xxh->smp = m->xxh->ins;
    m->xxh->chn = 0;
    smp_size = 0;
    pat_size = 0;

    m->xxh->flg |= XXM_FLG_MODRNG;

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
    memset(magic, 0, 8);
    fread(magic, 4, 1, f);

    for (i = 0; mod_magic[i].ch; i++) {
	if (!(strncmp (magic, mod_magic[i].magic, 4))) {
	    m->xxh->chn = mod_magic[i].ch;
	    tracker = mod_magic[i].tracker;
	    detected = mod_magic[i].flag;
	    break;
	}
    }

    /* Prowizard hack */
    if (memcmp(magic, "PWIZ", 4) == 0) {
	int pos = ftell(f);
	fseek(f, -40, SEEK_END);
	fread(magic, 8, 1, f);
	fread(idbuffer, 1, 32, f);
	fseek(f, pos, SEEK_SET);
	tracker = idbuffer;
    } else if (!m->xxh->chn) {
	if (!strncmp(magic + 2, "CH", 2) &&
	    isdigit(*magic) && isdigit(magic[1])) {
	    if ((m->xxh->chn = (*magic - '0') *
		10 + magic[1] - '0') > 32)
		return -1;
	} else if (!strncmp(magic + 1, "CHN", 3) &&
	    isdigit(*magic)) {
	    if (!(m->xxh->chn = (*magic - '0')))
		return -1;
	} else
	    return -1;
	tracker = "TakeTracker/FastTracker II";
	detected = 1;
	m->xxh->flg &= ~XXM_FLG_MODRNG;
    }

    strncpy (xmp_ctl->name, (char *) mh.name, 20);

    m->xxh->len = mh.len;
    /* m->xxh->rst = mh.restart; */

    if (m->xxh->rst >= m->xxh->len)
	m->xxh->rst = 0;
    memcpy(m->xxo, mh.order, 128);

    for (i = 0; i < 128; i++) {
	/* This fixes dragnet.mod (garbage in the order list) */
	if (m->xxo[i] > 0x7f)
		break;
	if (m->xxo[i] > m->xxh->pat)
	    m->xxh->pat = m->xxo[i];
    }
    m->xxh->pat++;

    pat_size = 256 * m->xxh->chn * m->xxh->pat;

    INSTRUMENT_INIT();

    for (i = 0; i < m->xxh->ins; i++) {
	smp_size += 2 * mh.ins[i].size;

	m->xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	m->xxs[i].len = 2 * mh.ins[i].size;
	m->xxs[i].lps = lps_mult * mh.ins[i].loop_start;
	m->xxs[i].lpe = m->xxs[i].lps + 2 * mh.ins[i].loop_size;
	m->xxs[i].flg = (mh.ins[i].loop_size > 1 && m->xxs[i].lpe > 8) ?
		WAVE_LOOPING : 0;
	m->xxi[i][0].fin = (int8)(mh.ins[i].finetune << 4);
	m->xxi[i][0].vol = mh.ins[i].volume;
	m->xxi[i][0].pan = 0x80;
	m->xxi[i][0].sid = i;
	m->xxih[i].nsm = !!(m->xxs[i].len);
	copy_adjust(m->xxih[i].name, mh.ins[i].name, 22);
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

    if (0x43c + m->xxh->pat * 4 * m->xxh->chn * 0x40 + smp_size < xmp_ctl->size) {
	int pos = ftell(f);
	fseek(f, 0x43c + m->xxh->pat * 4 * m->xxh->chn * 0x40 + smp_size, SEEK_SET);
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

    if ((wow = (!strncmp(magic, "M.K.", 4) &&
		(0x43c + m->xxh->pat * 32 * 0x40 + smp_size == xmp_ctl->size)))) {
	m->xxh->chn = 8;
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

    else if (sizeof (struct mod_header) + m->xxh->pat * 0x300 +
	smp_size == xmp_ctl->size) {
	return -1;
    }

    /* Test for Protracker song files
     */
    else if ((ptsong = (!strncmp((char *)magic, "M.K.", 4) &&
		(0x43c + m->xxh->pat * 0x400 == xmp_ctl->size)))) {
	tracker = "Protracker";
	goto skip_test;
    }

    /* Test Protracker-like files
     */
    if (m->xxh->chn == 4 && mh.restart == m->xxh->pat) {
	tracker = "Soundtracker";
    } else if (m->xxh->chn == 4 && mh.restart == 0x78) {
	tracker = "Noisetracker" /*" (0x78)"*/;
    } else if (mh.restart < 0x7f) {
	if (m->xxh->chn == 4)
	    tracker = "Noisetracker";
	else
	    tracker = "unknown tracker";
	m->xxh->rst = mh.restart;
    }

    if (m->xxh->chn != 4 && mh.restart == 0x7f)
	tracker = "unknown tracker";

    if (m->xxh->chn == 4 && mh.restart == 0x7f) {
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
			switch (m->xxh->chn) {
			case 4:
		            tracker = "old Noisetracker/Octalyzer";
			    break;
			case 6:
			case 8:
		            tracker = "Octalyser";
			    break;
			default:
		            tracker = "unknown tracker";
			}
		        goto skip_test;
		    }
	        }

		if (m->xxh->chn == 4) {
	    	    tracker = "Maybe Protracker";
		} else if (m->xxh->chn == 6 || m->xxh->chn == 8) {
	    	    tracker = "FastTracker 1.01?";
		} else {
	    	    tracker = "unknown tracker";
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
		tracker = "unknown/converted";
		goto skip_test;
	    }

	    if (m->xxh->chn == 4 || m->xxh->chn == 6 || m->xxh->chn == 8) {
	    	tracker = "Fast Tracker";
	        m->xxh->flg &= ~XXM_FLG_MODRNG;
		goto skip_test;
	    }

	    tracker = "unknown tracker";		/* ??!? */
	}
    }

skip_test:

    m->xxh->trk = m->xxh->chn * m->xxh->pat;

    snprintf(xmp_ctl->type, XMP_DEF_NAMESIZE, "%s (%s)", magic, tracker);
    MODULE_INFO();

    if (V (1)) {
	report ("     Instrument name        Len  LBeg LEnd L Vol Fin\n");

	for (i = 0; i < m->xxh->ins; i++) {
	    if (V(3) || (strlen ((char *) m->xxih[i].name) || (m->xxs[i].len > 2)))
		report ("[%2X] %-22.22s %04x %04x %04x %c V%02x %+d\n",
		    i, m->xxih[i].name, m->xxs[i].len, m->xxs[i].lps,
		    m->xxs[i].lpe, (mh.ins[i].loop_size > 1 && m->xxs[i].lpe > 8) ?
		    'L' : ' ', m->xxi[i][0].vol, (char) m->xxi[i][0].fin >> 4);
	}
    }

    PATTERN_INIT();

    /* Load and convert patterns */
    if (V (0))
	report ("Stored patterns: %d ", m->xxh->pat);

    for (i = 0; i < m->xxh->pat; i++) {
	PATTERN_ALLOC (i);
	m->xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	for (j = 0; j < (64 * m->xxh->chn); j++) {
	    event = &EVENT (i, j % m->xxh->chn, j / m->xxh->chn);
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
	report ("\nStored samples : %d ", m->xxh->smp);
    for (i = 0; i < m->xxh->smp; i++) {
	if (!m->xxs[i].len)
	    continue;
	if (ptsong) {
	    FILE *s;
	    char sn[256];
	    snprintf(sn, XMP_DEF_NAMESIZE, "%s%s", pathname, m->xxih[i].name);
	
	    if ((s = fopen (sn, "rb"))) {
	        xmp_drv_loadpatch (s, m->xxi[i][0].sid, xmp_ctl->c4rate, 0,
		    &m->xxs[m->xxi[i][0].sid], NULL);
		if (V (0))
		    report (".");
	    }
	} else {
	    xmp_drv_loadpatch (f, m->xxi[i][0].sid, xmp_ctl->c4rate, 0,
	        &m->xxs[m->xxi[i][0].sid], NULL);
	    if (V (0))
		report (".");
	}
    }
    if (V (0))
	report ("\n");

    if (m->xxh->chn > 4)
	xmp_ctl->fetch |= XMP_MODE_FT2;

    return 0;
}
