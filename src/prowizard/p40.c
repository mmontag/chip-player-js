/*
 * The_Player_4.0.c   Copyright (C) 1997 Asle / ReDoX
 *                    Copyright (C) 2007 Claudio Matsuoka
 *
 * The Player 4.0a and 4.0b to Protracker.
 *
 * note: It's a REAL mess !. It's VERY badly coded, I know. Just dont forget
 *      it was mainly done to test the description I made of P40* format. I
 *      certainly wont dare to beat Gryzor on the ground :). His Prowiz IS
 *      the converter to use !!!.
 *
 * $Id: p40.c,v 1.3 2007-09-29 17:01:33 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"

static int test_p40(uint8 *, int);
static int depack_p40 (FILE *, FILE *);

struct pw_format pw_p40 = {
	"P40",
	"The Player 4.0",
	0x00,
	test_p40,
	NULL,
	depack_p40
};


struct smp {
	uint8 name[22];
	int addy;
	uint8 size[2];
	int loop_addy;
	uint8 loop_size[2];
	uint16 fine;
	uint8 vol;
};

static int depack_p40 (FILE *in, FILE *out)
{
	uint8 c1, c2, c3, c4, c5;
	uint8 *tmp;
	uint8 PatPos = 0x00;
	uint8 PatMax = 0x00;
	uint8 Nbr_Sample = 0x00;
	uint8 sample, mynote, note[2];
	uint8 Track_Data[512][256];
	short Track_Addresses[128][4];
	int Track_Data_Address = 0;
	int Track_Table_Address = 0;
	int Sample_Data_Address = 0;
	int ssize = 0;
	int SampleAddress[31];
	int SampleSize[31];
	int i, j, k, l, a, b, c;
	struct smp ins;

	bzero(Track_Addresses, 128 * 4 * 2);
	bzero(Track_Data, 512 << 8);
	bzero(SampleAddress, 31 * 4);
	bzero(SampleSize, 31 * 4);

	/* read check ID */
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	fread (&c3, 1, 1, in);
	fread (&c4, 1, 1, in);
	if (c4 == 'A')
		pw_p40.name = "The Player 4.0A";
	else if (c4 == 'B')
		pw_p40.name = "The Player 4.0B";

	/* read Real number of pattern */
	fread (&PatMax, 1, 1, in);

	/* read number of pattern in pattern list */
	fread (&PatPos, 1, 1, in);

	/* read number of samples */
	fread (&Nbr_Sample, 1, 1, in);

	/* bypass empty byte */
	fseek (in, 1, 1);	/* SEEK_CUR */


/**********/

	/* read track data address */
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	fread (&c3, 1, 1, in);
	fread (&c4, 1, 1, in);
	Track_Data_Address =
		(c1 << 24) + (c2 << 16) + (c3 << 8) + c4;

	/* read track table address */
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	fread (&c3, 1, 1, in);
	fread (&c4, 1, 1, in);
	Track_Table_Address =
		(c1 << 24) + (c2 << 16) + (c3 << 8) + c4;

	/* read sample data address */
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	fread (&c3, 1, 1, in);
	fread (&c4, 1, 1, in);
	Sample_Data_Address =
		(c1 << 24) + (c2 << 16) + (c3 << 8) + c4;


	/* write title */
	tmp = (uint8 *) malloc (21);
	bzero (tmp, 21);
	fwrite (tmp, 20, 1, out);
	free (tmp);

	/* sample headers stuff */
	bzero(ins.name, 22);
	for (i = 0; i < Nbr_Sample; i++) {
		/* read sample data address */
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		fread (&c3, 1, 1, in);
		fread (&c4, 1, 1, in);
		ins.addy = (c1 << 24) + (c2 << 16) + (c3 << 8) + c4;
		SampleAddress[i] = ins.addy;

		/* write sample name */
		fwrite (ins.name, 22, 1, out);

		/* read sample size */
		fread (&ins.size[0], 1, 1, in);
		fread (&ins.size[1], 1, 1, in);
		ssize += (((ins.size[0] << 8) + ins.size[1]) * 2);
		SampleSize[i] = (((ins.size[0]) << 8) + ins.size[1]) * 2;

		/* loop start */
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		fread (&c3, 1, 1, in);
		fread (&c4, 1, 1, in);
		ins.loop_addy = (c1 << 24) + (c2 << 16) + (c3 << 8) + c4;

		/* loop size */
		fread (&ins.loop_size[0], 1, 1, in);
		fread (&ins.loop_size[1], 1, 1, in);

		/* fine */
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		ins.fine = (c1 << 8) + c2;

		/* bypass 00h */
		fseek (in, 1, 1);

		/* read vol */
		fread (&ins.vol, 1, 1, in);

		/* writing now */
		fwrite (ins.size, 2, 1, out);
		c1 = ins.fine / 74;
		fwrite (&c1, 1, 1, out);
		fwrite (&ins.vol, 1, 1, out);
		ins.loop_addy -= ins.addy;
		ins.loop_addy /= 2;
		/* WARNING !!! WORKS ONLY ON 80X86 computers ! */
		/* 68k machines code : c3 = *(tmp+2); */
		/* 68k machines code : c4 = *(tmp+3); */
		tmp = (uint8 *) & ins.loop_addy;
		c3 = *(tmp + 1);
		c4 = *tmp;
		fwrite (&c3, 1, 1, out);
		fwrite (&c4, 1, 1, out);
		fwrite (ins.loop_size, 2, 1, out);
	}

	/* go up to 31 samples */
	tmp = (uint8 *) malloc (30);
	bzero (tmp, 30);
	tmp[29] = 0x01;
	while (i != 31) {
		fwrite (tmp, 30, 1, out);
		i += 1;
	}
	free (tmp);

	/* write size of pattern list */
	fwrite (&PatPos, 1, 1, out);

	/* write noisetracker byte */
	c1 = 0x7f;
	fwrite (&c1, 1, 1, out);

	/* place file pointer at the pattern list address ... should be */
	/* useless, but then ... */
	fseek (in, Track_Table_Address + 4, 0);	/* SEEK_SET */

	/* create and write pattern list .. no optimization ! */
	/* I'll optimize when I'll feel in the mood */
	for (c1 = 0x00; c1 < PatPos; c1++) {
		fwrite (&c1, 1, 1, out);
	}
	c2 = 0x00;
	while (c1 < 128) {
		fwrite (&c2, 1, 1, out);
		c1 += 0x01;
	}

	/* write ptk's ID */
	c1 = 'M';
	c2 = '.';
	c3 = 'K';
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);

	/* reading all the track addresses */
	for (i = 0; i < PatPos; i++) {
		for (j = 0; j < 4; j++) {
			fread (&c1, 1, 1, in);
			fread (&c2, 1, 1, in);
			Track_Addresses[i][j] =
				(c1 << 8) + c2 + Track_Data_Address + 4;
		}
	}


	/* place file pointer at the pattern list address ... should be */
	/* useless, but then ... */
	fseek (in, Track_Data_Address + 4, 0);	/* SEEK_SET */


	/* rewrite the track data */
	/*printf ( "sorting and depacking tracks data ... " ); */
	fflush (stdout);
	for (i = 0; i < PatPos; i++) {
		for (j = 0; j < 4; j++) {
			fseek (in, Track_Addresses[i][j], 0);	/* SEEK_SET */
			for (k = 0; k < 64; k++) {
				fread (&c1, 1, 1, in);
				fread (&c2, 1, 1, in);
				fread (&c3, 1, 1, in);
				fread (&c4, 1, 1, in);

				if (c1 != 0x80) {

					sample =
						((c1 << 4) & 0x10) | ((c2 >>
						       4) & 0x0f);
					bzero (note, 2);
					mynote = c1 & 0x7f;
					note[0] = ptk_table[mynote / 2][0];
					note[1] = ptk_table[mynote / 2][1];
					switch (c2 & 0x0f) {
					case 0x08:
						c2 -= 0x08;
						break;
					case 0x05:
					case 0x06:
					case 0x0A:
						if (c3 >= 0x80)
							c3 = (c3 << 4) & 0xf0;
						break;
					default:
						break;
					}
					Track_Data[i * 4 + j][k * 4] =
						(sample & 0xf0) | (note[0] &
						0x0f);
					Track_Data[i * 4 + j][k * 4 + 1] =
						note[1];
					Track_Data[i * 4 + j][k * 4 + 2] = c2;
					Track_Data[i * 4 + j][k * 4 + 3] = c3;

					if ((c4 > 0x00) && (c4 < 0x80))
						k += c4;
					if ((c4 > 0x7f)) {
						k += 1;
						for (l = 256; l > c4; l--) {
							Track_Data[i * 4 +
								j][k * 4] =
								(sample &
								0xf0) |
								(note[0] &
								0x0f);
							Track_Data[i * 4 +
								j][k * 4 +
								1] = note[1];
							Track_Data[i * 4 +
								j][k * 4 +
								2] = c2;
							Track_Data[i * 4 +
								j][k * 4 +
								3] = c3;
							k += 1;
						}
						k -= 1;
					}
				}

				else {
					a = ftell (in);

					c5 = c2;
					b =
						(c3 << 8) + c4 +
						Track_Data_Address + 4;
					fseek (in, b, 0);	/* SEEK_SET */
/*fprintf ( debug , "%2d (pattern %ld)(at %x)\n" , c2 , i , a-4 );*/
					for (c = 0; c <= c5; c++) {
/*fprintf ( debug , "%ld," , k );*/
						fread (&c1, 1, 1, in);
						fread (&c2, 1, 1, in);
						fread (&c3, 1, 1, in);
						fread (&c4, 1, 1, in);

						sample =
							((c1 << 4) & 0x10) |
							((c2 >> 4) & 0x0f);
						bzero (note, 2);
						mynote = c1 & 0x7f;
						note[0] = ptk_table[mynote / 2][0];
						note[1] = ptk_table[mynote / 2][1];
						switch (c2 & 0x0f) {
						case 0x08:
							c2 -= 0x08;
							break;
						case 0x05:
						case 0x06:
						case 0x0A:
							if (c3 >= 0x80)
								c3 =
									(c3 <<
									4) &
									0xf0;
							break;
						default:
							break;
						}
						Track_Data[i * 4 + j][k * 4] =
							(sample & 0xf0) |
							(note[0] & 0x0f);
						Track_Data[i * 4 + j][k * 4 +
							1] = note[1];
						Track_Data[i * 4 + j][k * 4 +
							2] = c2;
						Track_Data[i * 4 + j][k * 4 +
							3] = c3;

						if ((c4 > 0x00)
							&& (c4 < 0x80)) k +=
								c4;
						if ((c4 > 0x7f)) {
							k += 1;
							for (l = 256; l > c4;
								l--) {
								Track_Data[i *
									4 +
									j][k *
									4] =
									(sample
									&
									0xf0)
									|
									(note
									[0] &
									0x0f);
								Track_Data[i *
									4 +
									j][k *
									4 +
									1] =
									note
									[1];
								Track_Data[i *
									4 +
									j][k *
									4 +
									2] =
									c2;
								Track_Data[i *
									4 +
									j][k *
									4 +
									3] =
									c3;
								k += 1;
							}
							k -= 1;
						}
						k += 1;
					}

					k -= 1;
					fseek (in, a, 0);	/* SEEK_SET */
/*fprintf ( debug , "\n## back to %x\n" , a );*/
				}
			}
		}
	}
	/*printf ( "ok\n" ); */

/*
for ( i=0 ; i<PatPos*4 ; i++ )
{
  fprintf ( debug , "\n\ntrack #%ld----------------\n" , i );
  for ( j=0 ; j<64 ; j++ )
  {
    fprintf ( debug , "%2x %2x %2x %2x\n"
    , Track_Data[i][j*4]
    , Track_Data[i][j*4+1]
    , Track_Data[i][j*4+2]
    , Track_Data[i][j*4+3] );
  }
}
*/

	/* write pattern data */
	/*printf ( "writing pattern data ... " ); */
	fflush (stdout);
	tmp = (uint8 *) malloc (1024);
	for (i = 0; i < PatPos; i++) {
		bzero (tmp, 1024);
		for (j = 0; j < 64; j++) {
			for (k = 0; k < 4; k++) {
				tmp[j * 16 + k * 4] =
					Track_Data[k + i * 4][j * 4];
				tmp[j * 16 + k * 4 + 1] =
					Track_Data[k + i * 4][j * 4 + 1];
				tmp[j * 16 + k * 4 + 2] =
					Track_Data[k + i * 4][j * 4 + 2];
				tmp[j * 16 + k * 4 + 3] =
					Track_Data[k + i * 4][j * 4 + 3];
			}
		}
		fwrite (tmp, 1024, 1, out);
	}
	free (tmp);
	/*printf ( "ok\n" ); */


	/* go to sample data address */
	fseek (in, Sample_Data_Address + 4, 0);	/* SEEK_SET */

	/* read and write sample data */
	/*printf ( "writing sample data ... " ); */
	fflush (stdout);
	for (i = 0; i < Nbr_Sample; i++) {
		fseek (in, SampleAddress[i] + Sample_Data_Address, 0);
		tmp = (uint8 *) malloc (SampleSize[i]);
		bzero (tmp, SampleSize[i]);
		fread (tmp, SampleSize[i], 1, in);
		fwrite (tmp, SampleSize[i], 1, out);
		free (tmp);
	}
	/*printf ( "ok\n" ); */

	return 0;
}


static int test_p40(uint8 *data, int s)
{
	int j, k, l, o, n;
	int start, ssize;

	start = 0;

	/* number of pattern (real) */
	j = data[start + 4];
	if (j > 0x7f) {
		return -1;
	}

	/* number of sample */
	k = data[start + 6];
	if ((k > 0x1F) || (k == 0)) {
		return -1;
	}

	/* test volumes */
	for (l = 0; l < k; l++) {
		if (data[start + 35 + l * 16] > 0x40) {
			return -1;
		}
	}

	/* test sample sizes */
	ssize = 0;
	for (l = 0; l < k; l++) {
		/* size */
		o = (data[start + 24 + l * 16] << 8) +
			data[start + 25 + l * 16];
		/* loop size */
		n = (data[start + 30 + l * 16] << 8) +
			data[start + 31 + l * 16];
		o *= 2;
		n *= 2;

		if ((o > 0xFFFF) || (n > 0xFFFF)) {
			return -1;
		}

		if (n > (o + 2)) {
			return -1;
		}
		ssize += o;
	}
	if (ssize <= 4) {
		return - 1;
	}

	/* ssize is the size of the sample data .. WRONG !! */
	/* k is the number of samples */
	return 0;
}
