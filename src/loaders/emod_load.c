/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include "load.h"
#include "iff.h"

struct emod_ins {
    uint8 num;
    uint8 vol;
    uint16 len;
    char name[20];
    uint8 flg;
    uint8 fin;
    uint16 loop_start;
    uint16 loop_len;
    uint32 ptr;
} PACKED;

struct emod_pat {
    uint8 num;
    uint8 len;
    char name[20];
    uint32 ptr;
} PACKED;

struct emod_emic {
    uint16 version;
    char name[20];
    char author[20];
    uint8 tempo;
    uint8 nins;
    struct emod_ins ins[1];
} PACKED;

struct emod_emic2 {
    uint8 pad;
    uint8 pat;
    struct emod_pat pinfo[1];
} PACKED;

struct emod_emic3 {
    uint8 len;
    uint8 ord[1];
} PACKED;


static int *reorder;
static int pat;


static void get_emic (int size, void *buffer)
{
    struct emod_emic *emic;
    struct emod_emic2 *emic2;
    struct emod_emic3 *emic3;
    int i;

    emic = (struct emod_emic *)buffer;
    B_ENDIAN16 (emic->version);

    xxh->bpm = emic->tempo;
    xxh->ins = emic->nins;
    xxh->smp = xxh->ins;

    strncpy (xmp_ctl->name, emic->name, 20);
    sprintf (xmp_ctl->type, "EMOD v%d (Quadra Composer)", emic->version);
    strncpy (author_name, emic->author, 20);
    MODULE_INFO ();

    INSTRUMENT_INIT ();

    if (V (1))
	report ("     Instrument name      Len  LBeg LEnd L Vol Fin\n");

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	B_ENDIAN16 (emic->ins[i].len);
	B_ENDIAN16 (emic->ins[i].loop_start);
	B_ENDIAN16 (emic->ins[i].loop_len);
	xxih[i].nsm = 1;
	strncpy (xxih[i].name, emic->ins[i].name, 20);
	xxi[i][0].vol = emic->ins[i].vol;
	xxi[i][0].pan = 0x80;
	xxi[i][0].fin = emic->ins[i].fin;
	xxi[i][0].sid = i;
	xxs[i].len = 2 * emic->ins[i].len;
	xxs[i].lps = 2 * emic->ins[i].loop_start;
	xxs[i].lpe = xxs[i].lps + 2 * emic->ins[i].loop_len;
	xxs[i].flg = emic->ins[i].flg & 1 ? WAVE_LOOPING : 0;

	if (V (1) && (strlen ((char *) xxih[i].name) ||
	    (xxs[i].len > 2)))
	    report ("[%2X] %-20.20s %04x %04x %04x %c V%02x %+d\n",
		i, xxih[i].name, xxs[i].len, xxs[i].lps,
		xxs[i].lpe, emic->ins[i].flg & 1 ? 'L' : ' ',
		xxi[i][0].vol, (char) xxi[i][0].fin >> 4);
    }

    emic2 = (struct emod_emic2 *)(buffer + sizeof (struct emod_emic) +
	(xxh->ins - 1) * sizeof (struct emod_ins));

    pat = xxh->pat = emic2->pat;

    emic3 = (struct emod_emic3 *)((char *)emic2 + sizeof (struct emod_emic2) +
	(pat - 1) * sizeof (struct emod_pat));

    xxh->len = emic3->len;

    if (V (0))
	report ("Module length  : %d\n", xxh->len);

    for (i = 0; i < xxh->len; i++) {
	xxo[i] = emic3->ord[i];
	if (xxh->pat <= xxo[i])
	    xxh->pat = xxo[i] + 1;
    }

    xxh->trk = xxh->pat * xxh->chn;

    PATTERN_INIT ();

    reorder = calloc (4, pat);

    for (i = 0; i < pat; i++) {
	reorder[i] = emic2->pinfo[i].num;
	PATTERN_ALLOC (reorder[i]);
	B_ENDIAN32 (emic2->pinfo[i].ptr);
	xxp[reorder[i]]->rows = emic2->pinfo[i].len + 1;
	TRACK_ALLOC (reorder[i]);
    }
}


static void get_patt (int size, void *buffer)
{
    int i, j, k;
    struct xxm_event *event;
    uint8 *c;

    if (V (0))
        report ("Stored patterns: %d ", pat);

    for (c = buffer, i = 0; i < pat; i++) {
	for (j = 0; j < xxp[reorder[i]]->rows; j++) {
	    for (k = 0; k < xxh->chn; k++) {
		event = &EVENT (reorder[i], k, j);
		event->ins = *c++;
		event->note = *c++ + 1;
		if (event->note != 0)
		    event->note += 36;
		event->fxt = *c++ & 0x0f;
		event->fxp = *c++;

		if (!event->fxp) {
		    switch (event->fxt) {
		    case 0x05:
			event->fxt = 0x03;
			break;
		    case 0x06:
			event->fxt = 0x04;
			break;
		    case 0x01:
		    case 0x02:
		    case 0x0a:
			event->fxt = 0x00;
		    }
		}                                
	    }
	}
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");
}


static void get_8smp (int size, void *buffer)
{
    int i;

    if (V (0))
	report ("Stored samples : %d ", xxh->smp);

    for (i = 0; i < xxh->smp; i++) {
	xmp_drv_loadpatch (NULL, i, xmp_ctl->c4rate, XMP_SMP_NOLOAD,
	    &xxs[i], (char *)buffer);
	buffer += xxs[i].len;
	
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");
}


int emod_load (FILE *f)
{
    struct iff_header h;

    LOAD_INIT ();

    /* Check magic */
    fread (&h, 1, sizeof (struct iff_header), f);
    if (h.form[0] != 'F' || h.form[1] != 'O' || h.form[2] != 'R' ||
	h.form[3] != 'M' || h.id[0] != 'E' || h.id[1] != 'M' || h.id[2]
	!= 'O' || h.id[3] != 'D')
	return -1;

    /* IFF chunk IDs */
    iff_register ("EMIC", get_emic);
    iff_register ("PATT", get_patt);
    iff_register ("8SMP", get_8smp);

    /* Load IFF chunks */
    while (!feof (f))
	iff_chunk (f);

    iff_release ();
    free (reorder);

    return 0;
}
