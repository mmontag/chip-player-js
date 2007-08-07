/* Real Tracker module loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: rtm_load.c,v 1.6 2007-08-07 22:10:29 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "period.h"
#include "rtm.h"

#define MAX_SAMP 1024


static int read_object_header(FILE *f, struct ObjectHeader *h)
{
	fread(&h->id, 4, 1, f);
	_D(_D_WARN "object id: %02x %02x %02x %02x", h->id[0],
					h->id[1], h->id[2], h->id[3]);
	h->rc = read8(f);
	if (h->rc != 0x20)
		return -1;
	fread(&h->name, 32, 1, f);
	h->eof = read8(f);
	h->version = read16l(f);
	h->headerSize = read16l(f);
	_D(_D_INFO "object %-4.4s (%d)", h->id, h->headerSize);
	
	return 0;
}


int rtm_load(FILE *f)
{
	int i, j, r;
	struct xxm_event *event;
	struct ObjectHeader oh;
	struct RTMMHeader rh;
	struct RTNDHeader rp;
	struct RTINHeader ri;
	struct RTSMHeader rs;
	int offset, smpnum;

	LOAD_INIT();

	read_object_header(f, &oh);
	if (memcmp(oh.id, "RTMM", 4))
		return -1;

	fread(&rh.software, 1, 20, f);
	fread(&rh.composer, 1, 32, f);
	rh.flags = read16l(f);	/* bit 0: linear table, bit 1: track names */
	rh.ntrack = read8(f);
	rh.ninstr = read8(f);
	rh.nposition = read16l(f);
	rh.npattern = read16l(f);
	rh.speed = read8(f);
	rh.tempo = read8(f);
	fread(&rh.panning, 32, 1, f);
	rh.extraDataSize = read32l(f);
	for (i = 0; i < rh.nposition; i++)
		xxo[i] = read16l(f);
	
	strncpy(xmp_ctl->name, oh.name, 20);
	snprintf(xmp_ctl->type, XMP_DEF_NAMESIZE, "Real Tracker Module %d.%02d",
		 oh.version >> 8, oh.version & 0xff);
	snprintf(tracker_name, 80, "%-20.20s", rh.software);

	xxh->len = rh.nposition;
	xxh->pat = rh.npattern;
	xxh->chn = rh.ntrack;
	xxh->trk = xxh->chn * xxh->pat + 1;
	xxh->ins = rh.ninstr;
	xxh->tpo = rh.speed;
	xxh->bpm = rh.tempo;
	xxh->flg = rh.flags & 0x01 ? XXM_FLG_LINEAR : 0;

	MODULE_INFO();

	for (i = 0; i < xxh->chn; i++)
		xxc[i].pan = rh.panning[i] & 0xff;

	PATTERN_INIT();

	if (V(0))
		report("Stored patterns: %d ", xxh->pat);

	offset = 42 + oh.headerSize + rh.extraDataSize;

	for (i = 0; i < xxh->pat; i++) {
		uint8 c;

		fseek(f, offset, SEEK_SET);

		read_object_header(f, &oh);
	
		rp.flags = read16l(f);
		rp.ntrack = read8(f);
		rp.nrows = read16l(f);
		rp.datasize = read32l(f);

		offset += 42 + oh.headerSize + rp.datasize;

		PATTERN_ALLOC(i);
		xxp[i]->rows = rp.nrows;
		TRACK_ALLOC(i);

		for (r = 0; r < rp.nrows; r++) {
			for (j = 0; j < rp.ntrack; j++) {
				event = &EVENT(i, j, r);
				c = read8(f);
				if (c == 0)		/* next row */
					break;
				if (c & 0x01) {		/* set track */
					j = read8(f);
					event = &EVENT(i, j, r);
				}
				if (c & 0x02)		/* read note */
					event->note = read8(f);
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

		if (V(0))
		report(".");
	}

	if (V(0))
		report("\n");

	/*
	 * load instruments
	 */

	if (V(0))
		report("Instruments    : %d ", xxh->ins);
	if (V(1))
		report("\n");

	fseek(f, offset, SEEK_SET);

	/* ESTIMATED value! We don't know the actual value at this point */
	xxh->smp = MAX_SAMP;

	INSTRUMENT_INIT();

	for (smpnum = i = 0; i < xxh->ins; i++) {
		read_object_header(f, &oh);

		copy_adjust(xxih[i].name, (uint8 *)&oh.name, 32);

		if (oh.headerSize == 0) {
			if (V(1) && strlen((char *)xxih[i].name)) {
				report("[%2X] %-26.26s %2d ", i, xxih[i].name,
								xxih[i].nsm);
			}
			ri.nsample = 0;
			if (V(1) && (strlen((char *)xxih[i].name) || xxih[i].nsm))
				report("\n");
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

		/* I don't see these inside the module */
		//ri.midiPort = read8(f);
		//ri.midiChannel = read8(f);
		//ri.midiProgram = read8(f);
		//ri.midiEnable = read8(f);

		xxih[i].nsm = ri.nsample;
		if (V(1) && (strlen((char *)xxih[i].name) || ri.nsample)) {
			report("[%2X] %-26.26s %2d ", i, xxih[i].name,
							xxih[i].nsm);
		}
		if (xxih[i].nsm > 16)
			xxih[i].nsm = 16;
		xxi[i] = calloc(sizeof (struct xxm_instrument), xxih[i].nsm);

		for (j = 0; j < 96; j++)
			xxim->ins[j] = ri.table[j + 12];

		/* Envelope */
		xxih[i].rls = ri.volfade;
		xxih[i].aei.npt = ri.volumeEnv.npoint;
		xxih[i].aei.sus = ri.volumeEnv.sustain;
		xxih[i].aei.lps = ri.volumeEnv.loopstart;
		xxih[i].aei.lpe = ri.volumeEnv.loopend;
		xxih[i].aei.flg = ri.volumeEnv.flags;
		xxih[i].pei.npt = ri.panningEnv.npoint;
		xxih[i].pei.sus = ri.panningEnv.sustain;
		xxih[i].pei.lps = ri.panningEnv.loopstart;
		xxih[i].pei.lpe = ri.panningEnv.loopend;
		xxih[i].pei.flg = ri.panningEnv.flags;
		if (xxih[i].aei.npt)
			xxae[i] = calloc(4, xxih[i].aei.npt);
		else
			xxih[i].aei.flg &= ~XXM_ENV_ON;
		if (xxih[i].pei.npt)
			xxpe[i] = calloc(4, xxih[i].pei.npt);
		else
			xxih[i].pei.flg &= ~XXM_ENV_ON;

		for (j = 0; j < xxih[i].aei.npt; j++) {
			xxae[i][j * 2 + 0] = ri.volumeEnv.point[j].x;
			xxae[i][j * 2 + 1] = ri.volumeEnv.point[j].y / 2;
		}
		for (j = 0; j < xxih[i].pei.npt; j++) {
			xxpe[i][j * 2 + 0] = ri.panningEnv.point[j].x;
			xxpe[i][j * 2 + 1] = 32 + ri.panningEnv.point[j].y / 2;
		}

		/* For each sample */
		for (j = 0; j < xxih[i].nsm; j++, smpnum++) {
			read_object_header(f, &oh);

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
					&xxi[i][0].xpo, &xxi[i][0].fin);
			xxi[i][j].xpo += 48 - rs.basenote;

			xxi[i][j].vol = rs.defaultvolume * rs.basevolume / 0x40;
			xxi[i][j].pan = 0x80 + rs.panning * 2;
			xxi[i][j].vwf = ri.vibflg;
			xxi[i][j].vde = ri.vibdepth;
			xxi[i][j].vra = ri.vibrate;
			xxi[i][j].vsw = ri.vibsweep;
			xxi[i][j].sid = smpnum;

			if (smpnum >= MAX_SAMP) {
				fseek(f, rs.length, SEEK_CUR);
				continue;
			}

			copy_adjust(xxs[smpnum].name, (uint8 *)oh.name, 32);

			xxs[smpnum].len = rs.length;
			xxs[smpnum].lps = rs.loopbegin;
			xxs[smpnum].lpe = rs.loopend;
			xxs[smpnum].flg = rs.flags & 0x02 ?  WAVE_16_BITS : 0;
			xxs[smpnum].flg |= rs.loop & 0x03 ?  WAVE_LOOPING : 0;
			xxs[smpnum].flg |= rs.loop == 2 ? WAVE_BIDIR_LOOP : 0;

			if ((V(1)) && rs.length) {
				report ("%s[%1x] %05x%c%05x %05x %c "
						"V%02x F%+04d P%02x R%+03d",
					j ? "\n\t\t\t\t    " : " ", j,
					xxs[xxi[i][j].sid].len,
					xxs[xxi[i][j].sid].flg & WAVE_16_BITS ? '+' : ' ',
					xxs[xxi[i][j].sid].lps,
					xxs[xxi[i][j].sid].lpe,
					xxs[xxi[i][j].sid].flg & WAVE_BIDIR_LOOP ? 'B' :
					xxs[xxi[i][j].sid].flg & WAVE_LOOPING ? 'L' : ' ',
					xxi[i][j].vol, xxi[i][j].fin,
					xxi[i][j].pan, xxi[i][j].xpo);

			}

			xmp_drv_loadpatch(f, xxi[i][j].sid, xmp_ctl->c4rate,
				XMP_SMP_DIFF, &xxs[xxi[i][j].sid], NULL);
		}
		if (xmp_ctl->verbose == 1)
			report (".");

		if ((V(1)) && (strlen((char *) xxih[i].name) || ri.nsample))
			report ("\n");
	}

	xxh->smp = smpnum;
	xxs = realloc (xxs, sizeof (struct xxm_sample) * xxh->smp);

	xmp_ctl->fetch |= XMP_MODE_FT2;

	return 0;
}
