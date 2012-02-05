/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#include "load.h"
#include "stm.h"
#include "period.h"


static int stm_test (FILE *, char *, const int);
static int stm_load (struct xmp_context *, FILE *, const int);

struct format_loader stm_loader = {
    "STM",
    "Scream Tracker 2",
    stm_test,
    stm_load
};

static int stm_test(FILE *f, char *t, const int start)
{
    char buf[8];

    fseek(f, start + 20, SEEK_SET);
    if (fread(buf, 1, 8, f) < 8)
	return -1;
    if (memcmp(buf, "!Scream!", 8) && memcmp(buf, "BMOD2STM", 8))
	return -1;

    read8(f);

    if (read8(f) != STM_TYPE_MODULE)
	return -1;

    if (read8(f) < 1)		/* We don't want STX files */
	return -1;

    fseek(f, start + 0, SEEK_SET);
    read_title(f, t, 20);

    return 0;
}



#define FX_NONE		0xff

/*
 * Skaven's note from http://www.futurecrew.com/skaven/oldies_music.html
 *
 * FYI for the tech-heads: In the old Scream Tracker 2 the Arpeggio command
 * (Jxx), if used in a single row with a 0x value, caused the note to skip
 * the specified amount of halftones upwards halfway through the row. I used
 * this in some songs to give the lead some character. However, when played
 * in ModPlug Tracker, this effect doesn't work the way it did back then.
 */

static uint8 fx[] = {
    FX_NONE,		FX_TEMPO,
    FX_JUMP,		FX_BREAK,
    FX_VOLSLIDE,	FX_PORTA_DN,
    FX_PORTA_UP,	FX_TONEPORTA,
    FX_VIBRATO,		FX_TREMOR,
    FX_ARPEGGIO
};


static int stm_load(struct xmp_context *ctx, FILE *f, const int start)
{
    struct xmp_mod_context *m = &ctx->m;
    struct xmp_module *mod = &m->mod;
    int i, j;
    struct xmp_event *event;
    struct stm_file_header sfh;
    uint8 b;
    int bmod2stm = 0;

    LOAD_INIT();

    fread(&sfh.name, 20, 1, f);			/* ASCIIZ song name */
    fread(&sfh.magic, 8, 1, f);			/* '!Scream!' */
    sfh.rsvd1 = read8(f);			/* '\x1a' */
    sfh.type = read8(f);			/* 1=song, 2=module */
    sfh.vermaj = read8(f);			/* Major version number */
    sfh.vermin = read8(f);			/* Minor version number */
    sfh.tempo = read8(f);			/* Playback tempo */
    sfh.patterns = read8(f);			/* Number of patterns */
    sfh.gvol = read8(f);			/* Global volume */
    fread(&sfh.rsvd2, 13, 1, f);		/* Reserved */

    for (i = 0; i < 31; i++) {
	fread(&sfh.ins[i].name, 12, 1, f);	/* ASCIIZ instrument name */
	sfh.ins[i].id = read8(f);		/* Id=0 */
	sfh.ins[i].idisk = read8(f);		/* Instrument disk */
	sfh.ins[i].rsvd1 = read16l(f);		/* Reserved */
	sfh.ins[i].length = read16l(f);		/* Sample length */
	sfh.ins[i].loopbeg = read16l(f);	/* Loop begin */
	sfh.ins[i].loopend = read16l(f);	/* Loop end */
	sfh.ins[i].volume = read8(f);		/* Playback volume */
	sfh.ins[i].rsvd2 = read8(f);		/* Reserved */
	sfh.ins[i].c2spd = read16l(f);		/* C4 speed */
	sfh.ins[i].rsvd3 = read32l(f);		/* Reserved */
	sfh.ins[i].paralen = read16l(f);	/* Length in paragraphs */
    }

    if (!strncmp ((char *)sfh.magic, "BMOD2STM", 8))
	bmod2stm = 1;

    mod->pat = sfh.patterns;
    mod->trk = mod->pat * mod->chn;
    mod->tpo = MSN (sfh.tempo);
    mod->ins = 31;
    mod->smp = mod->ins;
    m->c4rate = C4_NTSC_RATE;

    copy_adjust(mod->name, sfh.name, 20);

    if (bmod2stm) {
	snprintf(mod->type, XMP_NAMESIZE, "BMOD2STM STM");
    } else {
	snprintf(mod->type, XMP_NAMESIZE, "Scream Tracker %d.%02d STM",
						sfh.vermaj, sfh.vermin);
    }

    MODULE_INFO();

    INSTRUMENT_INIT();

    /* Read and convert instruments and samples */
    for (i = 0; i < mod->ins; i++) {
	mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
	mod->xxi[i].nsm = !!(mod->xxs[i].len = sfh.ins[i].length);
	mod->xxs[i].lps = sfh.ins[i].loopbeg;
	mod->xxs[i].lpe = sfh.ins[i].loopend;
	if (mod->xxs[i].lpe == 0xffff)
	    mod->xxs[i].lpe = 0;
	mod->xxs[i].flg = mod->xxs[i].lpe > 0 ? XMP_SAMPLE_LOOP : 0;
	mod->xxi[i].sub[0].vol = sfh.ins[i].volume;
	mod->xxi[i].sub[0].pan = 0x80;
	mod->xxi[i].sub[0].sid = i;

	copy_adjust(mod->xxi[i].name, sfh.ins[i].name, 12);

	_D(_D_INFO "[%2X] %-14.14s %04x %04x %04x %c V%02x %5d", i,
		mod->xxi[i].name, mod->xxs[i].len, mod->xxs[i].lps,
		mod->xxs[i].lpe, mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		mod->xxi[i].sub[0].vol, sfh.ins[i].c2spd);

	sfh.ins[i].c2spd = 8363 * sfh.ins[i].c2spd / 8448;
	c2spd_to_note (sfh.ins[i].c2spd, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);
    }

    fread(mod->xxo, 1, 128, f);

    for (i = 0; i < 128; i++)
	if (mod->xxo[i] >= mod->pat)
	    break;

    mod->len = i;

    _D(_D_INFO "Module length: %d", mod->len);

    PATTERN_INIT();

    /* Read and convert patterns */
    _D(_D_INFO "Stored patterns: %d", mod->pat);

    for (i = 0; i < mod->pat; i++) {
	PATTERN_ALLOC (i);
	mod->xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	for (j = 0; j < 64 * mod->chn; j++) {
	    event = &EVENT (i, j % mod->chn, j / mod->chn);
	    b = read8(f);
	    memset (event, 0, sizeof (struct xmp_event));
	    switch (b) {
	    case 251:
	    case 252:
	    case 253:
		break;
	    case 255:
		b = 0;
	    default:
		event->note = b ? 1 + LSN(b) + 12 * (2 + MSN(b)) : 0;
		b = read8(f);
		event->vol = b & 0x07;
		event->ins = (b & 0xf8) >> 3;
		b = read8(f);
		event->vol += (b & 0xf0) >> 1;
		if (event->vol > 0x40)
		    event->vol = 0;
		else
		    event->vol++;
		event->fxt = fx[LSN(b)];
		event->fxp = read8(f);
		switch (event->fxt) {
		case FX_TEMPO:
		    event->fxp = MSN (event->fxp);
		    break;
		case FX_NONE:
		    event->fxp = event->fxt = 0;
		    break;
		}
	    }
	}
    }

    /* Read samples */
    _D(_D_INFO "Stored samples: %d", mod->smp);

    for (i = 0; i < mod->ins; i++) {
	load_sample(ctx, f, mod->xxi[i].sub[0].sid, 0,
	    &mod->xxs[mod->xxi[i].sub[0].sid], NULL);
    }

    m->quirk |= QUIRK_VSALL | QUIRKS_ST3;

    return 0;
}
