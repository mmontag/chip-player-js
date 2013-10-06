/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

/* Based on the format description written by Harald Zappe.
 * Additional information about Oktalyzer modules from Bernardo
 * Innocenti's XModule 3.4 sources.
 */

#include "loader.h"
#include "iff.h"


static int okt_test (HIO_HANDLE *, char *, const int);
static int okt_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader okt_loader = {
    "Oktalyzer",
    okt_test,
    okt_load
};

static int okt_test(HIO_HANDLE *f, char *t, const int start)
{
    char magic[8];

    if (hio_read(magic, 1, 8, f) < 8)
	return -1;

    if (strncmp (magic, "OKTASONG", 8))
	return -1;

    read_title(f, t, 0);

    return 0;
}


#define OKT_MODE8 0x00		/* 7 bit samples */
#define OKT_MODE4 0x01		/* 8 bit samples */
#define OKT_MODEB 0x02		/* Both */

#define NONE 0xff

struct local_data {
    int mode[36];
    int idx[36];
    int pattern;
    int sample;
};

static const int fx[] = {
    NONE,
    FX_PORTA_UP,	/*  1 */
    FX_PORTA_DN,	/*  2 */
    NONE,
    NONE,
    NONE,
    NONE,
    NONE,
    NONE,
    NONE,
    FX_OKT_ARP3,	/* 10 */
    FX_OKT_ARP4,	/* 11 */
    FX_OKT_ARP5,	/* 12 */
    FX_NSLIDE_DN,	/* 13 */
    NONE,
    NONE, /* Filter */	/* 15 */
    NONE,
    FX_NSLIDE_UP,	/* 17 */
    NONE,
    NONE,
    NONE,
    FX_F_NSLIDE_DN,	/* 21 */
    NONE,
    NONE,
    NONE,
    FX_JUMP,		/* 25 */
    NONE,
    NONE, /* Release */	/* 27 */
    FX_SPEED,		/* 28 */
    NONE,
    FX_F_NSLIDE_UP,	/* 30 */
    FX_VOLSET		/* 31 */
};


static int get_cmod(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{ 
    struct xmp_module *mod = &m->mod;
    int i, j, k;

    mod->chn = 0;
    for (i = 0; i < 4; i++) {
	j = hio_read16b(f);
	for (k = !!j; k >= 0; k--) {
	    mod->xxc[mod->chn].pan = (((i + 1) / 2) % 2) * 0xff;
	    mod->chn++;
	}
    }

    return 0;
}


static int get_samp(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
    struct xmp_module *mod = &m->mod;
    struct local_data *data = (struct local_data *)parm;
    int i, j;
    int looplen;

    /* Should be always 36 */
    mod->ins = size / 32;  /* sizeof(struct okt_instrument_header); */
    mod->smp = mod->ins;

    if (instrument_init(mod) < 0)
	return -1;

    for (j = i = 0; i < mod->ins; i++) {
	if (subinstrument_alloc(mod, i, 1) < 0)
	    return -1;

	hio_read(mod->xxi[i].name, 1, 20, f);
	str_adj((char *)mod->xxi[i].name);

	/* Sample size is always rounded down */
	mod->xxs[j].len = hio_read32b(f) & ~1;
	mod->xxs[j].lps = hio_read16b(f);
	looplen = hio_read16b(f);
	mod->xxs[j].lpe = mod->xxs[j].lps + looplen;
	mod->xxs[j].flg = looplen > 2 ? XMP_SAMPLE_LOOP : 0;

	mod->xxi[i].sub[0].vol = hio_read16b(f);
	data->mode[i] = hio_read16b(f);

	mod->xxi[i].sub[0].pan = 0x80;
	mod->xxi[i].sub[0].sid = j;

	data->idx[j] = i;

	if (mod->xxs[j].len > 0) {
	    mod->xxi[j].nsm = 1;
	    j++;
	}
    }

    return 0;
}


static int get_spee(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
    struct xmp_module *mod = &m->mod;

    mod->spd = hio_read16b(f);
    mod->bpm = 125;

    return 0;
}


static int get_slen(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
    struct xmp_module *mod = &m->mod;

    mod->pat = hio_read16b(f);
    mod->trk = mod->pat * mod->chn;

    return 0;
}


static int get_plen(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
    struct xmp_module *mod = &m->mod;

    mod->len = hio_read16b(f);
    D_(D_INFO "Module length: %d", mod->len);

    return 0;
}


static int get_patt(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
    struct xmp_module *mod = &m->mod;

    hio_read(mod->xxo, 1, mod->len, f);

    return 0;
}


static int get_pbod(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
    struct xmp_module *mod = &m->mod;
    struct local_data *data = (struct local_data *)parm;

    int j;
    uint16 rows;
    struct xmp_event *event;

    if (data->pattern >= mod->pat)
	return 0;

    if (!data->pattern) {
	if (pattern_init(mod) < 0)
	    return -1;
	D_(D_INFO "Stored patterns: %d", mod->pat);
    }

    rows = hio_read16b(f);

    if (pattern_tracks_alloc(mod, data->pattern, rows) < 0)
	return -1;

    for (j = 0; j < rows * mod->chn; j++) {
	uint8 note, ins;

	event = &EVENT(data->pattern, j % mod->chn, j / mod->chn);
	memset(event, 0, sizeof(struct xmp_event));

	note = hio_read8(f);
	ins = hio_read8(f);

	if (note) {
	    event->note = 48 + note;
	    event->ins = 1 + ins;
	}

	event->fxt = fx[hio_read8(f)];
	event->fxp = hio_read8(f);

	if ((event->fxt == FX_VOLSET) && (event->fxp > 0x40)) {
	    if (event->fxp <= 0x50) {
		event->fxt = FX_VOLSLIDE;
		event->fxp -= 0x40;
	    } else if (event->fxp <= 0x60) {
		event->fxt = FX_VOLSLIDE;
		event->fxp = (event->fxp - 0x50) << 4;
	    } else if (event->fxp <= 0x70) {
		event->fxt = FX_F_VSLIDE_DN;
		event->fxp = event->fxp - 0x60;
	    } else if (event->fxp <= 0x80) {
		event->fxt = FX_F_VSLIDE_UP;
		event->fxp = event->fxp - 0x70;
	    }
	}
	if (event->fxt == FX_ARPEGGIO)	/* Arpeggio fixup */
	    event->fxp = (((24 - MSN (event->fxp)) % 12) << 4) | LSN (event->fxp);
	if (event->fxt == NONE)
	    event->fxt = event->fxp = 0;
    }
    data->pattern++;

    return 0;
}


static int get_sbod(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
    struct xmp_module *mod = &m->mod;
    struct local_data *data = (struct local_data *)parm;
    int flags = 0;
    int i;

    if (data->sample >= mod->ins)
	return 0;

    D_(D_INFO "Stored samples: %d", mod->smp);

    i = data->idx[data->sample];
    if (data->mode[i] == OKT_MODE8 || data->mode[i] == OKT_MODEB)
	flags = SAMPLE_FLAG_7BIT;

    if (load_sample(m, f, flags, &mod->xxs[mod->xxi[i].sub[0].sid], NULL) < 0)
	return -1;

    data->sample++;

    return 0;
}


static int okt_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
    iff_handle handle;
    struct local_data data;
    int ret;

    LOAD_INIT();

    hio_seek(f, 8, SEEK_CUR);	/* OKTASONG */

    handle = iff_new();
    if (handle == NULL)
	return -1;

    memset(&data, 0, sizeof(struct local_data));

    /* IFF chunk IDs */
    ret = iff_register(handle, "CMOD", get_cmod);
    ret |= iff_register(handle, "SAMP", get_samp);
    ret |= iff_register(handle, "SPEE", get_spee);
    ret |= iff_register(handle, "SLEN", get_slen);
    ret |= iff_register(handle, "PLEN", get_plen);
    ret |= iff_register(handle, "PATT", get_patt);
    ret |= iff_register(handle, "PBOD", get_pbod);
    ret |= iff_register(handle, "SBOD", get_sbod);

    if (ret != 0)
	return -1;

    set_type(m, "Oktalyzer");

    MODULE_INFO();

    /* Load IFF chunks */
    if (iff_load(handle, m, f, &data) < 0) {
	iff_release(handle);
	return -1;
    }

    iff_release(handle);

    return 0;
}
