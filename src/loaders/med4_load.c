/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

/*
 * MED 2.13 is in Fish disk #424 and has a couple of demo modules, get it
 * from ftp://ftp.funet.fi/pub/amiga/fish/401-500/ff424. Alex Van Starrex's
 * HappySong MED4 is in ff401. MED 3.00 is in ff476.
 */

#include <assert.h>
#include "med.h"
#include "loader.h"
#include "med_extras.h"

#define MAGIC_MED4	MAGIC4('M','E','D',4)
#undef MED4_DEBUG

static int med4_test(HIO_HANDLE *, char *, const int);
static int med4_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader med4_loader = {
	"MED 2.10 MED4 (MED)",
	med4_test,
	med4_load
};

static int med4_test(HIO_HANDLE *f, char *t, const int start)
{
	if (hio_read32b(f) !=  MAGIC_MED4)
		return -1;

	read_title(f, t, 0);

	return 0;
}

#ifdef DEBUG
static const char *inst_type[] = {
	"HYB",		/* -2 */
	"SYN",		/* -1 */
	"SMP",		/*  0 */
	"I5O",		/*  1 */
	"I3O",		/*  2 */
	"I2O",		/*  3 */
	"I4O",		/*  4 */
	"I6O",		/*  5 */
	"I7O",		/*  6 */
	"EXT",		/*  7 */
};
#endif


static void fix_effect(struct xmp_event *event)
{
	switch (event->fxt) {
	case 0x00:	/* arpeggio */
	case 0x01:	/* slide up */
	case 0x02:	/* slide down */
	case 0x03:	/* portamento */
	case 0x04:	/* vibrato? */
		break;
	case 0x0c:	/* set volume (BCD) */
		event->fxp = MSN(event->fxp) * 10 +
					LSN(event->fxp);
		break;
	case 0x0d:	/* volume slides */
		event->fxt = FX_VOLSLIDE;
		break;
	case 0x0f:	/* tempo/break */
		if (event->fxp == 0)
			event->fxt = FX_BREAK;
		if (event->fxp == 0xff) {
			event->fxp = event->fxt = 0;
			event->vol = 1;
		} else if (event->fxp == 0xfe) {
			event->fxp = event->fxt = 0;
		} else if (event->fxp == 0xf1) {
			event->fxt = FX_EXTENDED;
			event->fxp = (EX_RETRIG << 4) | 3;
		} else if (event->fxp == 0xf2) {
			event->fxt = FX_EXTENDED;
			event->fxp = (EX_CUT << 4) | 3;
		} else if (event->fxp == 0xf3) {
			event->fxt = FX_EXTENDED;
			event->fxp = (EX_DELAY << 4) | 3;
		} else if (event->fxp > 10) {
			event->fxt = FX_S3M_BPM;
			event->fxp = 125 * event->fxp / 33;
		}
		break;
	default:
		event->fxp = event->fxt = 0;
	}
}

static inline uint8 read4(HIO_HANDLE *f, int *read4_ctl)
{
	static uint8 b = 0;
	uint8 ret;

	if (*read4_ctl & 0x01) {
		ret = b & 0x0f;
	} else {
		b = hio_read8(f);
		ret = b >> 4;
	}

	*read4_ctl ^= 0x01;

	return ret;
}

static inline uint16 read12b(HIO_HANDLE *f, int *read4_ctl)
{
	uint32 a, b, c;

	a = read4(f, read4_ctl);
	b = read4(f, read4_ctl);
	c = read4(f, read4_ctl);

	return (a << 8) | (b << 4) | c;
}

struct temp_inst {
	char name[32];
	int loop_start;
	int loop_end;
	int volume;
	int transpose;
};

struct temp_inst temp_inst[32];

static int med4_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j, k, y;
	uint32 m0, mask;
	int transp, masksz;
	int pos, vermaj, vermin;
	uint8 trkvol[16], buf[1024];
	struct xmp_event *event;
	int flags, hexvol = 0;
	int num_ins, num_smp;
	int smp_idx;
	int tempo;
	int read4_ctl;
	
	LOAD_INIT();

	hio_read32b(f);		/* Skip magic */

	vermaj = 2;
	vermin = 10;

	/*
	 * Check if we have a MEDV chunk at the end of the file
	 */
	pos = hio_tell(f);
	hio_seek(f, 0, SEEK_END);
	if (hio_tell(f) > 2000) {
		hio_seek(f, -1023, SEEK_CUR);
		hio_read(buf, 1, 1024, f);
		for (i = 0; i < 1012; i++) {
			if (!memcmp(buf + i, "MEDV\000\000\000\004", 8)) {
				vermaj = *(buf + i + 10);
				vermin = *(buf + i + 11);
				break;
			}
		}
	}
	hio_seek(f, start + pos, SEEK_SET);

	snprintf(mod->type, XMP_NAME_SIZE, "MED %d.%02d MED4", vermaj, vermin);

	m0 = hio_read8(f);

	mask = masksz = 0;
	for (i = 0; i < 8; i++, m0 <<= 1) {
		if (m0 & 0x80) {
			mask <<= 8;
			mask |= hio_read8(f);
			masksz++;
		}
	}
	mask <<= (32 - masksz * 8);
	/*printf("m0=%x mask=%x\n", m0, mask);*/

	/* read instrument names in temporary space */

	num_ins = 0;
	for (i = 0; i < 32; i++, mask <<= 1) {
		uint8 c, size, buf[40];
		uint16 loop_len = 0;

		memset(&temp_inst[i], 0, sizeof (struct temp_inst));

		if (~mask & 0x80000000)
			continue;

		/* read flags */
	   	c = hio_read8(f);

		/* read instrument name */
		size = hio_read8(f);
		for (j = 0; j < size; j++)
			buf[j] = hio_read8(f);
		buf[j] = 0;
#ifdef MED4_DEBUG
		printf("%02x %02x %2d [%s]\n", i, c, size, buf);
#endif

		temp_inst[i].volume = 0x40;

		if ((c & 0x01) == 0)
			temp_inst[i].loop_start = hio_read16b(f) << 1;
		if ((c & 0x02) == 0)
			loop_len = hio_read16b(f) << 1;
		if ((c & 0x04) == 0)	/* ? Tanko2 (MED 3.00 demo) */
			hio_read8(f);
		if ((c & 0x08) == 0)	/* Tim Newsham's "span" */
			hio_read8(f);
		if ((c & 0x20) == 0)
			temp_inst[i].volume = hio_read8(f);
		if ((c & 0x40) == 0)
			temp_inst[i].transpose = hio_read8s(f);

		temp_inst[i].loop_end = temp_inst[i].loop_start + loop_len;

		copy_adjust(temp_inst[i].name, buf, 32);

		num_ins++;
	}

	mod->pat = hio_read16b(f);
	mod->len = hio_read16b(f);
#ifdef MED4_DEBUG
	printf("pat=%x len=%x\n", mod->pat, mod->len);
#endif
	hio_read(mod->xxo, 1, mod->len, f);

	/* From MED V3.00 docs:
	 *
	 * The left proportional gadget controls the primary tempo. It canbe
	 * 1 - 240. The bigger the number, the faster the speed. Note that
	 * tempos 1 - 10 are Tracker-compatible (but obsolete, because
	 * secondary tempo can be used now).
	 */
	tempo = hio_read16b(f);
	if (tempo <= 10) {
		mod->spd = tempo;
		mod->bpm = 125;
	} else {
		mod->bpm = 125 * tempo / 33;
	}
        transp = hio_read8s(f);
        hio_read8s(f);
        flags = hio_read8s(f);
	mod->spd = hio_read8(f);

	if (~flags & 0x20)	/* sliding */
		m->quirk |= QUIRK_VSALL | QUIRK_PBALL;

	if (flags & 0x10)	/* dec/hex volumes */
		hexvol = 1;	/* not implemented */

	/* This is just a guess... */
	if (vermaj == 2)	/* Happy.med has tempo 5 but loads as 6 */
		mod->spd = flags & 0x20 ? 5 : 6;

	hio_seek(f, 20, SEEK_CUR);

	hio_read(trkvol, 1, 16, f);
	hio_read8(f);		/* master vol */

	MODULE_INFO();

	D_(D_INFO "Play transpose: %d", transp);

	for (i = 0; i < 32; i++)
		temp_inst[i].transpose += transp;

	hio_read8(f);
	mod->chn = hio_read8(f);;
	hio_seek(f, -2, SEEK_CUR);
	mod->trk = mod->chn * mod->pat;

	if (pattern_init(mod) < 0)
		return -1;

	/* Load and convert patterns */
	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		int size, plen, rows;
		uint8 ctl[4], chmsk, chn;
		uint32 linemask[8], fxmask[8], x;
		int num_masks;

#ifdef MED4_DEBUG
		printf("\n===== PATTERN %d =====\n", i);
		printf("offset = %lx\n", hio_tell(f));
#endif

		size = hio_read8(f);	/* pattern control block */
		chn = hio_read8(f);
		rows = (int)hio_read8(f) + 1;
		plen = hio_read16b(f);

#ifdef MED4_DEBUG
		printf("size = %02x\n", size);
		printf("chn  = %01x\n", chn);
		printf("rows = %01x\n", rows);
		printf("plen = %04x\n", plen);
#endif
		/* read control byte */
		for (j = 0; j < 4; j++) {
			if (rows > j * 64)
				ctl[j] = hio_read8(f);
			else
				break;
#ifdef MED4_DEBUG
			printf("ctl[%d] = %02x\n", j, ctl[j]);

#endif
		}

		if (pattern_tracks_alloc(mod, i, rows) < 0)
			return -1;

		/* initialize masks */
		for (y = 0; y < 8; y++) {
			linemask[y] = 0;
			fxmask[y] = 0;
		}

		/* read masks */
		num_masks = 0;
		for (y = 0; y < 8; y++) {
			if (rows > y * 32) {
				int c = ctl[y / 2];
				int s = 4 * (y % 2);
				linemask[y] = c & (0x80 >> s) ? ~0 :
					      c & (0x40 >> s) ? 0 : hio_read32b(f);
				fxmask[y]   = c & (0x20 >> s) ? ~0 :
					      c & (0x10 >> s) ? 0 : hio_read32b(f);
				num_masks++;
#ifdef MED4_DEBUG
				printf("linemask[%d] = %08x\n", y, linemask[y]);
				printf("fxmask[%d]   = %08x\n", y, fxmask[y]);
#endif
			} else {
				break;
			}
		}


		/* check block end */
		if (hio_read8(f) != 0xff) {
			D_(D_CRIT "error: module is corrupted");
			return -1;
		}

		read4_ctl = 0;

		for (y = 0; y < num_masks; y++) {

		for (j = 0; j < 32; j++) {
			int line = y * 32 + j;

			if (linemask[y] & 0x80000000) {
				chmsk = read4(f, &read4_ctl);
				for (k = 0; k < 4; k++, chmsk <<= 1) {
					event = &EVENT(i, k, line);

					if (chmsk & 0x08) {
						x = read12b(f, &read4_ctl);
						event->note = x >> 4;
						if (event->note)
							event->note += 48;
						event->ins  = x & 0x0f;
					}
				}
			}

			if (fxmask[y] & 0x80000000) {
				chmsk = read4(f, &read4_ctl);
				for (k = 0; k < 4; k++, chmsk <<= 1) {
					event = &EVENT(i, k, line);

					if (chmsk & 0x08) {
						x = read12b(f, &read4_ctl);
						event->fxt = x >> 8;
						event->fxp = x & 0xff;
						fix_effect(event);
					}
				}
			}

#ifdef MED4_DEBUG
			printf("%03d ", line);
			for (k = 0; k < 4; k++) {
				event = &EVENT(i, k, line);
				if (event->note)
					printf("%03d", event->note);
				else
					printf("---");
				printf(" %1x%1x%02x ",
					event->ins, event->fxt, event->fxp);
			}
			printf("\n");
#endif

			linemask[y] <<= 1;
			fxmask[y] <<= 1;
		}

		}
	}

	mod->ins =  num_ins;

	if (med_new_module_extras(m) != 0)
		return -1;

	/*
	 * Load samples
	 */
	mask = hio_read32b(f);
	hio_read16b(f);

#ifdef MED4_DEBUG
	printf("instrument mask: %08x\n", mask);
#endif

	hio_read16b(f);	/* ?!? */

	mask <<= 1;	/* no instrument #0 */

	/* obtain number of samples */
	pos = hio_tell(f);
	num_smp = 0;
	{
		int _len, _type, _mask = mask;
		for (i = 0; i < 32; i++, _mask <<= 1) {
			int _pos;

			if (~_mask & 0x80000000)
				continue;
			hio_read16b(f);
			_len = hio_read16b(f);
			_type = (int16)hio_read16b(f);

			_pos = hio_tell(f);

			if (_type == 0 || _type == -2) {
				num_smp++;
			} else if (_type == -1) {
				hio_seek(f, 20, SEEK_CUR);
				num_smp += hio_read16b(f);
			}

			hio_seek(f, _pos + _len, SEEK_SET);
		}
	}
	hio_seek(f, pos, SEEK_SET);

	mod->smp = num_smp;

	if (instrument_init(mod) < 0)
		return -1;

	D_(D_INFO "Instruments: %d", mod->ins);

	smp_idx = 0;
	for (i = 0; i < num_ins; i++, mask <<= 1) {
		int x1, length, type;
		struct SynthInstr synth;

		if (~mask & 0x80000000)
			continue;

		x1 = hio_read16b(f);
		length = hio_read16b(f);
		type = (int16)hio_read16b(f);	/* instrument type */

		strncpy((char *)mod->xxi[i].name, temp_inst[i].name, 32);

		D_(D_INFO "\n[%2X] %-32.32s %d", i, mod->xxi[i].name, type);

		/* This is very similar to MMD1 synth/hybrid instruments,
		 * but just different enough to be reimplemented here.
		 */

		if (type == -2) {			/* Hybrid */
			int length, type;
			int pos = hio_tell(f);

			hio_read32b(f);	/* ? - MSH 00 */
			hio_read16b(f);	/* ? - ffff */
			hio_read16b(f);	/* ? - 0000 */
			hio_read16b(f);	/* ? - 0000 */
			synth.rep = hio_read16b(f);		/* ? */
			synth.replen = hio_read16b(f);	/* ? */
			synth.voltbllen = hio_read16b(f);
			synth.wftbllen = hio_read16b(f);
			synth.volspeed = hio_read8(f);
			synth.wfspeed = hio_read8(f);
			synth.wforms = hio_read16b(f);
			hio_read(synth.voltbl, 1, synth.voltbllen, f);;
			hio_read(synth.wftbl, 1, synth.wftbllen, f);;

			hio_seek(f,  pos + hio_read32b(f), SEEK_SET);
			length = hio_read32b(f);
			type = hio_read16b(f);

			if (med_new_instrument_extras(&mod->xxi[i]) != 0)
				return -1;

			mod->xxi[i].nsm = 1;
			if (subinstrument_alloc(mod, i, 1) < 0)
				return -1;

			MED_INSTRUMENT_EXTRAS(mod->xxi[i])->vts = synth.volspeed;
			MED_INSTRUMENT_EXTRAS(mod->xxi[i])->wts = synth.wfspeed;
			mod->xxi[i].sub[0].pan = 0x80;
			mod->xxi[i].sub[0].vol = temp_inst[i].volume;
			mod->xxi[i].sub[0].xpo = temp_inst[i].transpose;
			mod->xxi[i].sub[0].sid = smp_idx;
			mod->xxi[i].sub[0].fin = 0 /*exp_smp.finetune*/;
			mod->xxs[smp_idx].len = length;
			mod->xxs[smp_idx].lps = temp_inst[i].loop_start;
			mod->xxs[smp_idx].lpe = temp_inst[i].loop_end;
			mod->xxs[smp_idx].flg = temp_inst[i].loop_end > 1 ?
						XMP_SAMPLE_LOOP : 0;

			D_(D_INFO "  %05x %05x %05x %02x %+03d",
				       mod->xxs[smp_idx].len, mod->xxs[smp_idx].lps,
				       mod->xxs[smp_idx].lpe, mod->xxi[i].sub[0].vol,
				       mod->xxi[i].sub[0].xpo /*,
				       mod->xxi[i].sub[0].fin >> 4*/);

			if (load_sample(m, f, 0, &mod->xxs[smp_idx], NULL) < 0)
				return -1;

			smp_idx++;

			if (mmd_alloc_tables(m, i, &synth) != 0)
				return -1;

			continue;
		}

		if (type == -1) {		/* Synthetic */
			int pos = hio_tell(f);

			hio_read32b(f);	/* ? - MSH 00 */
			hio_read16b(f);	/* ? - ffff */
			hio_read16b(f);	/* ? - 0000 */
			hio_read16b(f);	/* ? - 0000 */
			synth.rep = hio_read16b(f);		/* ? */
			synth.replen = hio_read16b(f);	/* ? */
			synth.voltbllen = hio_read16b(f);
			synth.wftbllen = hio_read16b(f);
			synth.volspeed = hio_read8(f);
			synth.wfspeed = hio_read8(f);
			synth.wforms = hio_read16b(f);
			hio_read(synth.voltbl, 1, synth.voltbllen, f);;
			hio_read(synth.wftbl, 1, synth.wftbllen, f);;
			for (j = 0; j < synth.wforms; j++)
				synth.wf[j] = hio_read32b(f);

			D_(D_INFO "  VS:%02x WS:%02x WF:%02x %02x %+03d",
					synth.volspeed, synth.wfspeed,
					synth.wforms & 0xff,
					temp_inst[i].volume,
					temp_inst[i].transpose /*,
					exp_smp.finetune*/);

			if (synth.wforms == 0xffff)	
				continue;

			if (med_new_instrument_extras(&mod->xxi[i]) != 0)
				return -1;

			mod->xxi[i].nsm = synth.wforms;
			if (subinstrument_alloc(mod, i, synth.wforms) < 0)
				return -1;

			MED_INSTRUMENT_EXTRAS(mod->xxi[i])->vts = synth.volspeed;
			MED_INSTRUMENT_EXTRAS(mod->xxi[i])->wts = synth.wfspeed;

			for (j = 0; j < synth.wforms; j++) {
				mod->xxi[i].sub[j].pan = 0x80;
				mod->xxi[i].sub[j].vol = temp_inst[i].volume;
				mod->xxi[i].sub[j].xpo = temp_inst[i].transpose - 24;
				mod->xxi[i].sub[j].sid = smp_idx;
				mod->xxi[i].sub[j].fin = 0 /*exp_smp.finetune*/;

				hio_seek(f, pos + synth.wf[j], SEEK_SET);
/*printf("pos=%lx tell=%lx ", pos, hio_tell(f));*/

				mod->xxs[smp_idx].len = hio_read16b(f) * 2;
/*printf("idx=%x len=%x\n", synth.wf[j], mod->xxs[smp_idx].len);*/
				mod->xxs[smp_idx].lps = 0;
				mod->xxs[smp_idx].lpe = mod->xxs[smp_idx].len;
				mod->xxs[smp_idx].flg = XMP_SAMPLE_LOOP;

				if (load_sample(m, f, 0, &mod->xxs[smp_idx],
								NULL) < 0) {
					return -1;
				}

				smp_idx++;
			}

			if (mmd_alloc_tables(m, i, &synth) != 0)
				return -1;

			hio_seek(f, pos + length, SEEK_SET);
			continue;
		}

		if (type != 0) {
			hio_seek(f, length, SEEK_CUR);
			continue;
		}

                /* instr type is sample */
                mod->xxi[i].nsm = 1;
		if (subinstrument_alloc(mod, i, 1) < 0)
			return -1;
		
		mod->xxi[i].sub[0].vol = temp_inst[i].volume;
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].xpo = temp_inst[i].transpose;
		mod->xxi[i].sub[0].sid = smp_idx;

		mod->xxs[smp_idx].len = length;
		mod->xxs[smp_idx].lps = temp_inst[i].loop_start;
		mod->xxs[smp_idx].lpe = temp_inst[i].loop_end;
		mod->xxs[smp_idx].flg = temp_inst[i].loop_end > 1 ?
						XMP_SAMPLE_LOOP : 0;

		D_(D_INFO "  %04x %04x %04x %c V%02x %+03d",
			mod->xxs[smp_idx].len, mod->xxs[smp_idx].lps,
			mod->xxs[smp_idx].lpe,
			mod->xxs[smp_idx].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
			mod->xxi[i].sub[0].vol, mod->xxi[i].sub[0].xpo);

		if (load_sample(m, f, 0, &mod->xxs[smp_idx], NULL) < 0)
			return -1;

		smp_idx++;
	}

	hio_read16b(f);	/* unknown */

	/* IFF-like section */
	while (!hio_eof(f)) {
		int32 id, size, s2, pos, ver;

		if ((id = hio_read32b(f)) < 0)
			break;

		if ((size = hio_read32b(f)) < 0)
			break;

		pos = hio_tell(f);

		switch (id) {
		case MAGIC4('M','E','D','V'):
			ver = hio_read32b(f);
			D_(D_INFO "MED Version: %d.%0d\n",
					(ver & 0xff00) >> 8, ver & 0xff);
			break;
		case MAGIC4('A','N','N','O'):
			/* annotation */
			s2 = size < 1023 ? size : 1023;
			hio_read(buf, 1, s2, f);
			buf[s2] = 0;
			D_(D_INFO "Annotation: %s\n", buf);
			break;
		case MAGIC4('H','L','D','C'):
			/* hold & decay */
			break;
		}

		hio_seek(f, pos + size, SEEK_SET);
	}

	return 0;
}
