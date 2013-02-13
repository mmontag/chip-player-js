/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

/*
 * Originally based on the PSM loader from Modplug by Olivier Lapicque and
 * fixed comparing the One Must Fall! PSMs with Kenny Chou's MTM files.
 */

/*
 * From EPICTEST Readme.1st:
 *
 * The Music And Sound Interface, MASI, is the basis behind all new Epic
 * games. MASI uses its own proprietary file format, PSM, for storing
 * its music.
 */

/*
 * kode54's comment on Sinaria PSMs in the foo_dumb hydrogenaudio forum:
 *
 * "The Sinaria variant uses eight character pattern and instrument IDs,
 * the sample headers are laid out slightly different, and the patterns
 * use a different format for the note values, and also different effect
 * scales for certain commands.
 *
 * [Epic] PSM uses high nibble for octave and low nibble for note, for
 * a valid range up to 0x7F, for a range of D-1 through D#9 compared to
 * IT. (...) Sinaria PSM uses plain note values, from 1 - 83, for a
 * range of C-3 through B-9.
 *
 * [Epic] PSM also uses an effect scale for portamento, volume slides,
 * and vibrato that is about four times as sensitive as the IT equivalents.
 * Sinaria does not. This seems to coincide with the MOD/S3M to PSM
 * converter that Joshua Jensen released in the EPICTEST.ZIP file which
 * can still be found on a few FTP sites. It converted effects literally,
 * even though the bundled players behaved as the libraries used with
 * Epic's games did and made the effects sound too strong."
 */

/*
 * Claudio's note: Sinaria seems to have a finetune byte just before
 * volume, slightly different sample size (subtract 2 bytes?) and some
 * kind of (stereo?) interleaved sample, with 16-byte frames (see Sinaria
 * songs 5 and 8). Sinaria song 10 sounds ugly, possibly caused by wrong
 * pitchbendings (see note above).
 */

/* FIXME: TODO: sinaria effects */

#include "loader.h"
#include "iff.h"
#include "period.h"

#define MAGIC_PSM_	MAGIC4('P','S','M',' ')
#define MAGIC_FILE	MAGIC4('F','I','L','E')
#define MAGIC_TITL	MAGIC4('T','I','T','L')
#define MAGIC_OPLH	MAGIC4('O','P','L','H')


static int masi_test (FILE *, char *, const int);
static int masi_load (struct module_data *, FILE *, const int);

const struct format_loader masi_loader = {
	"Epic MegaGames MASI (PSM)",
	masi_test,
	masi_load
};

static int masi_test(FILE *f, char *t, const int start)
{
	int val;

	if (read32b(f) != MAGIC_PSM_)
		return -1;

	read8(f);
	read8(f);
	read8(f);
	if (read8(f) != 0)
		return -1;

	if (read32b(f) != MAGIC_FILE) 
		return -1;

	read32b(f);
	val = read32l(f);
	fseek(f, val, SEEK_CUR);

	if (read32b(f) == MAGIC_TITL) {
		val = read32l(f);
		read_title(f, t, val);
	} else {
		read_title(f, t, 0);
	}

	return 0;
}

struct local_data {
    int sinaria;
    int cur_pat;
    int cur_ins;
    uint8 *pnam;
    uint8 *pord;
};

static void get_sdft(struct module_data *m, int size, FILE *f, void *parm)
{
}

static void get_titl(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	char buf[40];
	
	fread(buf, 1, 40, f);
	strncpy(mod->name, buf, size > 32 ? 32 : size);
}

static void get_dsmp_cnt(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;

	mod->ins++;
	mod->smp = mod->ins;
}

static void get_pbod_cnt(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	char buf[20];

	mod->pat++;
	fread(buf, 1, 20, f);
	if (buf[9] != 0 && buf[13] == 0)
		data->sinaria = 1;
}


static void get_dsmp(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	int i, srate;
	int finetune;

	read8(f);				/* flags */
	fseek(f, 8, SEEK_CUR);			/* songname */
	fseek(f, data->sinaria ? 8 : 4, SEEK_CUR);	/* smpid */

	i = data->cur_ins;
	mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);

	fread(&mod->xxi[i].name, 1, 34, f);
	str_adj((char *)mod->xxi[i].name);
	fseek(f, 5, SEEK_CUR);
	read8(f);		/* insno */
	read8(f);
	mod->xxs[i].len = read32l(f);
	mod->xxi[i].nsm = !!(mod->xxs[i].len);
	mod->xxs[i].lps = read32l(f);
	mod->xxs[i].lpe = read32l(f);
	mod->xxs[i].flg = mod->xxs[i].lpe > 2 ? XMP_SAMPLE_LOOP : 0;
	read16l(f);

	if ((int32)mod->xxs[i].lpe < 0)
		mod->xxs[i].lpe = 0;

	finetune = 0;
	if (data->sinaria) {
		if (mod->xxs[i].len > 2)
			mod->xxs[i].len -= 2;
		if (mod->xxs[i].lpe > 2)
			mod->xxs[i].lpe -= 2;

		finetune = (int8)(read8s(f) << 4);
	}

	mod->xxi[i].sub[0].vol = read8(f) / 2 + 1;
	read32l(f);
	mod->xxi[i].sub[0].pan = 0x80;
	mod->xxi[i].sub[0].sid = i;
	srate = read32l(f);

	D_(D_INFO "[%2X] %-32.32s %05x %05x %05x %c V%02x %+04d %5d", i,
		mod->xxi[i].name, mod->xxs[i].len, mod->xxs[i].lps, mod->xxs[i].lpe,
		mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ', mod->xxi[i].sub[0].vol,
		finetune, srate);

	srate = 8363 * srate / 8448;
	c2spd_to_note(srate, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);
	mod->xxi[i].sub[0].fin += finetune;

	fseek(f, 16, SEEK_CUR);
	load_sample(f, SAMPLE_FLAG_8BDIFF, &mod->xxs[i], NULL);

	data->cur_ins++;
}


static void get_pbod(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	int i, r;
	struct xmp_event *event, dummy;
	uint8 flag, chan;
	uint32 len;
	int rows, rowlen;

	i = data->cur_pat;

	len = read32l(f);
	fread(data->pnam + i * 8, 1, data->sinaria ? 8 : 4, f);

	rows = read16l(f);

	PATTERN_ALLOC(i);
	mod->xxp[i]->rows = rows;
	TRACK_ALLOC(i);

	r = 0;

	do {
		rowlen = read16l(f) - 2;
		while (rowlen > 0) {
			flag = read8(f);
	
			if (rowlen == 1)
				break;
	
			chan = read8(f);
			rowlen -= 2;
	
			event = chan < mod->chn ? &EVENT(i, chan, r) : &dummy;
	
			if (flag & 0x80) {
				uint8 note = read8(f);
				rowlen--;
				if (data->sinaria)
					note += 37;
				else
					note = (note >> 4) * 12 + (note & 0x0f) + 2 + 12;
				event->note = note;
			}

			if (flag & 0x40) {
				event->ins = read8(f) + 1;
				rowlen--;
			}
	
			if (flag & 0x20) {
				event->vol = read8(f) / 2;
				rowlen--;
			}
	
			if (flag & 0x10) {
				uint8 fxt = read8(f);
				uint8 fxp = read8(f);
				rowlen -= 2;
	
				/* compressed events */
				if (fxt >= 0x40) {
					switch (fxp >> 4) {
					case 0x0: {
						uint8 note;
						note = (fxt>>4)*12 +
							(fxt & 0x0f) + 2;
						event->note = note;
						fxt = FX_TONEPORTA;
						fxp = (fxp + 1) * 2;
						break; }
					default:
printf("p%d r%d c%d: compressed event %02x %02x\n", i, r, chan, fxt, fxp);
					}
				} else
				switch (fxt) {
				case 0x01:		/* fine volslide up */
					fxt = FX_EXTENDED;
					fxp = (EX_F_VSLIDE_UP << 4) |
						((fxp / 2) & 0x0f);
					break;
				case 0x02:		/* volslide up */
					fxt = FX_VOLSLIDE;
					fxp = (fxp / 2) << 4;
					break;
				case 0x03:		/* fine volslide down */
					fxt = FX_EXTENDED;
					fxp = (EX_F_VSLIDE_DN << 4) |
						((fxp / 2) & 0x0f);
					break;
				case 0x04: 		/* volslide down */
					fxt = FX_VOLSLIDE;
					fxp /= 2;
					break;
			    	case 0x0C:		/* portamento up */
					fxt = FX_PORTA_UP;
					fxp = (fxp - 1) / 2;
					break;
				case 0x0E:		/* portamento down */
					fxt = FX_PORTA_DN;
					fxp = (fxp - 1) / 2;
					break;
				case 0x0f:		/* tone portamento */
					fxt = FX_TONEPORTA;
					fxp /= 4;
					break;
				case 0x15:		/* vibrato */
					fxt = data->sinaria ?
						FX_VIBRATO : FX_FINE4_VIBRA;
					/* fxp remains the same */
					break;
				case 0x2a:		/* retrig note */
					fxt = FX_EXTENDED;
					fxp = (EX_RETRIG << 4) | (fxp & 0x0f); 
					break;
				case 0x29:		/* unknown */
					read16l(f);
					rowlen -= 2;
					break;
				case 0x33:		/* position Jump */
					fxt = FX_JUMP;
					break;
			    	case 0x34:		/* pattern break */
					fxt = FX_BREAK;
					break;
				case 0x3D:		/* speed */
					fxt = FX_SPEED;
					break;
				case 0x3E:		/* tempo */
					fxt = FX_SPEED;
					break;
				default:
printf("p%d r%d c%d: unknown effect %02x %02x\n", i, r, chan, fxt, fxp);
					fxt = fxp = 0;
				}
	
				event->fxt = fxt;
				event->fxp = fxp;
			}
		}
		r++;
	} while (r < rows);

	data->cur_pat++;
}

static void get_song(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;

	fseek(f, 10, SEEK_CUR);
	mod->chn = read8(f);
}

static void get_song_2(struct module_data *m, int size, FILE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	uint32 magic;
	char c, buf[20];
	int i;

	fread(buf, 1, 9, f);
	read16l(f);

	D_(D_INFO "Subsong title: %-9.9s", buf);

	magic = read32b(f);
	while (magic != MAGIC_OPLH) {
		int skip;
		skip = read32l(f);;
		fseek(f, skip, SEEK_CUR);
		magic = read32b(f);
	}

	read32l(f);	/* chunk size */

	fseek(f, 9, SEEK_CUR);		/* unknown data */
	
	c = read8(f);
	for (i = 0; c != 0x01; c = read8(f)) {
		switch (c) {
		case 0x07:
			mod->spd = read8(f);
			read8(f);		/* 08 */
			mod->bpm = read8(f);
			break;
		case 0x0d:
			read8(f);		/* channel number? */
			mod->xxc[i].pan = read8(f);
			read8(f);		/* flags? */
			i++;
			break;
		case 0x0e:
			read8(f);		/* channel number? */
			read8(f);		/* ? */
			break;
		default:
			printf("channel %d: %02x %02x\n", i, c, read8(f));
		}
	}

	for (; c == 0x01; c = read8(f)) {
		fread(data->pord + mod->len * 8, 1, data->sinaria ? 8 : 4, f);
		mod->len++;
	}
}

static int masi_load(struct module_data *m, FILE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	iff_handle handle;
	int offset;
	int i, j;
	struct local_data data;

	LOAD_INIT();

	read32b(f);

	data.sinaria = 0;
	mod->name[0] = 0;

	fseek(f, 8, SEEK_CUR);		/* skip file size and FILE */
	mod->smp = mod->ins = 0;
	data.cur_pat = 0;
	data.cur_ins = 0;
	offset = ftell(f);

	handle = iff_new();
	if (handle == NULL)
		return -1;

	/* IFF chunk IDs */
	iff_register(handle, "TITL", get_titl);
	iff_register(handle, "SDFT", get_sdft);
	iff_register(handle, "SONG", get_song);
	iff_register(handle, "DSMP", get_dsmp_cnt);
	iff_register(handle, "PBOD", get_pbod_cnt);
	iff_set_quirk(handle, IFF_LITTLE_ENDIAN);

	/* Load IFF chunks */
	while (!feof(f)) {
		iff_chunk(handle, m, f, &data);
	}

	iff_release(handle);

	mod->trk = mod->pat * mod->chn;
	data.pnam = malloc(mod->pat * 8);	/* pattern names */
	data.pord = malloc(255 * 8);		/* pattern orders */

	set_type(m, data.sinaria ?
		"Sinaria PSM" : "Epic MegaGames MASI PSM");

	MODULE_INFO();
	INSTRUMENT_INIT();
	PATTERN_INIT();

	D_(D_INFO "Stored patterns: %d", mod->pat);
	D_(D_INFO "Stored samples : %d", mod->smp);

	fseek(f, start + offset, SEEK_SET);

	mod->len = 0;

	handle = iff_new();
	if (handle == NULL)
		return -1;

	/* IFF chunk IDs */
	iff_register(handle, "SONG", get_song_2);
	iff_register(handle, "DSMP", get_dsmp);
	iff_register(handle, "PBOD", get_pbod);
	iff_set_quirk(handle, IFF_LITTLE_ENDIAN);

	/* Load IFF chunks */
	while (!feof (f)) {
		iff_chunk(handle, m, f, &data);
	}

	iff_release(handle);

	for (i = 0; i < mod->len; i++) {
		for (j = 0; j < mod->pat; j++) {
			if (!memcmp(data.pord + i * 8, data.pnam + j * 8, data.sinaria ? 8 : 4)) {
				mod->xxo[i] = j;
				break;
			}
		}

		if (j == mod->pat)
			break;
	}

	free(data.pnam);
	free(data.pord);

	return 0;
}
