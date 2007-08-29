/* MMD2/3 MED module loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: mmd3_load.c,v 1.6 2007-08-29 03:23:58 cmatsuoka Exp $
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

static void xlat_fx(uint8 *fxt, uint8 *fxp)
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

int mmd3_load(FILE *f)
{
	int i, j, k;
	struct MMD0 header;
	struct MMD2song song;
	struct MMD1Block block;
	struct InstrHdr instr;
	struct InstrExt exp_smp;
	struct MMD0exp expdata;
	struct xxm_event *event;
	int ver = 0;
	int smp_idx = 0;
	uint8 e[4];
	int song_offset;
	int seqtable_offset;
	int trackvols_offset;
	int trackpans_offset;
	int blockarr_offset;
	int smplarr_offset;
	int expdata_offset;
	int expsmp_offset;
	int songname_offset;
	int iinfo_offset;
	int playseq_offset;

	LOAD_INIT();

	fread(&header.id, 4, 1, f);

	if (memcmp(&header.id, "MMD2", 4) && memcmp(&header.id, "MMD3", 4))
		return -1;

	ver = *((char *)&header.id + 3) - '1' + 1;

	_D(_D_WARN "load header");
	header.modlen = read32b(f);
	song_offset = read32b(f);
	_D(_D_INFO "song_offset = 0x%08x", song_offset);
	read16b(f);
	read16b(f);
	blockarr_offset = read32b(f);
	_D(_D_INFO "blockarr_offset = 0x%08x", blockarr_offset);
	read32b(f);
	smplarr_offset = read32b(f);
	_D(_D_INFO "smplarr_offset = 0x%08x", smplarr_offset);
	read32b(f);
	expdata_offset = read32b(f);
	_D(_D_INFO "expdata_offset = 0x%08x", expdata_offset);
	read32b(f);
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
	_D(_D_WARN "load song");
	fseek(f, song_offset, SEEK_SET);
	for (i = 0; i < 63; i++) {
		song.sample[i].rep = read16b(f);
		song.sample[i].replen = read16b(f);
		song.sample[i].midich = read8(f);
		song.sample[i].midipreset = read8(f);
		song.sample[i].svol = read8(f);
		song.sample[i].strans = read8s(f);
	}
	song.numblocks = read16b(f);
	song.songlen = read16b(f);
	_D(_D_INFO "song.songlen = %d", song.songlen);
	seqtable_offset = read32b(f);
	read32b(f);
	trackvols_offset = read32b(f);
	song.numtracks = read16b(f);
	song.numpseqs = read16b(f);
	trackpans_offset = read32b(f);
	song.flags3 = read32b(f);
	song.voladj = read16b(f);
	song.channels = read16b(f);
	song.mix_echotype = read8(f);
	song.mix_echodepth = read8(f);
	song.mix_echolen = read16b(f);
	song.mix_stereosep = read8(f);

	fseek(f, 223, SEEK_CUR);

	song.deftempo = read16b(f);
	song.playtransp = read8(f);
	song.flags = read8(f);
	song.flags2 = read8(f);
	song.tempo2 = read8(f);
	for (i = 0; i < 16; i++)
		read8(f);		/* reserved */
	song.mastervol = read8(f);
	song.numsamples = read8(f);

	/*
	 * read sequence
	 */
	fseek(f, seqtable_offset, SEEK_SET);
	playseq_offset = read32b(f);
	fseek(f, playseq_offset, SEEK_SET);
	fseek(f, 32, SEEK_CUR);	/* skip name */
	read32b(f);
	read32b(f);
	xxh->len = read16b(f);
	fread(xxo, xxh->len, 1, f);

	/*
	 * convert header
	 */
	xmp_ctl->c4rate = C4_NTSC_RATE;
	xmp_ctl->fetch |= song.flags & FLAG_STSLIDE ? 0 : XMP_CTL_VSALL;
	bpmon = song.flags2 & FLAG2_BPM;
	bpmlen = 1 + (song.flags2 & FLAG2_BMASK);
	xmp_ctl->fetch |= bpmon ? 0 : XMP_CTL_MEDBPM;

	/* FIXME: med tempos are incorrectly handled */
	xxh->tpo = song.tempo2;
	xxh->bpm = bpmon ? song.deftempo * bpmlen / 4 : song.deftempo;
	if (!bpmon && xxh->bpm <= 10)
		xxh->bpm = xxh->bpm * 33 / 6;
	xxh->pat = song.numblocks;
	xxh->ins = song.numsamples;
	xxh->smp = xxh->ins;
	xxh->rst = 0;
	xxh->chn = 0;
	xmp_ctl->name[0] = 0;

	/*
	 * expdata
	 */
	_D(_D_WARN "load expdata");
	expdata.s_ext_entries = 0;
	expdata.s_ext_entrsz = 0;
	expdata.i_ext_entries = 0;
	expdata.i_ext_entrsz = 0;
	expsmp_offset = 0;
	iinfo_offset = 0;
	if (expdata_offset) {
		fseek(f, expdata_offset, SEEK_SET);
		read32b(f);
		expsmp_offset = read32b(f);
		_D(_D_INFO "expsmp_offset = 0x%08x", expsmp_offset);
		expdata.s_ext_entries = read16b(f);
		expdata.s_ext_entrsz = read16b(f);
		read32b(f);
		read32b(f);
		iinfo_offset = read32b(f);
		_D(_D_INFO "iinfo_offset = 0x%08x", iinfo_offset);
		expdata.i_ext_entries = read16b(f);
		expdata.i_ext_entrsz = read16b(f);
		read32b(f);
		read32b(f);
		read32b(f);
		read32b(f);
		songname_offset = read32b(f);
		_D(_D_INFO "songname_offset = 0x%08x", songname_offset);
		expdata.songnamelen = read32b(f);
		fseek(f, songname_offset, SEEK_SET);
		_D(_D_INFO "expdata.songnamelen = %d", expdata.songnamelen);
		for (i = 0; i < expdata.songnamelen; i++) {
			if (i >= XMP_DEF_NAMESIZE)
				break;
			xmp_ctl->name[i] = read8(f);
		}
	}

	/*
	 * Quickly scan patterns to check the number of channels
	 */
	_D(_D_WARN "find number of channels");

	for (i = 0; i < xxh->pat; i++) {
		int block_offset;

		fseek(f, blockarr_offset + i * 4, SEEK_SET);
		block_offset = read32b(f);
		_D(_D_INFO "block %d block_offset = 0x%08x", i, block_offset);
		if (block_offset == 0)
			continue;
		fseek(f, block_offset, SEEK_SET);

		block.numtracks = read16b(f);
		block.lines = read16b(f);

		if (block.numtracks > xxh->chn)
			xxh->chn = block.numtracks;
	}

	xxh->trk = xxh->pat * xxh->chn;

	sprintf(xmp_ctl->type, "MMD%c (MED)", '0' + ver);
	MODULE_INFO();

	if (V(0)) {
		report("BPM mode       : %s", bpmon ? "on" : "off");
		if (bpmon)
			report(" (length = %d)", bpmlen);
		report("\n");
		if (song.playtransp)
			report("Song transpose : %d semitones\n",
			       song.playtransp);
		report("Stored patterns: %d ", xxh->pat);
	}

	/*
	 * Read and convert patterns
	 */
	_D(_D_WARN "read patterns");
	PATTERN_INIT();

	for (i = 0; i < xxh->pat; i++) {
		int block_offset;

		fseek(f, blockarr_offset + i * 4, SEEK_SET);
		block_offset = read32b(f);
		if (block_offset == 0)
			continue;
		fseek(f, block_offset, SEEK_SET);

		block.numtracks = read16b(f);
		block.lines = read16b(f);
		read32b(f);

		PATTERN_ALLOC(i);

		xxp[i]->rows = block.lines + 1;
		TRACK_ALLOC(i);

		for (j = 0; j < xxp[i]->rows; j++) {
			for (k = 0; k < block.numtracks; k++) {
				e[0] = read8(f);
				e[1] = read8(f);
				e[2] = read8(f);
				e[3] = read8(f);

				event = &EVENT(i, k, j);
				event->note = e[0] & 0x7f;
				if (event->note)
					event->note += 12 + song.playtransp;
				event->ins = e[1] & 0x3f;
				event->fxt = e[2];
				event->fxp = e[3];
				xlat_fx(&event->fxt, &event->fxp);
			}
		}

		if (V(0))
			report(".");
	}
	if (V(0))
		report("\n");

	/*
	 * Read and convert instruments and samples
	 */
	_D(_D_WARN "read instruments");
	INSTRUMENT_INIT();

	if (V(0))
		report("Instruments    : %d ", xxh->ins);

	if (V(1)) {
		report("\n     Instrument name                          "
		       "Typ Len   LBeg  LEnd  Vl Xp Ft");
	}

	for (smp_idx = i = 0; i < xxh->ins; i++) {
		int smpl_offset;
		fseek(f, smplarr_offset + i * 4, SEEK_SET);
		smpl_offset = read32b(f);
		//_D(_D_INFO "sample %d smpl_offset = 0x%08x", i, smpl_offset);
		if (smpl_offset == 0)
			continue;

		fseek(f, smpl_offset, SEEK_SET);
		instr.length = read32b(f);
		instr.type = read16b(f);

		if (V(1)) {
			char name[40] = "";
			if (expdata_offset && i < expdata.i_ext_entries) {
				fseek(f, iinfo_offset +
					i * expdata.i_ext_entrsz, SEEK_SET);
				fread(name, 40, 1, f);
			}

			report("\n[%2x] %-40.40s %s ", i, name,
				instr.type + 2 <= NUM_INST_TYPES ?
					inst_type[instr.type + 2] : "???");
		}

		exp_smp.finetune = 0;
		if (expdata_offset && i < expdata.s_ext_entries) {
			fseek(f, expsmp_offset + i * expdata.s_ext_entrsz,
							SEEK_SET);
			exp_smp.hold = read8(f);
			exp_smp.decay = read8(f);
			exp_smp.suppress_midi_off = read8(f);
			exp_smp.finetune = read8(f);

		}

		if (instr.type != 0)
			continue;

		/* instr type is sample */
		xxi[i] = calloc(sizeof(struct xxm_instrument), 1);
		xxih[i].nsm = 1;

		xxi[i][0].vol = song.sample[i].svol;
		xxi[i][0].pan = 0x80;
		xxi[i][0].xpo = song.sample[i].strans;
		xxi[i][0].sid = smp_idx;
		xxi[i][0].fin = exp_smp.finetune << 4;

		xxs[smp_idx].len = instr.length;
		xxs[smp_idx].lps = 2 * song.sample[i].rep;
		xxs[smp_idx].lpe = xxs[smp_idx].lps + 2 *
						song.sample[i].replen;
		xxs[smp_idx].flg = song.sample[i].replen > 1 ? WAVE_LOOPING : 0;

		if (V(1)) {
			report("%05x %05x %05x %02x %02x %+1d ",
			       xxs[smp_idx].len, xxs[smp_idx].lps,
			       xxs[smp_idx].lpe, xxi[i][0].vol,
			       (uint8) xxi[i][0].xpo, xxi[i][0].fin >> 4);
		}

		fseek(f, smpl_offset + 6, SEEK_SET);
		xmp_drv_loadpatch(f, smp_idx, xmp_ctl->c4rate, 0,
				  &xxs[smp_idx], NULL);

		if (V(0))
			report(".");

		smp_idx++;
	}

	if (V(0))
		report("\n");

	fseek(f, trackvols_offset, SEEK_SET);
	for (i = 0; i < xxh->chn; i++)
		xxc[i].vol = read8(f);;

	if (trackpans_offset) {
		fseek(f, trackpans_offset, SEEK_SET);
		for (i = 0; i < xxh->chn; i++) {
			int p = 8 * read8s(f);
			xxc[i].pan = 0x80 + (p > 127 ? 127 : p);
		}
	} else {
		for (i = 0; i < xxh->chn; i++)
			xxc[i].pan = 0x80;
	}

	return 0;
}
