/*
 *   The_Player_6.1a.c   1998 (c) Asle / ReDoX
 *
 * The Player 6.1a to Protracker.
 *
 * note: As for version 5.0A and 6.0A, it's a REAL mess !.
 *      It's VERY badly coded, I know. Just dont forget it was mainly done
 *      to test the description I made of P61a format.
 *      I certainly wont dare to beat Gryzor on the ground :). His Prowiz IS
 *      the converter to use !!!. Though, using the official depacker could
 *      be a good idea too :).
 *
*/

#include <string.h>
#include <stdlib.h>

#define ON 1
#define OFF 2

void Depack_P61A (FILE * in, FILE * out)
{
	uint8 c1, c2, c3, c4, c5, c6;
	long Max;
	uint8 *tmp;
	signed char *insDataWork;
	uint8 PatPos = 0x00;
	uint8 PatMax = 0x00;
	uint8 Nbr_Sample = 0x00;
	uint8 ptk_table[37][2];
	uint8 Track_Data[512][256];
	uint8 ptable[128];
	uint8 isize[31][2];
	uint8 PACK[31];
/*  uint8 DELTA[31];*/
	uint8 GLOBAL_DELTA = OFF;
	uint8 GLOBAL_PACK = OFF;
	long Track_Address[128][4];
	long Track_Data_Address = 0;
	long Sample_Data_Address = 0;
	long ssize = 0;
	long i = 0, j, k, l, a, b, z;
	long SampleSizes[31];
	long SampleAddresses[32];
	long Unpacked_Sample_Data_Size;
	// FILE *in,*out;

	if (Save_Status == BAD)
		return;

	bzero (Track_Address, 128 * 4 * 4);
	bzero (Track_Data, 512 << 8);
	bzero (ptable, 128);
	bzero (SampleSizes, 31 * 4);
	bzero (SampleAddresses, 32 * 4);
	bzero (isize, 31 * 2);
	for (i = 0; i < 31; i++) {
		PACK[i] = OFF;
/*    DELTA[i] = OFF;*/
	}

#include "ptktable.h"

	// in = fdopen (fd_in, "rb");
	// sprintf ( Depacked_OutName , "%ld.mod" , Cpt_Filename-1 );
	// out = fdopen (fd_out, "w+b");

	/* read sample data address */
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	Sample_Data_Address = (c1 << 8) + c2;

	/* read Real number of pattern */
	fread (&PatMax, 1, 1, in);

	/* read number of samples */
	fread (&Nbr_Sample, 1, 1, in);
	if ((Nbr_Sample & 0x80) == 0x80) {
		/*printf ( "Samples are saved as delta values !\n" ); */
		GLOBAL_DELTA = ON;
	}
	if ((Nbr_Sample & 0x40) == 0x40) {
		/*printf ( "some samples are packed !\n" ); */
		/*printf ( "\n! Since I could not understand the packing method of the\n" */
		/*         "! samples, neither could I do a depacker .. . mission ends here :)\n" ); */
		GLOBAL_PACK = ON;
		return;
	}
	Nbr_Sample &= 0x3F;

	/* read unpacked sample data size */
	if (GLOBAL_PACK == ON) {
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		fread (&c3, 1, 1, in);
		fread (&c4, 1, 1, in);
		Unpacked_Sample_Data_Size =
			(c1 << 24) + (c2 << 16) +
			(c3 << 8) + c4;
	}

	/* write title */
	tmp = (uint8 *) malloc (21);
	bzero (tmp, 21);
	fwrite (tmp, 20, 1, out);
	free (tmp);

	/* sample headers stuff */
	for (i = 0; i < Nbr_Sample; i++) {
		/* write sample name */
		c1 = 0x00;
		for (j = 0; j < 22; j++)
			fwrite (&c1, 1, 1, out);

		/* sample size */
		fread (&isize[i][0], 1, 1, in);
		fread (&isize[i][1], 1, 1, in);
		j = (isize[i][0] << 8) + isize[i][1];
		if (j > 0xFF00) {
			SampleSizes[i] = SampleSizes[0xFFFF - j];
			isize[i][0] = isize[0xFFFF - j][0];
			isize[i][1] = isize[0xFFFF - j][1];
/*fprintf ( debug , "!%2ld!" , 0xFFFF-j );*/
			SampleAddresses[i + 1] = SampleAddresses[0xFFFF - j + 1];	/* - SampleSizes[i]+SampleSizes[0xFFFF-j]; */
		} else {
			SampleAddresses[i + 1] =
				SampleAddresses[i] + SampleSizes[i - 1];
			SampleSizes[i] = j * 2;
			ssize += SampleSizes[i];
		}
		j = SampleSizes[i] / 2;
		fwrite (&isize[i][0], 1, 1, out);
		fwrite (&isize[i][1], 1, 1, out);

		/* finetune */
		fread (&c1, 1, 1, in);
		if ((c1 & 0x40) == 0x40)
			PACK[i] = ON;
		c1 &= 0x3F;
		fwrite (&c1, 1, 1, out);

		/* volume */
		fread (&c1, 1, 1, in);
		fwrite (&c1, 1, 1, out);

		/* loop start */
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
/*fprintf ( debug , "loop start : %2x, %2x " , c1,c2 );*/
		if ((c1 == 0xFF) && (c2 == 0xFF)) {
			c3 = 0x00;
			c4 = 0x01;
			fwrite (&c3, 1, 1, out);
			fwrite (&c3, 1, 1, out);
			fwrite (&c3, 1, 1, out);
			fwrite (&c4, 1, 1, out);
/*fprintf ( debug , " <--- no loop! (%2x,%2x)\n" ,c3,c4);*/
			continue;
		}
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
		l = j - ((c1 << 8) + c2);
/*fprintf ( debug , " -> size:%6ld  lstart:%5d  -> lsize:%ld\n" , j,c1*256+c2,l );*/

		/* WARNING !!! WORKS ONLY ON 80X86 computers ! */
		/* 68k machines code : c1 = *(tmp+2); */
		/* 68k machines code : c2 = *(tmp+3); */
		tmp = (uint8 *) & l;
		c1 = *(tmp + 1);
		c2 = *tmp;
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
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
	/*printf ( "Whole sample size : %ld\n" , ssize ); */

/*fprintf ( debug , "Where after sample headers : %x\n" , ftell ( in ) );*/

	/* read tracks addresses per pattern */
	for (i = 0; i < PatMax; i++) {
/*fprintf ( debug , "\npattern %ld : " , i );*/
		for (j = 0; j < 4; j++) {
			fread (&c1, 1, 1, in);
			fread (&c2, 1, 1, in);
			Track_Address[i][j] = (c1 << 8) + c2;
/*fprintf ( debug , "%6ld, " , Track_Address[i][j] );*/
		}
	}
/*fprintf ( debug , "\n\nwhere after the track addresses : %x\n\n" , ftell ( in ));*/


	/* pattern table */
/*fprintf ( debug , "\nPattern table :\n" );*/
	for (PatPos = 0; PatPos < 128; PatPos++) {
		fread (&c1, 1, 1, in);
		if (c1 == 0xFF)
			break;
		ptable[PatPos] = c1;	/* <--- /2 in p50a */
/*fprintf ( debug , "%2x, " , ptable[PatPos] );*/
	}

	/* write size of pattern list */
	fwrite (&PatPos, 1, 1, out);
/*fprintf ( debug , "\nsize of the pattern table : %d\n\n" , PatPos );*/

	/* write noisetracker byte */
	c1 = 0x7f;
	fwrite (&c1, 1, 1, out);

	/* write pattern table */
	fwrite (ptable, 128, 1, out);

	/* write ptk's ID */
	c1 = 'M';
	c2 = '.';
	c3 = 'K';
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);

/*fprintf ( debug , "\n\nbefore reading track data : %x\n" , ftell ( in ) );*/
	Track_Data_Address = ftell (in);

	/* rewrite the track data */

	/*printf ( "sorting and depacking tracks data ... " ); */
	for (i = 0; i < PatMax; i++) {
/*fprintf ( debug , "\n\npattern %ld\n" , i );*/
		Max = 63;
		for (j = 0; j < 4; j++) {
			fseek (in, Track_Address[i][j] + Track_Data_Address,
				0);
/*fprintf ( debug , "track %ld (at:%ld)\n" , j,ftell ( in ) );*/
			for (k = 0; k <= Max; k++) {
				fread (&c1, 1, 1, in);
/*fprintf ( debug , "%2ld: %2x, " , k , c1 );*/

				/* case no fxt nor fxtArg  (3 bytes) */
				if (((c1 & 0x70) == 0x70) && (c1 != 0xFF)
					&& (c1 != 0x7F)) {
					fread (&c2, 1, 1, in);
/*fprintf ( debug , "%2x, " , c2 );*/
					c6 =
						((c1 << 4) & 0xf0) | ((c2 >>
							   4) & 0x0e);
					Track_Data[i * 4 + j][k * 4] =
						(c2 & 0x10) | (ptk_table[c6 /
							2][0]);
					Track_Data[i * 4 + j][k * 4 + 1] =
						ptk_table[c6 / 2][1];
					Track_Data[i * 4 + j][k * 4 + 2] =
						((c2 << 4) & 0xf0);

					if ((c1 & 0x80) == 0x80) {
						fread (&c3, 1, 1, in);
						if (c3 < 0x80) {
/*fprintf ( debug , "%2x  <--- bypass %d rows !\n" , c3 , c3 );*/
							k += c3;
							continue;
						}
/*fprintf ( debug , "%2x  <--- repeat current row %d times\n" , c3 , c3-0x80 );*/
						c4 = c3 - 0x80;
						for (l = 0; l < c4; l++) {
							k += 1;
/*fprintf ( debug , "*%2ld: %2x, %2x\n" , k , c1,c2 );*/
							Track_Data[i * 4 +
								j][k * 4] =
								(c2 & 0x10) |
								(ptk_table[c6 /
									2]
								[0]);
							Track_Data[i * 4 +
								j][k * 4 +
								1] =
								ptk_table[c6 /
								2][1];
							Track_Data[i * 4 +
								j][k * 4 +
								2] =
								((c2 << 4) &
								0xf0);
						}
					}
/*fprintf ( debug , "\n" );*/
					continue;
				}
				/* end of case no fxt nor fxtArg */

				/* case no Sample number nor Relative not number */
				if (((c1 & 0x70) == 0x60) && (c1 != 0xFF)) {
					fread (&c2, 1, 1, in);
/*fprintf ( debug , "%2x, " , c2 );*/

					c6 = c1 & 0x0f;
					if (c6 == 0x08)
						c1 -= 0x08;
					Track_Data[i * 4 + j][k * 4 + 2] =
						(c1 & 0x0f);
					if ((c6 == 0x05) || (c6 == 0x06)
						|| (c6 == 0x0a))
						c2 =
							(c2 >
							0x7f) ? ((0x100 -
							     c2) << 4) : c2;
					Track_Data[i * 4 + j][k * 4 + 3] = c2;

					if (c6 == 0x0D) {
/*fprintf ( debug , " <-- PATTERN BREAK !, track ends\n" );*/
						Max = k;
						k = 9999l;
						continue;
					}
					if (c6 == 0x0B) {
/*fprintf ( debug , " <-- PATTERN JUMP !, track ends\n" );*/
						Max = k;
						k = 9999l;
						continue;
					}
					if ((c1 & 0x80) == 0x80) {
						fread (&c3, 1, 1, in);
						if (c3 < 0x80) {
/*fprintf ( debug , "%2x  <--- bypass %d rows !\n" , c3 , c3 );*/
							k += c3;
							continue;
						}
/*fprintf ( debug , "%2x  <--- repeat current row %d times\n" , c3 , c3-0x80 );*/
						c4 = c3 - 0x80;
						for (l = 0; l < c4; l++) {
							k += 1;
/*fprintf ( debug , "*%2ld: %2x, %2x\n" , k , c1,c2 );*/
							Track_Data[i * 4 +
								j][k * 4 +
								2] =
								(c1 & 0x0f);
							Track_Data[i * 4 +
								j][k * 4 +
								3] = c2;
						}
					}
/*fprintf ( debug , "\n" );*/
					continue;
				}
				/* end of case no Sample number nor Relative not number */

				if (((c1 & 0x80) == 0x80) && (c1 != 0xFF)) {
					fread (&c2, 1, 1, in);
					fread (&c3, 1, 1, in);
					fread (&c4, 1, 1, in);
/*fprintf ( debug , "%2x, %2x, " , c2,c3);*/
					c1 = c1 & 0x7F;
					Track_Data[i * 4 + j][k * 4] =
						((c1 << 4) & 0x10) | (ptk_table[c1
							/ 2][0]);
					Track_Data[i * 4 + j][k * 4 + 1] =
						ptk_table[c1 / 2][1];
					c6 = c2 & 0x0f;
					if (c6 == 0x08)
						c2 -= 0x08;
					Track_Data[i * 4 + j][k * 4 + 2] = c2;
					if ((c6 == 0x05) || (c6 == 0x06)
						|| (c6 == 0x0a))
						c3 =
							(c3 >
							0x7f) ? ((0x100 -
							     c3) << 4) : c3;
					Track_Data[i * 4 + j][k * 4 + 3] = c3;
					if (c6 == 0x0D) {
/*fprintf ( debug , " <-- PATTERN BREAK !, track ends\n" );*/
						Max = k;
						k = 9999l;
						continue;
					}
					if (c6 == 0x0B) {
/*fprintf ( debug , " <-- PATTERN JUMP !, track ends\n" );*/
						Max = k;
						k = 9999l;
						continue;
					}

					if (c4 < 0x80) {
/*fprintf ( debug , "%2x  <--- bypass %d rows !\n" , c4 , c4 );*/
						k += c4;
						continue;
					}
/*fprintf ( debug , "%2x  <--- repeat current row %d times\n" , c4 , c4-0x80 );*/
					c4 = c4 - 0x80;
					for (l = 0; l < c4; l++) {
						k += 1;
/*fprintf ( debug , "*%2ld: %2x, %2x, %2x\n" , k , c1,c2,c3 );*/
						Track_Data[i * 4 + j][k * 4] =
							((c1 << 4) & 0x10) |
							(ptk_table[c1 / 2][0]);
						Track_Data[i * 4 + j][k * 4 +
							1] = ptk_table[c1 / 2][1];
						c6 = c2 & 0x0f;
						if (c6 == 0x08)
							c2 -= 0x08;
						Track_Data[i * 4 + j][k * 4 +
							2] = c2;
						if ((c6 == 0x05)
							|| (c6 == 0x06)
							|| (c6 == 0x0a))
							c3 =
								(c3 >
								0x7f)
								? ((0x100 -
								  c3) << 4) :
								c3;
						Track_Data[i * 4 + j][k * 4 +
							3] = c3;
					}
					continue;
				}


				if ((c1 & 0x7F) == 0x7F) {
					if ((c1 & 0x80) == 0x00) {
/*fprintf ( debug , "  <--- bypass 1 row !\n" );*/
						/*k += 1; */
						continue;
					}
					fread (&c2, 1, 1, in);
					if (c2 < 0x40) {
/*fprintf ( debug , "%2x  <--- bypass %d rows !\n" , c2 , c2 );*/
						k += c2;
						continue;
					}
/*fprintf ( debug , "%2x, " , c2 );*/
					c2 -= 0x40;
					fread (&c3, 1, 1, in);
/*fprintf ( debug , "%2x, " , c3 );*/
					z = c3;
					if (c2 >= 0x80) {
						c2 -= 0x80;
						fread (&c4, 1, 1, in);
/*fprintf ( debug , "%2x, " , c4 );*/
						z = (c3 << 8) + c4;
					}
/*fprintf ( debug , " <--- repeat %2d lines some %ld bytes before\n" , c2,z );*/
					a = ftell (in);
					c5 = c2;
					fseek (in, -(z), 1);
					for (l = 0; (l <= c5) && (k <= Max);
						l++, k++) {
						fread (&c1, 1, 1, in);
/*fprintf ( debug , "#%2ld: %2x, " , l , c1 );*/

						/* case no fxt nor fxtArg  (3 bytes) */
						if (((c1 & 0x70) == 0x70)
							&& (c1 != 0xFF)
							&& (c1 != 0x7F)) {
							fread (&c2, 1, 1, in);
/*fprintf ( debug , "%2x, " , c2 );*/
							c6 =
								((c1 << 4) &
								0xf0) | ((c2
									>> 4)
								& 0x0e);
							Track_Data[i * 4 +
								j][k * 4] =
								(c2 & 0x10) |
								(ptk_table[c6 /
									2]
								[0]);
							Track_Data[i * 4 +
								j][k * 4 +
								1] =
								ptk_table[c6 /
								2][1];
							Track_Data[i * 4 +
								j][k * 4 +
								2] =
								((c2 << 4) &
								0xf0);

							if ((c1 & 0x80) ==
								0x80) {
								fread (&c3, 1,
									1,
									in);
								if (c3 < 0x80) {
/*fprintf ( debug , "%2x  <--- bypass %d rows !\n" , c3 , c3 );*/
									k +=
										c3;
									continue;
								}
/*fprintf ( debug , "%2x  <--- repeat current row %d times\n" , c3 , c3-0x80 );*/
								c4 =
									c3 -
									0x80;
								for (b = 0;
									b <
									c4;
									b++) {
									k +=
										1;
/*fprintf ( debug , "*%2ld: %2x, %2x\n" , k , c1,c2 );*/
									Track_Data
										[i
										*
										4
										+
										j]
										[k
										*
										4]
										=
										(c2
										&
										0x10)
										|
										(ptk_table
										[c6
											/
											2]
										[0]);
									Track_Data
										[i
										*
										4
										+
										j]
										[k
										*
										4
										+
										1]
										=
										ptk_table
										[c6
										/
										2]
										[1];
									Track_Data
										[i
										*
										4
										+
										j]
										[k
										*
										4
										+
										2]
										=
										(
										(c2
										       <<
											   4)
										&
										0xf0);
								}
							}
/*fprintf ( debug , "\n" );*/
							continue;
						}
						/* end of case no fxt nor fxtArg */

						/* case no Sample number nor Relative not number */
						if (((c1 & 0x60) == 0x60)
							&& (c1 != 0xFF)
							&& (c1 != 0x7F)) {
							fread (&c2, 1, 1, in);
/*fprintf ( debug , "%2x, " , c2 );*/
							c6 = c1 & 0x0f;
							if (c6 == 0x08)
								c1 -= 0x08;
							Track_Data[i * 4 +
								j][k * 4 +
								2] =
								(c1 & 0x0f);
							if ((c6 == 0x05)
								|| (c6 ==
									0x06)
								|| (c6 ==
									0x0a))
									c2 =
									(c2 >
									0x7f)
									? (
									(0x100
										-
										c2)
									<< 4)
									: c2;
							Track_Data[i * 4 +
								j][k * 4 +
								3] = c2;

							if (c6 == 0x0D) {
/*fprintf ( debug , " <-- PATTERN BREAK !, track ends\n" );*/
								Max = k;
								k = 9999l;
								continue;
							}
							if (c6 == 0x0B) {
/*fprintf ( debug , " <-- PATTERN JUMP !, track ends\n" );*/
								Max = k;
								k = 9999l;
								continue;
							}

							if ((c1 & 0x80) ==
								0x80) {
								fread (&c3, 1,
									1,
									in);
								if (c3 < 0x80) {
/*fprintf ( debug , "%2x  <--- bypass %d rows !\n" , c3 , c3 );*/
									k +=
										c3;
									continue;
								}
/*fprintf ( debug , "%2x  <--- repeat current row %d times\n" , c3 , c3-0x80 );*/
								c4 =
									c3 -
									0x80;
								for (b = 0;
									b <
									c4;
									b++) {
									k +=
										1;
/*fprintf ( debug , "*%2ld: %2x, %2x\n" , k , c1,c2 );*/
									Track_Data
										[i
										*
										4
										+
										j]
										[k
										*
										4
										+
										2]
										=
										(c1
										&
										0x0f);
									Track_Data
										[i
										*
										4
										+
										j]
										[k
										*
										4
										+
										3]
										=
										c2;
								}
							}
/*fprintf ( debug , "\n" );*/
							continue;
						}
						/* end of case no Sample number nor Relative not number */

						if (((c1 & 0x80) == 0x80)
							&& (c1 != 0xFF)
							&& (c1 != 0x7F)) {
							fread (&c2, 1, 1, in);
							fread (&c3, 1, 1, in);
							fread (&c4, 1, 1, in);
/*fprintf ( debug , "%2x, %2x, " , c2,c3);*/
							c1 = c1 & 0x7f;
							Track_Data[i * 4 +
								j][k * 4] =
								((c1 << 4) &
								0x10) |
								(ptk_table[c1 /
									2]
								[0]);
							Track_Data[i * 4 +
								j][k * 4 +
								1] =
								ptk_table[c1 /
								2][1];
							c6 = c2 & 0x0f;
							if (c6 == 0x08)
								c2 -= 0x08;
							Track_Data[i * 4 +
								j][k * 4 +
								2] = c2;
							if ((c6 == 0x05)
								|| (c6 ==
									0x06)
								|| (c6 ==
									0x0a))
									c3 =
									(c3 >
									0x7f)
									? (
									(0x100
										-
										c3)
									<< 4)
									: c3;
							Track_Data[i * 4 +
								j][k * 4 +
								3] = c3;
							if (c6 == 0x0D) {
								Max = k;
								k = l = 9999l;
/*fprintf ( debug , " <-- PATTERN BREAK !, track ends\n" );*/
								continue;
							}
							if (c6 == 0x0B) {
								Max = k;
								k = l = 9999l;
/*fprintf ( debug , " <-- PATTERN JUMP !, track ends\n" );*/
								continue;
							}

							if (c4 < 0x80) {
/*fprintf ( debug , "%2x  <--- bypass %d rows !\n" , c4 , c4 );*/
								/*l += c4; */
								k += c4;
								continue;
							}
/*fprintf ( debug , "%2x  <--- repeat current row %d times\n" , c4 , c4-0x80 );*/
							c4 = c4 - 0x80;
							/*l += (c4-1); */
							for (b = 0; b < c4;
								b++) {
								k += 1;
/*fprintf ( debug , "*%2ld: %2x, %2x, %2x\n" , k , c1,c2,c3 );*/
								Track_Data[i *
									4 +
									j][k *
									4] =
									((c1
										<<
										4)
									&
									0x10)
									|
									(ptk_table
									[c1 /
										2]
									[0]);
								Track_Data[i *
									4 +
									j][k *
									4 +
									1] =
									ptk_table
									[c1 /
									2][1];
								c6 =
									c2 &
									0x0f;
								if (c6 ==
									0x08)
										c2
										-=
										0x08;
								Track_Data[i *
									4 +
									j][k *
									4 +
									2] =
									c2;
								if ((c6 ==
										0x05)
									|| (c6
										==
										0x06)
									|| (c6
										==
										0x0a))
										c3
										=
										(c3
										>
										0x7f)
										?
										(
										(0x100
										       -
											c3)
										<<
										4)
										:
										c3;
								Track_Data[i *
									4 +
									j][k *
									4 +
									3] =
									c3;
							}
							continue;
						}
						if ((c1 & 0x7F) == 0x7F) {
							if ((c1 & 0x80) ==
								0x00) {
/*fprintf ( debug , "  <--- bypass 1 row !\n" );*/
								/*k += 1; */
								continue;
							}
							fread (&c2, 1, 1, in);
							if (c2 < 0x40) {
/*fprintf ( debug , "%2x  <--- bypass %d rows !\n" , c2 , c2 );*/
								k += c2;
								continue;
							}
							continue;
						}

						fread (&c2, 1, 1, in);
						fread (&c3, 1, 1, in);
/*fprintf ( debug , "%2x, %2x" , c2,c3 );*/
						Track_Data[i * 4 + j][k * 4] =
							((c1 << 4) & 0x10) |
							(ptk_table[c1 / 2][0]);
						Track_Data[i * 4 + j][k * 4 +
							1] = ptk_table[c1 / 2][1];
						c6 = c2 & 0x0f;
						if (c6 == 0x08)
							c2 -= 0x08;
						Track_Data[i * 4 + j][k * 4 +
							2] = c2;
						if ((c6 == 0x05)
							|| (c6 == 0x06)
							|| (c6 == 0x0a))
							c3 =
								(c3 >
								0x7f)
								? ((0x100 -
								  c3) << 4) :
								c3;
						Track_Data[i * 4 + j][k * 4 +
							3] = c3;
/*fprintf ( debug , "\n" );*/
					}
					fseek (in, a, 0);
/*fprintf ( debug , "\n" );*/
					k -= 1;
					continue;
				}


				fread (&c2, 1, 1, in);
				fread (&c3, 1, 1, in);
/*fprintf ( debug , "%2x, %2x" , c2,c3 );*/
				Track_Data[i * 4 + j][k * 4] =
					((c1 << 4) & 0x10) | (ptk_table[c1 /
						2][0]);
				Track_Data[i * 4 + j][k * 4 + 1] =
					ptk_table[c1 / 2][1];
				c6 = c2 & 0x0f;
				if (c6 == 0x08)
					c2 -= 0x08;
				Track_Data[i * 4 + j][k * 4 + 2] = c2;
				if ((c6 == 0x05) || (c6 == 0x06)
					|| (c6 == 0x0a)) c3 =
						(c3 >
						0x7f) ? ((0x100 -
						  c3) << 4) : c3;
				Track_Data[i * 4 + j][k * 4 + 3] = c3;
				if (c6 == 0x0D) {
/*fprintf ( debug , " <-- PATTERN BREAK !, track ends\n" );*/
					Max = k;
					k = 9999l;
					continue;
				}
				if (c6 == 0x0B) {
/*fprintf ( debug , " <-- PATTERN JUMP !, track ends\n" );*/
					Max = k;
					k = 9999l;
					continue;
				}

/*fprintf ( debug , "\n" );*/
			}
		}
	}
	/*printf ( "ok\n" ); */

	/* write pattern data */

	/*printf ( "writing pattern data ... " ); */
	tmp = (uint8 *) malloc (1024);
	for (i = 0; i < PatMax; i++) {
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

	fseek (in, Sample_Data_Address, 0);

	/* read and write sample data */

	/*printf ( "writing sample data ... " ); */
/*fprintf ( debug , "\n\nSample shit:\n" );*/
	for (i = 0; i < Nbr_Sample; i++) {
		fseek (in, Sample_Data_Address + SampleAddresses[i + 1], 0);
/*fprintf ( debug , "%2ld: read %-6ld at %ld\n" , i , SampleSizes[i] , ftell ( in ));*/
		insDataWork = (signed char *) malloc (SampleSizes[i]);
		bzero (insDataWork, SampleSizes[i]);
		fread (insDataWork, SampleSizes[i], 1, in);
		if (GLOBAL_DELTA == ON) {
			c1 = 0x00;
			for (j = 1; j < SampleSizes[i]; j++) {
				c2 = insDataWork[j];
				c2 = 0x100 - c2;
				c3 = c2 + c1;
				insDataWork[j] = c3;
				c1 = c3;
			}
		}
		fwrite (insDataWork, SampleSizes[i], 1, out);
		free (insDataWork);
	}
	/*printf ( "ok\n" ); */

	if (GLOBAL_DELTA == ON) {
		Crap ("P61A:The Player 6.1A", GOOD, BAD, out);
		/*
		   fseek ( out , 770 , SEEK_SET );
		   fprintf ( out , "[! Delta samples   ]" );
		 */
	}

	Crap ("P61A:The Player 6.1A", BAD, BAD, out);

	fflush (in);
	fflush (out);

	printf ("done\n");
	return;			/* useless ... but */
}


void testP61A_nopack (void)
{
	if (i < 7) {
		Test = BAD;
		return;
	}
	start = i - 7;

	/* number of pattern (real) */
	m = data[start + 2];
	if ((m > 0x7f) || (m == 0)) {
/*printf ( "#1 Start:%ld\n" , start );*/
		Test = BAD;
		return;
	}
	/* m is the real number of pattern */

	/* number of sample */
	k = (data[start + 3] & 0x3F);
	if ((k > 0x1F) || (k == 0)) {
/*printf ( "#2 Start:%ld\n" , start );*/
		Test = BAD;
		return;
	}
	/* k is the number of sample */

	/* test volumes */
	for (l = 0; l < k; l++) {
		if (data[start + 7 + l * 6] > 0x40) {
/*printf ( "#3 Start:%ld\n" , start );*/
			Test = BAD;
			return;
		}
	}

	/* test fines */
	for (l = 0; l < k; l++) {
		if (data[start + 6 + l * 6] > 0x0F) {
			Test = BAD;
/*printf ( "#4 Start:%ld\n" , start );*/
			return;
		}
	}

	/* test sample sizes and loop start */
	ssize = 0;
	for (n = 0; n < k; n++) {
		o = ((data[start + 4 + n * 6] << 8) +
			data[start + 5 + n * 6]);
		if (((o < 0xFFDF) && (o > 0x8000)) || (o == 0)) {
/*printf ( "#5 Start:%ld\n" , start );*/
			Test = BAD;
			return;
		}
		if (o < 0xFF00)
			ssize += (o * 2);

		j = ((data[start + 8 + n * 6] << 8) +
			data[start + 9 + n * 6]);
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
	j =
		(data[start] << 8) + data[start +
		1];
	if (j < (k * 6 + 4 + m * 8)) {
/*printf ( "#6 Start:%ld\n" , start );*/
		Test = BAD;
		ssize = 0;
		return;
	}
	/* j is the address of the sample data */


	/* test track table */
	for (l = 0; l < (m * 4); l++) {
		o =
			((data[start + 4 + k * 6 +
					 l * 2] << 8) +
			data[start + 4 + k * 6 + l * 2 +
				1]);
		if ((o + k * 6 + 4 + m * 8) > j) {
/*printf ( "#7 Start:%ld (value:%ld)(where:%x)(l:%ld)(m:%ld)(o:%ld)\n"
, start
, (data[start+k*6+4+l*2]*256)+data[start+4+k*6+l*2+1]
, start+k*6+4+l*2
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
	if ((start + k * 6 + 4 + m * 8) > in_size) {
/*printf ( "8,0 Start:%ld\n" , start );*/
		Test = BAD;
		return;
	}
	while ((data[start + k * 6 + 4 + m * 8 + l] !=
			0xFF) && (l < 128)) {
		if (data[start + k * 6 + 4 + m * 8 +
				l] > (m - 1)) {
/*printf ( "#8,1 Start:%ld (value:%ld)(where:%x)(l:%ld)(m:%ld)(k:%ld)\n"
, start
, data[start+k*6+4+m*8+l]
, start+k*6+4+m*8+l
, l
, m
, k );*/
			Test = BAD;
			ssize = 0;
			return;
		}
		if (data[start + k * 6 + 4 + m * 8 +
				l] > o)
			o =
				data[start + k * 6 + 4 +
				m * 8 + l];
		l++;
	}
	/* are we beside the sample data address ? */
	if ((k * 6 + 4 + m * 8 + l) > j) {
		Test = BAD;
		return;
	}
	if ((l == 0) || (l == 128)) {
/*printf ( "#8.2 Start:%ld\n" , start );*/
		Test = BAD;
		return;
	}
	o += 1;
	/* o is the highest number of pattern */


	/* test notes ... pfiew */
	l += 1;
	for (n = (k * 6 + 4 + m * 8 + l); n < j; n++) {
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
/*printf ( "#9,2 Start:%ld (value:%ld) (where:%x) (n:%ld) (j:%ld)\n"
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
/*printf ( "#9,3 Start:%ld (value:%ld) (where:%x) (n:%ld) (j:%ld)\n"
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
/*printf ( "#9,4 Start:%ld (value:%ld) (where:%x) (n:%ld) (j:%ld)\n"
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
	j =
		(data[start] << 8) + data[start +
		1];
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
