/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

/* Based on the Farandole Composer format specifications by Daniel Potter.
 *
 * "(...) this format is for EDITING purposes (storing EVERYTHING you're
 * working on) so it may include information not completely neccessary."
 */

#include "loader.h"

struct far_header {
	uint32 magic;		/* File magic: 'FAR\xfe' */
	uint8 name[40];		/* Song name */
	uint8 crlf[3];		/* 0x0d 0x0a 0x1A */
	uint16 headersize;	/* Remaining header size in bytes */
	uint8 version;		/* Version MSN=major, LSN=minor */
	uint8 ch_on[16];	/* Channel on/off switches */
	uint8 rsvd1[9];		/* Current editing values */
	uint8 tempo;		/* Default tempo */
	uint8 pan[16];		/* Channel pan definitions */
	uint8 rsvd2[4];		/* Grid, mode (for editor) */
	uint16 textlen;		/* Length of embedded text */
};

struct far_header2 {
	uint8 order[256];	/* Orders */
	uint8 patterns;		/* Number of stored patterns (?) */
	uint8 songlen;		/* Song length in patterns */
	uint8 restart;		/* Restart pos */
	uint16 patsize[256];	/* Size of each pattern in bytes */
};

struct far_instrument {
	uint8 name[32];		/* Instrument name */
	uint32 length;		/* Length of sample (up to 64Kb) */
	uint8 finetune;		/* Finetune (unsuported) */
	uint8 volume;		/* Volume (unsuported?) */
	uint32 loop_start;	/* Loop start */
	uint32 loopend;		/* Loop end */
	uint8 sampletype;	/* 1=16 bit sample */
	uint8 loopmode;
};

struct far_event {
	uint8 note;
	uint8 instrument;
	uint8 volume;		/* In reverse nibble order? */
	uint8 effect;
};


#define MAGIC_FAR	MAGIC4('F','A','R',0xfe)


static int far_test (HIO_HANDLE *, char *, const int);
static int far_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader far_loader = {
    "Farandole Composer (FAR)",
    far_test,
    far_load
};

static int far_test(HIO_HANDLE *f, char *t, const int start)
{
    if (hio_read32b(f) != MAGIC_FAR)
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

static const uint8 fx[] = {
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
    FX_SPEED			/* 0xf?  Tempo */
};


static int far_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
    struct xmp_module *mod = &m->mod;
    int i, j, vib = 0;
    struct xmp_event *event;
    struct far_header ffh;
    struct far_header2 ffh2;
    struct far_instrument fih;
    uint8 sample_map[8];

    LOAD_INIT();

    hio_read32b(f);				/* File magic: 'FAR\xfe' */
    hio_read(&ffh.name, 40, 1, f);		/* Song name */
    hio_read(&ffh.crlf, 3, 1, f);		/* 0x0d 0x0a 0x1A */
    ffh.headersize = hio_read16l(f);	/* Remaining header size in bytes */
    ffh.version = hio_read8(f);		/* Version MSN=major, LSN=minor */
    hio_read(&ffh.ch_on, 16, 1, f);	/* Channel on/off switches */
    hio_seek(f, 9, SEEK_CUR);		/* Current editing values */
    ffh.tempo = hio_read8(f);		/* Default tempo */
    hio_read(&ffh.pan, 16, 1, f);		/* Channel pan definitions */
    hio_read32l(f);				/* Grid, mode (for editor) */
    ffh.textlen = hio_read16l(f);		/* Length of embedded text */

    hio_seek(f, ffh.textlen, SEEK_CUR);	/* Skip song text */

    hio_read(&ffh2.order, 256, 1, f);	/* Orders */
    ffh2.patterns = hio_read8(f);		/* Number of stored patterns (?) */
    ffh2.songlen = hio_read8(f);		/* Song length in patterns */
    ffh2.restart = hio_read8(f);		/* Restart pos */
    for (i = 0; i < 256; i++)
	ffh2.patsize[i] = hio_read16l(f);	/* Size of each pattern in bytes */

    mod->chn = 16;
    /*mod->pat=ffh2.patterns; (Error in specs? --claudio) */
    mod->len = ffh2.songlen;
    mod->spd = 6;
    mod->bpm = 8 * 60 / ffh.tempo;
    memcpy (mod->xxo, ffh2.order, mod->len);

    for (mod->pat = i = 0; i < 256; i++) {
	if (ffh2.patsize[i])
	    mod->pat = i + 1;
    }

    mod->trk = mod->chn * mod->pat;

    strncpy(mod->name, (char *)ffh.name, 40);
    set_type(m, "Farandole Composer %d.%d", MSN(ffh.version), LSN(ffh.version));

    MODULE_INFO();

    if (pattern_init(mod) < 0)
	return -1;

    /* Read and convert patterns */
    D_(D_INFO "Comment bytes  : %d", ffh.textlen);
    D_(D_INFO "Stored patterns: %d", mod->pat);

    for (i = 0; i < mod->pat; i++) {
	uint8 brk, note, ins, vol, fxb;

	if (pattern_alloc(mod, i) < 0)
	    return -1;

	if (!ffh2.patsize[i])
	    continue;

	mod->xxp[i]->rows = (ffh2.patsize[i] - 2) / 64;

	if (tracks_in_pattern_alloc(mod, i) < 0)
	    return -1;

	brk = hio_read8(f) + 1;
	hio_read8(f);

	for (j = 0; j < mod->xxp[i]->rows * mod->chn; j++) {
	    event = &EVENT(i, j % mod->chn, j / mod->chn);

	    if ((j % mod->chn) == 0 && (j / mod->chn) == brk)
		event->f2t = FX_BREAK;
	
	    note = hio_read8(f);
	    ins = hio_read8(f);
	    vol = hio_read8(f);
	    fxb = hio_read8(f);

	    if (note)
		event->note = note + 48;
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
	    case FX_SPEED:
		event->fxp = 8 * 60 / event->fxp;
		break;
	    }
	}
    }

    mod->ins = -1;
    hio_read(sample_map, 1, 8, f);
    for (i = 0; i < 64; i++) {
	if (sample_map[i / 8] & (1 << (i % 8)))
		mod->ins = i;
    }
    mod->ins++;

    mod->smp = mod->ins;

    if (instrument_init(mod) < 0)
	return -1;

    /* Read and convert instruments and samples */

    for (i = 0; i < mod->ins; i++) {
	if (!(sample_map[i / 8] & (1 << (i % 8))))
		continue;

	if (subinstrument_alloc(mod, i, 1) < 0)
	    return -1;

	hio_read(&fih.name, 32, 1, f);	/* Instrument name */
	fih.length = hio_read32l(f);	/* Length of sample (up to 64Kb) */
	fih.finetune = hio_read8(f);	/* Finetune (unsuported) */
	fih.volume = hio_read8(f);		/* Volume (unsuported?) */
	fih.loop_start = hio_read32l(f);	/* Loop start */
	fih.loopend = hio_read32l(f);	/* Loop end */
	fih.sampletype = hio_read8(f);	/* 1=16 bit sample */
	fih.loopmode = hio_read8(f);

	fih.length &= 0xffff;
	fih.loop_start &= 0xffff;
	fih.loopend &= 0xffff;
	mod->xxs[i].len = fih.length;
	mod->xxs[i].lps = fih.loop_start;
	mod->xxs[i].lpe = fih.loopend;
	mod->xxs[i].flg = 0;

	if (mod->xxs[i].len > 0)
		mod->xxi[i].nsm = 1;

	if (fih.sampletype != 0) {
		mod->xxs[i].flg |= XMP_SAMPLE_16BIT;
		mod->xxs[i].len >>= 1;
		mod->xxs[i].lps >>= 1;
		mod->xxs[i].lpe >>= 1;
	}

	mod->xxs[i].flg |= fih.loopmode ? XMP_SAMPLE_LOOP : 0;
	mod->xxi[i].sub[0].vol = 0xff; /* fih.volume; */
	mod->xxi[i].sub[0].sid = i;

	instrument_name(mod, i, fih.name, 32);

	D_(D_INFO "[%2X] %-32.32s %04x %04x %04x %c V%02x",
		i, mod->xxi[i].name, mod->xxs[i].len, mod->xxs[i].lps,
		mod->xxs[i].lpe, fih.loopmode ? 'L' : ' ', mod->xxi[i].sub[0].vol);

	if (load_sample(m, f, 0, &mod->xxs[i], NULL) < 0)
		return -1;
    }

    m->volbase = 0xff;

    return 0;
}
