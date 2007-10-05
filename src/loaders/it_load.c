/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr.
 *
 * $Id: it_load.c,v 1.23 2007-10-05 00:18:44 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "it.h"
#include "period.h"

#define MAGIC_IMPM	MAGIC4('I','M','P','M')
#define MAGIC_IMPS	MAGIC4('I','M','P','S')

#define	FX_NONE	0xff
#define FX_XTND 0xfe
#define L_CHANNELS 64


static uint32 *pp_ins;		/* Pointers to instruments */
static uint32 *pp_smp;		/* Pointers to samples */
static uint32 *pp_pat;		/* Pointers to patterns */
static uint8 arpeggio_val[64];
static uint8 last_h[64], last_fxp[64];

static uint8 fx[] = {
	/*   */ FX_NONE,
	/* A */ FX_S3M_TEMPO,
	/* B */ FX_JUMP,
	/* C */ FX_BREAK,
	/* D */ FX_VOLSLIDE,
	/* E */ FX_PORTA_DN,
	/* F */ FX_PORTA_UP,
	/* G */ FX_TONEPORTA,
	/* H */ FX_VIBRATO,
	/* I */ FX_TREMOR,
	/* J */ FX_ARPEGGIO,
	/* K */ FX_VIBRA_VSLIDE,
	/* L */ FX_TONE_VSLIDE,
	/* M */ FX_TRK_VOL,
	/* N */ FX_TRK_VSLIDE,
	/* O */ FX_OFFSET,
	/* P */ FX_NONE,
	/* Q */ FX_MULTI_RETRIG,
	/* R */ FX_TREMOLO,
	/* S */ FX_XTND,
	/* T */ FX_S3M_BPM,
	/* U */ FX_NONE,
	/* V */ FX_GLOBALVOL,
	/* W */ FX_G_VOLSLIDE,
	/* X */ FX_NONE,
	/* Y */ FX_NONE,
	/* Z */ FX_FLT_CUTOFF
};

static char *nna[] = { "cut", "cont", "off", "fade" };
static char *dct[] = { "off", "note", "smp", "inst" };
static int dca2nna[] = { 0, 2, 3 };
static int new_fx;

int itsex_decompress8 (FILE *, void *, int, int);
int itsex_decompress16 (FILE *, void *, int, int);


static void xlat_fx (int c, struct xxm_event *e)
{
    uint8 h = MSN (e->fxp), l = LSN (e->fxp);

    switch (e->fxt = fx[e->fxt]) {
    case FX_ARPEGGIO:		/* Arpeggio */
	if (e->fxp)
	    arpeggio_val[c] = e->fxp;
	else
	    e->fxp = arpeggio_val[c];
	break;
    case FX_VIBRATO:		/* Old or new vibrato */
	if (new_fx)
	    e->fxt = FX_FINE2_VIBRA;
	break;
    case FX_XTND:		/* Extended effect */
	e->fxt = FX_EXTENDED;

	if (h == 0 && e->fxp == 0) {
	    h = last_h[c];
	    e->fxp = last_fxp[c];
	} else {
	    last_h[c] = h;
	    last_fxp[c] = e->fxp;
	}

	switch (h) {
	case 0x1:		/* Glissando */
	    e->fxp = 0x30 | l;
	    break;
	case 0x2:		/* Finetune */
	    e->fxp = 0x50 | l;
	    break;
	case 0x3:		/* Vibrato wave */
	    e->fxp = 0x40 | l;
	    break;
	case 0x4:		/* Tremolo wave */
	    e->fxp = 0x70 | l;
	    break;
	case 0x5:		/* Panbrello wave -- NOT IMPLEMENTED */
	    e->fxt = e->fxp = 0;
	    break;
	case 0x6:		/* Pattern delay */
	    e->fxp = 0xe0 | l;
	    break;
	case 0x7:		/* Instrument functions */
	    e->fxt = FX_IT_INSTFUNC;
	    e->fxp &= 0x0f;
	    break;
	case 0x8:		/* Set pan position */
	    e->fxt = FX_SETPAN;
	    e->fxp = l << 4;
	    break;
	case 0x9:		/* 0x91 = set surround -- NOT IMPLEMENTED */
	    e->fxt = e->fxp = 0;
	    break;
	case 0xb:		/* Pattern loop */
	    e->fxp = 0x60 | l;
	    break;
	case 0xc:		/* Note cut */
	case 0xd:		/* Note delay */
	case 0xe:		/* Pattern delay */
	    break;
	default:
	    e->fxt = e->fxp = 0;
	}
	break;
    case FX_FLT_CUTOFF:
	if (e->fxp > 0x7f && e->fxp < 0x90) {	/* Resonance */
	    e->fxt = FX_FLT_RESN;
	    e->fxp = (e->fxp - 0x80) * 16;
	} else {		/* Cutoff */
	    e->fxp *= 2;
	}
	break;
    case FX_NONE:		/* No effect */
	e->fxt = e->fxp = 0;
	break;
    }
}


static void xlat_volfx (struct xxm_event *event)
{
    int b;

    b = event->vol;
    event->vol = 0;

    if (b <= 0x40) {
	event->vol = b + 1;
    } else if (b >= 65 && b <= 74) {
	event->f2t = FX_EXTENDED;
	event->f2p = (EX_F_VSLIDE_UP << 4) | (b - 65);
    } else if (b >= 75 && b <= 84) {
	event->f2t = FX_EXTENDED;
	event->f2p = (EX_F_VSLIDE_DN << 4) | (b - 75);
    } else if (b >= 85 && b <= 94) {
	event->f2t = FX_VOLSLIDE_2;
	event->f2p = (b - 85) << 4;
    } else if (b >= 95 && b <= 104) {
	event->f2t = FX_VOLSLIDE_2;
	event->f2p = b - 95;
    } else if (b >= 105 && b <= 114) {
	event->f2t = FX_PORTA_DN;
	event->f2p = (b - 105) << 2;
    } else if (b >= 115 && b <= 124) {
	event->f2t = FX_PORTA_UP;
	event->f2p = (b - 115) << 2;
    } else if (b >= 128 && b <= 192) {
	if (b == 192)
	    b = 191;
	event->f2t = FX_SETPAN;
	event->f2p = (b - 128) << 2;
    } else if (b >= 193 && b <= 202) {
	event->f2t = FX_TONEPORTA;
	event->f2p = 1 << (b - 193);
#if 0
    } else if (b >= 193 && b <= 202) {
	event->f2t = FX_VIBRATO;
	event->f2p = ???;
#endif
    }
}


int it_load (FILE * f)
{
    int r, c, i, j, k, pat_len;
    struct xxm_event *event, dummy, lastevent[L_CHANNELS];
    struct it_file_header ifh;
    struct it_instrument1_header i1h;
    struct it_instrument2_header i2h;
    struct it_sample_header ish;
    struct it_envelope env;
    uint8 b, mask[L_CHANNELS];
    uint16 x16;
    int max_ch, flag;
    int inst_map[120], inst_rmap[96];
    char tracker_name[80];

    LOAD_INIT ();

    /* Load and convert header */
    if (read32b(f) != MAGIC_IMPM)
	return -1;

    fread(&ifh.name, 26, 1, f);
    fread(&ifh.rsvd1, 2, 1, f);

    ifh.ordnum = read16l(f);
    ifh.insnum = read16l(f);
    ifh.smpnum = read16l(f);
    ifh.patnum = read16l(f);

    ifh.cwt = read16l(f);
    ifh.cmwt = read16l(f);
    ifh.flags = read16l(f);
    ifh.special = read16l(f);

    ifh.gv = read8(f);
    ifh.mv = read8(f);
    ifh.is = read8(f);
    ifh.it = read8(f);
    ifh.sep = read8(f);
    ifh.zero = read8(f);

    ifh.msglen = read16l(f);
    ifh.msgofs = read32l(f);

    fread(&ifh.rsvd2, 4, 1, f);
    fread(&ifh.chpan, 64, 1, f);
    fread(&ifh.chvol, 64, 1, f);

    strcpy (xmp_ctl->name, (char *) ifh.name);
    xxh->len = ifh.ordnum;
    xxh->ins = ifh.insnum;
    xxh->smp = ifh.smpnum;
    xxh->pat = ifh.patnum;
    if (xxh->ins)		/* Sample mode has no instruments */
	pp_ins = calloc (4, xxh->ins);
    pp_smp = calloc (4, xxh->smp);
    pp_pat = calloc (4, xxh->pat);
    xxh->tpo = ifh.is;
    xxh->bpm = ifh.it;
    xxh->flg = ifh.flags & IT_LINEAR_FREQ ? XXM_FLG_LINEAR : 0;
    xxh->flg |= (ifh.flags & IT_USE_INST) && (ifh.cmwt >= 0x200) ?
					XXM_FLG_INSVOL : 0;
    for (i = 0; i < 64; i++) {
	if (ifh.chpan[i] == 100)
	    ifh.chpan[i] = 32;

	if (~ifh.chpan[i] & 0x80)
	    xxh->chn = i + 1;
	else
	    ifh.chvol[i] = 0;

	xxc[i].pan = ifh.flags & IT_STEREO ?
	    ((int) ifh.chpan[i] * 0x80 >> 5) & 0xff : 0x80;

	xxc[i].vol = ifh.chvol[i];
    }
    fread (xxo, 1, xxh->len, f);

    new_fx = ifh.flags & IT_OLD_FX ? 0 : 1;

    /* S3M skips pattern 0xfe */
    for (i = 0; i < (xxh->len - 1); i++) {
	if (xxo[i] == 0xfe) {
	    memcpy (&xxo[i], &xxo[i + 1], xxh->len - i - 1);
	    xxh->len--;
	}
    }
    for (i = 0; i < xxh->ins; i++)
	pp_ins[i] = read32l(f);
    for (i = 0; i < xxh->smp; i++)
	pp_smp[i] = read32l(f);
    for (i = 0; i < xxh->pat; i++)
	pp_pat[i] = read32l(f);

    xmp_ctl->c4rate = C4_NTSC_RATE;
    xmp_ctl->fetch |= XMP_CTL_FINEFX | XMP_CTL_ENVFADE;

    switch (ifh.cwt >> 8) {
    case 0x00:
	sprintf(tracker_name, "unmo3");
	break;
    case 0x01:
    case 0x02:
	if (ifh.cmwt == 0x0200 && ifh.cwt == 0x0217) {
	    sprintf(tracker_name, "ModPlug Tracker 1.16");
	    /* ModPlug Tracker files aren't really IMPM 2.00 */
	    ifh.cmwt = ifh.flags & IT_USE_INST ? 0x214 : 0x100;	
	} else if (ifh.cwt == 0x0216) {
	    sprintf(tracker_name, "Impulse Tracker 2.14v3");
	} else if (ifh.cwt == 0x0217) {
	    sprintf(tracker_name, "Impulse Tracker 2.14v5");
	} else {
	    sprintf(tracker_name, "Impulse Tracker %d.%02x",
			(ifh.cwt & 0x0f00) >> 8, ifh.cwt & 0xff);
	}
	break;
    case 0x08:
	if (ifh.cwt == 0x0888) {
	    sprintf(tracker_name, "ModPlug Tracker >= 1.17");
	} else {
	    sprintf(tracker_name, "unknown (%04x)", ifh.cwt);
	}
    case 0x10:
	sprintf(tracker_name, "Schism Tracker %d.%02x",
			(ifh.cwt & 0x0f00) >> 8, ifh.cwt & 0xff);
	break;
    default:
	sprintf(tracker_name, "unknown (%04x)", ifh.cwt);
    }

    sprintf (xmp_ctl->type, "IMPM %d.%02x (%s)",
			ifh.cmwt >> 8, ifh.cmwt & 0xff, tracker_name);

    MODULE_INFO ();

    if (V (0))
	report ("Instr/FX mode  : %s/%s",
	    ifh.flags & IT_USE_INST ? ifh.cmwt >= 0x200 ?
	    "new" : "old" : "sample",
	    ifh.flags & IT_OLD_FX ? "old" : "IT");

    if (~ifh.flags & IT_USE_INST)
	xxh->ins = xxh->smp;

    if (V (2) && ifh.special & IT_HAS_MSG) {
	report ("\nMessage length : %d\n| ", ifh.msglen);
	i = ftell (f);
	fseek (f, ifh.msgofs, SEEK_SET);
	for (j = 0; j < ifh.msglen; j++) {
	    fread (&b, 1, 1, f);
	    if (b == '\r')
		b = '\n';
	    if ((b < 32 || b > 127) && b != '\n' && b != '\t')
		b = '.';
	    report ("%c", b);
	    if (b == '\n')
		report ("| ");
	}
	fseek (f, i, SEEK_SET);
    }

    INSTRUMENT_INIT ();

    if (xxh->ins && V (0) && (ifh.flags & IT_USE_INST)) {
	report ("\nInstruments    : %d ", xxh->ins);
	if (V (1)) {
	    if (ifh.cmwt >= 0x200)
		report ("\n     Instrument name            NNA  DCT  DCA  "
		    "Fade  GbV Pan RV Env NSm FC FR");
	    else
		report ("\n     Instrument name            NNA  DNC  "
		    "Fade  VolEnv NSm");
	}
    }

    for (i = 0; i < xxh->ins; i++) {
	/*
	 * IT files can have three different instrument types: 'New'
	 * instruments, 'old' instruments or just samples. We need a
	 * different loader for each of them.
	 */

	if ((ifh.flags & IT_USE_INST) && (ifh.cmwt >= 0x200)) {
	    /* New instrument format */
	    fseek (f, pp_ins[i], SEEK_SET);

	    i2h.magic = read32b(f);
	    fread(&i2h.dosname, 12, 1, f);
	    i2h.zero = read8(f);
	    i2h.nna = read8(f);
	    i2h.dct = read8(f);
	    i2h.dca = read8(f);
	    i2h.fadeout = read16l(f);

	    i2h.pps = read8(f);
	    i2h.ppc = read8(f);
	    i2h.gbv = read8(f);
	    i2h.dfp = read8(f);
	    i2h.rv = read8(f);
	    i2h.rp = read8(f);
	    i2h.trkvers = read16l(f);

	    i2h.nos = read8(f);
	    i2h.rsvd1 = read8(f);
	    fread(&i2h.name, 26, 1, f);

	    i2h.ifc = read8(f);
	    i2h.ifr = read8(f);
	    i2h.mch = read8(f);
	    i2h.mpr = read8(f);
	    i2h.mbnk = read16l(f);
	    fread(&i2h.keys, 240, 1, f);

	    copy_adjust(xxih[i].name, i2h.name, 24);
	    xxih[i].rls = i2h.fadeout << 6;

	    /* Envelopes */

#define BUILD_ENV(X) { \
            env.flg = read8(f); \
            env.num = read8(f); \
            env.lpb = read8(f); \
            env.lpe = read8(f); \
            env.slb = read8(f); \
            env.sle = read8(f); \
            for (j = 0; j < 25; j++) { \
            	env.node[j].y = read8(f); \
            	env.node[j].x = read16l(f); \
            } \
            env.unused = read8(f); \
	    xxih[i].X##ei.flg = env.flg & IT_ENV_ON ? XXM_ENV_ON : 0; \
	    xxih[i].X##ei.flg |= env.flg & IT_ENV_LOOP ? XXM_ENV_LOOP : 0; \
	    xxih[i].X##ei.flg |= env.flg & IT_ENV_SLOOP ? XXM_ENV_SUS : 0; \
	    xxih[i].X##ei.npt = env.num; \
	    xxih[i].X##ei.sus = env.slb; \
	    xxih[i].X##ei.sue = env.sle; \
	    xxih[i].X##ei.lps = env.lpb; \
	    xxih[i].X##ei.lpe = env.lpe; \
	    if (env.num) xx##X##e[i] = calloc (4, env.num); \
	    for (j = 0; j < env.num; j++) { \
		xx##X##e[i][j * 2] = env.node[j].x; \
		xx##X##e[i][j * 2 + 1] = env.node[j].y; \
	    } \
}

	    BUILD_ENV (a);
	    BUILD_ENV (p);
	    BUILD_ENV (f);
	    
	    if (xxih[i].pei.flg & XXM_ENV_ON)
		for (j = 0; j < xxih[i].pei.npt; j++)
		    xxpe[i][j * 2 + 1] += 32;

	    if (env.flg & IT_ENV_FILTER) {
		xxih[i].fei.flg |= XXM_ENV_FLT;
		for (j = 0; j < env.num; j++) {
		    xxfe[i][j * 2 + 1] += 32;
		    xxfe[i][j * 2 + 1] *= 4;
		}
	    } else {
		/* Pitch envelope is *50 to get fine interpolation */
		for (j = 0; j < env.num; j++)
		    xxfe[i][j * 2 + 1] *= 50;
	    }

	    /* See how many different instruments we have */
	    for (j = 0; j < 96; j++)
		inst_map[j] = -1;

	    for (k = j = 0; j < 96; j++) {
		c = i2h.keys[25 + j * 2] - 1;
		if (c < 0) {
		    xxim[i].ins[j] = 0xff;	/* No sample */
		    xxim[i].xpo[j] = 0;
		    continue;
		}
		if (inst_map[c] == -1) {
		    inst_map[c] = k;
		    inst_rmap[k] = c;
		    k++;
		}
		xxim[i].ins[j] = inst_map[c];
		xxim[i].xpo[j] = i2h.keys[24 + j * 2] - (j + 12);
	    }

	    xxih[i].nsm = k;
	    xxih[i].vol = i2h.gbv >> 1;

	    if (k) {
		xxi[i] = calloc (sizeof (struct xxm_instrument), k);
		for (j = 0; j < k; j++) {
		    xxi[i][j].sid = inst_rmap[j];
		    xxi[i][j].nna = i2h.nna;
		    xxi[i][j].dct = i2h.dct;
		    xxi[i][j].dca = dca2nna[i2h.dca & 0x03];
		    xxi[i][j].pan = i2h.dfp & 0x80 ? 0x80 : i2h.dfp * 4;
		    xxi[i][j].ifc = i2h.ifc;
		    xxi[i][j].ifr = i2h.ifr;
	        }
	    }

	    if (V (1))
		report ("\n[%2X] %-26.26s %-4.4s %-4.4s %-4.4s %4d %4d  %2x "
			"%02x %c%c%c %3d %02x %02x ",
		i, i2h.name,
		i2h.nna < 4 ? nna[i2h.nna] : "none",
		i2h.dct < 4 ? dct[i2h.dct] : "none",
		i2h.dca < 3 ? nna[dca2nna[i2h.dca]] : "none",
		i2h.fadeout,
		i2h.gbv,
		i2h.dfp & 0x80 ? 0x80 : i2h.dfp * 4,
		i2h.rv,
		xxih[i].aei.flg & XXM_ENV_ON ? 'V' : '-',
		xxih[i].pei.flg & XXM_ENV_ON ? 'P' : '-',
		env.flg & 0x01 ? env.flg & 0x80 ? 'F' : 'P' : '-',
		xxih[i].nsm,
		i2h.ifc,
		i2h.ifr
	    );

	    if (V (0))
		report (".");

	} else if (ifh.flags & IT_USE_INST) {
/* Old instrument format */
	    fseek (f, pp_ins[i], SEEK_SET);

	    i1h.magic = read32b(f);
	    fread(&i1h.dosname, 12, 1, f);

	    i1h.zero = read8(f);
	    i1h.flags = read8(f);
	    i1h.vls = read8(f);
	    i1h.vle = read8(f);
	    i1h.sls = read8(f);
	    i1h.sle = read8(f);
	    i1h.rsvd1 = read16l(f);
	    i1h.fadeout = read16l(f);

	    i1h.nna = read8(f);
	    i1h.dnc = read8(f);
	    i1h.trkvers = read16l(f);
	    i1h.nos = read8(f);
	    i1h.rsvd2 = read8(f);

	    fread(&i1h.name, 26, 1, f);
	    fread(&i1h.rsvd3, 6, 1, f);
	    fread(&i1h.keys, 240, 1, f);
	    fread(&i1h.epoint, 200, 1, f);
	    fread(&i1h.enode, 50, 1, f);

	    copy_adjust(xxih[i].name, i1h.name, 24);

	    xxih[i].rls = i1h.fadeout << 7;

	    xxih[i].aei.flg = i1h.flags & IT_ENV_ON ? XXM_ENV_ON : 0;
	    xxih[i].aei.flg |= i1h.flags & IT_ENV_LOOP ? XXM_ENV_LOOP : 0;
	    xxih[i].aei.flg |= i1h.flags & IT_ENV_SLOOP ? XXM_ENV_SUS : 0;
	    xxih[i].aei.lps = i1h.vls;
	    xxih[i].aei.lpe = i1h.vle;
	    xxih[i].aei.sus = i1h.sls;
	    xxih[i].aei.sue = i1h.sle;

	    for (k = 0; i1h.enode[k * 2] != 0xff; k++);
	    xxae[i] = calloc (4, k);
	    for (xxih[i].aei.npt = k; k--; ) {
		xxae[i][k * 2] = i1h.enode[k * 2];
		xxae[i][k * 2 + 1] = i1h.enode[k * 2 + 1];
	    }
	    
	    /* See how many different instruments we have */
	    for (j = 0; j < 96; j++)
		inst_map[j] = -1;

	    for (k = j = 0; j < 96; j++) {
		c = i1h.keys[25 + j * 2] - 1;
		if (c < 0) {
		    xxim[i].ins[j] = 0xff;	/* No sample */
		    xxim[i].xpo[j] = 0;
		    continue;
		}
		if (inst_map[c] == -1) {
		    inst_map[c] = k;
		    inst_rmap[k] = c;
		    k++;
		}
		xxim[i].ins[j] = inst_map[c];
		xxim[i].xpo[j] = i1h.keys[24 + j * 2] - (j + 12);
	    }

	    xxih[i].nsm = k;
	    xxih[i].vol = i2h.gbv >> 1;

	    if (k) {
		xxi[i] = calloc (sizeof (struct xxm_instrument), k);
		for (j = 0; j < k; j++) {
		    xxi[i][j].sid = inst_rmap[j];
		    xxi[i][j].nna = i1h.nna;
		    xxi[i][j].dct = i1h.dnc ? XXM_DCT_NOTE : XXM_DCT_OFF;
		    xxi[i][j].dca = XXM_DCA_CUT;
		    xxi[i][j].pan = 0x80;
	        }
	    }

	    if (V (1))
		report ("\n[%2X] %-26.26s %-4.4s %-4.4s %4d  "
			"%2d %c%c%c %3d ",
		i, i1h.name,
		i1h.nna < 4 ? nna[i1h.nna] : "none",
		i1h.dnc ? "on" : "off",
		i1h.fadeout,
		xxih[i].aei.npt,
		xxih[i].aei.flg & XXM_ENV_ON ? 'V' : '-',
		xxih[i].aei.flg & XXM_ENV_LOOP ? 'L' : '-',
		xxih[i].aei.flg & XXM_ENV_SUS ? 'S' : '-',
		xxih[i].nsm
	    );

	    if (V (0))
		report (".");
	}
    }

    if (V (0))
	report ("\nStored Samples : %d ", xxh->smp);

    if (V (2) || (~ifh.flags & IT_USE_INST && V (1)))
	report (
"\n     Sample name                Len   LBeg  LEnd  SBeg  SEnd  FlCv VlGv C5Spd"
	);
    
    for (i = 0; i < xxh->smp; i++) {
	if (~ifh.flags & IT_USE_INST)
	    xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	fseek (f, pp_smp[i], SEEK_SET);

	ish.magic = read32b(f);
	fread(&ish.dosname, 12, 1, f);
	ish.zero = read8(f);
	ish.gvl = read8(f);
	ish.flags = read8(f);
	ish.vol = read8(f);
	fread(&ish.name, 26, 1, f);

	ish.convert = read8(f);
	ish.dfp = read8(f);
	ish.length = read32l(f);
	ish.loopbeg = read32l(f);
	ish.loopend = read32l(f);
	ish.c5spd = read32l(f);
	ish.sloopbeg = read32l(f);
	ish.sloopend = read32l(f);
	ish.sample_ptr = read32l(f);

	ish.vis = read8(f);
	ish.vid = read8(f);
	ish.vir = read8(f);
	ish.vit = read8(f);

	if (ish.magic != MAGIC_IMPS)
	    return -2;
	
	if (ish.flags & IT_SMP_16BIT) {
	    xxs[i].len = ish.length * 2;
	    xxs[i].lps = ish.loopbeg * 2;
	    xxs[i].lpe = ish.loopend * 2;
	    xxs[i].flg = WAVE_16_BITS;
	} else {
	    xxs[i].len = ish.length;
	    xxs[i].lps = ish.loopbeg;
	    xxs[i].lpe = ish.loopend;
	}

	xxs[i].flg |= ish.flags & IT_SMP_LOOP ? WAVE_LOOPING : 0;
	xxs[i].flg |= ish.flags & IT_SMP_BLOOP ? WAVE_BIDIR_LOOP : 0;

	if (~ifh.flags & IT_USE_INST) {
	    /* Create an instrument for each sample */
	    xxi[i][0].vol = ish.vol;
	    xxi[i][0].pan = 0x80;
	    xxi[i][0].sid = i;
	    xxih[i].nsm = !!(xxs[i].len);

	    copy_adjust(xxih[i].name, ish.name, 24);
	}

	if (V (2) || (~ifh.flags & IT_USE_INST && V (1))) {
	    if (strlen ((char *) ish.name) || xxs[i].len > 1) {
		report (
		    "\n[%2X] %-26.26s %05x%c%05x %05x %05x %05x "
		    "%02x%02x %02x%02x %5d ",
		    i, ish.name,
		    xxs[i].len,
		    ish.flags & IT_SMP_16BIT ? '+' : ' ',
		    xxs[i].lps, xxs[i].lpe,
		    ish.sloopbeg, ish.sloopend,
		    ish.flags, ish.convert,
		    ish.vol, ish.gvl, ish.c5spd
		);
	    }
	}

	/* Convert C5SPD to relnote/finetune
	 *
	 * In IT we can have a sample associated with two or more
	 * instruments, but c5spd is a sample attribute -- so we must
	 * scan all xmp instruments to set the correct transposition
	 */
	
	for (j = 0; j < xxh->ins; j++) {
	    for (k = 0; k < xxih[j].nsm; k++) {
		if (xxi[j][k].sid == i) {
		    xxi[j][k].vol = ish.vol;
		    xxi[j][k].gvl = ish.gvl;
		    c2spd_to_note(ish.c5spd, &xxi[j][k].xpo, &xxi[j][k].fin);
		}
	    }
	}

	if (ish.flags & IT_SMP_SAMPLE && xxs[i].len > 1) {
	    int cvt = 0;

	    fseek(f, ish.sample_ptr, SEEK_SET);

	    if (~ish.convert & IT_CVT_SIGNED)
		cvt |= XMP_SMP_UNS;

	    /* Handle compressed samples using Tammo Hinrichs' routine */
	    if (ish.flags & IT_SMP_COMP) {
		char *buf;
		buf = calloc(1, xxs[i].len);

		if (ish.flags & IT_SMP_16BIT) {
		    itsex_decompress16 (f, buf, xxs[i].len >> 1, 
					ish.convert & IT_CVT_DIFF);
		} else {
		    itsex_decompress8(f, buf, xxs[i].len, ifh.cmwt == 0x0215);
		}

		xmp_drv_loadpatch(NULL, i, xmp_ctl->c4rate,
				XMP_SMP_NOLOAD | cvt, &xxs[i], buf);
		free (buf);
		reportv(0, "c");
	    } else {
		xmp_drv_loadpatch(f, i, xmp_ctl->c4rate, cvt, &xxs[i], NULL);

		reportv(0, ".");
	    }
	}
    }

    if (V (0))
	report ("\nStored Patterns: %d ", xxh->pat);

    xxh->trk = xxh->pat * xxh->chn;
    memset(arpeggio_val, 0, 64);
    memset(last_h, 0, 64);
    memset(last_fxp, 0, 64);

    PATTERN_INIT ();

    /* Read patterns */
    for (max_ch = i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	r = 0;
	/* If the offset to a pattern is 0, the pattern is empty */
	if (!pp_pat[i]) {
	    xxp[i]->rows = 64;
	    xxt[i * xxh->chn] = calloc (sizeof (struct xxm_track) +
		sizeof (struct xxm_event) * 64, 1);
	    xxt[i * xxh->chn]->rows = 64;
	    for (j = 0; j < xxh->chn; j++)
		xxp[i]->info[j].index = i * xxh->chn;
	    continue;
	}
	fseek (f, pp_pat[i], SEEK_SET);
	pat_len = read16l(f) /* - 4*/;
	xxp[i]->rows = read16l(f);
	TRACK_ALLOC (i);
	memset (mask, 0, L_CHANNELS);
	fread (&x16, 2, 1, f);
	fread (&x16, 2, 1, f);

	while (--pat_len >= 0) {
	    fread (&b, 1, 1, f);
	    if (!b) {
		r++;
		continue;
	    }
	    c = (b - 1) & 63;

	    if (b & 0x80) {
		fread (&mask[c], 1, 1, f);
		pat_len--;
	    }
	    /*
	     * WARNING: we IGNORE events in disabled channels. Disabled
	     * channels should be muted only, but we don't know the
	     * real number of channels before loading the patterns and
	     * we don't want to set it to 64 channels.
	     */
	    event = c >= xxh->chn ? &dummy : &EVENT (i, c, r);
	    if (mask[c] & 0x01) {
		fread (&b, 1, 1, f);
		switch (b) {
		case 0xff:	/* key off */
		    b = 0x61;
		    break;
		case 0xfe:	/* cut */
		    b = 0x62;
		    break;
		case 0xfd:	/* fade */
		    b = 0x63;
		    break;
		default:
		    if (b < 11)
			b = 0;
		    else
			b -= 11;
		}
		if (b > 0x62)
		    b = 0;
		lastevent[c].note = event->note = b;
		pat_len--;
	    }
	    if (mask[c] & 0x02) {
		fread (&b, 1, 1, f);
		lastevent[c].ins = event->ins = b;
		pat_len--;
	    }
	    if (mask[c] & 0x04) {
		fread (&b, 1, 1, f);
		lastevent[c].vol = event->vol = b;
		xlat_volfx (event);
		pat_len--;
	    }
	    if (mask[c] & 0x08) {
		fread (&b, 1, 1, f);
		event->fxt = b;
		fread (&b, 1, 1, f);
		event->fxp = b;
		xlat_fx (c, event);
		lastevent[c].fxt = event->fxt;
		lastevent[c].fxp = event->fxp;
		pat_len -= 2;
	    }
	    if (mask[c] & 0x10) {
		event->note = lastevent[c].note;
	    }
	    if (mask[c] & 0x20) {
		event->ins = lastevent[c].ins;
	    }
	    if (mask[c] & 0x40) {
		event->vol = lastevent[c].vol;
		xlat_volfx (event);
	    }
	    if (mask[c] & 0x80) {
		event->fxt = lastevent[c].fxt;
		event->fxp = lastevent[c].fxp;
	    }
	}
	reportv(0, ".");

	/* Sweep channels, look for unused tracks */
	for (c = xxh->chn - 1; c >= max_ch; c--) {
	    for (flag = j = 0; j < xxt[xxp[i]->info[c].index]->rows; j++) {
		event = &EVENT (i, c, j);
		if (event->note || event->vol || event->ins || event->fxt ||
		    event->fxp || event->f2t || event->f2p) {
		    flag = 1;
		    break;
		}
	    }
	    if (flag && c > max_ch)
		max_ch = c;
	}
    }

    xxh->chn = max_ch + 1;
    xmp_ctl->fetch |= ifh.flags & IT_USE_INST ? XMP_MODE_IT : XMP_MODE_ST3;
    xmp_ctl->fetch |= XMP_CTL_VIRTUAL | (xmp_ctl->flags & XMP_CTL_FILTER);

    reportv(0, "\n");

    return XMP_OK;
}
