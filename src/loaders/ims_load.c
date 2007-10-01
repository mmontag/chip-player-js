/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: ims_load.c,v 1.4 2007-10-01 22:03:19 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Loader for Images Music System modules based on the EP replayer.
 *
 * Date: Thu, 19 Apr 2001 19:13:06 +0200
 * From: Michael Doering <mldoering@gmx.net>
 *
 * I just "stumbled" upon something about the Unic.3C format when I was
 * testing replayers for the upcoming UADE 0.21 that might be also
 * interesting to you for xmp. The "Beastbusters" tune is not a UNIC file :)
 * It's actually a different Format, although obviously related, called
 * "Images Music System".
 *
 * I was testing the replayer from the Wanted Team with one of their test
 * tunes, among them also the beastbuster music. When I first listened to
 * it, I knew I have heard it somewhere, a bit different but it was alike.
 * This one had more/richer percussions and there was no strange beep in
 * the bg. ;) After some searching on my HD I found it among the xmp test
 * tunes as a UNIC file.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <sys/types.h>

#include "load.h"
#include "period.h"


struct ims_instrument {
    uint8 name[20];
    int16 finetune;		/* Causes squeaks in beast-busters1! */
    uint16 size;
    uint8 unknown;
    uint8 volume;
    uint16 loop_start;
    uint16 loop_size;
};

struct ims_header {
    uint8 title[20];			/* LAX has no title */
    struct ims_instrument ins[31];
    uint8 len;
    uint8 zero;
    uint8 orders[128];
    uint8 magic[4];
};


int ims_load (FILE *f)
{
    int i, j;
    int smp_size;
    struct xxm_event *event;
    struct ims_header ih;
    uint8 ims_event[3];
    int xpo = 21;		/* Tuned against UADE */

    LOAD_INIT ();

    xxh->ins = 31;
    xxh->smp = xxh->ins;
    smp_size = 0;

    fread (&ih.title, 20, 1, f);		/* LAX has no title */

    for (i = 0; i < 31; i++) {
	fread (&ih.ins[i].name, 20, 1, f);
	ih.ins[i].finetune = read16b(f);	/* FIXME: int16 */
	ih.ins[i].size = read16b(f);
	ih.ins[i].unknown = read8(f);
	ih.ins[i].volume = read8(f);
	ih.ins[i].loop_start = read16b(f);
	ih.ins[i].loop_size = read16b(f);
    }

    ih.len = read8(f);
    ih.zero = read8(f);
    fread (&ih.orders, 128, 1, f);
    fread (&ih.magic, 4, 1, f);
  
    if (ih.magic[3] != 0x3c)
	return -1;

    if (ih.len > 0x7f)
	return -1;

    xxh->len = ih.len;
    memcpy (xxo, ih.orders, xxh->len);

    for (i = 0; i < xxh->len; i++)
	if (xxo[i] > xxh->pat)
	    xxh->pat = xxo[i];

    xxh->pat++;
    xxh->trk = xxh->chn * xxh->pat;

    if (1) {
	for (i = 0; i < 31; i++)
	    smp_size += ih.ins[i].size * 2;
	i = xxh->pat * 0x300 + smp_size + sizeof (struct ims_header);
	if ((xmp_ctl->size != (i - 4)) && (xmp_ctl->size != i))
	    return -1;

	if (xmp_ctl->size == i - 4) {		/* No magic */
	    fseek (f, -4, SEEK_CUR);
	    /* nomagic = 1; */
	}
    }

    /* Corrputed mod? */
    if (xxh->pat > 0x7f || xxh->len == 0 || xxh->len > 0x7f)
	return -1;

    strncpy (xmp_ctl->name, (char *) ih.title, 20);
    sprintf (xmp_ctl->type, "IMS (Images Music System)");

    MODULE_INFO ();

    INSTRUMENT_INIT ();

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxs[i].len = 2 * ih.ins[i].size;
	xxs[i].lpe = xxs[i].lps + 2 * ih.ins[i].loop_size;
	xxs[i].flg = ih.ins[i].loop_size > 1 ? WAVE_LOOPING : 0;
	xxi[i][0].fin = 0; /* ih.ins[i].finetune; */
	xxi[i][0].vol = ih.ins[i].volume;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	xxih[i].nsm = !!(xxs[i].len);
	xxih[i].rls = 0xfff;

	copy_adjust(xxih[i].name, ih.ins[i].name, 20);

	if (V (1) &&
		(strlen ((char *) xxih[i].name) || (xxs[i].len > 2))) {
	    report ("[%2X] %-20.20s %04x %04x %04x %c V%02x %+d\n",
		i, xxih[i].name, xxs[i].len, xxs[i].lps,
		xxs[i].lpe, ih.ins[i].loop_size > 1 ? 'L' : ' ',
		xxi[i][0].vol, (char) xxi[i][0].fin >> 4);
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
	for (j = 0; j < 0x100; j++) {
	    event = &EVENT (i, j & 0x3, j >> 2);
	    fread (ims_event, 1, 3, f);

	    /* Event format:
	     *
	     * 0000 0000  0000 0000  0000 0000
	     *  |\     /  \  / \  /  \       /
	     *  | note    ins   fx   parameter
	     * ins
	     *
	     * 0x3f is a blank note.
	     */
	    event->note = ims_event[0] & 0x3f;
	    if (event->note != 0x00 && event->note != 0x3f)
		event->note += xpo;
	    else
		event->note = 0;
	    event->ins = ((ims_event[0] & 0x40) >> 2) | MSN (ims_event[1]);
	    event->fxt = LSN (ims_event[1]);
	    event->fxp = ims_event[2];

	    disable_continue_fx (event);

	    /* According to Asle:
	     * ``Just note that pattern break effect command (D**) uses
	     * HEX value in UNIC format (while it is DEC values in PTK).
	     * Thus, it has to be converted!''
	     */
	    if (event->fxt == 0x0d)
		 event->fxp = (event->fxp / 10) << 4 | (event->fxp % 10);
	}

	if (V (0))
	    report (".");
    }

    xxh->flg |= XXM_FLG_MODRNG;

    /* Load samples */

    if (V (0))
	report ("\nStored samples : %d ", xxh->smp);
    for (i = 0; i < xxh->smp; i++) {
	if (!xxs[i].len)
	    continue;
	xmp_drv_loadpatch (f, xxi[i][0].sid, xmp_ctl->c4rate, 0,
	    &xxs[xxi[i][0].sid], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    return 0;
}
