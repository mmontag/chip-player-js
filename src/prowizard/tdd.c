/*
 * tdd.c   Copyright (C) 1999 Asle / ReDoX
 *         Copyright (C) 2006-2007 Claudio Matsuoka
 *
 * Converts TDD packed MODs back to PTK MODs
 *
 * $Id: tdd.c,v 1.7 2007-10-08 16:51:26 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"


static int test_tdd (uint8 *, int);
static int depack_tdd (FILE *, FILE *);

struct pw_format pw_tdd = {
	"TDD",
	"The Dark Demon",
	0x00,
	test_tdd,
	depack_tdd
};


static int depack_tdd (FILE *in, FILE *out)
{
	uint8 *tmp;
	uint8 pat[1024];
	uint8 pmax;
	int i, j, k;
	int size, ssize = 0;
	int saddr[31];
	int ssizes[31];

	memset(saddr, 0, 31 * 4);
	memset(ssizes, 0, 31 * 4);

	/* write ptk header */
	pw_write_zero(out, 1080);

	/* read/write pattern list + size and ntk byte */
	tmp = (uint8 *)malloc(130);
	memset(tmp, 0, 130);
	fseek(out, 950, 0);
	fread(tmp, 130, 1, in);
	fwrite(tmp, 130, 1, out);

	for (pmax = i = 0; i < 128; i++)
		if (tmp[i + 2] > pmax)
			pmax = tmp[i + 2];
	free(tmp);

	/* sample descriptions */
	for (i = 0; i < 31; i++) {
		fseek(out, 42 + (i * 30), SEEK_SET);
		/* sample address */
		saddr[i] = read32b(in);

		/* read/write size */
		write16b(out, size = read16b(in));
		ssize += size;
		ssizes[i] = size;

		write8(out, read8(in));		/* read/write finetune */
		write8(out, read8(in));		/* read/write volume */
		/* read/write loop start */
		write16b(out, (read32b(in) - saddr[i]) / 2);
		write16b(out, read16b(in));	/* read/write replen */
	}

	/* bypass Samples datas */
	fseek(in, ssize, SEEK_CUR);

	/* write ptk's ID string */
	fseek (out, 0, 2);
	write32b(out, PW_MOD_MAGIC);

	/* read/write pattern data */
	tmp = (uint8 *)malloc(1024);
	for (i = 0; i <= pmax; i++) {
		memset(tmp, 0, 1024);
		memset(pat, 0, 1024);
		fread(tmp, 1024, 1, in);
		for (j = 0; j < 64; j++) {
			for (k = 0; k < 4; k++) {
				int x = j * 16 + k * 4;

				/* fx arg */
				pat[x + 3] = tmp[x + 3];

				/* fx */
				pat[x + 2] = tmp[x + 2] & 0x0f;

				/* smp */
				pat[x] = tmp[x] & 0xf0;
				pat[x + 2] |= (tmp[x] << 4) & 0xf0;

				/* note */
				pat[x] |= ptk_table[tmp[x + 1] / 2][0];
				pat[x + 1] = ptk_table[tmp[x + 1] / 2][1];
			}
		}
		fwrite(pat, 1024, 1, out);
	}
	free(tmp);

	/* Sample data */
	for (i = 0; i < 31; i++) {
		if (ssizes[i] == 0)
			continue;
		fseek(in, saddr[i], SEEK_SET);
		pw_move_data(out, in, ssizes[i]);
	}

	return 0;
}


static int test_tdd (uint8 *data, int s)
{
	int j, k, l, m, n;
	int start = 0, ssize;

	PW_REQUEST_DATA (s, 564);

	/* test #2 (volumes,sample addresses and whole sample size) */
	ssize = 0;
	for (j = 0; j < 31; j++) {
		/* sample address */
		k = (data[start + j * 14 + 130] << 24) +
			(data[start + j * 14 + 131] << 16) +
			(data[start + j * 14 + 132] << 8) +
			data[start + j * 14 + 133];

		/* sample size */
		l = (((data[start + j * 14 + 134] << 8) +
			data[start + j * 14 + 135]) * 2);

		/* loop start address */
		m = (data[start + j * 14 + 138] << 24) +
			(data[start + j * 14 + 139] << 16) +
			(data[start + j * 14 + 140] << 8) +
			data[start + j * 14 + 141];

		/* loop size (replen) */
		n = (((data[start + j * 14 + 142] << 8) +
			data[start + j * 14 + 143]) * 2);

		/* volume > 40h ? */
		if (data[start + j * 14 + 137] > 0x40)
			return -1;

		/* loop start addy < sampl addy ? */
		if (m < k)
			return -1;

		/* addy < 564 ? */
		if (k < 564 || m < 564)
			return -1;

		/* loop start > size ? */
		if ((m - k) > l)
			return -1;

		/* loop start+replen > size ? */
		if ((m - k + n) > (l + 2))
			return -1;

		ssize += l;
	}

	if (ssize <= 2 || ssize > (31 * 65535))
		return -1;

#if 0
	/* test #3 (addresses of pattern in file ... ptk_tableible ?) */
	/* ssize is the whole sample size :) */
	if ((ssize + 564) > in_size) {
/*printf ( "#3 (start:%ld)\n" , start );*/
		Test = BAD;
		return;
	}
#endif

	/* test size of pattern list */
	if (data[start] > 0x7f || data[start] == 0x00)
		return -1;

	/* test pattern list */
	k = 0;
	for (j = 0; j < 128; j++) {
		if (data[start + j + 2] > 0x7f)
			return -1;
		if (data[start + j + 2] > k)
			k = data[start + j + 2];
	}
	k += 1;
	k *= 1024;

	/* test end of pattern list */
	for (j = data[start] + 2; j < 128; j++) {
		if (data[start + j + 2] != 0)
			return -1;
	}

#if 0
	/* test if not out of file range */
	if ((start + ssize + 564 + k) > in_size)
		return -1;
#endif

	/* ssize is the whole sample data size */
	/* k is the whole pattern data size */
	/* test pattern data now ... */
	l = start + 564 + ssize;
	/* l points on pattern data */

	for (j = 0; j < k; j += 4) {
		/* sample number > 31 ? */
		if (data[l + j] > 0x1f)
			return -1;

		/* note > 0x48 (36*2) */
		if (data[l + j + 1] > 0x48 || (data[l + j + 1] & 0x01) == 0x01)
			return -1;

		/* fx=C and fxtArg > 64 ? */
		if ((data[l + j + 2] & 0x0f) == 0x0c && data[l + j + 3] > 0x40)
			return -1;

		/* fx=D and fxtArg > 64 ? */
		if ((data[l + j + 2] & 0x0f) == 0x0d && data[l + j + 3] > 0x40)
			return -1;

		/* fx=B and fxtArg > 127 ? */
		if ((data[l + j + 2] & 0x0f) == 0x0b)
			return -1;
	}

	return -1;
}
