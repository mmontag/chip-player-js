/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Liquid Tracker module loader based on the format description written
 * by Nir Oren. Tested with Shell.liq sent by Adi Sapir.
 */

#include <assert.h>
#include "period.h"
#include "load.h"
#include "liq.h"


static int liq_test (FILE *, char *, const int);
static int liq_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info liq_loader = {
    "LIQ",
    "Liquid Tracker",
    liq_test,
    liq_load
};

static int liq_test(FILE *f, char *t, const int start)
{
    char buf[15];

    if (fread(buf, 1, 15, f) < 15)
	return -1;

    if (memcmp(buf, "Liquid Module:", 14))
	return -1;

    read_title(f, t, 30);

    return 0;
}


#define NONE 0xff


static uint8 fx[] = {
	FX_ARPEGGIO,
	FX_S3M_BPM,
	FX_BREAK,
	FX_PORTA_DN,
	NONE,
	FX_FINE4_VIBRA,
	NONE,
	NONE,
	NONE,
	FX_JUMP,
	NONE,
	FX_VOLSLIDE,
	FX_EXTENDED,
	FX_TONEPORTA,
	FX_OFFSET,
	NONE,			/* FIXME: Pan */
	NONE,
	NONE, /*FX_MULTI_RETRIG,*/
	FX_S3M_TEMPO,
	FX_TREMOLO,
	FX_PORTA_UP,
	FX_VIBRATO,
	NONE,
	FX_TONE_VSLIDE,
	FX_VIBRA_VSLIDE,
};


/* Effect translation */
static void xlat_fx(int c, struct xmp_event *e)
{
    uint8 h = MSN (e->fxp), l = LSN (e->fxp);

    switch (e->fxt = fx[e->fxt]) {
    case FX_EXTENDED:			/* Extended effects */
	switch (h) {
	case 0x3:			/* Glissando */
	    e->fxp = l | (EX_GLISS << 4);
	    break;
	case 0x4:			/* Vibrato wave */
	    e->fxp = l | (EX_VIBRATO_WF << 4);
	    break;
	case 0x5:			/* Finetune */
	    e->fxp = l | (EX_FINETUNE << 4);
	    break;
	case 0x6:			/* Pattern loop */
	    e->fxp = l | (EX_PATTERN_LOOP << 4);
	    break;
	case 0x7:			/* Tremolo wave */
	    e->fxp = l | (EX_TREMOLO_WF << 4);
	    break;
	case 0xc:			/* Cut */
	    e->fxp = l | (EX_CUT << 4);
	    break;
	case 0xd:			/* Delay */
	    e->fxp = l | (EX_DELAY << 4);
	    break;
	case 0xe:			/* Pattern delay */
	    e->fxp = l | (EX_PATT_DELAY << 4);
	    break;
	default:			/* Ignore */
	    e->fxt = e->fxp = 0;
	    break;
	}
	break;
    case NONE:				/* No effect */
	e->fxt = e->fxp = 0;
	break;
    }
}


static void decode_event(uint8 x1, struct xmp_event *event, FILE *f)
{
    uint8 x2;

    memset (event, 0, sizeof (struct xmp_event));

    if (x1 & 0x01) {
	x2 = read8(f);
	if (x2 == 0xfe)
	    event->note = XMP_KEY_OFF;
	else
	    event->note = x2 + 1 + 24;
    }

    if (x1 & 0x02)
	event->ins = read8(f) + 1;

    if (x1 & 0x04)
	event->vol = read8(f);

    if (x1 & 0x08)
	event->fxt = read8(f) - 'A';

    if (x1 & 0x10)
	event->fxp = read8(f);

    _D(_D_INFO "  event: %02x %02x %02x %02x %02x",
	event->note, event->ins, event->vol, event->fxt, event->fxp);

    assert (event->note <= 107 || event->note == XMP_KEY_OFF);
    assert (event->ins <= 100);
    assert (event->vol <= 64);
    assert (event->fxt <= 26);
}

static int liq_load(struct xmp_context *ctx, FILE *f, const int start)
{
    struct xmp_mod_context *m = &ctx->m;
    int i;
    struct xmp_event *event = NULL;
    struct liq_header lh;
    struct liq_instrument li;
    struct liq_pattern lp;
    uint8 x1, x2;
    uint32 pmag;
    char tracker_name[21];

    LOAD_INIT();

    fread(&lh.magic, 14, 1, f);
    fread(&lh.name, 30, 1, f);
    fread(&lh.author, 20, 1, f);
    read8(f);
    fread(&lh.tracker, 20, 1, f);

    lh.version = read16l(f);
    lh.speed = read16l(f);
    lh.bpm = read16l(f);
    lh.low = read16l(f);
    lh.high = read16l(f);
    lh.chn = read16l(f);
    lh.flags = read32l(f);
    lh.pat = read16l(f);
    lh.ins = read16l(f);
    lh.len = read16l(f);
    lh.hdrsz = read16l(f);

    if ((lh.version >> 8) == 0) {
	lh.hdrsz = lh.len;
	lh.len = 0;
	fseek(f, -2, SEEK_CUR);
    }

    m->mod.tpo = lh.speed;
    m->mod.bpm = lh.bpm;
    m->mod.chn = lh.chn;
    m->mod.pat = lh.pat;
    m->mod.ins = m->mod.smp = lh.ins;
    m->mod.len = lh.len;
    m->mod.trk = m->mod.chn * m->mod.pat;
    m->mod.flg = XXM_FLG_INSVOL;

    strncpy(m->mod.name, (char *)lh.name, 30);
    strncpy(tracker_name, (char *)lh.tracker, 20);
    /* strncpy(m->author, (char *)lh.author, 20); */
    tracker_name[20] = 0;
    for (i = 20; i >= 0; i--) {
	if (tracker_name[i] == 0x20)
	   tracker_name[i] = 0;
	if (tracker_name[i])
	   break;
    }
    snprintf(m->mod.type, XMP_NAMESIZE, "LIQ %d.%02d (%s)",
		lh.version >> 8, lh.version & 0x00ff, tracker_name);

    if (lh.version > 0) {
	for (i = 0; i < m->mod.chn; i++)
	    m->mod.xxc[i].pan = read8(f) << 2;

	for (i = 0; i < m->mod.chn; i++)
	    m->mod.xxc[i].vol = read8(f);

	fread(m->mod.xxo, 1, m->mod.len, f);

	/* Skip 1.01 echo pools */
	fseek(f, lh.hdrsz - (0x6d + m->mod.chn * 2 + m->mod.len), SEEK_CUR);
    } else {
	fseek(f, start + 0xf0, SEEK_SET);
	fread (m->mod.xxo, 1, 256, f);
	fseek(f, start + lh.hdrsz, SEEK_SET);

	for (i = 0; i < 256; i++) {
	    if (m->mod.xxo[i] == 0xff)
		break;
	}
	m->mod.len = i;
    }

    MODULE_INFO();


    PATTERN_INIT();

    /* Read and convert patterns */

    _D(_D_INFO "Stored patterns: %d", m->mod.pat);

    x1 = x2 = 0;
    for (i = 0; i < m->mod.pat; i++) {
	int row, channel, count;

	PATTERN_ALLOC (i);
	pmag = read32b(f);
	if (pmag == 0x21212121)		/* !!!! */
	    continue;
	assert(pmag == 0x4c500000);	/* LP\0\0 */
	
	fread(&lp.name, 30, 1, f);
	lp.rows = read16l(f);
	lp.size = read32l(f);
	lp.reserved = read32l(f);

	_D(_D_INFO "rows: %d  size: %d\n", lp.rows, lp.size);
	m->mod.xxp[i]->rows = lp.rows;
	TRACK_ALLOC (i);

	row = 0;
	channel = 0;
	count = ftell (f);

/*
 * Packed pattern data is stored full Track after full Track from the left to
 * the right (all Intervals in Track and then going Track right). You should
 * expect 0C0h on any pattern end, and then your Unpacked Patterndata Pointer
 * should be equal to the value in offset [24h]; if it's not, you should exit
 * with an error.
 */

read_event:
	event = &EVENT(i, channel, row);

	if (x2) {
	    decode_event (x1, event, f);
	    xlat_fx (channel, event); 
	    x2--;
	    goto next_row;	
	}

	x1 = read8(f);

test_event:
	event = &EVENT(i, channel, row);
	_D(_D_INFO "* count=%ld chan=%d row=%d event=%02x",
				ftell(f) - count, channel, row, x1);

	switch (x1) {
	case 0xc0:			/* end of pattern */
	    _D(_D_WARN "- end of pattern");
	    assert (ftell (f) - count == lp.size);
	    goto next_pattern;
	case 0xe1:			/* skip channels */
	    x1 = read8(f);
	    channel += x1;
	    _D(_D_INFO "  [skip %d channels]", x1);
	    /* fall thru */
	case 0xa0:			/* next channel */
	    _D(_D_INFO "  [next channel]");
	    channel++;
	    if (channel >= m->mod.chn) {
		_D(_D_CRIT "uh-oh! bad channel number!");
		channel--;
	    }
	    row = -1;
	    goto next_row;
	case 0xe0:			/* skip rows */
	    x1 = read8(f);
	    _D(_D_INFO "  [skip %d rows]", x1);
	    row += x1;
	    /* fall thru */
	case 0x80:			/* next row */
	    _D(_D_INFO "  [next row]");
	    goto next_row;
	}

	if (x1 > 0xc0 && x1 < 0xe0) {	/* packed data */
	    _D(_D_INFO "  [packed data]");
	    decode_event (x1, event, f);
	    xlat_fx (channel, event); 
	    goto next_row;
	}

	if (x1 > 0xa0 && x1 < 0xc0) {	/* packed data repeat */
	    x2 = read8(f);
	    _D(_D_INFO "  [packed data - repeat %d times]", x2);
	    decode_event (x1, event, f);
	    xlat_fx (channel, event); 
	    goto next_row;
	}

	if (x1 > 0x80 && x1 < 0xa0) {	/* packed data repeat, keep note */
	    x2 = read8(f);
	    _D(_D_INFO "  [packed data - repeat %d times, keep note]", x2);
	    decode_event (x1, event, f);
	    xlat_fx (channel, event); 
	    while (x2) {
	        row++;
		memcpy(&EVENT(i, channel, row), event, sizeof (struct xmp_event));
		x2--;
	    }
	    goto next_row;
	}

	/* unpacked data */
	_D (_D_INFO "  [unpacked data]");
	if (x1 != 0xff)
	    event->note = 1 + 24 + x1;
	else if (x1 == 0xfe)
	    event->note = XMP_KEY_OFF;

	x1 = read8(f);
	if (x1 > 100) {
	    row++;
	    goto test_event;
	}
	if (x1 != 0xff)
	    event->ins = x1 + 1;

	x1 = read8(f);
	if (x1 != 0xff)
	    event->vol = x1;

	x1 = read8(f);
	if (x1 != 0xff)
	    event->fxt = x1 - 'A';

	x1 = read8(f);
	event->fxp = x1;

	assert(event->fxt <= 26);

	xlat_fx(channel, event); 

	_D(_D_INFO "  event: %02x %02x %02x %02x %02x\n",
	    event->note, event->ins, event->vol, event->fxt, event->fxp);

	assert (event->note <= 107 || event->note == XMP_KEY_OFF);
	assert (event->ins <= 100);
	assert (event->vol <= 65);

next_row:
	row++;
	if (row >= m->mod.xxp[i]->rows) {
	    row = 0;
	    x2 = 0;
	    channel++;

	    /* FIXME */
	    if (channel >= m->mod.chn) {
		channel = 0;
	    }
	}

	goto read_event;

next_pattern:
	;
    }

    /* Read and convert instruments */

    INSTRUMENT_INIT();

    _D(_D_INFO "Instruments: %d", m->mod.ins);

    for (i = 0; i < m->mod.ins; i++) {
	unsigned char b[4];

	m->mod.xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
	fread (&b, 1, 4, f);

	if (b[0] == '?' && b[1] == '?' && b[2] == '?' && b[3] == '?')
	    continue;
	assert (b[0] == 'L' && b[1] == 'D' && b[2] == 'S' && b[3] == 'S');
	_D(_D_WARN "INS %d: %c %c %c %c", i, b[0], b[1], b[2], b[3]);

	li.version = read16l(f);
	fread(&li.name, 30, 1, f);
	fread(&li.editor, 20, 1, f);
	fread(&li.author, 20, 1, f);
	li.hw_id = read8(f);

	li.length = read32l(f);
	li.loopstart = read32l(f);
	li.loopend = read32l(f);
	li.c2spd = read32l(f);

	li.vol = read8(f);
	li.flags = read8(f);
	li.pan = read8(f);
	li.midi_ins = read8(f);
	li.gvl = read8(f);
	li.chord = read8(f);

	li.hdrsz = read16l(f);
	li.comp = read16l(f);
	li.crc = read32l(f);

	li.midi_ch = read8(f);
	fread(&li.rsvd, 11, 1, f);
	fread(&li.filename, 25, 1, f);

	m->mod.xxi[i].nsm = !!(li.length);
	m->mod.xxi[i].vol = 0x40;
	m->mod.xxs[i].len = li.length;
	m->mod.xxs[i].lps = li.loopstart;
	m->mod.xxs[i].lpe = li.loopend;

	if (li.flags & 0x01) {
	    m->mod.xxs[i].flg = XMP_SAMPLE_16BIT;
	    m->mod.xxs[i].len >>= 1;
	    m->mod.xxs[i].lps >>= 1;
	    m->mod.xxs[i].lpe >>= 1;
	}

	if (li.loopend > 0)
	    m->mod.xxs[i].flg = XMP_SAMPLE_LOOP;

	/* FIXME: LDSS 1.0 have global vol == 0 ? */
	/* if (li.gvl == 0) */
	    li.gvl = 0x40;

	m->mod.xxi[i].sub[0].vol = li.vol;
	m->mod.xxi[i].sub[0].gvl = li.gvl;
	m->mod.xxi[i].sub[0].pan = li.pan;
	m->mod.xxi[i].sub[0].sid = i;

	copy_adjust(m->mod.xxi[i].name, li.name, 31);

	_D(_D_INFO "[%2X] %-30.30s %05x%c%05x %05x %c %02x %02x %2d.%02d %5d",
		i, m->mod.xxi[i].name, m->mod.xxs[i].len,
		m->mod.xxs[i].flg & XMP_SAMPLE_16BIT ? '+' : ' ',
		m->mod.xxs[i].lps, m->mod.xxs[i].lpe,
		m->mod.xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		m->mod.xxi[i].sub[0].vol, m->mod.xxi[i].sub[0].gvl,
		li.version >> 8, li.version & 0xff, li.c2spd);

	c2spd_to_note (li.c2spd, &m->mod.xxi[i].sub[0].xpo, &m->mod.xxi[i].sub[0].fin);
	fseek(f, li.hdrsz - 0x90, SEEK_CUR);

	if (!m->mod.xxs[i].len)
	    continue;
	load_patch(ctx, f, m->mod.xxi[i].sub[0].sid, 0, &m->mod.xxs[i], NULL);
    }

    return 0;
}

