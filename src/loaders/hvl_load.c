/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _DEBUG

#include "load.h"

#define MAGIC_HVL	MAGIC4('H','V','L', 0)

static int hvl_test (FILE *, char *, const int);
static int hvl_load (struct xmp_context *, FILE *, const int);


struct xmp_loader_info hvl_loader = {
	"HVL",
	"Hively Tracker",
	hvl_test,
	hvl_load
};

static int hvl_test(FILE *f, char *t, const int start)
{
	if (read32b(f) != MAGIC_HVL)
		return -1;

	uint16 off = read16b(f);
	if (fseek(f, off + 1, SEEK_SET))
		return -1;

	read_title(f, t, 32);

	return 0;
}


static void hvl_GenSawtooth(int8 * buf, uint32 len)
{
	uint32 i;
	int32 val, add;

	add = 256 / (len - 1);
	val = -128;

	for (i = 0; i < len; i++, val += add)
		*buf++ = (int8) val;
}

static void hvl_GenTriangle(int8 * buf, uint32 len)
{
	uint32 i;
	int32 d2, d5, d1, d4;
	int32 val;
	int8 *buf2;

	d2 = len;
	d5 = len >> 2;
	d1 = 128 / d5;
	d4 = -(d2 >> 1);
	val = 0;

	for (i = 0; i < d5; i++) {
		*buf++ = val;
		val += d1;
	}
	*buf++ = 0x7f;

	if (d5 != 1) {
		val = 128;
		for (i = 0; i < d5 - 1; i++) {
			val -= d1;
			*buf++ = val;
		}
	}

	buf2 = buf + d4;
	for (i = 0; i < d5 * 2; i++) {
		int8 c;

		c = *buf2++;
		if (c == 0x7f)
			c = 0x80;
		else
			c = -c;

		*buf++ = c;
	}
}

static void hvl_GenSquare(int8 * buf)
{
	uint32 i, j;

	for (i = 1; i <= 0x20; i++) {
		for (j = 0; j < (0x40 - i) * 2; j++)
			*buf++ = 0x80;
		for (j = 0; j < i * 2; j++)
			*buf++ = 0x7f;
	}
}

static void hvl_GenWhiteNoise(int8 * buf, uint32 len)
{
	uint32 ays;

	ays = 0x41595321;

	do {
		uint16 ax, bx;
		int8 s;

		s = ays;

		if (ays & 0x100) {
			s = 0x80;

			if ((int32) (ays & 0xffff) >= 0)
				s = 0x7f;
		}

		*buf++ = s;
		len--;

		ays = (ays >> 5) | (ays << 27);
		ays = (ays & 0xffffff00) | ((ays & 0xff) ^ 0x9a);
		bx = ays;
		ays = (ays << 2) | (ays >> 30);
		ax = ays;
		bx += ax;
		ax ^= bx;
		ays = (ays & 0xffff0000) | ax;
		ays = (ays >> 3) | (ays << 29);
	} while (len);
}

static void fix_effect (uint8 *fx, uint8 *param) {
	switch (*fx) {
	case 0:
		if (*param)
			*fx = FX_AHX_FILTER;
		break;
	case 3: /* init square */
		*fx = FX_AHX_SQUARE;
		break;
	case 4: /* set filter */
		*fx = FX_AHX_MODULATE;
		break;
	case 6: /* unused */
	case 8: /* unused */
		*fx = *param = 0;
		break;
	case 7:
		*fx = FX_MASTER_PAN;
		*param ^= 0x80;
//		printf ("pan %02x\n", *param);
		break;
	case 9:
		_D(_D_INFO "square %02x", *param);
		break;
	case 12:
		if (*param >= 0x50 && *param <= 0x90) {
			*param -= 0x50;
			*fx = FX_GLOBALVOL;
		} else if (*param >= 0xa0 && *param <= 0xe0) {
			*param -= 0xa0;
			*fx = FX_TRK_VOL;
		}
		break;
	case 15:
		*fx = FX_S3M_TEMPO;
		break;
	}
}

static int hvl_load(struct xmp_context *ctx, FILE *f, const int start)
{
	struct xmp_player_context *p = &ctx->p;
	struct xmp_mod_context *m = &p->m;
	int i, j, tmp, blank;

	LOAD_INIT();

	read32b(f);

	uint16 title_offset = read16b(f);
	tmp = read16b(f);
	m->xxh->len = tmp & 0xfff;
	blank = tmp & 0x8000;
		
	tmp = read16b(f);
	m->xxh->chn = (tmp >> 10) + 4;
	m->xxh->rst = tmp & 1023;

	int pattlen = read8(f);
	m->xxh->trk = read8(f) + 1;
	m->xxh->ins = read8(f);
	int subsongs = read8(f);
	int gain = read8(f);
	int stereo = read8(f);

	_D(_D_WARN "pattlen=%d npatts=%d nins=%d seqlen=%d stereo=%02x",
		pattlen, m->xxh->trk, m->xxh->ins, m->xxh->len, stereo);

	set_type(m, "HVL (Hively Tracker)");
	MODULE_INFO();

	m->xxh->pat = m->xxh->len;
	m->xxh->smp = 20;
	PATTERN_INIT();
	INSTRUMENT_INIT();

	fseek (f, subsongs*2, SEEK_CUR);

	uint8 *seqbuf = malloc(m->xxh->len * m->xxh->chn * 2);
	uint8 *seqptr = seqbuf;
	fread (seqbuf, 1, m->xxh->len * m->xxh->chn * 2, f);

	uint8 **transbuf = malloc (m->xxh->len * m->xxh->chn * sizeof(uint8 *));
	int transposed = 0;

	reportv(ctx, 0, "Stored patterns: %d ", m->xxh->len);

	for (i = 0; i < m->xxh->len; i++) {
		PATTERN_ALLOC(i);
		m->xxp[i]->rows = pattlen;
		for (j = 0; j < m->xxh->chn; j++) {
			if (seqptr[1]) {
//				printf ("%d: transpose %02x by %d\n", i, seqptr[0], seqptr[1]);
				m->xxp[i]->info[j].index = m->xxh->trk + transposed;
				transbuf[transposed] = seqptr;
				transposed++;
			} else {
				m->xxp[i]->info[j].index = seqptr[0];
			}
			seqptr += 2;
//			printf ("%02x ", m->xxp[i]->info[j].index);
		}
//		printf ("\n");
		m->xxo[i] = i;
		reportv(ctx, 0, ".");
	}
	reportv(ctx, 0, "\n");
	

	/*
	 * tracks
	 */

	if (transposed) {
		m->xxh->trk += transposed;
		m->xxt = realloc(m->xxt, m->xxh->trk * sizeof (struct xxm_track *));
	}
	
	reportv(ctx, 0, "Stored tracks  : %d ", m->xxh->trk);

	for (i = 0; i < m->xxh->trk; i++) {
		m->xxt[i] = calloc(sizeof(struct xxm_track) +
				   sizeof(struct xxm_event) * pattlen - 1, 1);
                m->xxt[i]->rows = pattlen;

		if (!i && blank)
			continue;

		if (i >= m->xxh->trk-transposed) {
			int n=i-(m->xxh->trk - transposed);
			int o=transbuf[n][1];
			if (o>127)
				o-=256;
//			printf ("pattern %02x: source %02x offset %d\n", i, n, o);
			memcpy (m->xxt[i], m->xxt[transbuf[n][0]],
				sizeof(struct xxm_track) +
				sizeof(struct xxm_event) * pattlen - 1);
			for (j = 0; j < m->xxt[i]->rows; j++) {
				struct xxm_event *event = &m->xxt[i]->event[j];
				if (event->note)
					event->note+=o;
			}
			continue;
		}

		for (j = 0; j < m->xxt[i]->rows; j++) {
			struct xxm_event *event = &m->xxt[i]->event[j];
			int note = read8(f);			

			if (note != 0x3f) {
				uint32 b = read32b(f);
				event->note = note?note+24:0;
				event->ins = b >> 24;
				event->fxt = (b & 0xf00000) >> 20;
				event->f2t = (b & 0x0f0000) >> 16;
				event->fxp = (b & 0xff00) >> 8;
				event->f2p = b & 0xff;
				fix_effect (&event->fxt, &event->fxp);
				fix_effect (&event->f2t, &event->f2p);
				//	printf ("#");
			} //else 
			//	printf (".");
		}
		if (V(0) && !(i % m->xxh->chn))
			report (".");
	}
	reportv(ctx, 0, "\n");

	free(seqbuf);
	free(transbuf);

	/*
	 * Instruments
	 */

	for (i = 0; i < m->xxh->ins; i++) {
		uint8 buf[22];
		int vol, fspd, wavelen, flow, vibdel, hclen, hc;
		int vibdep, vibspd, sqmin, sqmax, sqspd, fmax, plen, pspd;
		int Alen, Avol, Dlen, Dvol, Slen, Rlen, Rvol;
                m->xxih[i].sub = calloc(sizeof (struct xxm_subinstrument), 1);

		fread(buf, 22, 1, f);

		vol = buf[0];		/* Master volume (0 to 64) */
		fspd = ((buf[1] >> 3) & 0x1f) | ((buf[12] >> 2) & 0x20);
					/* Filter speed */
		wavelen = buf[1] & 7;	/* Wave length */

		Alen = buf[2];		/* attack length, 1 to 255 */
		Avol = buf[3];		/* attack volume, 0 to 64 */
		Dlen = buf[4];		/* decay length, 1 to 255 */
		Dvol = buf[5];		/* decay volume, 0 to 64 */
		Slen = buf[6];		/* sustain length, 1 to 255 */
		Rlen = buf[7];		/* release length, 1 to 255 */
		Rvol = buf[8];		/* release volume, 0 to 64 */

		flow = buf[12] & 0x7f;	/* filter modulation lower limit */
		vibdel = buf[13];	/* vibrato delay */
		hclen = (buf[14] >> 4) & 0x07;
					/* Hardcut length */
		hc = buf[14] & 0x80 ? 1 : 0;
					/* Hardcut release */
		vibdep = buf[14] & 15;	/* vibrato depth, 0 to 15 */
		vibspd = buf[15];	/* vibrato speed, 0 to 63 */
		sqmin = buf[16];	/* square modulation lower limit */
		sqmax = buf[17];	/* square modulation upper limit */
		sqspd = buf[18];	/* square modulation speed */
		fmax = buf[19] & 0x3f;	/* filter modulation upper limit */
		pspd = buf[20];		/* playlist default speed */	
		plen = buf[21];		/* playlist length */

		if (!pspd)
			pspd=1;
		int poff = 0;

		_D(_D_WARN "I: %02x plen %02x pspd %02x vibdep=%d vibspd=%d sqmin=%d sqmax=%d", i, plen, pspd, vibdep, vibspd, sqmin, sqmax);
		int j;
		int wave=0;

		m->xxih[i].fei.flg = XXM_ENV_ON; /* | XXM_ENV_LOOP;*/
		m->xxih[i].fei.npt = plen*2;
		m->xxfe[i] = calloc (4, m->xxih[i].fei.npt);

		int note=0;
		int jump = -1;
		int dosq = -1;

		for (j = 0; j < plen; j++) {
			uint8 tmp[5];
			fread (tmp, 1, 5, f);

			int fx1 = tmp[0] & 15;
			int fx2 = (tmp[1] >> 3) & 15;

			/* non-zero waveform means change to wf[waveform-1] */
			/* 0: triangle
			   1: sawtooth
			   2: square
			   3: white noise
			*/

			int fixed = (tmp[2] >> 6) & 0x01;

			if (tmp[2] & 0x3f) {
				note = tmp[2] & 0x3f;
				if (fixed)
					note-=60;
			}

			if (fx1 == 15)
				pspd = tmp[3];
			else if (fx2 == 15)
				pspd = tmp[4];

			m->xxfe[i][j*4] = poff;
			m->xxfe[i][j*4+1] = note * 100;
			poff += pspd;
			m->xxfe[i][j*4+2] = poff;
			m->xxfe[i][j*4+3] = note * 100;

			if (jump >= 0) 
				continue;

			if ((fx1 == 4 && tmp[3] & 0xf) ||
			    (fx2 == 4 && tmp[4] & 0xf))
				dosq = sqmax;

			if (fx1 == 3)
				dosq = tmp[3] & 0x3f;
			else if (fx2 == 3)
				dosq = tmp[4] & 0x3f;

			if ((tmp[1] & 7))
				wave = tmp[1] & 7;
			
			if (fx1 == 5)
				jump = tmp[3];
			else if (fx2 == 5)
				jump = tmp[4];
			
			if (jump >= 0) {
				printf ("jump %d-%d\n", jump,j);
				m->xxih[i].fei.flg |= XXM_ENV_LOOP;
				m->xxih[i].fei.lps = jump*2;
				m->xxih[i].fei.lpe = j*2+1;
			}

			_D(_D_INFO "[%d W:%x 1:%x%02x 2:%x%02x n:%02x]", j, tmp[1] &7, tmp[0]&15, tmp[3], (tmp[1]>>3)&15, tmp[4], tmp[2]);
		}

		if (!wave)
			wave = 2;
		else if (dosq >= 0)
			wave = ((dosq>>2)&15)+4;
		else
			wave--;

		_D(_D_INFO "I: %02x V: %02x A: %02x %02x D: %02x %02x S:  %02x R: %02x %02x wave %02x",
			i, vol, Alen, Avol, Dlen, Dvol, Slen, Rlen, Rvol, wave);
		m->xxih[i].aei.flg = XXM_ENV_ON;
		m->xxih[i].aei.npt = 5;
		m->xxae[i] = calloc (4, m->xxih[i].aei.npt);
		m->xxae[i][0] = 0;
		m->xxae[i][1] = vol;
		m->xxae[i][2] = Alen; /* these are *not* multiplied by pspd */
		m->xxae[i][3] = Avol;
		m->xxae[i][4] = (Alen+Dlen);
		m->xxae[i][5] = Dvol;
		m->xxae[i][6] = (Alen+Dlen+Slen);
		m->xxae[i][7] = Dvol;
		m->xxae[i][8] = (Alen+Dlen+Slen+Rlen);
		m->xxae[i][9] = Rvol;

		m->xxih[i].sub[0].vol = 64;
		m->xxih[i].sub[0].sid = wave;
		m->xxih[i].sub[0].pan = 128;
		m->xxih[i].sub[0].xpo = (3-wavelen) * 12 - 1;
		/*m->xxih[i].sub[0].vde = vibdep;
		  m->xxih[i].sub[0].vra = vibspd; */
		m->xxih[i].nsm = 1;
        }

#define LEN 64
	int8 b[16384];

	for (i=0; i<20; i++) {
		m->xxs[i].len = LEN;
		m->xxs[i].lps = 0;
		m->xxs[i].lpe = LEN;
		m->xxs[i].flg |= XMP_SAMPLE_LOOP;

		switch (i) {
		case 0:
			hvl_GenTriangle (b, LEN);
			break;
		case 1:
			hvl_GenSawtooth (b, LEN);
			break;
		case 2:
			memset (b, 0x80, LEN/2);
			memset (b+LEN/2, 0x7f, LEN/2);
			break;
		case 3:
			m->xxs[i].len = m->xxs[i].lpe = 16384;
			hvl_GenWhiteNoise (b, 16384);
			break;
		default:
			memset (b, 0x7f, LEN);
			memset (b, 0x80, LEN*(20-i)/32);
			break;
		}

		xmp_drv_loadpatch(ctx, NULL, i,
				  XMP_SMP_NOLOAD, &m->xxs[i], (char *)b);
	}


	{
		int len, i;
		uint8 *namebuf, *nameptr;

		fseek (f, 0, SEEK_END);
		len = ftell(f) - title_offset;
		fseek (f, title_offset, SEEK_SET);

		nameptr = namebuf = malloc (len+1);
		fread (namebuf, 1, len, f);
		namebuf[len]=0;

		copy_adjust ((uint8 *)m->name, namebuf, 32);
		m->name[31]=0;
//		printf ("len=%d, name=%s\n", len, m->name);
		
		for (i=0; nameptr < namebuf+len && i < m->xxh->ins; i++) {
			nameptr += strlen((char *)nameptr)+1;
			copy_adjust(m->xxih[i].name, nameptr, 32);

			printf ("%02x: %s\n", i, nameptr);
		}

		free (namebuf);
	}

	for (i = 0; i < m->xxh->chn; i++)
                m->xxc[i].pan = ((i&3)%3) ? 128+stereo*31 : 128-stereo*31;


/*	m->quirk |= XMP_CTL_VBLANK;*/
/*	m->quirk |= XMP_QRK_UNISLD;*/
	return 0;
}

