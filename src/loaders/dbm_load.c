/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Based on DigiBooster_E.guide from the DigiBoosterPro 2.20 package.
 * DigiBooster Pro written by Tomasz & Waldemar Piasta
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "iff.h"


struct dbm_inst {
    char name[30];
    uint16 sid;
    uint16 vol;
    uint32 c2spd;
    uint32 loop_start;
    uint32 loop_len;
    uint16 pan;
    uint16 flags;
} PACKED;



static void get_info (int size, uint16 *buffer)
{
    xxh->smp = xxh->ins = *buffer++;
    /* xxh->smp = *buffer++; */
    buffer++;	/* Samples */
    buffer++;	/* Songs */
    xxh->pat = *buffer++;
    xxh->chn = *buffer++;

    B_ENDIAN16 (xxh->ins);
    B_ENDIAN16 (xxh->smp);
    B_ENDIAN16 (xxh->pat);
    B_ENDIAN16 (xxh->chn);

    xxh->trk = xxh->pat * xxh->chn;

    INSTRUMENT_INIT ();
}


static void get_song (int size, char *buffer)
{
    if (V (0) && *buffer)
	report ("Song name      : %-44.44s\n", buffer);

    buffer += 44;

    xxh->len = *((uint16 *)buffer)++;
    B_ENDIAN16 (xxh->len);

    if (V (0))
	report ("Song length    : %d patterns\n", xxh->len);
    
    memcpy (xxo, buffer, xxh->len);
}


static void get_inst (int size, char *buffer)
{
    int i;
    struct dbm_inst *inst;

    if (V (0))
	report ("Instruments    : %d", xxh->ins);

    for (inst = (struct dbm_inst *)buffer, i = 0; i < xxh->ins; inst++, i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);

	B_ENDIAN16 (inst->sid);
	B_ENDIAN16 (inst->vol);
	B_ENDIAN32 (inst->c2spd);
	B_ENDIAN32 (inst->loop_start);
	B_ENDIAN32 (inst->loop_len);
	B_ENDIAN16 (inst->pan);
	B_ENDIAN16 (inst->flags);

	xxih[i].nsm = 1;
	xxs[i].lps = inst->loop_start;
	xxs[i].lpe = inst->loop_start + inst->loop_len;
	/*xxs[i].flg = inst[i].looplen > 2 ? WAVE_LOOPING : 0;*/
	xxi[i][0].vol = inst->vol;
	xxi[i][0].pan = inst->pan;
	xxi[i][0].sid = inst->sid;

	strncpy ((char *) xxih[i].name, inst->name, 20);
	if (V (1) && (strlen ((char *) inst->name) || (xxs[i].len > 1)))
	    report ("\n[%2X] %-30.30s %05x %05x %c %02x %02x", i,
		inst->name, xxs[i].lps, xxs[i].lpe, xxs[i].flg
		& WAVE_LOOPING ? 'L' : ' ', xxi[i][0].vol, xxi[i][0].pan);
    }
}


static void get_patt (int size, char *buffer)
{
    int i, c, r, n, rows, sz;
    struct xxm_event *event, dummy;
    uint8 *p = buffer;

    PATTERN_INIT ();
    if (V (0))
	report ("\nStored patterns: %d ", xxh->pat);

    for (i = 0; i < xxh->pat; i++) {
	rows = *((uint16 *)p)++;
	B_ENDIAN16 (rows);
	
	PATTERN_ALLOC (i);
	xxp[i]->rows = rows;
	TRACK_ALLOC (i);

	sz = *((uint32 *)p)++;
	B_ENDIAN32 (sz);

	r = 0;
	c = -1;

	while (sz >= 0) {
	    n = *p++;
	    sz--;
	    if (n == 0) {
		r++;
		c = -1;
		continue;
	    }
	    if (c >= *p)
		r++;
	    c = *p++;
	    sz--;
	    event = c >= xxh->chn? &dummy : &EVENT (i, c, r);
	    if (n & 0x01) {
		event->note = MSN (*p) * 12 + LSN (*p);
		p++; 
		sz--;
	    }
	    if (n & 0x02)
		sz--, event->ins = *p++ + 1;
	    if (n & 0x04)
		sz--, event->fxt = *p++;
	    if (n & 0x08)
		sz--, event->fxp = *p++;
	    if (n & 0x10)
		sz--, event->f2t = *p++;
	    if (n & 0x20)
		sz--, event->f2p = *p++;
	}
	if (V (0))
	    report (".");
    }
}


#if 0
static void get_sbod (int size, char *buffer)
{
    int flags;

    if (sample >= xxh->ins)
	return;

    if (!sample && V (0))
	report ("\nStored samples : %d ", xxh->smp);

    flags = XMP_SMP_NOLOAD;
    if (mode[idx[sample]] == OKT_MODE8)
	flags |= XMP_SMP_7BIT;
    if (mode[idx[sample]] == OKT_MODEB)
	flags |= XMP_SMP_7BIT;
    xmp_drv_loadpatch (NULL, sample, xmp_ctl->c4rate, flags,
	&xxs[idx[sample]], buffer);
    if (V (0))
	report (".");
    sample++;
}
#endif


int dbm_load (FILE * f)
{
    char magic[4], name[44];
    uint16 version;

    LOAD_INIT ();

    /* Check magic */
    fread (magic, 1, 4, f);
    if (strncmp (magic, "DBM0", 4))
	return -1;

    fread (&version, 1, 2, f);
    B_ENDIAN16 (version);

    fseek (f, 10, SEEK_CUR);
    fread (name, 1, 44, f);

    /* IFF chunk IDs */
    iff_register ("INFO", get_info);
    iff_register ("SONG", get_song);
    iff_register ("INST", get_inst);
    iff_register ("PATT", get_patt);
#if 0
    iff_register ("SMPL", get_smpl);
    iff_register ("VENV", get_venv);
#endif

    strncpy (xmp_ctl->name, name, XMP_DEF_NAMESIZE);
    strcpy (xmp_ctl->type, "DBM0");
    sprintf (tracker_name, "DigiBooster Pro %d.%0x",
	version >> 8, version & 0xff);

    MODULE_INFO ();

    /* Load IFF chunks */
    while (!feof (f))
	iff_chunk (f);

    iff_release ();

    if (V (0))
	report ("\n");

    return 0;
}
