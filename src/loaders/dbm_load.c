/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

/* Based on DigiBooster_E.guide from the DigiBoosterPro 2.20 package.
 * DigiBooster Pro written by Tomasz & Waldemar Piasta
 */

#include "loader.h"
#include "iff.h"
#include "period.h"

#define MAGIC_DBM0	MAGIC4('D','B','M','0')


static int dbm_test(HIO_HANDLE *, char *, const int);
static int dbm_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader dbm_loader = {
	"DigiBooster Pro (DBM)",
	dbm_test,
	dbm_load
};

static int dbm_test(HIO_HANDLE * f, char *t, const int start)
{
	if (hio_read32b(f) != MAGIC_DBM0)
		return -1;

	hio_seek(f, 12, SEEK_CUR);
	read_title(f, t, 44);

	return 0;
}


struct local_data {
	int have_song;
};


static int get_info(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;

	mod->ins = hio_read16b(f);
	mod->smp = hio_read16b(f);
	hio_read16b(f);			/* Songs */
	mod->pat = hio_read16b(f);
	mod->chn = hio_read16b(f);

	mod->trk = mod->pat * mod->chn;

	if (instrument_init(mod) < 0)
		return -1;

	return 0;
}

static int get_song(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	int i;
	char buffer[50];

	if (data->have_song)
		return 0;

	data->have_song = 1;

	hio_read(buffer, 44, 1, f);
	D_(D_INFO "Song name: %s", buffer);

	mod->len = hio_read16b(f);
	D_(D_INFO "Song length: %d patterns", mod->len);

	for (i = 0; i < mod->len; i++)
		mod->xxo[i] = hio_read16b(f);

	return 0;
}

static int get_inst(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i;
	int c2spd, flags, snum;
	uint8 buffer[50];

	D_(D_INFO "Instruments: %d", mod->ins);

	for (i = 0; i < mod->ins; i++) {
		mod->xxi[i].nsm = 1;
		if (subinstrument_alloc(mod, i, 1) < 0)
			return -1;

		hio_read(buffer, 30, 1, f);
		instrument_name(mod, i, buffer, 30);
		snum = hio_read16b(f);
		if (snum == 0 || snum > mod->smp)
			continue;

		mod->xxi[i].sub[0].sid = --snum;
		mod->xxi[i].sub[0].vol = hio_read16b(f);
		c2spd = hio_read32b(f);
		mod->xxs[snum].lps = hio_read32b(f);
		mod->xxs[snum].lpe = mod->xxs[i].lps + hio_read32b(f);
		mod->xxi[i].sub[0].pan = 0x80 + (int16)hio_read16b(f);
		if (mod->xxi[i].sub[0].pan > 0xff)
			mod->xxi[i].sub[0].pan = 0xff;
		flags = hio_read16b(f);
		mod->xxs[snum].flg = flags & 0x03 ? XMP_SAMPLE_LOOP : 0;
		mod->xxs[snum].flg |= flags & 0x02 ? XMP_SAMPLE_LOOP_BIDIR : 0;

		c2spd_to_note(c2spd, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);

		D_(D_INFO "[%2X] %-30.30s #%02X V%02x P%02x %5d",
			i, mod->xxi[i].name, snum,
			mod->xxi[i].sub[0].vol, mod->xxi[i].sub[0].pan, c2spd);
	}

	return 0;
}

static int get_patt(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i, c, r, n, sz;
	struct xmp_event *event, dummy;
	uint8 x;

	if (pattern_init(mod) < 0)
		return -1;

	D_(D_INFO "Stored patterns: %d ", mod->pat);

	/*
	 * Note: channel and flag bytes are inverted in the format
	 * description document
	 */

	for (i = 0; i < mod->pat; i++) {
		if (pattern_tracks_alloc(mod, i, hio_read16b(f)) < 0)
			return -1;

		sz = hio_read32b(f);
		//printf("rows = %d, size = %d\n", mod->xxp[i]->rows, sz);

		r = 0;
		c = -1;

		while (sz > 0) {
			//printf("  offset=%x,  sz = %d, ", hio_tell(f), sz);
			c = hio_read8(f);
			if (--sz <= 0) break;
			//printf("c = %02x\n", c);

			if (c == 0) {
				r++;
				c = -1;
				continue;
			}
			c--;

			n = hio_read8(f);
			if (--sz <= 0) break;
			//printf("    n = %d\n", n);

			if (c >= mod->chn || r >= mod->xxp[i]->rows)
				event = &dummy;
			else
				event = &EVENT(i, c, r);

			if (n & 0x01) {
				x = hio_read8(f);
				event->note = 13 + MSN(x) * 12 + LSN(x);
				if (--sz <= 0) break;
			}
			if (n & 0x02) {
				event->ins = hio_read8(f);
				if (--sz <= 0) break;
			}
			if (n & 0x04) {
				event->fxt = hio_read8(f);
				if (--sz <= 0) break;
			}
			if (n & 0x08) {
				event->fxp = hio_read8(f);
				if (--sz <= 0) break;
			}
			if (n & 0x10) {
				event->f2t = hio_read8(f);
				if (--sz <= 0) break;
			}
			if (n & 0x20) {
				event->f2p = hio_read8(f);
				if (--sz <= 0) break;
			}

			if (event->fxt == 0x1c)
				event->fxt = FX_S3M_BPM;

			if (event->fxt > 0x1c)
				event->fxt = event->f2p = 0;

			if (event->f2t == 0x1c)
				event->f2t = FX_S3M_BPM;

			if (event->f2t > 0x1c)
				event->f2t = event->f2p = 0;
		}
	}

	return 0;
}

static int get_smpl(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i, flags;

	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < mod->smp; i++) {
		flags = hio_read32b(f);
		mod->xxs[i].len = hio_read32b(f);

		if (flags & 0x02) {
			mod->xxs[i].flg |= XMP_SAMPLE_16BIT;
		}

		if (flags & 0x04) {	/* Skip 32-bit samples */
			mod->xxs[i].len <<= 2;
			hio_seek(f, mod->xxs[i].len, SEEK_CUR);
			continue;
		}
		
		if (load_sample(m, f, SAMPLE_FLAG_BIGEND, &mod->xxs[i], NULL) < 0)
			return -1;

		if (mod->xxs[i].len == 0)
			continue;

		D_(D_INFO "[%2X] %08x %05x%c%05x %05x %c",
			i, flags, mod->xxs[i].len,
			mod->xxs[i].flg & XMP_SAMPLE_16BIT ? '+' : ' ',
			mod->xxs[i].lps, mod->xxs[i].lpe,
			mod->xxs[i].flg & XMP_SAMPLE_LOOP ?
			(mod->xxs[i].flg & XMP_SAMPLE_LOOP_BIDIR ? 'B' : 'L') : ' ');

	}

	return 0;
}

static int get_venv(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i, j, nenv, ins;

	nenv = hio_read16b(f);

	D_(D_INFO "Vol envelopes  : %d ", nenv);

	for (i = 0; i < nenv; i++) {
		ins = hio_read16b(f) - 1;
		mod->xxi[ins].aei.flg = hio_read8(f) & 0x07;
		mod->xxi[ins].aei.npt = hio_read8(f);
		mod->xxi[ins].aei.sus = hio_read8(f);
		mod->xxi[ins].aei.lps = hio_read8(f);
		mod->xxi[ins].aei.lpe = hio_read8(f);
		hio_read8(f);	/* 2nd sustain */
		//hio_read8(f);	/* reserved */

		for (j = 0; j < 32; j++) {
			mod->xxi[ins].aei.data[j * 2 + 0] = hio_read16b(f);
			mod->xxi[ins].aei.data[j * 2 + 1] = hio_read16b(f);
		}
	}

	return 0;
}

static int dbm_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	iff_handle handle;
	char name[44];
	uint16 version;
	int i, ret;
	struct local_data data;

	LOAD_INIT();

	hio_read32b(f);		/* DBM0 */

	data.have_song = 0;
	version = hio_read16b(f);

	hio_seek(f, 10, SEEK_CUR);
	hio_read(name, 1, 44, f);

	handle = iff_new();
	if (handle == NULL)
		return -1;

	/* IFF chunk IDs */
	ret = iff_register(handle, "INFO", get_info);
	ret |= iff_register(handle, "SONG", get_song);
	ret |= iff_register(handle, "INST", get_inst);
	ret |= iff_register(handle, "PATT", get_patt);
	ret |= iff_register(handle, "SMPL", get_smpl);
	ret |= iff_register(handle, "VENV", get_venv);

	if (ret != 0)
		return -1;

	strncpy(mod->name, name, XMP_NAME_SIZE);
	snprintf(mod->type, XMP_NAME_SIZE, "DigiBooster Pro %d.%02x DBM0",
					version >> 8, version & 0xff);
	MODULE_INFO();

	/* Load IFF chunks */
	if (iff_load(handle, m, f, &data) < 0) {
		iff_release(handle);
		return -1;
	}

	iff_release(handle);

	for (i = 0; i < mod->chn; i++)
		mod->xxc[i].pan = 0x80;

	return 0;
}
