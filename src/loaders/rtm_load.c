/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "loader.h"
#include "period.h"
#include "rtm.h"


static int rtm_test(FILE *, char *, const int);
static int rtm_load (struct module_data *, FILE *, const int);

const struct format_loader rtm_loader = {
	"Real Tracker (RTM)",
	rtm_test,
	rtm_load
};

static int rtm_test(FILE *f, char *t, const int start)
{
	char buf[4];

	if (fread(buf, 1, 4, f) < 4)
		return -1;
	if (memcmp(buf, "RTMM", 4))
		return -1;

	if (read8(f) != 0x20)
		return -1;

	read_title(f, t, 32);

	return 0;
}


#define MAX_SAMP 1024

static int read_object_header(FILE *f, struct ObjectHeader *h, char *id)
{
	fread(&h->id, 4, 1, f);
	D_(D_WARN "object id: %02x %02x %02x %02x", h->id[0],
					h->id[1], h->id[2], h->id[3]);

	if (memcmp(id, h->id, 4))
		return -1;

	h->rc = read8(f);
	if (h->rc != 0x20)
		return -1;
	fread(&h->name, 32, 1, f);
	h->eof = read8(f);
	h->version = read16l(f);
	h->headerSize = read16l(f);
	D_(D_INFO "object %-4.4s (%d)", h->id, h->headerSize);
	
	return 0;
}


static int rtm_load(struct module_data *m, FILE *f, const int start)
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

	fread(tracker_name, 1, 20, f);
	tracker_name[20] = 0;
	fread(composer, 1, 32, f);
	composer[32] = 0;
	rh.flags = read16l(f);	/* bit 0: linear table, bit 1: track names */
	rh.ntrack = read8(f);
	rh.ninstr = read8(f);
	rh.nposition = read16l(f);
	rh.npattern = read16l(f);
	rh.speed = read8(f);
	rh.tempo = read8(f);
	fread(&rh.panning, 32, 1, f);
	rh.extraDataSize = read32l(f);

	if (version >= 0x0112)
		fseek(f, 32, SEEK_CUR);		/* skip original name */

	for (i = 0; i < rh.nposition; i++)
		mod->xxo[i] = read16l(f);
	
	strncpy(mod->name, oh.name, 20);
	snprintf(mod->type, XMP_NAME_SIZE, "%s RTM %x.%02x",
				tracker_name, version >> 8, version & 0xff);
	/* strncpy(m->author, composer, XMP_NAME_SIZE); */

	mod->len = rh.nposition;
	mod->pat = rh.npattern;
	mod->chn = rh.ntrack;
	mod->trk = mod->chn * mod->pat + 1;
	mod->ins = rh.ninstr;
	mod->spd = rh.speed;
	mod->bpm = rh.tempo;

	m->quirk |= rh.flags & 0x01 ? QUIRK_LINEAR : 0;

	MODULE_INFO();

	for (i = 0; i < mod->chn; i++)
		mod->xxc[i].pan = rh.panning[i] & 0xff;

	PATTERN_INIT();

	D_(D_INFO "Stored patterns: %d", mod->pat);

	offset = 42 + oh.headerSize + rh.extraDataSize;

	for (i = 0; i < mod->pat; i++) {
		uint8 c;

		fseek(f, start + offset, SEEK_SET);

		if (read_object_header(f, &oh, "RTND") < 0) {
			D_(D_CRIT "Error reading pattern %d", i);
			return -1;
		}
	
		rp.flags = read16l(f);
		rp.ntrack = read8(f);
		rp.nrows = read16l(f);
		rp.datasize = read32l(f);

		offset += 42 + oh.headerSize + rp.datasize;

		PATTERN_ALLOC(i);
		mod->xxp[i]->rows = rp.nrows;
		TRACK_ALLOC(i);

		for (r = 0; r < rp.nrows; r++) {
			for (j = 0; /*j < rp.ntrack */; j++) {

				c = read8(f);
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
					j = read8(f);
					event = &EVENT(i, j, r);
				}
				if (c & 0x02) {		/* read note */
					event->note = read8(f) + 1;
					if (event->note == 0xff) {
						event->note = XMP_KEY_OFF;
					} else {
						event->note += 12;
					}
				}
				if (c & 0x04)		/* read instrument */
					event->ins = read8(f);
				if (c & 0x08)		/* read effect */
					event->fxt = read8(f);
				if (c & 0x10)		/* read parameter */
					event->fxp = read8(f);
				if (c & 0x20)		/* read effect 2 */
					event->f2t = read8(f);
				if (c & 0x40)		/* read parameter 2 */
					event->f2p = read8(f);
			}
		}
	}

	/*
	 * load instruments
	 */

	D_(D_INFO "Instruments: %d", mod->ins);

	fseek(f, start + offset, SEEK_SET);

	/* ESTIMATED value! We don't know the actual value at this point */
	mod->smp = MAX_SAMP;

	INSTRUMENT_INIT();

	smpnum = 0;
	for (i = 0; i < mod->ins; i++) {
		if (read_object_header(f, &oh, "RTIN") < 0) {
			D_(D_CRIT "Error reading instrument %d", i);
			return -1;
		}

		copy_adjust(mod->xxi[i].name, (uint8 *)&oh.name, 32);

		if (oh.headerSize == 0) {
			D_(D_INFO "[%2X] %-26.26s %2d ", i,
					mod->xxi[i].name, mod->xxi[i].nsm);
			ri.nsample = 0;
			continue;
		}

		ri.nsample = read8(f);
		ri.flags = read16l(f);	/* bit 0 : default panning enabled */
		fread(&ri.table, 120, 1, f);

		ri.volumeEnv.npoint = read8(f);
		for (j = 0; j < 12; j++) {
			ri.volumeEnv.point[j].x = read32l(f);
			ri.volumeEnv.point[j].y = read32l(f);
		}
		ri.volumeEnv.sustain = read8(f);
		ri.volumeEnv.loopstart = read8(f);
		ri.volumeEnv.loopend = read8(f);
		ri.volumeEnv.flags = read16l(f); /* bit 0:enable 1:sus 2:loop */
		
		ri.panningEnv.npoint = read8(f);
		for (j = 0; j < 12; j++) {
			ri.panningEnv.point[j].x = read32l(f);
			ri.panningEnv.point[j].y = read32l(f);
		}
		ri.panningEnv.sustain = read8(f);
		ri.panningEnv.loopstart = read8(f);
		ri.panningEnv.loopend = read8(f);
		ri.panningEnv.flags = read16l(f);

		ri.vibflg = read8(f);
		ri.vibsweep = read8(f);
		ri.vibdepth = read8(f);
		ri.vibrate = read8(f);
		ri.volfade = read16l(f);

		if (version >= 0x0110) {
			ri.midiPort = read8(f);
			ri.midiChannel = read8(f);
			ri.midiProgram = read8(f);
			ri.midiEnable = read8(f);
		}
		if (version >= 0x0112) {
			ri.midiTranspose = read8(f);
			ri.midiBenderRange = read8(f);
			ri.midiBaseVolume = read8(f);
			ri.midiUseVelocity = read8(f);
		}

		mod->xxi[i].nsm = ri.nsample;

		D_(D_INFO "[%2X] %-26.26s %2d", i, mod->xxi[i].name,
							mod->xxi[i].nsm);

		if (mod->xxi[i].nsm > 16)
			mod->xxi[i].nsm = 16;
		mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), mod->xxi[i].nsm);

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

			rs.flags = read16l(f);
			rs.basevolume = read8(f);
			rs.defaultvolume = read8(f);
			rs.length = read32l(f);
			rs.loop = read32l(f);
			rs.loopbegin = read32l(f);
			rs.loopend = read32l(f);
			rs.basefreq = read32l(f);
			rs.basenote = read8(f);
			rs.panning = read8(f);

			c2spd_to_note(rs.basefreq,
					&mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);
			mod->xxi[i].sub[j].xpo += 48 - rs.basenote;

			mod->xxi[i].sub[j].vol = rs.defaultvolume * rs.basevolume / 0x40;
			mod->xxi[i].sub[j].pan = 0x80 + rs.panning * 2;
			mod->xxi[i].sub[j].vwf = ri.vibflg;
			mod->xxi[i].sub[j].vde = ri.vibdepth;
			mod->xxi[i].sub[j].vra = ri.vibrate;
			mod->xxi[i].sub[j].vsw = ri.vibsweep;
			mod->xxi[i].sub[j].sid = smpnum;

			if (smpnum >= MAX_SAMP) {
				fseek(f, rs.length, SEEK_CUR);
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

			load_sample(f, SAMPLE_FLAG_DIFF,
				&mod->xxs[mod->xxi[i].sub[j].sid], NULL);
		}
	}

	mod->smp = smpnum;
	mod->xxs = realloc(mod->xxs, sizeof (struct xmp_sample) * mod->smp);

	m->quirk |= QUIRKS_FT2;
	m->read_event_type = READ_EVENT_FT2;

	return 0;
}
