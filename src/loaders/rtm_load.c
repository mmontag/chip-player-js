/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "loader.h"
#include "period.h"

#ifndef __amigaos4__
typedef uint8 BYTE;
typedef uint16 WORD;
#endif
typedef uint32 DWORD;

/* Data structures from the specification of the RTM format version 1.10 by
 * Arnaud Hasenfratz
 */

struct ObjectHeader {
	char id[4];		/* "RTMM", "RTND", "RTIN" or "RTSM" */
	char rc;		/* 0x20 */
	char name[32];		/* object name */
	char eof;		/* "\x1A" */
	WORD version;		/* version of the format (actual : 0x110) */
	WORD headerSize;	/* object header size */
};

struct RTMMHeader {		/* Real Tracker Music Module */
	char software[20];	/* software used for saving the module */
	char composer[32];
	WORD flags;		/* song flags */
				/* bit 0 : linear table,
				   bit 1 : track names present */
	BYTE ntrack;		/* number of tracks */
	BYTE ninstr;		/* number of instruments */
	WORD nposition;		/* number of positions */
	WORD npattern;		/* number of patterns */
	BYTE speed;		/* initial speed */
	BYTE tempo;		/* initial tempo */
	char panning[32];	/* initial pannings (for S3M compatibility) */
	DWORD extraDataSize;	/* length of data after the header */

/* version 1.12 */
	char originalName[32];
};

struct RTNDHeader {		/* Real Tracker Note Data */
	WORD flags;		/* Always 1 */
	BYTE ntrack;
	WORD nrows;
	DWORD datasize;		/* Size of packed data */
};

struct EnvelopePoint {
	long x;
	long y;
};

struct Envelope {
	BYTE npoint;
	struct EnvelopePoint point[12];
	BYTE sustain;
	BYTE loopstart;
	BYTE loopend;
	WORD flags;		/* bit 0 : enable envelope,
				   bit 1 : sustain, bit 2 : loop */
};

struct RTINHeader {		/* Real Tracker Instrument */
	BYTE nsample;
	WORD flags;		/* bit 0 : default panning enabled
				   bit 1 : mute samples */
	BYTE table[120];	/* sample number for each note */
	struct Envelope volumeEnv;
	struct Envelope panningEnv;
	char vibflg;		/* vibrato type */
	char vibsweep;		/* vibrato sweep */
	char vibdepth;		/* vibrato depth */
	char vibrate;		/* vibrato rate */
	WORD volfade;

/* version 1.10 */
	BYTE midiPort;
	BYTE midiChannel;
	BYTE midiProgram;
	BYTE midiEnable;

/* version 1.12 */
	char midiTranspose;
	BYTE midiBenderRange;
	BYTE midiBaseVolume;
	char midiUseVelocity;
};

struct RTSMHeader {		/* Real Tracker Sample */
	WORD flags;		/* bit 1 : 16 bits,
				   bit 2 : delta encoded (always) */
	BYTE basevolume;
	BYTE defaultvolume;
	DWORD length;
	BYTE loop;		/* =0:no loop, =1:forward loop,
				   =2:bi-directional loop */
	BYTE reserved[3];
	DWORD loopbegin;
	DWORD loopend;
	DWORD basefreq;
	BYTE basenote;
	char panning;		/* Panning from -64 to 64 */
};


static int rtm_test(HIO_HANDLE *, char *, const int);
static int rtm_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader rtm_loader = {
	"Real Tracker (RTM)",
	rtm_test,
	rtm_load
};

static int rtm_test(HIO_HANDLE *f, char *t, const int start)
{
	char buf[4];

	if (hio_read(buf, 1, 4, f) < 4)
		return -1;
	if (memcmp(buf, "RTMM", 4))
		return -1;

	if (hio_read8(f) != 0x20)
		return -1;

	read_title(f, t, 32);

	return 0;
}


#define MAX_SAMP 1024

static int read_object_header(HIO_HANDLE *f, struct ObjectHeader *h, char *id)
{
	hio_read(&h->id, 4, 1, f);
	D_(D_WARN "object id: %02x %02x %02x %02x", h->id[0],
					h->id[1], h->id[2], h->id[3]);

	if (memcmp(id, h->id, 4))
		return -1;

	h->rc = hio_read8(f);
	if (h->rc != 0x20)
		return -1;
	hio_read(&h->name, 32, 1, f);
	h->eof = hio_read8(f);
	h->version = hio_read16l(f);
	h->headerSize = hio_read16l(f);
	D_(D_INFO "object %-4.4s (%d)", h->id, h->headerSize);
	
	return 0;
}


static int rtm_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j, r;
	struct xmp_event *event;
	struct ObjectHeader oh;
	struct RTMMHeader rh;
	struct RTNDHeader rp;
	struct RTINHeader ri;
	struct RTSMHeader rs;
	int offset, smpnum, version;
	char tracker_name[21], composer[33];

	LOAD_INIT();

	if (read_object_header(f, &oh, "RTMM") < 0)
		return -1;

	version = oh.version;

	hio_read(tracker_name, 1, 20, f);
	tracker_name[20] = 0;
	hio_read(composer, 1, 32, f);
	composer[32] = 0;
	rh.flags = hio_read16l(f);	/* bit 0: linear table, bit 1: track names */
	rh.ntrack = hio_read8(f);
	rh.ninstr = hio_read8(f);
	rh.nposition = hio_read16l(f);
	rh.npattern = hio_read16l(f);
	rh.speed = hio_read8(f);
	rh.tempo = hio_read8(f);
	hio_read(&rh.panning, 32, 1, f);
	rh.extraDataSize = hio_read32l(f);

	if (version >= 0x0112)
		hio_seek(f, 32, SEEK_CUR);		/* skip original name */

	for (i = 0; i < rh.nposition; i++)
		mod->xxo[i] = hio_read16l(f);
	
	strncpy(mod->name, oh.name, 20);
	snprintf(mod->type, XMP_NAME_SIZE, "%s RTM %x.%02x",
				tracker_name, version >> 8, version & 0xff);
	/* strncpy(m->author, composer, XMP_NAME_SIZE); */

	mod->len = rh.nposition;
	mod->pat = rh.npattern;
	mod->chn = rh.ntrack;
	mod->trk = mod->chn * mod->pat;
	mod->ins = rh.ninstr;
	mod->spd = rh.speed;
	mod->bpm = rh.tempo;

	m->quirk |= rh.flags & 0x01 ? QUIRK_LINEAR : 0;

	MODULE_INFO();

	for (i = 0; i < mod->chn; i++)
		mod->xxc[i].pan = rh.panning[i] & 0xff;

	if (pattern_init(mod) < 0)
		return -1;

	D_(D_INFO "Stored patterns: %d", mod->pat);

	offset = 42 + oh.headerSize + rh.extraDataSize;

	for (i = 0; i < mod->pat; i++) {
		uint8 c;

		hio_seek(f, start + offset, SEEK_SET);

		if (read_object_header(f, &oh, "RTND") < 0) {
			D_(D_CRIT "Error reading pattern %d", i);
			return -1;
		}
	
		rp.flags = hio_read16l(f);
		rp.ntrack = hio_read8(f);
		rp.nrows = hio_read16l(f);
		rp.datasize = hio_read32l(f);

		offset += 42 + oh.headerSize + rp.datasize;

		if (pattern_tracks_alloc(mod, i, rp.nrows) < 0)
			return -1;

		for (r = 0; r < rp.nrows; r++) {
			for (j = 0; /*j < rp.ntrack */; j++) {

				c = hio_read8(f);
				if (c == 0)		/* next row */
					break;

				/* should never happen! */
				if (j >= rp.ntrack) {
					D_(D_CRIT "error: decoding "
						"pattern %d row %d", i, r);
					break;
				}

				event = &EVENT(i, j, r);

				if (c & 0x01) {		/* set track */
					j = hio_read8(f);
					event = &EVENT(i, j, r);
				}
				if (c & 0x02) {		/* read note */
					event->note = hio_read8(f) + 1;
					if (event->note == 0xff) {
						event->note = XMP_KEY_OFF;
					} else {
						event->note += 12;
					}
				}
				if (c & 0x04)		/* read instrument */
					event->ins = hio_read8(f);
				if (c & 0x08)		/* read effect */
					event->fxt = hio_read8(f);
				if (c & 0x10)		/* read parameter */
					event->fxp = hio_read8(f);
				if (c & 0x20)		/* read effect 2 */
					event->f2t = hio_read8(f);
				if (c & 0x40)		/* read parameter 2 */
					event->f2p = hio_read8(f);
			}
		}
	}

	/*
	 * load instruments
	 */

	D_(D_INFO "Instruments: %d", mod->ins);

	hio_seek(f, start + offset, SEEK_SET);

	/* ESTIMATED value! We don't know the actual value at this point */
	mod->smp = MAX_SAMP;

	if (instrument_init(mod) < 0)
		return -1;

	smpnum = 0;
	for (i = 0; i < mod->ins; i++) {
		if (read_object_header(f, &oh, "RTIN") < 0) {
			D_(D_CRIT "Error reading instrument %d", i);
			return -1;
		}

		instrument_name(mod, i, (uint8 *)&oh.name, 32);

		if (oh.headerSize == 0) {
			D_(D_INFO "[%2X] %-26.26s %2d ", i,
					mod->xxi[i].name, mod->xxi[i].nsm);
			ri.nsample = 0;
			continue;
		}

		ri.nsample = hio_read8(f);
		ri.flags = hio_read16l(f);	/* bit 0 : default panning enabled */
		hio_read(&ri.table, 120, 1, f);

		ri.volumeEnv.npoint = hio_read8(f);
		for (j = 0; j < 12; j++) {
			ri.volumeEnv.point[j].x = hio_read32l(f);
			ri.volumeEnv.point[j].y = hio_read32l(f);
		}
		ri.volumeEnv.sustain = hio_read8(f);
		ri.volumeEnv.loopstart = hio_read8(f);
		ri.volumeEnv.loopend = hio_read8(f);
		ri.volumeEnv.flags = hio_read16l(f); /* bit 0:enable 1:sus 2:loop */
		
		ri.panningEnv.npoint = hio_read8(f);
		for (j = 0; j < 12; j++) {
			ri.panningEnv.point[j].x = hio_read32l(f);
			ri.panningEnv.point[j].y = hio_read32l(f);
		}
		ri.panningEnv.sustain = hio_read8(f);
		ri.panningEnv.loopstart = hio_read8(f);
		ri.panningEnv.loopend = hio_read8(f);
		ri.panningEnv.flags = hio_read16l(f);

		ri.vibflg = hio_read8(f);
		ri.vibsweep = hio_read8(f);
		ri.vibdepth = hio_read8(f);
		ri.vibrate = hio_read8(f);
		ri.volfade = hio_read16l(f);

		if (version >= 0x0110) {
			ri.midiPort = hio_read8(f);
			ri.midiChannel = hio_read8(f);
			ri.midiProgram = hio_read8(f);
			ri.midiEnable = hio_read8(f);
		}
		if (version >= 0x0112) {
			ri.midiTranspose = hio_read8(f);
			ri.midiBenderRange = hio_read8(f);
			ri.midiBaseVolume = hio_read8(f);
			ri.midiUseVelocity = hio_read8(f);
		}

		mod->xxi[i].nsm = ri.nsample;

		D_(D_INFO "[%2X] %-26.26s %2d", i, mod->xxi[i].name,
							mod->xxi[i].nsm);

		if (mod->xxi[i].nsm > 16)
			mod->xxi[i].nsm = 16;

		if (subinstrument_alloc(mod, i, mod->xxi[i].nsm) < 0)
			return -1;

		for (j = 0; j < 120; j++)
			mod->xxi[i].map[j].ins = ri.table[j];

		/* Envelope */
		mod->xxi[i].rls = ri.volfade;
		mod->xxi[i].aei.npt = ri.volumeEnv.npoint;
		mod->xxi[i].aei.sus = ri.volumeEnv.sustain;
		mod->xxi[i].aei.lps = ri.volumeEnv.loopstart;
		mod->xxi[i].aei.lpe = ri.volumeEnv.loopend;
		mod->xxi[i].aei.flg = ri.volumeEnv.flags;
		mod->xxi[i].pei.npt = ri.panningEnv.npoint;
		mod->xxi[i].pei.sus = ri.panningEnv.sustain;
		mod->xxi[i].pei.lps = ri.panningEnv.loopstart;
		mod->xxi[i].pei.lpe = ri.panningEnv.loopend;
		mod->xxi[i].pei.flg = ri.panningEnv.flags;

		if (mod->xxi[i].aei.npt <= 0)
			mod->xxi[i].aei.flg &= ~XMP_ENVELOPE_ON;

		if (mod->xxi[i].pei.npt <= 0)
			mod->xxi[i].pei.flg &= ~XMP_ENVELOPE_ON;

		for (j = 0; j < mod->xxi[i].aei.npt; j++) {
			mod->xxi[i].aei.data[j * 2 + 0] = ri.volumeEnv.point[j].x;
			mod->xxi[i].aei.data[j * 2 + 1] = ri.volumeEnv.point[j].y / 2;
		}
		for (j = 0; j < mod->xxi[i].pei.npt; j++) {
			mod->xxi[i].pei.data[j * 2 + 0] = ri.panningEnv.point[j].x;
			mod->xxi[i].pei.data[j * 2 + 1] = 32 + ri.panningEnv.point[j].y / 2;
		}

		/* For each sample */
		for (j = 0; j < mod->xxi[i].nsm; j++, smpnum++) {
			if (read_object_header(f, &oh, "RTSM") < 0) {
				D_(D_CRIT "Error reading sample %d", j);
				return -1;
			}

			rs.flags = hio_read16l(f);
			rs.basevolume = hio_read8(f);
			rs.defaultvolume = hio_read8(f);
			rs.length = hio_read32l(f);
			rs.loop = hio_read32l(f);
			rs.loopbegin = hio_read32l(f);
			rs.loopend = hio_read32l(f);
			rs.basefreq = hio_read32l(f);
			rs.basenote = hio_read8(f);
			rs.panning = hio_read8(f);

			c2spd_to_note(rs.basefreq, &mod->xxi[i].sub[0].xpo,
						&mod->xxi[i].sub[0].fin);
			mod->xxi[i].sub[j].xpo += 48 - rs.basenote;

			mod->xxi[i].sub[j].vol = rs.defaultvolume *
						rs.basevolume / 0x40;
			mod->xxi[i].sub[j].pan = 0x80 + rs.panning * 2;
			mod->xxi[i].sub[j].vwf = ri.vibflg;
			mod->xxi[i].sub[j].vde = ri.vibdepth;
			mod->xxi[i].sub[j].vra = ri.vibrate;
			mod->xxi[i].sub[j].vsw = ri.vibsweep;
			mod->xxi[i].sub[j].sid = smpnum;

			if (smpnum >= MAX_SAMP) {
				hio_seek(f, rs.length, SEEK_CUR);
				continue;
			}

			copy_adjust(mod->xxs[smpnum].name, (uint8 *)oh.name, 32);

			mod->xxs[smpnum].len = rs.length;
			mod->xxs[smpnum].lps = rs.loopbegin;
			mod->xxs[smpnum].lpe = rs.loopend;

			mod->xxs[smpnum].flg = 0;
			if (rs.flags & 0x02) {
				mod->xxs[smpnum].flg |= XMP_SAMPLE_16BIT;
				mod->xxs[smpnum].len >>= 1;
				mod->xxs[smpnum].lps >>= 1;
				mod->xxs[smpnum].lpe >>= 1;
			}

			mod->xxs[smpnum].flg |= rs.loop & 0x03 ?  XMP_SAMPLE_LOOP : 0;
			mod->xxs[smpnum].flg |= rs.loop == 2 ? XMP_SAMPLE_LOOP_BIDIR : 0;

			D_(D_INFO "  [%1x] %05x%c%05x %05x %c "
						"V%02x F%+04d P%02x R%+03d",
				j, mod->xxs[mod->xxi[i].sub[j].sid].len,
				mod->xxs[mod->xxi[i].sub[j].sid].flg & XMP_SAMPLE_16BIT ? '+' : ' ',
				mod->xxs[mod->xxi[i].sub[j].sid].lps,
				mod->xxs[mod->xxi[i].sub[j].sid].lpe,
				mod->xxs[mod->xxi[i].sub[j].sid].flg & XMP_SAMPLE_LOOP_BIDIR ? 'B' :
				mod->xxs[mod->xxi[i].sub[j].sid].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
				mod->xxi[i].sub[j].vol, mod->xxi[i].sub[j].fin,
				mod->xxi[i].sub[j].pan, mod->xxi[i].sub[j].xpo);

			if (load_sample(m, f, SAMPLE_FLAG_DIFF,
			    &mod->xxs[mod->xxi[i].sub[j].sid], NULL) < 0) {
				return -1;
			}
		}
	}

	mod->smp = smpnum;
	mod->xxs = realloc(mod->xxs, sizeof (struct xmp_sample) * mod->smp);

	m->quirk |= QUIRKS_FT2;
	m->read_event_type = READ_EVENT_FT2;

	return 0;
}
