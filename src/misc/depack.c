/* Copyright (c) Marc Espie, 1995
 * See accompanying file README for distribution information
 *
 * Modified by Claudio Matsuoka for use in xmp.
 * small modifications by mld for use with uade
 *
 * $Id: depack.c,v 1.2 2001-11-15 09:59:54 cmatsuoka Exp $
 */

/* part of libdecr.a for uade
 *  
 * REINSERTED:
 *  "if packed" and memory allocation checks from original ppunpack.
 *    
 * DONE:
 *  corrupt file and data detection 
 *  (thanks to Don Adan and Dirk Stoecker for help and infos)
 *  implemeted "efficiency" checks
 *  further detection based on code by Georg Hoermann
 *
 * TODO:
 *  more corrupt file checking, because the decruncher segfaults on some corrupt
 *  data :-(
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#ifdef __EMX__
#include <sys/types.h>
#endif
#include <sys/stat.h>
#include <unistd.h>

#include "xmpi.h"

#define val(p) ((p)[0]<<16 | (p)[1] << 8 | (p)[2])

static uint32 shift_in;
static uint32 counter;
static uint8 *source;


static uint32 get_bits (uint32 n)
{
    uint32 result = 0;
    int i = n;

      while (i--){
	if (counter == 0) {
	    counter = 8;
	    shift_in = *--source;
	}

	result = (result << 1) | (shift_in & 1);
	shift_in >>= 1;
	counter--;
    }
    return result;
}


static int
ppdepack (uint8 *packed, uint8 *depacked, uint32 plen, uint32 unplen)
{
    uint8 *dest;
    int n_bits;
    int idx;
    uint32 bytes;
    int to_add;
    uint32 offset;
    uint8 offset_sizes[4];
    int i;

    offset_sizes[0] = packed[4];	/* skip signature */
    offset_sizes[1] = packed[5];
    offset_sizes[2] = packed[6];
    offset_sizes[3] = packed[7];

    /* initialize source of bits */

    source = packed + plen - 4;
    dest = depacked + unplen;


    /* skip bits */
    get_bits (source[3]);

        /* do it forever, i.e., while the whole file isn't unpacked */
    while (dest > depacked) {

	/* copy some bytes from the source anyway */

    if (source < packed)
	{
	return -1;
	}

	if (get_bits (1) == 0) {
	    bytes = 0;
	    do {
		to_add = get_bits (2);
		bytes += to_add;
	    } while (to_add == 3);
	    for (i = 0; i <= bytes; i++)
	    {
	    if (--dest < depacked)
		return 0;
		*dest = get_bits (8);
	    }	
	}
	/* decode what to copy from the destination file */

	idx = get_bits (2);
	n_bits = offset_sizes[idx];
	/* bytes to copy */
	bytes = idx + 1;
	if (bytes == 4) {	/* 4 means >=4 */

	    /* and maybe a bigger offset */
	    if (get_bits (1) == 0)
		offset = get_bits (7);
	    else
		offset = get_bits (n_bits);

	    do {
		to_add = get_bits (3);
		bytes += to_add;
	    } while (to_add == 7);
	} else
	    offset = get_bits (n_bits);

	    ++offset;
	for (i = 0; i <= bytes; i++) {
	    if (--dest < depacked)
	    return 0;
	    if (dest + offset > depacked + unplen) return -1;
	    *dest = dest[offset];
	}

    }

    return 0;
}


int decrunch_pp (FILE *f, FILE *fo)
{
    uint8 *packed, *unpacked;
    int plen, unplen;
    struct stat st;

    if (fo == NULL)
	return -1;

    fstat (fileno (f), &st);
    plen = st.st_size;
    counter = 0;


    /* Amiga longwords are only on even addresses.
     * The pp20 data format has the length stored in a longword
     * after the packed data, so I guess a file that is not even
     * is probl not a valid pp20 file. Thanks for Don Adan for
     * reminding me on this! - mld
     */

    if ((plen != (plen / 2) * 2))
	{    
	 fprintf(stderr, "filesize not even...");
	 return -1 ;
        }

    packed = malloc (plen);
    if (!packed)
	{
	 fprintf(stderr, "can't allocate memory for packed data...");
	 return -1;
	}

    fread (packed, plen, 1, f);

    /* Hmmh... original pp20 only support efficiency from 9 9 9 9 up to 9 10 12 13, afaik
     * but the xfd detection code says this... *sigh*
     *
     * move.l 4(a0),d0
     * cmp.b #9,d0
     * blo.b .Exit
     * and.l #$f0f0f0f0,d0
     * bne.s .Exit
     */	 


    if (((packed[4] < 9 ) || (packed[5] < 9 ) || (packed[6] < 9 ) || (packed[7] < 9)))
	{
	 fprintf(stderr, "invalid efficiency...");
	 return -1;
	}


    if 	(((((val (packed +4) ) * 256 ) + packed[7] ) & 0xf0f0f0f0) != 0 )
	{
	 fprintf(stderr, "invalid efficiency(?)...");
	 return -1;
	}
    

    unplen = val (packed + plen - 4);

    if (!unplen) 
	{
	  fprintf(stderr, "not a powerpacked file...");
	  return -1;
	}
    
    unpacked = (uint8 *) malloc (unplen); 
    if (!unpacked)
	{
	 fprintf(stderr, "can't allocate memory for unpacked data...");
	 return -1;
	}

    if (ppdepack (packed, unpacked, plen, unplen) == -1)
	{
	 fprintf(stderr, "error while decrunching data...");
	 return -1;
	}
     
    fwrite (unpacked, unplen, 1, fo);
    free (unpacked);
    free (packed);

    return 0;
}
