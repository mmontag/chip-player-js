/* nomarch 1.0 - extract old `.arc' archives.
 * Copyright (C) 2001 Russell Marks. See main.c for license details.
 *
 * readrle.h
 */

struct rledata {
  int lastchr,repeating;
};

extern void outputrle(int chr,void (*outputfunc)(int), struct rledata *);
extern unsigned char *convert_rle(unsigned char *data_in,
                                  unsigned long in_len,
                                  unsigned long orig_len);
