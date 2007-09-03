/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr.
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See docs/COPYING
 * for more information.
 *
 * $Id: driver.c,v 1.21 2007-09-03 17:05:24 cmatsuoka Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include "driver.h"
#include "convert.h"
#include "mixer.h"
#include "period.h"
#include "readlzw.h"

#define	FREE	-1

extern int xmp_bpm;

struct xmp_control *xmp_ctl;

static int *ch2vo_count = NULL;
static int *ch2vo_array = NULL;

struct patch_info **patch_array = NULL;
static struct voice_info *voice_array = NULL;
static struct xmp_drv_info *drv_array = NULL;
static struct xmp_drv_info *driver = NULL;

static int *cmute_array = NULL;

static int numusr;		/* number of user channels */
static int numvoc;		/* number of soundcard voices */
static int numchn;		/* number of channels need to play one module */
static int numtrk;		/* number of tracks in module */
static int nummte;		/* number of channels in cmute_array */
static int maxvoc;		/* */
static int agevoc;		/* */
static int extern_drv;		/* set output to driver other than mixer */

static int drv_select (struct xmp_control *);
static inline void drv_resetvoice (int, int);

void (*_driver_callback)(void *, int) = NULL;

#ifndef XMMS_PLUGIN

extern struct xmp_drv_info drv_file;
extern struct xmp_drv_info drv_wav;

#ifdef DRIVER_OSX
extern struct xmp_drv_info drv_osx;
#endif
#ifdef DRIVER_SOLARIS
extern struct xmp_drv_info drv_solaris;
#endif
#ifdef DRIVER_HPUX
extern struct xmp_drv_info drv_hpux;
#endif
#ifdef DRIVER_BSD
extern struct xmp_drv_info drv_bsd;
#endif
#ifdef DRIVER_NETBSD
extern struct xmp_drv_info drv_netbsd;
#endif
#ifdef DRIVER_OPENBSD
extern struct xmp_drv_info drv_openbsd;
#endif
#ifdef DRIVER_SGI
extern struct xmp_drv_info drv_sgi;
#endif
#ifdef DRIVER_AIX
extern struct xmp_drv_info drv_aix;
#endif
#ifdef DRIVER_OSS_SEQ
extern struct xmp_drv_info drv_oss_seq;
#endif
#ifdef DRIVER_OSS_MIX
extern struct xmp_drv_info drv_oss_mix;
#endif
#ifdef DRIVER_ALSA_MIX
extern struct xmp_drv_info drv_alsa_mix;
#endif
#ifdef DRIVER_NET
extern struct xmp_drv_info drv_net;
#endif
#ifdef DRIVER_ESD
extern struct xmp_drv_info drv_esd;
#endif
#ifdef DRIVER_ARTS
extern struct xmp_drv_info drv_arts;
#endif
#ifdef DRIVER_NAS
extern struct xmp_drv_info drv_nas;
#endif
#ifdef DRIVER_OS2DART
extern struct xmp_drv_info drv_os2dart;
#endif
#ifdef DRIVER_QNX
extern struct xmp_drv_info drv_qnx;
#endif

#endif /* XMMS_PLUGIN */

void (*xmp_event_callback) (unsigned long);

#include "../player/mixer.c"


void xmp_init_drivers ()
{
#ifndef XMMS_PLUGIN

    /* Output to file will be always available -- except in the plugin ;) */
    xmp_drv_register (&drv_file);
    xmp_drv_register (&drv_wav);

#ifdef DRIVER_OSX
    xmp_drv_register (&drv_osx);
#endif
#ifdef DRIVER_WIN32
    xmp_drv_register (&drv_win32);
#endif
#ifdef DRIVER_SOLARIS
    xmp_drv_register (&drv_solaris);
#endif
#ifdef DRIVER_HPUX
    xmp_drv_register (&drv_hpux);
#endif
#ifdef DRIVER_BSD
    xmp_drv_register (&drv_bsd);
#endif
#ifdef DRIVER_NETBSD
    xmp_drv_register (&drv_netbsd);
#endif
#ifdef DRIVER_OPENBSD
    xmp_drv_register (&drv_openbsd);
#endif
#ifdef DRIVER_SGI
    xmp_drv_register (&drv_sgi);
#endif
#ifdef DRIVER_OSS_SEQ
    xmp_drv_register (&drv_oss_seq);
#endif
#ifdef DRIVER_OSS_MIX
    xmp_drv_register (&drv_oss_mix);
#endif
#ifdef DRIVER_ALSA_MIX
    xmp_drv_register (&drv_alsa_mix);
#endif
#ifdef DRIVER_NET
    xmp_drv_register (&drv_net);
#endif
#ifdef DRIVER_OS2DART
    xmp_drv_register (&drv_os2dart);
#endif
#ifdef DRIVER_ARTS
    xmp_drv_register (&drv_arts);
#endif
#ifdef DRIVER_ESD
    xmp_drv_register (&drv_esd);
#endif
#ifdef DRIVER_NAS
    xmp_drv_register (&drv_nas);
#endif

#endif /* XMMS_PLUGIN */
}


static int drv_select (struct xmp_control *ctl)
{
    int tmp;
    struct xmp_drv_info *drv;

    if (!drv_array)
	return XMP_ERR_DNREG;

    if (ctl->drv_id) {
	tmp = XMP_ERR_NODRV;
	for (drv = drv_array; drv; drv = drv->next)
	    if (!strcmp (drv->id, ctl->drv_id))
		if ((tmp = drv->init (ctl)) == XMP_OK)
		    break;
    } else {
	tmp = XMP_ERR_DSPEC;
	drv = drv_array->next;	/* skip file */
	drv = drv->next;	/* skip wav */
	for (; drv; drv = drv->next) {
	    if (ctl->verbose > 2)
		report ("Probing %s... ", drv->description);
	    if (drv->init (ctl) == XMP_OK) {
		if (ctl->verbose > 2)
		    report ("found\n");
		tmp = XMP_OK;
		break;
	    }
	    if (ctl->verbose > 2)
		report ("not found\n");
	}
    }
    if (tmp != XMP_OK)
	return tmp;

    ctl->drv_id = drv->id;
    ctl->description = drv->description;
    ctl->help = drv->help;
    driver = drv;

    return XMP_OK;
}


static void drv_resetvoice (int voc, int mute)
{
    if ((uint32) voc >= numvoc)
	return;

    if (mute)
	driver->setvol (voc, 0);

    xmp_ctl->numvoc--;
    ch2vo_count[voice_array[voc].root]--;
    ch2vo_array[voice_array[voc].chn] = FREE;
    memset (&voice_array[voc], 0, sizeof (struct voice_info));
    voice_array[voc].chn = voice_array[voc].root = FREE;
}


void xmp_drv_register (struct xmp_drv_info *drv)
{
    if (!drv_array)
	drv_array = drv;
    else {
	struct xmp_drv_info *tmp;

	for (tmp = drv_array; tmp->next; tmp = tmp->next);
	tmp->next = drv;
    }
    drv->next = NULL;
}


int xmp_drv_mutelloc (int num)
{
    if ((cmute_array = calloc (num, sizeof (int))) == NULL)
	  return XMP_ERR_ALLOC;
    nummte = num;

    return XMP_OK;
}


int xmp_drv_open (struct xmp_control *ctl)
{
    int status;

    if (!ctl)
	return XMP_ERR_NCTRL;

    if(ctl->flags & XMP_CTL_BIGEND)
        ctl->outfmt |= XMP_FMT_BIGEND;

    xmp_ctl = ctl;
    ctl->memavl = 0;
    smix_buf32b = NULL;
    extern_drv = TURN_ON;
    if ((status = drv_select (ctl)) != XMP_OK)
	return status;

    if ((patch_array = calloc (XMP_DEF_MAXPAT,
		sizeof (struct patch_info *))) == NULL) {
	driver->shutdown ();
	return XMP_ERR_ALLOC;
    }

    synth_init (ctl->freq);
    synth_reset ();

    return XMP_OK;
}


/* Kludgy funcion for the XMMS plugin */
int xmp_drv_set (struct xmp_control *ctl)
{
    struct xmp_drv_info *drv;

    if (!ctl)
	return XMP_ERR_NCTRL;

    if (!drv_array)
	return XMP_ERR_DNREG;

    patch_array = NULL;
    xmp_ctl = ctl;
    for (drv = drv_array; drv; drv = drv->next)
	if (!strcmp (drv->id, ctl->drv_id)) {
	    driver = drv;
	    return XMP_OK;
	}

    return XMP_ERR_DNREG;
}


void xmp_drv_close ()
{
    xmp_drv_off ();
    free (cmute_array);
    free (patch_array);
    driver->shutdown ();
    cmute_array = NULL;
    xmp_ctl = NULL;
    nummte = 0;

    synth_deinit ();
}


/* xmp_drv_on (number of tracks) */
int xmp_drv_on (int num)
{
    if (!xmp_ctl)
	return XMP_ERR_DOPEN;

    numusr = xmp_ctl->numusr;
    numtrk = xmp_ctl->numtrk = num;
    num = driver->numvoices (driver->numvoices (135711));
    driver->reset ();

    numchn = numtrk += numusr;
    maxvoc = xmp_ctl->fetch & XMP_CTL_VIRTUAL ? xmp_ctl->maxvoc : 1;
    if (maxvoc > 1)
	numchn += num;
    else if (num > numchn)
	num = numchn;

    num = numvoc = driver->numvoices (num);

    voice_array = calloc (numvoc, sizeof (struct voice_info));
    ch2vo_array = calloc (numchn, sizeof (int));
    ch2vo_count = calloc (numchn, sizeof (int));
    if (!(voice_array && ch2vo_array && ch2vo_count))
	return XMP_ERR_ALLOC;

    for (; num--; voice_array[num].chn = voice_array[num].root = FREE);
    for (num = numchn; num--; ch2vo_array[num] = FREE);
    xmp_ctl->numvoc = agevoc = 0;

    xmp_ctl->numchn = numchn;

    smix_mode = xmp_ctl->outfmt & XMP_FMT_MONO ? 1 : 2;
    smix_resol = xmp_ctl->resol > 8 ? 2 : 1;
    smix_resetvar ();

    return XMP_OK;
}


void xmp_drv_off ()
{
    if (numchn < 1)
	return;

    xmp_drv_writepatch (NULL);
    xmp_ctl->numvoc = numvoc = 0;
    xmp_ctl->numchn = numchn = 0;
    numtrk = 0;
    free (ch2vo_count);
    free (ch2vo_array);
    free (voice_array);
}


/*inline*/ void xmp_drv_clearmem ()
{
    driver->clearmem ();
}


void xmp_drv_reset ()
{
    int num;

    if (numchn < 1)
	return;

    driver->numvoices (driver->numvoices (43210));
    driver->reset ();
    driver->numvoices (num = numvoc);

    memset (ch2vo_count, 0, numchn * sizeof (int));
    memset (voice_array, 0, numvoc * sizeof (struct voice_info));
    for (; num--; voice_array[num].chn = voice_array[num].root = FREE);
    for (num = numchn; num--; ch2vo_array[num] = FREE);
    xmp_ctl->numvoc = agevoc = 0;
}


void xmp_drv_resetchannel (int chn)
{
    int voc;

    chn += numusr;
    if ((uint32) chn >= numchn || (uint32) (voc = ch2vo_array[chn]) >= numvoc)
	return;

    driver->setvol (voc, 0);

    xmp_ctl->numvoc--;
    ch2vo_count[voice_array[voc].root]--;
    ch2vo_array[chn] = FREE;
    memset (&voice_array[voc], 0, sizeof (struct voice_info));
    voice_array[voc].chn = voice_array[voc].root = FREE;
}


static int drv_allocvoice (int chn)
{
    int voc, vfree;
    uint32 age;

    if (ch2vo_count[chn] < maxvoc) {
	for (voc = numvoc; voc-- && voice_array[voc].chn != FREE;);
	if (voc < 0)
	    return voc;

	voice_array[voc].age = agevoc;
	ch2vo_count[chn]++;
	xmp_ctl->numvoc++;

	return voc;
    }
    for (voc = numvoc, vfree = age = FREE; voc--;)
	if (voice_array[voc].root == chn && voice_array[voc].age < age)
	    age = voice_array[vfree = voc].age;

    ch2vo_array[voice_array[vfree].chn] = FREE;
    voice_array[vfree].age = agevoc;

    return vfree;
}


void xmp_drv_mute (int chn, int status)
{
    chn += numusr;
    if ((uint32) chn >= nummte)
	return;

    if (status < 0)
	cmute_array[chn] = !cmute_array[chn];
    else
	cmute_array[chn] = status;
}


void xmp_drv_setvol (int chn, int vol)
{
    int voc;

    chn += numusr;
    if ((uint32) chn >= numchn || (uint32) (voc = ch2vo_array[chn]) >= numvoc)
	return;

    if (nummte > voice_array[voc].root && cmute_array[voice_array[voc].root])
	vol = TURN_OFF;

    driver->setvol (voc, vol);

    if (!(vol || chn < numtrk))
	drv_resetvoice (voc, TURN_ON);
}


void xmp_drv_setpan (int chn, int pan)
{
    int voc;

    chn += numusr;
    if ((uint32) chn >= numchn || (uint32) (voc = ch2vo_array[chn]) >= numvoc)
	return;

    driver->setpan (voc, pan);
}


void xmp_drv_seteffect (int chn, int type, int val)
{
    int voc;

    chn += numusr;
    if ((uint32) chn >= numchn || (uint32) (voc = ch2vo_array[chn]) >= numvoc)
	return;

    driver->seteffect (voc, type, val);
}


void xmp_drv_setsmp (int chn, int smp)
{
    int voc, pos, itp;
    struct voice_info *vi;

    chn += numusr;
    if ((uint32) chn >= numchn || (uint32) (voc = ch2vo_array[chn]) >= numvoc)
	return;

    vi = &voice_array[voc];
    if ((uint32) smp >= XMP_DEF_MAXPAT || !patch_array[smp] || vi->smp == smp)
	return;

    pos = vi->pos;
    itp = vi->itpt;

    smix_setpatch (voc, smp, TURN_ON);
    smix_voicepos (voc, pos, itp);
    if (extern_drv) {
	driver->setpatch (voc, smp);
	driver->setnote (voc, vi->note);
	driver->voicepos (voc, pos << !!(patch_array[smp]->mode& WAVE_16_BITS));
    }
}


int xmp_drv_setpatch (int chn, int ins, int smp, int note, int nna,
	int dct, int dca, int flg) {
    int voc, vfree;

    chn += numusr;
    if ((uint32) chn >= numchn)
	return -numusr - 1;
    if (ins < 0 || (uint32) smp >= XMP_DEF_MAXPAT || !patch_array[smp])
	smp = -1;

    if (dct) {
	for (voc = numvoc; voc--;)
	    if (voice_array[voc].root == chn && voice_array[voc].ins == ins)
		if ((dct == XXM_DCT_INST) ||
		    (dct == XXM_DCT_SMP && voice_array[voc].smp == smp) ||
		    (dct == XXM_DCT_NOTE && voice_array[voc].note == note)) {
		    if (dca) {
			if (voc != ch2vo_array[chn] || voice_array[voc].act)
			    voice_array[voc].act = dca;
		    } else
			drv_resetvoice (voc, TURN_ON);
		}
    }
    voc = ch2vo_array[chn];
    if (voc > FREE) {
	if (voice_array[voc].act && maxvoc > 1) {
	    if ((vfree = drv_allocvoice (chn)) > FREE) {
		voice_array[vfree].root = chn;
		voice_array[ch2vo_array[chn] = vfree].chn = chn;
		for (chn = numtrk; ch2vo_array[chn++] > FREE;);
		voice_array[voc].chn = --chn;
		ch2vo_array[chn] = voc;
		voc = vfree;
	    } else if (flg)
		return -numusr - 1;
	}
    } else {
	if ((voc = drv_allocvoice (chn)) < 0)
	    return -numusr - 1;
	voice_array[ch2vo_array[chn] = voc].chn = chn;
	voice_array[voc].root = chn;
    }

    if (smp < 0) {
	drv_resetvoice (voc, TURN_ON);
	return -numusr - 1;
    }
    smix_setpatch (voc, smp, TURN_ON);
    smix_setnote (voc, note);
    voice_array[voc].ins = ins;
    voice_array[voc].act = nna;
    if (extern_drv) {
	driver->setpatch (voc, smp);
	driver->setnote (voc, note);
    }
    agevoc++;
    return chn - numusr;
}


void xmp_drv_setnna (int chn, int nna)
{
    int voc;

    chn += numusr;
    if ((uint32) chn >= numchn || (uint32) (voc = ch2vo_array[chn]) >= numvoc)
	return;

    voice_array[voc].act = nna;
}


void xmp_drv_setbend (int chn, int bend)
{
    int voc;

    chn += numusr;
    if ((uint32) chn >= numchn || (uint32) (voc = ch2vo_array[chn]) >= numvoc)
	return;

    smix_setbend (voc, bend);
    if (extern_drv)
	driver->setbend (voc, bend);
}


void xmp_drv_retrig (int chn)
{
    int voc;

    chn += numusr;
    if ((uint32) chn >= numchn || (uint32) (voc = ch2vo_array[chn]) >= numvoc)
	return;

    smix_voicepos (voc, 0, 0);
    if (extern_drv)
	driver->setnote (voc, voice_array[voc].note);
}


void xmp_drv_pastnote (int chn, int act)
{
    int voc;

    chn += numusr;
    for (voc = numvoc; voc--;)
	if (voice_array[voc].root == chn && voice_array[voc].chn >= numtrk) {
	    if (act == XMP_ACT_CUT)
		drv_resetvoice (voc, TURN_ON);
	    else
		voice_array[voc].act = act;
	}
}


void xmp_drv_voicepos (int chn, int pos)
{
    int voc;
    struct patch_info *pi;

    chn += numusr;
    if ((uint32) chn >= numchn || (uint32) (voc = ch2vo_array[chn]) >= numvoc)
	return;

    pi = patch_array[voice_array[voc].smp];

    if (pi->base_note != C4_FREQ)	/* process crunching samples */
	pos = ((long long) pos << SMIX_SFT_FT) / (int)
	    (((long long) pi->base_note << SMIX_SFT_FT) / C4_FREQ);

    if (pos > pi->len)	/* Attempt to set offset beyond the end of sample */
	return;

    smix_voicepos (voc, pos, 0);
    if (extern_drv)
	driver->voicepos (voc, pos << !!(pi->mode & WAVE_16_BITS));
}


int xmp_drv_cstat (int chn)
{
    int voc;

    chn += numusr;
    if ((uint32) chn >= numchn || (uint32) (voc = ch2vo_array[chn]) >= numvoc)
	return XMP_CHN_DUMB;

    return chn < numtrk ? XMP_CHN_ACTIVE : voice_array[voc].act;
}


inline void xmp_drv_echoback (int msg)
{
    driver->echoback (msg);
}


inline int xmp_drv_getmsg ()
{
    return driver->getmsg ();
}


extern int **xmp_mix_buffer;
int hold_buffer[256];
int hold_enabled = 0;
inline void xmp_drv_bufdump ()
{
    int i = softmixer ();
    if (hold_enabled)
	memcpy (hold_buffer, *xmp_mix_buffer, 256 * sizeof (int));
    driver->bufdump (i);
}


inline void xmp_drv_starttimer ()
{
    xmp_drv_sync (0);
    driver->starttimer ();
}


void xmp_drv_stoptimer ()
{
    int voc;

    for (voc = numvoc; voc--;)
	driver->setvol (voc, 0);
    driver->stoptimer ();

    xmp_drv_bufdump ();
}


double xmp_drv_sync (double step)
{
    static double next_time = 0;

    if (step == 0)
	next_time = step;
    driver->sync (next_time += step);

    return next_time;
}


inline void xmp_drv_bufwipe ()
{
    driver->bufwipe ();
}


int xmp_drv_writepatch (struct patch_info *patch)
{
    int num;

    if (!xmp_ctl)
	return XMP_ERR_DOPEN;

    if (!patch_array)	/* FIXME -- this makes xmms happy */
    	return XMP_OK;

    if (!patch) {
	driver->writepatch (patch);

	for (num = XMP_DEF_MAXPAT; num--;) {
	    free (patch_array[num]);
	    patch_array[num] = NULL;
	}
	return XMP_OK;
    }
    if (patch->instr_no >= XMP_DEF_MAXPAT)
	return XMP_ERR_PATCH;
    patch_array[patch->instr_no] = patch;

    return XMP_OK;
}


int xmp_drv_flushpatch (int ratio)
{
    struct patch_info *patch;
    int smp, num, crunch;

    if (!patch_array)		/* FIXME -- this makes xmms happy */
	return XMP_OK;

    if (!ratio)
	ratio = 0x10000;

    for (smp = XMP_DEF_MAXPAT, num = 0; smp--;)
	if (patch_array[smp])
	    num++;

    if (extern_drv) {
	if (xmp_ctl->verbose)
	    report ("Uploading smps : %d ", num);

	for (smp = XMP_DEF_MAXPAT; smp--;) {
	    if (!patch_array[smp])
		continue;
	    patch = patch_array[smp];

	    if (patch->len == XMP_PATCH_FM) {
	        if (xmp_ctl->verbose)
		    report ("F");
		continue;
	    }

	    crunch = xmp_cvt_crunch (&patch, ratio);
	    xmp_cvt_anticlick (patch);
	    if ((num = driver->writepatch (patch)) != XMP_OK) {
		patch_array[smp] = NULL;	/* Bad type, reset array */
		free (patch);
	    } else 
		patch_array[smp] = realloc (patch, sizeof (struct patch_info));

	    if (xmp_ctl->verbose) {
		if (num != XMP_OK)
		    report ("E");		/* Show type error */
		else if (!crunch)
		    report ("i");		/* Show sbi patch type */
		else report (crunch < 0x10000 ?
		    "c" : crunch > 0x10000 ? "x" : ".");
	    }
	}
	if (xmp_ctl->verbose)
	    report ("\n");
    } else {					/* Softmixer writepatch */
	for (smp = XMP_DEF_MAXPAT; smp--;) {
	    if (!patch_array[smp])
		continue;
	    patch = patch_array[smp];
	    xmp_cvt_anticlick (patch);
	    if (driver->writepatch (patch) != XMP_OK) {
		patch_array[smp] = NULL;	/* Bad type, reset array */
		free (patch);
	    }
	}
    }

    return XMP_OK;
}


static void adpcm4_decoder(uint8 *inp, uint8 *outp, char *tab, int len)
{
    char delta = 0;
    uint8 b0, b1;
    int i;

    len = (len + 1) / 2;

    for (i = 0; i < len; i++) {
	b0 = *inp;
	b1 = *inp++ >> 4;
        delta += tab[b0 & 0x0f];
	*outp++ = delta;
        delta += tab[b1 & 0x0f];
        *outp++ = delta;
    }
}


int xmp_drv_loadpatch (FILE *f, int id, int basefreq, int flags,
    struct xxm_sample *xxs, char *buffer)
{
    struct patch_info *patch;
    char s[5];

    /* FM patches
     */
    if (!xxs) {
	if (!(patch = calloc (1, sizeof (struct patch_info) + 11)))
	      return XMP_ERR_ALLOC;
	memcpy (patch->data, buffer, 11);
	patch->instr_no = id;
	patch->len = XMP_PATCH_FM;
	patch->base_note = 60;

	return xmp_drv_writepatch (patch);
    }
    /* Empty samples
     */
    if (xxs->len < 5) {
	if (~flags & XMP_SMP_NOLOAD)
	    fread (s, 1, xxs->len, f);
	return XMP_OK;
    }
    /* Patches with samples
     */
    if (!(patch = calloc (1, sizeof (struct patch_info) +
		xxs->len + sizeof (int))))
	  return XMP_ERR_ALLOC;

    if (flags & XMP_SMP_NOLOAD)
	memcpy (patch->data, buffer, xxs->len);
    else {
	int pos = ftell(f);
	fread (s, 1, 5, f);
	fseek (f, pos, SEEK_SET);

	if (!strncmp (s, "ADPCM", 5)) {
	    int x2 = xxs->len >> 1;
	    char table[16];

	    fseek (f, 5, SEEK_CUR);	/* Skip "ADPCM" */
	    fread (table, 1, 16, f);
	    fread (patch->data + x2, 1, x2, f);
	    adpcm4_decoder((uint8 *)patch->data + x2, (uint8 *)patch->data,
						table, xxs->len);
	} else
	    fread (patch->data, 1, xxs->len, f);
    }

#if 0
    /* dump patch to file */
    if (id == 2) {
	printf("dump patch\n");
	FILE *f = fopen("patch_data", "w");
	fwrite(patch->data, 1, xxs->len, f);
	fclose(f);
    }
#endif

    if (xxs->flg & WAVE_16_BITS) {
#ifdef WORDS_BIGENDIAN
	if ((flags & XMP_SMP_BIGEND) || (~xmp_ctl->fetch & XMP_CTL_BIGEND))
	   xmp_cvt_sex (xxs->len, patch->data);
#else
	if ((flags & XMP_SMP_BIGEND) || (xmp_ctl->fetch & XMP_CTL_BIGEND))
	    xmp_cvt_sex (xxs->len, patch->data);
#endif
    }
    if (flags & XMP_SMP_7BIT)
	xmp_cvt_2xsmp (xxs->len, patch->data);
    if (flags & XMP_SMP_DIFF)
	xmp_cvt_diff2abs (xxs->len, xxs->flg & WAVE_16_BITS, patch->data);
    else if (flags & XMP_SMP_8BDIFF)
	xmp_cvt_diff2abs (xxs->len, 0, patch->data);
    if (flags & XMP_SMP_VIDC)
	xmp_cvt_vidc(xxs->len, patch->data);

    patch->key = GUS_PATCH;
    patch->instr_no = id;
    patch->mode = xxs->flg;
    patch->mode |= (flags & XMP_SMP_UNS) ? WAVE_UNSIGNED : 0;
    patch->len = xxs->len;
    patch->loop_start = (xxs->lps > xxs->len) ? xxs->len : xxs->lps;
    patch->loop_end = (xxs->lpe > xxs->len) ? xxs->len : xxs->lpe;
    if (patch->loop_end <= patch->loop_start || !(patch->mode & WAVE_LOOPING))
	patch->mode &= ~(WAVE_LOOPING | WAVE_BIDIR_LOOP | WAVE_LOOP_BACK);

    patch->base_note = C4_FREQ;
    patch->base_freq = basefreq;
    patch->high_note = 0x7fffffff;
    patch->low_note = 0;
    patch->volume = 120;
    patch->detuning = 0;
    patch->panning = 0;

    xmp_cvt_crunch (&patch, flags & XMP_SMP_8X ? 0x80000 : 0x10000);
    return xmp_drv_writepatch (patch);
}


inline struct xmp_drv_info *xmp_drv_array ()
{
    return drv_array;
}


void xmp_register_driver_callback (void (*callback)(void *, int))
{
    _driver_callback = callback;
}

