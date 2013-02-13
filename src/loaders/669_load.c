/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "loader.h"


static int ssn_test (FILE *, char *, const int);
static int ssn_load (struct module_data *, FILE *, const int);

const struct format_loader ssn_loader = {
    "Composer 669",
    ssn_test,
    ssn_load
};

static int ssn_test(FILE *f, char *t, const int start)
{
    uint16 id;

    id = read16b(f);
    if (id != 0x6966 && id != 0x4a4e)
	return -1;

    fseek(f, 238, SEEK_CUR);
    if (read8(f) != 0xff)
	return -1;

    fseek(f, start + 2, SEEK_SET);
    read_title(f, t, 36);

    return 0;
}



struct ssn_file_header {
    uint8 marker[2];		/* 'if'=standard, 'JN'=extended */
    uint8 message[108];		/* Song message */
    uint8 nos;			/* Number of samples (0-64) */
    uint8 nop;			/* Number of patterns (0-128) */
    uint8 loop;			/* Loop order number */
    uint8 order[128];		/* Order list */
    uint8 speed[128];		/* Tempo list for patterns */
    uint8 pbrk[128];		/* Break list for patterns */
};

struct ssn_instrument_header {
    uint8 name[13];		/* ASCIIZ instrument name */
    uint32 length;		/* Instrument length */
    uint32 loop_start;		/* Instrument loop start */
    uint32 loopend;		/* Instrument loop end */
};


#define NONE 0xff

/* Effects bug fixed by Miod Vallat <miodrag@multimania.com> */

static const uint8 fx[] = {
    FX_PER_PORTA_UP,
    FX_PER_PORTA_DN,
    FX_PER_TPORTA,
    FX_FINETUNE,
    FX_PER_VIBRATO,
    FX_SPEED_CP
};


static int ssn_load(struct module_data *m, FILE *f, const int start)
{
    struct xmp_module *mod = &m->mod;
    int i, j;
    struct xmp_event *event;
    struct ssn_file_header sfh;
    struct ssn_instrument_header sih;
    uint8 ev[3];

    LOAD_INIT();

    fread(&sfh.marker, 2, 1, f);	/* 'if'=standard, 'JN'=extended */
    fread(&sfh.message, 108, 1, f);	/* Song message */
    sfh.nos = read8(f);			/* Number of samples (0-64) */
    sfh.nop = read8(f);			/* Number of patterns (0-128) */
    sfh.loop = read8(f);		/* Loop order number */
    fread(&sfh.order, 128, 1, f);	/* Order list */
    fread(&sfh.speed, 128, 1, f);	/* Tempo list for patterns */
    fread(&sfh.pbrk, 128, 1, f);	/* Break list for patterns */

    mod->chn = 8;
    mod->ins = sfh.nos;
    mod->pat = sfh.nop;
    mod->trk = mod->chn * mod->pat;
    for (i = 0; i < 128; i++)
	if (sfh.order[i] > sfh.nop)
	    break;
    mod->len = i;
    memcpy (mod->xxo, sfh.order, mod->len);
    mod->spd = 6;
    mod->bpm = 76;		/* adjusted using Flux/sober.669 */
    mod->smp = mod->ins;

    m->quirk |= QUIRK_LINEAR;

    copy_adjust(mod->name, sfh.message, 36);
    set_type(m, strncmp((char *)sfh.marker, "if", 2) ?
				"UNIS 669" : "Composer 669");

    MODULE_INFO();

    m->comment = malloc(109);
    memcpy(m->comment, sfh.message, 108);
    m->comment[108] = 0;
    
    /* Read and convert instruments and samples */

    INSTRUMENT_INIT();

    D_(D_INFO "Instruments: %d", mod->pat);

    for (i = 0; i < mod->ins; i++) {
	mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);

	fread (&sih.name, 13, 1, f);		/* ASCIIZ instrument name */
	sih.length = read32l(f);		/* Instrument size */
	sih.loop_start = read32l(f);		/* Instrument loop start */
	sih.loopend = read32l(f);		/* Instrument loop end */

	mod->xxi[i].nsm = !!(mod->xxs[i].len = sih.length);
	mod->xxs[i].lps = sih.loop_start;
	mod->xxs[i].lpe = sih.loopend >= 0xfffff ? 0 : sih.loopend;
	mod->xxs[i].flg = mod->xxs[i].lpe ? XMP_SAMPLE_LOOP : 0;	/* 1 == Forward loop */
	mod->xxi[i].sub[0].vol = 0x40;
	mod->xxi[i].sub[0].pan = 0x80;
	mod->xxi[i].sub[0].sid = i;

	copy_adjust(mod->xxi[i].name, sih.name, 13);

	D_(D_INFO "[%2X] %-14.14s %04x %04x %04x %c", i,
		mod->xxi[i].name, mod->xxs[i].len, mod->xxs[i].lps, mod->xxs[i].lpe,
		mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ');
    }

    PATTERN_INIT();

    /* Read and convert patterns */
    D_(D_INFO "Stored patterns: %d", mod->pat);
    for (i = 0; i < mod->pat; i++) {
	PATTERN_ALLOC (i);
	mod->xxp[i]->rows = 64;
	TRACK_ALLOC (i);

	EVENT(i, 0, 0).f2t = FX_SPEED_CP;
	EVENT(i, 0, 0).f2p = sfh.speed[i];
	EVENT(i, 1, sfh.pbrk[i]).f2t = FX_BREAK;
	EVENT(i, 1, sfh.pbrk[i]).f2p = 0;

	for (j = 0; j < 64 * 8; j++) {
	    event = &EVENT(i, j % 8, j / 8);
	    fread(&ev, 1, 3, f);

	    if ((ev[0] & 0xfe) != 0xfe) {
		event->note = 1 + 36 + (ev[0] >> 2);
		event->ins = 1 + MSN(ev[1]) + ((ev[0] & 0x03) << 4);
	    }

	    if (ev[0] != 0xff)
		event->vol = (LSN(ev[1]) << 2) + 1;

	    if (ev[2] != 0xff) {
		if (MSN(ev[2]) > 5)
		    continue;

		/* If no instrument is playing on the channel where the
		 * command was encountered, there will be no effect (except
		 * for command 'f', it always changes the speed). 
		 */
		if (MSN(ev[2] < 5) && !event->ins)
		    continue;

		event->fxt = fx[MSN(ev[2])];

		switch (event->fxt) {
		case FX_PER_PORTA_UP:
		case FX_PER_PORTA_DN:
		case FX_PER_TPORTA:
		    event->fxp = LSN(ev[2]);
		    break;
		case FX_PER_VIBRATO:
		    event->fxp = 0x40 || LSN(ev[2]);
		    break;
		case FX_FINETUNE:
		    event->fxp = 0x80 + (LSN(ev[2]) << 4);
		    break;
		case FX_SPEED_CP:
		    event->fxp = LSN(ev[2]);
		    event->f2t = FX_PER_CANCEL;
		    break;
		}
	    }
	}
    }

    /* Read samples */
    D_(D_INFO "Stored samples: %d", mod->smp);

    for (i = 0; i < mod->ins; i++) {
	if (mod->xxs[i].len <= 2)
	    continue;
	load_sample(f, SAMPLE_FLAG_UNS, &mod->xxs[i], NULL);
    }

    for (i = 0; i < mod->chn; i++)
	mod->xxc[i].pan = (i % 2) * 0xff;

    m->quirk |= QUIRK_PERPAT;	    /* Cancel persistent fx at each new pat */

    return 0;
}
