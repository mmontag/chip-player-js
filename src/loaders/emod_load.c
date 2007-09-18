/* Quadra Composer module loader for xmp
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: emod_load.c,v 1.3 2007-09-18 11:28:50 cmatsuoka Exp $
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

#define MAGIC_FORM	MAGIC4('F','O','R','M')
#define MAGIC_EMOD	MAGIC4('E','M','O','D')


static int *reorder;


static void get_emic(int size, FILE *f)
{
    int i, ver;

    ver = read16b(f);
    fread(xmp_ctl->name, 1, 20, f);
    fread(author_name, 1, 20, f);
    xxh->bpm = read8(f);
    xxh->ins = read8(f);
    xxh->smp = xxh->ins;

    snprintf(xmp_ctl->type, XMP_DEF_NAMESIZE,
				"EMOD v%d (Quadra Composer)", ver);
    MODULE_INFO ();

    INSTRUMENT_INIT ();

    reportv(1, "     Instrument name      Len  LBeg LEnd L Vol Fin\n");

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);

	read8(f);		/* num */
	xxi[i][0].vol = read8(f);
	xxs[i].len = 2 * read16b(f);
	fread(xxih[i].name, 1, 20, f);
	xxs[i].flg = read8(f) & 1 ? WAVE_LOOPING : 0;
	xxi[i][0].fin = read8(f);
	xxs[i].lps = 2 * read16b(f);
	xxs[i].lpe = xxs[i].lps + 2 * read16b(f);
	read32b(f);		/* ptr */

	xxih[i].nsm = 1;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;

	if (V(1) && (strlen ((char *)xxih[i].name) || (xxs[i].len > 2))) {
	    report ("[%2X] %-20.20s %04x %04x %04x %c V%02x %+d\n",
			i, xxih[i].name, xxs[i].len, xxs[i].lps,
			xxs[i].lpe, xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
			xxi[i][0].vol, (char)xxi[i][0].fin >> 4);
	}
    }

    read8(f);			/* pad */
    xxh->pat = read8(f);

    xxh->trk = xxh->pat * xxh->chn;

    PATTERN_INIT ();

    reorder = calloc(sizeof(int), 256);

    for (i = 0; i < xxh->pat; i++) {
	reorder[read8(f)] = i;
	PATTERN_ALLOC(i);
	xxp[i]->rows = read8(f) + 1;
	TRACK_ALLOC(i);
	fseek(f, 20, SEEK_CUR);		/* skip name */
	read32b(f);			/* ptr */
    }

    xxh->len = read8(f);

    reportv(0, "Module length  : %d\n", xxh->len);

    for (i = 0; i < xxh->len; i++)
	xxo[i] = reorder[read8(f)];
}


static void get_patt(int size, FILE *f)
{
    int i, j, k;
    struct xxm_event *event;

    reportv(0, "Stored patterns: %d ", xxh->pat);

    for (i = 0; i < xxh->pat; i++) {
	for (j = 0; j < xxp[i]->rows; j++) {
	    for (k = 0; k < xxh->chn; k++) {
		event = &EVENT(i, k, j);
		event->ins = read8(f);
		event->note = read8(f) + 1;
		if (event->note != 0)
		    event->note += 36;
		event->fxt = read8(f) & 0x0f;
		event->fxp = read8(f);

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
	reportv(0, ".");
    }
    reportv(0, "\n");
}


static void get_8smp(int size, FILE *f)
{
    int i;

    reportv(0, "Stored samples : %d ", xxh->smp);

    for (i = 0; i < xxh->smp; i++) {
	xmp_drv_loadpatch (f, i, xmp_ctl->c4rate, 0, &xxs[i], NULL);
	reportv(0, ".");
    }
    reportv(0, "\n");
}


int emod_load(FILE *f)
{
    LOAD_INIT ();

    /* Check magic */
    if (read32b(f) != MAGIC_FORM)
	return -1;

    read32b(f);

    if (read32b(f) != MAGIC_EMOD)
	return -1;

    /* IFF chunk IDs */
    iff_register("EMIC", get_emic);
    iff_register("PATT", get_patt);
    iff_register("8SMP", get_8smp);

    /* Load IFF chunks */
    while (!feof(f))
	iff_chunk(f);

    iff_release();

    free(reorder);

    return 0;
}
