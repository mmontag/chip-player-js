/* ProWizard
 * Copyright (C) 1998 Asle / ReDoX
 * Modified by Claudio Matsuoka
 * Modified in 2021 by Alice Rowan
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

/*
 * The_Player_6.1a.c
 *
 * The Player 6.1a to Protracker.
 *
 * note: As for version 5.0A and 6.0A, it's a REAL mess !.
 *      It's VERY badly coded, I know. Just dont forget it was mainly done
 *      to test the description I made of P61a format.
 *      I certainly wont dare to beat Gryzor on the ground :). His Prowiz IS
 *      the converter to use !!!. Though, using the official depacker could
 *      be a good idea too :).
 */

#include "prowiz.h"


static int depack_p61a(HIO_HANDLE *in, FILE *out)
{
    uint8 c1, c2, c3, c4, c5, c6;
    long max_row;
    uint8 tmp[1024];
    signed char *smp_buffer;
    int len;
    int npat;
    int nins;
    uint8 tdata[512][256];
    uint8 ptable[128];
    int isize[31];
    /* uint8 PACK[31]; */
    uint8 use_delta = 0;
    /* uint8 use_packed = 0; */
    int taddr[128][4];
    int tdata_addr = 0;
    int sdata_addr = 0;
    /* int ssize = 0; */
    int i = 0, j, k, l, a, b, z;
    int smp_size[31];
    int saddr[31];
    /* int Unpacked_Sample_Data_Size; */
    int val;

    memset(taddr, 0, sizeof(taddr));
    memset(tdata, 0, sizeof(tdata));
    memset(ptable, 0, sizeof(ptable));
    memset(smp_size, 0, sizeof(smp_size));
    memset(isize, 0, sizeof(isize));
    memset(saddr, 0, sizeof(saddr));
    for (i = 0; i < 31; i++) {
	/* PACK[i] = 0; */
	/* DELTA[i] = 0;*/
    }

    sdata_addr = hio_read16b(in);	/* read sample data address */
    npat = hio_read8(in);		/* read real number of pattern */

    /* Sanity check */
    if (npat >= 128) {
        return -1;
    }

    nins = hio_read8(in);		/* read number of samples */

    if (nins & 0x80) {
	/* Samples are saved as delta values */
	use_delta = 1;
    }
    if (nins & 0x40) {
	/* Some samples are packed -- depacking not implemented */
	/* use_packed = 1; */
	return -1;
    }
    nins &= 0x3f;

    /* read unpacked sample data size */
    /*
    if (use_packed == 1)
	Unpacked_Sample_Data_Size = hio_read32b(in);
    */

    pw_write_zero(out, 20);		/* write title */

    /* sample headers stuff */
    for (i = 0; i < nins; i++) {
	pw_write_zero(out, 22);		/* write sample name */

	j = isize[i] = hio_read16b(in);	/* sample size */

	if (j > 0xff00) {
	    smp_size[i] = smp_size[0xffff - j];
	    isize[i] = isize[0xffff - j];
	    saddr[i] = saddr[0xffff - j];
	} else {
            if (i > 0) {
	        saddr[i] = saddr[i - 1] + smp_size[i - 1];
            }
	    smp_size[i] = j * 2;
	    /* ssize += smp_size[i]; */
	}
	j = smp_size[i] / 2;
	write16b(out, isize[i]);

	c1 = hio_read8(in);			/* finetune */
	/*
	if (c1 & 0x40)
	    PACK[i] = 1;
	*/
	c1 &= 0x3f;
	write8(out, c1);

	write8(out, hio_read8(in));		/* volume */

	/* loop start */
	val = hio_read16b(in);
	if (val == 0xffff) {
	    write16b(out, 0x0000);
	    write16b(out, 0x0001);
	    continue;
	}
	write16b(out, val);
	write16b(out, j - val);
    }

    /* go up to 31 samples */
    memset(tmp, 0, 30);
    tmp[29] = 0x01;
    for (; i < 31; i++)
	fwrite(tmp, 30, 1, out);

    /* read tracks addresses per pattern */
    for (i = 0; i < npat; i++) {
	for (j = 0; j < 4; j++)
	    taddr[i][j] = hio_read16b(in);
    }

    /* pattern table */
    for (len = 0; len < 128; len++) {
	c1 = hio_read8(in);
	if (c1 == 0xff)
	    break;
	ptable[len] = c1;		/* <--- /2 in p50a */
    }

    write8(out, len);			/* write size of pattern list */
    write8(out, 0x7f);			/* write noisetracker byte */
    fwrite(ptable, 128, 1, out);	/* write pattern table */
    write32b(out, PW_MOD_MAGIC);	/* write ptk ID */

    if ((tdata_addr = hio_tell(in)) < 0) {
        return -1;
    }

    /* rewrite the track data */
    for (i = 0; i < npat; i++) {
	max_row = 63;
	for (j = 0; j < 4; j++) {
	    hio_seek(in, taddr[i][j] + tdata_addr, SEEK_SET);
	    for (k = 0; k <= max_row; k++) {
		uint8 *x = &tdata[i * 4 + j][k * 4];
		c1 = hio_read8(in);

	        /* case no fxt nor fxtArg  (3 bytes) */
	        if ((c1 & 0x70) == 0x70 && c1 != 0xff && c1 != 0x7F) {
		    c2 = hio_read8(in);
	            c6 = ((c1 << 4) & 0xf0) | ((c2 >> 4) & 0x0e);

		    /* Sanity check */
		    if (hio_error(in) || !PTK_IS_VALID_NOTE(c6 / 2)) {
			return -1;
		    }

	            *x++ = (c2 & 0x10) | (ptk_table[c6 / 2][0]);
	            *x++ = ptk_table[c6 / 2][1];
	            *x++ = (c2 << 4) & 0xf0;

	            if (c1 & 0x80) {
			c3 = hio_read8(in);
	                if (c3 < 0x80) {
	                    k += c3;
	                    continue;
	                }
	                c4 = c3 - 0x80;

	                for (l = 0; l < c4 && k < max_row; l++) {
	                    k++;
			    x = &tdata[i * 4 + j][k * 4];
	                    *x++ = (c2 & 0x10) | ptk_table[c6 / 2][0];
	                    *x++ = ptk_table[c6 / 2][1];
	                    *x++ = (c2 << 4) & 0xf0;
	                }
	            }
	            continue;
	        }
	        /* end of case no fxt nor fxtArg */

	        /* case no sample number nor relative note number */
	        if ((c1 & 0x70) == 0x60 && c1 != 0xff) {
		    c2 = hio_read8(in);
	            c6 = c1 & 0x0f;
	            if (c6 == 0x08)
	                c1 -= 0x08;
		    x += 2;
	            *x++ = c1 & 0x0f;
	            if (c6 == 0x05 || c6 == 0x06 || c6 == 0x0a)
	                c2 = c2 > 0x7f ? (0x100 - c2) << 4 : c2;
	            *x++ = c2;

	            if (c6 == 0x0d) {	/* PATTERN BREAK, track ends */
	                max_row = k;
			break;
	            }
	            if (c6 == 0x0B) {	/* PATTERN JUMP, track ends */
	                max_row = k;
			break;
	            }
	            if (c1 & 0x80) {
			c3 = hio_read8(in);
	                if (c3 < 0x80) {	/* bypass c3 rows */
	                    k += c3;
	                    continue;
	                }
	                c4 = c3 - 0x80;		/* repeat current row */
	                for (l = 0; l < c4 && k < max_row; l++) {
	                    k++;
			    x = &tdata[i * 4 + j][k * 4] + 2;
	                    *x++ = c1 & 0x0f;
	                    *x++ = c2;
	                }
	            }
	            continue;
	        }
	        /* end of case no Sample number nor Relative not number */

	        if ((c1 & 0x80) == 0x80 && c1 != 0xff) {
		    c2 = hio_read8(in);
		    c3 = hio_read8(in);
		    c4 = hio_read8(in);
	            c1 &= 0x7f;

		    /* Sanity check */
		    if (hio_error(in) || !PTK_IS_VALID_NOTE(c1 / 2)) {
			return -1;
		    }

		    *x++ = ((c1 << 4) & 0x10) | ptk_table[c1 / 2][0];
	            *x++ = ptk_table[c1 / 2][1];
	            c6 = c2 & 0x0f;
	            if (c6 == 0x08)
	                c2 -= 0x08;
	            *x++ = c2;

	            if (c6 == 0x05 || c6 == 0x06 || c6 == 0x0a)
	                c3 = c3 > 0x7f ? (0x100 - c3) << 4 : c3;

	            *x++ = c3;

	            if (c6 == 0x0d) {	/* PATTERN BREAK, track ends */
	                max_row = k;
			break;
	            }
	            if (c6 == 0x0B) {	/* PATTERN JUMP, track ends */
	                max_row = k;
			break;
	            }

	            if (c4 < 0x80) {	/* bypass c4 rows */
	                k += c4;
	                continue;
	            }
	            c4 = c4 - 0x80;

	            for (l = 0; l < c4 && k < max_row; l++) {	/* repeat row c4-0x80 times */
	                k++;
			x = &tdata[i * 4 + j][k * 4];

	                *x++ = ((c1 << 4) & 0x10) | ptk_table[c1 / 2][0];
	                *x++ = ptk_table[c1 / 2][1];
	                c6 = c2 & 0x0f;
	                if (c6 == 0x08)
	                    c2 -= 0x08;
	                *x++ = c2;

	                if (c6 == 0x05 || c6 == 0x06 || c6 == 0x0a)
	                    c3 = c3 > 0x7f ? (0x100 - c3) << 4 : c3;
	                *x++ = c3;
	            }
	            continue;
	        }

	        if ((c1 & 0x7f) == 0x7f) {
	            if (~c1 & 0x80) {		/* bypass 1 row */
	                /*k += 1; */
	                continue;
	            }
		    c2 = hio_read8(in);
	            if (c2 < 0x40) {	/* bypass c2 rows */
	                k += c2;
	                continue;
	            }
	            c2 -= 0x40;
		    c3 = hio_read8(in);
	            z = c3;
	            if (c2 >= 0x80) {
	                c2 -= 0x80;
			c4 = hio_read8(in);
	                z = (c3 << 8) + c4;
	            }
	            if ((a = hio_tell(in)) < 0) {
                        return -1;
                    }
	            c5 = c2;

	            hio_seek(in, -z, SEEK_CUR);

	            for (l = 0; l <= c5 && k <= max_row; l++, k++) {
			c1 = hio_read8(in);
			x = &tdata[i * 4 + j][k * 4];

	                /* case no fxt nor fxtArg  (3 bytes) */
	                if ((c1 & 0x70) == 0x70 && c1 != 0xff && c1 != 0x7f) {
			    c2 = hio_read8(in);
	                    c6 = ((c1 << 4) & 0xf0) | ((c2 >> 4) & 0x0e);

			    /* Sanity check */
			    if (hio_error(in) || !PTK_IS_VALID_NOTE(c6 / 2)) {
				return -1;
			    }

	                    *x++ = (c2 & 0x10) | ptk_table[c6 / 2][0];
	                    *x++ = ptk_table[c6 / 2][1];
	                    *x++ = (c2 << 4) & 0xf0;

	                    if (c1 & 0x80) {
				c3 = hio_read8(in);
	                        if (c3 < 0x80) {	/* bypass c3 rows */
	                            k += c3;
	                            continue;
	                        }
	                        c4 = c3 - 0x80;	/* repeat row c3-0x80 times */
	                        for (b = 0; b < c4 && k < max_row; b++) {
	                            k++;
			            x = &tdata[i * 4 + j][k * 4];
	                            *x++ = (c2 & 0x10) | ptk_table[c6 / 2][0];
	                            *x++ = ptk_table[c6 / 2][1];
	                            *x++ = (c2 << 4) & 0xf0;
	                        }
	                    }
	                    continue;
	                }
	                /* end of case no fxt nor fxtArg */

	                /* case no sample number nor relative note number */
	                if ((c1 & 0x60) == 0x60 && c1 != 0xff && c1 != 0x7f) {
			    c2 = hio_read8(in);
	                    c6 = c1 & 0x0f;
	                    if (c6 == 0x08)
	                        c1 -= 0x08;

			    x += 2;
	                    *x++ = c1 & 0x0f;

	                    if (c6 == 0x05 || c6 == 0x06 || c6 == 0x0a)
	                            c2 = c2 > 0x7f ? (0x100 - c2) << 4 : c2;
	                    *x++ = c2;

	                    if (c6 == 0x0d) {	/* PATTERN BREAK, track ends */
	                        max_row = k;
				goto brk_k;
	                    }
	                    if (c6 == 0x0b) {	/* PATTERN JUMP, track ends */
	                        max_row = k;
				goto brk_k;
	                    }

	                    if (c1 & 0x80) {
				c3 = hio_read8(in);
	                        if (c3 < 0x80) {	/* bypass c3 rows */
	                            k += c3;
	                            continue;
	                        }
	                        c4 = c3 - 0x80;	/* repeat row c3-0x80 times */
	                        for (b = 0; b < c4 && k < max_row; b++) {
	                            k++;
			            x = &tdata[i * 4 + j][k * 4] + 2;
	                            *x++ = c1 & 0x0f;
	                            *x++ = c2;
	                        }
	                    }
	                    continue;
	                }
	                /* end of case no sample nor relative note number */

	                if ((c1 & 0x80) && c1 != 0xff && c1 != 0x7f) {
			    c2 = hio_read8(in);
			    c3 = hio_read8(in);
			    c4 = hio_read8(in);
	                    c1 &= 0x7f;

			    /* Sanity check */
			    if (hio_error(in) || !PTK_IS_VALID_NOTE(c1 / 2)) {
				return -1;
			    }

	                    *x++ = ((c1 << 4) & 0x10) | ptk_table[c1 / 2][0];
	                    *x++ = ptk_table[c1 / 2][1];

	                    c6 = c2 & 0x0f;
	                    if (c6 == 0x08)
	                        c2 -= 0x08;
	                    *x++ = c2;

	                    if (c6 == 0x05 || c6 == 0x06 || c6 == 0x0a)
	                            c3 = c3 > 0x7f ? (0x100 - c3) << 4 : c3;
	                    *x++ = c3;

	                    if (c6 == 0x0d) {	/* PATTERN BREAK, track ends */
	                        max_row = k;
				goto brk_k;
	                    }
	                    if (c6 == 0x0b) {	/* PATTERN JUMP, track ends */
	                        max_row = k;
				goto brk_k;
	                    }
	                    if (c4 < 0x80) {	/* bypass c4 rows */
	                        k += c4;
	                        continue;
	                    }
	                    c4 = c4 - 0x80;	/* repeat row c4-0x80 times */
	                    for (b = 0; b < c4 && k < max_row; b++) {
	                        k++;
			        x = &tdata[i * 4 + j][k * 4];

	                        *x++ = ((c1 << 4) & 0x10) |ptk_table[c1 / 2][0];
	                        *x++ = ptk_table[c1 / 2][1];

	                        c6 = c2 & 0x0f;
	                        if (c6 == 0x08)
	                             c2 -= 0x08;
	                        *x++ = c2;

	                        if (c6 == 0x05 || c6 == 0x06 || c6 == 0x0a)
	                             c3 = c3 > 0x7f ? (0x100 - c3) << 4 : c3;
	                        *x++ = c3;
	                    }
	                    continue;
	                }
	                if ((c1 & 0x7f) == 0x7f) {
	                    if ((c1 & 0x80) == 0x00) {	/* bypass 1 row */
	                        /*k += 1; */
	                        continue;
	                    }
			    c2 = hio_read8(in);
	                    if (c2 < 0x40) {	/* bypass c2 rows */
	                        k += c2;
	                        continue;
	                    }
	                    continue;
	                }

			c2 = hio_read8(in);
			c3 = hio_read8(in);

			if (hio_error(in) || !PTK_IS_VALID_NOTE(c1 / 2)) {
			    return -1;
			}

	                *x++ = ((c1 << 4) & 0x10) | ptk_table[c1 / 2][0];
	                *x++ = ptk_table[c1 / 2][1];

	                c6 = c2 & 0x0f;
	                if (c6 == 0x08)
	                    c2 -= 0x08;
	                *x++ = c2;

	                if (c6 == 0x05 || c6 == 0x06 || c6 == 0x0a)
	                    c3 = c3 > 0x7f ? (0x100 - c3) << 4 : c3;
	                *x++ = c3;
	            }
	            hio_seek(in, a, SEEK_SET);
	            k -= 1;
	            continue;
	        }

		c2 = hio_read8(in);
		c3 = hio_read8(in);

		/* Sanity check */
		if (hio_error(in) || !PTK_IS_VALID_NOTE(c1 / 2)) {
		    return -1;
		}

	        *x++ = ((c1 << 4) & 0x10) | ptk_table[c1 / 2][0];
	        *x++ = ptk_table[c1 / 2][1];

	        c6 = c2 & 0x0f;
	        if (c6 == 0x08)
	            c2 -= 0x08;
	        *x++ = c2;

	        if (c6 == 0x05 || c6 == 0x06 || c6 == 0x0a)
		    c3 = c3 > 0x7f ? (0x100 - c3) << 4 : c3;
	        *x++ = c3;

	        if (c6 == 0x0d) {	/* PATTERN BREAK, track ends */
	            max_row = k;
		    break;
	        }
	        if (c6 == 0x0b) {	/* PATTERN JUMP, track ends */
	            max_row = k;
		    break;
	        }
	    }
	    brk_k: ;
	}
    }

    /* write pattern data */
    for (i = 0; i < npat; i++) {
	memset(tmp, 0, sizeof(tmp));
	for (j = 0; j < 64; j++) {
	    for (k = 0; k < 4; k++)
		memcpy(&tmp[j * 16 + k * 4], &tdata[k + i * 4][j * 4], 4);
	}
	fwrite (tmp, 1024, 1, out);
    }

    /* go to sample data address */
    hio_seek(in, sdata_addr, SEEK_SET);

    /* read and write sample data */

    /*printf ( "writing sample data ... " ); */
    for (i = 0; i < nins; i++) {
	hio_seek(in, sdata_addr + saddr[i], 0);
	smp_buffer = (signed char *) calloc(1, smp_size[i]);
	hio_read(smp_buffer, smp_size[i], 1, in);
	if (use_delta == 1) {
	    c1 = 0;
	    for (j = 1; j < smp_size[i]; j++) {
	        c2 = smp_buffer[j];
	        c2 = 0x100 - c2;
	        c3 = c2 + c1;
	        smp_buffer[j] = c3;
	        c1 = c3;
	    }
	}
	fwrite(smp_buffer, smp_size[i], 1, out);
	free(smp_buffer);
    }

    /* if (use_delta == 1)
	pw_p60a.flags |= PW_DELTA; */

    return 0;
}

static int test_p61a(const uint8 *data, char *t, int s)
{
    int i, n;
    int len;
    int lstart;
    int npat;
    int nins;
    int pattern_data_offset;
    int sample_data_offset;
    /* int ssize; */

#if 0
    if (i < 7) {
	Test = BAD;
	return;
    }
    start = i - 7;
#endif

    PW_REQUEST_DATA(s, 4);

    /* number of pattern (real) */
    npat = data[2];
    if (npat > 0x7f || npat == 0)
	return -1;

    /* number of sample */
    nins = (data[3] & 0x3f);
    if (nins > 0x1f || nins == 0)
	return -1;

    PW_REQUEST_DATA(s, 4 + nins * 6);

    for (i = 0; i < nins; i++) {
	/* test volumes */
	if (data[7 + i * 6] > 0x40)
	    return -1;

	/* test finetunes */
	if (data[6 + i * 6] > 0x0f)
	    return -1;
    }

    /* test sample sizes and loop start */
    /* ssize = 0; */
    for (i = 0; i < nins; i++) {
	len = readmem16b(data + i * 6 + 4);
	if ((len <= 0xffdf && len > 0x8000) || len == 0)
	    return -1;

	/*
	if (len < 0xff00)
	    ssize += len * 2;
	*/

	lstart = readmem16b(data + i * 6 + 8);
	if (lstart != 0xffff && lstart >= len)
	    return -1;

	if (len > 0xffdf) {
	    if (0xffff - len > nins)
	        return -1;
	}
    }

    pattern_data_offset = 4 + nins * 6 + npat * 8;

    /* test sample data address */
    sample_data_offset = readmem16b(data);
    if (sample_data_offset < pattern_data_offset)
	return -1;

    PW_REQUEST_DATA(s, pattern_data_offset);

    /* test track table */
    for (i = 0; i < npat * 4; i++) {
	int track_start = readmem16b(data + 4 + nins * 6 + i * 2);
	if (track_start + pattern_data_offset > sample_data_offset)
	    return -1;
    }

    PW_REQUEST_DATA(s, sample_data_offset);

    /* test pattern table */
    for (i = 0; i < 128; i++) {
	if (pattern_data_offset + i >= sample_data_offset)
	    return -1;

	if (data[pattern_data_offset + i] == 0xff)
	    break;

	if (data[pattern_data_offset + i] > npat - 1)
	    return -1;
    }

    if (i == 0 || i == 128)
	return -1;

    /* test notes ... pfiew */

    for (n = pattern_data_offset + i + 1; n < sample_data_offset - 1; n++) {
        uint8 d, e;

	d = data[n];
	e = data[n + 1];

	if ((d & 0xff) == 0xff) {
	    if ((e & 0xc0) == 0x00) {
	        n += 1;
	        continue;
	    }
	    if ((e & 0xc0) == 0x40) {
	        n += 2;
	        continue;
	    }
	    if ((e & 0xc0) == 0xc0) {
	        n += 3;
	        continue;
	    }
	}

	if ((d & 0xff) == 0x7f)
	    continue;

	/* no fxt nor fxtArg */
	if ((d & 0xf0) == 0xf0) {
	    if ((e & 0x1f) > nins)
	        return -1;
	    n += 2;
	    continue;
	}

	if ((d & 0xf0) == 0x70) {
	    if ((e & 0x1f) > nins)
	        return -1;
	    n += 1;
	    continue;
	}

	/* no note nor Sample number */
	if ((d & 0xf0) == 0xe0) {
	    n += 2;
	    continue;
	}

	if ((d & 0xf0) == 0x60) {
	    n += 1;
	    continue;
	}

	if ((d & 0x80) == 0x80) {
	    if ((((d << 4) & 0x10) | ((e >> 4) & 0x0f)) > nins)
	        return -1;
	    n += 3;
	    continue;
	}

	if ((((d << 4) & 0x10) | ((e >> 4) & 0x0F)) > nins)
	    return -1;
	n += 2;
    }

    return 0;
}

#if 0
/******************/
/* packed samples */
/******************/
void testP61A_pack (void)
{
    if (i < 11) {
	Test = BAD;
	return;
    }
    start = i - 11;

    /* number of pattern (real) */
    m = data[start + 2];
    if ((m > 0x7f) || (m == 0)) {
/*printf ( "#1 Start:%ld\n" , start );*/
	Test = BAD;
	return;
    }
    /* m is the real number of pattern */

    /* number of sample */
    k = data[start + 3];
    if ((k & 0x40) != 0x40) {
/*printf ( "#2,0 Start:%ld\n" , start );*/
	Test = BAD;
	return;
    }
    k &= 0x3F;
    if ((k > 0x1F) || (k == 0)) {
/*printf ( "#2,1 Start:%ld (k:%ld)\n" , start,k );*/
	Test = BAD;
	return;
    }
    /* k is the number of sample */

    /* test volumes */
    for (l = 0; l < k; l++) {
	if (data[start + 11 + l * 6] > 0x40) {
/*printf ( "#3 Start:%ld\n" , start );*/
	    Test = BAD;
	    return;
	}
    }

    /* test fines */
    for (l = 0; l < k; l++) {
	if ((data[start + 10 + l * 6] & 0x3F) > 0x0F) {
	    Test = BAD;
/*printf ( "#4 Start:%ld\n" , start );*/
	    return;
	}
    }

    /* test sample sizes and loop start */
    ssize = 0;
    for (n = 0; n < k; n++) {
	o = ((data[start + 8 + n * 6] << 8) +
	    data[start + 9 + n * 6]);
	if (((o < 0xFFDF) && (o > 0x8000)) || (o == 0)) {
/*printf ( "#5 Start:%ld\n" , start );*/
	    Test = BAD;
	    return;
	}
	if (o < 0xFF00)
	    ssize += (o * 2);

	j = ((data[start + 12 + n * 6] << 8) +
	    data[start + 13 + n * 6]);
	if ((j != 0xFFFF) && (j >= o)) {
/*printf ( "#5,1 Start:%ld\n" , start );*/
	    Test = BAD;
	    return;
	}
	if (o > 0xFFDF) {
	    if ((0xFFFF - o) > k) {
/*printf ( "#5,2 Start:%ld\n" , start );*/
	        Test = BAD;
	        return;
	    }
	}
    }

    /* test sample data address */
    j = (data[start] << 8) + data[start + 1];
    if (j < (k * 6 + 8 + m * 8)) {
/*printf ( "#6 Start:%ld\n" , start );*/
	Test = BAD;
	ssize = 0;
	return;
    }
    /* j is the address of the sample data */


    /* test track table */
    for (l = 0; l < (m * 4); l++) {
	o =
	    ((data[start + 8 + k * 6 +
	             l * 2] << 8) +
	    data[start + 8 + k * 6 + l * 2 +
	        1]);
	if ((o + k * 6 + 8 + m * 8) > j) {
/*printf ( "#7 Start:%ld (value:%ld)(where:%x)(l:%ld)(m:%ld)(o:%ld)\n"
, start
, (data[start+k*6+8+l*2]*256)+data[start+8+k*6+l*2+1]
, start+k*6+8+l*2
, l
, m
, o );*/
	    Test = BAD;
	    return;
	}
    }

    /* test pattern table */
    l = 0;
    o = 0;
    /* first, test if we dont oversize the input file */
    if ((k * 6 + 8 + m * 8) > in_size) {
/*printf ( "8,0 Start:%ld\n" , start );*/
	Test = BAD;
	return;
    }
    while ((data[start + k * 6 + 8 + m * 8 + l] !=
	    0xFF) && (l < 128)) {
	if (data[start + k * 6 + 8 + m * 8 +
	        l] > (m - 1)) {
/*printf ( "#8,1 Start:%ld (value:%ld)(where:%x)(l:%ld)(m:%ld)(k:%ld)\n"
, start
, data[start+k*6+8+m*8+l]
, start+k*6+8+m*8+l
, l
, m
, k );*/
	    Test = BAD;
	    ssize = 0;
	    return;
	}
	if (data[start + k * 6 + 8 + m * 8 +
	        l] > o)
	    o =
	        data[start + k * 6 + 8 +
	        m * 8 + l];
	l++;
    }
    if ((l == 0) || (l == 128)) {
/*printf ( "#8.2 Start:%ld\n" , start );*/
	Test = BAD;
	return;
    }
    o /= 2;
    o += 1;
    /* o is the highest number of pattern */


    /* test notes ... pfiew */
    l += 1;
    for (n = (k * 6 + 8 + m * 8 + l); n < j; n++) {
	if ((data[start + n] & 0xff) == 0xff) {
	    if ((data[start + n + 1] & 0xc0) ==
	        0x00) {
	        n += 1;
	        continue;
	    }
	    if ((data[start + n + 1] & 0xc0) ==
	        0x40) {
	        n += 2;
	        continue;
	    }
	    if ((data[start + n + 1] & 0xc0) ==
	        0xc0) {
	        n += 3;
	        continue;
	    }
	}
	if ((data[start + n] & 0xff) == 0x7f) {
	    continue;
	}

	/* no fxt nor fxtArg */
	if ((data[start + n] & 0xf0) == 0xf0) {
	    if ((data[start + n + 1] & 0x1F) >
	        k) {
/*printf ( "#9,1 Start:%ld (value:%ld) (where:%x) (n:%ld) (j:%ld)\n"
, start
, data[start+n]
, start+n
, n
, j
 );*/
	        Test = BAD;
	        return;
	    }
	    n += 2;
	    continue;
	}
	if ((data[start + n] & 0xf0) == 0x70) {
	    if ((data[start + n + 1] & 0x1F) >
	        k) {
/*printf ( "#9,1 Start:%ld (value:%ld) (where:%x) (n:%ld) (j:%ld)\n"
, start
, data[start+n]
, start+n
, n
, j
 );*/
	        Test = BAD;
	        return;
	    }
	    n += 1;
	    continue;
	}
	/* no note nor Sample number */
	if ((data[start + n] & 0xf0) == 0xe0) {
	    n += 2;
	    continue;
	}
	if ((data[start + n] & 0xf0) == 0x60) {
	    n += 1;
	    continue;
	}

	if ((data[start + n] & 0x80) == 0x80) {
	    if ((((data[start +
	                        n] << 4) &
	                0x10) |
	            ((data[start + n +
	                        1] >> 4) &
	                0x0F)) > k) {
/*printf ( "#9,1 Start:%ld (value:%ld) (where:%x) (n:%ld) (j:%ld)\n"
, start
, data[start+n]
, start+n
, n
, j
 );*/
	        Test = BAD;
	        return;
	    }
	    n += 3;
	    continue;
	}

	if ((((data[start +
	                    n] << 4) & 0x10) |
	        ((data[start + n +
	                    1] >> 4) & 0x0F)) >
	    k) {
/*printf ( "#9,1 Start:%ld (value:%ld) (where:%x) (n:%ld) (j:%ld)\n"
, start
, data[start+n]
, start+n
, n
, j
 );*/
	    Test = BAD;
	    return;
	}
	n += 2;
    }


    /* ssize is the whole sample data size */
    /* j is the address of the sample data */
    Test = GOOD;
}
#endif

const struct pw_format pw_p61a = {
    "The Player 6.1a",
    test_p61a,
    depack_p61a
};
