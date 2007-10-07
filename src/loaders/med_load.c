/* Extended Module Player
 * Copyright (C) 1996-2001 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: med_load.c,v 1.3 2007-10-07 14:14:27 cmatsuoka Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "med.h"
#include "load.h"

#define NUM_INST_TYPES 9
static char *inst_type[] = {
	"HYB",			/* -2 */
	"SYN",			/* -1 */
	"SMP",			/*  0 */
	"I5O",			/*  1 */
	"I3O",			/*  2 */
	"I2O",			/*  3 */
	"I4O",			/*  4 */
	"I6O",			/*  5 */
	"I7O",			/*  6 */
	"EXT",			/*  7 */
};

static int bpmon, bpmlen;

static void xlat_fx(uint8 * fxt, uint8 * fxp)
{
	switch (*fxt) {
	case 0x05:		/* Old vibrato */
		*fxp = (LSN(*fxp) << 4) | MSN(*fxp);
		break;
	case 0x06:		/* Not defined in MED 3.2 */
	case 0x07:		/* Not defined in MED 3.2 */
		break;
	case 0x08:		/* Set hold/decay */
		break;
	case 0x09:		/* Set secondary tempo */
		*fxt = 0x0f;
		break;
	case 0x0d:		/* Volume slide */
		*fxt = 0x0a;
		break;
	case 0x0e:		/* Synth JMP */
		break;
	case 0x0f:
		if (*fxp == 0x00) {	/* Jump to next block */
			*fxt = 0x0d;
			break;
		} else if (*fxp <= 0x0a) {
			break;
		} else if (*fxp <= 0xf0) {
			if (*fxp < 0x21)
				*fxp = 0x21;
			break;
		}
		switch (*fxp) {
		case 0xf1:	/* Play note twice */
			break;
		case 0xf2:	/* Delay note */
			break;
		case 0xf3:	/* Play note three times */
			break;
		case 0xf8:	/* Turn filter off */
		case 0xf9:	/* Turn filter on */
		case 0xfa:	/* MIDI pedal on */
		case 0xfb:	/* MIDI pedal off */
			*fxt = *fxp = 0;
			break;
		case 0xfd:	/* Set pitch */
			*fxt = *fxp = 0;
			break;
		case 0xfe:	/* End of song */
			*fxt = *fxp = 0;
			break;
		case 0xff:	/* Note cut */
			*fxt = *fxp = 0;
			break;
		}
		break;
	}
}

void bytecopy(void *dst, void *src, int size)
{

	while (size-- > 0)
		*(char *)dst++ = *(char *)src++;
}

int med_load(FILE * f)
{
	int i, j, k;
	char *mmd, m[4];
	struct MMD0 header;
	//struct MMD0song song;
	struct MMD2song song;
	struct MMD0Block *block0;
	struct MMD1Block *block1;
	struct PlaySeq **playseq;
	struct InstrHdr instr;
	struct SynthInstr *synth;
	struct SynthWF *synthwf;
	struct MMD0exp expdata;
	struct InstrExt *instrext = NULL;
	struct xxm_event *event;
	int ver = 0;
	int smp_idx = 0;
	uint8 *e;
	int song_offset;
	int blockarr_offset;
	int smplarr_offset;
	int expdata_offset;
	int playseqtable_offset;

	LOAD_INIT();

	fread(&m, 1, 4, f);
	fseek(f, -4, SEEK_CUR);
	if (m[0] != 'M' || m[1] != 'M' || m[2] != 'D' ||
	    (m[3] != '0' && m[3] != '1' && m[3] != '2' && m[3] != '3'))
		return -1;

	ver = m[3] - '1' + 1;

	if (ver > 3) {
		report("MMD%c format currently unsupported.\n", m[3]);
		return -1;
	}

	header.modlen = read32b(f);
	song_offset = read32b(f);
	header.psecnum = read16b(f);
	header.pseq = read16b(f);
	blockarr_offset = read32b(f);
	read8(f);
	smplarr_offset = read32b(f);
	read32l(f);
	expdata_offset = read32b(f);
	read32l(f);
	header.pstate = read16b(f);
	header.pblock = read16b(f);
	header.pline = read16b(f);
	header.pseqnum = read16b(f);
	header.actplayline = read16b(f);
	header.counter = read8(f);
	header.extra_songs = read8(f);

	/*
	 * song structure
	 */
	fseek(f, song_offset, SEEK_SET);
	for (i = 0; i < 64; i++) {
		song.sample[i].rep = read16b(f);
		song.sample[i].replen = read16b(f);
		song.sample[i].midich = read8(f);
		song.sample[i].midipreset = read8(f);
		song.sample[i].midisvol = read8(f);
		song.sample[i].strans = read8(f);
	}
	song.numblocks = read16b(f);
	song.songlen = read16b(f);
	playseqtable_offset = read32(f);
	song.deftempo = read16b(f);
	song.playtransp = read8(f);
	song.flags = read8(f);
	song.flags2 = read8(f);
	song.tempo2 = read8(f);
	for (i = 0; i < 16; i++)
		song.trkvol[i] = read8(f);
	song.mastervol = read8(f);
	song.numsamples = read8(f);

	/*
	 * convert header
	 */
	xmp_ctl->c4rate = C4_NTSC_RATE;
	xmp_ctl->fetch |= song.flags & FLAG_STSLIDE ? 0 : XMP_CTL_VSALL;
	bpmon = song.flags2 & FLAG2_BPM;
	bpmlen = 1 + (song.flags2 & FLAG2_BMASK);
	xmp_ctl->fetch |= bpmon ? 0 : XMP_CTL_MEDBPM;

#warning FIXME: med tempos are incorrectly handled
	xxh->tpo = song.tempo2;
	xxh->bpm = bpmon ? song.deftempo * bpmlen / 4 : song.deftempo;
	if (!bpmon && xxh->bpm <= 10)
		xxh->bpm = xxh->bpm * 33 / 6;
	xxh->pat = song.numblocks;
	xxh->ins = song.numsamples;
	if (ver < 2)
		xxh->len = song.songlen;
	xxh->rst = 0;
	xxh->chn = 0;

	/*
	 * load instruments
	 */
	for (i = 0; i < song.numsamples; i++) {
		int smpl_offset;
		fseek(f, smplarr_offset + i * 4, SEEK_SET);
		smpl_offset = read32b(f);
		if (!smpl_offset)
			continue;
		fseek(f, smpl_offset, SEEK_SET);
		instr.length = read32b(f);
		instr.type = read16b(f);
		if (instr.type != 0)
			continue;
	}

#if 0
	/* We can have more samples than instruments -- each synth instrument
	 * can have up to 64 samples!
	 */
	for (i = 0; i < xxh->ins; i++) {
		bytecopy(&instr, mmd + (uint32) header->smplarr + i * 4, 4);
		B_ENDIAN32((uint32) instr);
		if (!instr)
			continue;
		instr = (struct InstrHdr *)(mmd + (uint32) instr);
		synth = (struct SynthInstr *)instr;

		switch (instr->type) {
		case -1:	/* Synth */
			xxh->smp += synth->wforms;
			break;
		case -2:	/* Hybrid */
		case 0:	/* Sample */
			xxh->smp++;
			break;
		case 1:	/* IFF5OCT */
		case 2:	/* IFF3OCT */
		case 3:	/* IFF2OCT */
		case 4:	/* IFF4OCT */
		case 5:	/* IFF6OCT */
		case 6:	/* IFF7OCT */
			break;
		}
	}
#endif

	/*
	 * sequence
	 */
	if (ver < 2) {
		memcpy(xxo, song.playseq, xxh->len);
	} else {

		playseq =
		    (struct PlaySeq **)(mmd + (uint32) song2->playseqtable);
		B_ENDIAN32((uint32) playseq[0]);
		playseq[0] = (struct PlaySeq *)(mmd + (uint32) playseq[0]);
		B_ENDIAN16((uint16) playseq[0]->length);
		xxh->len = playseq[0]->length;
		if (xxh->len > 0xff)
			xxh->len = 0xff;
		for (i = 0; i < xxh->len; i++) {
			B_ENDIAN16((uint16) playseq[0]->seq[i]);
			xxo[i] = playseq[0]->seq[i];
		}
	}

	/* Quickly scan patterns to check the number of channels */

	for (i = 0; i < xxh->pat; i++) {
		bytecopy(&block0, mmd + (uint32) header->blockarr + i * 4, 4);
		B_ENDIAN32((uint32) block0);
		block0 = (struct MMD0Block *)(mmd + (uint32) block0);
		block1 = (struct MMD1Block *)block0;

		if (ver > 0) {
			B_ENDIAN16(block1->numtracks);
			B_ENDIAN16(block1->lines);

			if (block1->numtracks > xxh->chn)
				xxh->chn = block1->numtracks;
		} else {
			if (block0->numtracks > xxh->chn)
				xxh->chn = block0->numtracks;
		}
	}

	xxh->trk = xxh->pat * xxh->chn;

	if (expdata && expdata->songname)
		strncpy(xmp_ctl->name, mmd + (uint32) expdata->songname,
			XMP_DEF_NAMESIZE);
	sprintf(xmp_ctl->type, "MMD%c (MED)", m[3]);
	MODULE_INFO();

	if (V(0)) {
		report("BPM mode       : %s", bpmon ? "on" : "off");
		if (bpmon)
			report(" (length = %d)", bpmlen);
		report("\n");
		if (song->playtransp)
			report("Song transpose : %d semitones\n",
			       song->playtransp);
		report("Stored patterns: %d ", xxh->pat);
	}

	/* Read and convert patterns */

	PATTERN_INIT();

	for (i = 0; i < xxh->pat; i++) {
		bytecopy(&block0, mmd + (uint32) header->blockarr + i * 4, 4);
		B_ENDIAN32((uint32) block0);
		block0 = (struct MMD0Block *)(mmd + (uint32) block0);
		block1 = (struct MMD1Block *)block0;

		PATTERN_ALLOC(i);

		if (ver > 0) {
			xxp[i]->rows = block1->lines + 1;
			TRACK_ALLOC(i);
			e = (uint8 *) block1 + sizeof(struct MMD1Block);
			for (j = 0; j < xxp[i]->rows; j++) {
				for (k = 0; k < block1->numtracks; k++, e += 4) {
					event = &EVENT(i, k, j);
					event->note = e[0] & 0x7f;
					if (event->note)
						event->note +=
						    36 + song->playtransp;
					event->ins = e[1] & 0x3f;
					event->fxt = e[2];
					event->fxp = e[3];
					xlat_fx(&event->fxt, &event->fxp);
				}
			}
		} else {
			xxp[i]->rows = block0->lines + 1;
			TRACK_ALLOC(i);
			e = (uint8 *) block0 + sizeof(struct MMD0Block);
			for (j = 0; j < xxp[i]->rows; j++) {
				for (k = 0; k < block0->numtracks; k++, e += 3) {
					event = &EVENT(i, k, j);
					if ((event->note = e[0] & 0x3f))
						event->note += 36;
					event->ins =
					    (e[1] >> 4) | ((e[0] & 0x80) >> 3) |
					    ((e[0] & 0x40) >> 1);
					event->fxt = e[1] & 0x0f;
					event->fxp = e[2];
					xlat_fx(&event->fxt, &event->fxp);
				}
			}
		}

		if (V(0))
			report(".");
	}
	if (V(0))
		report("\n");

	INSTRUMENT_INIT();

	med_vol_table = calloc(sizeof(uint8 *), xxh->ins);
	med_wav_table = calloc(sizeof(uint8 *), xxh->ins);

	/* Read and convert instruments and samples */

	if (V(0))
		report("Instruments    : %d ", xxh->ins);

	if (V(1))
		report("\n     Instrument name                          "
		       "Typ Len   LBeg  LEnd  Vl Xp Ft");

	for (smp_idx = i = 0; i < xxh->ins; i++) {
		bytecopy(&instr, mmd + (uint32) header->smplarr + i * 4, 4);
		B_ENDIAN32((uint32) instr);
		if (!instr)
			continue;
		instr = (struct InstrHdr *)(mmd + (uint32) instr);
		synth = (struct SynthInstr *)instr;

		B_ENDIAN32(instr->length);
		B_ENDIAN16(instr->type);

		if (V(1)) {
			report("\n[%2x] %-40.40s ", i,
			       expdata
			       && expdata->iinfo ? mmd +
			       (uint32) expdata->iinfo +
			       expdata->i_ext_entrsz * i : "");
			report("%s ",
			       instr->type + 2 <=
			       NUM_INST_TYPES ? inst_type[instr->type +
							  2] : "???");
		}

		if (expdata)
			instrext =
			    (struct InstrExt *)(mmd +
						(uint32) expdata->exp_smp);

		switch (instr->type) {
		case -2:	/* Hybrid */
			B_ENDIAN16(synth->rep);
			B_ENDIAN16(synth->replen);
			B_ENDIAN16(synth->voltbllen);
			B_ENDIAN16(synth->wftbllen);
			B_ENDIAN16(synth->wforms);

			if (expdata)
				instrext =
				    (struct InstrExt *)(mmd +
							(uint32) expdata->
							exp_smp);

			xxi[i] = calloc(sizeof(struct xxm_instrument), 1);
			xxih[i].nsm = 1;
			xxih[i].vts = synth->volspeed;
			xxih[i].wts = synth->wfspeed;

			B_ENDIAN32((uint32) synth->wf[0]);
			instr =
			    (struct InstrHdr *)((char *)synth +
						(uint32) synth->wf[0]);
			B_ENDIAN32(instr->length);
			B_ENDIAN16(instr->type);

			xxi[i][0].pan = 0x80;
			xxi[i][0].vol = song->sample[i].svol;
			xxi[i][0].xpo = song->sample[i].strans;
			xxi[i][0].sid = smp_idx;
			xxi[i][0].fin = expdata ? instrext->finetune << 4 : 0;

			xxs[smp_idx].len = instr->length;
			xxs[smp_idx].lps = 2 * song->sample[i].rep;
			xxs[smp_idx].lpe =
			    xxs[smp_idx].lps + 2 * song->sample[i].replen;
			xxs[smp_idx].flg =
			    song->sample[i].replen > 1 ? WAVE_LOOPING : 0;

			if (V(1)) {
				report("%05x %05x %05x %02x %02x %+1d ",
				       xxs[smp_idx].len, xxs[smp_idx].lps,
				       xxs[smp_idx].lpe, xxi[i][0].vol,
				       (uint8) xxi[i][0].xpo,
				       xxi[i][0].fin >> 4);
			}

			xmp_drv_loadpatch(NULL, smp_idx, xmp_ctl->c4rate,
					  XMP_SMP_NOLOAD, &xxs[smp_idx],
					  (char *)instr +
					  sizeof(struct InstrHdr));

			smp_idx++;

			med_vol_table[i] = calloc(1, synth->voltbllen);
			memcpy(med_vol_table[i], synth->voltbl,
			       synth->voltbllen);

			med_wav_table[i] = calloc(1, synth->wftbllen);
			memcpy(med_wav_table[i], synth->wftbl, synth->wftbllen);

			if (V(0))
				report(".");

			break;
		case -1:	/* Synthetic */
			B_ENDIAN16(synth->rep);
			B_ENDIAN16(synth->replen);
			B_ENDIAN16(synth->voltbllen);
			B_ENDIAN16(synth->wftbllen);
			B_ENDIAN16(synth->wforms);

			if (V(1))
				report
				    ("VS:%02x WS:%02x WF:%02x %02x %02x %+1d ",
				     synth->volspeed, synth->wfspeed,
				     synth->wforms, song->sample[i].svol,
				     (uint8) song->sample[i].strans,
				     expdata ? instrext->finetune : 0);

			xxi[i] =
			    calloc(sizeof(struct xxm_instrument),
				   synth->wforms);
			xxih[i].nsm = synth->wforms;
			xxih[i].vts = synth->volspeed;
			xxih[i].wts = synth->wfspeed;

			for (j = 0; j < synth->wforms; j++) {
				B_ENDIAN32((uint32) synth->wf[j]);

				xxi[i][j].pan = 0x80;
				xxi[i][j].vol = song->sample[i].svol;
				xxi[i][j].xpo = song->sample[i].strans - 24;
				xxi[i][j].sid = smp_idx;
				xxi[i][j].fin =
				    expdata ? instrext->finetune << 4 : 0;

				synthwf = (struct SynthWF *)((char *)synth +
							     (uint32) synth->
							     wf[j]);
				B_ENDIAN16(synthwf->length);
				xxs[smp_idx].len = synthwf->length * 2;
				xxs[smp_idx].lps = 0;
				xxs[smp_idx].lpe = xxs[smp_idx].len;
				xxs[smp_idx].flg = WAVE_LOOPING;

				xmp_drv_loadpatch(NULL, smp_idx,
						  xmp_ctl->c4rate,
						  XMP_SMP_NOLOAD | XMP_SMP_8X,
						  &xxs[smp_idx],
						  &synthwf->wfdata[0]);

				smp_idx++;
			}

			med_vol_table[i] = calloc(1, synth->voltbllen);
			memcpy(med_vol_table[i], synth->voltbl,
			       synth->voltbllen);

			med_wav_table[i] = calloc(1, synth->wftbllen);
			memcpy(med_wav_table[i], synth->wftbl, synth->wftbllen);

			if (V(0))
				report(".");

			break;
		case 0:	/* Sample */
			xxi[i] = calloc(sizeof(struct xxm_instrument), 1);
			xxih[i].nsm = 1;

			xxi[i][0].vol = song->sample[i].svol;
			xxi[i][0].xpo = song->sample[i].strans;
			xxi[i][0].sid = smp_idx;
			xxi[i][0].fin = expdata ? instrext->finetune << 4 : 0;

			if (ver >= 2)
				xxi[i][0].xpo -= 24;

			xxs[smp_idx].len = instr->length;
			xxs[smp_idx].lps = 2 * song->sample[i].rep;
			xxs[smp_idx].lpe =
			    xxs[smp_idx].lps + 2 * song->sample[i].replen;
			xxs[smp_idx].flg =
			    song->sample[i].replen > 1 ? WAVE_LOOPING : 0;

			if (V(1)) {
				report("%05x %05x %05x %02x %02x %+1d ",
				       xxs[smp_idx].len, xxs[smp_idx].lps,
				       xxs[smp_idx].lpe, xxi[i][0].vol,
				       (uint8) xxi[i][0].xpo,
				       xxi[i][0].fin >> 4);
			}

			xmp_drv_loadpatch(NULL, smp_idx, xmp_ctl->c4rate,
					  XMP_SMP_NOLOAD, &xxs[smp_idx],
					  (char *)instr +
					  sizeof(struct InstrHdr));

			if (V(0))
				report(".");

			smp_idx++;
			break;
		default:
			if (V(1))
				report("(unsupported)");
		}

	}

	if (V(0))
		report("\n");

	for (i = 0; i < xxh->chn; i++) {
		if (ver < 2) {
			xxc[i].vol = song->trkvol[i];
			xxc[i].pan = (((i + 1) / 2) % 2) * 0xff;
		} else {
			xxc[i].vol = 0x40;
			xxc[i].pan = (((i + 1) / 2) % 2) * 0xff;
		}
	}

	free(mmd);

	return 0;
}
