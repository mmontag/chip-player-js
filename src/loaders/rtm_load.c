/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: rtm_load.c,v 1.2 2007-08-07 01:42:09 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "rtm.h"

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
	//int instr_no = 0;
	//uint8 *patbuf, *p, b;
	struct xxm_event *event;
	struct ObjectHeader oh;
	struct RTMMHeader rh;
	struct RTNDHeader rp;
	int pattern_offset;

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
		xxo[i] = read8(f);
	
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

	pattern_offset = 42 + oh.headerSize + rh.extraDataSize;

	for (i = 0; i < xxh->pat; i++) {
		uint8 c;

		fseek(f, pattern_offset, SEEK_SET);

		read_object_header(f, &oh);
	
		rp.flags = read16l(f);
		rp.ntrack = read8(f);
		rp.nrows = read16l(f);
		rp.datasize = read32l(f);

		pattern_offset += 42 + oh.headerSize + rp.datasize;

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

	if (V(0))
		report("Instruments    : %d ", xxh->ins);
	if (V(1))
		report("\n");

#if 0
	/* ESTIMATED value! We don't know the actual value at this point */
	xxh->smp = MAX_SAMP;

	INSTRUMENT_INIT();

	for (i = 0; i < xxh->ins; i++) {
		xih.size = read32l(f);	/* Instrument size */
		fread(&xih.name, 22, 1, f);	/* Instrument name */
		xih.type = read8(f);	/* Instrument type (always 0) */
		xih.samples = read16l(f);	/* Number of samples */
		xih.sh_size = read32l(f);	/* Sample header size */

		copy_adjust(xxih[i].name, xih.name, 22);

		xxih[i].nsm = xih.samples;
		if (xxih[i].nsm > 16)
			xxih[i].nsm = 16;

		if (V(1) && (strlen((char *)xxih[i].name) || xxih[i].nsm))
			report("[%2X] %-22.22s %2d ", i, xxih[i].name,
			       xxih[i].nsm);

		if (xxih[i].nsm) {
			xxi[i] =
			    calloc(sizeof(struct xxm_instrument), xxih[i].nsm);

			fread(&xi.sample, 96, 1, f);	/* Sample map */
			for (j = 0; j < 24; j++)
				xi.v_env[j] = read16l(f);	/* Points for volume envelope */
			for (j = 0; j < 24; j++)
				xi.p_env[j] = read16l(f);	/* Points for pan envelope */
			xi.v_pts = read8(f);	/* Number of volume points */
			xi.p_pts = read8(f);	/* Number of pan points */
			xi.v_sus = read8(f);	/* Volume sustain point */
			xi.v_start = read8(f);	/* Volume loop start point */
			xi.v_end = read8(f);	/* Volume loop end point */
			xi.p_sus = read8(f);	/* Pan sustain point */
			xi.p_start = read8(f);	/* Pan loop start point */
			xi.p_end = read8(f);	/* Pan loop end point */
			xi.v_type = read8(f);	/* Bit 0:On 1:Sustain 2:Loop */
			xi.p_type = read8(f);	/* Bit 0:On 1:Sustain 2:Loop */
			xi.y_wave = read8(f);	/* Vibrato waveform */
			xi.y_sweep = read8(f);	/* Vibrato sweep */
			xi.y_depth = read8(f);	/* Vibrato depth */
			xi.y_rate = read8(f);	/* Vibrato rate */
			xi.v_fade = read16l(f);	/* Volume fadeout */

			/* Skip reserved space */
			fseek(f,
			      xih.size - 33 /*sizeof (xih) */  -
			      208 /*sizeof (xi) */ , SEEK_CUR);

			/* Envelope */
			xxih[i].rls = xi.v_fade;
			xxih[i].aei.npt = xi.v_pts;
			xxih[i].aei.sus = xi.v_sus;
			xxih[i].aei.lps = xi.v_start;
			xxih[i].aei.lpe = xi.v_end;
			xxih[i].aei.flg = xi.v_type;
			xxih[i].pei.npt = xi.p_pts;
			xxih[i].pei.sus = xi.p_sus;
			xxih[i].pei.lps = xi.p_start;
			xxih[i].pei.lpe = xi.p_end;
			xxih[i].pei.flg = xi.p_type;
			if (xxih[i].aei.npt)
				xxae[i] = calloc(4, xxih[i].aei.npt);
			else
				xxih[i].aei.flg &= ~XXM_ENV_ON;
			if (xxih[i].pei.npt)
				xxpe[i] = calloc(4, xxih[i].pei.npt);
			else
				xxih[i].pei.flg &= ~XXM_ENV_ON;
			memcpy(xxae[i], xi.v_env, xxih[i].aei.npt * 4);
			memcpy(xxpe[i], xi.p_env, xxih[i].pei.npt * 4);

			memcpy(&xxim[i], xi.sample, 96);
			for (j = 0; j < 96; j++) {
				if (xxim[i].ins[j] >= xxih[i].nsm)
					xxim[i].ins[j] = (uint8) XMP_DEF_MAXPAT;
			}

			for (j = 0; j < xxih[i].nsm; j++, instr_no++) {

				xsh[j].length = read32l(f);	/* Sample length */
				xsh[j].loop_start = read32l(f);	/* Sample loop start */
				xsh[j].loop_length = read32l(f);	/* Sample loop length */
				xsh[j].volume = read8(f);	/* Volume */
				xsh[j].finetune = read8s(f);	/* Finetune (-128..+127) */
				xsh[j].type = read8(f);	/* Flags */
				xsh[j].pan = read8(f);	/* Panning (0-255) */
				xsh[j].relnote = read8s(f);	/* Relative note number */
				xsh[j].reserved = read8(f);
				fread(&xsh[j].name, 22, 1, f);	/* Sample_name */

				xxi[i][j].vol = xsh[j].volume;
				xxi[i][j].pan = xsh[j].pan;
				xxi[i][j].xpo = xsh[j].relnote;
				xxi[i][j].fin = xsh[j].finetune;
				xxi[i][j].vwf = xi.y_wave;
				xxi[i][j].vde = xi.y_depth;
				xxi[i][j].vra = xi.y_rate;
				xxi[i][j].vsw = xi.y_sweep;
				xxi[i][j].sid = instr_no;
				if (instr_no >= MAX_SAMP)
					continue;

				copy_adjust(xxs[instr_no].name, xsh[j].name,
					    22);

				xxs[instr_no].len = xsh[j].length;
				xxs[instr_no].lps = xsh[j].loop_start;
				xxs[instr_no].lpe =
				    xsh[j].loop_start + xsh[j].loop_length;
				xxs[instr_no].flg =
				    xsh[j].
				    type & XM_SAMPLE_16BIT ? WAVE_16_BITS : 0;
				xxs[instr_no].flg |=
				    xsh[j].
				    type & XM_LOOP_FORWARD ? WAVE_LOOPING : 0;
				xxs[instr_no].flg |=
				    xsh[j].
				    type & XM_LOOP_PINGPONG ? WAVE_LOOPING |
				    WAVE_BIDIR_LOOP : 0;
			}
			for (j = 0; j < xxih[i].nsm; j++) {
				if (instr_no >= MAX_SAMP)
					continue;
				if ((V(1)) && xsh[j].length)
					report("%s[%1x] %05x%c%05x %05x %c "
					       "V%02x F%+04d P%02x R%+03d",
					       j ? "\n\t\t\t\t" : "\t", j,
					       xxs[xxi[i][j].sid].len,
					       xxs[xxi[i][j].sid].
					       flg & WAVE_16_BITS ? '+' : ' ',
					       xxs[xxi[i][j].sid].lps,
					       xxs[xxi[i][j].sid].lpe,
					       xxs[xxi[i][j].sid].
					       flg & WAVE_BIDIR_LOOP ? 'B' :
					       xxs[xxi[i][j].sid].
					       flg & WAVE_LOOPING ? 'L' : ' ',
					       xsh[j].volume, xsh[j].finetune,
					       xsh[j].pan, xsh[j].relnote);

				if (xfh.version > 0x0103)
					xmp_drv_loadpatch(f, xxi[i][j].sid,
							  xmp_ctl->c4rate,
							  XMP_SMP_DIFF,
							  &xxs[xxi[i][j].sid],
							  NULL);
			}
			if (xmp_ctl->verbose == 1)
				report(".");
		} else {
			fseek(f, xih.size - 33 /*sizeof (xih) */ , SEEK_CUR);
		}

		if ((V(1)) && (strlen((char *)xxih[i].name) || xih.samples))
			report("\n");
	}
	xxh->smp = instr_no;
	xxs = realloc(xxs, sizeof(struct xxm_sample) * xxh->smp);

	if (xfh.version <= 0x0103) {
		if (xmp_ctl->verbose > 0 && xmp_ctl->verbose < 2)
			report("\n");
		goto load_patterns;
	}
      load_samples:
	if ((V(0) && xfh.version <= 0x0103) || V(1))
		report("Stored samples : %d ", xxh->smp);

	/* XM 1.02 stores all samples after the patterns */

	if (xfh.version <= 0x0103) {
		for (i = 0; i < xxh->ins; i++) {
			for (j = 0; j < xxih[i].nsm; j++) {
				xmp_drv_loadpatch(f, xxi[i][j].sid,
						  xmp_ctl->c4rate, XMP_SMP_DIFF,
						  &xxs[xxi[i][j].sid], NULL);
				if (V(0))
					report(".");
			}
		}
	}
	if (V(0))
		report("\n");

	/* If dynamic pan is disabled, XM modules will use the standard
	 * MOD channel panning (LRRL). Moved to module_play () --Hipolito.
	 */


	xmp_ctl->fetch |= XMP_MODE_FT2;
#endif

	return 0;
}
