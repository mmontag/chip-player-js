/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

/* Based on the format description by FreeJack of The Elven Nation
 *
 * The loader recognizes four subformats:
 * - MAS_UTrack_V001: Ultra Tracker version < 1.4
 * - MAS_UTrack_V002: Ultra Tracker version 1.4
 * - MAS_UTrack_V003: Ultra Tracker version 1.5
 * - MAS_UTrack_V004: Ultra Tracker version 1.6
 */

#include "period.h"
#include "loader.h"


static int ult_test (FILE *, char *, const int);
static int ult_load (struct module_data *, FILE *, const int);

const struct format_loader ult_loader = {
    "Ultra Tracker (ULT)",
    ult_test,
    ult_load
};

static int ult_test(FILE *f, char *t, const int start)
{
    char buf[15];

    if (fread(buf, 1, 15, f) < 15)
	return -1;

    if (memcmp(buf, "MAS_UTrack_V000", 14))
	return -1;

    if (buf[14] < '0' || buf[14] > '4')
	return -1;

    read_title(f, t, 32);

    return 0;
}


#define KEEP_TONEPORTA 32	/* Rows to keep portamento effect */

struct ult_header {
    uint8 magic[15];		/* 'MAS_UTrack_V00x' */
    uint8 name[32];		/* Song name */
    uint8 msgsize;		/* ver < 1.4: zero */
};

struct ult_header2 {
    uint8 order[256];		/* Orders */
    uint8 channels;		/* Number of channels - 1 */
    uint8 patterns;		/* Number of patterns - 1 */
};

struct ult_instrument {
    uint8 name[32];		/* Instrument name */
    uint8 dosname[12];		/* DOS file name */
    uint32 loop_start;		/* Loop start */
    uint32 loopend;		/* Loop end */
    uint32 sizestart;		/* Sample size is sizeend - sizestart */
    uint32 sizeend;
    uint8 volume;		/* Volume (log; ver >= 1.4 linear) */
    uint8 bidiloop;		/* Sample loop flags */
    uint16 finetune;		/* Finetune */
    uint16 c2spd;		/* C2 frequency */
};

struct ult_event {
    /* uint8 note; */
    uint8 ins;
    uint8 fxt;			/* MSN = fxt, LSN = f2t */
    uint8 f2p;			/* Secondary comes first -- little endian! */
    uint8 fxp;
};


static int ult_load(struct module_data *m, FILE *f, const int start)
{
    struct xmp_module *mod = &m->mod;
    int i, j, k, ver, cnt;
    struct xmp_event *event;
    struct ult_header ufh;
    struct ult_header2 ufh2;
    struct ult_instrument uih;
    struct ult_event ue;
    char *verstr[4] = { "< 1.4", "1.4", "1.5", "1.6" };

    int keep_porta1 = 0, keep_porta2 = 0;
    uint8 x8;

    LOAD_INIT();

    fread(&ufh.magic, 15, 1, f);
    fread(&ufh.name, 32, 1, f);
    ufh.msgsize = read8(f);

    ver = ufh.magic[14] - '0';

    strncpy(mod->name, (char *)ufh.name, 32);
    ufh.name[0] = 0;
    set_type(m, "Ultra Tracker %s ULT V%04d", verstr[ver - 1], ver);

    MODULE_INFO();

    fseek(f, ufh.msgsize * 32, SEEK_CUR);

    mod->ins = mod->smp = read8(f);
    /* mod->flg |= XXM_FLG_LINEAR; */

    /* Read and convert instruments */

    INSTRUMENT_INIT();

    D_(D_INFO "Instruments: %d", mod->ins);

    for (i = 0; i < mod->ins; i++) {
	mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);

	fread(&uih.name, 32, 1, f);
	fread(&uih.dosname, 12, 1, f);
	uih.loop_start = read32l(f);
	uih.loopend = read32l(f);
	uih.sizestart = read32l(f);
	uih.sizeend = read32l(f);
	uih.volume = read8(f);
	uih.bidiloop = read8(f);
	uih.finetune = read16l(f);
	uih.c2spd = ver < 4 ? 0 : read16l(f);

	if (ver > 3) {			/* Incorrect in ult_form.txt */
	    uih.c2spd ^= uih.finetune;
	    uih.finetune ^= uih.c2spd;
	    uih.c2spd ^= uih.finetune;
	}
	mod->xxs[i].len = uih.sizeend - uih.sizestart;
	mod->xxi[i].nsm = !!mod->xxs[i].len;
	mod->xxs[i].lps = uih.loop_start;
	mod->xxs[i].lpe = uih.loopend;

	/* BiDi Loop : (Bidirectional Loop)
	 *
	 * UT takes advantage of the Gus's ability to loop a sample in
	 * several different ways. By setting the Bidi Loop, the sample can
	 * be played forward or backwards, looped or not looped. The Bidi
	 * variable also tracks the sample resolution (8 or 16 bit).
	 *
	 * The following table shows the possible values of the Bidi Loop.
	 * Bidi = 0  : No looping, forward playback,  8bit sample
	 * Bidi = 4  : No Looping, forward playback, 16bit sample
	 * Bidi = 8  : Loop Sample, forward playback, 8bit sample
	 * Bidi = 12 : Loop Sample, forward playback, 16bit sample
	 * Bidi = 24 : Loop Sample, reverse playback 8bit sample
	 * Bidi = 28 : Loop Sample, reverse playback, 16bit sample
	 */

	/* Claudio's note: I'm ignoring reverse playback for samples */

	switch (uih.bidiloop) {
	case 20:		/* Type 20 is in seasons.ult */
	case 4:
	    mod->xxs[i].flg = XMP_SAMPLE_16BIT;
	    break;
	case 8:
	    mod->xxs[i].flg = XMP_SAMPLE_LOOP;
	    break;
	case 12:
	    mod->xxs[i].flg = XMP_SAMPLE_16BIT | XMP_SAMPLE_LOOP;
	    break;
	case 24:
	    mod->xxs[i].flg = XMP_SAMPLE_LOOP | XMP_SAMPLE_LOOP_REVERSE;
	    break;
	case 28:
	    mod->xxs[i].flg = XMP_SAMPLE_16BIT | XMP_SAMPLE_LOOP | XMP_SAMPLE_LOOP_REVERSE;
	    break;
	}

/* TODO: Add logarithmic volume support */
	mod->xxi[i].sub[0].vol = uih.volume;
	mod->xxi[i].sub[0].pan = 0x80;
	mod->xxi[i].sub[0].sid = i;

	copy_adjust(mod->xxi[i].name, uih.name, 24);

	D_(D_INFO "[%2X] %-32.32s %05x%c%05x %05x %c V%02x F%04x %5d",
		i, uih.name, mod->xxs[i].len,
		mod->xxs[i].flg & XMP_SAMPLE_16BIT ? '+' : ' ',
		mod->xxs[i].lps, mod->xxs[i].lpe,
		mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		mod->xxi[i].sub[0].vol, uih.finetune, uih.c2spd);

	if (ver > 3)
	    c2spd_to_note(uih.c2spd, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);
    }

    fread(&ufh2.order, 256, 1, f);
    ufh2.channels = read8(f);
    ufh2.patterns = read8(f);

    for (i = 0; i < 256; i++) {
	if (ufh2.order[i] == 0xff)
	    break;
	mod->xxo[i] = ufh2.order[i];
    }
    mod->len = i;
    mod->chn = ufh2.channels + 1;
    mod->pat = ufh2.patterns + 1;
    mod->spd = 6;
    mod->bpm = 125;
    mod->trk = mod->chn * mod->pat;

    for (i = 0; i < mod->chn; i++) {
	if (ver >= 3) {
	    x8 = read8(f);
	    mod->xxc[i].pan = 255 * x8 / 15;
	} else {
	    mod->xxc[i].pan = (((i + 1) / 2) % 2) * 0xff;	/* ??? */
	}
    }

    PATTERN_INIT();

    /* Read and convert patterns */

    D_(D_INFO "Stored patterns: %d", mod->pat);

    /* Events are stored by channel */
    for (i = 0; i < mod->pat; i++) {
	PATTERN_ALLOC (i);
	mod->xxp[i]->rows = 64;
	TRACK_ALLOC (i);
    }

    for (i = 0; i < mod->chn; i++) {
	for (j = 0; j < 64 * mod->pat; ) {
	    cnt = 1;
	    x8 = read8(f);		/* Read note or repeat code (0xfc) */
	    if (x8 == 0xfc) {
		cnt = read8(f);			/* Read repeat count */
		x8 = read8(f);			/* Read note */
	    }
	    fread(&ue, 4, 1, f);		/* Read rest of the event */

	    if (cnt == 0)
		cnt++;

	    for (k = 0; k < cnt; k++, j++) {
		event = &EVENT (j >> 6, i , j & 0x3f);
		memset(event, 0, sizeof (struct xmp_event));
		if (x8)
		    event->note = x8 + 36;
		event->ins = ue.ins;
		event->fxt = MSN (ue.fxt);
		event->f2t = LSN (ue.fxt);
		event->fxp = ue.fxp;
		event->f2p = ue.f2p;

		switch (event->fxt) {
		case 0x00:		/* <mumble> */
		    if (event->fxp)
			keep_porta1 = 0;
		    if (keep_porta1) {
			event->fxt = 0x03;
			keep_porta1--;
		    }
		    break;
		case 0x03:		/* Portamento kludge */
		    keep_porta1 = KEEP_TONEPORTA;
		    break;
		case 0x05:		/* 'Special' effect */
		case 0x06:		/* Reserved */
		    event->fxt = event->fxp = 0;
		    break;
		case 0x0b:		/* Pan */
		    event->fxt = FX_SETPAN;
		    event->fxp <<= 4;
		    break;
		case 0x09:		/* Sample offset */
/* TODO: fine sample offset */
		    event->fxp <<= 2;
		    break;
		}

		switch (event->f2t) {
		case 0x00:		/* <mumble> */
		    if (event->f2p)
			keep_porta2 = 0;
		    if (keep_porta2) {
			event->f2t = 0x03;
			keep_porta2--;
		    }
		    break;
		case 0x03:		/* Portamento kludge */
		    keep_porta2 = KEEP_TONEPORTA;
		    break;
		case 0x05:		/* 'Special' effect */
		case 0x06:		/* Reserved */
		    event->f2t = event->f2p = 0;
		    break;
		case 0x0b:		/* Pan */
		    event->f2t = FX_SETPAN;
		    event->f2p <<= 4;
		    break;
		case 0x09:		/* Sample offset */
/* TODO: fine sample offset */
		    event->f2p <<= 2;
		    break;
		}

	    }
	}
    }

    D_(D_INFO "Stored samples: %d", mod->smp);

    for (i = 0; i < mod->ins; i++) {
	if (!mod->xxs[i].len)
	    continue;
	load_sample(f, 0, &mod->xxs[i], NULL);
    }

    m->volbase = 0x100;

    return 0;
}

