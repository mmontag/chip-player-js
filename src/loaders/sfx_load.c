/* Extended Module Player
 * Copyright (C) 1996-2006 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: sfx_load.c,v 1.3 2006-06-28 12:20:38 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Reverse engineered from the two SFX files in the Delitracker mods disk
 * and music from Future Wars, Twinworld and Operation Stealth. Effects
 * must be verified/implemented.
 */

/* From the ExoticRipper docs:
 * [SoundFX 2.0 is] simply the same as SoundFX 1.3, except that it
 * uses 31 samples [instead of 15].
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "period.h"
#include "load.h"


struct sfx_ins {
    uint8 name[22];		/* Instrument name */
    uint16 len;			/* Sample length in words */
    uint8 finetune;		/* Finetune */
    uint8 volume;		/* Volume (0-63) */
    uint16 loop_start;		/* Sample loop start in bytes */
    uint16 loop_length;		/* Sample loop length in words */
};

struct sfx_header {
    uint8 magic[4];		/* 'SONG' */
    uint16 delay;		/* Delay value (tempo), default is 0x38e5 */
    uint16 unknown[7];		/* ? */
};

struct sfx_header2 {
    uint8 len;			/* Song length */
    uint8 restart;		/* Restart pos (?) */
    uint8 order[128];		/* Order list */
};


static int sfx_13_20_load (FILE *, int);


int sfx_load (FILE * f)
{
    if (sfx_13_20_load (f, 15) < 0)
	return sfx_13_20_load (f, 31);
    return 0;
}


static int sfx_13_20_load (FILE *f, int nins)
{
    int i, j;
    struct xxm_event *event;
    struct sfx_header sfx;
    struct sfx_header2 sfx2;
    uint8 ev[4];
    int ins_size[31];
    struct sfx_ins ins[31];	/* Instruments */

    LOAD_INIT ();

    for (i = 0; i < nins; i++)
	ins_size[i] = read32b(f);

    fread(&sfx.magic, 4, 1, f);
    sfx.delay = read16b(f);
    fread(&sfx.unknown, 14, 1, f);

    if (strncmp ((char *) sfx.magic, "SONG", 4))
	return -1;

    xxh->ins = nins;
    xxh->smp = xxh->ins;
    xxh->bpm = 14565 * 122 / sfx.delay;

    for (i = 0; i < xxh->ins; i++) {
	fread(&ins[i].name, 22, 1, f);
	ins[i].len = read16b(f);
	ins[i].finetune = read8(f);
	ins[i].volume = read8(f);
	ins[i].loop_start = read16b(f);
	ins[i].loop_length = read16b(f);
    }

    sfx2.len = read8(f);
    sfx2.restart = read8(f);
    fread(&sfx2.order, 128, 1, f);

    xxh->len = sfx2.len;
    if (xxh->len > 0x7f)
	return -1;

    memcpy (xxo, sfx2.order, xxh->len);
    for (xxh->pat = i = 0; i < xxh->len; i++)
	if (xxo[i] > xxh->pat)
	    xxh->pat = xxo[i];
    xxh->pat++;

    xxh->trk = xxh->chn * xxh->pat;

    strcpy (xmp_ctl->type, xxh->ins == 15 ? "SoundFX 1.3" : "SoundFX 2.0");

    MODULE_INFO ();

    INSTRUMENT_INIT ();

    if (V (1))
	report ("     Instrument name        Len  LBeg LEnd L Vol Fin\n");

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxih[i].nsm = !!(xxs[i].len = ins_size[i]);
	xxs[i].lps = ins[i].loop_start;
	xxs[i].lpe = xxs[i].lps + 2 * ins[i].loop_length;
	xxs[i].flg = ins[i].loop_length > 1 ? WAVE_LOOPING : 0;
	xxi[i][0].vol = ins[i].volume;
	xxi[i][0].fin = (int8)(ins[i].finetune << 4); 
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;

	copy_adjust(xxih[i].name, ins[i].name, 22);

	if ((V (1)) && (strlen ((char *) xxih[i].name) || (xxs[i].len > 2)))
	    report ("[%2X] %-22.22s %04x %04x %04x %c  %02x %+d\n",
		i, xxih[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe,
		xxs[i].flg & WAVE_LOOPING ? 'L' : ' ', xxi[i][0].vol,
		(char) xxi[i][0].fin >> 4);
    }

    PATTERN_INIT ();

    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	TRACK_ALLOC (i);

	for (j = 0; j < 64 * xxh->chn; j++) {
	    event = &EVENT (i, j % xxh->chn, j / xxh->chn);
	    fread (ev, 1, 4, f);

	    event->note = period_to_note ((LSN (ev[0]) << 8) | ev[1]);
	    event->ins = (MSN (ev[0]) << 4) | MSN (ev[2]);
	    event->fxp = ev[3];

	    switch (LSN (ev[2])) {
	    case 0x1:			/* Arpeggio */
		event->fxt = FX_ARPEGGIO;
		break;
	    case 0x02:			/* Pitch bend */
		if (event->fxp >> 4) {
		    event->fxt = FX_PORTA_DN;
		    event->fxp >>= 4;
		} else if (event->fxp & 0x0f) {
		    event->fxt = FX_PORTA_UP;
		    event->fxp &= 0x0f;
		}
		break;
	    case 0x5:			/* Volume up */
		event->fxt = FX_VOLSLIDE_DN;
		break;
	    case 0x6:			/* Set volume (attenuation) */
		event->fxt = FX_VOLSET;
		event->fxp = 0x40 - ev[3];
		break;
	    case 0x3:			/* LED on */
	    case 0x4:			/* LED off */
	    case 0x7:			/* Set step up */
	    case 0x8:			/* Set step down */
	    default:
		event->fxt = event->fxp = 0;
		break;
	    }
	}
	if (V (0))
	    report (".");
    }

    xxh->flg |= XXM_FLG_MODRNG;

    /* Read samples */

    if (V (0))
	report ("\nStored samples : %d ", xxh->smp);

    for (i = 0; i < xxh->ins; i++) {
	if (xxs[i].len <= 2)
	    continue;
	xmp_drv_loadpatch (f, i, xmp_ctl->c4rate, 0, &xxs[i], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    return 0;
}
