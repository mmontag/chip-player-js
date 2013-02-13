/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

/* Based on the DIGI Booster player v1.6 by Tap (Tomasz Piasta), with the
 * help of Louise Heimann <louise.heimann@softhome.net>. The following
 * DIGI Booster effects are _NOT_ recognized by this player:
 *
 * 8xx robot
 * e00 filter off
 * e01 filter on
 * e30 backwd play sample
 * e31 backwd play sample+loop
 * e50 channel off
 * e51 channel on
 * e8x sample offset 2
 * e9x retrace
 */

#include "loader.h"


static int digi_test (FILE *, char *, const int);
static int digi_load (struct module_data *, FILE *, const int);

const struct format_loader digi_loader = {
    "DIGI Booster",
    digi_test,
    digi_load
};

static int digi_test(FILE *f, char *t, const int start)
{
    char buf[20];

    if (fread(buf, 1, 20, f) < 20)
	return -1;

    if (memcmp(buf, "DIGI Booster module", 19))
	return -1;

    fseek(f, 156, SEEK_CUR);
    fseek(f, 3 * 4 * 32, SEEK_CUR);
    fseek(f, 2 * 1 * 32, SEEK_CUR);

    read_title(f, t, 32);

    return 0;
}


struct digi_header {
    uint8 id[20];		/* ID: "DIGI Booster module\0" */
    uint8 vstr[4];		/* Version string: "Vx.y" */
    uint8 ver;			/* Version hi-nibble.lo-nibble */
    uint8 chn;			/* Number of channels */
    uint8 pack;			/* PackEnable */
    uint8 unknown[19];		/* ??!? */
    uint8 pat;			/* Number of patterns */
    uint8 len;			/* Song length */
    uint8 ord[128];		/* Orders */
    uint32 slen[31];		/* Sample length for 31 samples */
    uint32 sloop[31];		/* Sample loop start for 31 samples */
    uint32 sllen[31];		/* Sample loop length for 31 samples */
    uint8 vol[31];		/* Instrument volumes */
    uint8 fin[31];		/* Finetunes */
    uint8 title[32];		/* Song name */
    uint8 insname[31][30];	/* Instrument names */
};


static int digi_load(struct module_data *m, FILE *f, const int start)
{
    struct xmp_module *mod = &m->mod;
    struct xmp_event *event = 0;
    struct digi_header dh;
    uint8 digi_event[4], chn_table[64];
    uint16 w;
    int i, j, k, c;

    LOAD_INIT();

    fread(&dh.id, 20, 1, f);

    fread(&dh.vstr, 4, 1, f);
    dh.ver = read8(f);
    dh.chn = read8(f);
    dh.pack = read8(f);
    fread(&dh.unknown, 19, 1, f);
    dh.pat = read8(f);
    dh.len = read8(f);
    fread(&dh.ord, 128, 1, f);

    for (i = 0; i < 31; i++)
	dh.slen[i] = read32b(f);
    for (i = 0; i < 31; i++)
	dh.sloop[i] = read32b(f);
    for (i = 0; i < 31; i++)
	dh.sllen[i] = read32b(f);
    for (i = 0; i < 31; i++)
	dh.vol[i] = read8(f);
    for (i = 0; i < 31; i++)
	dh.fin[i] = read8(f);

    fread(&dh.title, 32, 1, f);

    for (i = 0; i < 31; i++)
        fread(&dh.insname[i], 30, 1, f);

    mod->ins = 31;
    mod->smp = mod->ins;
    mod->pat = dh.pat + 1;
    mod->chn = dh.chn;
    mod->trk = mod->pat * mod->chn;
    mod->len = dh.len + 1;

    m->quirk |= QUIRK_MODRNG;

    copy_adjust(mod->name, dh.title, 32);
    set_type(m, "DIGI Booster %-4.4s", dh.vstr);

    MODULE_INFO();
 
    for (i = 0; i < mod->len; i++)
	mod->xxo[i] = dh.ord[i];
 
    INSTRUMENT_INIT();

    /* Read and convert instruments and samples */

    for (i = 0; i < mod->ins; i++) {
	mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
	mod->xxi[i].nsm = !!(mod->xxs[i].len = dh.slen[i]);
	mod->xxs[i].lps = dh.sloop[i];
	mod->xxs[i].lpe = dh.sloop[i] + dh.sllen[i];
	mod->xxs[i].flg = mod->xxs[i].lpe > 0 ? XMP_SAMPLE_LOOP : 0;
	mod->xxi[i].sub[0].vol = dh.vol[i];
	mod->xxi[i].sub[0].fin = dh.fin[i];
	mod->xxi[i].sub[0].pan = 0x80;
	mod->xxi[i].sub[0].sid = i;

	copy_adjust(mod->xxi[i].name, dh.insname[i], 30);

	D_(D_INFO "[%2X] %-30.30s %04x %04x %04x %c V%02x", i,
		mod->xxi[i].name, mod->xxs[i].len, mod->xxs[i].lps, mod->xxs[i].lpe,
		mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ', mod->xxi[i].sub[0].vol);
    }

    PATTERN_INIT();

    /* Read and convert patterns */
    D_(D_INFO "Stored patterns: %d", mod->pat);

    for (i = 0; i < mod->pat; i++) {
	PATTERN_ALLOC (i);
	mod->xxp[i]->rows = 64;
	TRACK_ALLOC (i);

	if (dh.pack) {
	    w = (read16b(f) - 64) >> 2;
	    fread (chn_table, 1, 64, f);
	} else {
	    w = 64 * mod->chn;
	    memset (chn_table, 0xff, 64);
	}

	for (j = 0; j < 64; j++) {
	    for (c = 0, k = 0x80; c < mod->chn; c++, k >>= 1) {
	        if (chn_table[j] & k) {
		    fread (digi_event, 4, 1, f);
		    event = &EVENT (i, c, j);
	            cvt_pt_event(event, digi_event);
		    switch (event->fxt) {
		    case 0x08:		/* Robot */
			event->fxt = event->fxp = 0;
			break;
		    case 0x0e:
			switch (MSN (event->fxp)) {
			case 0x00:
			case 0x03:
			case 0x08:
			case 0x09:
			    event->fxt = event->fxp = 0;
			    break;
			case 0x04:
			    event->fxt = 0x0c;
			    event->fxp = 0x00;
			    break;
			}
		    }
		    w--;
		}
	    }
	}

	if (w) {
	    D_(D_CRIT "Corrupted file (w = %d)", w);
	}
    }

    /* Read samples */
    D_(D_INFO "Stored samples: %d", mod->smp);
    for (i = 0; i < mod->ins; i++) {
	load_sample(f, 0, &mod->xxs[mod->xxi[i].sub[0].sid], NULL);
    }

    return 0;
}
