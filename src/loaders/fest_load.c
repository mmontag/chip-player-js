/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "loader.h"
#include "mod.h"
#include "period.h"

/*
 * From http://www.livet.se/mahoney/:
 *
 * Most modules from His Master's Noise uses special chip-sounds or
 * fine-tuning of samples that never was a part of the standard NoiseTracker
 * v2.0 command set. So if you want to listen to them correctly use an Amiga
 * emulator and run the demo! DeliPlayer does a good job of playing them
 * (there are some occasional error mostly concerning vibrato and portamento
 * effects, but I can live with that!), and it can be downloaded from
 * http://www.deliplayer.com
 */

/*
 * From http://www.cactus.jawnet.pl/attitude/index.php?action=readtext&issue=12&which=12
 *
 * [Bepp] For your final Amiga release, the music disk His Master's Noise,
 * you developed a special version of NoiseTracker. Could you tell us a
 * little about this project?
 *
 * [Mahoney] I wanted to make a music disk with loads of songs, without being
 * too repetitive or boring. So all of my "experimental features" that did not
 * belong to NoiseTracker v2.0 were put into a separate version that would
 * feature wavetable sounds, chord calculations, off-line filter calculations,
 * mixing, reversing, sample accurate delays, resampling, fades - calculations
 * that would be done on a standard setup of sounds instead of on individual
 * modules. This "compression technique" lead to some 100 songs fitting on two
 * standard 3.5" disks, written by 22 different composers. I'd say that writing
 * a music program does give you loads of talented friends - you should try
 * that yourself someday!
 */

static int fest_test(FILE *, char *, const int);
static int fest_load(struct module_data *, FILE *, const int);

const struct format_loader fest_loader = {
	"His Master's Noise (MOD)",
	fest_test,
	fest_load
};

/* His Master's Noise M&K! will fail in regular Noisetracker loading
 * due to invalid finetune values.
 */
#define MAGIC_FEST	MAGIC4('F', 'E', 'S', 'T')
#define MAGIC_MK	MAGIC4('M', '&', 'K', '!')

static int fest_test(FILE * f, char *t, const int start)
{
	int magic;

	fseek(f, start + 1080, SEEK_SET);
	magic = read32b(f);

	if (magic != MAGIC_FEST && magic != MAGIC_MK)
		return -1;

	fseek(f, start + 0, SEEK_SET);
	read_title(f, t, 20);

	return 0;
}

static int fest_load(struct module_data *m, FILE * f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j;
	struct xmp_event *event;
	struct mod_header mh;
	uint8 mod_event[4];

	LOAD_INIT();

	fread(&mh.name, 20, 1, f);
	for (i = 0; i < 31; i++) {
		fread(&mh.ins[i].name, 22, 1, f);	/* Instrument name */
		mh.ins[i].size = read16b(f);	/* Length in 16-bit words */
		mh.ins[i].finetune = read8(f);	/* Finetune (signed nibble) */
		mh.ins[i].volume = read8(f);	/* Linear playback volume */
		mh.ins[i].loop_start = read16b(f);	/* Loop start in 16-bit words */
		mh.ins[i].loop_size = read16b(f);	/* Loop size in 16-bit words */
	}
	mh.len = read8(f);
	mh.restart = read8(f);
	fread(&mh.order, 128, 1, f);
	fread(&mh.magic, 4, 1, f);

	mod->chn = 4;
	mod->ins = 31;
	mod->smp = mod->ins;
	mod->len = mh.len;
	mod->rst = mh.restart;
	memcpy(mod->xxo, mh.order, 128);

	for (i = 0; i < 128; i++) {
		if (mod->xxo[i] > mod->pat)
			mod->pat = mod->xxo[i];
	}

	mod->pat++;

	mod->trk = mod->chn * mod->pat;

	strncpy(mod->name, (char *)mh.name, 20);
	set_type(m, "%s (%4.4s)", "His Master's Noise", mh.magic);
	MODULE_INFO();

	INSTRUMENT_INIT();

	for (i = 0; i < mod->ins; i++) {
		mod->xxi[i].sub = calloc(sizeof(struct xmp_subinstrument), 1);
		mod->xxs[i].len = 2 * mh.ins[i].size;
		mod->xxs[i].lps = 2 * mh.ins[i].loop_start;
		mod->xxs[i].lpe = mod->xxs[i].lps + 2 * mh.ins[i].loop_size;
		mod->xxs[i].flg = mh.ins[i].loop_size > 1 ? XMP_SAMPLE_LOOP : 0;
		mod->xxi[i].sub[0].fin = -(int8)(mh.ins[i].finetune << 4);
		mod->xxi[i].sub[0].vol = mh.ins[i].volume;
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].sid = i;
		mod->xxi[i].nsm = !!(mod->xxs[i].len);

printf("%2x] ", i);
int j; for(j = 0; j < 10; j++) printf("%02x ", mh.ins[i].name[j]); printf("\n");
		copy_adjust(mod->xxi[i].name, mh.ins[i].name, 22);
	}

	PATTERN_INIT();

	/* Load and convert patterns */
	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		PATTERN_ALLOC(i);
		mod->xxp[i]->rows = 64;
		TRACK_ALLOC(i);
		for (j = 0; j < (64 * 4); j++) {
			event = &EVENT(i, j % 4, j / 4);
			fread(mod_event, 1, 4, f);
			decode_noisetracker_event(event, mod_event);
		}
	}

	m->quirk |= QUIRK_MODRNG;

	/* Load samples */

	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < mod->smp; i++) {
		load_sample(m, f, SAMPLE_FLAG_FULLREP,
			    &mod->xxs[mod->xxi[i].sub[0].sid], NULL);
	}

	return 0;
}
