/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "inflate.h"


int decrunch_muse(FILE *f, FILE *fo)                          
{                                                          
	uint32 checksum;
  
	fseek(f, 24, SEEK_SET);
	inflate(f, fo, &checksum, 0);

	return 0;
}
