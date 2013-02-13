/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

/* AMF loader written based on the format specs by Miodrag Vallat with
 * fixes by Andre Timmermans
 *
 * The AMF format is the internal format used by DSMI, the DOS Sound and Music
 * Interface, which is the engine of DMP. As DMP was able to play more and more
 * module formats, the format evolved to support more features. There were 5
 * official formats, numbered from 10 (AMF 1.0) to 14 (AMF 1.4).
 */

#include "loader.h"
#include "period.h"


static int amf_test(FILE *, char *, const int);
static int amf_load (struct module_data *, FILE *, const int);

const struct format_loader amf_loader = {
	"DSMI Advanced Module Format (AMF)",
	amf_test,
	amf_load
};

static int amf_test(FILE * f, char *t, const int start)
{
	char buf[4];
	int ver;

	if (fread(buf, 1, 3, f) < 3)
		return -1;

	if (buf[0] != 'A' || buf[1] != 'M' || buf[2] != 'F')
		return -1;

	ver = read8(f);
	if (ver < 0x0a || ver > 0x0e)
		return -1;

	read_title(f, t, 32);

	return 0;
}


static int amf_load(struct module_data *m, FILE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j;
	struct xmp_event *event;
	uint8 buf[1024];
	int *trkmap, newtrk;
	int ver;

	LOAD_INIT();

	fread(buf, 1, 3, f);
	ver = read8(f);

	fread(buf, 1, 32, f);
	strncpy(mod->name, (char *)buf, 32);
	set_type(m, "DSMI %d.%d AMF", ver / 10, ver % 10);

	mod->ins = read8(f);
	mod->len = read8(f);
	mod->trk = read16l(f);
	mod->chn = read8(f);

	mod->smp = mod->ins;
	mod->pat = mod->len;

	if (ver == 0x0a)
		fread(buf, 1, 16, f);		/* channel remap table */

	if (ver >= 0x0d) {
		fread(buf, 1, 32, f);		/* panning table */
		for (i = 0; i < 32; i++) {
			mod->xxc->pan = 0x80 + 2 * (int8)buf[i];
		}
		mod->bpm = read8(f);
		mod->spd = read8(f);
	} else if (ver >= 0x0b) {
		fread(buf, 1, 16, f);
	}

	MODULE_INFO();
 

	/* Orders */

	/*
	 * Andre Timmermans <andre.timmermans@atos.net> says:
	 *
	 * Order table: track numbers in this table are not explained,
	 * but as you noticed you have to perform -1 to obtain the index
	 * in the track table. For value 0, found in some files, I think
	 * it means an empty track.
	 */

	for (i = 0; i < mod->len; i++)
		mod->xxo[i] = i;

	D_(D_INFO "Stored patterns: %d", mod->pat);

	mod->xxp = calloc(sizeof(struct xmp_pattern *), mod->pat + 1);

	for (i = 0; i < mod->pat; i++) {
		PATTERN_ALLOC(i);
		mod->xxp[i]->rows = ver >= 0x0e ? read16l(f) : 64;
		for (j = 0; j < mod->chn; j++) {
			uint16 t = read16l(f);
			mod->xxp[i]->index[j] = t;
		}
	}

	/* Instruments */

	INSTRUMENT_INIT();

	/* Probe for 2-byte loop start 1.0 format
	 * in facing_n.amf and sweetdrm.amf have only the sample
	 * loop start specified in 2 bytes
	 */
	if (ver <= 0x0a) {
		uint8 b;
		int len, start, end;
		long pos = ftell(f);
		for (i = 0; i < mod->ins; i++) {
			b = read8(f);
			if (b != 0 && b != 1) {
				ver = 0x09;
				break;
			}
			fseek(f, 32 + 13, SEEK_CUR);
			if (read32l(f) > 0x100000) {	/* check index */
				ver = 0x09;
				break;
			}
			len = read32l(f);
			if (len > 0x100000) {		/* check len */
				ver = 0x09;
				break;
			}
			if (read16l(f) == 0x0000) {	/* check c2spd */
				ver = 0x09;
				break;
			}
			if (read8(f) > 0x40) {		/* check volume */
				ver = 0x09;
				break;
			}
			start = read32l(f);
			if (start > len) {		/* check loop start */
				ver = 0x09;
				break;
			}
			end = read32l(f);
			if (end > len) {		/* check loop end */
				ver = 0x09;
				break;
			}
		}
		fseek(f, pos, SEEK_SET);
	}

	for (i = 0; i < mod->ins; i++) {
		uint8 b;
		int c2spd;

		mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);

		b = read8(f);
		mod->xxi[i].nsm = b ? 1 : 0;

		fread(buf, 1, 32, f);
		copy_adjust(mod->xxi[i].name, buf, 32);

		fread(buf, 1, 13, f);	/* sample name */
		read32l(f);		/* sample index */

		mod->xxi[i].sub[0].sid = i;
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxs[i].len = read32l(f);
		c2spd = read16l(f);
		c2spd_to_note(c2spd, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);
		mod->xxi[i].sub[0].vol = read8(f);

		/*
		 * Andre Timmermans <andre.timmermans@atos.net> says:
		 *
		 * [Miodrag Vallat's] doc tells that in version 1.0 only
		 * sample loop start is present (2 bytes) but the files I
		 * have tells both start and end are present (2*4 bytes).
		 * Maybe it should be read as version < 1.0.
		 *
		 * CM: confirmed with Maelcum's "The tribal zone"
		 */

		if (ver < 0x0a) {
			mod->xxs[i].lps = read16l(f);
			mod->xxs[i].lpe = mod->xxs[i].len - 1;
		} else {
			mod->xxs[i].lps = read32l(f);
			mod->xxs[i].lpe = read32l(f);
		}

		if (ver < 0x0a) {
			mod->xxs[i].flg = mod->xxs[i].lps > 0 ? XMP_SAMPLE_LOOP : 0;
		} else {
			mod->xxs[i].flg = mod->xxs[i].lpe > mod->xxs[i].lps ?
							XMP_SAMPLE_LOOP : 0;
		}

		D_(D_INFO "[%2X] %-32.32s %05x %05x %05x %c V%02x %5d",
			i, mod->xxi[i].name, mod->xxs[i].len, mod->xxs[i].lps,
			mod->xxs[i].lpe, mod->xxs[i].flg & XMP_SAMPLE_LOOP ?
			'L' : ' ', mod->xxi[i].sub[0].vol, c2spd);
	}
				

	/* Tracks */

	trkmap = calloc(sizeof(int), mod->trk);
	newtrk = 0;

	for (i = 0; i < mod->trk; i++) {		/* read track table */
		uint16 t;
		t = read16l(f);
		trkmap[i] = t;
		if (t > newtrk) newtrk = t;
/*printf("%d -> %d\n", i, t);*/
	}

	for (i = 0; i < mod->pat; i++) {		/* read track table */
		for (j = 0; j < mod->chn; j++) {
			int k = mod->xxp[i]->index[j] - 1;

			/* Use empty track if an invalid track is requested
			 * (such as in Lasse Makkonen "faster and louder")
			 */
			if (k < 0 || k > mod->trk)
				k = 0;
			mod->xxp[i]->index[j] = trkmap[k];
/*printf("mod->xxp[%d]->info[%d].index = %d (k = %d)\n", i, j, trkmap[k], k);*/
		}
	}

	mod->trk = newtrk;		/* + empty track */
	free(trkmap);

	D_(D_INFO "Stored tracks: %d", mod->trk);

	mod->trk++;
	mod->xxt = calloc (sizeof (struct xmp_track *), mod->trk);

	/* Alloc track 0 as empty track */
	mod->xxt[0] = calloc(sizeof(struct xmp_track) +
				sizeof(struct xmp_event) * 64 - 1, 1);
	mod->xxt[0]->rows = 64;

	/* Alloc rest of the tracks */
	for (i = 1; i < mod->trk; i++) {
		uint8 t1, t2, t3;
		int size;

		mod->xxt[i] = calloc(sizeof(struct xmp_track) +
			sizeof(struct xmp_event) * 64 - 1, 1);
		mod->xxt[i]->rows = 64;

		size = read24l(f);
/*printf("TRACK %d SIZE %d\n", i, size);*/

		for (j = 0; j < size; j++) {
			t1 = read8(f);			/* row */
			t2 = read8(f);			/* type */
			t3 = read8(f);			/* parameter */
/*printf("track %d row %d: %02x %02x %02x\n", i, t1, t1, t2, t3);*/

			if (t1 == 0xff && t2 == 0xff && t3 == 0xff)
				break;

			event = &mod->xxt[i]->event[t1];

			if (t2 < 0x7f) {		/* note */
				if (t2 > 0)
					event->note = t2 + 1;
				event->vol = t3;
			} else if (t2 == 0x7f) {	/* copy previous */
				memcpy(event, &mod->xxt[i]->event[t1 - 1],
					sizeof(struct xmp_event));
			} else if (t2 == 0x80) {	/* instrument */
				event->ins = t3 + 1;
			} else  {			/* effects */
				uint8 fxp, fxt;

				fxp = fxt = 0;

				switch (t2) {
				case 0x81:
					fxt = FX_SPEED;
					fxp = t3;
					break;
				case 0x82:
					if ((int8)t3 > 0) {
						fxt = FX_VOLSLIDE;
						fxp = t3 << 4;
					} else {
						fxt = FX_VOLSLIDE;
						fxp = -(int8)t3 & 0x0f;
					}
					break;
				case 0x83:
					event->vol = t3;
					break;
				case 0x84:
					/* AT: Not explained for 0x84, pitch
					 * slide, value 0x00 corresponds to
					 * S3M E00 and 0x80 stands for S3M F00
					 * (I checked with M2AMF)
					 */
					if ((int8)t3 >= 0) {
						fxt = FX_PORTA_DN;
						fxp = t3;
					} if (t3 == 0x80) {
						fxt = FX_PORTA_UP;
						fxp = 0;
					} else {
						fxt = FX_PORTA_UP;
						fxp = -(int8)t3;
					}
					break;
				case 0x85:
					/* porta abs -- unknown */
					break;
				case 0x86:
					fxt = FX_TONEPORTA;
					fxp = t3;
					break;

				/* AT: M2AMF maps both tremolo and tremor to
				 * 0x87. Since tremor is only found in certain
				 * formats, maybe it would be better to
				 * consider it is a tremolo.
				 */
				case 0x87:
					fxt = FX_TREMOLO;
					fxp = t3;
					break;
				case 0x88:
					fxt = FX_ARPEGGIO;
					fxp = t3;
					break;
				case 0x89:
					fxt = FX_VIBRATO;
					fxp = t3;
					break;
				case 0x8a:
					if ((int8)t3 > 0) {
						fxt = FX_TONE_VSLIDE;
						fxp = t3 << 4;
					} else {
						fxt = FX_TONE_VSLIDE;
						fxp = -(int8)t3 & 0x0f;
					}
					break;
				case 0x8b:
					if ((int8)t3 > 0) {
						fxt = FX_VIBRA_VSLIDE;
						fxp = t3 << 4;
					} else {
						fxt = FX_VIBRA_VSLIDE;
						fxp = -(int8)t3 & 0x0f;
					}
					break;
				case 0x8c:
					fxt = FX_BREAK;
					fxp = t3;
					break;
				case 0x8d:
					fxt = FX_JUMP;
					fxp = t3;
					break;
				case 0x8e:
					/* sync -- unknown */
					break;
				case 0x8f:
					fxt = FX_EXTENDED;
					fxp = (EX_RETRIG << 4) | (t3 & 0x0f);
					break;
				case 0x90:
					fxt = FX_OFFSET;
					fxp = t3;
					break;
				case 0x91:
					if ((int8)t3 > 0) {
						fxt = FX_EXTENDED;
						fxp = (EX_F_VSLIDE_UP << 4) |
							(t3 & 0x0f);
					} else {
						fxt = FX_EXTENDED;
						fxp = (EX_F_VSLIDE_DN << 4) |
							(t3 & 0x0f);
					}
					break;
				case 0x92:
					if ((int8)t3 > 0) {
						fxt = FX_PORTA_DN;
						fxp = 0xf0 | (fxp & 0x0f);
					} else {
						fxt = FX_PORTA_UP;
						fxp = 0xf0 | (fxp & 0x0f);
					}
					break;
				case 0x93:
					fxt = FX_EXTENDED;
					fxp = (EX_DELAY << 4) | (t3 & 0x0f);
					break;
				case 0x94:
					fxt = FX_EXTENDED;
					fxp = (EX_CUT << 4) | (t3 & 0x0f);
					break;
				case 0x95:
					fxt = FX_SPEED;
					if (t3 < 0x21)
						t3 = 0x21;
					fxp = t3;
					break;
				case 0x96:
					if ((int8)t3 > 0) {
						fxt = FX_PORTA_DN;
						fxp = 0xe0 | (fxp & 0x0f);
					} else {
						fxt = FX_PORTA_UP;
						fxp = 0xe0 | (fxp & 0x0f);
					}
					break;
				case 0x97:
					fxt = FX_SETPAN;
					fxp = 0x80 + 2 * (int8)t3;
					break;
				}

				event->fxt = fxt;
				event->fxp = fxp;
			}

		}
	}


	/* Samples */

	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < mod->ins; i++) {
		load_sample(f, SAMPLE_FLAG_UNS, &mod->xxs[mod->xxi[i].sub[0].sid], NULL);
	}

	m->quirk |= QUIRK_FINEFX;

	return 0;
}
