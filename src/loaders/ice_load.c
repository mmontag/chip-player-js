/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

/* Loader for Soundtracker 2.6/Ice Tracker modules */

#include "loader.h"

#define MAGIC_MTN_	MAGIC4('M','T','N',0)
#define MAGIC_IT10	MAGIC4('I','T','1','0')


static int ice_test (FILE *, char *, const int);
static int ice_load (struct module_data *, FILE *, const int);

const struct format_loader ice_loader = {
    "Soundtracker 2.6/Ice Tracker (MTN)",
    ice_test,
    ice_load
};

static int ice_test(FILE *f, char *t, const int start)
{
    uint32 magic;

    fseek(f, start + 1464, SEEK_SET);
    magic = read32b(f);
    if (magic != MAGIC_MTN_ && magic != MAGIC_IT10)
	return -1;

    fseek(f, start + 0, SEEK_SET);
    read_title(f, t, 28);

    return 0;
}


struct ice_ins {
    char name[22];		/* Instrument name */
    uint16 len;			/* Sample length / 2 */
    uint8 finetune;		/* Finetune */
    uint8 volume;		/* Volume (0-63) */
    uint16 loop_start;		/* Sample loop start in file */
    uint16 loop_size;		/* Loop size / 2 */
};

struct ice_header {
    char title[20];
    struct ice_ins ins[31];	/* Instruments */
    uint8 len;			/* Size of the pattern list */
    uint8 trk;			/* Number of tracks */
    uint8 ord[128][4];
    uint32 magic;		/* 'MTN\0', 'IT10' */
};


static int ice_load(struct module_data *m, FILE *f, const int start)
{
    struct xmp_module *mod = &m->mod;
    int i, j;
    struct xmp_event *event;
    struct ice_header ih;
    uint8 ev[4];

    LOAD_INIT();

    fread(&ih.title, 20, 1, f);
    for (i = 0; i < 31; i++) {
	fread(&ih.ins[i].name, 22, 1, f);
	ih.ins[i].len = read16b(f);
	ih.ins[i].finetune = read8(f);
	ih.ins[i].volume = read8(f);
	ih.ins[i].loop_start = read16b(f);
	ih.ins[i].loop_size = read16b(f);
    }
    ih.len = read8(f);
    ih.trk = read8(f);
    fread(&ih.ord, 128 * 4, 1, f);
    ih.magic = read32b(f);

    if (ih.magic == MAGIC_IT10)
        set_type(m, "Ice Tracker IT10");
    else if (ih.magic == MAGIC_MTN_)
        set_type(m, "Soundtracker 2.6 MTN");
    else
	return -1;

    mod->ins = 31;
    mod->smp = mod->ins;
    mod->pat = ih.len;
    mod->len = ih.len;
    mod->trk = ih.trk;

    strncpy (mod->name, (char *) ih.title, 20);
    MODULE_INFO();

    INSTRUMENT_INIT();

    for (i = 0; i < mod->ins; i++) {
	mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
	mod->xxi[i].nsm = !!(mod->xxs[i].len = 2 * ih.ins[i].len);
	mod->xxs[i].lps = 2 * ih.ins[i].loop_start;
	mod->xxs[i].lpe = mod->xxs[i].lps + 2 * ih.ins[i].loop_size;
	mod->xxs[i].flg = ih.ins[i].loop_size > 1 ? XMP_SAMPLE_LOOP : 0;
	mod->xxi[i].sub[0].vol = ih.ins[i].volume;
	mod->xxi[i].sub[0].fin = ((int16)ih.ins[i].finetune / 0x48) << 4;
	mod->xxi[i].sub[0].pan = 0x80;
	mod->xxi[i].sub[0].sid = i;

	D_(D_INFO "[%2X] %-22.22s %04x %04x %04x %c %02x %01x",
		i, ih.ins[i].name, mod->xxs[i].len, mod->xxs[i].lps,
		mod->xxs[i].lpe, mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		mod->xxi[i].sub[0].vol, mod->xxi[i].sub[0].fin >> 4);
    }

    PATTERN_INIT();

    D_(D_INFO "Stored patterns: %d", mod->pat);

    for (i = 0; i < mod->pat; i++) {
	PATTERN_ALLOC (i);
	mod->xxp[i]->rows = 64;
	for (j = 0; j < mod->chn; j++) {
	    mod->xxp[i]->index[j] = ih.ord[i][j];
	}
	mod->xxo[i] = i;
    }

    D_(D_INFO "Stored tracks: %d", mod->trk);

    for (i = 0; i < mod->trk; i++) {
	mod->xxt[i] = calloc (sizeof (struct xmp_track) + sizeof
		(struct xmp_event) * 64, 1);
	mod->xxt[i]->rows = 64;
	for (j = 0; j < mod->xxt[i]->rows; j++) {
		event = &mod->xxt[i]->event[j];
		fread (ev, 1, 4, f);
		cvt_pt_event (event, ev);
	}
    }

    m->quirk |= QUIRK_MODRNG;

    /* Read samples */

    D_(D_INFO "Stored samples: %d", mod->smp);

    for (i = 0; i < mod->ins; i++) {
	if (mod->xxs[i].len <= 4)
	    continue;
	load_sample(f, 0, &mod->xxs[i], NULL);
    }

    return 0;
}

