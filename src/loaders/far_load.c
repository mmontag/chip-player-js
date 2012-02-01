/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Based on the Farandole Composer format specifications by Daniel Potter.
 *
 * "(...) this format is for EDITING purposes (storing EVERYTHING you're
 * working on) so it may include information not completely neccessary."
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "far.h"

#define MAGIC_FAR	MAGIC4('F','A','R',0xfe)


static int far_test (FILE *, char *, const int);
static int far_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info far_loader = {
    "FAR",
    "Farandole Composer",
    far_test,
    far_load
};

static int far_test(FILE *f, char *t, const int start)
{
    if (read32b(f) != MAGIC_FAR)
	return -1;

    read_title(f, t, 40);

    return 0;
}


#define NONE			0xff
#define FX_FAR_SETVIBRATO	0xfe
#define FX_FAR_VSLIDE_UP	0xfd
#define FX_FAR_VSLIDE_DN	0xfc
#define FX_FAR_RETRIG		0xfb
#define FX_FAR_DELAY		0xfa
#define FX_FAR_PORTA_UP		0xf9
#define FX_FAR_PORTA_DN		0xf8

static uint8 fx[] = {
    NONE,
    FX_FAR_PORTA_UP,		/* 0x1?  Pitch Adjust */
    FX_FAR_PORTA_DN,		/* 0x2?  Pitch Adjust */
    FX_PER_TPORTA,		/* 0x3?  Port to Note -- FIXME */
    FX_FAR_RETRIG,		/* 0x4?  Retrigger */
    FX_FAR_SETVIBRATO,		/* 0x5?  Set VibDepth */
    FX_VIBRATO,			/* 0x6?  Vibrato note */
    FX_FAR_VSLIDE_UP,		/* 0x7?  VolSld Up */
    FX_FAR_VSLIDE_DN,		/* 0x8?  VolSld Dn */
    FX_PER_VIBRATO,		/* 0x9?  Vibrato Sust */
    NONE,			/* 0xa?  Port To Vol */
    NONE,			/* N/A */
    FX_FAR_DELAY,		/* 0xc?  Note Offset */
    NONE,			/* 0xd?  Fine Tempo dn */
    NONE,			/* 0xe?  Fine Tempo up */
    FX_TEMPO			/* 0xf?  Tempo */
};


static int far_load(struct xmp_context *ctx, FILE *f, const int start)
{
    struct xmp_mod_context *m = &ctx->m;
    int i, j, vib = 0;
    struct xmp_event *event;
    struct far_header ffh;
    struct far_header2 ffh2;
    struct far_instrument fih;
    uint8 sample_map[8];

    LOAD_INIT();

    read32b(f);				/* File magic: 'FAR\xfe' */
    fread(&ffh.name, 40, 1, f);		/* Song name */
    fread(&ffh.crlf, 3, 1, f);		/* 0x0d 0x0a 0x1A */
    ffh.headersize = read16l(f);	/* Remaining header size in bytes */
    ffh.version = read8(f);		/* Version MSN=major, LSN=minor */
    fread(&ffh.ch_on, 16, 1, f);	/* Channel on/off switches */
    fseek(f, 9, SEEK_CUR);		/* Current editing values */
    ffh.tempo = read8(f);		/* Default tempo */
    fread(&ffh.pan, 16, 1, f);		/* Channel pan definitions */
    read32l(f);				/* Grid, mode (for editor) */
    ffh.textlen = read16l(f);		/* Length of embedded text */

    fseek(f, ffh.textlen, SEEK_CUR);	/* Skip song text */

    fread(&ffh2.order, 256, 1, f);	/* Orders */
    ffh2.patterns = read8(f);		/* Number of stored patterns (?) */
    ffh2.songlen = read8(f);		/* Song length in patterns */
    ffh2.restart = read8(f);		/* Restart pos */
    for (i = 0; i < 256; i++)
	ffh2.patsize[i] = read16l(f);	/* Size of each pattern in bytes */

    m->mod.xxh->chn = 16;
    /*m->mod.xxh->pat=ffh2.patterns; (Error in specs? --claudio) */
    m->mod.xxh->len = ffh2.songlen;
    m->mod.xxh->tpo = 6;
    m->mod.xxh->bpm = 8 * 60 / ffh.tempo;
    memcpy (m->mod.xxo, ffh2.order, m->mod.xxh->len);

    for (m->mod.xxh->pat = i = 0; i < 256; i++) {
	if (ffh2.patsize[i])
	    m->mod.xxh->pat = i + 1;
    }

    m->mod.xxh->trk = m->mod.xxh->chn * m->mod.xxh->pat;

    strncpy(m->mod.name, (char *)ffh.name, 40);
    set_type(m, "FAR (Farandole Composer %d.%d)",
				MSN(ffh.version), LSN(ffh.version));

    MODULE_INFO();

    PATTERN_INIT();

    /* Read and convert patterns */
    _D(_D_INFO "Comment bytes  : %d", ffh.textlen);
    _D(_D_INFO "Stored patterns: %d", m->mod.xxh->pat);

    for (i = 0; i < m->mod.xxh->pat; i++) {
	uint8 brk, note, ins, vol, fxb;

	PATTERN_ALLOC(i);
	if (!ffh2.patsize[i])
	    continue;
	m->mod.xxp[i]->rows = (ffh2.patsize[i] - 2) / 64;
	TRACK_ALLOC(i);

	brk = read8(f) + 1;
	read8(f);

	for (j = 0; j < m->mod.xxp[i]->rows * m->mod.xxh->chn; j++) {
	    event = &EVENT(i, j % m->mod.xxh->chn, j / m->mod.xxh->chn);

	    if ((j % m->mod.xxh->chn) == 0 && (j / m->mod.xxh->chn) == brk)
		event->f2t = FX_BREAK;
	
	    note = read8(f);
	    ins = read8(f);
	    vol = read8(f);
	    fxb = read8(f);

	    if (note)
		event->note = note + 36;
	    if (event->note || ins)
		event->ins = ins + 1;

	    vol = 16 * LSN(vol) + MSN(vol);

	    if (vol)
		event->vol = vol - 0x10;	/* ? */

	    event->fxt = fx[MSN(fxb)];
	    event->fxp = LSN(fxb);

	    switch (event->fxt) {
	    case NONE:
	        event->fxt = event->fxp = 0;
		break;
	    case FX_FAR_PORTA_UP:
		event->fxt = FX_EXTENDED;
		event->fxp |= (EX_F_PORTA_UP << 4);
		break;
	    case FX_FAR_PORTA_DN:
		event->fxt = FX_EXTENDED;
		event->fxp |= (EX_F_PORTA_DN << 4);
		break;
	    case FX_FAR_RETRIG:
		event->fxt = FX_EXTENDED;
		event->fxp |= (EX_RETRIG << 4);
		break;
	    case FX_FAR_DELAY:
		event->fxt = FX_EXTENDED;
		event->fxp |= (EX_DELAY << 4);
		break;
	    case FX_FAR_SETVIBRATO:
		vib = event->fxp & 0x0f;
		event->fxt = event->fxp = 0;
		break;
	    case FX_VIBRATO:
		event->fxp = (event->fxp << 4) + vib;
		break;
	    case FX_PER_VIBRATO:
		event->fxp = (event->fxp << 4) + vib;
		break;
	    case FX_FAR_VSLIDE_UP:	/* Fine volume slide up */
		event->fxt = FX_EXTENDED;
		event->fxp |= (EX_F_VSLIDE_UP << 4);
		break;
	    case FX_FAR_VSLIDE_DN:	/* Fine volume slide down */
		event->fxt = FX_EXTENDED;
		event->fxp |= (EX_F_VSLIDE_DN << 4);
		break;
	    case FX_TEMPO:
		event->fxp = 8 * 60 / event->fxp;
		break;
	    }
	}
    }

    m->mod.xxh->ins = -1;
    fread(sample_map, 1, 8, f);
    for (i = 0; i < 64; i++) {
	if (sample_map[i / 8] & (1 << (i % 8)))
		m->mod.xxh->ins = i;
    }
    m->mod.xxh->ins++;

    m->mod.xxh->smp = m->mod.xxh->ins;

    INSTRUMENT_INIT();

    /* Read and convert instruments and samples */

    for (i = 0; i < m->mod.xxh->ins; i++) {
	if (!(sample_map[i / 8] & (1 << (i % 8))))
		continue;

	m->mod.xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);

	fread(&fih.name, 32, 1, f);	/* Instrument name */
	fih.length = read32l(f);	/* Length of sample (up to 64Kb) */
	fih.finetune = read8(f);	/* Finetune (unsuported) */
	fih.volume = read8(f);		/* Volume (unsuported?) */
	fih.loop_start = read32l(f);	/* Loop start */
	fih.loopend = read32l(f);	/* Loop end */
	fih.sampletype = read8(f);	/* 1=16 bit sample */
	fih.loopmode = read8(f);

	fih.length &= 0xffff;
	fih.loop_start &= 0xffff;
	fih.loopend &= 0xffff;
	m->mod.xxs[i].len = fih.length;
	m->mod.xxi[i].nsm = fih.length > 0 ? 1 : 0;
	m->mod.xxs[i].lps = fih.loop_start;
	m->mod.xxs[i].lpe = fih.loopend;
	m->mod.xxs[i].flg = 0;

	if (fih.sampletype != 0) {
		m->mod.xxs[i].flg |= XMP_SAMPLE_16BIT;
		m->mod.xxs[i].len >>= 1;
		m->mod.xxs[i].lps >>= 1;
		m->mod.xxs[i].lpe >>= 1;
	}

	m->mod.xxs[i].flg |= fih.loopmode ? XMP_SAMPLE_LOOP : 0;
	m->mod.xxi[i].sub[0].vol = 0xff; /* fih.volume; */
	m->mod.xxi[i].sub[0].sid = i;

	copy_adjust(m->mod.xxi[i].name, fih.name, 32);

	_D(_D_INFO "[%2X] %-32.32s %04x %04x %04x %c V%02x",
		i, m->mod.xxi[i].name, m->mod.xxs[i].len, m->mod.xxs[i].lps,
		m->mod.xxs[i].lpe, fih.loopmode ? 'L' : ' ', m->mod.xxi[i].sub[0].vol);

	xmp_drv_loadpatch(ctx, f, m->mod.xxi[i].sub[0].sid, 0, &m->mod.xxs[i], NULL);
    }

    m->volbase = 0xff;

    return 0;
}
