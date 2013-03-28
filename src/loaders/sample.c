/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "common.h"
#include "synth.h"
#include "spectrum.h"
#include "loader.h"

/*
 * From the Audio File Formats (version 2.5)
 * Submitted-by: Guido van Rossum <guido@cwi.nl>
 * Last-modified: 27-Aug-1992
 *
 * The Acorn Archimedes uses a variation on U-LAW with the bit order
 * reversed and the sign bit in bit 0.  Being a 'minority' architecture,
 * Arc owners are quite adept at converting sound/image formats from
 * other machines, and it is unlikely that you'll ever encounter sound in
 * one of the Arc's own formats (there are several).
 */
static const int8 vdic_table[128] = {
	/*   0 */	  0,   0,   0,   0,   0,   0,   0,   0,
	/*   8 */	  0,   0,   0,   0,   0,   0,   0,   0,
	/*  16 */	  0,   0,   0,   0,   0,   0,   0,   0,
	/*  24 */	  1,   1,   1,   1,   1,   1,   1,   1,
	/*  32 */	  1,   1,   1,   1,   2,   2,   2,   2,
	/*  40 */	  2,   2,   2,   2,   3,   3,   3,   3,
	/*  48 */	  3,   3,   4,   4,   4,   4,   5,   5,
	/*  56 */	  5,   5,   6,   6,   6,   6,   7,   7,
	/*  64 */	  7,   8,   8,   9,   9,  10,  10,  11,
	/*  72 */	 11,  12,  12,  13,  13,  14,  14,  15,
	/*  80 */	 15,  16,  17,  18,  19,  20,  21,  22,
	/*  88 */	 23,  24,  25,  26,  27,  28,  29,  30,
	/*  96 */	 31,  33,  34,  36,  38,  40,  42,  44,
	/* 104 */	 46,  48,  50,  52,  54,  56,  58,  60,
	/* 112 */	 62,  65,  68,  72,  77,  80,  84,  91,
	/* 120 */	 95,  98, 103, 109, 114, 120, 126, 127
};


/* Convert differential to absolute sample data */
static void convert_delta(uint8 *p, int l, int r)
{
	uint16 *w = (uint16 *)p;
	uint16 abs = 0;

	if (r) {
		for (; l--;) {
			abs = *w + abs;
			*w++ = abs;
		}
	} else {
		for (; l--;) {
			abs = *p + abs;
			*p++ = (uint8) abs;
		}
	}
}

/* Convert signed to unsigned sample data */
static void convert_signal(uint8 *p, int l, int r)
{
	uint16 *w = (uint16 *)p;

	if (r) {
		for (; l--; w++)
			*w += 0x8000;
	} else {
		for (; l--; p++)
			*p += (char)0x80;	/* cast needed by MSVC++ */
	}
}

/* Convert little-endian 16 bit samples to big-endian */
static void convert_endian(uint8 *p, int l)
{
	uint8 b;
	int i;

	for (i = 0; i < l; i++) {
		b = p[0];
		p[0] = p[1];
		p[1] = b;
		p += 2;
	}
}

/* Downmix stereo samples to mono */
static void convert_stereo_to_mono(uint8 *p, int l, int r)
{
	int16 *b = (int16 *)p;
	int i;

	if (r) {
		l /= 2;
		for (i = 0; i < l; i++)
			b[i] = (b[i * 2] + b[i * 2 + 1]) / 2;
	} else {
		for (i = 0; i < l; i++)
			p[i] = (p[i * 2] + p[i * 2 + 1]) / 2;
	}
}

/* Convert 7 bit samples to 8 bit */
static void convert_7bit_to_8bit(uint8 *p, int l)
{
	for (; l--; p++) {
		*p <<= 1;
	}
}

/* Convert Archimedes VIDC samples to linear */
static void convert_vidc_to_linear(uint8 *p, int l)
{
	int i;
	uint8 x;

	for (i = 0; i < l; i++) {
		x = p[i];
		p[i] = vdic_table[x >> 1];
		if (x & 0x01)
			p[i] *= -1;
	}
}

/* Convert HSC OPL2 instrument data to SBI instrument data */
static void convert_hsc_to_sbi(uint8 *a)
{
	uint8 b[11];
	int i;

	for (i = 0; i < 10; i += 2) {
		uint8 x;
		x = a[i];
		a[i] = a[i + 1];
		a[i + 1] = x;
	}

	memcpy(b, a, 11);
	a[8] = b[10];
	a[10] = b[9];
	a[9] = b[8];
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


int load_sample(FILE *f, int flags, struct xmp_sample *xxs, void *buffer)
{
	int bytelen, extralen, unroll_extralen, i;

	/* Synth patches
	 * Default is YM3128 for historical reasons
	 */
	if (flags & SAMPLE_FLAG_SYNTH) {
		int size = 11;	/* Adlib instrument size */

		if (flags & SAMPLE_FLAG_SPECTRUM) {
			size = sizeof(struct spectrum_sample);
		} else if (flags & SAMPLE_FLAG_HSC) {
			convert_hsc_to_sbi(buffer);
		}

		if ((xxs->data = malloc(size + 4)) == NULL)
			return -1;
		*(uint32 *)xxs->data = 0;
		xxs->data += 4;

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
	extralen = 4;
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

	/* add guard bytes before the buffer for higher order interpolation */
	if ((xxs->data = malloc(bytelen + extralen + unroll_extralen + 4)) == NULL)
		return -1;
	*(uint32 *)xxs->data = 0;
	xxs->data += 4;

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
			adpcm4_decoder((uint8 *)xxs->data + x2,
				       (uint8 *)xxs->data, table, bytelen);
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
		convert_7bit_to_8bit(xxs->data, xxs->len);
	}

	/* Fix endianism if needed */
	if (xxs->flg & XMP_SAMPLE_16BIT) {
		if (is_big_endian() ^ ((flags & SAMPLE_FLAG_BIGEND) != 0))
			convert_endian(xxs->data, xxs->len);
	}

	/* Convert delta samples */
	if (flags & SAMPLE_FLAG_DIFF) {
		convert_delta(xxs->data, xxs->len, xxs->flg & XMP_SAMPLE_16BIT);
	} else if (flags & SAMPLE_FLAG_8BDIFF) {
		int len = xxs->len;
		if (xxs->flg & XMP_SAMPLE_16BIT) {
			len *= 2;
		}
		convert_delta(xxs->data, len, 0);
	}

	/* Convert samples to signed */
	if (flags & SAMPLE_FLAG_UNS) {
		convert_signal(xxs->data, xxs->len,
				xxs->flg & XMP_SAMPLE_16BIT);
	}

	/* Downmix stereo samples */
	if (flags & SAMPLE_FLAG_STEREO) {
		convert_stereo_to_mono(xxs->data, xxs->len,
					xxs->flg & XMP_SAMPLE_16BIT);
		xxs->len /= 2;
	}

	if (flags & SAMPLE_FLAG_VIDC) {
		convert_vidc_to_linear(xxs->data, xxs->len);
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
		for (i = 0; i < 8; i++) {
			xxs->data[bytelen + i] = xxs->data[bytelen - 2 + i];
		}
	} else {
		for (i = 0; i < 4; i++) {
			xxs->data[bytelen + i] = xxs->data[bytelen - 1 + i];
		}
	}

	/* Add extra samples at start */
	if (xxs->flg & XMP_SAMPLE_16BIT) {
		xxs->data[-2] = xxs->data[0];
		xxs->data[-1] = xxs->data[1];
	} else {
		xxs->data[-1] = xxs->data[0];
	}

	/* Fix sample at loop */
	if (xxs->flg & XMP_SAMPLE_LOOP) {
		if (xxs->flg & XMP_SAMPLE_16BIT) {
			int lpe = xxs->lpe * 2;
			int lps = xxs->lps * 2;

			if (xxs->flg & XMP_SAMPLE_LOOP_BIDIR) {
				lpe += (xxs->lpe - xxs->lps) * 2;
			}
			xxs->data[lpe] = xxs->data[lpe - 2];
			xxs->data[lpe + 1] = xxs->data[lpe - 1];
			for (i = 0; i < 6; i++) {
				xxs->data[lpe + 2 + i] = xxs->data[lps + i];
			}
		} else {
			int lpe = xxs->lpe + unroll_extralen;
			int lps = xxs->lps;
			xxs->data[lpe] = xxs->data[lpe - 1];
			for (i = 0; i < 3; i++) {
				xxs->data[lpe + 1 + i] = xxs->data[lps + i];
			}
		}
	}

	return 0;
}
