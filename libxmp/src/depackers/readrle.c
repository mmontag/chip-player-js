/* nomarch 1.0 - extract old `.arc' archives.
 * Copyright (C) 2001 Russell Marks. See unarc.c for license details.
 *
 * readrle.c - read RLE-compressed files.
 *
 * Also provides the generic outputrle() for the other RLE-using methods
 * to use.
 */

#include "../common.h"
#include "readrle.h"


#if 0
static void rawoutput(int byte, struct data_in_out *io)
{
if(io->data_out_point<io->data_out_max)
  *io->data_out_point++=byte;
}
#endif


/* call with -1 before starting, to make sure state is initialised */
int libxmp_outputrle(int chr,int (*outputfunc)(int, struct data_in_out *), struct rledata *rd, struct data_in_out *io)
{
int f;

if(chr==-1)
  {
  rd->lastchr=rd->repeating=0;
  return 0;
  }

if(rd->repeating)
  {
  if(chr==0)
  {
    if ((*outputfunc)(0x90, io) != 0)
      return -1;
  }
  else
  {
    for(f=1;f<chr;f++)
      if ((*outputfunc)(rd->lastchr, io) != 0)
        return -1;
  }
  rd->repeating=0;
  }
else
  {
  if(chr==0x90)
    rd->repeating=1;
  else
    {
    if ((*outputfunc)(chr, io) != 0)
      return -1;
    rd->lastchr=chr;
    }
  }
  return 0;
}

#if 0
unsigned char *convert_rle(unsigned char *data_in,
                           unsigned long in_len,
                           unsigned long orig_len)
{
unsigned char *data_out;
struct rledata rd;
struct data_in_out io;

if((data_out=malloc(orig_len))==NULL)
  fprintf(stderr,"nomarch: out of memory!\n"),exit(1);

io.data_in_point=data_in; io.data_in_max=data_in+in_len;
io.data_out_point=data_out; io.data_out_max=data_out+orig_len;
outputrle(-1,NULL, &rd, &io);

while(io.data_in_point<io.data_in_max)
  outputrle(*io.data_in_point++,rawoutput, &rd, &io);

return(data_out);
}
#endif
