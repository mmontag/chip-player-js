/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "loader.h"


static int ssn_test (HIO_HANDLE *, char *, const int);
static int ssn_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader ssn_loader = {
    "Composer 669",
    ssn_test,
    ssn_load
};

static int ssn_test(HIO_HANDLE *f, char *t, const int start)
{
    uint16 id;

    id = hio_read16b(f);
    if (id != 0x6966 && id != 0x4a4e)
	return -1;

    hio_seek(f, 238, SEEK_CUR);
    if (hio_read8(f) != 0xff)
	return -1;

    hio_seek(f, start + 2, SEEK_SET);
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


static int ssn_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
    struct xmp_module *mod = &m->mod;
    int i, j;
    struct xmp_event *event;
    struct ssn_file_header sfh;
    struct ssn_instrument_header sih;
    uint8 ev[3];

    LOAD_INIT();

    hio_read(&sfh.marker, 2, 1, f);	/* 'if'=standard, 'JN'=extended */
    hio_read(&sfh.message, 108, 1, f);	/* Song message */
    sfh.nos = hio_read8(f);		/* Number of samples (0-64) */
    sfh.nop = hio_read8(f);		/* Number of patterns (0-128) */
    sfh.loop = hio_read8(f);		/* Loop order number */
    hio_read(&sfh.order, 128, 1, f);	/* Order list */
    hio_read(&sfh.speed, 128, 1, f);	/* Tempo list for patterns */
    hio_read(&sfh.pbrk, 128, 1, f);	/* Break list for patterns */

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

    if (instrument_init(mod) < 0)
	return -1;

    D_(D_INFO "Instruments: %d", mod->pat);

    for (i = 0; i < mod->ins; i++) {
	if (subinstrument_alloc(mod, i, 1) < 0)
	    return -1;

	hio_read (&sih.name, 13, 1, f);		/* ASCIIZ instrument name */
	sih.length = hio_read32l(f);		/* Instrument size */
	sih.loop_start = hio_read32l(f);	/* Instrument loop start */
	sih.loopend = hio_read32l(f);		/* Instrument loop end */

	mod->xxs[i].len = sih.length;
	mod->xxs[i].lps = sih.loop_start;
	mod->xxs[i].lpe = sih.loopend >= 0xfffff ? 0 : sih.loopend;
	mod->xxs[i].flg = mod->xxs[i].lpe ? XMP_SAMPLE_LOOP : 0;	/* 1 == Forward loop */
	mod->xxi[i].sub[0].vol = 0x40;
	mod->xxi[i].sub[0].pan = 0x80;
	mod->xxi[i].sub[0].sid = i;

	if (mod->xxs[i].len > 0)
		mod->xxi[i].nsm = 1;

	instrument_name(mod, i, sih.name, 13);

	D_(D_INFO "[%2X] %-14.14s %04x %04x %04x %c", i,
		mod->xxi[i].name, mod->xxs[i].len, mod->xxs[i].lps, mod->xxs[i].lpe,
		mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ');
    }

    if (pattern_init(mod) < 0)
	return -1;

    /* Read and convert patterns */
    D_(D_INFO "Stored patterns: %d", mod->pat);
    for (i = 0; i < mod->pat; i++) {
	if (pattern_tracks_alloc(mod, i, 64) < 0)
	    return -1;

	EVENT(i, 0, 0).f2t = FX_SPEED_CP;
	EVENT(i, 0, 0).f2p = sfh.speed[i];
	EVENT(i, 1, sfh.pbrk[i]).f2t = FX_BREAK;
	EVENT(i, 1, sfh.pbrk[i]).f2p = 0;

	for (j = 0; j < 64 * 8; j++) {
	    event = &EVENT(i, j % 8, j / 8);
	    hio_read(&ev, 1, 3, f);

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
	if (load_sample(m, f, SAMPLE_FLAG_UNS, &mod->xxs[i], NULL) < 0)
	    return -1;
    }

    for (i = 0; i < mod->chn; i++)
	mod->xxc[i].pan = (i % 2) * 0xff;

    m->quirk |= QUIRK_PERPAT;	    /* Cancel persistent fx at each new pat */

    return 0;
}
