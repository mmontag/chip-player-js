/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdarg.h>

#include "xmp.h"
#include "common.h"
#include "period.h"
#include "load.h"

#include "spectrum.h"


char *copy_adjust(char *s, uint8 *r, int n)
{
	int i;

	memset(s, 0, n + 1);
	strncpy(s, (char *)r, n);

	for (i = 0; s[i] && i < n; i++) {
		if (!isprint(s[i]) || ((uint8)s[i] > 127))
			s[i] = '.';
	}

	while (*s && (s[strlen(s) - 1] == ' '))
		s[strlen(s) - 1] = 0;

	return s;
}

int test_name(uint8 *s, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		if (s[i] > 0x7f)
			return -1;
		if (s[i] > 0 && s[i] < 32)
			return -1;
	}

	return 0;
}

void read_title(FILE *f, char *t, int s)
{
	uint8 buf[XMP_NAME_SIZE];

	if (t == NULL)
		return;

	if (s >= XMP_NAME_SIZE)
		s = XMP_NAME_SIZE -1;

	memset(t, 0, s + 1);

	fread(buf, 1, s, f);
	buf[s] = 0;
	copy_adjust(t, buf, s);
}

void set_xxh_defaults(struct xmp_module *mod)
{
	mod->pat = 0;
	mod->trk = 0;
	mod->chn = 4;
	mod->ins = 0;
	mod->smp = 0;
	mod->tpo = 6;
	mod->bpm = 125;
	mod->len = 0;
	mod->rst = 0;
	mod->gvl = 0x40;
}

void cvt_pt_event(struct xmp_event *event, uint8 *mod_event)
{
	event->note = period_to_note((LSN(mod_event[0]) << 8) + mod_event[1]);
	event->ins = ((MSN(mod_event[0]) << 4) | MSN(mod_event[2]));
	event->fxt = LSN(mod_event[2]);
	event->fxp = mod_event[3];

	disable_continue_fx(event);
}

void disable_continue_fx(struct xmp_event *event)
{
	if (!event->fxp) {
		switch (event->fxt) {
		case 0x05:
			event->fxt = 0x03;
			break;
		case 0x06:
			event->fxt = 0x04;
			break;
		case 0x01:
		case 0x02:
		case 0x0a:
			event->fxt = 0x00;
		}
	}
}


uint8 ord_xlat[255];

/* normalize pattern sequence */
void clean_s3m_seq(struct xmp_module *mod, uint8 *xxo)
{
    int i, j;
    
    /*for (i = 0; i < len; i++) printf("%02x ", xxo[i]);
    printf("\n");*/
    for (i = j = 0; i < mod->len; i++, j++) {
	while (xxo[i] == 0xfe) {
	    mod->len--;
	    ord_xlat[j] = i;
            //printf("xlat %d -> %d\n", j, i);
	    j++;
	    //printf("move %d from %d to %d\n", len - i, i + 1, i);
	    memmove(xxo + i, xxo + i + 1, mod->len - i);
        }

	ord_xlat[j] = i;
        //printf("xlat %d -> %d\n", j, i);

	if (xxo[i] == 0xff) {
	    mod->len = i;
	    break;
	}
    }
    /*for (i = 0; i < len; i++) printf("%02x ", xxo[i]);
    printf("\n");*/
}


int check_filename_case(char *dir, char *name, char *new_name, int size)
{
	int found = 0;
	DIR *dirfd;
	struct dirent *d;

	dirfd = opendir(dir);
	if (dirfd) {
		while ((d = readdir(dirfd))) {
			if (!strcasecmp(d->d_name, name)) {
				found = 1;
				break;
			}
		}
	}

	if (found)
		strncpy(new_name, d->d_name, size);

	closedir(dirfd);

	return found;
}

void get_instrument_path(struct module_data *m, char *var, char *path, int size)
{
	if (m->instrument_path) {
		strncpy(path, m->instrument_path, size);
	} else if (var && getenv(var)) {
		strncpy(path, getenv(var), size);
	} else if (getenv("XMP_INSTRUMENT_PATH")) {
		strncpy(path, getenv("XMP_INSTRUMENT_PATH"), size);
	} else {
		strncpy(path, ".", size);
	}
}

void set_type(struct module_data *m, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	vsnprintf(m->mod.type, XMP_NAME_SIZE, fmt, ap);
	va_end(ap);
}

static void adpcm4_decoder(uint8 * inp, uint8 *outp, char *tab, int len)
{
	char delta = 0;
	uint8 b0, b1;
	int i;

	len = (len + 1) / 2;

	for (i = 0; i < len; i++) {
		b0 = *inp;
		b1 = *inp++ >> 4;
		delta += tab[b0 & 0x0f];
		*outp++ = delta;
		delta += tab[b1 & 0x0f];
		*outp++ = delta;
	}
}

static void unroll_loop(struct xmp_sample *xxs)
{
	int8 *s8;
	int16 *s16;
	int start, loop_size;
	int i;

	s16 = (int16 *)xxs->data;
	s8 = (int8 *)xxs->data;

	if (xxs->len > xxs->lpe) {
		start = xxs->lpe;
	} else {
		start = xxs->len;
	}

	loop_size = xxs->lpe - xxs->lps;

	if (xxs->flg & XMP_SAMPLE_16BIT) {
		s16 += start;
		for (i = 0; i < loop_size; i++) {
			*(s16 + i) = *(s16 - i - 1);	
		}
	} else {
		s8 += start;
		for (i = 0; i < loop_size; i++) {
			*(s8 + i) = *(s8 - i - 1);	
		}
	}
}


int load_sample(FILE *f, int id, int flags, struct xmp_sample *xxs,
                void *buffer)
{
	int bytelen, extralen, unroll_extralen;

	/* Synth patches
	 * Default is YM3128 for historical reasons
	 */
	if (flags & SAMPLE_FLAG_SYNTH) {
		int size = 11;	/* Adlib instrument size */

		if (flags & SAMPLE_FLAG_SPECTRUM)
			size = sizeof(struct spectrum_sample);

		if ((xxs->data = malloc(size)) == NULL)
			return -1;

		memcpy(xxs->data, buffer, size);

		xxs->flg |= XMP_SAMPLE_SYNTH;
		xxs->len = size;

		return 0;
	}

	/* Empty samples
	 */
	if (xxs->len == 0) {
		return 0;
	}

	/* Loop parameters sanity check
	 */
	if (xxs->lpe > xxs->len) {
		xxs->lpe = xxs->len;
	}
	if (xxs->lps >= xxs->len || xxs->lps >= xxs->lpe) {
		xxs->lps = xxs->lpe = 0;
		xxs->flg &= ~(XMP_SAMPLE_LOOP | XMP_SAMPLE_LOOP_BIDIR);
	}

	/* Patches with samples
	 * Allocate extra sample for interpolation.
	 */
	bytelen = xxs->len;
	extralen = 2;
	unroll_extralen = 0;

	/* Disable birectional loop flag if sample is not looped
	 */
	if (xxs->flg & XMP_SAMPLE_LOOP_BIDIR) {
		if (~xxs->flg & XMP_SAMPLE_LOOP)
			xxs->flg &= ~XMP_SAMPLE_LOOP_BIDIR;
	}
	/* Unroll bidirectional loops
	 */
	if (xxs->flg & XMP_SAMPLE_LOOP_BIDIR) {
		unroll_extralen = (xxs->lpe - xxs->lps) -
				(xxs->len - xxs->lpe);

		if (unroll_extralen < 0) {
			unroll_extralen = 0;
		}
	}

	if (xxs->flg & XMP_SAMPLE_16BIT) {
		bytelen *= 2;
		extralen *= 2;
		unroll_extralen *= 2;
	}

	if ((xxs->data = malloc(bytelen + extralen + unroll_extralen)) == NULL)
		return -1;

	if (flags & SAMPLE_FLAG_NOLOAD) {
		memcpy(xxs->data, buffer, bytelen);
	} else {
		uint8 buf[5];
		int pos = ftell(f);
		int num = fread(buf, 1, 5, f);

		fseek(f, pos, SEEK_SET);

		if (num == 5 && !memcmp(buf, "ADPCM", 5)) {
			int x2 = bytelen >> 1;
			char table[16];

			fseek(f, 5, SEEK_CUR);	/* Skip "ADPCM" */
			fread(table, 1, 16, f);
			fread(xxs->data + x2, 1, x2, f);
			adpcm4_decoder((uint8 *) xxs->data + x2,
				       (uint8 *) xxs->data, table, bytelen);
		} else {
			int x = fread(xxs->data, 1, bytelen, f);
			if (x != bytelen) {
				fprintf(stderr, "libxmp: short read (%d) in "
					"sample load\n", x - bytelen);
				memset(xxs->data + x, 0, bytelen - x);
			}
		}
	}

	if (flags & SAMPLE_FLAG_7BIT) {
		convert_7bit_to_8bit(xxs->len, xxs->data);
	}

	if (flags & SAMPLE_FLAG_DIFF) {
		convert_delta(xxs->len, xxs->flg & XMP_SAMPLE_16BIT, xxs->data);
	} else if (flags & SAMPLE_FLAG_8BDIFF) {
		int len = xxs->len;
		if (xxs->flg & XMP_SAMPLE_16BIT) {
			len *= 2;
		}
		convert_delta(len, 0, xxs->data);
	}

	/* Convert samples to signed */
	if (flags & SAMPLE_FLAG_UNS) {
		convert_signal(xxs->len, xxs->flg & XMP_SAMPLE_16BIT,
				xxs->data);
	}

	/* Fix endianism if needed */
	if (xxs->flg & XMP_SAMPLE_16BIT) {
		if (is_big_endian() ^ ((flags & SAMPLE_FLAG_BIGEND) != 0))
			convert_endian(xxs->len, xxs->data);
	}

	/* Downmix stereo samples */
	if (flags & SAMPLE_FLAG_STEREO) {
		convert_stereo_to_mono(xxs->len, xxs->flg & XMP_SAMPLE_16BIT,
				  xxs->data);
		xxs->len /= 2;
	}

	if (flags & SAMPLE_FLAG_VIDC) {
		convert_vidc_to_linear(xxs->len, xxs->data);
	}

	/* Check for full loop samples */
	if (flags & SAMPLE_FLAG_FULLREP) {
	    if (xxs->lps == 0 && xxs->len > xxs->lpe)
		xxs->flg |= XMP_SAMPLE_LOOP_FULL;
	}

	/* Unroll bidirectional loops */
	if (xxs->flg & XMP_SAMPLE_LOOP_BIDIR) {
		unroll_loop(xxs);
		bytelen += unroll_extralen;
	}
	
	/* Add extra samples at end */
	if (xxs->flg & XMP_SAMPLE_16BIT) {
		xxs->data[bytelen] = xxs->data[bytelen - 2];
		xxs->data[bytelen + 1] = xxs->data[bytelen - 1];
		xxs->data[bytelen + 2] = xxs->data[bytelen];
		xxs->data[bytelen + 3] = xxs->data[bytelen + 1];
	} else {
		xxs->data[bytelen] = xxs->data[bytelen - 1];
		xxs->data[bytelen + 1] = xxs->data[bytelen];
	}

	/* Fix sample at loop */
	if (xxs->flg & XMP_SAMPLE_LOOP) {
		if (xxs->flg & XMP_SAMPLE_16BIT) {
			int lpe = xxs->lpe * 2;
			int lps = xxs->lps * 2;
			xxs->data[lpe] = xxs->data[lpe - 2];
			xxs->data[lpe + 1] = xxs->data[lpe - 1];
			xxs->data[lpe + 2] = xxs->data[lps];
			xxs->data[lpe + 3] = xxs->data[lps + 1];
		} else {
			xxs->data[xxs->lpe] = xxs->data[xxs->lpe - 1];
			xxs->data[xxs->lpe + 1] = xxs->data[xxs->lps];
		}
	}

	return 0;
}
