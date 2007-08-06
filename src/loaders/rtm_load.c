/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: rtm_load.c,v 1.1 2007-08-06 22:22:20 cmatsuoka Exp $
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
	h->rc = read8(f);
	if (h->rc != 0x20)
		return -1;
	fread(&h->name, 32, 1, f);
	h->eof = read8(f);
	h->version = read16l(f);
	h->headerSize = read16l(f);
	
	return 0;
}


int rtm_load(FILE *f)
{
	int i, j, r;
	int instr_no = 0;
	uint8 *patbuf, *p, b;
	struct xxm_event *event;
	struct ObjectHeader oh;
	struct RTMMHeader rh;

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

	fseek(f, oh.headerSize + rh.extraDataSize, SEEK_SET);

	read_object_header(f, &oh);
	
#if 0
	for (i = 0; i < xxh->pat; i++) {
		xph.length = read32l(f);
		xph.packing = read8(f);
		xph.rows = xfh.version > 0x0102 ? read16l(f) : read8(f) + 1;
		xph.datasize = read16l(f);

		PATTERN_ALLOC(i);
		if (!(r = xxp[i]->rows = xph.rows))
			r = xxp[i]->rows = 0x100;
		TRACK_ALLOC(i);

		if (xph.datasize) {
			p = patbuf = calloc(1, xph.datasize);
			fread(patbuf, 1, xph.datasize, f);
			for (j = 0; j < (xxh->chn * r); j++) {
				if ((p - patbuf) >= xph.datasize)
					break;
				event = &EVENT(i, j % xxh->chn, j / xxh->chn);
				if ((b = *p++) & XM_EVENT_PACKING) {
					if (b & XM_EVENT_NOTE_FOLLOWS)
						event->note = *p++;
					if (b & XM_EVENT_INSTRUMENT_FOLLOWS) {
						if (*p & XM_END_OF_SONG)
							break;
						event->ins = *p++;
					}
					if (b & XM_EVENT_VOLUME_FOLLOWS)
						event->vol = *p++;
					if (b & XM_EVENT_FXTYPE_FOLLOWS) {
						event->fxt = *p++;
#if 0
						if (event->fxt == FX_GLOBALVOL)
							event->fxt = FX_TRK_VOL;
						if (event->fxt == FX_G_VOLSLIDE)
							event->fxt =
							    FX_TRK_VSLIDE;
#endif
					}
					if (b & XM_EVENT_FXPARM_FOLLOWS)
						event->fxp = *p++;
				} else {
					event->note = b;
					event->ins = *p++;
					event->vol = *p++;
					event->fxt = *p++;
					event->fxp = *p++;
				}
				if (!event->vol)
					continue;

				/* Volume set */
				if ((event->vol >= 0x10)
				    && (event->vol <= 0x50)) {
					event->vol -= 0x0f;
					continue;
				}
				/* Volume column effects */
				switch (event->vol >> 4) {
				case 0x06:	/* Volume slide down */
					event->f2t = FX_VOLSLIDE_2;
					event->f2p = event->vol - 0x60;
					break;
				case 0x07:	/* Volume slide up */
					event->f2t = FX_VOLSLIDE_2;
					event->f2p = (event->vol - 0x70) << 4;
					break;
				case 0x08:	/* Fine volume slide down */
					event->f2t = FX_EXTENDED;
					event->f2p =
					    (EX_F_VSLIDE_DN << 4) | (event->
								     vol -
								     0x80);
					break;
				case 0x09:	/* Fine volume slide up */
					event->f2t = FX_EXTENDED;
					event->f2p =
					    (EX_F_VSLIDE_UP << 4) | (event->
								     vol -
								     0x90);
					break;
				case 0x0a:	/* Set vibrato speed */
					event->f2t = FX_VIBRATO;
					event->f2p = (event->vol - 0xa0) << 4;
					break;
				case 0x0b:	/* Vibrato */
					event->f2t = FX_VIBRATO;
					event->f2p = event->vol - 0xb0;
					break;
				case 0x0c:	/* Set panning */
					event->f2t = FX_SETPAN;
					event->f2p =
					    ((event->vol - 0xc0) << 4) + 8;
					break;
				case 0x0d:	/* Pan slide left */
					event->f2t = FX_PANSLIDE;
					event->f2p = (event->vol - 0xd0) << 4;
					break;
				case 0x0e:	/* Pan slide right */
					event->f2t = FX_PANSLIDE;
					event->f2p = event->vol - 0xe0;
					break;
				case 0x0f:	/* Tone portamento */
					event->f2t = FX_TONEPORTA;
					event->f2p = (event->vol - 0xf0) << 4;
					break;
				}
				event->vol = 0;
			}
			free(patbuf);
			if (V(0))
				report(".");
		}
	}

	PATTERN_ALLOC(i);

	xxp[i]->rows = 64;
	xxt[i * xxh->chn] = calloc(1, sizeof(struct xxm_track) +
				   sizeof(struct xxm_event) * 64);
	xxt[i * xxh->chn]->rows = 64;
	for (j = 0; j < xxh->chn; j++)
		xxp[i]->info[j].index = i * xxh->chn;

	if (xfh.version <= 0x0103) {
		if (V(0))
			report("\n");
		goto load_samples;
	}
	if (V(0))
		report("\n");

      load_instruments:
	if (V(0))
		report("Instruments    : %d ", xxh->ins);
	if (V(1))
		report("\n");

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
