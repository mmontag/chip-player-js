/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

extern struct xmp_fmt_info *__fmt_head;


struct xmp_fmt_info *xmp_get_fmt_info(struct xmp_fmt_info **x)
{
    return *x = __fmt_head;
}
