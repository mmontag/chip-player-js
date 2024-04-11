/* Extended Module Player
 * Copyright (C) 1996-2021 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "loader.h"
#include "synth.h"

static int rad_test(HIO_HANDLE *, char *, const int);
static int rad_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_rad = {
	"Reality Adlib Tracker",
	rad_test,
	rad_load
};

static int rad_test(HIO_HANDLE *f, char *t, const int start)
{
	char buf[16];

	if (hio_read(buf, 1, 16, f) < 16)
		return -1;

	if (memcmp(buf, "RAD by REALiTY!!", 16))
		return -1;

	libxmp_read_title(f, t, 0);

	return 0;
}

struct rad_instrument {
	uint8 num;		/* Number of instruments that follows */
	uint8 reg[11];		/* Adlib registers */
};


static int rad_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event;
	int i, j;
	uint8 *buf;
	uint16 ppat[32];
	uint8 b, r, c;
	uint8 version;		/* Version in BCD */
	uint8 flags;		/* bit 7=descr,6=slow-time,4..0=tempo */
	int pos;

	LOAD_INIT();

	hio_seek(f, 16, SEEK_SET);		/* skip magic */
	version = hio_read8(f);
	flags = hio_read8(f);

	mod->chn = 9;
	mod->bpm = 125;
	mod->spd = flags & 0x1f;

	/* FIXME: tempo setting in RAD modules */
	if (mod->spd <= 2)
		mod->spd = 6;
	mod->smp = 0;

	libxmp_set_type(m, "RAD %d.%d", MSN(version), LSN(version));

	MODULE_INFO();

	/* Read description */
	if (flags & 0x80) {
		while ((b = hio_read8(f)) != 0);
	}

	/* Read instruments */
	D_(D_INFO "Read instruments");

	if ((pos = hio_tell(f)) < 0) {
		goto err;
	}

	mod->ins = 0;
	while ((b = hio_read8(f)) != 0) {
		if (b > mod->ins) {
			mod->ins = b;
		}
		if (hio_seek(f, 11, SEEK_CUR) < 0) {
			return -1;
		}
	}

	hio_seek(f, pos, SEEK_SET);
	mod->smp = mod->ins;

	if (libxmp_init_instrument(m) < 0) {
		goto err;
	}

	if ((buf = calloc(mod->ins, 11)) == NULL) {
		goto err;
	}

	while ((b = hio_read8(f)) != 0) {

		/* Sanity check */
		if (b > mod->ins || mod->xxs[b - 1].data != NULL) {
			goto err2;
		}

		if (hio_read(&buf[(b - 1) * 11], 1, 11, f) != 11) {
			goto err2;
		}

		if (libxmp_load_sample(m, f, SAMPLE_FLAG_ADLIB | SAMPLE_FLAG_HSC,
					&mod->xxs[b - 1], &buf[(b - 1) * 11]) < 0) {
			goto err2;
		}
	}

	for (i = 0; i < mod->ins; i++) {
		if (libxmp_alloc_subinstrument(mod, i, 1) < 0) {
			goto err2;
		}
		mod->xxi[i].sub[0].vol = 63 - (buf[i * 11 + 3] & 63);
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].sid = i;
		mod->xxi[i].nsm = 1;
	}

	free(buf);

	/* Read orders */
	mod->len = hio_read8(f);
	if (hio_error(f) || mod->len >= 128) {
		goto err;
	}

	for (j = i = 0; i < mod->len; i++) {
		b = hio_read8(f);
		if (b < 0x80)
			mod->xxo[j++] = b;
	}

	/* Read pattern pointers */
	for (mod->pat = i = 0; i < 32; i++) {
		ppat[i] = hio_read16l(f);
		if (ppat[i])
			mod->pat++;
	}
	mod->trk = mod->pat * mod->chn;

	D_(D_INFO "Module length: %d", mod->len);
	D_(D_INFO "Instruments: %d", mod->ins);
	D_(D_INFO "Stored patterns: %d", mod->pat);

	if (libxmp_init_pattern(mod) < 0) {
		goto err;
	}

	/* Read and convert patterns */
	for (i = 0; i < mod->pat; i++) {
		if (libxmp_alloc_pattern_tracks(mod, i, 64) < 0) {
			goto err;
		}

		if (ppat[i] == 0) {
			continue;
		}

		if (hio_seek(f, start + ppat[i], SEEK_SET) < 0) {
			goto err;
		}

		do {
			r = hio_read8(f);		/* Row number */

			if ((r & 0x7f) >= 64) {
				D_(D_CRIT "** Whoops! row = %d\n", r);
				goto err;
			}

			do {
				c = hio_read8(f);	/* Channel number */

				/* Sanity check */
				if ((c & 0x7f) >= mod->chn || (r & 0x7f) >= 64) {
					goto err;
				}

				event = &EVENT(i, c & 0x7f, r & 0x7f);

				b = hio_read8(f);	/* Note + octave + inst */
				event->ins = (b & 0x80) >> 3;
				event->note = LSN(b);

				if (event->note == 15)
					event->note = XMP_KEY_OFF;
				else if (event->note)
					event->note += 25 +
						12 * ((b & 0x70) >> 4);

				b = hio_read8(f);	/* Inst + effect */
				event->ins |= MSN(b);
				event->fxt = LSN(b);
				if (event->fxt) {
					b = hio_read8(f); /* Effect parameter */
					event->fxp = b;

					switch (event->fxt) {
					case 0x05:
						if (event->fxp > 0 && event->fxp < 50) {
							event->f2t = FX_VOLSLIDE_DN;
						} else if (event->fxp > 50 && event->fxp < 100) {
							event->f2t = FX_VOLSLIDE_UP;
						}
						event->f2p = event->fxp;
						event->fxt = 0x03;
						event->fxp = 0;
						
					case 0x0a:
						if (event->fxp > 0 && event->fxp < 50) {
							event->fxt = FX_VOLSLIDE_DN;
						} else if (event->fxp > 50 && event->fxp < 100) {
							event->fxt = FX_VOLSLIDE_UP;
						}
						break;
					case 0x0f:
						/* FIXME: tempo setting */
					    	if (event->fxp <= 2) {
							event->fxp = 6;
						}
						break;
					}
				}
			} while (~c & 0x80);
		} while (~r & 0x80);
	}

	for (i = 0; i < mod->chn; i++) {
		mod->xxc[i].pan = 0x80;
		mod->xxc[i].flg = XMP_CHANNEL_SYNTH;
	}

	m->synth = &synth_adlib;

	m->quirk |= QUIRK_LINEAR | QUIRK_VSALL | QUIRK_PBALL | QUIRK_VIBALL;

	return 0;

    err2:
	free(buf);
    err:
	return -1;
}
