/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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
	uint8 buf[XMP_NAMESIZE];

	if (t == NULL)
		return;

	if (s >= XMP_NAMESIZE)
		s = XMP_NAMESIZE -1;

	memset(t, 0, s + 1);

	fread(buf, 1, s, f);
	buf[s] = 0;
	copy_adjust(t, buf, s);
}

void set_xxh_defaults(struct xmp_module_header *xxh)
{
	memset(xxh, 0, sizeof(struct xmp_module_header));
	xxh->gvl = 0x40;
	xxh->tpo = 6;
	xxh->bpm = 125;
	xxh->chn = 4;
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
void clean_s3m_seq(struct xmp_module_header *xxh, uint8 *xxo)
{
    int i, j;
    
    /*for (i = 0; i < xxh->len; i++) printf("%02x ", xxo[i]);
    printf("\n");*/
    for (i = j = 0; i < xxh->len; i++, j++) {
	while (xxo[i] == 0xfe) {
	    xxh->len--;
	    ord_xlat[j] = i;
            //printf("xlat %d -> %d\n", j, i);
	    j++;
	    //printf("move %d from %d to %d\n", xxh->len - i, i + 1, i);
	    memmove(xxo + i, xxo + i + 1, xxh->len - i);
        }

	ord_xlat[j] = i;
        //printf("xlat %d -> %d\n", j, i);

	if (xxo[i] == 0xff) {
	    xxh->len = i;
	    break;
	}
    }
    /*for (i = 0; i < xxh->len; i++) printf("%02x ", xxo[i]);
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

void get_instrument_path(struct xmp_context *ctx, char *var, char *path, int size)
{
	struct xmp_options *o = &ctx->o;

	if (o->ins_path) {
		strncpy(path, o->ins_path, size);
	} else if (var && getenv(var)) {
		strncpy(path, getenv(var), size);
	} else if (getenv("XMP_INSTRUMENT_PATH")) {
		strncpy(path, getenv("XMP_INSTRUMENT_PATH"), size);
	} else {
		strncpy(path, ".", size);
	}
}

void set_type(struct xmp_mod_context *m, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	vsnprintf(m->mod.type, XMP_NAMESIZE, fmt, ap);
	va_end(ap);
}


static void adpcm4_decoder(uint8 *inp, uint8 *outp, char *tab, int len)
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


int load_patch(struct xmp_context *ctx, FILE * f, int id, int flags,
	       struct xmp_sample *xxs, void *buffer)
{
	struct xmp_options *o = &ctx->o;
	uint8 s[5];
	int bytelen, extralen;

	/* Synth patches
	 * Default is YM3128 for historical reasons
	 */
	if (flags & XMP_SMP_SYNTH) {
		int size = 11;	/* Adlib instrument size */

		if (flags & XMP_SMP_SPECTRUM)
			size = sizeof(struct spectrum_sample);

		if ((xxs->data = malloc(size)) == NULL)
			return XMP_ERR_ALLOC;

		memcpy(xxs->data, buffer, size);

		xxs->flg |= XMP_SAMPLE_SYNTH;

		return 0;
	}

	if (o->skipsmp) {	/* Will fail for ADPCM samples */
		if (~flags & XMP_SMP_NOLOAD)
			fseek(f, xxs->len, SEEK_CUR);
		return 0;
	}

	/* Empty samples
	 */
	if (xxs->len == 0) {
		return 0;
	}

	/* Patches with samples
	 */
	bytelen = xxs->len;
	extralen = 1;
	if (xxs->flg & XMP_SAMPLE_16BIT) {
		bytelen *= 2;
		extralen = 2;
	}

	if ((xxs->data = malloc(bytelen + extralen)) == NULL)
		return XMP_ERR_ALLOC;

	if (flags & XMP_SMP_NOLOAD) {
		memcpy(xxs->data, buffer, bytelen);
	} else {
		int pos = ftell(f);
		int num = fread(s, 1, 5, f);

		fseek(f, pos, SEEK_SET);

		if (num == 5 && !memcmp(s, "ADPCM", 5)) {
			int x2 = bytelen >> 1;
			char table[16];

			fseek(f, 5, SEEK_CUR);	/* Skip "ADPCM" */
			fread(table, 1, 16, f);
			fread(xxs->data + x2, 1, x2, f);
			adpcm4_decoder((uint8 *) xxs->data + x2,
				       (uint8 *) xxs->data, table, bytelen);
		} else {
			int x = fread(xxs->data, 1, bytelen, f);
			if (x != bytelen)
				fprintf(stderr, "short read: %d\n",
					bytelen - x);
		}
	}

	/* Convert samples to signed */
	if (flags & XMP_SMP_UNS) {
		xmp_cvt_sig2uns(xxs->len, xxs->flg & XMP_SAMPLE_16BIT,
				xxs->data);
	}

	/* Fix endianism if needed */
	if (xxs->flg & XMP_SAMPLE_16BIT) {
		if (!!o->big_endian ^ !!(flags & XMP_SMP_BIGEND))
			xmp_cvt_sex(xxs->len, xxs->data);
	}

	/* Downmix stereo samples */
	if (flags & XMP_SMP_STEREO) {
		xmp_cvt_stdownmix(xxs->len, xxs->flg & XMP_SAMPLE_16BIT,
				  xxs->data);
		xxs->len /= 2;
	}

	if (flags & XMP_SMP_7BIT)
		xmp_cvt_2xsmp(xxs->len, xxs->data);

	if (flags & XMP_SMP_DIFF)
		xmp_cvt_diff2abs(xxs->len, xxs->flg & XMP_SAMPLE_16BIT,
				 xxs->data);
	else if (flags & XMP_SMP_8BDIFF)
		xmp_cvt_diff2abs(xxs->len, 0, xxs->data);

	if (flags & XMP_SMP_VIDC)
		xmp_cvt_vidc(xxs->len, xxs->data);

	/* Duplicate last sample -- prevent click in bidir loops */
	if (xxs->flg & XMP_SAMPLE_16BIT) {
		xxs->data[bytelen] = xxs->data[bytelen - 2];
		xxs->data[bytelen + 1] = xxs->data[bytelen - 1];
	} else {
		xxs->data[bytelen] = xxs->data[bytelen - 1];
	}
	/* xxs->len++; */

	return 0;
}
