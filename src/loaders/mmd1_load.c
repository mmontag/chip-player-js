/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

/*
 * OctaMED v1.00b: ftp://ftp.funet.fi/pub/amiga/fish/501-600/ff579
 */

#include "med.h"
#include "loader.h"
#include "med_extras.h"

static int mmd1_test(HANDLE *, char *, const int);
static int mmd1_load (struct module_data *, HANDLE *, const int);

const struct format_loader mmd1_loader = {
	"MED 2.10/OctaMED (MED)",
	mmd1_test,
	mmd1_load
};

static int mmd1_test(HANDLE *f, char *t, const int start)
{
	char id[4];
	uint32 offset, len;

	if (hread(id, 1, 4, f) < 4)
		return -1;

	if (memcmp(id, "MMD0", 4) && memcmp(id, "MMD1", 4))
		return -1;

	hseek(f, 28, SEEK_CUR);
	offset = hread_32b(f);		/* expdata_offset */
	
	if (offset) {
		hseek(f, start + offset + 44, SEEK_SET);
		offset = hread_32b(f);
		len = hread_32b(f);
		hseek(f, start + offset, SEEK_SET);
		read_title(f, t, len);
	} else {
		read_title(f, t, 0);
	}

	return 0;
}


static int mmd1_load(struct module_data *m, HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j, k;
	struct MMD0 header;
	struct MMD0song song;
	struct MMD1Block block;
	struct InstrHdr instr;
	struct SynthInstr synth;
	struct InstrExt exp_smp;
	struct MMD0exp expdata;
	struct xmp_event *event;
	int ver = 0;
	int smp_idx = 0;
	uint8 e[4];
	int song_offset;
	int blockarr_offset;
	int smplarr_offset;
	int expdata_offset;
	int expsmp_offset;
	int songname_offset;
	int iinfo_offset;
	int pos;
	int bpm_on, bpmlen, med_8ch;

	LOAD_INIT();

	hread(&header.id, 4, 1, f);

	ver = *((char *)&header.id + 3) - '1' + 1;

	D_(D_WARN "load header");
	header.modlen = hread_32b(f);
	song_offset = hread_32b(f);
	D_(D_INFO "song_offset = 0x%08x", song_offset);
	hread_16b(f);
	hread_16b(f);
	blockarr_offset = hread_32b(f);
	D_(D_INFO "blockarr_offset = 0x%08x", blockarr_offset);
	hread_32b(f);
	smplarr_offset = hread_32b(f);
	D_(D_INFO "smplarr_offset = 0x%08x", smplarr_offset);
	hread_32b(f);
	expdata_offset = hread_32b(f);
	D_(D_INFO "expdata_offset = 0x%08x", expdata_offset);
	hread_32b(f);
	header.pstate = hread_16b(f);
	header.pblock = hread_16b(f);
	header.pline = hread_16b(f);
	header.pseqnum = hread_16b(f);
	header.actplayline = hread_16b(f);
	header.counter = hread_8(f);
	header.extra_songs = hread_8(f);

	/*
	 * song structure
	 */
	D_(D_WARN "load song");
	hseek(f, start + song_offset, SEEK_SET);
	for (i = 0; i < 63; i++) {
		song.sample[i].rep = hread_16b(f);
		song.sample[i].replen = hread_16b(f);
		song.sample[i].midich = hread_8(f);
		song.sample[i].midipreset = hread_8(f);
		song.sample[i].svol = hread_8(f);
		song.sample[i].strans = hread_8s(f);
	}
	song.numblocks = hread_16b(f);
	song.songlen = hread_16b(f);
	D_(D_INFO "song.songlen = %d", song.songlen);
	for (i = 0; i < 256; i++)
		song.playseq[i] = hread_8(f);
	song.deftempo = hread_16b(f);
	song.playtransp = hread_8(f);
	song.flags = hread_8(f);
	song.flags2 = hread_8(f);
	song.tempo2 = hread_8(f);
	for (i = 0; i < 16; i++)
		song.trkvol[i] = hread_8(f);
	song.mastervol = hread_8(f);
	song.numsamples = hread_8(f);

	/*
	 * convert header
	 */
	m->c4rate = C4_NTSC_RATE;
	m->quirk |= song.flags & FLAG_STSLIDE ? 0 : QUIRK_VSALL | QUIRK_PBALL;
	med_8ch = song.flags & FLAG_8CHANNEL;
	bpm_on = song.flags2 & FLAG2_BPM;
	bpmlen = 1 + (song.flags2 & FLAG2_BMASK);
	m->time_factor = MED_TIME_FACTOR;

	/* From the OctaMEDv4 documentation:
	 *
	 * In 8-channel mode, you can control the playing speed more
	 * accurately (to techies: by changing the size of the mix buffer).
	 * This can be done with the left tempo gadget (values 1-10; the
	 * lower, the faster). Values 11-240 are equivalent to 10.
	 */

        mod->spd = song.tempo2;
        mod->bpm = med_8ch ?
			mmd_get_8ch_tempo(song.deftempo) :
			(bpm_on ? song.deftempo * bpmlen / 16 : song.deftempo);

	mod->pat = song.numblocks;
	mod->ins = song.numsamples;
	//mod->smp = mod->ins;
	mod->len = song.songlen;
	mod->rst = 0;
	mod->chn = 0;
	memcpy(mod->xxo, song.playseq, mod->len);
	mod->name[0] = 0;

	/*
	 * Obtain number of samples from each instrument
	 */
	mod->smp = 0;
	for (i = 0; i < mod->ins; i++) {
		uint32 smpl_offset;
		int16 type;
		hseek(f, start + smplarr_offset + i * 4, SEEK_SET);
		smpl_offset = hread_32b(f);
		if (smpl_offset == 0)
			continue;
		hseek(f, start + smpl_offset, SEEK_SET);
		hread_32b(f);				/* length */
		type = hread_16b(f);
		if (type == -1) {			/* type is synth? */
			hseek(f, 14, SEEK_CUR);
			mod->smp += hread_16b(f);		/* wforms */
		} else {
			mod->smp++;
		}
	}

	/*
	 * expdata
	 */
	D_(D_WARN "load expdata");
	expdata.s_ext_entries = 0;
	expdata.s_ext_entrsz = 0;
	expdata.i_ext_entries = 0;
	expdata.i_ext_entrsz = 0;
	expsmp_offset = 0;
	iinfo_offset = 0;
	if (expdata_offset) {
		hseek(f, start + expdata_offset, SEEK_SET);
		hread_32b(f);
		expsmp_offset = hread_32b(f);
		D_(D_INFO "expsmp_offset = 0x%08x", expsmp_offset);
		expdata.s_ext_entries = hread_16b(f);
		expdata.s_ext_entrsz = hread_16b(f);
		hread_32b(f);
		hread_32b(f);
		iinfo_offset = hread_32b(f);
		D_(D_INFO "iinfo_offset = 0x%08x", iinfo_offset);
		expdata.i_ext_entries = hread_16b(f);
		expdata.i_ext_entrsz = hread_16b(f);
		hread_32b(f);
		hread_32b(f);
		hread_32b(f);
		hread_32b(f);
		songname_offset = hread_32b(f);
		D_(D_INFO "songname_offset = 0x%08x", songname_offset);
		expdata.songnamelen = hread_32b(f);
		hseek(f, start + songname_offset, SEEK_SET);
		D_(D_INFO "expdata.songnamelen = %d", expdata.songnamelen);
		for (i = 0; i < expdata.songnamelen; i++) {
			if (i >= XMP_NAME_SIZE)
				break;
			mod->name[i] = hread_8(f);
		}
	}

	/*
	 * Quickly scan patterns to check the number of channels
	 */
	D_(D_WARN "find number of channels");

	for (i = 0; i < mod->pat; i++) {
		int block_offset;

		hseek(f, start + blockarr_offset + i * 4, SEEK_SET);
		block_offset = hread_32b(f);
		D_(D_INFO "block %d block_offset = 0x%08x", i, block_offset);
		if (block_offset == 0)
			continue;
		hseek(f, start + block_offset, SEEK_SET);

		if (ver > 0) {
			block.numtracks = hread_16b(f);
			block.lines = hread_16b(f);
		} else {
			block.numtracks = hread_8(f);
			block.lines = hread_8(f);
		}

		if (block.numtracks > mod->chn)
			mod->chn = block.numtracks;
	}

	mod->trk = mod->pat * mod->chn;

	set_type(m, ver == 0 ? mod->chn > 4 ? "OctaMED 2.00 MMD0" :
				"MED 2.10 MMD0" : "OctaMED 4.00 MMD1");
	
	MODULE_INFO();

	D_(D_INFO "BPM mode: %s (length = %d)", bpm_on ? "on" : "off", bpmlen);
	D_(D_INFO "Song transpose: %d", song.playtransp);
	D_(D_INFO "Stored patterns: %d", mod->pat);

	/*
	 * Read and convert patterns
	 */
	D_(D_WARN "read patterns");
	PATTERN_INIT();

	for (i = 0; i < mod->pat; i++) {
		int block_offset;

		hseek(f, start + blockarr_offset + i * 4, SEEK_SET);
		block_offset = hread_32b(f);
		if (block_offset == 0)
			continue;
		hseek(f, start + block_offset, SEEK_SET);

		if (ver > 0) {
			block.numtracks = hread_16b(f);
			block.lines = hread_16b(f);
			hread_32b(f);
		} else {
			block.numtracks = hread_8(f);
			block.lines = hread_8(f);
		}

		PATTERN_ALLOC(i);

		mod->xxp[i]->rows = block.lines + 1;
		TRACK_ALLOC(i);

		if (ver > 0) {		/* MMD1 */
			for (j = 0; j < mod->xxp[i]->rows; j++) {
				for (k = 0; k < block.numtracks; k++) {
					e[0] = hread_8(f);
					e[1] = hread_8(f);
					e[2] = hread_8(f);
					e[3] = hread_8(f);

					event = &EVENT(i, k, j);
					event->note = e[0] & 0x7f;
					if (event->note)
						event->note +=
						    48 + song.playtransp;
					event->ins = e[1] & 0x3f;
					event->fxt = e[2];
					event->fxp = e[3];
					mmd_xlat_fx(event, bpm_on,
							bpmlen, med_8ch);
				}
			}
		} else {		/* MMD0 */
			for (j = 0; j < mod->xxp[i]->rows; j++) {
				for (k = 0; k < block.numtracks; k++) {
					e[0] = hread_8(f);
					e[1] = hread_8(f);
					e[2] = hread_8(f);

					event = &EVENT(i, k, j);
					event->note = e[0] & 0x3f;
					if (event->note)
						event->note += 48;
					event->ins =
					    (e[1] >> 4) | ((e[0] & 0x80) >> 3)
					    | ((e[0] & 0x40) >> 1);
					event->fxt = e[1] & 0x0f;
					event->fxp = e[2];
					mmd_xlat_fx(event, bpm_on,
							bpmlen, med_8ch);
				}
			}
		}
	}

	m->med_vol_table = calloc(sizeof(uint8 *), mod->ins);
	m->med_wav_table = calloc(sizeof(uint8 *), mod->ins);

	/*
	 * Read and convert instruments and samples
	 */
	D_(D_WARN "read instruments");
	INSTRUMENT_INIT();

	D_(D_INFO "Instruments: %d", mod->ins);

	for (smp_idx = i = 0; i < mod->ins; i++) {
		int smpl_offset;
		char name[40] = "";

		hseek(f, start + smplarr_offset + i * 4, SEEK_SET);
		smpl_offset = hread_32b(f);

		D_(D_INFO "sample %d smpl_offset = 0x%08x", i, smpl_offset);

		if (smpl_offset == 0)
			continue;

		hseek(f, start + smpl_offset, SEEK_SET);
		instr.length = hread_32b(f);
		instr.type = hread_16b(f);

		pos = htell(f);

		if (expdata_offset && i < expdata.i_ext_entries) {
		    hseek(f, iinfo_offset + i * expdata.i_ext_entrsz, SEEK_SET);
		    hread(name, 40, 1, f);
		}

		D_(D_INFO "\n[%2x] %-40.40s %d", i, name, instr.type);

		exp_smp.finetune = 0;
		if (expdata_offset && i < expdata.s_ext_entries) {
			hseek(f, expsmp_offset + i * expdata.s_ext_entrsz,
							SEEK_SET);
			exp_smp.hold = hread_8(f);
			exp_smp.decay = hread_8(f);
			exp_smp.suppress_midi_off = hread_8(f);
			exp_smp.finetune = hread_8(f);
		}

		hseek(f, pos, SEEK_SET);

		if (instr.type == -2) {			/* Hybrid */
			int length, type;
			int pos = htell(f);

			synth.defaultdecay = hread_8(f);
			hseek(f, 3, SEEK_CUR);
			synth.rep = hread_16b(f);
			synth.replen = hread_16b(f);
			synth.voltbllen = hread_16b(f);
			synth.wftbllen = hread_16b(f);
			synth.volspeed = hread_8(f);
			synth.wfspeed = hread_8(f);
			synth.wforms = hread_16b(f);
			hread(synth.voltbl, 1, 128, f);;
			hread(synth.wftbl, 1, 128, f);;

			hseek(f, pos - 6 + hread_32b(f), SEEK_SET);
			length = hread_32b(f);
			type = hread_16b(f);

			mod->xxi[i].extra = malloc(sizeof (struct med_extras));
			if (mod->xxi[i].extra == NULL)
				return -1;

			mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
			if (mod->xxi[i].sub == NULL)
				return -1;

			mod->xxi[i].nsm = 1;
			MED_EXTRA(mod->xxi[i])->vts = synth.volspeed;
			MED_EXTRA(mod->xxi[i])->wts = synth.wfspeed;
			mod->xxi[i].sub[0].pan = 0x80;
			mod->xxi[i].sub[0].vol = song.sample[i].svol;
			mod->xxi[i].sub[0].xpo = song.sample[i].strans;
			mod->xxi[i].sub[0].sid = smp_idx;
			mod->xxi[i].sub[0].fin = exp_smp.finetune;
			mod->xxs[smp_idx].len = length;
			mod->xxs[smp_idx].lps = 2 * song.sample[i].rep;
			mod->xxs[smp_idx].lpe = mod->xxs[smp_idx].lps +
						2 * song.sample[i].replen;
			mod->xxs[smp_idx].flg = song.sample[i].replen > 1 ?
						XMP_SAMPLE_LOOP : 0;

			D_(D_INFO "  %05x %05x %05x %02x %+3d %+1d",
				       mod->xxs[smp_idx].len,
				       mod->xxs[smp_idx].lps,
				       mod->xxs[smp_idx].lpe,
				       mod->xxi[i].sub[0].vol,
				       mod->xxi[i].sub[0].xpo,
				       mod->xxi[i].sub[0].fin >> 4);

			load_sample(m, f, 0, &mod->xxs[smp_idx], NULL);

			smp_idx++;

			m->med_vol_table[i] = calloc(1, synth.voltbllen);
			memcpy(m->med_vol_table[i], synth.voltbl, synth.voltbllen);

			m->med_wav_table[i] = calloc(1, synth.wftbllen);
			memcpy(m->med_wav_table[i], synth.wftbl, synth.wftbllen);

			continue;
		}

		if (instr.type == -1) {			/* Synthetic */
			int pos = htell(f);

			synth.defaultdecay = hread_8(f);
			hseek(f, 3, SEEK_CUR);
			synth.rep = hread_16b(f);
			synth.replen = hread_16b(f);
			synth.voltbllen = hread_16b(f);
			synth.wftbllen = hread_16b(f);
			synth.volspeed = hread_8(f);
			synth.wfspeed = hread_8(f);
			synth.wforms = hread_16b(f);
			hread(synth.voltbl, 1, 128, f);;
			hread(synth.wftbl, 1, 128, f);;
			for (j = 0; j < 64; j++)
				synth.wf[j] = hread_32b(f);

			D_(D_INFO "  VS:%02x WS:%02x WF:%02x %02x %+3d %+1d",
					synth.volspeed, synth.wfspeed,
					synth.wforms & 0xff,
					song.sample[i].svol,
					song.sample[i].strans,
					exp_smp.finetune);

			if (synth.wforms == 0xffff)	
				continue;

			mod->xxi[i].extra = malloc(sizeof (struct med_extras));
			if (mod->xxi[i].extra == NULL)
				return -1;

			mod->xxi[i].sub = calloc(sizeof(struct xmp_subinstrument),
							synth.wforms);
			if (mod->xxi[i].sub == NULL)
				return -1;

			mod->xxi[i].nsm = synth.wforms;
			MED_EXTRA(mod->xxi[i])->vts = synth.volspeed;
			MED_EXTRA(mod->xxi[i])->wts = synth.wfspeed;

			for (j = 0; j < synth.wforms; j++) {
				mod->xxi[i].sub[j].pan = 0x80;
				mod->xxi[i].sub[j].vol = song.sample[i].svol;
				mod->xxi[i].sub[j].xpo = song.sample[i].strans - 24;
				mod->xxi[i].sub[j].sid = smp_idx;
				mod->xxi[i].sub[j].fin = exp_smp.finetune;

				hseek(f, pos - 6 + synth.wf[j], SEEK_SET);

				mod->xxs[smp_idx].len = hread_16b(f) * 2;
				mod->xxs[smp_idx].lps = 0;
				mod->xxs[smp_idx].lpe = mod->xxs[smp_idx].len;
				mod->xxs[smp_idx].flg = XMP_SAMPLE_LOOP;

				load_sample(m, f, 0, &mod->xxs[smp_idx], NULL);

				smp_idx++;
			}

			m->med_vol_table[i] = calloc(1, synth.voltbllen);
			memcpy(m->med_vol_table[i], synth.voltbl, synth.voltbllen);

			m->med_wav_table[i] = calloc(1, synth.wftbllen);
			memcpy(m->med_wav_table[i], synth.wftbl, synth.wftbllen);

			continue;
		}

		if (instr.type != 0)
			continue;

		/* instr type is sample */
		mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
		mod->xxi[i].nsm = 1;

		mod->xxi[i].sub[0].vol = song.sample[i].svol;
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].xpo = song.sample[i].strans;
		mod->xxi[i].sub[0].sid = smp_idx;
		mod->xxi[i].sub[0].fin = exp_smp.finetune << 4;

		mod->xxs[smp_idx].len = instr.length;
		mod->xxs[smp_idx].lps = 2 * song.sample[i].rep;
		mod->xxs[smp_idx].lpe = mod->xxs[smp_idx].lps + 2 *
						song.sample[i].replen;

		mod->xxs[smp_idx].flg = 0;
		if (song.sample[i].replen > 1)
			mod->xxs[smp_idx].flg |= XMP_SAMPLE_LOOP;

		D_(D_INFO "  %05x %05x %05x %02x %+3d %+1d",
				mod->xxs[smp_idx].len,
				mod->xxs[smp_idx].lps,
				mod->xxs[smp_idx].lpe,
				mod->xxi[i].sub[0].vol,
				mod->xxi[i].sub[0].xpo,
				mod->xxi[i].sub[0].fin >> 4);

		hseek(f, start + smpl_offset + 6, SEEK_SET);
		load_sample(m, f, 0, &mod->xxs[smp_idx], NULL);

		smp_idx++;
	}

	/*
	 * Adjust event note data in patterns
	 */

	/* Restrict non-synth instruments to 3 octave range.
	 * Checked in MMD0 with med.egypian/med.medieval from Lemmings 2
	 * and MED.ParasolStars, MMD1 with med.Lemmings2
	 */
	for (i = 0; i < mod->pat; i++) {
		for (j = 0; j < mod->xxp[i]->rows; j++) {
			for (k = 0; k < mod->chn; k++) {
				event = &EVENT(i, k, j);

				if (!event->note || !event->ins)
					continue;

				/* Not a synth instrument */
				if (!m->med_wav_table[event->ins - 1]) {
					while (event->note > (48 + 36))
						event->note -= 12;
				}
			}
		}
	}

	for (i = 0; i < mod->chn; i++) {
		mod->xxc[i].vol = song.trkvol[i];
		mod->xxc[i].pan = (((i + 1) / 2) % 2) * 0xff;
	}

	return 0;
}
