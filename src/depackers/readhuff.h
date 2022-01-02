/* nomarch 1.0 - extract old `.arc' archives.
 * Copyright (C) 2001 Russell Marks. See main.c for license details.
 *
 * readhuff.h
 */

#ifndef LIBXMP_READHUFF_H
#define LIBXMP_READHUFF_H

unsigned char	*libxmp_convert_huff(unsigned char *data_in,
                                     unsigned long in_len,
                                     unsigned long orig_len);

#endif
