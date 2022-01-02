/* PowerPacker decrunch
 * Based on code by Stuart Caie <kyzer@4u.net>
 * This software is in the Public Domain
 */

/* Code from Heikki Orsila's amigadepack 0.02 to replace previous
 * PowerPack depacker with license issues.
 *
 * Modified for xmp by Claudio Matsuoka, 08/2007
 * - merged mld's checks from the old depack sources. Original credits:
 *   - corrupt file and data detection
 *     (thanks to Don Adan and Dirk Stoecker for help and infos)
 *   - implemeted "efficiency" checks
 *   - further detection based on code by Georg Hoermann
 *
 * Modified for xmp by Claudio Matsuoka, 05/2013
 * - decryption code removed
 */

#include "../common.h"
#include "depacker.h"

/* #define val(p) ((p)[0]<<16 | (p)[1] << 8 | (p)[2]) */


#define PP_READ_BITS(nbits, var) do {                          \
  bit_cnt = (nbits);                                           \
  while (bits_left < bit_cnt) {                                \
    if (buf_src < src) return 0; /* out of source bits */      \
    bit_buffer |= (*--buf_src << bits_left);                   \
    bits_left += 8;                                            \
  }                                                            \
  (var) = 0;                                                   \
  bits_left -= bit_cnt;                                        \
  while (bit_cnt--) {                                          \
    (var) = ((var) << 1) | (bit_buffer & 1);                   \
    bit_buffer >>= 1;                                          \
  }                                                            \
} while(0)

#define PP_BYTE_OUT(byte) do {                                 \
  if (out <= dest) return 0; /* output overflow */             \
  *--out = (byte);                                             \
  written++;                                                   \
} while (0)

static int ppDecrunch(uint8 *src, uint8 *dest, uint8 *offset_lens,
               uint32 src_len, uint32 dest_len, uint8 skip_bits)
{
  uint8 *buf_src, *out, *dest_end, bits_left = 0, bit_cnt;
  uint32 bit_buffer = 0, x, todo, offbits, offset, written=0;

  if (src == NULL || dest == NULL || offset_lens == NULL) return 0;
  if (skip_bits > 32) return 0;

  /* set up input and output pointers */
  buf_src = src + src_len;
  out = dest_end = dest + dest_len;

  /* skip the first few bits */
  PP_READ_BITS(skip_bits, x);

  /* while there are input bits left */
  while (written < dest_len) {
    PP_READ_BITS(1, x);
    if (x == 0) {
      /* 1bit==0: literal, then match. 1bit==1: just match */
      todo = 1; do { PP_READ_BITS(2, x); todo += x; } while (x == 3);
      while (todo--) { PP_READ_BITS(8, x); PP_BYTE_OUT(x); }

      /* should we end decoding on a literal, break out of the main loop */
      if (written == dest_len) break;
    }

    /* match: read 2 bits for initial offset bitlength / match length */
    PP_READ_BITS(2, x);
    offbits = offset_lens[x];
    todo = x+2;
    if (x == 3) {
      PP_READ_BITS(1, x);
      if (x==0) offbits = 7;
      PP_READ_BITS(offbits, offset);
      do { PP_READ_BITS(3, x); todo += x; } while (x == 7);
    }
    else {
      PP_READ_BITS(offbits, offset);
    }
    if ((out + offset) >= dest_end) return 0; /* match overflow */
    while (todo--) { x = out[offset]; PP_BYTE_OUT(x); }
  }

  /* all output bytes written without error */
  return 1;
  /* return (src == buf_src) ? 1 : 0; */
}

static int ppdepack(uint8 *data, size_t len, void **output, long *outlen)
{
  /* PP FORMAT:
   *      1 longword identifier           'PP20' or 'PX20'
   *     [1 word checksum (if 'PX20')     $ssss]
   *      1 longword efficiency           $eeeeeeee
   *      X longwords crunched file       $cccccccc,$cccccccc,...
   *      1 longword decrunch info        'decrlen' << 8 | '8 bits other info'
   */
  /* uint8 *crypted; */

  if (len < 16) {
    /*fprintf(stderr, "File is too short to be a PP file (%u bytes)\n", len);*/
    return -1;
  }

  if (data[0]=='P' && data[1]=='P' && data[2]=='2' && data[3]=='0') {
    if (len & 0x03) {
      /*fprintf(stderr, "File length is not a multiple of 4\n");*/
      return -1;
    }
    /*crypted = 0;*/
  }
#if 0
  else if (data[0]=='P' && data[1]=='X' && data[2]=='2' && data[3]=='0') {
    if ((len-2) & 0x03) {
      /*fprintf(stderr, "(file length - 2) is not a multiple of 4\n");*/
      return -1;
    }
    crypted = 1;
  }
#endif
  else {
    /*fprintf(stderr, "File does not have the PP signature\n");*/
    return -1;
  }

  *outlen = readmem24b(data + len - 4);

  /* fprintf(stderr, "decrunched length = %u bytes\n", *outlen); */

  *output = (uint8 *) malloc(*outlen);
  if (*output == NULL) {
    /*fprintf(stderr, "out of memory!\n");*/
    return -1;
  }

  /* if (crypted == 0) { */
    /*fprintf(stderr, "not encrypted, decrunching anyway\n"); */
    if (ppDecrunch(&data[8], (uint8 *) *output, &data[4], len-12, *outlen, data[len-1])) {
      /* fprintf(stderr, "Decrunch successful! "); */
      return 0;
    }
  /**/

  free(*output);
  *output = NULL;
  *outlen = 0;
  return -1;
}

static int test_pp(unsigned char *b)
{
	return memcmp(b, "PP20", 4) == 0;
}

static int decrunch_pp(HIO_HANDLE *f, void **out, long inlen, long *outlen)
{
    uint8 *packed;
    int unplen;

    /* Amiga longwords are only on even addresses.
     * The pp20 data format has the length stored in a longword
     * after the packed data, so I guess a file that is not even
     * is probl not a valid pp20 file. Thanks for Don Adan for
     * reminding me on this! - mld
     */

    if ((inlen != (inlen / 2) * 2)) {
	 /*fprintf(stderr, "filesize not even\n");*/
         goto err;
    }

    packed = (uint8 *) malloc(inlen);
    if (packed == NULL) {
	 /*fprintf(stderr, "can't allocate memory for packed data\n");*/
	 goto err;
    }

    if (hio_read(packed, 1, inlen, f) != inlen) {
         goto err1;
    }

    /* Hmmh... original pp20 only support efficiency from 9 9 9 9 up to 9 10 12 13, afaik
     * but the xfd detection code says this... *sigh*
     *
     * move.l 4(a0),d0
     * cmp.b #9,d0
     * blo.b .Exit
     * and.l #$f0f0f0f0,d0
     * bne.s .Exit
     */

    if (((packed[4] < 9) || (packed[5] < 9) || (packed[6] < 9) || (packed[7] < 9))) {
	 /*fprintf(stderr, "invalid efficiency\n");*/
         goto err1;
    }


    if (((readmem24b(packed +4)  * 256  + packed[7]) & 0xf0f0f0f0) != 0 ) {
	 /*fprintf(stderr, "invalid efficiency(?)\n");*/
         goto err1;
    }

    unplen = readmem24b(packed + inlen - 4);
    if (!unplen) {
	 /*fprintf(stderr, "not a powerpacked file\n");*/
         goto err1;
    }

    if (ppdepack (packed, inlen, out, outlen) == -1) {
	 /*fprintf(stderr, "error while decrunching data...");*/
         goto err1;
    }

    free (packed);

    return 0;

err1:
    free(packed);
err:
    return -1;
}

struct depacker libxmp_depacker_pp = {
	test_pp,
	NULL,
	decrunch_pp
};
