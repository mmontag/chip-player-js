/*
 * Kris_tracker.c   Copyright (C) 1997 Asle / ReDoX
 *                  Copyright (C) 2006-2007 Claudio Matsuoka
 *
 * Kris Tracker to Protracker.
 *
 * $Id: kris.c,v 1.7 2007-11-15 15:30:03 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"

static int test_kris (uint8 *, int);
static int depack_kris (FILE *, FILE *);

struct pw_format pw_kris = {
	"KRIS",
	"ChipTracker",
	0x00,
	test_kris,
	depack_kris
};


static int depack_kris(FILE *in, FILE *out)
{
	uint8 tmp[1025];
	uint8 c3;
	uint8 npat, max;
	uint8 ptable[128];
	uint8 note, ins, fxt, fxp;
	uint8 tdata[512][256];
	short taddr[128][4];
	short maxtaddr = 0;
	int i, j, k;
	int size, ssize = 0;

	memset(tmp, 0, 1025);
	memset(ptable, 0, 128);
	memset(taddr, 0, 128 * 4 * 2);
	memset(tdata, 0, 512 << 8);

	pw_move_data(out, in, 20);			/* title */
	fseek(in, 2, SEEK_CUR);

	/* 31 samples */
	for (i = 0; i < 31; i++) {
		/* sample name */
		fread(tmp, 22, 1, in);
		if (tmp[0] == 0x01)
			tmp[0] = 0x00;
		fwrite(tmp, 22, 1, out);

		write16b(out, size = read16b(in));	/* size */
		ssize += size * 2;
		write8(out, read8(in));			/* fine */
		write8(out, read8(in));			/* volume */
		write16b(out, read16b(in) / 2);		/* loop start */
		write16b(out, read16b(in));		/* loop size */
	}

	read32b(in);			/* bypass ID "KRIS" */
	write8(out, npat = read8(in));	/* number of pattern in pattern list */
	write8(out, read8(in));		/* Noisetracker restart byte */

	/* pattern table (read,count and write) */
	c3 = 0;
	k = 0;
	for (i = 0; i < 128; i++, k++) {
		for (j = 0; j < 4; j++) {
			taddr[k][j] = read16b(in);
			if (taddr[k][j] > maxtaddr)
				maxtaddr = taddr[k][j];
		}
		for (j = 0; j < k; j++) {
			if ((taddr[j][0] == taddr[k][0])
				&& (taddr[j][1] == taddr[k][1])
				&& (taddr[j][2] == taddr[k][2])
				&& (taddr[j][3] == taddr[k][3])) {
				ptable[i] = ptable[j];
				k -= 1;
				break;
			}
		}
		if (k == j) {
			ptable[i] = c3;
			c3 += 0x01;
		}
		write8(out, ptable[i]);
	}

	max = c3 - 0x01;

	write32b(out, PW_MOD_MAGIC);	/* ptk ID */

	read16b(in);			/* bypass two unknown bytes */

	/* Track data ... */
	for (i = 0; i <= (maxtaddr / 256); i += 1) {
		memset(tmp, 0, 1025);
		fread(tmp, 256, 1, in);

		for (j = 0; j < 64; j++) {
			note = tmp[j * 4];
			ins = tmp[j * 4 + 1];
			fxt = tmp[j * 4 + 2] & 0x0f;
			fxp = tmp[j * 4 + 3];

			tdata[i][j * 4] = (ins & 0xf0);

			if (note != 0xa8) {
				tdata[i][j * 4] |=
					ptk_table[(note / 2) - 35][0];
				tdata[i][j * 4 + 1] |=
					ptk_table[(note / 2) - 35][1];
			}
			tdata[i][j * 4 + 2] = (ins << 4) & 0xf0;
			tdata[i][j * 4 + 2] |= (fxt & 0x0f);
			tdata[i][j * 4 + 3] = fxp;
		}
	}

	for (i = 0; i <= max; i++) {
		memset(tmp, 0, 1025);
		for (j = 0; j < 64; j++) {
			tmp[j * 16] = tdata[taddr[i][0] / 256][j * 4];
			tmp[j * 16 + 1] = tdata[taddr[i][0] / 256][j * 4 + 1];
			tmp[j * 16 + 2] = tdata[taddr[i][0] / 256][j * 4 + 2];
			tmp[j * 16 + 3] = tdata[taddr[i][0] / 256][j * 4 + 3];

			tmp[j * 16 + 4] = tdata[taddr[i][1] / 256][j * 4];
			tmp[j * 16 + 5] = tdata[taddr[i][1] / 256][j * 4 + 1];
			tmp[j * 16 + 6] = tdata[taddr[i][1] / 256][j * 4 + 2];
			tmp[j * 16 + 7] = tdata[taddr[i][1] / 256][j * 4 + 3];

			tmp[j * 16 + 8] = tdata[taddr[i][2] / 256][j * 4];
			tmp[j * 16 + 9] = tdata[taddr[i][2] / 256][j * 4 + 1];
			tmp[j * 16 + 10] = tdata[taddr[i][2] / 256][j * 4 + 2];
			tmp[j * 16 + 11] = tdata[taddr[i][2] / 256][j * 4 + 3];

			tmp[j * 16 + 12] = tdata[taddr[i][3] / 256][j * 4];
			tmp[j * 16 + 13] = tdata[taddr[i][3] / 256][j * 4 + 1];
			tmp[j * 16 + 14] = tdata[taddr[i][3] / 256][j * 4 + 2];
			tmp[j * 16 + 15] = tdata[taddr[i][3] / 256][j * 4 + 3];
		}
		fwrite (tmp, 1024, 1, out);
	}

	/* sample data */
	pw_move_data(out, in, ssize);

	return 0;
}

static int test_kris (uint8 *data, int s)
{
	int j;
	int start = 0;

	/* test 1 */
	PW_REQUEST_DATA (s, 1024);

	if (data[952] != 'K' || data[953] != 'R' ||
		data[954] != 'I' || data[955] != 'S')
		return -1;

	/* test 2 */
	for (j = 0; j < 31; j++) {
		if (data[47 + j * 30] > 0x40)
			return -1;
		if (data[46 + j * 30] > 0x0f)
			return -1;
	}

	/* test volumes */
	for (j = 0; j < 31; j++) {
		if (data[start + j * 30 + 47] > 0x40)
			return -1;
	}

	return 0;
}
