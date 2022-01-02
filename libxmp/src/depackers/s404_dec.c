/*
   StoneCracker S404 algorithm data decompression routine
   (c) 2006 Jouni 'Mr.Spiv' Korhonen. The code is in public domain.

   from shd:
   Some portability notes. We are using int32_t as a file size, and that fits
   all Amiga file sizes. size_t is of course the right choice.

   Warning: Code is not re-entrant.

   modified for xmp by Claudio Matsuoka, Jan 2010
   (couldn't keep stdint types, some platforms we build on didn't like them)
*/

/*#include <assert.h>*/
#include "../common.h"
#include "depacker.h"


struct bitstream {
  /* bit buffer for rolling data bit by bit from the compressed file */
  uint32 word;

  /* bits left in the bit buffer */
  int left;

  /* compressed data source */
  uint16 *src;
  uint8 *orgsrc;
};


static int initGetb(struct bitstream *bs, uint8 *src, uint32 src_length)
{
  int eff;

  bs->src = (uint16 *) (src + src_length);
  bs->orgsrc = src;

  bs->left = readmem16b((uint8 *)bs->src); /* bit counter */
  /*if (bs->left & (~0xf))
    fprintf(stderr, "Workarounded an ancient stc bug\n");*/
  /* mask off any corrupt bits */
  bs->left &= 0x000f;
  bs->src--;

  /* get the first 16-bits of the compressed stream */
  bs->word = readmem16b((uint8 *)bs->src);
  bs->src--;

  eff = readmem16b((uint8 *)bs->src); /* efficiency */
  bs->src--;

  return eff;
}


/* get nbits from the compressed stream */
static int getb(struct bitstream *bs, int nbits)
{
  bs->word &= 0x0000ffff;

  /* If not enough bits in the bit buffer, get more */
  if (bs->left < nbits) {
    bs->word <<= bs->left;
    /* assert((bs->word & 0x0000ffffU) == 0); */

    /* Check that we don't go out of bounds */
    /*assert((uint8 *)bs->src >= bs->orgsrc);*/
    if (bs->orgsrc > (uint8 *)bs->src) {
       return -1;
    }

    bs->word |= readmem16b((uint8 *)bs->src);
    bs->src--;

    nbits -= bs->left;
    bs->left = 16; /* 16 unused (and some used) bits left in the word */
  }

  /* Shift nbits off the word and return them */
  bs->left -= nbits;
  bs->word <<= nbits;
  return bs->word >> 16;
}


/* Returns bytes still to read.. or < 0 if error. */
static int checkS404File(uint32 *buf,
                         int32 *oLen, int32 *pLen, int32 *sLen )
{
  if (memcmp(buf, "S404", 4) != 0)
    return -1;

  *sLen = readmem32b((uint8 *)&buf[1]); /* Security length */
  if (*sLen < 0)
    return -1;
  *oLen = readmem32b((uint8 *)&buf[2]); /* Depacked length */
  if (*oLen <= 0)
    return -1;
  *pLen = readmem32b((uint8 *)&buf[3]); /* Packed length */
  if (*pLen <= 6)
    return -1;

  return 0;
}


static int decompressS404(uint8 *src, uint8 *orgdst,
                          int32 dst_length, int32 src_length)
{
  uint16 w;
  int32 eff;
  int32 n;
  uint8 *dst;
  int32 oLen = dst_length;
  struct bitstream bs;
  int x;

  dst = orgdst + oLen;

  eff = initGetb(&bs, src, src_length);

  /* Sanity check--prevent invalid shift exponents. */
  if (eff < 6 || eff >= 32)
    return -1;

  /*printf("_bl: %02X, _bb: %04X, eff: %d\n",_bl,_bb, eff);*/

  while (oLen > 0) {
    x = getb(&bs, 9);

    /* Sanity check */
    if (x < 0) {
      return -1;
    }

    w = x;

    /*printf("oLen: %d _bl: %02X, _bb: %04X, w: %04X\n",oLen,_bl,_bb,w);*/

    if (w < 0x100) {
      /*assert(dst > orgdst);*/
      if (orgdst >= dst) {
        return -1;
      }
      *--dst = w;
      /*printf("0+[8] -> %02X\n",w);*/
      oLen--;
    } else if (w == 0x13e || w == 0x13f) {
      w <<= 4;
      x = getb(&bs, 4);
      /* Sanity check */
      if (x < 0) {
        return -1;
      }
      w |= x;

      n = (w & 0x1f) + 14;
      oLen -= n;
      while (n-- > 0) {
        x = getb(&bs, 8);
        /* Sanity check */
        if (x < 0) {
          return -1;
        }
        w = x;

        /*printf("1+001+1111+[4] -> [8] -> %02X\n",w);*/
        /*assert(dst > orgdst);*/
        if (orgdst >= dst) {
          return -1;
        }

        *--dst = w;
      }
    } else {
      if (w >= 0x180) {
        /* copy 2-3 */
        n = w & 0x40 ? 3 : 2;

        if (w & 0x20) {
          /* dist 545 -> */
          w = (w & 0x1f) << (eff - 5);
          x = getb(&bs, eff - 5);
          /* Sanity check */
          if (x < 0) {
            return -1;
          }
          w |= x;
          w += 544;
          /* printf("1+1+[1]+1+[%d] -> ", eff); */
        } else if (w & 0x30) {
          // dist 1 -> 32
          w = (w & 0x0f) << 1;
          x = getb(&bs, 1);
          /* Sanity check */
          if (x < 0) {
            return -1;
          }
          w |= x;
          /* printf("1+1+[1]+01+[5] %d %02X %d %04X-> ",n,w, _bl, _bb); */
        } else {
          /* dist 33 -> 544 */
          w = (w & 0x0f) << 5;
          x = getb(&bs, 5);
          /* Sanity check */
          if (x < 0) {
            return -1;
          }
          w |= x;
          w += 32;
          /* printf("1+1+[1]+00+[9] -> "); */
        }
      } else if (w >= 0x140) {
        /* copy 4-7 */
        n = ((w & 0x30) >> 4) + 4;

        if (w & 0x08) {
          /* dist 545 -> */
          w = (w & 0x07) << (eff - 3);
          x = getb(&bs, eff - 3);
          /* Sanity check */
          if (x < 0) {
            return -1;
          }
          w |= x;
          w += 544;
          /* printf("1+01+[2]+1+[%d] -> ", eff); */
        } else if (w & 0x0c) {
          /* dist 1 -> 32 */
          w = (w & 0x03) << 3;
          x = getb(&bs, 3);
          /* Sanity check */
          if (x < 0) {
            return -1;
          }
          w |= x;
          /* printf("1+01+[2]+01+[5] -> "); */
        } else {
          /* dist 33 -> 544 */
          w = (w & 0x03) << 7;
          x = getb(&bs, 7);
          /* Sanity check */
          if (x < 0) {
            return -1;
          }
          w |= x;
          w += 32;
          /* printf("1+01+[2]+00+[9] -> "); */
        }
      } else if (w >= 0x120) {
        /* copy 8-22 */
        n = ((w & 0x1e) >> 1) + 8;

        if (w & 0x01) {
          /* dist 545 -> */
          x = getb(&bs, eff);
          /* Sanity check */
          if (x < 0) {
            return -1;
          }
          w = x;
          w += 544;
          /* printf("1+001+[4]+1+[%d] -> ", eff); */
        } else {
          x = getb(&bs, 6);
          /* Sanity check */
          if (x < 0) {
            return -1;
          }
          w = x;

          if (w & 0x20) {
            /* dist 1 -> 32 */
            w &= 0x1f;
            /* printf("1+001+[4]+001+[5] -> "); */
          } else {
            /* dist 33 -> 544 */
            w <<= 4;
            x = getb(&bs, 4);
            /* Sanity check */
            if (x < 0) {
              return -1;
            }
            w |= x;

            w += 32;
            /* printf("1+001+[4]+00+[9] -> "); */
          }
        }
      } else {
        w = (w & 0x1f) << 3;
        x = getb(&bs, 3);
        /* Sanity check */
        if (x < 0) {
          return -1;
        }
        w |= x;
        n = 23;

        while (w == 0xff) {
          n += w;
          x = getb(&bs, 8);
          /* Sanity check */
          if (x < 0) {
            return -1;
          }
          w = x;
        }
        n += w;

        x = getb(&bs, 7);
        w = x;

        if (w & 0x40) {
          /* dist 545 -> */
          w = (w & 0x3f) << (eff - 6);
          x = getb(&bs, eff - 6);
          /* Sanity check */
          if (x < 0) {
            return -1;
          }
          w |= x;

          w += 544;
        } else if (w & 0x20) {
          /* dist 1 -> 32 */
          w &= 0x1f;
          /* printf("1+000+[8]+01+[5] -> "); */
        } else {
          /* dist 33 -> 544; */
          w <<= 4;
          x = getb(&bs, 4);
          /* Sanity check */
          if (x < 0) {
            return -1;
          }
          w |= x;

          w += 32;
          /* printf("1+000+[8]+00+[9] -> "); */
        }
      }

      /* printf("<%d,%d>\n",n,w+1); fflush(stdout); */
      oLen -= n;

      while (n-- > 0) {
        /* printf("Copying: %02X\n",dst[w]); */
        dst--;
        if (dst < orgdst || (dst + w + 1) >= (orgdst + dst_length))
            return -1;
        *dst = dst[w + 1];
      }
    }
  }

  return 0;
}

static int test_s404(unsigned char *b)
{
	return memcmp(b, "S404", 4) == 0;
}

static int decrunch_s404(HIO_HANDLE *in, void **out, long inlen, long *outlen)
{
  int32 oLen, sLen, pLen;
  uint8 *dst = NULL;
  uint8 *buf, *src;

  if (inlen <= 16)
    return -1;
  src = buf = (uint8 *) malloc(inlen);
  if (src == NULL)
    return -1;
  if (hio_read(buf, 1, inlen, in) != inlen) {
    goto error;
  }

  if (checkS404File((uint32 *) src, &oLen, &pLen, &sLen)) {
    /*fprintf(stderr,"S404 Error: checkS404File() failed..\n");*/
    goto error;
  }

  /* Sanity check */
  if (pLen > inlen - 18) {
    goto error;
  }

  /**
   * Best case ratio of S404 sliding window:
   *
   *  2-3:  9b + (>=1b) -> 2-3B  ->  24:10
   *  4-7:  9b + (>=3b) -> 4-7B  ->  56:12
   *  8:22: 9b + (>=6b) -> 8-22B -> 176:15
   *  23+:  9b + 3b + 8b * floor((n-23)/255) + 7b + (>=0b) -> n B -> ~255:1
   */
  if (pLen < (oLen / 255)) {
    goto error;
  }

  if ((dst = (uint8 *)malloc(oLen)) == NULL) {
    /*fprintf(stderr,"S404 Error: malloc(%d) failed..\n", oLen);*/
    goto error;
  }

  /* src + 16 skips S404 header */
  if (decompressS404(src + 16, dst, oLen, pLen) < 0) {
      goto error1;
  }

  free(src);

  *out = dst;
  *outlen = oLen;

  return 0;

 error1:
  free(dst);
 error:
  free(src);
  return -1;
}

struct depacker libxmp_depacker_s404 = {
	test_s404,
	NULL,
	decrunch_s404
};
