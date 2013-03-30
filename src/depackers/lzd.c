/*
Lempel-Ziv decompression.  Mostly based on Tom Pfau's assembly language
code.  The contents of this file are hereby released to the public domain.
                                 -- Rahul Dhesi 1986/11/14
*/

#include <stdio.h>
#include <stdlib.h>
#include "common.h"

#define  STACKSIZE	4000
#define  INBUFSIZ 	(IN_BUF_SIZE - 10)	/* avoid obo errors */
#define  OUTBUFSIZ	(OUT_BUF_SIZE - 10)
#define  MAXBITS	13
#define  CLEAR		256	/* clear code */
#define  Z_EOF		257	/* end of file marker */
#define  FIRST_FREE	258	/* first free code */
#define  MAXMAX		8192	/* max code + 1 */


/*
The main I/O buffer (called in_buf_adr in zoo.c) is reused
in several places.
*/

#define IN_BUF_SIZE	8192
#define OUT_BUF_SIZE	8192

/* MEM_BLOCK_SIZE must be no less than (2 * DICSIZ + MAXMATCH)
(see ar.h and lzh.h for values).  The buffer of this size will
also hold an input buffer of IN_BUF_SIZE and an output buffer
of OUT_BUF_SIZE.  FUDGE is a fudge factor, to keep some spare and
avoid off-by-one errors. */

#define FUDGE		8
#define MEM_BLOCK_SIZE	(8192 + 8192 + 256 + 8)


typedef FILE *BLOCKFILE;

struct tabentry {
   unsigned next;
   char z_ch;
};

struct local_data {
   unsigned stack_pointer;
   unsigned *stack;

   char *out_buf_adr;		/* output buffer */
   char *in_buf_adr;		/* input buffer */
   BLOCKFILE in_f, out_f; 

   char memflag;			/* memory allocated? flag */
   struct tabentry *table;		/* hash table from lzc.c */
   unsigned cur_code;
   unsigned old_code;
   unsigned in_code;

   unsigned free_code;
   int nbits;
   unsigned max_code;

   char fin_char;
   char k;

   uint32 crccode;
   uint32 *crctab;
};

#define push(x)  {  \
                     data->stack[data->stack_pointer++] = (x); \
                     if (data->stack_pointer >= STACKSIZE) \
                        fprintf(stderr, "libxmp: stack overflow in lzd().\n"); \
                  }
#define pop()    (data->stack[--data->stack_pointer])

static unsigned masks[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0x1ff, 0x3ff, 0x7ff, 0xfff, 0x1fff };
static unsigned bit_offset;
static unsigned output_offset;

#define		BLOCKREAD(f,b,c) fread((b),1,(c),(f))
#define		BLOCKWRITE(f,b,c) fwrite((b),1,(c),(f))


static void addbfcrc(char *buffer, int count, struct local_data *data)
{
   unsigned int localcrc;

   localcrc = data->crccode;

   for (; count--; )
      localcrc = (localcrc>>8) ^ data->crctab[(localcrc ^ (*buffer++)) & 0x00ff];

   data->crccode = localcrc;
}

/* rd_dcode() reads a code from the input (compressed) file and returns
its value. */
static unsigned rd_dcode(struct local_data *data)
{
   register char *ptra, *ptrb;    /* miscellaneous pointers */
   unsigned word;                     /* first 16 bits in buffer */
   unsigned byte_offset;
   char nextch;                           /* next 8 bits in buffer */
   unsigned ofs_inbyte;               /* offset within byte */

   ofs_inbyte = bit_offset % 8;
   byte_offset = bit_offset / 8;
   bit_offset = bit_offset + data->nbits;

   /* assert(data->nbits >= 9 && data->nbits <= 13); */

   if (byte_offset >= INBUFSIZ - 5) {
      int space_left;

#ifdef CHECK_BREAK
	check_break();
#endif

      /* assert(byte_offset >= INBUFSIZ - 5); */

      bit_offset = ofs_inbyte + data->nbits;
      space_left = INBUFSIZ - byte_offset;
      ptrb = byte_offset + data->in_buf_adr;          /* point to char */
      ptra = data->in_buf_adr;
      /* we now move the remaining characters down buffer beginning */
      while (space_left > 0) {
         *ptra++ = *ptrb++;
         space_left--;
      }
      /* assert(ptra - data->in_buf_adr == ptrb - (data->in_buf_adr + byte_offset));
      assert(space_left == 0); */

      if (BLOCKREAD (data->in_f, ptra, byte_offset) == -1)
         fprintf(stderr, "libxmp: I/O error in lzd:rd_dcode.\n");
      byte_offset = 0;
   }
   ptra = byte_offset + data->in_buf_adr;
   /* NOTE:  "word = *((int *) ptra)" would not be independent of byte order. */
   word = (unsigned char) *ptra; ptra++;
   word = word | ( ((unsigned char) *ptra) << 8 ); ptra++;

   nextch = *ptra;
   if (ofs_inbyte != 0) {
      /* shift nextch right by ofs_inbyte bits */
      /* and shift those bits right into word; */
      word = (word >> ofs_inbyte) | (((unsigned)nextch) << (16-ofs_inbyte));
   }
   return (word & masks[data->nbits]); 
} /* rd_dcode() */

static void init_dtab(struct local_data *data)
{
   data->nbits = 9;
   data->max_code = 512;
   data->free_code = FIRST_FREE;
}

static void wr_dchar(int ch, struct local_data *data)
{
   if (output_offset >= OUTBUFSIZ) {      /* if buffer full */
#ifdef CHECK_BREAK
	check_break();
#endif
      if (BLOCKWRITE (data->out_f, data->out_buf_adr, output_offset) != output_offset)
         fprintf(stderr, "libxmp: write error in lzd:wr_dchar.\n");
      addbfcrc(data->out_buf_adr, output_offset, data);	/* update CRC */
      output_offset = 0;			/* restore empty buffer */
   }
   /* assert(output_offset < OUTBUFSIZ); */
   data->out_buf_adr[output_offset++] = ch;		/* store character */
} /* wr_dchar() */

/* adds a code to table */
static void ad_dcode(struct local_data *data)
{
   /* assert(data->nbits >= 9 && data->nbits <= 13);
   assert(data->free_code <= MAXMAX+1); */
   data->table[data->free_code].z_ch = data->k;		/* save suffix char */
   data->table[data->free_code].next = data->old_code;	/* save prefix code */
   data->free_code++;
   /* assert(data->nbits >= 9 && data->nbits <= 13); */
   if (data->free_code >= data->max_code) {
      if (data->nbits < MAXBITS) {
         data->nbits++;
         /* assert(data->nbits >= 9 && data->nbits <= 13); */
         data->max_code = data->max_code << 1;	/* double data->max_code */
      }
   }
}

int decode_lzd(BLOCKFILE input_f, BLOCKFILE output_f, uint32 *crc_table)
{
   struct local_data *data;

   data = (struct local_data *)calloc(1, sizeof (struct local_data));
   if (data == NULL)
      goto err;

   data->in_f = input_f;                 /* make it avail to other fns */
   data->out_f = output_f;               /* ditto */
   data->nbits = 9;
   data->max_code = 512;
   data->free_code = FIRST_FREE;
   data->crctab = crc_table;

   /*
   Here we allocate a large block of memory for the duration of the program.
   lzc() and lzd() will use half of it each.  Routine getfile() will use all
   of it.  Routine decode() will use the first 8192 bytes of it.  Routine
   encode() will use all of it.

                               fudge/2           fudge/2
                  [______________||________________|]
                    output buffer    input buffer
   */
   data->out_buf_adr = malloc(MEM_BLOCK_SIZE);
   if (data->out_buf_adr == NULL)
      goto err1;

   data->in_buf_adr = data->out_buf_adr + OUT_BUF_SIZE + (FUDGE/2);

   if (BLOCKREAD (data->in_f, data->in_buf_adr, INBUFSIZ) == -1)
      goto err2;

   data->table = (struct tabentry *)malloc((MAXMAX+10) * sizeof(struct tabentry));
   if (data->table == NULL)
      goto err2;

   data->stack = (unsigned *)malloc(sizeof (unsigned) * STACKSIZE + 20);
   if (data->stack == NULL)
      goto err3;

   init_dtab(data);             /* initialize table */

   while (1) {
      data->cur_code = rd_dcode(data);
   goteof: /* special case for CLEAR then Z_EOF, for 0-length files */
      if (data->cur_code == Z_EOF) {
         if (output_offset != 0) {
            if (BLOCKWRITE (data->out_f, data->out_buf_adr, output_offset) != output_offset)
               fprintf(stderr, "libxmp: output error in lzd().\n");
            addbfcrc(data->out_buf_adr, output_offset, data);
         }
   
         free(data->stack);
         free(data->table);
         free(data->out_buf_adr);
         free(data);
         return 0;
      }
   
      /* assert(data->nbits >= 9 && data->nbits <= 13); */
   
      if (data->cur_code == CLEAR) {
         init_dtab(data);
         data->fin_char = data->k = data->old_code = data->cur_code = rd_dcode(data);
   	 if (data->cur_code == Z_EOF) /* special case for 0-length files */
   	    goto goteof;

         wr_dchar(data->k, data);
         continue;
      }
   
      data->in_code = data->cur_code;
      if (data->cur_code >= data->free_code) {  /* if code not in table (k<w>k<w>k) */
         data->cur_code = data->old_code;   /* previous code becomes current */
         push(data->fin_char);
      }
   
      while (data->cur_code > 255) {               /* if code, not character */
         push(data->table[data->cur_code].z_ch);         /* push suffix char */
         data->cur_code = data->table[data->cur_code].next;    /* <w> := <w>.code */
      }
   
      /* assert(data->nbits >= 9 && data->nbits <= 13); */
   
      data->k = data->fin_char = data->cur_code;
      push(data->k);
      while (data->stack_pointer != 0) {
         wr_dchar(pop(), data);
      }
      /* assert(data->nbits >= 9 && data->nbits <= 13); */
      ad_dcode(data);
      data->old_code = data->in_code;
   
      /* assert(data->nbits >= 9 && data->nbits <= 13); */
   }

 err3:
   free(data->table);
 err2:
   free(data->out_buf_adr);
 err1:
   free(data);
 err:
   return -1;

} /* lzd() */

