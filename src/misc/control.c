/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: control.c,v 1.16 2007-10-15 19:19:22 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include "driver.h"
#include "mixer.h"
#include "formats.h"

static int drv_parm = 0;
extern struct xmp_drv_info drv_callback;
//extern struct xmp_ord_info xxo_info[XMP_DEF_MAXORD];
int big_endian;

int pw_init(void);

void *xmp_create_context()
{
	return calloc(1, sizeof(struct xmp_player_context));
}

void xmp_free_context(xmp_context ctx)
{
	free(ctx);
}

void xmp_init_callback(struct xmp_control *ctl, void (*callback)(void *, int))
{
    xmp_drv_register(&drv_callback);
    xmp_init_formats();
    pw_init();
    xmp_register_driver_callback(callback);
    ctl->drv_id = "callback";
}

void xmp_init(int argc, char **argv, struct xmp_control *ctl)
{
    int num;
    uint16 w;

    w = 0x00ff;
    big_endian = (*(char *)&w == 0x00);

    xmp_init_drivers();
    xmp_init_formats();
    pw_init();

    memset (ctl, 0, sizeof (struct xmp_control));
    xmp_event_callback = NULL;

    /* Set defaults */
    ctl->freq = 44100;
    ctl->mix = 80;
    ctl->resol = 16;
    ctl->flags = XMP_CTL_DYNPAN | XMP_CTL_FILTER | XMP_CTL_ITPT;

    /* Set max number of voices per channel */
    ctl->maxvoc = 16;

    /* must be parsed before loading the rc file. */
    for (num = 1; num < argc; num++) {
	if (!strcmp (argv[num], "--norc"))
	    break;
    }
    if (num >= argc)
	xmpi_read_rc(ctl);

    xmpi_tell_wait();
}


inline int xmp_open_audio (struct xmp_control *ctl)
{
    return xmp_drv_open (ctl);
}


inline void xmp_close_audio ()
{
    xmp_drv_close ();
}


void xmp_set_driver_parameter (struct xmp_control *ctl, char *s)
{
    ctl->parm[drv_parm] = s;
    while (isspace (*ctl->parm[drv_parm]))
	ctl->parm[drv_parm]++;
    drv_parm++;
}


inline void xmp_register_event_callback (void (*cb)())
{
    xmp_event_callback = cb;
}


void xmp_channel_mute(int from, int num, int on)
{
    from += num - 1;

    if (num > 0) {
	while (num--)
	    xmp_drv_mute(from - num, on);
    }
}


int xmp_player_ctl(xmp_context ctx, int cmd, int arg)
{
    struct xmp_player_context *p = (struct xmp_player_context *)ctx;

    switch (cmd) {
    case XMP_ORD_PREV:
	if (xmp_ctl->pos > 0)
	    xmp_ctl->pos--;
	return xmp_ctl->pos;
    case XMP_ORD_NEXT:
	if (xmp_ctl->pos < p->m.xxh->len)
	    xmp_ctl->pos++;
	return xmp_ctl->pos;
    case XMP_ORD_SET:
	if (arg < p->m.xxh->len && arg >= 0)
	    xmp_ctl->pos = arg;
	return xmp_ctl->pos;
    case XMP_MOD_STOP:
	xmp_ctl->pos = -2;
	break;
    case XMP_MOD_PAUSE:
	xmp_ctl->pause ^= 1;
	return xmp_ctl->pause;
    case XMP_MOD_RESTART:
	xmp_ctl->pos = -1;
	break;
    case XMP_GVOL_DEC:
	if (xmp_ctl->volume > 0)
	    xmp_ctl->volume--;
	return xmp_ctl->volume;
    case XMP_GVOL_INC:
	if (xmp_ctl->volume < 64)
	    xmp_ctl->volume++;
	return xmp_ctl->volume;
    case XMP_TIMER_STOP:
	xmp_drv_stoptimer(p);
	break;
    case XMP_TIMER_RESTART:
	xmp_drv_starttimer(p);
	break;
    }

    return XMP_OK;
}


int xmp_play_module(xmp_context ctx)
{
    struct xmp_player_context *p = (struct xmp_player_context *)ctx;
    time_t t0, t1;
    int t;

    time (&t0);
    xmpi_player_start(p);
    time (&t1);
    t = difftime(t1, t0);

    xmp_ctl->start = 0;

    return t;
}


void xmp_release_module(xmp_context ctx)
{
    struct xmp_player_context *p = (struct xmp_player_context *)ctx;
    int i;

    _D (_D_INFO "Freeing memory");

    if (p->m.med_vol_table) {
	for (i = 0; i < p->m.xxh->ins; i++)
	     if (p->m.med_vol_table[i])
		free(p->m.med_vol_table[i]);
	free(p->m.med_vol_table);
    }

    if (p->m.med_wav_table) {
	for (i = 0; i < p->m.xxh->ins; i++)
	     if (p->m.med_wav_table[i])
		free(p->m.med_wav_table[i]);
	free(p->m.med_wav_table);
    }

    for (i = 0; i < p->m.xxh->trk; i++)
	free(p->m.xxt[i]);
    for (i = 0; i < p->m.xxh->pat; i++)
	free(p->m.xxp[i]);
    for (i = 0; i < p->m.xxh->ins; i++) {
	free(p->m.xxfe[i]);
	free(p->m.xxpe[i]);
	free(p->m.xxae[i]);
	free(p->m.xxi[i]);
    }
    free(p->m.xxt);
    free(p->m.xxp);
    free(p->m.xxi);
    if (p->m.xxh->smp > 0)
	free(p->m.xxs);
    free(p->m.xxim);
    free(p->m.xxih);
    free(p->m.xxfe);
    free(p->m.xxpe);
    free(p->m.xxae);
    free(p->m.xxh);
}


void xmp_get_driver_cfg (int *srate, int *res, int *chn, int *itpt)
{
    *srate = xmp_ctl->memavl ? 0 : xmp_ctl->freq;
    *res = xmp_ctl->resol ? xmp_ctl->resol : 8 /* U_LAW */;
    *chn = xmp_ctl->outfmt & XMP_FMT_MONO ? 1 : 2;
    *itpt = !!(xmp_ctl->fetch & XMP_CTL_ITPT);
}


int xmp_verbosity_level (int i)
{
    int tmp;

    tmp = xmp_ctl->verbose;
    xmp_ctl->verbose = i;

    return tmp;
}


int xmp_seek_time(xmp_context ctx, int time)
{
    struct xmp_player_context *p = (struct xmp_player_context *)ctx;
    int i, t;
    /* _D("seek to %d, total %d", time, xmp_cfg.time); */

    time *= 1000;
    for (i = 0; i < p->m.xxh->len; i++) {
	t = p->m.xxo_info[i].time;

	_D("%2d: %d %d", i, time, t);

	if (t > time) {
	    if (i > 0)
		i--;
	    xmp_ord_set(ctx, i);
	    return XMP_OK;
	}
    }
    return -1;
}
