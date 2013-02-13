/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "loader.h"


struct mtm_file_header {
	uint8 magic[3];		/* "MTM" */
	uint8 version;		/* MSN=major, LSN=minor */
	uint8 name[20];		/* ASCIIZ Module name */
	uint16 tracks;		/* Number of tracks saved */
	uint8 patterns;		/* Number of patterns saved */
	uint8 modlen;		/* Module length */
	uint16 extralen;	/* Length of the comment field */
	uint8 samples;		/* Number of samples */
	uint8 attr;		/* Always zero */
	uint8 rows;		/* Number rows per track */
	uint8 channels;		/* Number of tracks per pattern */
	uint8 pan[32];		/* Pan positions for each channel */
};

struct mtm_instrument_header {
	uint8 name[22];		/* Instrument name */
	uint32 length;		/* Instrument length in bytes */
	uint32 loop_start;	/* Sample loop start */
	uint32 loopend;		/* Sample loop end */
	uint8 finetune;		/* Finetune */
	uint8 volume;		/* Playback volume */
	uint8 attr;		/* &0x01: 16bit sample */
};


static int mtm_test (FILE *, char *, const int);
static int mtm_load (struct module_data *, FILE *, const int);

const struct format_loader mtm_loader = {
    "Multitracker (MTM)",
    mtm_test,
    mtm_load
};

static int mtm_test(FILE *f, char *t, const int start)
{
    uint8 buf[4];

    if (fread(buf, 1, 4, f) < 4)
	return -1;
    if (memcmp(buf, "MTM", 3))
	return -1;
    if (buf[3] != 0x10)
	return -1;

    read_title(f, t, 20);

    return 0;
}


static int mtm_load(struct module_data *m, FILE *f, const int start)
{
    struct xmp_module *mod = &m->mod;
    int i, j;
    struct mtm_file_header mfh;
    struct mtm_instrument_header mih;
    uint8 mt[192];
    uint16 mp[32];

    LOAD_INIT();

    fread(&mfh.magic, 3, 1, f);		/* "MTM" */
    mfh.version = read8(f);		/* MSN=major, LSN=minor */
    fread(&mfh.name, 20, 1, f);		/* ASCIIZ Module name */
    mfh.tracks = read16l(f);		/* Number of tracks saved */
    mfh.patterns = read8(f);		/* Number of patterns saved */
    mfh.modlen = read8(f);		/* Module length */
    mfh.extralen = read16l(f);		/* Length of the comment field */
    mfh.samples = read8(f);		/* Number of samples */
    mfh.attr = read8(f);		/* Always zero */
    mfh.rows = read8(f);		/* Number rows per track */
    mfh.channels = read8(f);		/* Number of tracks per pattern */
    fread(&mfh.pan, 32, 1, f);		/* Pan positions for each channel */

#if 0
    if (strncmp ((char *)mfh.magic, "MTM", 3))
	return -1;
#endif

    mod->trk = mfh.tracks + 1;
    mod->pat = mfh.patterns + 1;
    mod->len = mfh.modlen + 1;
    mod->ins = mfh.samples;
    mod->smp = mod->ins;
    mod->chn = mfh.channels;
    mod->spd = 6;
    mod->bpm = 125;

    strncpy(mod->name, (char *)mfh.name, 20);
    set_type(m, "MultiTracker %d.%02d MTM", MSN(mfh.version), LSN(mfh.version));

    MODULE_INFO();

    INSTRUMENT_INIT();

    /* Read and convert instruments */
    for (i = 0; i < mod->ins; i++) {
	mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);

	fread(&mih.name, 22, 1, f);		/* Instrument name */
	mih.length = read32l(f);		/* Instrument length in bytes */
	mih.loop_start = read32l(f);		/* Sample loop start */
	mih.loopend = read32l(f);		/* Sample loop end */
	mih.finetune = read8(f);		/* Finetune */
	mih.volume = read8(f);			/* Playback volume */
	mih.attr = read8(f);			/* &0x01: 16bit sample */

	mod->xxs[i].len = mih.length;
	mod->xxi[i].nsm = mih.length > 0 ? 1 : 0;
	mod->xxs[i].lps = mih.loop_start;
	mod->xxs[i].lpe = mih.loopend;
	mod->xxs[i].flg = mod->xxs[i].lpe ? XMP_SAMPLE_LOOP : 0;	/* 1 == Forward loop */
	if (mfh.attr & 1) {
	    mod->xxs[i].flg |= XMP_SAMPLE_16BIT;
	    mod->xxs[i].len >>= 1;
	    mod->xxs[i].lps >>= 1;
	    mod->xxs[i].lpe >>= 1;
	}

	mod->xxi[i].sub[0].vol = mih.volume;
	mod->xxi[i].sub[0].fin = 0x80 + (int8)(mih.finetune << 4);
	mod->xxi[i].sub[0].pan = 0x80;
	mod->xxi[i].sub[0].sid = i;

	copy_adjust(mod->xxi[i].name, mih.name, 22);

	D_(D_INFO "[%2X] %-22.22s %04x%c%04x %04x %c V%02x F%+03d\n", i,
		mod->xxi[i].name, mod->xxs[i].len,
		mod->xxs[i].flg & XMP_SAMPLE_16BIT ? '+' : ' ',
		mod->xxs[i].lps, mod->xxs[i].lpe,
		mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		mod->xxi[i].sub[0].vol, mod->xxi[i].sub[0].fin - 0x80);
    }

    fread(mod->xxo, 1, 128, f);

    PATTERN_INIT();

    D_(D_INFO "Stored tracks: %d", mod->trk - 1);

    for (i = 0; i < mod->trk; i++) {
	mod->xxt[i] = calloc (sizeof (struct xmp_track) +
	    sizeof (struct xmp_event) * mfh.rows, 1);
	mod->xxt[i]->rows = mfh.rows;
	if (!i)
	    continue;
	fread (&mt, 3, 64, f);
	for (j = 0; j < 64; j++) {
	    if ((mod->xxt[i]->event[j].note = mt[j * 3] >> 2))
		mod->xxt[i]->event[j].note += 37;
	    mod->xxt[i]->event[j].ins = ((mt[j * 3] & 0x3) << 4) + MSN (mt[j * 3 + 1]);
	    mod->xxt[i]->event[j].fxt = LSN (mt[j * 3 + 1]);
	    mod->xxt[i]->event[j].fxp = mt[j * 3 + 2];
	    if (mod->xxt[i]->event[j].fxt > FX_SPEED)
		mod->xxt[i]->event[j].fxt = mod->xxt[i]->event[j].fxp = 0;
	    /* Set pan effect translation */
	    if ((mod->xxt[i]->event[j].fxt == FX_EXTENDED) &&
		(MSN (mod->xxt[i]->event[j].fxp) == 0x8)) {
		mod->xxt[i]->event[j].fxt = FX_SETPAN;
		mod->xxt[i]->event[j].fxp <<= 4;
	    }
	}
    }

    /* Read patterns */
    D_(D_INFO "Stored patterns: %d", mod->pat - 1);

    for (i = 0; i < mod->pat; i++) {
	PATTERN_ALLOC (i);
	mod->xxp[i]->rows = 64;
	for (j = 0; j < 32; j++)
	    mp[j] = read16l(f);
	for (j = 0; j < mod->chn; j++)
	    mod->xxp[i]->index[j] = mp[j];
    }

    /* Comments */
    fseek(f, mfh.extralen, SEEK_CUR);

    /* Read samples */
    D_(D_INFO "Stored samples: %d", mod->smp);

    for (i = 0; i < mod->ins; i++) {
	load_sample(f, SAMPLE_FLAG_UNS, &mod->xxs[mod->xxi[i].sub[0].sid], NULL);
    }

    for (i = 0; i < mod->chn; i++)
	mod->xxc[i].pan = mfh.pan[i] << 4;

    return 0;
}
