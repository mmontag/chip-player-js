/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
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


inline struct xmp_module_info *xmp_get_module_info (struct xmp_module_info *i)
{
    strncpy (i->name, xmp_ctl->name, 0x40);
    strncpy (i->type, xmp_ctl->type, 0x40);
    i->chn = xxh->chn;
    i->pat = xxh->pat;
    i->ins = xxh->ins;
    i->trk = xxh->trk;
    i->smp = xxh->smp;
    i->len = xxh->len;

    return i;
}


inline char *xmp_get_driver_description ()
{
    return xmp_ctl->description;
}


inline struct xmp_fmt_info *xmp_get_fmt_info (struct xmp_fmt_info **x)
{
    return *x = __fmt_head;
}


inline struct xmp_drv_info *xmp_get_drv_info (struct xmp_drv_info **x)
{
    return *x = xmp_drv_array ();		/* #?# **** FIXME **** */
}
