/*
 * fuzzac.c   Copyright (C) 1997 Asle / ReDoX
 *            Modified by Claudio Matsuoka
 *
 * Converts Fuzzac packed MODs back to PTK MODs
 * thanks to Gryzor and his ProWizard tool ! ... without it, this prog
 * would not exist !!!
 *
 * note: A most worked-up prog ... took some time to finish this !.
 *      there's what lot of my other depacker are missing : the correct
 *      pattern order (most of the time the list is generated badly ..).
 *      Dont know why I did it for this depacker because I've but one
 *      exemple file ! :)
 *
 * $Id: fuzzac.c,v 1.3 2007-10-08 16:51:24 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"


static int test_fuzz (uint8 *, int);
static int depack_fuzz (FILE *, FILE *);

struct pw_format pw_fuzz = {
	"FUZZ",
	"Fuzzac Packer",
	0x00,
	test_fuzz,
	depack_fuzz
};


#define ON  1
#define OFF 2


static int depack_fuzz(FILE *in, FILE *out)
{
	uint8 c1, c2, c3, c4, c5;
	uint8 PatPos;
	uint8 *tmp;
	uint8 NbrTracks;
	uint8 Pattern[1024];
	uint8 PatternList[128];
	uint8 Track_Numbers[128][16];
	uint8 Track_Numbers_Real[128][4];
	uint8 Track_Datas[4][256];
	uint8 Status = ON;
	long ssize = 0;
	long i, j, k, l;

	memset(Track_Numbers, 0, 128 * 16);
	memset(Track_Numbers_Real, 0, 128 * 4);
	memset(PatternList, 0, 128);

	/* bypass ID */
	fseek (in, 4, 0);	/* SEEK_SET */

	/* bypass 2 unknown bytes */
	fseek (in, 2, 1);	/* SEEK_CUR */

	/* write title */
	tmp = (uint8 *) malloc (20);
	memset(tmp, 0, 20);
	fwrite (tmp, 20, 1, out);
	free (tmp);

	/*printf ( "Converting header ... " ); */
	/*fflush ( stdout ); */
	for (i = 0; i < 31; i++) {
		c1 = 0x00;
		for (j = 0; j < 22; j++) {	/*sample name */
			fread (&c1, 1, 1, in);
			fwrite (&c1, 1, 1, out);
		}

		fseek (in, 38, 1);	/* SEEK_CUR */

		fread (&c1, 1, 1, in);	/* size */
		fread (&c2, 1, 1, in);
		ssize += (((c1 << 8) + c2) * 2);
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
		fread (&c1, 1, 1, in);	/* loop start */
		fread (&c2, 1, 1, in);
		fread (&c3, 1, 1, in);	/* loop size */
		fread (&c4, 1, 1, in);
		fread (&c5, 1, 1, in);	/* finetune */
		fwrite (&c5, 1, 1, out);
		fread (&c5, 1, 1, in);	/* volume */
		fwrite (&c5, 1, 1, out);
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);

		if ((c3 << 8) + c4 == 0x00)
			c4 = 0x01;
		fwrite (&c3, 1, 1, out);
		fwrite (&c4, 1, 1, out);
	}
	/*printf ( "ok\n" ); */
	/*printf ( " - Whole sample size : %ld\n" , ssize ); */

	/* read & write size of pattern list */
	fread (&PatPos, 1, 1, in);
	fwrite (&PatPos, 1, 1, out);
	/*printf ( " - size of pattern list : %d\n" , PatPos ); */

	/* read the number of tracks */
	fread (&NbrTracks, 1, 1, in);

	/* write noisetracker byte */
	c1 = 0x7f;
	fwrite (&c1, 1, 1, out);


	/* place file pointer at track number list address */
	fseek (in, 2118, 0);	/* SEEK_SET */

	/* read tracks numbers */
	for (i = 0; i < 4; i++) {
		for (j = 0; j < PatPos; j++) {
			fread (&Track_Numbers[j][i * 4], 1, 1, in);
			fread (&Track_Numbers[j][i * 4 + 1], 1, 1, in);
			fread (&Track_Numbers[j][i * 4 + 2], 1, 1, in);
			fread (&Track_Numbers[j][i * 4 + 3], 1, 1, in);
		}
	}


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
				if (Track_Numbers[j][k * 4] !=
					Track_Numbers[i][k * 4]) {
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
			Track_Numbers_Real[c1][1] = Track_Numbers[i][4];
			Track_Numbers_Real[c1][2] = Track_Numbers[i][8];
			Track_Numbers_Real[c1][3] = Track_Numbers[i][12];
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
		Track_Numbers_Real[c1][1] = Track_Numbers[i][4];
		Track_Numbers_Real[c1][2] = Track_Numbers[i][8];
		Track_Numbers_Real[c1][3] = Track_Numbers[i][12];
		c1 += 0x01;
		Status = ON;
	}

	/* write pattern list */
	fwrite (PatternList, 128, 1, out);

	/* write ID */
	c1 = 'M';
	c2 = '.';
	c3 = 'K';
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);


	/* pattern data */
	/*printf ( "Processing the pattern datas ... " ); */
	/*fflush ( stdout ); */
	l = 2118 + (PatPos * 16);
	for (i = 0; i < c5; i++) {
		memset(Pattern, 0, 1024);
		memset(Track_Datas, 0, 4 << 8);
		fseek (in, l + (Track_Numbers_Real[i][0] << 8), 0);
		fread (Track_Datas[0], 256, 1, in);
		fseek (in, l + (Track_Numbers_Real[i][1] << 8), 0);
		fread (Track_Datas[1], 256, 1, in);
		fseek (in, l + (Track_Numbers_Real[i][2] << 8), 0);
		fread (Track_Datas[2], 256, 1, in);
		fseek (in, l + (Track_Numbers_Real[i][3] << 8), 0);
		fread (Track_Datas[3], 256, 1, in);

		for (j = 0; j < 64; j++) {
			Pattern[j * 16] = Track_Datas[0][j * 4];
			Pattern[j * 16 + 1] = Track_Datas[0][j * 4 + 1];
			Pattern[j * 16 + 2] = Track_Datas[0][j * 4 + 2];
			Pattern[j * 16 + 3] = Track_Datas[0][j * 4 + 3];
			Pattern[j * 16 + 4] = Track_Datas[1][j * 4];
			Pattern[j * 16 + 5] = Track_Datas[1][j * 4 + 1];
			Pattern[j * 16 + 6] = Track_Datas[1][j * 4 + 2];
			Pattern[j * 16 + 7] = Track_Datas[1][j * 4 + 3];
			Pattern[j * 16 + 8] = Track_Datas[2][j * 4];
			Pattern[j * 16 + 9] = Track_Datas[2][j * 4 + 1];
			Pattern[j * 16 + 10] = Track_Datas[2][j * 4 + 2];
			Pattern[j * 16 + 11] = Track_Datas[2][j * 4 + 3];
			Pattern[j * 16 + 12] = Track_Datas[3][j * 4];
			Pattern[j * 16 + 13] = Track_Datas[3][j * 4 + 1];
			Pattern[j * 16 + 14] = Track_Datas[3][j * 4 + 2];
			Pattern[j * 16 + 15] = Track_Datas[3][j * 4 + 3];
		}

		fwrite (Pattern, 1024, 1, out);
		/*printf ( "+" ); */
		/*fflush ( stdout ); */
	}
	/*printf ( "ok\n" ); */

	/* sample data */
	/*printf ( "Saving sample data ... " ); */
	/*fflush ( stdout ); */
	fseek (in, l + (NbrTracks << 8) + 4, SEEK_SET);
	/* l : 2118 + npat*16 */
	/* 4 : to bypass the "SEnd" unidentified ID */
	tmp = (uint8 *) malloc (ssize);
	fread (tmp, ssize, 1, in);
	fwrite (tmp, ssize, 1, out);
	free (tmp);
	/*printf ( "ok\n" ); */

	return 0;
}


static int test_fuzz (uint8 *data, int s)
{
	int j, k;
	int start = 0, ssize = 0;

	if (data[0]!='M' || data[1]!='1' || data[2]!='.' || data[3]!='0')
		return -1;

	/* test finetune */
	for (k = 0; k < 31; k++) {
		if (data[start + 72 + k * 68] > 0x0F)
			return -1;
	}

	/* test volumes */
	for (k = 0; k < 31; k++) {
		if (data[start + 73 + k * 68] > 0x40)
			return -1;
	}

	/* test sample sizes */
	for (k = 0; k < 31; k++) {
		j = (data[start + 66 + k * 68] << 8) +
			data[start + 67 + k * 68];
		if (j > 0x8000)
			return -1;
		ssize += (j * 2);
	}

	/* test size of pattern list */
	if (data[start + 2114] == 0x00)
		return -1;

	return 0;
}
