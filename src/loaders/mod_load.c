/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
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
 * - TakeTracker TDZ4
 * - (unknown) NSMS
 */

#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include "loader.h"
#include "mod.h"

struct mod_magic {
    char *magic;
    int flag;
    int id;
    int ch;
};

#define TRACKER_PROTRACKER	0
#define TRACKER_NOISETRACKER	1
#define TRACKER_SOUNDTRACKER	2
#define TRACKER_FASTTRACKER	3
#define TRACKER_FASTTRACKER2	4
#define TRACKER_OCTALYSER	5
#define TRACKER_TAKETRACKER	6
#define TRACKER_DIGITALTRACKER	7
#define TRACKER_FLEXTRAX	8
#define TRACKER_MODSGRAVE	9
#define TRACKER_SCREAMTRACKER3	10
#define TRACKER_UNKNOWN_CONV	95
#define TRACKER_CONVERTEDST	96
#define TRACKER_CONVERTED	97
#define TRACKER_CLONE		98
#define TRACKER_UNKNOWN		99

const struct mod_magic mod_magic[] = {
    { "M.K.", 0, TRACKER_PROTRACKER, 4 },
    { "M!K!", 1, TRACKER_PROTRACKER, 4 },
    { "M&K!", 1, TRACKER_NOISETRACKER, 4 },
    { "N.T.", 1, TRACKER_NOISETRACKER, 4 },
    { "6CHN", 0, TRACKER_FASTTRACKER, 6 },
    { "8CHN", 0, TRACKER_FASTTRACKER, 8 },
    { "CD61", 1, TRACKER_OCTALYSER, 6 },	/* Atari STe/Falcon */
    { "CD81", 1, TRACKER_OCTALYSER, 8 },	/* Atari STe/Falcon */
    { "TDZ4", 1, TRACKER_TAKETRACKER, 4 },	/* see XModule SaveTracker.c */
    { "FA04", 1, TRACKER_DIGITALTRACKER, 4 },	/* Atari Falcon */
    { "FA06", 1, TRACKER_DIGITALTRACKER, 6 },	/* Atari Falcon */
    { "FA08", 1, TRACKER_DIGITALTRACKER, 8 },	/* Atari Falcon */
    { "NSMS", 1, TRACKER_UNKNOWN, 4 },		/* in Kingdom.mod */
    { "", 0 }
};


static int mod_test (HIO_HANDLE *, char *, const int);
static int mod_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader mod_loader = {
    "Amiga Protracker/Compatible",
    mod_test,
    mod_load
};

static int mod_test(HIO_HANDLE *f, char *t, const int start)
{
    int i;
    char buf[4];
    struct stat st;
    int smp_size, num_pat;

    hio_seek(f, start + 1080, SEEK_SET);
    if (hio_read(buf, 1, 4, f) < 4)
	return -1;

    if (!strncmp(buf + 2, "CH", 2) && isdigit((int)buf[0]) && isdigit((int)buf[1])) {
	i = (buf[0] - '0') * 10 + buf[1] - '0';
	if (i > 0 && i <= 32) {
	    goto found;
	}
    }

    if (!strncmp(buf + 1, "CHN", 3) && isdigit((int)*buf)) {
	if (*buf - '0') {
	    goto found;
	}
    }

    for (i = 0; mod_magic[i].ch; i++) {
	if (!memcmp(buf, mod_magic[i].magic, 4))
	    break;
    }
    if (mod_magic[i].ch == 0)
	return -1;

    /*
     * Sanity check to prevent loading NoiseRunner and other module
     * formats with valid magic at offset 1080
     */

    hio_seek(f, start + 20, SEEK_SET);
    for (i = 0; i < 31; i++) {
	hio_seek(f, 22, SEEK_CUR);			/* Instrument name */
	if (hio_read16b(f) & 0x8000)		/* test length */
		return -1;
	if (hio_read8(f) & 0xf0)			/* test finetune */
		return -1;
	if (hio_read8(f) > 0x40)			/* test volume */
		return -1;
	if (hio_read16b(f) & 0x8000)		/* test loop start */
		return -1;
	if (hio_read16b(f) & 0x8000)		/* test loop size */
		return -1;
    }

    if (HIO_HANDLE_TYPE(f) != HIO_HANDLE_TYPE_FILE)
	goto found;

    /* Test for UNIC tracker modules
     *
     * From Gryzor's Pro-Wizard PW_FORMATS-Engl.guide:
     * ``The UNIC format is very similar to Protracker... At least in the
     * heading... same length : 1084 bytes. Even the "M.K." is present,
     * sometimes !! Maybe to disturb the rippers.. hehe but Pro-Wizard
     * doesn't test this only!''
     */

    /* get file size */
    hio_stat(f, &st);
    smp_size = 0;
    hio_seek(f, start + 20, SEEK_SET);

    /* get samples size */
    for (i = 0; i < 31; i++) {
	hio_seek(f, 22, SEEK_CUR);
	smp_size += 2 * hio_read16b(f);		/* Length in 16-bit words */
	hio_seek(f, 6, SEEK_CUR);
    } 

    /* get number of patterns */
    num_pat = 0;
    hio_seek(f, start + 952, SEEK_SET);
    for (i = 0; i < 128; i++) {
	uint8 x = hio_read8(f);
	if (x > 0x7f)
		break;
	if (x > num_pat)
	    num_pat = x;
    }
    num_pat++;

    if (start + 1084 + num_pat * 0x300 + smp_size == st.st_size)
	return -1;

  found:
    hio_seek(f, start + 0, SEEK_SET);
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
    if (!isdigit((int)s[3]) || !isdigit((int)s[4]))
	return 0;

    return 1;
}


static int mod_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
    struct xmp_module *mod = &m->mod;
    int i, j;
    int smp_size, pat_size, wow, ptsong = 0;
    struct xmp_event *event;
    struct mod_header mh;
    uint8 mod_event[4];
    char *x, pathname[PATH_MAX] = "", *tracker = "";
    int detected = 0;
    char magic[8], idbuffer[32];
    int ptkloop = 0;			/* Protracker loop */
    int tracker_id = TRACKER_PROTRACKER;

    LOAD_INIT();

    mod->ins = 31;
    mod->smp = mod->ins;
    mod->chn = 0;
    smp_size = 0;
    pat_size = 0;

    m->quirk |= QUIRK_MODRNG;

    hio_read(&mh.name, 20, 1, f);
    for (i = 0; i < 31; i++) {
	hio_read(&mh.ins[i].name, 22, 1, f);	/* Instrument name */
	mh.ins[i].size = hio_read16b(f);	/* Length in 16-bit words */
	mh.ins[i].finetune = hio_read8(f);	/* Finetune (signed nibble) */
	mh.ins[i].volume = hio_read8(f);	/* Linear playback volume */
	mh.ins[i].loop_start = hio_read16b(f);	/* Loop start in 16-bit words */
	mh.ins[i].loop_size = hio_read16b(f);	/* Loop size in 16-bit words */

	smp_size += 2 * mh.ins[i].size;
    }
    mh.len = hio_read8(f);
    mh.restart = hio_read8(f);
    hio_read(&mh.order, 128, 1, f);
    memset(magic, 0, 8);
    hio_read(magic, 4, 1, f);

    for (i = 0; mod_magic[i].ch; i++) {
	if (!(strncmp (magic, mod_magic[i].magic, 4))) {
	    mod->chn = mod_magic[i].ch;
	    tracker_id = mod_magic[i].id;
	    detected = mod_magic[i].flag;
	    break;
	}
    }

    if (!mod->chn) {
	if (!strncmp(magic + 2, "CH", 2) &&
	    isdigit((int)magic[0]) && isdigit((int)magic[1])) {
	    mod->chn = (*magic - '0') * 10 + magic[1] - '0';
	} else if (!strncmp(magic + 1, "CHN", 3) && isdigit((int)*magic)) {
	    mod->chn = *magic - '0';
	} else {
	    return -1;
	}
	tracker_id = mod->chn & 1 ? TRACKER_TAKETRACKER : TRACKER_FASTTRACKER2;
	detected = 1;
	m->quirk &= ~QUIRK_MODRNG;
    }

    strncpy(mod->name, (char *) mh.name, 20);

    mod->len = mh.len;
    /* mod->rst = mh.restart; */

    if (mod->rst >= mod->len)
	mod->rst = 0;
    memcpy(mod->xxo, mh.order, 128);

    for (i = 0; i < 128; i++) {
	/* This fixes dragnet.mod (garbage in the order list) */
	if (mod->xxo[i] > 0x7f)
		break;
	if (mod->xxo[i] > mod->pat)
	    mod->pat = mod->xxo[i];
    }
    mod->pat++;

    pat_size = 256 * mod->chn * mod->pat;

    if (instrument_init(mod) < 0)
	return -1;

    for (i = 0; i < mod->ins; i++) {
	if (subinstrument_alloc(mod, i, 1) < 0)
	    return -1;

	mod->xxs[i].len = 2 * mh.ins[i].size;
	mod->xxs[i].lps = 2 * mh.ins[i].loop_start;
	mod->xxs[i].lpe = mod->xxs[i].lps + 2 * mh.ins[i].loop_size;
	if (mod->xxs[i].lpe > mod->xxs[i].len)
		mod->xxs[i].lpe = mod->xxs[i].len;
	mod->xxs[i].flg = (mh.ins[i].loop_size > 1 && mod->xxs[i].lpe > 8) ?
		XMP_SAMPLE_LOOP : 0;
	mod->xxi[i].sub[0].fin = (int8)(mh.ins[i].finetune << 4);
	mod->xxi[i].sub[0].vol = mh.ins[i].volume;
	mod->xxi[i].sub[0].pan = 0x80;
	mod->xxi[i].sub[0].sid = i;
	instrument_name(mod, i, mh.ins[i].name, 22);

	if (mod->xxs[i].len > 0)
		mod->xxi[i].nsm = 1;
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

    if (0x43c + mod->pat * 4 * mod->chn * 0x40 + smp_size < m->size) {
	int pos = hio_tell(f);
	hio_seek(f, start + 0x43c + mod->pat * 4 * mod->chn * 0x40 + smp_size, SEEK_SET);
	hio_read(idbuffer, 1, 4, f);
	hio_seek(f, start + pos, SEEK_SET);

	if (!memcmp(idbuffer, "FLEX", 4)) {
	    tracker_id = TRACKER_FLEXTRAX;
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
		(0x43c + mod->pat * 32 * 0x40 + smp_size == m->size)))) {
	mod->chn = 8;
	tracker_id = TRACKER_MODSGRAVE;
	goto skip_test;
    }

    /* Test for Protracker song files
     */
    else if ((ptsong = (!strncmp((char *)magic, "M.K.", 4) &&
		(0x43c + mod->pat * 0x400 == m->size)))) {
	tracker_id = TRACKER_PROTRACKER;
	goto skip_test;
    }

    /* Test Protracker-like files
     */
    if (mod->chn == 4 && mh.restart == mod->pat) {
	tracker_id = TRACKER_SOUNDTRACKER;
    } else if (mod->chn == 4 && mh.restart == 0x78) {
        tracker_id = TRACKER_NOISETRACKER;
    } else if (mh.restart < 0x7f) {
	if (mod->chn == 4) {
	    tracker_id = TRACKER_NOISETRACKER;
	} else {
	    tracker_id = TRACKER_UNKNOWN;
	}
	mod->rst = mh.restart;
    }

    if (mod->chn != 4 && mh.restart == 0x7f) {
	tracker_id = TRACKER_SCREAMTRACKER3;
	m->quirk &= ~QUIRK_MODRNG;
	m->read_event_type = READ_EVENT_ST3;
    }

    if (mod->chn == 4 && mh.restart == 0x7f) {
	for (i = 0; i < 31; i++) {
	    if (mh.ins[i].loop_size == 0)
		break;
	}
	if (i < 31) {
	    tracker_id = TRACKER_CLONE;
	}
    }

    if (mh.restart != 0x78 && mh.restart < 0x7f) {
	for (i = 0; i < 31; i++) {
	    if (mh.ins[i].loop_size == 0)
		break;
	}
	if (i == 31) {	/* All loops are size 2 or greater */
	    for (i = 0; i < 31; i++) {
		if (mh.ins[i].size == 1 && mh.ins[i].volume == 0) {
		    tracker_id = TRACKER_CONVERTED;
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
			switch (mod->chn) {
			case 4:
			    tracker_id = TRACKER_NOISETRACKER;	/* or Octalyser */
			    break;
			case 6:
			case 8:
		            tracker_id = TRACKER_OCTALYSER;
			    break;
			default:
		            tracker_id = TRACKER_UNKNOWN;
			}
		        goto skip_test;
		    }
	        }

		if (mod->chn == 4) {
	    	    tracker_id = TRACKER_PROTRACKER;
		} else if (mod->chn == 6 || mod->chn == 8) {
	    	    tracker_id = TRACKER_FASTTRACKER;	/* FastTracker 1.01? */
		    m->quirk &= ~QUIRK_MODRNG;
		} else {
	    	    tracker_id = TRACKER_UNKNOWN;
		}
	    }
	} else {	/* Has loops with 0 size */
	    for (i = 15; i < 31; i++) {
	        if (strlen((char *)mh.ins[i].name) || mh.ins[i].size > 0)
		    break;
	    }
	    if (i == 31 && is_st_ins((char *)mh.ins[14].name)) {
		tracker_id = TRACKER_CONVERTEDST;
		goto skip_test;
	    }

	    /* Assume that Fast Tracker modules won't have ST- instruments */
	    for (i = 0; i < 31; i++) {
	        if (is_st_ins((char *)mh.ins[i].name))
		    break;
	    }
	    if (i < 31) {
		tracker_id = TRACKER_UNKNOWN_CONV;
		goto skip_test;
	    }

	    if (mod->chn == 4 || mod->chn == 6 || mod->chn == 8) {
	    	tracker_id = TRACKER_FASTTRACKER;
	        m->quirk &= ~QUIRK_MODRNG;
		goto skip_test;
	    }

	    tracker_id = TRACKER_UNKNOWN;	/* ??!? */
	}
    }

skip_test:

    switch (tracker_id) {
    case TRACKER_PROTRACKER:
	tracker = "Protracker";
	ptkloop = 1;
	break;
    case TRACKER_NOISETRACKER:
	tracker = "Noisetracker";
	ptkloop = 1;
	break;
    case TRACKER_SOUNDTRACKER:
	tracker = "Soundtracker";
	ptkloop = 1;
	break;
    case TRACKER_FASTTRACKER:
	tracker = "Fast Tracker";
	break;
    case TRACKER_FASTTRACKER2:
	tracker = "FastTracker 2";
	break;
    case TRACKER_OCTALYSER:
	tracker = "Octalyser";
	break;
    case TRACKER_TAKETRACKER:
	tracker = "TakeTracker";
	break;
    case TRACKER_DIGITALTRACKER:
	tracker = "Digital Tracker";
	break;
    case TRACKER_FLEXTRAX:
	tracker = "Flextrax";
	break;
    case TRACKER_MODSGRAVE:
	tracker = "Mod's Grave";
	break;
    case TRACKER_SCREAMTRACKER3:
	tracker = "Scream Tracker III";
	break;
    case TRACKER_UNKNOWN_CONV:
	tracker = "unknown or converted";
	break;
    case TRACKER_CONVERTEDST:
	tracker = "converted ST2.2 or earlier";
	break;
    case TRACKER_CONVERTED:
	tracker = "converted";
	break;
    case TRACKER_CLONE:
	tracker = "Protracker clone";
	break;
    default:
    case TRACKER_UNKNOWN:
	tracker = "unknown";
	break;
    }

    mod->trk = mod->chn * mod->pat;

    snprintf(mod->type, XMP_NAME_SIZE, "%s (%s)", tracker, magic);
    MODULE_INFO();

    for (i = 0; i < mod->ins; i++) {
	D_(D_INFO "[%2X] %-22.22s %04x %04x %04x %c V%02x %+d %c\n",
		i, mod->xxi[i].name,
		mod->xxs[i].len, mod->xxs[i].lps, mod->xxs[i].lpe,
		(mh.ins[i].loop_size > 1 && mod->xxs[i].lpe > 8) ?
			'L' : ' ', mod->xxi[i].sub[0].vol,
		mod->xxi[i].sub[0].fin >> 4,
		ptkloop && mod->xxs[i].lps == 0 && mh.ins[i].loop_size > 1 &&
			mod->xxs[i].len > mod->xxs[i].lpe ? '!' : ' ');
    }

    if (pattern_init(mod) < 0)
	return -1;

    /* Load and convert patterns */
    D_(D_INFO "Stored patterns: %d", mod->pat);

    for (i = 0; i < mod->pat; i++) {
	if (pattern_tracks_alloc(mod, i, 64) < 0)
	    return -1;

	for (j = 0; j < (64 * mod->chn); j++) {
	    event = &EVENT (i, j % mod->chn, j / mod->chn);
	    hio_read (mod_event, 1, 4, f);

	    switch (tracker_id) {
	    case TRACKER_NOISETRACKER:
		decode_noisetracker_event(event, mod_event);
		break;
	    case TRACKER_PROTRACKER:
	    default:
		decode_protracker_event(event, mod_event);
		break;
	    }
	}
    }

    /* Load samples */

    if (m->filename && (x = strrchr(m->filename, '/')))
	strncpy(pathname, m->filename, x - m->filename);

    D_(D_INFO "Stored samples: %d", mod->smp);

    for (i = 0; i < mod->smp; i++) {
	int flags;

	if (!mod->xxs[i].len)
	    continue;

	flags = ptkloop ? SAMPLE_FLAG_FULLREP : 0;

	if (ptsong) {
	    HIO_HANDLE *s;
	    char sn[256];
	    snprintf(sn, XMP_NAME_SIZE, "%s%s", pathname, mod->xxi[i].name);
	
	    if ((s = hio_open_file(sn, "rb"))) {
	        if (load_sample(m, s, flags, &mod->xxs[i], NULL) < 0) {
		    hio_close(s);
		    return -1;
		}
		hio_close(s);
	    }
	} else {
	    if (load_sample(m, f, flags, &mod->xxs[i], NULL) < 0)
		return -1;
	}
    }

    if (mod->chn > 4) {
	m->quirk &= ~QUIRK_MODRNG;
	m->quirk |= QUIRKS_FT2;
	m->read_event_type = READ_EVENT_FT2;
    } else if (strcmp(tracker, "Protracker") == 0) {
	m->quirk |= QUIRK_INVLOOP;
    }

    return 0;
}
