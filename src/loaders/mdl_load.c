/* Extended Module Player
 * Copyright (C) 1996-2004 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: mdl_load.c,v 1.2 2004-09-15 19:01:48 cmatsuoka Exp $
 */

/* Note: envelope switching (effect 9) and sample status change (effect 8)
 * are not supported. Frequency envelopes will be added in xmp 1.2.0. In
 * MDL envelopes are defined for each instrument in a patch -- xmp accepts
 * only one envelope for each patch, and the enveope of the first instrument
 * will be taken.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "iff.h"
#include "mdl.h"
#include "period.h"

static struct dev_info *dinfo;
static int i_map[16];
static int *i_index;
static int *s_index;
static int *v_index;
static int *p_index;
static int *c2spd;
static int *packinfo;
static int v_envnum;
static int p_envnum;
static struct mdl_envelope *v_env;
static struct mdl_envelope *p_env;


/* Effects 1-6 (note effects) can only be entered in the first effect
 * column, G-L (volume-effects) only in the second column.
 */

static void xlat_fx1 (uint8 *t, uint8 *p)
{
    switch (*t) {
    case 0x05:			/* Arpeggio */
	*t = FX_ARPEGGIO;
	break;
    case 0x06:			/* 6 - Not used */
    case 0x0a:			/* 6 - Not used */
    case 0x09:			/* Set envelope -- not supported */
	*t = *p = 0x00;
	break;
    case 0x07:			/* Set BPM */
	*t = FX_TEMPO;
	if (*p < 0x20)
	    *p = 0x20;
	break;
    case 0x0e:			/* Extended */
	switch (MSN (*p)) {
	case 0x0:		/* E0 - not used */
	case 0x3:		/* E3 - not used */
	case 0x8:		/* Set sample status -- unsupported */
	    *t = *p = 0x00;
	    break;
	case 0x1:		/* Pan slide left */
	    *t = FX_PANSLIDE;
	    *p <<= 4;
	    break;
	case 0x2:		/* Pan slide right */
	    *t = FX_PANSLIDE;
	    *p &= 0x0f;
	    break;
	}
	break;
    case 0x0f:
	if (*p >= 0x20)
	    *t = FX_S3M_TEMPO;
	break;
    }
}


static void xlat_fx2 (uint8 *t, uint8 *p)
{
    switch (*t) {
    case 0x06:			/* L - Not used */
    case 0x09:			/* Set envelope -- not supported */
	*t = *p = 0x00;
	break;
    case 0x07:			/* Set BPM */
	*t = FX_TEMPO;
	if (*p < 0x20)
	    *p = 0x20;
	break;
	*t = *p = 0x00;
    case 0x03:			/* Multi-retrig */
	*t = FX_MULTI_RETRIG;
	break;
    case 0x04:			/* Tremolo */
	*t = FX_TREMOLO;
	break;
    case 0x05:			/* Tremor */
	*t = FX_TREMOR;
	break;
    case 0x0e:			/* Extended */
	switch (MSN (*p)) {
	case 0x0:		/* E0 - not used */
	case 0x3:		/* E3 - not used */
	case 0x8:		/* Set sample status -- unsupported */
	    *t = *p = 0x00;
	    break;
	case 0x1:		/* Pan slide left */
	    *t = FX_PANSLIDE;
	    *p <<= 4;
	    break;
	case 0x2:		/* Pan slide right */
	    *t = FX_PANSLIDE;
	    *p &= 0x0f;
	    break;
	}
	break;
    }
}


static unsigned int get_bits (char i, uint8 **buf)
{
    static uint32 b = 0, n = 32;
    unsigned int x;

    if (i == 0) {
	b = *((uint32 *)(*buf));
	*buf += 4;
	n = 32;
	return 0;
    }

    x = b & ((1 << i) - 1);
    b >>= i;
    if ((n -= i) <= 24) {
	b |= (uint32)(*(*buf)++) << n;
	n += 8;
    }

    return x;
}

/* From the Digitrakker docs:
 *
 * The method is based on the Huffman algorithm. It's easy and very fast
 * and effective on samples. The packed sample is a bit stream:
 *
 *	     Byte 0    Byte 1    Byte 2    Byte 3
 *	Bit 76543210  fedcba98  nmlkjihg  ....rqpo
 *
 * A packed byte is stored in the following form:
 *
 *	xxxx10..0s => byte = <xxxx> + (number of <0> bits between
 *		s and 1) * 16 - 8;
 *	if s==1 then byte = byte xor 255
 *
 * If there are no <0> bits between the first bit (sign) and the <1> bit,
 * you have the following form:
 *
 *	xxx1s => byte = <xxx>; if s=1 then byte = byte xor 255
 */

static void unpack_sample8 (uint8 *t, uint8 *f, int l)
{
    int i, s;
    uint8 b, d;

    get_bits (0, &f);

    for (i = b = d = 0; i < l; i++) {
	s = get_bits (1, &f);
	if (get_bits (1, &f)) {
	    b = get_bits (3, &f);
	} else {
            b = 8;
	    while (!get_bits (1, &f))
		b += 16;
	    b += get_bits (4, &f);
	}

	if (s)
	    b ^= 0xff;

	d += b;
	*t++ = d;
    }
}


static void unpack_sample16 (uint16 *t, uint8 *f, int l)
{
    int i, lo, s;
    uint8 b, d;

    for (i = lo = b = d = 0; i < l; i++) {
	lo = get_bits (8, &f);
	s = get_bits (1, &f);
	if (get_bits (1, &f)) {
	    b = get_bits (3, &f);
	} else {
            b = 8;
	    while (!get_bits (1, &f))
		b += 16;
	    b += get_bits (4, &f);
	}

	if (s)
	    b ^= 0xff;

	d += b;
	*t++ = ((uint32)d << 8) | lo;
    }
}

/*
 * IFF chunk handlers
 */

static void get_chunk_in (int size, struct mdl_in_chunk *buffer)
{
    int i;

    strncpy (xmp_ctl->name, buffer->name, 32);
    strncpy (author_name, buffer->author, 20);

    L_ENDIAN16 (buffer->length);
    L_ENDIAN16 (buffer->restart);

    xxh->len = buffer->length;
    xxh->rst = buffer->restart;
    xxh->tpo = buffer->tempo;
    xxh->bpm = buffer->bpm;

    for (i = 0; i < 32; i++) {
	if (buffer->chinfo[i] & 0x80)
	    break;
	xxc[i].pan = buffer->chinfo[i] << 1;
    }
    xxh->chn = i;

    memcpy (xxo, buffer->orders, xxh->len);

    MODULE_INFO ();
}


static void get_chunk_pa (int size, uint8 *buffer)
{
    int i, j, chn;
    uint16 x16;

    xxh->pat = *buffer++;
    xxh->trk = xxh->pat * xxh->chn;	/* Max */

    PATTERN_INIT ();
    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	chn = *buffer++;
	xxp[i]->rows = (int) (*buffer++) + 1;

	buffer += 16;		/* Skip pattern name */
	for (j = 0; j < chn; j++) {
	    x16 = *((uint16 *)buffer);
	    buffer += 2;
	    L_ENDIAN16 (x16);
	    xxp[i]->info[j].index = x16;
	}
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");
}


static void get_chunk_p0 (int size, uint8 *buffer)
{
    int i, j;
    uint16 x16;

    xxh->pat = *buffer++;
    xxh->trk = xxh->pat * xxh->chn;	/* Max */

    PATTERN_INIT ();
    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;

	for (j = 0; j < 32; j++) {
	    x16 = *((uint16 *)buffer);
	    buffer += 2;
	    L_ENDIAN16 (x16);
	    if (j < xxh->chn)
		xxp[i]->info[j].index = x16;
	}
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");
}


static void get_chunk_tr (int size, uint8 *buffer)
{
    int i, j, k, row, len;
    struct xxm_track *track;

    xxh->trk = *((uint16 *) buffer) + 1;
    buffer += 2;

    if (V (0))
	report ("Stored tracks  : %d ", xxh->trk);

    track = calloc (1, sizeof (struct xxm_track) +
	sizeof (struct xxm_event) * 256);

    /* Empty track 0 is not stored in the file */
    xxt[0] = calloc (1, sizeof (struct xxm_track) +
	256 * sizeof (struct xxm_event));
    xxt[0]->rows = 256;

    for (i = 1; i < xxh->trk; i++) {
	/* Length of the track in bytes */
	len = *((uint16 *) buffer);
	buffer += 2;

	memset (track, 0, sizeof (struct xxm_track) +
            sizeof (struct xxm_event) * 256);

	for (row = 0; len;) {
	    j = *buffer++;
	    len--;
	    switch (j & 0x03) {
	    case 0:
		row += j >> 2;
		break;
	    case 1:
		for (k = 0; k <= (j >> 2); k++)
		    memcpy (&track->event[row + k], &track->event[row - 1],
			sizeof (struct xxm_event));
		row += k - 1;
		break;
	    case 2:
		memcpy (&track->event[row], &track->event[j >> 2],
		    sizeof (struct xxm_event));
		break;
	    case 3:
		if (j & MDL_NOTE_FOLLOWS)
		    len--, track->event[row].note = *buffer++;
		if (j & MDL_INSTRUMENT_FOLLOWS)
		    len--, track->event[row].ins = *buffer++;
		if (j & MDL_VOLUME_FOLLOWS)
		    len--, track->event[row].vol = *buffer++;
		if (j & MDL_EFFECT_FOLLOWS) {
		    len--, k = *buffer++;
		    track->event[row].fxt = LSN (k);
		    track->event[row].f2t = MSN (k);
		}
		if (j & MDL_PARAMETER1_FOLLOWS)
		    len--, track->event[row].fxp = *buffer++;
		if (j & MDL_PARAMETER2_FOLLOWS)
		    len--, track->event[row].f2p = *buffer++;
		break;
	    }

	    xlat_fx1 (&track->event[row].fxt, &track->event[row].fxp);
	    xlat_fx2 (&track->event[row].f2t, &track->event[row].f2p);

	    row++;
	}

	if (row <= 64)
	    row = 64;
	else if (row <= 128)
	    row = 128;
	else row = 256;

	xxt[i] = calloc (1, sizeof (struct xxm_track) +
	    sizeof (struct xxm_event) * row);
	memcpy (xxt[i], track, sizeof (struct xxm_track) +
	    sizeof (struct xxm_event) * row);
	xxt[i]->rows = row;

	if (V (0) && !(i % xxh->chn))
	    report (".");
    }

    free (track);

    if (V (0))
	report ("\n");
}


static void get_chunk_ii (int size, uint8 *buffer)
{
    int i, j;
    char *instr;

    xxh->ins = *buffer++;

    if (V (0))
	report ("Instruments    : %d ", xxh->ins);

    INSTRUMENT_INIT ();

    for (i = 0; i < xxh->ins; i++) {
	i_index[i] = *buffer++;
	xxih[i].nsm = *buffer++;
	instr = buffer;
	*(instr + 31) = 0;
	str_adj (instr);
	strncpy (xxih[i].name, buffer, 24);
	buffer += 32;

	if (V (1) && (strlen ((char *) xxih[i].name) || xxih[i].nsm))
	    report ("\n[%2X] %-32.32s %2d ", i_index[i], instr, xxih[i].nsm);

	xxi[i] = calloc (sizeof (struct xxm_instrument), xxih[i].nsm);

	for (j = 0; j < xxih[i].nsm; j++) {
	    xxi[i][j].sid = *buffer++;
	    i_map[j] = *buffer++;
	    xxi[i][j].vol = *buffer++;

	    if (j == 0)
		v_index[i] = *buffer & 0x80 ? *buffer & 0x3f : -1;
	    if (~*buffer & 0x40)
		xxi[i][j].vol = 0xff;
	    buffer++;

	    xxi[i][j].pan = *buffer++ << 1;

	    if (j == 0)
		p_index[i] = *buffer & 0x80 ? *buffer & 0x3f : -1;
	    if (~*buffer & 0x40)
		xxi[i][j].pan = 0x80;
	    buffer++;

	    if (j == 0) {
		xxih[i].rls = *(uint16 *)buffer;
		L_ENDIAN32 (xxih[i].rls);
	    }
	    buffer += 2;

	    xxi[i][j].vra = *buffer++;
	    xxi[i][j].vde = *buffer++;
	    xxi[i][j].vsw = *buffer++;
	    xxi[i][j].vwf = *buffer++;
	    buffer++;		/* Reserved */
	    buffer++;		/* Pitch envelope */

	    if (V (1)) {
		report ("%s[%1x] V%02x S%02x ",
		    j ? "\n\t\t\t\t" : "\t", j, xxi[i][j].vol, xxi[i][j].sid);
		if (v_index[i] > 0)
		    report ("v%02x ", v_index[i]);
		else
		    report ("v-- ");
		if (p_index[i] > 0)
		    report ("p%02x ", p_index[i]);
		else
		    report ("p-- ");
	    } else if (V (0) == 1)
		report (".");
	}
    }
    if (V (0))
	report ("\n");
}


static void get_chunk_is (int size, uint8 *buffer)
{
    int i;

    xxh->smp = *buffer++;
    xxs = calloc (sizeof (struct xxm_sample), xxh->smp);
    packinfo = calloc (sizeof (int), xxh->smp);

    if (V (1))
	report ("Sample infos   : %d ", xxh->smp);

    for (i = 0; i < xxh->smp; i++) {
	s_index[i] = *buffer++;		/* Sample number */
	*(buffer + 31) = 0;
	str_adj (buffer);
	if (V (2))
	    report ("\n[%2X] %-32.32s ", s_index[i], (char *)buffer);
	buffer += 32;			/* Sample name */
	buffer += 8;			/* Sample filename */

	c2spd[i] = *((uint32 *)buffer); buffer += 4;

	xxs[i].len = *((uint32 *)buffer); buffer += 4;
	xxs[i].lps = *((uint32 *)buffer); buffer += 4;
	xxs[i].lpe = *((uint32 *)buffer); buffer += 4;

	L_ENDIAN32 (xxs[i].len);
	L_ENDIAN32 (xxs[i].lps);
	L_ENDIAN32 (xxs[i].lpe);

	xxs[i].flg = xxs[i].lpe > 0 ? WAVE_LOOPING : 0;
	xxs[i].lpe = xxs[i].lps + xxs[i].lpe;

	buffer++;			/* Volume in DMDL 0.0 */
	xxs[i].flg |= (*buffer & 0x01) ? WAVE_16_BITS : 0;
	xxs[i].flg |= (*buffer & 0x02) ? WAVE_BIDIR_LOOP : 0;
	packinfo[i] = (*buffer & 0x0c) >> 2;
	buffer++;

#if 0
	if (xxs[i].flg & WAVE_16_BITS) {
	    xxs[i].len <<= 1;
	    xxs[i].lps <<= 1;
	    xxs[i].lpe <<= 1;
	}
#endif

	if (V (2)) {
	    report ("%5d %05x%c %05x %05x ", c2spd[i],
		xxs[i].len, xxs[i].flg & WAVE_16_BITS ? '+' : ' ',
		xxs[i].lps, xxs[i].lpe);
	    switch (packinfo[i]) {
	    case 0:
		report ("[nopack] ");
		break;
	    case 1:
		report ("[pack8]  ");
		break;
	    case 2:
		report ("[pack16] ");
		break;
	    case 3:
		report ("[error]  ");
		break;
	    }
	}

	if (V (1))
	    report (".");
    }
    if (V (1))
	report ("\n");
}


static void get_chunk_i0 (int size, uint8 *buffer)
{
    int i;

    xxh->ins = xxh->smp = *buffer++;

    if (V (0))
	report ("Instruments    : %d ", xxh->ins);

    INSTRUMENT_INIT ();

    xxs = calloc (sizeof (struct xxm_sample), xxh->smp);
    packinfo = calloc (sizeof (int), xxh->smp);

    for (i = 0; i < xxh->ins; i++) {
	xxih[i].nsm = 1;
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxi[i][0].sid = i_index[i] = s_index[i] = *buffer++;

	*(buffer + 31) = 0;
	str_adj (buffer);
	if (V (1))
	    report ("\n[%2X] %-32.32s ", i_index[i], (char *)buffer);
	buffer += 32;			/* Sample name */
	buffer += 8;			/* Sample filename */

	c2spd[i] = *((uint16 *)buffer); buffer += 2;
	L_ENDIAN16 (c2spd[i]);

	xxs[i].len = *((uint32 *)buffer); buffer += 4;
	xxs[i].lps = *((uint32 *)buffer); buffer += 4;
	xxs[i].lpe = *((uint32 *)buffer); buffer += 4;

	L_ENDIAN32 (xxs[i].len);
	L_ENDIAN32 (xxs[i].lps);
	L_ENDIAN32 (xxs[i].lpe);

	xxs[i].flg = xxs[i].lpe > 0 ? WAVE_LOOPING : 0;
	xxs[i].lpe = xxs[i].lps + xxs[i].lpe;

	xxi[i][0].vol = *buffer++;	/* Volume */
	xxi[i][0].pan = 0x80;

	xxs[i].flg |= (*buffer & 0x01) ? WAVE_16_BITS : 0;
	xxs[i].flg |= (*buffer & 0x02) ? WAVE_BIDIR_LOOP : 0;
	packinfo[i] = (*buffer & 0x0c) >> 2;
	buffer++;

	if (V (1)) {
	    report ("%5d V%02x %05x%c %05x %05x ",
		c2spd[i],  xxi[i][0].vol,
		xxs[i].len, xxs[i].flg & WAVE_16_BITS ? '+' : ' ',
		xxs[i].lps, xxs[i].lpe);
	    switch (packinfo[i]) {
	    case 0:
		report ("[nopack] ");
		break;
	    case 1:
		report ("[pack8]  ");
		break;
	    case 2:
		report ("[pack16] ");
		break;
	    case 3:
		report ("[error]  ");
		break;
	    }
	}

	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");
}


static void get_chunk_sa (int size, uint8 *buffer)
{
    int i, len;
    char *smpbuf;

    if (V (0))
	report ("Stored samples : %d ", xxh->smp);

    for (i = 0; i < xxh->smp; i++) {
	smpbuf = calloc (1, xxs[i].flg & WAVE_16_BITS ?
		xxs[i].len << 1 : xxs[i].len);

	switch (packinfo[i]) {
	case 0:
	    memcpy (smpbuf, buffer, xxs[i].len);
	    buffer += xxs[i].len;
	    break;
	case 1: 
	    len = *((uint32 *)buffer); buffer += 4;
	    unpack_sample8 (smpbuf, buffer, xxs[i].len);
	    buffer += len;
	    break;
	case 2:
	    len = *((uint32 *)buffer); buffer += 4;
	    unpack_sample16 ((uint16 *)smpbuf, buffer, xxs[i].len >> 1);
	    buffer += len;
	    break;
	}
	
	xmp_drv_loadpatch (NULL, i, xmp_ctl->c4rate, XMP_SMP_NOLOAD, &xxs[i], smpbuf);

	free (smpbuf);

	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");
}


static void get_chunk_ve (int size, uint8 *buffer)
{
    if ((v_envnum = *buffer++) == 0)
	return;

    if (V (1))
	report ("Vol envelopes  : %d\n", v_envnum);

    v_env = calloc (v_envnum, sizeof (struct mdl_envelope));
    memcpy (v_env, buffer, v_envnum * sizeof (struct mdl_envelope));
}


static void get_chunk_pe (int size, uint8 *buffer)
{
    if ((p_envnum = *buffer++) == 0)
	return;

    if (V (1))
	report ("Pan envelopes  : %d\n", p_envnum);

    p_env = calloc (p_envnum, sizeof (struct mdl_envelope));
    memcpy (p_env, buffer, p_envnum * sizeof (struct mdl_envelope));
}


int mdl_load (FILE *f, struct dev_info *d)
{
    int i, j, k, l;
    char buf[8];

    LOAD_INIT ();

    /* Check magic and get version */
    fread (buf, 1, 4, f);
    if (strncmp (buf, "DMDL", 4))
	return -1;
    fread (buf, 1, 1, f);

    /* IFFoid chunk IDs */
    iff_register ("IN", get_chunk_in);	/* Module info */
    iff_register ("TR", get_chunk_tr);	/* Tracks */
    iff_register ("SA", get_chunk_sa);	/* Sampled data */
    iff_register ("VE", get_chunk_ve);	/* Volume envelopes */
    iff_register ("PE", get_chunk_pe);	/* Pan envelopes */

    if (MSN (*buf)) {
	iff_register ("II", get_chunk_ii);	/* Instruments */
	iff_register ("PA", get_chunk_pa);	/* Patterns */
	iff_register ("IS", get_chunk_is);	/* Sample info */
    } else {
	iff_register ("PA", get_chunk_p0);	/* Old 0.0 patterns */
	iff_register ("IS", get_chunk_i0);	/* Old 0.0 Sample info */
    }

    /* MDL uses a degenerated IFF-style file format with 16 bit IDs and
     * little endian 32 bit chunk size. There's only one chunk per data
     * type i.e. one huge chunk for all the sampled instruments.
     */
    iff_idsize (2);
    iff_setflag (IFF_LITTLE_ENDIAN);

    sprintf (xmp_ctl->type, "DMDL %d.%d", MSN (*buf), LSN (*buf));

    dinfo = d;
    xmp_ctl->volbase = 0xff;
    xmp_ctl->c4rate = C4_NTSC_RATE;

    v_envnum = 0;
    s_index = calloc (256, sizeof (int));
    i_index = calloc (256, sizeof (int));
    v_index = calloc (256, sizeof (int));
    p_index = calloc (256, sizeof (int));
    c2spd = calloc (256, sizeof (int));

    /* Load IFFoid chunks */
    while (!feof (f))
	iff_chunk (f);

    iff_release ();

    /* Re-index instruments & samples */

    for (i = 0; i < xxh->pat; i++)
	for (j = 0; j < xxp[i]->rows; j++)
	    for (k = 0; k < xxh->chn; k++)
		for (l = 0; l < xxh->ins; l++)
		    if (EVENT(i, k, j).ins && EVENT(i, k, j).ins == i_index[l]) {
		    	EVENT(i, k, j).ins = l + 1;
			break;
		    }

    for (i = 0; i < xxh->ins; i++) {
	/* Envelopes */
	if (v_index[i] > 0) {
	    xxih[i].aei.flg = XXM_ENV_ON;
	    xxih[i].aei.npt = 16;
	    xxae[i] = calloc (4, xxih[i].aei.npt);

	    for (j = 0; j < v_envnum; j++) {
		if (v_index[i] == j) {
		    xxih[i].aei.flg |= v_env[j].sus & 0x10 ? XXM_ENV_SUS : 0;
		    xxih[i].aei.flg |= v_env[j].sus & 0x20 ? XXM_ENV_LOOP : 0;
		    xxih[i].aei.sus = v_env[j].sus & 0x0f;
		    xxih[i].aei.lps = v_env[j].loop & 0x0f;
		    xxih[i].aei.lpe = v_env[j].loop & 0xf0;
		    xxae[i][0] = 0;
		    for (k = 1; k < xxih[i].aei.npt; k++) {
			if ((xxae[i][k * 2] = v_env[j].data[(k - 1) * 2]) == 0)
			    break;
			xxae[i][k * 2] += xxae[i][(k - 1) * 2];
			xxae[i][k * 2 + 1] = v_env[j].data[k * 2];
		    }
		    xxih[i].aei.npt = k;
		    break;
		}
	    }
	}

	if (p_index[i] > 0) {
	    xxih[i].pei.flg = XXM_ENV_ON;
	    xxih[i].pei.npt = 16;
	    xxae[i] = calloc (4, xxih[i].pei.npt);

	    for (j = 0; j < p_envnum; j++) {
		if (p_index[i] == j) {
		    xxih[i].pei.flg |= p_env[j].sus & 0x10 ? XXM_ENV_SUS : 0;
		    xxih[i].pei.flg |= p_env[j].sus & 0x20 ? XXM_ENV_LOOP : 0;
		    xxih[i].pei.sus = p_env[j].sus & 0x0f;
		    xxih[i].pei.lps = p_env[j].loop & 0x0f;
		    xxih[i].pei.lpe = p_env[j].loop & 0xf0;
		    xxae[i][0] = 0;
		    for (k = 1; k < xxih[i].pei.npt; k++) {
			if ((xxae[i][k * 2] = p_env[j].data[(k - 1) * 2]) == 0)
			    break;
			xxae[i][k * 2] += xxae[i][(k - 1) * 2];
			xxae[i][k * 2 + 1] = p_env[j].data[k * 2];
		    }
		    xxih[i].pei.npt = k;
		    break;
		}
	    }
	}

	for (j = 0; j < xxih[i].nsm; j++)
	    for (k = 0; k < xxh->smp; k++)
		if (xxi[i][j].sid == s_index[k]) {
		    xxi[i][j].sid = k;
		    c2spd_to_note (c2spd[k], &xxi[i][j].xpo, &xxi[i][j].fin);
		    break;
		}
    }

    free (c2spd);
    free (p_index);
    free (v_index);
    free (i_index);
    free (s_index);

    if (v_envnum)
	free (v_env);
    if (p_envnum)
	free (p_env);

    xmp_ctl->fetch |= XMP_CTL_FINEFX;

    return 0;
}

