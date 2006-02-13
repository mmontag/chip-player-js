/*
 * Pro-Wizard_1.c
 *
 * Copyright (C) 1997-1999 Sylvain "Asle" Chipaux
 * Modified by Claudio Matsuoka for xmp
 *
 * $Id: prowiz.c,v 1.4 2006-02-13 00:21:46 cmatsuoka Exp $
 */
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "xmp.h"
#if 0
#include "global.h"
#include "component.h"
#endif
#include "prowiz.h"

static int check (unsigned char *, int);

#if 0
static int decrunch (int, int);

static struct _xmp_depacker depacker_pw = {
	"Pro-Wizard v1.45c1 depacker",
	"Copyright (C) 1997-1999 Sylvain Chipaux",
	"prowiz",
	0x0100,
	XMP_DEPACKER_LAST,
	check,
	decrunch
};
#endif

LIST_HEAD(format_list);


int pw_register (struct pw_format *f)
{
	list_add_tail (&f->list, &format_list);
	return 0;
}

int pw_unregister (struct pw_format *f)
{
	list_del (&f->list);
	return 0;
}

int pw_init (int i)
{
	/* char *o; */

	if (!i)
		return 0;

	/* With signature */
	pw_register (&pw_ac1d);
	/* pw_register (&pw_emod); */
	pw_register (&pw_fchs);
	pw_register (&pw_fcm);
	pw_register (&pw_fuzz);
	pw_register (&pw_kris);
	pw_register (&pw_ksm);
	pw_register (&pw_mp_id);
	pw_register (&pw_p18a);
	pw_register (&pw_p10c);
	pw_register (&pw_pru1);
	pw_register (&pw_pru2);
	pw_register (&pw_pha);
	pw_register (&pw_stim);
	pw_register (&pw_wn);
	pw_register (&pw_unic_id);

	/* No signature */
	/* pw_register (&pw_xann); */
	pw_register (&pw_mp_noid);	/* Must check before Heatseeker */
	pw_register (&pw_di);
	pw_register (&pw_eu);
	pw_register (&pw_p60a);
	pw_register (&pw_np2);
	pw_register (&pw_np1);
	pw_register (&pw_np3);
	pw_register (&pw_zen);
	pw_register (&pw_unic_emptyid);
	pw_register (&pw_unic_noid);
	pw_register (&pw_unic2);
	pw_register (&pw_crb);
	pw_register (&pw_tdd);

#if 0
	while ((o = _xmp_parse (XMP_DEPACKER, (xmp_component *)&depacker_pw,
		"disable")) != NULL) {
		struct list_head *tmp;
		list_for_each(tmp, &format_list) {
			struct pw_format *format;
			format = list_entry(tmp, struct pw_format, list);
			if (!strcmp (format->id, o)) {
				pw_unregister (format);
				break;
			}
		}
	}
#endif


	return 0;
}

static struct list_head *checked_format = &format_list;

int pw_wizardry (int in, int out)
{
	struct list_head *tmp;
	struct pw_format *format;
	struct stat st;
	int size, in_size;
	uint8 *data;
	FILE *file_in, *file_out;

	file_in = fdopen (in, "rb");
	if (file_in == NULL)
		return -1;

	file_out = fdopen (out, "w+b");

	if (fstat (fileno (file_in), &st) < 0)
		in_size = -1;
	else
		in_size = st.st_size;

	/* printf ("input file size : %d\n", in_size); */
	if (in_size < MIN_FILE_LENGHT)
		return -2;

	/* alloc mem */
	data = (uint8 *) malloc (in_size + 4096);	/* slack added */
	if (data == NULL) {
		perror ("Couldn't allocate memory");
		return -1;
	}
	fread (data, in_size, 1, file_in);


  /********************************************************************/
  /**************************   SEARCH   ******************************/
  /********************************************************************/

	if (checked_format != &format_list)
		goto checked;

	list_for_each(tmp, &format_list) {
		format = list_entry(tmp, struct pw_format, list);
		_D ("checking format: %s", format->name);
		if (format->test (data, in_size) >= 0) {
			fseek (file_in, 0, SEEK_SET);
			if (format->depackb)
				size = format->depackb (data, file_out);
			else if (format->depackf) 
				size = format->depackf (file_in, file_out);

			if (size < 0)
				return -1;
			format->flags |= PW_MARK;
			pw_crap(format, file_out);
			fflush (file_out);
			break;
		}
	}
	goto done;

checked:
	format = list_entry(checked_format, struct pw_format, list);
	_D (_D_WARN "checked format: %s", format->name);
	checked_format = &format_list;

done:
	fseek (file_in, 0, SEEK_SET);
	size = -1;	/* paranoia setting */
	if (format->depackb)
		size = format->depackb (data, file_out);
	else if (format->depackf) 
		size = format->depackf (file_in, file_out);

	if (size < 0)
		return -1;

	format->flags |= PW_MARK;
	pw_crap(format, file_out);
	fflush (file_out);

	/*
	 * ADD: Rip based on size
	 */

	free (data);

	return 0;

	/****** end ******/

#if 0
	/*for (i = 0; i < (in_size - MINIMAL_FILE_LENGHT); i += 1) {*/

	for (i = 0; i < 1024; i++) {

    	/*******************************************************************/
	/* ok ... no the real job start here. So, we look for ID or Volume */
	/* values. A simple switch case follows .. based on Hex values of, */
	/* as foretold, ID or volume (not all file have ID ... few file_in fact */
	/* do ..).                                                         */
	/*******************************************************************/


		if (data[i] > 0x40)
			goto part2;

		/*
		 * first, let's take care of the formats with
		 * 'ID' value < 0x40
		 */
		/* "!PM!" : ID of Power Music */
		if (testPM () != BAD) {
			Depack_PM (file_in, file_out);
			continue;
		}

#if 0
		/* XANN packer */
		if (testXANN (i, data) != BAD) {
			Depack_XANN (file_in, file_out);
			continue;
		}

		/*
		 * hum ... that's where thfile_ings become interresting :)
		 * Module Protector withfile_out ID
		 * LEAVE IT THERE !!! ... at least before Heatseeker
		 * format sfile_ince they are VERY similar!
		 */
		testMP_noID ();
		if (Test != BAD) {
			// Rip_MP_noID ();
			Depack_MP (file_in, file_out);
			continue;
		}

		/* Digital Illusion */
		testDI ();
		if (Test != BAD) {
			// Rip_DI ();
			Depack_DI (file_in, file_out);
			continue;
		}

		/* eureka packer */
		testEUREKA ();
		if (Test != BAD) {
			// Rip_EUREKA ();
			Depack_EUREKA (file_in, file_out);
			continue;
		}
#endif

		/* The player 5.0a ? */
		testP50A ();
		if (Test != BAD) {
			// Rip_P50A ();
			Depack_P50A (file_in, file_out);
			continue;
		}

		/* The player 6.0a ? */
		testP60A_nopack ();
		if (Test != BAD) {
			// Rip_P60A ();
			Depack_P60A (file_in, file_out);
			continue;
		}

		/* The player 6.1a ? */
		testP61A_nopack ();
		if (Test != BAD) {
			Depack_P61A (file_in, file_out);
			continue;
		}

		/* Propacker 1.0 */
		testPP10 ();
		if (Test != BAD) {
			Depack_PP10 (file_in, file_out);
			continue;
		}

#if 0
		/* Noise Packer v2 */
		/* LEAVE VERSION 2 BEFORE VERSION 1 !!!!! */

		testNoisepacker2 ();
		if (Test != BAD) {
			Depack_Noisepacker2 (file_in, file_out);
			continue;
		}

		/* Noise Packer v1 */
		testNoisepacker1 ();
		if (Test != BAD) {
			Depack_Noisepacker1 (file_in, file_out);
			continue;
		}

		/* Noise Packer v3 */
		testNoisepacker3 ();
		if (Test != BAD) {
			Depack_Noisepacker3 (file_in, file_out);
			continue;
		}
#endif

		/* Promizer 0.1 */
		testPM01 ();
		if (Test != BAD) {
			Depack_PM01 (file_in, file_out);
			continue;
		}

		/* ProPacker 2.1 */
		testPP21 ();
		if (Test != BAD) {
			Depack_PP21 (file_in, file_out);
			continue;
		}

		/* ProPacker 3.0 */
		testPP30 ();
		if (Test != BAD) {
			Depack_PP30 (file_in, file_out);
			continue;
		}

		/* StartTrekker pack */
		testSTARPACK ();
		if (Test != BAD) {
			Depack_STARPACK (file_in, file_out);
			continue;
		}

#if 0
		/* Zen packer */
		testZEN ();
		if (Test != BAD) {
			Depack_ZEN (file_in, file_out);
			continue;
		}

		/* Unic tracker v1 ? */
		testUNIC_withemptyID ();
		if (Test != BAD) {
			Depack_UNIC (file_in, file_out);
			continue;
		}

		/* Unic tracker v1 ? */
		testUNIC_noID ();
		if (Test != BAD) {
			Depack_UNIC (file_in, file_out);
			continue;
		}

		/* Unic trecker v2 ? */
		testUNIC2 ();
		if (Test != BAD) {
			Depack_UNIC2 (file_in, file_out);
			continue;
		}
#endif

		/* Game Music Creator ? */
		testGMC ();
		if (Test != BAD) {
			Depack_GMC (file_in, file_out);
			continue;
		}

#if 0
		/* Heatseeker ? */
		testHEATSEEKER ();
		if (Test != BAD) {
			Depack_HEATSEEKER (file_in, file_out);
			continue;
		}
#endif

		/* SoundTracker (15 smp) */
		testSoundTracker ();
		if (Test != BAD) {
			// Rip_SoundTracker ();
			continue;
		}

#if 0
		/* The Dark Demon (group name) format */
		testTheDarkDemon ();
		if (Test != BAD) {
			Depack_TheDarkDemon (file_in, file_out);
			continue;
		}
#endif


		continue;
part2:
		/**********************************/
		/* ok, now, the files with ID ... */
		/**********************************/
		switch (data[i]) {
		case 'C':	/* 0x43 */
			/* CPLX_TP3 ?!? */
			if ((data[i + 1] == 'P') &&
				(data[i + 2] == 'L') &&
				(data[i + 3] == 'X') &&
				(data[i + 4] == '_') &&
				(data[i + 5] == 'T') &&
				(data[i + 6] == 'P') &&
				(data[i + 7] == '3')) {
				testTP3 ();
				if (Test == BAD)
					break;
				// Rip_TP3 ();
				Depack_TP3 (file_in, file_out);
				break;
			}
			break;

		case 'E':	/* 0x45 */
			/* "EMOD" : ID of Quadra Composer */
			if ((data[i + 1] == 'M') &&
				(data[i + 2] == 'O') &&
				(data[i + 3] == 'D')) {
				testQuadraComposer ();
				if (Test == BAD)
					break;
				Depack_QuadraComposer (file_in, file_out);
				break;
			}
			break;

		case 'F':	/* 0x46 */
			/* "FC-M" : ID of FC-M packer */
			if ((data[i + 1] == 'C') &&
				(data[i + 2] == '-') &&
				(data[i + 3] == 'M')) {
				testFC_M ();
				if (Test == BAD)
					break;
				Depack_FC_M (file_in, file_out);
				break;
			}
			break;

		case 'H':	/* 0x48 */
			/* "HRT!" : ID of Hornet packer */
			if ((data[i + 1] == 'R') &&
				(data[i + 2] == 'T') &&
				(data[i + 3] == '!')) {
				testHRT ();
				if (Test == BAD)
					break;
				// Rip_HRT ();
				Depack_HRT (file_in, file_out);
				break;
			}
			break;

		case 'K':	/* 0x4B */
			/* "KRIS" : ID of Chip Tracker */
			if ((data[i + 1] == 'R') &&
				(data[i + 2] == 'I') &&
				(data[i + 3] == 'S')) {
				testKRIS ();
				if (Test == BAD)
					break;
				// Rip_KRIS ();
				Depack_KRIS (file_in, file_out);
				break;
			}
			break;

		case 'M':	/* 0x4D */
			if ((data[i + 1] == '.') &&
				(data[i + 2] == 'K') &&
				(data[i + 3] == '.')) {
				/* protracker ? */
				testPTK ();
				if (Test != BAD) {
					// Rip_PTK ();
					break;
				}

				/* Unic tracker v1 ? */
				testUNIC_withID ();
				if (Test != BAD) {
					// Rip_UNIC_withID ();
					Depack_UNIC (file_in, file_out);
					break;
				}

				/* Noiserunner ? */
				testNoiserunner ();
				if (Test != BAD) {
					// Rip_Noiserunner ();
					Depack_Noiserunner (file_in, file_out);
					break;
				}
			}

			if ((data[i + 1] == '1') &&
				(data[i + 2] == '.') &&
				(data[i + 3] == '0')) {
				/* Fuzzac packer */
				testFUZZAC ();
				if (Test != BAD) {
					// Rip_Fuzzac ();
					Depack_Fuzzac (file_in, file_out);
					break;
				}
			}

			if ((data[i + 1] == 'E') &&
				(data[i + 2] == 'X') &&
				(data[i + 3] == 'X')) {
				/* tracker packer v2 */
				testTP2 ();
				if (Test != BAD) {
					// Rip_TP2 ();
					Depack_TP2 (file_in, file_out);
					break;
				}
				/* tracker packer v1 */
				testTP1 ();
				if (Test != BAD) {
					// Rip_TP1 ();
					Depack_TP1 (file_in, file_out);
					break;
				}
			}

#if 0
			if (data[i + 1] == '.') {
				/* Kefrens sound machine ? */
				testKSM ();
				if (Test != BAD) {
					// Rip_KSM ();
					Depack_KSM (file_in, file_out);
					break;
				}
			}
#endif
			break;

		case 'P':	/* 0x50 */
			/* "P40A" : ID of The Player */
			if ((data[i + 1] == '4') &&
				(data[i + 2] == '0') &&
				(data[i + 3] == 'A')) {
				testP40A ();
				if (Test == BAD)
					break;
				Depack_P40 (file_in, file_out);
			}

			/* "P40B" : ID of The Player */
			if ((data[i + 1] == '4') &&
				(data[i + 2] == '0') &&
				(data[i + 3] == 'B')) {
				testP40A ();
				if (Test == BAD)
					break;
				Depack_P40 (file_in, file_out);
			}

			/* "P41A" : ID of The Player */
			if ((data[i + 1] == '4') &&
				(data[i + 2] == '1') &&
				(data[i + 3] == 'A')) {
				testP41A ();
				if (Test == BAD)
					break;
				Depack_P41A (file_in, file_out);
				break;
			}

			/* "PM40" : ID of Promizer 4 */
			if ((data[i + 1] == 'M') &&
				(data[i + 2] == '4') &&
				(data[i + 3] == '0')) {
				testPM40 ();
				if (Test == BAD)
					break;
				Depack_PM40 (file_in, file_out);
				break;
			}
			break;

		case 'S':	/* 0x53 */
			/* "SNT!" ProRunner 2 */
			if ((data[i + 1] == 'N') &&
				(data[i + 2] == 'T') &&
				(data[i + 3] == '!')) {
				testPRUN2 ();
				if (Test == BAD)
					break;
				// Rip_PRUN2 ();
				Depack_PRUN2 (file_in, file_out);
				break;
			}
			/* "SNT." ProRunner 1 */
			if ((data[i + 1] == 'N') &&
				(data[i + 2] == 'T') &&
				(data[i + 3] == '.')) {
				testPRUN1 ();
				if (Test == BAD)
					break;
				// Rip_PRUN1 ();
				Depack_PRUN1 (file_in, file_out);
				break;
			}

			/* SKYT packer */
			if ((data[i + 1] == 'K') &&
				(data[i + 2] == 'Y') &&
				(data[i + 3] == 'T')) {
				testSKYT ();
				if (Test == BAD)
					break;
				// Rip_SKYT ();
				Depack_SKYT (file_in, file_out);
				break;
			}

#if 0
			/* STIM Slamtilt */
			if ((data[i + 1] == 'T') &&
				(data[i + 2] == 'I') &&
				(data[i + 3] == 'M')) {
				testSTIM ();
				if (Test != BAD) {
					// Rip_STIM ();
					Depack_STIM (file_in, file_out);
					break;
				}
			}

			/* SONG Fuchs Tracker */
			if ((data[i + 1] == 'O') &&
				(data[i + 2] == 'N') &&
				(data[i + 3] == 'G')) {
				testFuchsTracker ();
				if (Test != BAD) {
					// Rip_FuchsTracker ();
					Depack_FuchsTracker (file_in, file_out);
					break;
				}
			}
#endif
			break;

		case 'T':
#if 0
			/* "TRK1" Module Protector */
			if ((data[i + 1] == 'R') &&
				(data[i + 2] == 'K') &&
				(data[i + 3] == '1')) {
				/* Module Protector */
				testMP_withID ();
				if (Test == BAD)
					break;
				// Rip_MP_withID ();
				Depack_MP (file_in, file_out);
				break;
			}
#endif
			break;

		case 'W':	/* 0x57 */
			/* "WN" Wanton Packer */
			if ((data[i + 1] == 'N') &&
				(data[i + 2] == 0x00)) {
				testWN ();
				if (Test == BAD)
					break;
				// Rip_WN ();
				Depack_WN (file_in, file_out);
				break;
			}
			break;

		case 0x60:
			/* promizer 1.8a ? */
			if ((data[i + 1] == 0x38) &&
				(data[i + 2] == 0x60) &&
				(data[i + 3] == 0x00) &&
				(data[i + 4] == 0x00) &&
				(data[i + 5] == 0xa0) &&
				(data[i + 6] == 0x60) &&
				(data[i + 7] == 0x00) &&
				(data[i + 8] == 0x01) &&
				(data[i + 9] == 0x3e) &&
				(data[i + 10] == 0x60) &&
				(data[i + 11] == 0x00) &&
				(data[i + 12] == 0x01) &&
				(data[i + 13] == 0x0c) &&
				(data[i + 14] == 0x48) &&
				(data[i + 15] == 0xe7)) {	/* gosh !, should be enough :) */
				testPMZ ();
				if (Test != BAD) {
					// Rip_PM18a ();
					Depack_PM18a (file_in, file_out);
					break;
				}

				/* Promizer 1.0c */
				testPM10c ();
				if (Test != BAD) {
					// Rip_PM10c ();
					Depack_PM10c (file_in, file_out);
					break;
				}
				break;
			}

			/* promizer 2.0 ? */
			if ((data[i + 1] == 0x00) &&
				(data[i + 2] == 0x00) &&
				(data[i + 3] == 0x16) &&
				(data[i + 4] == 0x60) &&
				(data[i + 5] == 0x00) &&
				(data[i + 6] == 0x01) &&
				(data[i + 7] == 0x40) &&
				(data[i + 8] == 0x60) &&
				(data[i + 9] == 0x00) &&
				(data[i + 10] == 0x00) &&
				(data[i + 11] == 0xf0) &&
				(data[i + 12] == 0x3f) &&
				(data[i + 13] == 0x00) &&
				(data[i + 14] == 0x10) &&
				(data[i + 15] == 0x3a)) {	/* gosh !, should be enough :) */
				testPM2 ();
				if (Test == BAD)
					break;
				// Rip_PM20 ();
				Depack_PM20 (file_in, file_out);
				break;
			}
			break;

#if 0
		case 0xAC:
			/* AC1D packer ?!? */
			if (data[i + 1] == 0x1D) {
				testAC1D ();
				if (Test != BAD) {
					// Rip_AC1D ();
					Depack_AC1D (file_in, file_out);
					break;
				}
			}
			break;
#endif

		case 0xC0:
			/* Pha Packer */
			if ((i >= 1) && (data[i - 1] == 0x03)) {
				testPHA ();
				if (Test != BAD) {
					// Rip_PHA ();
					Depack_PHA (file_in, file_out);
					break;
				}
			}
			break;

		default:	/* do nothfile_ing ... save continuing :) */
			break;

		}		/* end of switch */

	}

	free (data);

	return 0;
#endif
}


/* writfile_ing craps in converted MODs */
void pw_crap (struct pw_format *f, FILE *file_out)
{
	int i, p, l;
	char buf[40];

#define add_crap(offset,msg...) do {	\
  char _b[40];				\
  fseek (file_out, offset, SEEK_SET);	\
  memset (_b, 0, 40);			\
  sprintf (_b, "%s", msg);		\
  fwrite (_b, 1, 22, file_out); } while (0)

	_D ("packer: %s", f->name);
	i = ftell (file_out);

	if (f->flags & PW_MARK) {
		fseek (file_out, 0x438, SEEK_SET);
		fwrite ("PWIZ", 1, 4, file_out);
		fseek (file_out, 0, SEEK_END);
  		sprintf (buf, "%-8.8s%-.22s", f->id, f->name);
		for (i = 0; i < 8; i++) {
			if (buf[i] == ' ')
				buf[i] = 0;
		}
		fwrite (buf, 1, 30, file_out);
	}

	add_crap (560, "[   Converted with   ]");
	add_crap (590, "[ ProWizard for UNIX ]");
	add_crap (620, "[  written by Asle!  ]");
	add_crap (650, "[        ----        ]");
	add_crap (680, "[  Original format:  ]");
	add_crap (710, "[                    ]");

	l = strlen (f->name);
	if (l > 20)
		l = 20;
	p = (22 - l) / 2;
	if (p < 0)
		p = 0;
	fseek (file_out, 710 + p, SEEK_SET);
	fwrite (f->name, 1, l, file_out);

#if 0	
	if (f->flags & PW_DELTA)
		add_crap (770, "[    Delta samples   ]");

	if (f->flags & PW_PACKED)
		add_crap (800, "[   Packed samples   ]");
#endif

	fseek (file_out, i, SEEK_SET);
}


/*
 * xmp plugfile_in API
 */

static struct list_head *shortcut = &format_list;
static int check (unsigned char *b, int s)
{
	struct list_head *tmp;
	struct pw_format *format;
	int extra;

	list_for_each(tmp, shortcut) {
		if (tmp == &format_list)
			break;
		format = list_entry(tmp, struct pw_format, list);
		_D ("checking format [%d]: %s", s, format->name);
		if ((extra = format->test (b, s)) > 0) {
			_D ("format: %s, extra: %d", format->id, extra);
			shortcut = tmp->prev;
			return extra;
		}
		if (extra == 0) {
			_D ("format ok: %s", format->id);
			checked_format = tmp;
			shortcut = &format_list;
			return 0;
		}
	}

	shortcut = &format_list;
	return -1;
}

int pw_check(unsigned char *b, int s)
{
	return check(b, s);
}

int decrunch_pw(FILE *f1, FILE *f2)
{
	if (pw_wizardry(fileno(f1), fileno(f2)) < 0)
		return -1;

	return 0;
}

#if 0
_XMP_PLUGIN (XMP_DEPACKER, depacker_pw, pw_init);
#endif
