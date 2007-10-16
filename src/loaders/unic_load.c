/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Loader for UNIC and Laxity modules based on the format description
 * written by Sylvain Chipaux (Asle/ReDoX) and the Pro-Wizard docs
 * by Gryzor. UNIC and Laxity formats created by Anders E. Hansen
 * (Laxity/Kefrens).
 *
 * Gryzor's comments from PW_FORMATS-Engl.guide:
 * UNIC: "The UNIC format is very similar to Protracker... At least in
 * the heading... same length: 1084 bytes. Even the 'M.K.' is present,
 * sometimes!"
 * LAX: "This format is VERY VERY similar to UNIC (and to Protracker)!
 * Except that the module name is no longer present... And no 'M.K.'
 * mark either..."
 *
 * Tested with the LAX modules sent by Bert Jahn:
 * LAX.KEFRENS_DD.checknobankh
 * LAX.KEFRENS_DD.maj-strings.cc
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <sys/types.h>

#include "load.h"
#include "period.h"


struct unic_instrument {
    uint8 name[20];
    int16 finetune;
    uint16 size;
    uint8 unknown;
    uint8 volume;
    uint16 loop_start;
    uint16 loop_size;
} PACKED;

struct unic_header {
    uint8 title[20];			/* LAX has no title */
    struct unic_instrument ins[31];
    uint8 len;
    uint8 zero;
    uint8 orders[128];
    uint8 magic[4];
} PACKED;

static int _unic_load (FILE *, int);


int unic_load(struct xmp_mod_context *m, FILE *f)
{
    return _unic_load (f, 0) ? _unic_load (f, 1) : 0;
}


static int _unic_load (FILE *f, int lax)
{
    int i, j;
    int smp_size;
    struct xxm_event *event;
    struct unic_header uh;
    uint8 unic_event[3];
    int nomagic, xpo;

    LOAD_INIT ();

    m->xxh->ins = 31;
    m->xxh->smp = m->xxh->ins;
    smp_size = 0;
    nomagic = 0;
    xpo = 36;

    fread ((uint8 *)&uh + 20 * lax, 1,
	sizeof (struct unic_header) - 20 * lax, f);

    for (i = 0; i < m->xxh->ins; i++) {
	B_ENDIAN16 (uh.ins[i].finetune);
	B_ENDIAN16 (uh.ins[i].size);
	B_ENDIAN16 (uh.ins[i].loop_start);
	B_ENDIAN16 (uh.ins[i].loop_size);
    }

    if (uh.len > 0x7f)
	return -1;

    m->xxh->len = uh.len;
    memcpy (m->xxo, uh.orders, m->xxh->len);

    for (i = 0; i < m->xxh->len; i++)
	if (m->xxo[i] > m->xxh->pat)
	    m->xxh->pat = m->xxo[i];

    m->xxh->pat++;
    m->xxh->trk = m->xxh->chn * m->xxh->pat;

    /* According to Asle:
     *
     * ``Gryzor says this ID is not always present. I've only two Unic
     * files and both have this ID. Now, Marley states there can be the
     * ID "UNIC" instead or even $00-00-00-00. I've just find out that
     * the first music of the demo "desert dream" by Kefrens is a UNIC,
     * has a title and no ID string!''
     *
     * Claudio's note: I'll check for UNIC first -- if it fails, check
     * module size with and without magic ID.
     */

    if (lax || strncmp ((char *)uh.magic, "UNIC", 4)) {
	for (i = 0; i < 31; i++)
	    smp_size += uh.ins[i].size * 2;
	i = m->xxh->pat * 0x300 + smp_size + sizeof (struct unic_header) -
	    20 * lax;
	if ((m->size != (i - 4)) && (m->size != i))
	    return -1;

	if (m->size == i - 4) {		/* No magic */
	    fseek (f, -4, SEEK_CUR);
	    nomagic = 1;
	}
    }

    /* Corrputed mod? */
    if (m->xxh->pat > 0x7f || m->xxh->len == 0 || m->xxh->len > 0x7f)
	return -1;

    if (!lax) {
	strncpy (m->name, (char *) uh.title, 20);
	if (nomagic) {
	    sprintf (m->type, "UNIC Tracker (no magic)");
	} else {
	    sprintf (m->type, "UNIC Tracker [%02X %02X %02X %02X]",
		uh.magic[0], uh.magic[1], uh.magic[2], uh.magic[3]);
	}
    } else {
	sprintf (m->type, "UNIC Tracker 2 (Laxity)");
    }

    /*
     * Just a wild guess based on some of my UNICs (UNIC.Am*-3ch)
     * Could someone check this?
     */
    if (!nomagic) {
	if (uh.magic[3] == 0x3c)
	    xpo = 24;
    }

    MODULE_INFO ();

    INSTRUMENT_INIT ();

    for (i = 0; i < m->xxh->ins; i++) {
	m->xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	m->xxs[i].len = 2 * uh.ins[i].size;
	if (lax)
	m->xxs[i].lps = (2 + (m->fetch & XMP_CTL_FIXLOOP ? 0 : 2)) *
	    uh.ins[i].loop_start;
	m->xxs[i].lpe = m->xxs[i].lps + 2 * uh.ins[i].loop_size;
	m->xxs[i].flg = uh.ins[i].loop_size > 1 ? WAVE_LOOPING : 0;
	m->xxi[i][0].fin = uh.ins[i].finetune;
	m->xxi[i][0].vol = uh.ins[i].volume;
	m->xxi[i][0].pan = 0x80;
	m->xxi[i][0].sid = i;
	m->xxih[i].nsm = !!(m->xxs[i].len);
	m->xxih[i].rls = 0xfff;
	strncpy (m->xxih[i].name, uh.ins[i].name, 20);
	str_adj (m->xxih[i].name);

	if (V (1) &&
		(strlen ((char *) m->xxih[i].name) || (m->xxs[i].len > 2))) {
	    report ("[%2X] %-20.20s %04x %04x %04x %c V%02x %+d\n",
		i, m->xxih[i].name, m->xxs[i].len, m->xxs[i].lps,
		m->xxs[i].lpe, uh.ins[i].loop_size > 1 ? 'L' : ' ',
		m->xxi[i][0].vol, (char) m->xxi[i][0].fin >> 4);
	}
    }

    PATTERN_INIT ();

    /* Load and convert patterns */
    if (V (0))
	report ("Stored patterns: %d ", m->xxh->pat);

    for (i = 0; i < m->xxh->pat; i++) {
	PATTERN_ALLOC (i);
	m->xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	for (j = 0; j < 0x100; j++) {
	    event = &EVENT (i, j & 0x3, j >> 2);
	    fread (unic_event, 1, 3, f);

	    /* Event format:
	     *
	     * 0000 0000  0000 0000  0000 0000
	     *  |\     /  \  / \  /  \       /
	     *  | note    ins   fx   parameter
	     * ins
	     *
	     * 0x3f is a blank note.
	     */
	    event->note = unic_event[0] & 0x3f;
	    if (event->note != 0x00 && event->note != 0x3f)
		event->note += xpo;
	    else
		event->note = 0;
	    event->ins = ((unic_event[0] & 0x40) >> 2) | MSN (unic_event[1]);
	    event->fxt = LSN (unic_event[1]);
	    event->fxp = unic_event[2];

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

    m->xxh->flg |= XXM_FLG_MODRNG;

    /* Load samples */

    if (V (0))
	report ("\nStored samples : %d ", m->xxh->smp);
    for (i = 0; i < m->xxh->smp; i++) {
	if (!m->xxs[i].len)
	    continue;
	xmp_drv_loadpatch (f, m->xxi[i][0].sid, m->c4rate, 0,
	    &m->xxs[m->xxi[i][0].sid], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    return 0;
}
