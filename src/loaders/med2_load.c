/* Extended Module Player
 * Copyright (C) 1996-2001 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: med2_load.c,v 1.1 2001-06-02 20:27:00 cmatsuoka Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "med2.h"


int med2_load (FILE *f)
{
    int i /* , j */;
    //struct xxm_event *event;
    struct Song m2h;
    char id[4], *nameptr;

    LOAD_INIT ();

    fread (id, 1, 4, f);
    if (id[0] != 'M' || id[1] != 'E' || id[2] != 'D')
	return -1;

    switch (id[3]) {
    case 0x02:
	strcpy (xmp_ctl->type, "MED2");
	strcpy (tracker_name, "MED 1.11/1.12");
        fread (&m2h, 1, 32 * 40 + 32 + 64 + 64, f);
	goto header_2;
	break;
    case 0x03:
	strcpy (xmp_ctl->type, "MED3");
	strcpy (tracker_name, "MED 2.00");
	break;
    default:
	return -1;
    }

    /* read instrument names */
    for (i = 0; i < 32; i++) {
	nameptr = &m2h.instrument[i][0];
	//fread (nameptr, 1, 1, f);
	for(; ; ) {
	    fread (nameptr, 1, 1, f);
	    if (!*nameptr++)
		break;
	}
    }

    /* read instrument volumes */
    MED2_MASK_LOAD (32, m2h.instrument_volume, 1, f);

    /* read instrument loop start */
    MED2_MASK_LOAD (32, m2h.loop, 2, f);

    /* read instrument loop length */
    MED2_MASK_LOAD (32, m2h.loop_length, 2, f);

header_2:

    fread (&m2h.patterns, 1, 2, f);
    fread (&m2h.song_length, 1, 2, f);
    B_ENDIAN16 (m2h.patterns);
    B_ENDIAN16 (m2h.song_length);

    fread (&m2h.sequence, 1, m2h.song_length, f);
    fread (&m2h.tempo, 1, 2, f);
    B_ENDIAN16 (m2h.tempo);

#if 0
    fread (&m2h.flags, 1, 1, f);
    fread (&m2h.sliding, 1, 2, f);
    fread (&m2h.jump_mask, 1, 2, f);
    B_ENDIAN16 (m2h.sliding);
    B_ENDIAN32 (m2h.jump_mask);
#endif

    xxh->ins = 32;
    xxh->len = m2h.song_length;
    xxh->pat = m2h.patterns;
    memcpy (xxo, m2h.sequence, xxh->len);
    xxh->trk = xxh->chn * xxh->pat;

    //strncpy (xmp_ctl->name, (char *) mh.name, 20);
    MODULE_INFO ();

    INSTRUMENT_INIT ();

    if (V (1))
	report ("     Instrument name                  Len  LBeg LEnd L Vol Fin\n");

    for (i = 0; i < xxh->ins; i++) {
	B_ENDIAN16 (m2h.loop[i]);
	B_ENDIAN16 (m2h.loop_length[i]);

	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxs[i].len = m2h.loop[i] + m2h.loop_length[i];
	xxs[i].lps = m2h.loop[i];
	xxs[i].lpe = m2h.loop[i] + m2h.loop_length[i];
	xxs[i].flg = m2h.loop_length[i] > 1 ? WAVE_LOOPING : 0;
	xxi[i][0].fin = 0;
	xxi[i][0].vol = m2h.instrument_volume[i];
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	xxih[i].nsm = !!(xxs[i].len);
	xxih[i].rls = 0xfff;
	strncpy (xxih[i].name, m2h.instrument[i], 32);
	str_adj (xxih[i].name);

	if ((V (1)) && (strlen ((char *) xxih[i].name) ||
	    xxs[i].len > 2))
	    report ("[%2X] %-32.32s %04x %04x %04x %c V%02x %+d\n",
		i, xxih[i].name, xxs[i].len, xxs[i].lps,
		xxs[i].lpe, xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
		xxi[i][0].vol, (char) xxi[i][0].fin >> 4);
    }

    PATTERN_INIT ();

    /* Load and convert patterns */
    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

#if 0
    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	for (j = 0; j < (64 * 4); j++) {
	    event = &EVENT (i, j % 4, j / 4);
	    fread (mod_event, 1, 4, f);
	    cvt_pt_event (event, mod_event);
	}
	if (xxh->chn > 4) {
	    for (j = 0; j < (64 * 4); j++) {
		event = &EVENT (i, (j % 4) + 4, j / 4);
		fread (mod_event, 1, 4, f);
		cvt_pt_event (event, mod_event);
	    }
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
#endif

    return 0;
}
