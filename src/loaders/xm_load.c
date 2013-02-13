/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

/*
 * Fri, 26 Jun 1998 17:45:59 +1000  Andrew Leahy <alf@cit.nepean.uws.edu.au>
 * Finally got it working on the DEC Alpha running DEC UNIX! In the pattern
 * reading loop I found I was getting "0" for (p-patbuf) and "0" for
 * xph.datasize, the next if statement (where it tries to read the patbuf)
 * would then cause a seg_fault.
 *
 * Sun Sep 27 12:07:12 EST 1998  Claudio Matsuoka <claudio@pos.inf.ufpr.br>
 * Extended Module 1.02 stores data in a different order, we must handle
 * this accordingly. MAX_SAMP used as a workaround to check the number of
 * samples recognized by the player.
 */

#include "loader.h"
#include "xm.h"

#define MAX_SAMP 1024

static int xm_test (FILE *, char *, const int);
static int xm_load (struct module_data *, FILE *, const int);

const struct format_loader xm_loader = {
    "Fast Tracker II (XM)",
    xm_test,
    xm_load
};

static int xm_test(FILE *f, char *t, const int start)
{
    char buf[20];

    if (fread(buf, 1, 17, f) < 17)		/* ID text */
	return -1;

    if (memcmp(buf, "Extended Module: ", 17))
	return -1;

    read_title(f, t, 20);

    return 0;
}

static int xm_load(struct module_data *m, FILE *f, const int start)
{
    struct xmp_module *mod = &m->mod;
    int i, j, r;
    int sample_num = 0;
    uint8 *patbuf, *pat, b;
    struct xmp_event *event;
    struct xm_file_header xfh;
    struct xm_pattern_header xph;
    struct xm_instrument_header xih;
    struct xm_instrument xi;
    struct xm_sample_header xsh[16];
    char tracker_name[21];

    LOAD_INIT();

    fread(&xfh.id, 17, 1, f);		/* ID text */
    fread(&xfh.name, 20, 1, f);		/* Module name */
    read8(f);				/* 0x1a */
    fread(&xfh.tracker, 20, 1, f);	/* Tracker name */
    xfh.version = read16l(f);		/* Version number, minor-major */
    xfh.headersz = read32l(f);		/* Header size */
    xfh.songlen = read16l(f);		/* Song length */
    xfh.restart = read16l(f);		/* Restart position */
    xfh.channels = read16l(f);		/* Number of channels */
    xfh.patterns = read16l(f);		/* Number of patterns */
    xfh.instruments = read16l(f);	/* Number of instruments */
    xfh.flags = read16l(f);		/* 0=Amiga freq table, 1=Linear */
    xfh.tempo = read16l(f);		/* Default tempo */
    xfh.bpm = read16l(f);		/* Default BPM */

    /* Honor header size -- needed by BoobieSqueezer XMs */
    fread(&xfh.order, xfh.headersz - 0x14, 1, f); /* Pattern order table */

    strncpy(mod->name, (char *)xfh.name, 20);

    mod->len = xfh.songlen;
    mod->rst = xfh.restart;
    mod->chn = xfh.channels;
    mod->pat = xfh.patterns;
    mod->trk = mod->chn * mod->pat + 1;
    mod->ins = xfh.instruments;
    mod->spd = xfh.tempo;
    mod->bpm = xfh.bpm;

    m->quirk |= xfh.flags & XM_LINEAR_PERIOD_MODE ? QUIRK_LINEAR : 0;

    memcpy(mod->xxo, xfh.order, mod->len);
    tracker_name[20] = 0;
    snprintf(tracker_name, 20, "%-20.20s", xfh.tracker);
    for (i = 20; i >= 0; i--) {
	if (tracker_name[i] == 0x20)
	    tracker_name[i] = 0;
	if (tracker_name[i])
	    break;
    }

    if (xfh.headersz == 0x0113) {
	strcpy(tracker_name, "unknown tracker");
    } else if (*tracker_name == 0) {
	strcpy(tracker_name, "Digitrakker");	/* best guess */
    }

    /* See MMD1 loader for explanation */
    if (!strncmp(tracker_name, "MED2XM by J.Pynnone", 19))
	if (mod->bpm <= 10)
	    mod->bpm = 125 * (0x35 - mod->bpm * 2) / 33;

    if (!strncmp(tracker_name, "FastTracker v 2.00", 18))
	strcpy(tracker_name, "old ModPlug Tracker");

    snprintf(mod->type, XMP_NAME_SIZE, "%s XM %d.%02d",
			tracker_name, xfh.version >> 8, xfh.version & 0xff);

    MODULE_INFO();

    /* Honor header size */

    fseek(f, start + xfh.headersz + 60, SEEK_SET);

    /* XM 1.02/1.03 has a different structure. This is a hack to re-order
     * the loader and recognize 1.02 files correctly.
     */
    if (xfh.version <= 0x0103)
	goto load_instruments;

load_patterns:
    PATTERN_INIT();

    D_(D_INFO "Stored patterns: %d", mod->pat);

    /* Endianism fixed by Miodrag Vallat <miodrag@multimania.com>
     * Mon, 04 Jan 1999 11:17:20 +0100
     */
    for (i = 0; i < mod->pat; i++) {
	xph.length = read32l(f);
	xph.packing = read8(f);
	xph.rows = xfh.version > 0x0102 ? read16l(f) : read8(f) + 1;
	xph.datasize = read16l(f);

	PATTERN_ALLOC(i);
	if (!(r = mod->xxp[i]->rows = xph.rows))
	    r = mod->xxp[i]->rows = 0x100;
	TRACK_ALLOC(i);

	if (xph.datasize) {
	    pat = patbuf = calloc(1, xph.datasize);
	    fread (patbuf, 1, xph.datasize, f);
	    for (j = 0; j < (mod->chn * r); j++) {
		if ((pat - patbuf) >= xph.datasize)
		    break;
		event = &EVENT(i, j % mod->chn, j / mod->chn);
		if ((b = *pat++) & XM_EVENT_PACKING) {
		    if (b & XM_EVENT_NOTE_FOLLOWS)
			event->note = *pat++;
		    if (b & XM_EVENT_INSTRUMENT_FOLLOWS) {
			if (*pat & XM_END_OF_SONG)
			    break;
			event->ins = *pat++;
		    }
		    if (b & XM_EVENT_VOLUME_FOLLOWS)
			event->vol = *pat++;
		    if (b & XM_EVENT_FXTYPE_FOLLOWS) {
			event->fxt = *pat++;
#if 0
			if (event->fxt == FX_GLOBALVOL)
			    event->fxt = FX_TRK_VOL;
			if (event->fxt == FX_G_VOLSLIDE)
			    event->fxt = FX_TRK_VSLIDE;
#endif
		    }
		    if (b & XM_EVENT_FXPARM_FOLLOWS)
			event->fxp = *pat++;
		} else {
		    event->note = b;
		    event->ins = *pat++;
		    event->vol = *pat++;
		    event->fxt = *pat++;
		    event->fxp = *pat++;
		}

		if (event->note == 0x61)
		    event->note = XMP_KEY_OFF;
		else if (event->note > 0)
		    event->note += 12;

		if (!event->vol)
		    continue;

		/* Volume set */
		if ((event->vol >= 0x10) && (event->vol <= 0x50)) {
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
		    event->f2p = (EX_F_VSLIDE_DN << 4) | (event->vol - 0x80);
		    break;
		case 0x09:	/* Fine volume slide up */
		    event->f2t = FX_EXTENDED;
		    event->f2p = (EX_F_VSLIDE_UP << 4) | (event->vol - 0x90);
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
		    event->f2p = ((event->vol - 0xc0) << 4) + 8;
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
	    free (patbuf);
	}
    }

    /* Alloc one extra pattern */
    PATTERN_ALLOC(i);

    mod->xxp[i]->rows = 64;
    mod->xxt[i * mod->chn] = calloc (1, sizeof (struct xmp_track) +
	sizeof (struct xmp_event) * 64);
    mod->xxt[i * mod->chn]->rows = 64;
    for (j = 0; j < mod->chn; j++)
	mod->xxp[i]->index[j] = i * mod->chn;
    mod->pat++;

    if (xfh.version <= 0x0103) {
	goto load_samples;
    }

load_instruments:
    D_(D_INFO "Instruments: %d", mod->ins);

    /* ESTIMATED value! We don't know the actual value at this point */
    mod->smp = MAX_SAMP;

    INSTRUMENT_INIT();

    for (i = 0; i < mod->ins; i++) {

	xih.size = read32l(f);			/* Instrument size */

	/* Modules converted with MOD2XM 1.0 always say we have 31
	 * instruments, but file may end abruptly before that. This test
	 * will not work if file has trailing garbage.
	 */
	if (feof(f)) {
		mod->ins = i;
		break;
	}

	fread(&xih.name, 22, 1, f);		/* Instrument name */
	xih.type = read8(f);			/* Instrument type (always 0) */
	xih.samples = read16l(f);		/* Number of samples */
	xih.sh_size = read32l(f);		/* Sample header size */

	/* Sanity check */
	if (xih.samples > 0x10 || (xih.samples > 0 && xih.sh_size > 0x100)) {
		mod->ins = i;
		break;
	}

	copy_adjust(mod->xxi[i].name, xih.name, 22);

	mod->xxi[i].nsm = xih.samples;
	if (mod->xxi[i].nsm > 16)
	    mod->xxi[i].nsm = 16;

	D_(D_INFO "[%2X] %-22.22s %2d", i, mod->xxi[i].name, mod->xxi[i].nsm);

	if (mod->xxi[i].nsm) {
	    mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), mod->xxi[i].nsm);

	    /* for BoobieSqueezer (see http://boobie.rotfl.at/)
	     * It works pretty much the same way as Impulse Tracker's sample
	     * only mode, where it will strip off the instrument data.
	     */
	    if (xih.size == 0x26) {
		read8(f);
		read32l(f);
		memset(&xi, 0, sizeof (struct xm_instrument));
	    } else {
		fread(&xi.sample, 96, 1, f);	/* Sample map */
		for (j = 0; j < 24; j++)
		    xi.v_env[j] = read16l(f);	/* Points for volume envelope */
		for (j = 0; j < 24; j++)
		    xi.p_env[j] = read16l(f);	/* Points for pan envelope */
		xi.v_pts = read8(f);		/* Number of volume points */
		xi.p_pts = read8(f);		/* Number of pan points */
		xi.v_sus = read8(f);		/* Volume sustain point */
		xi.v_start = read8(f);		/* Volume loop start point */
		xi.v_end = read8(f);		/* Volume loop end point */
		xi.p_sus = read8(f);		/* Pan sustain point */
		xi.p_start = read8(f);		/* Pan loop start point */
		xi.p_end = read8(f);		/* Pan loop end point */
		xi.v_type = read8(f);		/* Bit 0:On 1:Sustain 2:Loop */
		xi.p_type = read8(f);		/* Bit 0:On 1:Sustain 2:Loop */
		xi.y_wave = read8(f);		/* Vibrato waveform */
		xi.y_sweep = read8(f);		/* Vibrato sweep */
		xi.y_depth = read8(f);		/* Vibrato depth */
		xi.y_rate = read8(f);		/* Vibrato rate */
		xi.v_fade = read16l(f);		/* Volume fadeout */

		/* Skip reserved space */
		fseek(f, (int)xih.size - 33 /*sizeof (xih)*/ - 208 /*sizeof (xi)*/, SEEK_CUR);

		/* Envelope */
		mod->xxi[i].rls = xi.v_fade;
		mod->xxi[i].aei.npt = xi.v_pts;
		mod->xxi[i].aei.sus = xi.v_sus;
		mod->xxi[i].aei.lps = xi.v_start;
		mod->xxi[i].aei.lpe = xi.v_end;
		mod->xxi[i].aei.flg = xi.v_type;
		mod->xxi[i].pei.npt = xi.p_pts;
		mod->xxi[i].pei.sus = xi.p_sus;
		mod->xxi[i].pei.lps = xi.p_start;
		mod->xxi[i].pei.lpe = xi.p_end;
		mod->xxi[i].pei.flg = xi.p_type;

		if (mod->xxi[i].aei.npt <= 0 || mod->xxi[i].aei.npt > XMP_MAX_ENV_POINTS)
		    mod->xxi[i].aei.flg &= ~XMP_ENVELOPE_ON;

		if (mod->xxi[i].pei.npt <= 0 || mod->xxi[i].pei.npt > XMP_MAX_ENV_POINTS)
		    mod->xxi[i].pei.flg &= ~XMP_ENVELOPE_ON;

		memcpy(mod->xxi[i].aei.data, xi.v_env, mod->xxi[i].aei.npt * 4);
		memcpy(mod->xxi[i].pei.data, xi.p_env, mod->xxi[i].pei.npt * 4);

		for (j = 12; j < 108; j++) {
		    mod->xxi[i].map[j].ins = xi.sample[j - 12];
		    if (mod->xxi[i].map[j].ins >= mod->xxi[i].nsm)
			mod->xxi[i].map[j].ins = -1;
		}
	    }

	    for (j = 0; j < mod->xxi[i].nsm; j++, sample_num++) {
		xsh[j].length = read32l(f);	/* Sample length */
		xsh[j].loop_start = read32l(f);	/* Sample loop start */
		xsh[j].loop_length = read32l(f);/* Sample loop length */
		xsh[j].volume = read8(f);	/* Volume */
		xsh[j].finetune = read8s(f);	/* Finetune (-128..+127) */
		xsh[j].type = read8(f);		/* Flags */
		xsh[j].pan = read8(f);		/* Panning (0-255) */
		xsh[j].relnote = read8s(f);	/* Relative note number */
		xsh[j].reserved = read8(f);
		fread(&xsh[j].name, 22, 1, f);	/* Sample_name */

		mod->xxi[i].sub[j].vol = xsh[j].volume;
		mod->xxi[i].sub[j].pan = xsh[j].pan;
		mod->xxi[i].sub[j].xpo = xsh[j].relnote;
		mod->xxi[i].sub[j].fin = xsh[j].finetune;
		mod->xxi[i].sub[j].vwf = xi.y_wave;
		mod->xxi[i].sub[j].vde = xi.y_depth;
		mod->xxi[i].sub[j].vra = xi.y_rate;
		mod->xxi[i].sub[j].vsw = xi.y_sweep;
		mod->xxi[i].sub[j].sid = sample_num;
		if (sample_num >= MAX_SAMP)
		    continue;

		copy_adjust(mod->xxs[sample_num].name, xsh[j].name, 22);

		mod->xxs[sample_num].len = xsh[j].length;
		mod->xxs[sample_num].lps = xsh[j].loop_start;
		mod->xxs[sample_num].lpe = xsh[j].loop_start + xsh[j].loop_length;

		mod->xxs[sample_num].flg = 0;
		if (xsh[j].type & XM_SAMPLE_16BIT) {
		    mod->xxs[sample_num].flg |= XMP_SAMPLE_16BIT;
		    mod->xxs[sample_num].len >>= 1;
		    mod->xxs[sample_num].lps >>= 1;
		    mod->xxs[sample_num].lpe >>= 1;
		}

		mod->xxs[sample_num].flg |= xsh[j].type & XM_LOOP_FORWARD ?
		    XMP_SAMPLE_LOOP : 0;
		mod->xxs[sample_num].flg |= xsh[j].type & XM_LOOP_PINGPONG ?
		    XMP_SAMPLE_LOOP | XMP_SAMPLE_LOOP_BIDIR : 0;
	    }
	    for (j = 0; j < mod->xxi[i].nsm; j++) {
		if (sample_num >= MAX_SAMP)
		    continue;
		D_(D_INFO " %1x: %06x%c%06x %06x %c V%02x F%+04d P%02x R%+03d",
		    j, mod->xxs[mod->xxi[i].sub[j].sid].len,
		    mod->xxs[mod->xxi[i].sub[j].sid].flg & XMP_SAMPLE_16BIT ? '+' : ' ',
		    mod->xxs[mod->xxi[i].sub[j].sid].lps,
		    mod->xxs[mod->xxi[i].sub[j].sid].lpe,
		    mod->xxs[mod->xxi[i].sub[j].sid].flg & XMP_SAMPLE_LOOP_BIDIR ? 'B' :
		    mod->xxs[mod->xxi[i].sub[j].sid].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		    mod->xxi[i].sub[j].vol, mod->xxi[i].sub[j].fin,
		    mod->xxi[i].sub[j].pan, mod->xxi[i].sub[j].xpo);

		if (xfh.version > 0x0103) {
		    load_sample(f, SAMPLE_FLAG_DIFF,
				&mod->xxs[mod->xxi[i].sub[j].sid], NULL);
		}
	    }
	} else {
	    /* The sample size is a field of struct xm_instrument_header that
	     * should be in struct xm_instrument according to the (official)
	     * format description. xmp puts the field in the header because
	     * Fasttracker writes the modules this way. BUT there's some
	     * other tracker or conversor out there following the specs
	     * verbatim and thus creating modules incompatible with those
	     * created by Fasttracker. The following piece of code is a
	     * workaround for this problem (allowing xmp to play "Braintomb"
	     * by Jazztiz/ART). (seek -4)
	     */

	    /* Umm, Cyke O'Path <cyker@heatwave.co.uk> sent me a couple of
	     * mods ("Breath of the Wind" and "Broken Dimension") that
	     * reserve the instrument data space after the instrument header
	     * even if the number of instruments is set to 0. In these modules
	     * the instrument header size is marked as 263. The following
	     * generalization should take care of both cases.
	     */

	     fseek(f, (int)xih.size - 33 /*sizeof (xih)*/, SEEK_CUR);
	}
    }
    mod->smp = sample_num;
    mod->xxs = realloc(mod->xxs, sizeof (struct xmp_sample) * mod->smp);

    if (xfh.version <= 0x0103) {
	goto load_patterns;
    }

load_samples:
    D_(D_INFO "Stored samples: %d", mod->smp);

    /* XM 1.02 stores all samples after the patterns */

    if (xfh.version <= 0x0103) {
	for (i = 0; i < mod->ins; i++) {
	    for (j = 0; j < mod->xxi[i].nsm; j++) {
		load_sample(f, SAMPLE_FLAG_DIFF,
				&mod->xxs[mod->xxi[i].sub[j].sid], NULL);
	    }
	}
    }

    /* If dynamic pan is disabled, XM modules will use the standard
     * MOD channel panning (LRRL). Moved to module_play() --Hipolito.
     */

    for (i = 0; i < mod->chn; i++) {
        mod->xxc[i].pan = 0x80;
    }

    m->quirk |= QUIRKS_FT2;
    m->read_event_type = READ_EVENT_FT2;

    return 0;
}
