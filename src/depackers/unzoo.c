/*
 * Based on the public domain version by Martin Schoenert
 * Refactored for libxmp by Claudio Matsuoka
 *
 * Copyright (C) 2013 Claudio Matsuoka
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/****************************************************************************
**
*A  unzoo.c                     Tools                        Martin Schoenert
**
*H  @(#)$Id: unzoo.c,v 4.4 2000/05/29 08:56:57 sal Exp $
**
*Y  This file is in the Public Domain.
**
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

int decode_lzd(FILE *, FILE *, uint32 *);

struct description {
	char text[20];	/* "ZOO 2.10 Archive.<ctr>Z"       */
	uint32 magic;	/* magic word 0xfdc4a7dc           */
	uint32 posent;	/* position of first directory ent. */
	uint32 klhvmh;	/* two's complement of posent      */
	uint8 majver;	/* major version needed to extract */
	uint8 minver;	/* minor version needed to extract */
	uint8 type;	/* type of current member (0,1)    */
	uint32 poscmt;	/* position of comment, 0 if none  */
	uint16 sizcmt;	/* length   of comment, 0 if none  */
	uint8 modgen;	/* gens. on, gen. limit            */
	/* the following are not in the archive file and are computed */
	uint32 sizorg;	/* uncompressed size of members    */
	uint32 siznow;	/*   compressed size of members    */
	uint32 number;	/* number of members               */
};

struct entry {
	uint32 magic;	/* magic word 0xfdc4a7dc           */
	uint8 type;	/* type of current member (1)      */
	uint8 method;	/* packing method of member (0..2) */
	uint32 posnxt;	/* position of next member         */
	uint32 posdat;	/* position of data                */
	uint16 datdos;	/* date (in DOS format)            */
	uint16 timdos;	/* time (in DOS format)            */
	uint16 crcdat;	/* crc value of member             */
	uint32 sizorg;	/* uncompressed size of member     */
	uint32 siznow;	/*   compressed size of member     */
	uint8 majver;	/* major version needed to extract */
	uint8 minver;	/* minor version needed to extract */
	uint8 delete;	/* 1 if member is deleted, 0 else  */
	uint8 spared;	/* spare entry to pad entry        */
	uint32 poscmt;	/* position of comment, 0 if none  */
	uint16 sizcmt;	/* length   of comment, 0 if none  */
	char nams[14];	/* short name of member or archive */
	uint16 lvar;	/* length of variable part         */
	uint8 timzon;	/* time zone                       */
	uint16 crcent;	/* crc value of entry              */
	uint8 lnamu;	/* length of long name             */
	uint8 ldiru;	/* length of directory             */
	char namu[256];	/* univ. name of member of archive */
	char diru[256];	/* univ. name of directory         */
	uint16 system;	/* system identifier               */
	uint32 permis;	/* file permissions                */
	uint8 modgen;	/* gens. on, last gen., gen. limit */
	uint16 ver;	/* version number of member        */
};

/* LZW definitions */
#define MAX_LIT		255	/* maximal literal code            */
#define MIN_LEN		3	/* minimal length of match         */
#define MAX_LEN		256	/* maximal length of match         */
#define MAX_CODE	(MAX_LIT+1 + MAX_LEN+1 - MIN_LEN)
#define BITS_CODE	9	/* 2^BITS_CODE > MAX_CODE (+1?)    */
#define MAX_OFF		8192	/* 13 bit sliding directory        */
#define MAX_LOG		13	/* maximal log_2 of offset         */
#define BITS_LOG	4	/* 2^BITS_LOG > MAX_LOG (+1?)      */
#define MAX_PRE		18	/* maximal pre code                */
#define BITS_PRE	5	/* 2^BITS_PRE > MAX_PRE (+1?)      */

struct local_data {
	FILE *in;
	FILE *out;
	struct description desc;
	struct entry entry;
	char buffer[8192];		/* at least MAX_OFF */

	uint32 crc;
	uint32 crctab[256];

	uint16 tree_left[2 * MAX_CODE + 1];   /* tree for codes (upper half) */
	uint16 tree_right[2 * MAX_CODE + 1];  /* and for offsets (lower half) */
	uint16 tabcode[4096];		/* table for fast lookup of codes */
	uint16 tablog[256];		/* table for fast lookup of logs */
	uint16 tabpre[256];		/* table for fast lookup of pres */
	uint8 lencode[MAX_CODE + 1];	/* number of bits used for code */
	uint8 lenlog[MAX_LOG + 1];	/* number of bits used for logs */
	uint8 lenpre[MAX_PRE + 1];	/* number of bits used for pres */
};


/****************************************************************************
**
*F  read_block(<blk>,<len>) . . . . .  read a block of bytes from the archive
*V  desc  . . . . . . . . . . . . . . . . . . . . . . header from the archive
*F  read_description()  . . . . . . . . . .  read the header from the archive
*V  entry . . . . . . . . . . . . . . . . header of a member from the archive
*F  read_entry()  . . . . . . .  read the header of a member from the archive
**
**  'desc' is the description of the archive.
**
**  'read_description' reads the description of the archive that starts at the
**  current position into the structure 'desc'. It should of course only
**  be called at the start of the archive file.
**
**  'entry' is the directory entry of the current member from the archive.
**
**  'read_entry'  reads the directory entry of  a member that starts at the
**  current position into the structure 'Entry'.
*/

static uint32 read_block(char *blk, uint32 len, struct local_data *data)
{
	int i, ch;

	for (i = 0; i < len; i++) {
		if ((ch = read8(data->in)) == EOF)
			return i;
		else
			*blk++ = ch;
	}

	return len;
}


static int read_description(struct local_data *data)
{
	/* read the text at the beginning */
	read_block(data->desc.text, 20L, data);
	data->desc.text[20] = '\0';

	/* try to read the magic words */
	if ((data->desc.magic = read32l(data->in)) != (uint32)0xfdc4a7dcL)
		return 0;

	/* read the old part of the description */
	data->desc.posent = read32l(data->in);
	data->desc.klhvmh = read32l(data->in);
	data->desc.majver = read8(data->in);
	data->desc.minver = read8(data->in);

	/* read the new part of the description if present */
	data->desc.type = (34 < data->desc.posent ? read8(data->in) : 0);
	data->desc.poscmt = (34 < data->desc.posent ? read32l(data->in) : 0);
	data->desc.sizcmt = (34 < data->desc.posent ? read16l(data->in) : 0);
	data->desc.modgen = (34 < data->desc.posent ? read8(data->in) : 0);

	/* initialize the fake entries */
	data->desc.sizorg = 0;
	data->desc.siznow = 0;
	data->desc.number = 0;

	/* indicate success */
	return 1;
}


static int read_entry(struct local_data *data)
{
	uint32 l;	/* 'data->entry.lnamu+data->entry.ldiru' */

	/* try to read the magic words */
	if ((data->entry.magic = read32l(data->in)) != (uint32)0xfdc4a7dcL)
		return 0;

	/* read the fixed part of the directory entry */
	data->entry.type = read8(data->in);
	data->entry.method = read8(data->in);
	data->entry.posnxt = read32l(data->in);
	data->entry.posdat = read32l(data->in);
	data->entry.datdos = read16l(data->in);
	data->entry.timdos = read16l(data->in);
	data->entry.crcdat = read16l(data->in);
	data->entry.sizorg = read32l(data->in);
	data->entry.siznow = read32l(data->in);
	data->entry.majver = read8(data->in);
	data->entry.minver = read8(data->in);
	data->entry.delete = read8(data->in);
	data->entry.spared = read8(data->in);
	data->entry.poscmt = read32l(data->in);
	data->entry.sizcmt = read16l(data->in);
	read_block(data->entry.nams, 13L, data);
	data->entry.nams[13] = '\0';

	/* handle the long name and the directory in the variable part */
	data->entry.lvar = (data->entry.type == 2 ? read16l(data->in) : 0);
	data->entry.timzon = (data->entry.type == 2 ? read8(data->in) : 127);
	data->entry.crcent = (data->entry.type == 2 ? read16l(data->in) : 0);
	data->entry.lnamu = (0 < data->entry.lvar ? read8(data->in) : 0);
	data->entry.ldiru = (1 < data->entry.lvar ? read8(data->in) : 0);
	read_block(data->entry.namu, (uint32)data->entry.lnamu, data);
	data->entry.namu[data->entry.lnamu] = '\0';
	read_block(data->entry.diru, (uint32)data->entry.ldiru, data);
	data->entry.diru[data->entry.ldiru] = '\0';
	l = data->entry.lnamu + data->entry.ldiru;
	data->entry.system = (l + 2 < data->entry.lvar ? read16l(data->in) : 0);
	data->entry.permis = (l + 4 < data->entry.lvar ? read24l(data->in) : 0);
	data->entry.modgen = (l + 7 < data->entry.lvar ? read8(data->in) : 0);
	data->entry.ver = (l + 7 < data->entry.lvar ? read16l(data->in) : 0);

	/* indicate success */
	return 1;
}

/****************************************************************************
**
*F  write_block(<blk>,<len>) . . . . . . .  write a block of bytes to a file
**
**  'write_block' writes <len>  bytes from the  buffer <blk> to the file and
**  returns the number  of bytes actually written,  which is less than <len>
**  only when a write error happened.
*/
static uint32 write_block(char *blk, uint32 len, struct local_data *data)
{
	return fwrite(blk, 1, len, data->out);
}

/****************************************************************************
**
*V  crc . . . . . . . . . . . . . . . . current cyclic redundancy check value
*F  CRC_BYTE(<crc>,<byte>)  . . . . . cyclic redundancy check value of a byte
*F  init_crc()  . . . . . . . . . . . initialize cylic redundancy check table
**
**  'crc'  is used by  the  decoding  functions to  communicate  the computed
**  CRC-16 value to the calling function.
**
**  'CRC_BYTE' returns the new value that one gets by updating the old CRC-16
**  value <crc> with the additional byte  <byte>.  It is  used to compute the
**  ANSI CRC-16 value for  each member of the archive.   They idea is that if
**  not  too many bits  of a member have corrupted,  then  the CRC-16 will be
**  different, and so the corruption can be detected.
**
**  'init_crc' initialize the table that 'CRC_BYTE' uses.   You must call this
**  before using 'CRC_BYTE'.
**
**  The  ANSI CRC-16  value  for a sequence of    bits of lenght  <length> is
**  computed by shifting the bits through the following shift register (where
**  'O' are the latches and '+' denotes logical xor)
**
**                  bit          bit            ...  bit   bit   bit   -->-
**                     <length>     <length>-1          3     2     1     |
**                                                                        V
**      -<-------<---------------------------------------------------<----+
**      |       |                                                   |     ^
**      V       V                                                   V     |
**      ->O-->O-+>O-->O-->O-->O-->O-->O-->O-->O-->O-->O-->O-->O-->O-+>O-->-
**       MSB                                                         LSB
**
**  Mathematically we compute in the polynomial ring $GF(2)[x]$ the remainder
**
**      $$\sum_{i=1}^{i=length}{bit_i x^{length+16-i}} mod crcpol$$
**
**  where  $crcpol = x^{16}  + x^{15}  +  x^2 +  1$.  Then  the  CRC-16 value
**  consists  of the  coefficients   of  the remainder,  with    the constant
**  coefficient being  the most significant bit (MSB)  and the coefficient of
**  $x^{15}$ the least significant bit (LSB).
**
**  Changing  a  single bit will  always cause  the  CRC-16  value to change,
**  because $x^{i} mod crcpol$ is never zero.
**
**  Changing two  bits  will cause the CRC-16   value to change,  unless  the
**  distance between the bits is a multiple  of 32767, which  is the order of
**  $x$ modulo $crcpol = (x+1)(x^{15} + x + 1)$ ($x^{15}+x+1$ is primitive).
**
**  Changing  16 adjacent  bits will always  cause the  CRC value  to change,
**  because $x^{16}$ and $crcpol$ are relatively prime.
**
**  David Schwaderer provided the CRC-16 calculation in PC Tech Journal 4/85.
*/
#define CRC_BYTE(crc,byte)  (((crc)>>8) ^ data->crctab[((crc)^(byte))&0xff])

static int init_crc(struct local_data *data)
{
	uint32 i, k;	/* loop variables                  */

	for (i = 0; i < 256; i++) {
		data->crctab[i] = i;
		for (k = 0; k < 8; k++) {
			data->crctab[i] = (data->crctab[i] >> 1) ^
				((data->crctab[i] & 1) ? 0xa001 : 0);
		}
	}

	return 1;
}

/****************************************************************************
**
*F  DecodeCopy(<size>). . . . . . . . . . . .  extract an uncompressed member
**
**  'DecodeCopy' simply  copies <size> bytes  from the  archive to the output
**  file.
*/
static int DecodeCopy(uint32 size, struct local_data *data)
{
	uint32 siz;	/* size of current block           */
	uint32 crc;	/* CRC-16 value                    */
	uint32 i;	/* loop variable                   */

	/* initialize the crc value */
	crc = 0;

	/* loop until everything has been copied */
	while (0 < size) {

		/* read as many bytes as possible in one go */
		siz = (sizeof(data->buffer) < size ? sizeof(data->buffer) : size);
		if (read_block(data->buffer, siz, data) != siz) {
			/* ErrMsg = "unexpected <eof> in the archive"; */
			return 0;
		}

		/* write them */
		if (write_block(data->buffer, siz, data) != siz) {
			/* ErrMsg = "cannot write output file"; */
			return 0;
		}

		/* compute the crc */
		for (i = 0; i < siz; i++)
			crc = CRC_BYTE(crc, data->buffer[i]);

		/* on to the next block */
		size -= siz;
	}

	/* store the crc and indicate success */
	data->crc = crc;
	return 1;
}

/****************************************************************************
**
*F  DecodeLzd() . . . . . . . . . . . . . . .  extract a LZ compressed member
**
*N  1993/10/21 martin add LZD.
*/
static int DecodeLzd(struct local_data *data)
{
	if (decode_lzd(data->in, data->out, data->crctab) == 0)
		return 1;
	else
		return 0;
}

/****************************************************************************
**
*F  DecodeLzh() . . . . . . . . . . . . . . . extract a LZH compressed member
**
**  'DecodeLzh'  decodes  a LZH  (Lempel-Ziv 77  with dynamic Huffman coding)
**  encoded member from the archive to the output file.
**
**  Each member is encoded as a  series of blocks.  Each  block starts with a
**  16  bit field that contains the  number of codes  in this block <number>.
**  The member is terminated by a block with 0 codes.
**
**  Next each block contains the  description of three Huffman codes,  called
**  pre code, literal/length code, and log code.  The purpose of the pre code
**  is to encode the description of  the literal/length code.  The purpose of
**  the literal/length code and the  log code is   to encode the  appropriate
**  fields in the LZ code.   I am too stupid to  understand the format of the
**  description.
**
**  Then   each block contains  <number>  codewords.  There  are two kinds of
**  codewords, *literals* and *copy instructions*.
**
**  A literal represents a certain byte.  For  the moment imaging the literal
**  as having 9 bits.   The first bit  is zero, the other  8 bits contain the
**  byte.
**
**      +--+----------------+
**      | 0|     <byte>     |
**      +--+----------------+
**
**  When a  literal is  encountered, the byte  <byte> that  it represents  is
**  appended to the output.
**
**  A copy  instruction represents a certain  sequence of bytes that appeared
**  already  earlier in the output.  The  copy instruction  consists of three
**  parts, the length, the offset logarithm, and the offset mantissa.
**
**      +--+----------------+--------+--------------------+
**      | 1|   <length>-3   |  <log> |     <mantissa>     |
**      +--+----------------+--------+--------------------+
**
**  <length>  is  the  length  of the sequence   which  this copy instruction
**  represents.  We store '<length>-3', because <length> is never 0, 1, or 2;
**  such sequences are better represented by 0, 1, or  2 literals.  <log> and
**  <mantissa>  together represent the offset at  which the sequence of bytes
**  already  appeared.  '<log>-1'  is  the number of   bits in the <mantissa>
**  field, and the offset is $2^{<log>-1} + <mantissa>$.  For example
**
**      +--+----------------+--------+----------+
**      | 1|        9       |    6   | 0 1 1 0 1|
**      +--+----------------+--------+----------+
**
**  represents the sequence of 12 bytes that appeared $2^5 + 8 + 4  + 1 = 45$
**  bytes earlier in the output (so those 18 bits of input represent 12 bytes
**  of output).
**
**  When a copy instruction  is encountered, the  sequence of  <length> bytes
**  that appeared   <offset> bytes earlier  in the  output  is again appended
**  (copied) to   the output.   For this  purpose  the last  <max>  bytes are
**  remembered,  where  <max>  is the   maximal  used offset.   In 'zoo' this
**  maximal offset is $2^{13} =  8192$.  The buffer in  which those bytes are
**  remembered is  called   a sliding  window for   reasons  that  should  be
**  obvious.
**
**  To save even  more space the first 9  bits of each code, which  represent
**  the type of code and either the literal value or  the length, are encoded
**  using  a Huffman code  called the literal/length  code.   Also the next 4
**  bits in  copy instructions, which represent  the logarithm of the offset,
**  are encoded using a second Huffman code called the log code.
**
**  Those  codes  are fixed, i.e.,  not  adaptive, but  may  vary between the
**  blocks, i.e., in each block  literals/lengths and logs  may be encoded by
**  different codes.  The codes are described at the beginning of each block.
**
**  Haruhiko Okumura  wrote the  LZH code (originally for his 'ar' archiver).
*/

static int MakeTablLzh(int nchar, uint8 bitlen[], int tablebits, uint16 table[], struct local_data *data)
{
	uint16 count[17], weight[17], start[18], *p;
	unsigned int i, k, len, ch, jutbits, avail, mask;

	for (i = 1; i <= 16; i++)
		count[i] = 0;
	for (i = 0; i < nchar; i++)
		count[bitlen[i]]++;

	start[1] = 0;
	for (i = 1; i <= 16; i++)
		start[i + 1] = start[i] + (count[i] << (16 - i));
	if (start[17] != (uint16)((unsigned)1 << 16))
		return 0;

	jutbits = 16 - tablebits;
	for (i = 1; i <= tablebits; i++) {
		start[i] >>= jutbits;
		weight[i] = (unsigned)1 << (tablebits - i);
	}
	while (i <= 16) {
		weight[i] = (unsigned)1 << (16 - i);
		i++;
	}

	i = start[tablebits + 1] >> jutbits;
	if (i != (uint16)((unsigned)1 << 16)) {
		k = 1 << tablebits;
		while (i != k)
			table[i++] = 0;
	}

	avail = nchar;
	mask = (unsigned)1 << (15 - tablebits);
	for (ch = 0; ch < nchar; ch++) {
		if ((len = bitlen[ch]) == 0)
			continue;
		if (len <= tablebits) {
			for (i = 0; i < weight[len]; i++)
				table[i + start[len]] = ch;
		} else {
			k = start[len];
			p = &table[k >> jutbits];
			i = len - tablebits;
			while (i != 0) {
				if (*p == 0) {
					data->tree_right[avail] = data->tree_left[avail] = 0;
					*p = avail++;
				}
				if (k & mask)
					p = &data->tree_right[*p];
				else
					p = &data->tree_left[*p];
				k <<= 1;
				i--;
			}
			*p = ch;
		}
		start[len] += weight[len];
	}

	/* indicate success                                                    */
	return 1;
}

static int DecodeLzh(struct local_data *data)
{
	uint32 cnt;	/* number of codes in block        */
	uint32 cnt2;	/* number of stuff in pre code     */
	uint32 code;	/* code from the Archive           */
	uint32 len;	/* length of match                 */
	uint32 log;	/* log_2 of offset of match        */
	uint32 off;	/* offset of match                 */
	uint32 pre;	/* pre code                        */
	char *cur;	/* current position in data->buffer */
	char *pos;	/* position of match               */
	char *end;	/* pointer to the end of data->buffer */
	char *stp;	/* stop pointer during copy        */
	uint32 crc;	/* cyclic redundancy check value   */
	uint32 i;	/* loop variable                   */
	uint32 bits;	/* the bits we are looking at      */
	uint32 bitc;	/* number of bits that are valid   */

#define PEEK_BITS(N)            ((bits >> (bitc-(N))) & ((1L<<(N))-1))
#define FLSH_BITS(N)            if ( (bitc -= (N)) < 16 ) { bits  = (bits<<16) + read16b(data->in); bitc += 16; }

	/* initialize bit source, output pointer, and crc */
	bits = 0;
	bitc = 0;
	FLSH_BITS(0);
	cur = data->buffer;
	end = data->buffer + MAX_OFF;
	crc = 0;

	/* loop until all blocks have been read */
	cnt = PEEK_BITS(16);
	FLSH_BITS(16);
	while (cnt != 0) {

		/* read the pre code */
		cnt2 = PEEK_BITS(BITS_PRE);
		FLSH_BITS(BITS_PRE);
		if (cnt2 == 0) {
			pre = PEEK_BITS(BITS_PRE);
			FLSH_BITS(BITS_PRE);
			for (i = 0; i < 256; i++)
				data->tabpre[i] = pre;
			for (i = 0; i <= MAX_PRE; i++)
				data->lenpre[i] = 0;
		} else {
			i = 0;
			while (i < cnt2) {
				len = PEEK_BITS(3);
				FLSH_BITS(3);
				if (len == 7) {
					while (PEEK_BITS(1)) {
						len++;
						FLSH_BITS(1);
					}
					FLSH_BITS(1);
				}
				data->lenpre[i++] = len;
				if (i == 3) {
					len = PEEK_BITS(2);
					FLSH_BITS(2);
					while (0 < len--)
						data->lenpre[i++] = 0;
				}
			}
			while (i <= MAX_PRE)
				data->lenpre[i++] = 0;
			if (!MakeTablLzh(MAX_PRE + 1, data->lenpre, 8, data->tabpre, data)) {
				/* ErrMsg = "pre code description corrupted"; */
				return 0;
			}
		}

		/* read the code (using the pre code) */
		cnt2 = PEEK_BITS(BITS_CODE);
		FLSH_BITS(BITS_CODE);
		if (cnt2 == 0) {
			code = PEEK_BITS(BITS_CODE);
			FLSH_BITS(BITS_CODE);
			for (i = 0; i < 4096; i++)
				data->tabcode[i] = code;
			for (i = 0; i <= MAX_CODE; i++)
				data->lencode[i] = 0;
		} else {
			i = 0;
			while (i < cnt2) {
				len = data->tabpre[PEEK_BITS(8)];
				if (len <= MAX_PRE) {
					FLSH_BITS(data->lenpre[len]);
				} else {
					FLSH_BITS(8);
					do {
						if (PEEK_BITS(1))
							len = data->tree_right[len];
						else
							len = data->tree_left[len];
						FLSH_BITS(1);
					} while (MAX_PRE < len);
				}
				if (len <= 2) {
					if (len == 0) {
						len = 1;
					} else if (len == 1) {
						len = PEEK_BITS(4) + 3;
						FLSH_BITS(4);
					} else {
						len = PEEK_BITS(BITS_CODE) + 20;
						FLSH_BITS(BITS_CODE);
					}
					while (0 < len--)
						data->lencode[i++] = 0;
				} else {
					data->lencode[i++] = len - 2;
				}
			}
			while (i <= MAX_CODE)
				data->lencode[i++] = 0;
			if (!MakeTablLzh(MAX_CODE + 1, data->lencode, 12, data->tabcode, data)) {
				/* ErrMsg = "literal/length code description corrupted"; */
				return 0;
			}
		}

		/* read the log_2 of offsets */
		cnt2 = PEEK_BITS(BITS_LOG);
		FLSH_BITS(BITS_LOG);
		if (cnt2 == 0) {
			log = PEEK_BITS(BITS_LOG);
			FLSH_BITS(BITS_LOG);
			for (i = 0; i < 256; i++)
				data->tablog[i] = log;
			for (i = 0; i <= MAX_LOG; i++)
				data->lenlog[i] = 0;
		} else {
			i = 0;
			while (i < cnt2) {
				len = PEEK_BITS(3);
				FLSH_BITS(3);
				if (len == 7) {
					while (PEEK_BITS(1)) {
						len++;
						FLSH_BITS(1);
					}
					FLSH_BITS(1);
				}
				data->lenlog[i++] = len;
			}
			while (i <= MAX_LOG)
				data->lenlog[i++] = 0;
			if (!MakeTablLzh(MAX_LOG + 1, data->lenlog, 8, data->tablog, data)) {
				/* ErrMsg = "log code description corrupted"; */
				return 0;
			}
		}

		/* read the codes */
		while (0 < cnt--) {
			/* try to decode the code the fast way */
			code = data->tabcode[PEEK_BITS(12)];

			/* if this code needs more than 12 bits look it up in the tree */
			if (code <= MAX_CODE) {
				FLSH_BITS(data->lencode[code]);
			} else {
				FLSH_BITS(12);
				do {
					if (PEEK_BITS(1))
						code = data->tree_right[code];
					else
						code = data->tree_left[code];
					FLSH_BITS(1);
				} while (MAX_CODE < code);
			}

			/* if the code is a literal, stuff it into the buffer */
			if (code <= MAX_LIT) {
				*cur++ = code;
				crc = CRC_BYTE(crc, code);
				if (cur == end) {
					if (write_block(data->buffer, cur - data->buffer, data) != cur - data->buffer) {
						/* ErrMsg = "cannot write output file"; */
						return 0;
					}
					cur = data->buffer;
				}
			}

			/* otherwise compute match length and offset and copy */
			else {
				len = code - (MAX_LIT + 1) + MIN_LEN;

				/* try to decode the log_2 of the offset the fast way */
				log = data->tablog[PEEK_BITS(8)];
				/* if this log_2 needs more than 8 bits look in the tree   */
				if (log <= MAX_LOG) {
					FLSH_BITS(data->lenlog[log]);
				} else {
					FLSH_BITS(8);
					do {
						if (PEEK_BITS(1))
							log = data->tree_right[log];
						else
							log = data->tree_left[log];
						FLSH_BITS(1);
					} while (MAX_LOG < log);
				}

				/* compute the offset */
				if (log == 0) {
					off = 0;
				} else {
					off =
					    ((unsigned)1 << (log - 1)) +
					    PEEK_BITS(log - 1);
					FLSH_BITS(log - 1);
				}

				/* copy the match (this accounts for ~ 50% of the time) */
				pos =
				    data->buffer +
				    (((cur - data->buffer) - off - 1) & (MAX_OFF -
								    1));
				if (cur < end - len && pos < end - len) {
					stp = cur + len;
					do {
						code = *pos++;
						crc = CRC_BYTE(crc, code);
						*cur++ = code;
					} while (cur < stp);
				} else {
					while (0 < len--) {
						code = *pos++;
						crc = CRC_BYTE(crc, code);
						*cur++ = code;
						if (pos == end) {
							pos = data->buffer;
						}
						if (cur == end) {
							if (write_block(data->buffer, cur - data->buffer, data) != cur - data->buffer) {
								/* ErrMsg = "cannot write output file"; */
								return 0;
							}
							cur = data->buffer;
						}
					}
				}
			}
		}

		cnt = PEEK_BITS(16);
		FLSH_BITS(16);
	}

	/* write out the rest of the buffer */
	if (write_block(data->buffer, cur - data->buffer, data) != cur - data->buffer) {
		/* ErrMsg = "cannot write output file"; */
		return 0;
	}

	/* indicate success */
	data->crc = crc;
	return 1;
}


/****************************************************************************
**
*F  ExtrArch(<bim>,<out>,<ovr>,<pre>,<arc>,<filec>,<files>) . extract members
**
**  'ExtrArch' extracts the members  of the archive with  the name <arc> that
**  match one  of the file name  patterns '<files>[0] .. <files>[<filec>-1]'.
**  If <bim> is 0, members with comments starting with '!TEXT!' are extracted
**  as text files and the other members are extracted as  binary files; if it
**  is 1,  all members are extracted  as text files; if  it is 2, all members
**  are  extracted as binary  files. If <out>  is 0, no members are extracted
**  and only tested  for integrity; if it  is 1, the  members are printed  to
**  stdout, i.e., to the screen.  and if it  is 2, the members are extracted.
**  If <ovr> is 0, members will not overwrite  existing files; otherwise they
**  will.  <pre> is a prefix that is prepended to all path names.
*/

int decrunch_zoo(FILE *in, FILE *out)
{
	struct local_data *data;
	int res;

	data = (struct local_data *)calloc(1, sizeof (struct local_data));
	if (data == NULL)
		goto err;

	init_crc(data);

	data->in = in;

	if (!read_description(data))
		goto err1;

	/* loop over the members of the archive */
	data->entry.posnxt = data->desc.posent;
	while (1) {
		if (fseek(data->in, data->entry.posnxt, SEEK_SET) || !read_entry(data))
			goto err1;
		if (!data->entry.posnxt)
			break;
		if (data->entry.delete == 1)
			continue;
		if (exclude_match(data->entry.nams))
			continue;

		/* check that we can decode this file */
		if ((data->entry.method > 2) || (data->entry.majver > 2)
		    || (data->entry.majver == 3 && data->entry.minver > 1))
			continue;

	        /* decode the file */
		if (fseek(data->in, data->entry.posdat, SEEK_SET))
			continue;

		data->out = out;

	        if (data->entry.method == 0)
			res = DecodeCopy(data->entry.siznow, data);
		else if (data->entry.method == 1)
			res = DecodeLzd(data);
		else if (data->entry.method == 2)
			res = DecodeLzh(data);
		else
			goto err1;

		if (res == 0)
			goto err1;
	}

	free(data);
	return 0;

    err1:
	free(data);
    err:
	return -1;
}
