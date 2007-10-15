/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
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


inline struct xmp_module_info *xmp_get_module_info(xmp_context ctx, struct xmp_module_info *i)
{
    struct xmp_player_context *p = (struct xmp_player_context *)ctx;
    struct xmp_mod_context *m = &p->m;

    strncpy(i->name, m->name, 0x40);
    strncpy(i->type, m->type, 0x40);
    i->chn = p->m.xxh->chn;
    i->pat = p->m.xxh->pat;
    i->ins = p->m.xxh->ins;
    i->trk = p->m.xxh->trk;
    i->smp = p->m.xxh->smp;
    i->len = p->m.xxh->len;

    return i;
}


inline char *xmp_get_driver_description()
{
    return xmp_ctl->description;
}


inline struct xmp_fmt_info *xmp_get_fmt_info(struct xmp_fmt_info **x)
{
    return *x = __fmt_head;
}


inline struct xmp_drv_info *xmp_get_drv_info(struct xmp_drv_info **x)
{
    return *x = xmp_drv_array();		/* **** H:FIXME **** */
}
