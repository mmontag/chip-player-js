/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Loader for FC-M Packer modules based on the format description
 * written by Sylvain Chipaux (Asle/ReDoX). Modules sent by Sylvain
 * Chipaux.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "period.h"


struct fcm_instrument {
    uint16 size;
    uint8 finetune;
    uint8 volume;
    uint16 loop_start;
    uint16 loop_size;
};

struct fcm_header {
    uint8 magic[4];		/* 'FC-M' magic ID */
    uint8 vmaj;
    uint8 vmin;
    uint8 name_id[4];		/* 'NAME' pseudo chunk ID */
    uint8 name[20];
    uint8 inst_id[4];		/* 'INST' pseudo chunk ID */
    struct fcm_instrument ins[31];
    uint8 long_id[4];		/* 'LONG' pseudo chunk ID */
    uint8 len;
    uint8 rst;
    uint8 patt_id[4];		/* 'PATT' pseudo chunk ID */
};
    

int fcm_load(struct xmp_context *ctx, FILE *f)
{
    int i, j, k;
    struct xxm_event *event;
    struct fcm_header fh;
    uint8 fe[4];

    LOAD_INIT();

    fread (&fh, 1, sizeof (struct fcm_header), f);

    if (fh.magic[0] != 'F' || fh.magic[1] != 'C' || fh.magic[2] != '-' ||
	fh.magic[3] != 'M' || fh.name_id[0] != 'N')
	return -1;

    strncpy (m->mod.name, fh.name, 20);
    set_type(m, "FC-M %d.%d", fh.vmaj, fh.vmin);

    MODULE_INFO();

    m->mod.xxh->len = fh.len;

    fread (m->mod.xxo, 1, m->mod.xxh->len, f);

    for (m->mod.xxh->pat = i = 0; i < m->mod.xxh->len; i++) {
	if (m->mod.xxo[i] > m->mod.xxh->pat)
	    m->mod.xxh->pat = m->mod.xxo[i];
    }
    m->mod.xxh->pat++;

    m->mod.xxh->trk = m->mod.xxh->pat * m->mod.xxh->chn;

    INSTRUMENT_INIT();

    for (i = 0; i < m->mod.xxh->ins; i++) {
	B_ENDIAN16 (fh.ins[i].size);
	B_ENDIAN16 (fh.ins[i].loop_start);
	B_ENDIAN16 (fh.ins[i].loop_size);
	m->mod.xxi[i].sub = calloc(sizeof (struct xxm_subinstrument), 1);
	m->mod.xxs[i].len = 2 * fh.ins[i].size;
	m->mod.xxs[i].lps = 2 * fh.ins[i].loop_start;
	m->mod.xxs[i].lpe = m->mod.xxs[i].lps + 2 * fh.ins[i].loop_size;
	m->mod.xxs[i].flg = fh.ins[i].loop_size > 1 ? XMP_SAMPLE_LOOP : 0;
	m->mod.xxi[i].sub[0].fin = (int8)fh.ins[i].finetune << 4;
	m->mod.xxi[i].sub[0].vol = fh.ins[i].volume;
	m->mod.xxi[i].sub[0].pan = 0x80;
	m->mod.xxi[i].sub[0].sid = i;
	m->mod.xxi[i].nsm = !!(m->mod.xxs[i].len);
	m->mod.xxi[i].rls = 0xfff;
	if (m->mod.xxi[i].sub[0].fin > 48)
	    m->mod.xxi[i].sub[0].xpo = -1;
	if (m->mod.xxi[i].sub[0].fin < -48)
	    m->mod.xxi[i].sub[0].xpo = 1;

	if (V(1) && (strlen(m->mod.xxi[i].name) || m->mod.xxs[i].len > 2)) {
	    report ("[%2X] %04x %04x %04x %c V%02x %+d\n",
		i, m->mod.xxs[i].len, m->mod.xxs[i].lps, m->mod.xxs[i].lpe,
		fh.ins[i].loop_size > 1 ? 'L' : ' ',
		m->mod.xxi[i].sub[0].vol, m->mod.xxi[i].sub[0].fin >> 4);
	}
    }

    PATTERN_INIT();

    /* Load and convert patterns */
    if (V(0))
	report ("Stored patterns: %d ", m->mod.xxh->pat);

    fread (fe, 4, 1, f);	/* Skip 'SONG' pseudo chunk ID */

    for (i = 0; i < m->mod.xxh->pat; i++) {
	PATTERN_ALLOC (i);
	m->mod.xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	for (j = 0; j < 64; j++) {
	    for (k = 0; k < 4; k++) {
		event = &EVENT (i, k, j);
		fread (fe, 4, 1, f);
		cvt_pt_event (event, fe);
	    }
	}

	if (V(0))
	    report (".");
    }

    m->mod.xxh->flg |= XXM_FLG_MODRNG;

    /* Load samples */

    fread (fe, 4, 1, f);	/* Skip 'SAMP' pseudo chunk ID */

    if (V(0))
	report ("\nStored samples : %d ", m->mod.xxh->smp);
    for (i = 0; i < m->mod.xxh->smp; i++) {
	if (!m->mod.xxs[i].len)
	    continue;
	xmp_drv_loadpatch(ctx, f, m->mod.xxi[i].sub[0].sid, 0,
	    &m->mod.xxs[m->mod.xxi[i].sub[0].sid], NULL);
	if (V(0))
	    report (".");
    }
    if (V(0))
	report ("\n");

    return 0;
}
