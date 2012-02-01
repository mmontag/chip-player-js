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

#include <string.h>
#include "driver.h"

extern struct xmp_fmt_info *__fmt_head;


#if 0
struct xmp_module_info *xmp_get_module_info(xmp_context opaque, struct xmp_module_info *i)
{
    struct xmp_context *ctx = (struct xmp_context *)opaque;
    struct xmp_mod_context *m = &ctx->m;

    strncpy(i->name, m->mod.name, 0x40);
    strncpy(i->type, m->mod.type, 0x40);
    i->chn = m->mod.chn;
    i->pat = m->mod.pat;
    i->ins = m->mod.ins;
    i->trk = m->mod.trk;
    i->smp = m->mod.smp;
    i->len = m->mod.len;
    i->bpm = m->mod.bpm;
    i->tpo = m->mod.tpo;
    i->time = m->time;

    return i;
}
#endif


char *xmp_get_driver_description(xmp_context opaque)
{
    struct xmp_context *ctx = (struct xmp_context *)opaque;
    struct xmp_driver_context *d = &ctx->d;

    return d->description;
}


struct xmp_fmt_info *xmp_get_fmt_info(struct xmp_fmt_info **x)
{
    return *x = __fmt_head;
}


struct xmp_drv_info *xmp_get_drv_info(struct xmp_drv_info **x)
{
    return *x = xmp_drv_array();		/* **** H:FIXME **** */
}
