/* Extended Module Player
 * Copyright (C) 1997-2001 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: synth.h,v 1.1 2001-06-02 20:28:29 cmatsuoka Exp $
 */

#ifndef __SYNTH_H
#define __SYNTH_H

int	synth_init	(int);
int	synth_deinit	(void);
int	synth_reset	(void);
void	synth_setpatch	(int, uint8 *);
void	synth_setnote	(int, int, int);
void	synth_mixer	(int*, int, int, int, int);


#endif
