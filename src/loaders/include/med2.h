/* Extended Module Player
 * Copyright (C) 1996-2001 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: med2.h,v 1.1 2001-06-02 20:27:11 cmatsuoka Exp $
 */

#include "xmpi.h"

typedef int32 LONG;
typedef uint32 ULONG;
typedef int16 WORD;  
typedef uint16 UWORD;
typedef int8 BYTE;
typedef uint8 UBYTE;
typedef char *STRPTR;

#define MED2_MASK_LOAD(n,v,s,f) do {	\
    uint32 mask;			\
    int i;				\
    fread (&mask, 1, 4, f);		\
    B_ENDIAN32 (mask);			\
    for(i = 0; i < 32; i++) {		\
        v[i] = 0;			\
        if(mask & 0x80000000)		\
            fread (&v[i], 1, s, f);	\
        mask <<= 1;			\
    }					\
} while (0)


/*
 * Based on MED.h V 2.000 10.03.1990 by Teijo Kinnunen, 1990
 *
 * Mini Finnish-to-English dictionary for variable names:
 * (probably not very accurate, but enough to understand the format ;))
 *
 * ala-: negation prefix (?)
 * hypätä: jump
 * hyppely: jumping (?)
 * kanava: channels
 * kappala, kappaleen: song
 * lippu : flag
 * liput: flags
 * lohko: pattern (?)
 * paala: on (?)
 * paletta: song (?)
 * pit, pituus: length
 * seis: pause (?)
 * soita: playing (?)
 * soitin: instrument
 * soittimenvoimakkuus:	volumes of instruments (?)
 * soittojarjestys: playing sequence
 * stoisto: loop
 * suodattaa: filter
 * tai: or (?)
 * vaihtoja: sliding (?)
 */

struct Pattern {	/* one pattern (4 tracks) */
	UBYTE	numtracks;
	UBYTE	pad[3];
	UBYTE	music[3 * 64 * 4];	/* at least 768 */
};
#define FOURTRKSIZE (3 * 64 * 4)
#define ONETRKSIZE (3 * 64)

#define ALATEEMITAAN 0	/* Play commands (these are ancient from V1.12) */
#define PLAY_PATTERN 1
#define PLAY_SONG 2
#define OHJELMANLOPPU 3
#define PAUSE 4
#define PLAY_NUOTTI 5
	/* Playing state */
#define DONT_PLAY 0			/* don't play */
#define PLAYING_PATTERN 1		/* playing pattern */
#define PLAYING_SONG 2			/* playing song */
	/* Flags */
#define	FLAG_FILTER_ON		0x1	/* filter on */
#define	FLAG_JUMPING_ON		0x2	/* jumping on */
#define	FLAG_JUMPING_TAHTIIN	0x4	/* every 8th */
#define	FLAG_INS_ATTACHED	0x8	/* instruments attached (MOD) */

struct Song111 {		/* 1.11/1.12 */
	ULONG id;		/* sisältää "MED\x02" versiolle 1.11 */
	char instrument[32][40];
	UBYTE instrument_volume[32];
	UWORD loop[32];
	UWORD loop_length[32];
	UWORD patterns;			/* montako lohkoa kappaleessa on */
	UBYTE sequence[100];		/* lohkojen soittoj... */
	UWORD song_length;		/* montako soittojärjestystä */
	UWORD tempo;	/* kappaleen tempo, jos ei ilmoitettu komennolla xFxx */
	UWORD flags;
	UWORD sliding;			/* 5 tai 6 */
	ULONG jump_mask;		/* jokainen bitti on 1 soitin */
	UWORD rgb[8];			/* vain 4 nyt käytössä */
} PACKED;

struct Song { /* The song structure for MED V2.00 */
	ULONG id;			/* contains "MED\x03" for V2.00 */
	char instrument[32][40];	/* names of the instruments */
	UBYTE instrument_volume[32];	/* volumes */
	UWORD loop[32];			/* repeats */
	UWORD loop_length[32];		/* rep. lengths */
	UWORD patterns;			/* number of patterns in the song */
	UBYTE sequence[100];		/* the playing sequence */
	UWORD song_length;		/* length of the playing sequence */
	UWORD tempo;			/* initial tempo */
	BYTE  playtransp;		/* play transpose */
	UBYTE flags;			/* flags */
	UWORD sliding;			/* sliding (5 or 6) */
	ULONG jump_mask;		/* jumping mask */
	UWORD rgb[8];			/* screen colors */
	UBYTE midi_channel[32];		/* midi channels */
	UBYTE midi_preset[32];		/* & presets */
} PACKED;

struct ObjSong {	/* in object files */
	UBYTE vol[32];			/* offs = 0 */
	UWORD rep[32],replen[32];	/* offs = 32, offs = 96 */
	UWORD patterns;			/* offs = 160 */
	UBYTE playseq[100];		/* offs = 162 */
	UWORD songlen;			/* offs = 262 */
	BYTE  playtransp;		/* offs = 264 */
	UBYTE flags;			/* offs = 265 */
	UWORD slide;			/* offs = 266 */
	UBYTE midichan[32],midipres[32]; /* offs = 268, offs = 300 */
};

struct SoundTrackerIns { /* SoundTracker instrument */
	char sti_nimi[22];
	UWORD sti_length;		/* length of the sample / 2 */
	UWORD sti_vol;			/* volume */
	UWORD sti_loop;			/* repeat */
	UWORD sti_looplen;		/* repeat length */
};

struct SoundTrackerSong { /* old SoundTracker song */
	char st_nimi[20];
	struct SoundTrackerIns st_ins[15];
	UBYTE st_song_length;		/* song length */
	UBYTE st_kummajainen; /* 0x78 (could anybody tell me what's this) ?? */
	UBYTE st_sequence[128];
};

struct MEDSoftIntCmd {	/* a bit ancient too */
	UWORD msic_cmd;
	ULONG msic_data;
	UWORD msic_instrument;
	UBYTE msic_aani;
	UBYTE pad;
};

struct Instrument {	/* a small sample structure */
	ULONG	length;
	UWORD	type;
	/* Followed by digitized data */
};

#define	IFF5OCT	1	/* types */
#define	IFF3OCT	2

struct ST24Mod {
	char songname[20];
	struct {
		char name[22];
		UWORD length;
		UWORD volume;
		UWORD loop;
		UWORD replen;
	} sample[31];
	UBYTE songlen;
	UBYTE I_dont_know_what_this_byte_is;
	UBYTE playseq[128];
	ULONG mk; /* contains M.K. ??? (ST songs are full of enigmas ;-) */
};

