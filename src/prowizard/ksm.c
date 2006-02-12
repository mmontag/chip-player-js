/*
 * Kefrens_Sound_Machine.c   Copyright (C) 1997 Sylvain "Asle" Chipaux
 *                           Modified by Claudio Matsuoka
 *
 * Depacks musics in the Kefrens Sound Machine format and saves in ptk.
 *
 * NOTE: some lines will work ONLY on IBM-PC !!!. Check the lines
 *      with the warning note to get this code working on 68k machines.
 *
 * $Id: ksm.c,v 1.1 2006-02-12 22:04:42 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"


static int test_ksm (uint8 *, int);
static int depack_ksm (FILE *, FILE *);

struct pw_format pw_ksm = {
	"KSM",
	"Kefrens Sound Machine",
	0x00,
	test_ksm,
	NULL,
	depack_ksm
};


#define ON  1
#define OFF 2

static int depack_ksm (FILE *in, FILE *out)
{
	uint8 *tmp;
	uint8 *address;
	uint8 c1 = 0x00, c2 = 0x00, c3 = 0x00, c5;
	uint8 PatternList[128];
	uint8 Track_Numbers[128][4];
	uint8 Track_Numbers_Real[128][4];
	uint8 Track_Datas[4][192];
	uint8 Max = 0x00;
	uint8 PatPos;
	uint8 Status = ON;
	long ssize = 0;
	long i = 0, j = 0, k = 0;

	bzero (PatternList, 128);
	bzero (Track_Numbers, 128 * 4);
	bzero (Track_Numbers_Real, 128 * 4);

	/* title */
	tmp = (uint8 *) malloc (20);
	bzero (tmp, 20);
	fseek (in, 2, 0);
	fread (tmp, 13, 1, in);
	fwrite (tmp, 20, 1, out);
	free (tmp);

	/* read and write whole header */
	/*printf ( "Converting sample headers ... " ); */
	fseek (in, 32, 0);
	tmp = (uint8 *) malloc (22);
	bzero (tmp, 22);
	for (i = 0; i < 15; i++) {
		/* write name */
		fwrite (tmp, 22, 1, out);
		/* bypass 16 unknown bytes and 4 address bytes */
		fseek (in, 20, 1);
		/* size */
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		k = (c1 << 8) + c2;
		ssize += k;
		c2 /= 2;
		if ((c1 / 2) * 2 != c1) {
			if (c2 < 0x80)
				c2 += 0x80;
			else {
				c2 -= 0x80;
				c1 += 0x01;
			}
		}
		c1 /= 2;
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
		/* finetune */
		c1 = 0x00;
		fwrite (&c1, 1, 1, out);
		/* volume */
		fread (&c1, 1, 1, in);
		fwrite (&c1, 1, 1, out);
		/* bypass 1 unknown byte */
		fseek (in, 1, 1);
		/* loop start */
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		j = k - ((c1 << 8) + c2);
		c2 /= 2;
		if ((c1 / 2) * 2 != c1) {
			if (c2 < 0x80)
				c2 += 0x80;
			else {
				c2 -= 0x80;
				c1 += 0x01;
			}
		}
		c1 /= 2;
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);

		if (j != k) {
			/* write loop size */
			/* WARNING !!! WORKS ONLY ON PC !!!       */
			/* 68k machines code : c1 = *(address+2); */
			/* 68k machines code : c2 = *(address+3); */
			j /= 2;
			address = (uint8 *) & j;
			c1 = *(address + 1);
			c2 = *address;
			fwrite (&c1, 1, 1, out);
			fwrite (&c2, 1, 1, out);
		} else {
			c1 = 0x00;
			c2 = 0x01;
			fwrite (&c1, 1, 1, out);
			fwrite (&c2, 1, 1, out);
		}
		/* bypass 6 unknown bytes */
		fseek (in, 6, 1);
	}
	free (tmp);
	tmp = (uint8 *) malloc (30);
	bzero (tmp, 30);
	tmp[29] = 0x01;
	for (i = 0; i < 16; i++)
		fwrite (tmp, 30, 1, out);
	free (tmp);
	/*printf ( "ok\n" ); */

	/* pattern list */
	/*printf ( "creating the pattern list ... " ); */
	fseek (in, 512, 0);
	for (PatPos = 0x00; PatPos < 128; PatPos++) {
		fread (&Track_Numbers[PatPos][0], 1, 1, in);
		fread (&Track_Numbers[PatPos][1], 1, 1, in);
		fread (&Track_Numbers[PatPos][2], 1, 1, in);
		fread (&Track_Numbers[PatPos][3], 1, 1, in);
		if (Track_Numbers[PatPos][0] == 0xFF)
			break;
		if (Track_Numbers[PatPos][0] > Max)
			Max = Track_Numbers[PatPos][0];
		if (Track_Numbers[PatPos][1] > Max)
			Max = Track_Numbers[PatPos][1];
		if (Track_Numbers[PatPos][2] > Max)
			Max = Track_Numbers[PatPos][2];
		if (Track_Numbers[PatPos][3] > Max)
			Max = Track_Numbers[PatPos][3];
	}

	/* write patpos */
	fwrite (&PatPos, 1, 1, out);

	/* ntk byte */
	c1 = 0x7f;
	fwrite (&c1, 1, 1, out);

	/* sort tracks numbers */
	c5 = 0x00;
	for (i = 0; i < PatPos; i++) {
		if (i == 0) {
			PatternList[0] = c5;
			c5 += 0x01;
			continue;
		}
		for (j = 0; j < i; j++) {
			Status = ON;
			for (k = 0; k < 4; k++) {
				if (Track_Numbers[j][k] !=
					Track_Numbers[i][k]) {
					Status = OFF;
					break;
				}
			}
			if (Status == ON) {
				PatternList[i] = PatternList[j];
				break;
			}
		}
		if (Status == OFF) {
			PatternList[i] = c5;
			c5 += 0x01;
		}
		Status = ON;
	}
	/* c5 is the Max pattern number */

	/* create a real list of tracks numbers for the really existing patterns */
	c1 = 0x00;
	for (i = 0; i < PatPos; i++) {
		if (i == 0) {
			Track_Numbers_Real[c1][0] = Track_Numbers[i][0];
			Track_Numbers_Real[c1][1] = Track_Numbers[i][1];
			Track_Numbers_Real[c1][2] = Track_Numbers[i][2];
			Track_Numbers_Real[c1][3] = Track_Numbers[i][3];
			c1 += 0x01;
			continue;
		}
		for (j = 0; j < i; j++) {
			Status = ON;
			if (PatternList[i] == PatternList[j]) {
				Status = OFF;
				break;
			}
		}
		if (Status == OFF)
			continue;
		Track_Numbers_Real[c1][0] = Track_Numbers[i][0];
		Track_Numbers_Real[c1][1] = Track_Numbers[i][1];
		Track_Numbers_Real[c1][2] = Track_Numbers[i][2];
		Track_Numbers_Real[c1][3] = Track_Numbers[i][3];
		c1 += 0x01;
		Status = ON;
	}

	/* write pattern list */
	fwrite (PatternList, 128, 1, out);
	/*printf ( "ok\n" ); */

	/* write ID */
	c1 = 'M';
	c2 = '.';
	c3 = 'K';
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);

	/* pattern data */
	/*printf ( "Converting pattern datas " ); */
	tmp = (uint8 *) malloc (1024);
	for (i = 0; i < c5; i++) {
		bzero (tmp, 1024);
		bzero (Track_Datas, 192 * 4);
		fseek (in, 1536 + 192 * Track_Numbers_Real[i][0], 0);
		fread (Track_Datas[0], 192, 1, in);
		fseek (in, 1536 + 192 * Track_Numbers_Real[i][1], 0);
		fread (Track_Datas[1], 192, 1, in);
		fseek (in, 1536 + 192 * Track_Numbers_Real[i][2], 0);
		fread (Track_Datas[2], 192, 1, in);
		fseek (in, 1536 + 192 * Track_Numbers_Real[i][3], 0);
		fread (Track_Datas[3], 192, 1, in);

		for (j = 0; j < 64; j++) {
			tmp[j * 16] = ptk_table[Track_Datas[0][j * 3]][0];
			tmp[j * 16 + 1] = ptk_table[Track_Datas[0][j * 3]][1];
			if ((Track_Datas[0][j * 3 + 1] & 0x0f) == 0x0D)
				Track_Datas[0][j * 3 + 1] -= 0x03;
			tmp[j * 16 + 2] = Track_Datas[0][j * 3 + 1];
			tmp[j * 16 + 3] = Track_Datas[0][j * 3 + 2];

			tmp[j * 16 + 4] = ptk_table[Track_Datas[1][j * 3]][0];
			tmp[j * 16 + 5] = ptk_table[Track_Datas[1][j * 3]][1];
			if ((Track_Datas[1][j * 3 + 1] & 0x0f) == 0x0D)
				Track_Datas[1][j * 3 + 1] -= 0x03;
			tmp[j * 16 + 6] = Track_Datas[1][j * 3 + 1];
			tmp[j * 16 + 7] = Track_Datas[1][j * 3 + 2];

			tmp[j * 16 + 8] = ptk_table[Track_Datas[2][j * 3]][0];
			tmp[j * 16 + 9] = ptk_table[Track_Datas[2][j * 3]][1];
			if ((Track_Datas[2][j * 3 + 1] & 0x0f) == 0x0D)
				Track_Datas[2][j * 3 + 1] -= 0x03;
			tmp[j * 16 + 10] = Track_Datas[2][j * 3 + 1];
			tmp[j * 16 + 11] = Track_Datas[2][j * 3 + 2];

			tmp[j * 16 + 12] =
				ptk_table[Track_Datas[3][j * 3]][0];
			tmp[j * 16 + 13] =
				ptk_table[Track_Datas[3][j * 3]][1];
			if ((Track_Datas[3][j * 3 + 1] & 0x0f) == 0x0D)
				Track_Datas[3][j * 3 + 1] -= 0x03;
			tmp[j * 16 + 14] = Track_Datas[3][j * 3 + 1];
			tmp[j * 16 + 15] = Track_Datas[3][j * 3 + 2];
		}


		fwrite (tmp, 1024, 1, out);
	}
	free (tmp);

	/* sample data */
	tmp = (uint8 *) malloc (ssize);
	bzero (tmp, ssize);
	fseek (in, 1536 + (192 * (Max + 1)), 0);
	fread (tmp, ssize, 1, in);
	fwrite (tmp, ssize, 1, out);
	free (tmp);

	return 0;
}

static int test_ksm (uint8 *data, int s)
{
	int j, k, l;
	int start = 0;

	PW_REQUEST_DATA (s, 1536);

	if (data[start] != 'M' || data[start + 1] != '.')
		return -1;

	/* test "a" */
	if (data[start + 15] != 'a')
		return -1;

	/* test volumes */
	for (k = 0; k < 15; k++) {
		if (data[start + 54 + k * 32] > 0x40)
			return -1;
	}


	/* test tracks data */
	/* first, get the highest track number .. */
	j = 0;
	for (k = 0; k < 1024; k++) {
		if (data[start + k + 512] == 0xFF)
			break;
		if (data[start + k + 512] > j)
			j = data[start + k + 512];
	}

	if (k == 1024)
		return -1;

	if (j == 0)
		return -1;

	PW_REQUEST_DATA (s, start + 1536 + j * 192 + 63 * 3);

	/* so, now, j is the highest track number (first is 00h !!) */
	/* real test on tracks data starts now */
	for (k = 0; k <= j; k++) {
		for (l = 0; l < 64; l++) {
			if (data[start + 1536 + k * 192 + l * 3] > 0x24)
				return -1;
		}
	}

	/* j is still the highest track number */

	return 0;
}
