/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include "load.h"
#include "iff.h"


#define PT3_FLAG_CIA	0x0001	/* VBlank if not set */
#define PT3_FLAG_FILTER	0x0002	/* Filter status */
#define PT3_FLAG_SONG	0x0004	/* Modules have this bit unset */
#define PT3_FLAG_IRQ	0x0008	/* Soft IRQ */
#define PT3_FLAG_VARPAT	0x0010	/* Variable pattern length */
#define PT3_FLAG_8VOICE	0x0020	/* 4 voices if not set */
#define PT3_FLAG_16BIT	0x0040	/* 8 bit samples if not set */
#define PT3_FLAG_RAWPAT	0x0080	/* Packed patterns if not set */


struct pt3_chunk_info {
    char name[32];
    uint16 nins;
    uint16 npos;
    uint16 npat;
    uint16 gvol;
    uint16 dbpm;
    uint16 flgs;
    uint16 cday;
    uint16 cmon;
    uint16 cyea;
    uint16 chrs;
    uint16 cmin;
    uint16 csec;
    uint16 dhrs;
    uint16 dmin;
    uint16 dsec;
    uint16 dmsc;
} PACKED;

struct pt3_chunk_inst {
    char name[32];
    uint16 ilen;
    uint16 ilps;
    uint16 ilpl;
    uint16 ivol;
    int16 fine;
} PACKED;


static FILE *__f;
int ptdt_load (FILE *);


static void get_vers (int size, void *buffer)
{
    sprintf (xmp_ctl->type, "%-6.6s (Protracker IFFMODL)", (char *)buffer + 4);

    /* Workaround for PT3.61 bug (?) */
    fseek (__f, 10 - size, SEEK_CUR);
}


static void get_info (int size, void *buffer)
{
    struct pt3_chunk_info *i;

    i = (struct pt3_chunk_info *)buffer;

    sprintf (xmp_ctl->name, "%-32.32s", i->name);
    B_ENDIAN16 (i->cday);
    B_ENDIAN16 (i->cmon);
    B_ENDIAN16 (i->cyea);
    B_ENDIAN16 (i->chrs);
    B_ENDIAN16 (i->cmin);
    B_ENDIAN16 (i->csec);
    B_ENDIAN16 (i->dhrs);
    B_ENDIAN16 (i->dmin);
    B_ENDIAN16 (i->dsec);

    MODULE_INFO ();

    if (V (0)) {
	report ("Creation date  : %02d/%02d/%02d %02d:%02d:%02d\n",
	    i->cmon, i->cday, i->cyea, i->chrs, i->cmin, i->csec);
	report ("Playing time   : %02d:%02d:%02d\n",
	    i->dhrs, i->dmin, i->dsec);
    }
}


static void get_cmnt (int size, uint16 *buffer)
{
    if (V (0))
	report ("Comment size   : %d\n", size);
}


static void get_ptdt (int size, uint16 *buffer)
{
    fseek (__f, -size, SEEK_CUR);
    ptdt_load (__f);
}


int pt3_load (FILE *f)
{
    struct iff_header h;

    __f = f;

    LOAD_INIT ();

    /* Check magic */
    fread (&h, 1, sizeof (struct iff_header), f);
    if (h.form[0] != 'F' || h.form[1] != 'O' || h.form[2] != 'R' ||
	h.form[3] != 'M' || h.id[0] != 'M' || h.id[1] != 'O' || h.id[2]
	!= 'D' || h.id[3] != 'L')
	return -1;

    /* IFF chunk IDs */
    iff_register ("VERS", get_vers);
    iff_register ("INFO", get_info);
    iff_register ("CMNT", get_cmnt);
    iff_register ("PTDT", get_ptdt);

    iff_setflag (IFF_FULL_CHUNK_SIZE);

    /* Load IFF chunks */
    while (!feof (f))
	iff_chunk (f);

    iff_release ();

    return 0;
}
