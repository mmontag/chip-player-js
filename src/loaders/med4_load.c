/* Extended Module Player
 * Copyright (C) 1996-2001 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: med4_load.c,v 1.1 2001-06-02 20:27:04 cmatsuoka Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "med2.h"


int med4_load (FILE *f)
{
    int i;
    //struct xxm_event *event;
    struct Song m3h;
    char id[4], *nameptr;
    int x, len;

    LOAD_INIT ();

    fread (id, 1, 4, f);
    if (id[0] != 'M' || id[1] != 'E' || id[2] != 'D' || id[3] != 4)
	return -1;

    strcpy (xmp_ctl->type, "MED4");
    strcpy (tracker_name, "MED 3.xx");

    /* read instrument names */

    memset (&m3h, 0, sizeof (m3h));

    for (x = fgetc (f); x; x <<= 1) {		/* mask */
	if (x & 0x80)
	    fgetc (f);
    }

    for (xxh->ins = i = 0; ; i++, xxh->ins++) {
        x = fgetc (f);

	if (x == 0)
	    break;

        len = fgetc (f);
        nameptr = &m3h.instrument[i][0];
        for(; len--; ) { 
            fread (nameptr, 1, 1, f);
            if (!*nameptr++)
                break;
        }

	if (x & 0x01)
	    m3h.loop[i] = 0;
	else
	    fread (&m3h.loop[i], 1, 2, f);

	if (x & 0x02)
	    m3h.loop_length[i] = 0;
	else
	    fread (&m3h.loop_length[i], 1, 2, f);

	if (x & 0x20)
	    m3h.instrument_volume[i] = 0x40;
	else
	    m3h.instrument_volume[i] = fgetc (f);

	switch (x) {
	case 0x4c:
	case 0x4f:
	case 0x6c:
	case 0x6f:
	    break;
	default:
	    printf ("Error: x = %02x\n", x);
	    exit (-1);
	}
    }
    ungetc (x, f);

    fread (&m3h.patterns, 1, 2, f);
    fread (&m3h.song_length, 1, 2, f);
    B_ENDIAN16 (m3h.patterns);
    B_ENDIAN16 (m3h.song_length);

    fread (&m3h.sequence, 1, m3h.song_length, f);
    fread (&m3h.tempo, 1, 2, f);
    B_ENDIAN16 (m3h.tempo);

    for (i = 0; i < 5 * 2; i++)		/* unknown */
	x = fgetc (f);

    fread (&m3h.rgb, 2, 8, f);		/* rgb info */

    for (i = 0; i < 16; i++)		/* relvols */
	x = fgetc (f);

    x = fgetc (f);			/* master vol */
    x = fgetc (f);			/* unknown */

    for (i = 0; i < 3 * 2; i++)		/* unknown */
	x = fgetc (f);

#if 0
    if (id[3] > 0x02) {
	fread (&m3h.playtransp, 1, 1, f);
    } else {
	m3h.playtransp = 0;
    }
    fread (&m3h.flags, 1, 1, f);
    fread (&m3h.sliding, 1, 2, f);
    fread (&m3h.jump_mask, 1, 2, f);
    B_ENDIAN16 (m3h.sliding);
    B_ENDIAN32 (m3h.jump_mask);
#endif

    xxh->len = m3h.song_length;
    xxh->pat = m3h.patterns;
    memcpy (xxo, m3h.sequence, xxh->len);
    xxh->trk = xxh->chn * xxh->pat;

    //strncpy (xmp_ctl->name, (char *) mh.name, 20);
    MODULE_INFO ();

    INSTRUMENT_INIT ();

    if (V (1))
	report ("     Instrument name                  Len  LBeg LEnd L Vol Fin\n");

    for (i = 0; i < xxh->ins; i++) {
	B_ENDIAN16 (m3h.loop[i]);
	B_ENDIAN16 (m3h.loop_length[i]);

	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxs[i].len = 42;
	xxs[i].lps = 2 * m3h.loop[i];
	xxs[i].lpe = 2 * (m3h.loop[i] + m3h.loop_length[i]);
	xxs[i].flg = m3h.loop_length[i] > 1 ? WAVE_LOOPING : 0;
	xxi[i][0].fin = 0;
	xxi[i][0].vol = m3h.instrument_volume[i];
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	xxih[i].nsm = !!(xxs[i].len);
	xxih[i].rls = 0xfff;
	strncpy (xxih[i].name, m3h.instrument[i], 32);
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
