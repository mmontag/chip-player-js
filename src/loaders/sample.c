#include "common.h"
#include "synth.h"
#include "spectrum.h"
#include "load.h"

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


int load_sample(FILE *f, int flags, struct xmp_sample *xxs, void *buffer)
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
		convert_7bit_to_8bit(xxs->data, xxs->len);
	}

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

	/* Fix endianism if needed */
	if (xxs->flg & XMP_SAMPLE_16BIT) {
		if (is_big_endian() ^ ((flags & SAMPLE_FLAG_BIGEND) != 0))
			convert_endian(xxs->data, xxs->len);
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
			int lpe = xxs->lpe * 2 + unroll_extralen;
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
