/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifndef __XMP_CONVERT_H
#define __XMP_CONVERT_H

#include "driver.h"

/* Convert functions ... */

void xmp_cvt_hsc2sbi (char *);
void xmp_cvt_diff2abs (int, int, char *);
void xmp_cvt_sig2uns (int, int, char *);
void xmp_cvt_sex (int, char *);
void xmp_cvt_2xsmp (int, char *);
void xmp_cvt_vidc (int, char *);
void xmp_cvt_to8bit (void);
void xmp_cvt_to16bit (void);
void xmp_cvt_bid2und (void);

void xmp_cvt_anticlick (struct patch_info *);
int xmp_cvt_crunch (struct patch_info **, unsigned int);

#endif /* __XMP_CONVERT_H */
