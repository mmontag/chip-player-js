/* $VER: unlzx.c 1.0 (22.2.98) */
/* Created: 11.2.98 */

/* LZX Extract in (supposedly) portable C.                                */

/* Modified by Claudio Matsuoka for xmp                                   */

/* Compile with:                                                          */
/* gcc unlzx.c -ounlzx -O2                                                */

/* Thanks to Dan Fraser for decoding the coredumps and helping me track   */
/* down some HIDEOUSLY ANNOYING bugs.                                     */

/* Everything is accessed as unsigned char's to try and avoid problems    */
/* with byte order and alignment. Most of the decrunch functions          */
/* encourage overruns in the buffers to make things as fast as possible.  */
/* All the time is taken up in crc_calc() and decrunch() so they are      */
/* pretty damn optimized. Don't try to understand this program.           */

/* ---------------------------------------------------------------------- */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common.h"
#include "crc32.h"

/* ---------------------------------------------------------------------- */

/* static const unsigned char VERSION[]="$VER: unlzx 1.0 (22.2.98)"; */

/* ---------------------------------------------------------------------- */

struct filename_node {
    struct filename_node *next;
    unsigned int length;
    unsigned int crc;
    char filename[256];
};

/* ---------------------------------------------------------------------- */

struct local_data {
    int mode;

    unsigned char archive_header[31];
    unsigned char header_filename[256];
    unsigned char header_comment[256];

    unsigned int pack_size;
    unsigned int unpack_size;

    unsigned int crc;
    unsigned char pack_mode;

    struct filename_node *filename_list;

    unsigned char read_buffer[16384];	/* have a reasonable sized read buffer */
    unsigned char buffer[258 + 65536 + 258];	/* allow overrun for speed */

    unsigned char *src;
    unsigned char *dest;
    unsigned char *src_end;
    unsigned char *dest_end;

    unsigned int method;
    unsigned int decrunch_length;
    unsigned int last_offset;
    unsigned int control;
    int shift;

    unsigned char offset_len[8];
    unsigned short offset_table[128];
    unsigned char huffman20_len[20];
    unsigned short huffman20_table[96];
    unsigned char literal_len[768];
    unsigned short literal_table[5120];

    unsigned int sum;

    FILE *outfile;
};

/* ---------------------------------------------------------------------- */

static const unsigned char table_one[32] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10,
    11, 11, 12, 12, 13, 13, 14, 14
};

static const unsigned int table_two[32] = {
    0, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512,
    768, 1024,
    1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 24576, 32768, 49152
};

static const unsigned int table_three[16] = {
    0, 1, 3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047, 4095, 8191, 16383, 32767
};

static const unsigned char table_four[34] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
};

/* ---------------------------------------------------------------------- */

/* Build a fast huffman decode table from the symbol bit lengths.         */
/* There is an alternate algorithm which is faster but also more complex. */

static int make_decode_table(int number_symbols, int table_size,
			     unsigned char *length, unsigned short *table)
{
    register unsigned char bit_num = 0;
    register int symbol;
    unsigned int leaf;		/* could be a register */
    unsigned int table_mask, bit_mask, pos, fill, next_symbol, reverse;

    pos = 0;			/* current position in decode table */

    bit_mask = table_mask = 1 << table_size;

    bit_mask >>= 1;		/* don't do the first number */
    bit_num++;

    while (bit_num <= table_size) {
	for (symbol = 0; symbol < number_symbols; symbol++) {
	    if (length[symbol] == bit_num) {
		reverse = pos;	/* reverse the order of the position's bits */
		leaf = 0;
		fill = table_size;

		do {		/* reverse the position */
		    leaf = (leaf << 1) + (reverse & 1);
		    reverse >>= 1;
		} while (--fill);

		if ((pos += bit_mask) > table_mask)
		    return -1;	/* we will overrun the table! abort! */

		fill = bit_mask;
		next_symbol = 1 << bit_num;

		do {
		    table[leaf] = symbol;
		    leaf += next_symbol;
		} while (--fill);
	    }
	}
	bit_mask >>= 1;
	bit_num++;
    }

    if (pos != table_mask) {
	for (symbol = pos; symbol < table_mask; symbol++) {	/* clear the rest of the table */
	    reverse = symbol;	/* reverse the order of the position's bits */
	    leaf = 0;
	    fill = table_size;

	    do {		/* reverse the position */
		leaf = (leaf << 1) + (reverse & 1);
		reverse >>= 1;
	    } while (--fill);

	    table[leaf] = 0;
	}

	next_symbol = table_mask >> 1;
	pos <<= 16;
	table_mask <<= 16;
	bit_mask = 32768;

	while (bit_num <= 16) {
	    for (symbol = 0; symbol < number_symbols; symbol++) {
		if (length[symbol] == bit_num) {
		    reverse = pos >> 16;	/* reverse the order of the position's bits */
		    leaf = 0;
		    fill = table_size;

		    do {	/* reverse the position */
			leaf = (leaf << 1) + (reverse & 1);
			reverse >>= 1;
		    } while (--fill);

		    for (fill = 0; fill < bit_num - table_size; fill++) {
			if (table[leaf] == 0) {
			    table[next_symbol << 1] = 0;
			    table[(next_symbol << 1) + 1] = 0;
			    table[leaf] = next_symbol++;
			}
			leaf = table[leaf] << 1;
			leaf += (pos >> (15 - fill)) & 1;
		    }

		    table[leaf] = symbol;
		    pos += bit_mask;

		    if (pos > table_mask)
			return -1;	/* we will overrun the table! abort! */
		}
	    }
	    bit_mask >>= 1;
	    bit_num++;
	}
    }

    if (pos != table_mask)
	return -1;		/* the table is incomplete! */

    return 0;
}

/* ---------------------------------------------------------------------- */

/* Read and build the decrunch tables. There better be enough data in the */
/* src buffer or it's stuffed. */

static int read_literal_table(struct local_data *data)
{
    register unsigned int control;
    register int shift;
    unsigned int temp;		/* could be a register */
    unsigned int symbol, pos, count, fix, max_symbol;
    int abort = 0;

    control = data->control;
    shift = data->shift;

    if (shift < 0) {		/* fix the control word if necessary */
	shift += 16;
	control += *data->src++ << (8 + shift);
	control += *data->src++ << shift;
    }

    /* read the decrunch method */

    data->method = control & 7;
    control >>= 3;
    shift -= 3;

    if (shift < 0) {
	shift += 16;
	control += *data->src++ << (8 + shift);
	control += *data->src++ << shift;
    }

    /* Read and build the offset huffman table */

    if (!abort && data->method == 3) {
	for (temp = 0; temp < 8; temp++) {
	    data->offset_len[temp] = control & 7;
	    control >>= 3;
	    if ((shift -= 3) < 0) {
		shift += 16;
		control += *data->src++ << (8 + shift);
		control += *data->src++ << shift;
	    }
	}
	abort = make_decode_table(8, 7, data->offset_len, data->offset_table);
    }

    /* read decrunch length */

    if (!abort) {
	data->decrunch_length = (control & 255) << 16;
	control >>= 8;
	shift -= 8;

	if (shift < 0) {
	    shift += 16;
	    control += *data->src++ << (8 + shift);
	    control += *data->src++ << shift;
	}

	data->decrunch_length += (control & 255) << 8;
	control >>= 8;
	shift -= 8;

	if (shift < 0) {
	    shift += 16;
	    control += *data->src++ << (8 + shift);
	    control += *data->src++ << shift;
	}

	data->decrunch_length += (control & 255);
	control >>= 8;
	shift -= 8;

	if (shift < 0) {
	    shift += 16;
	    control += *data->src++ << (8 + shift);
	    control += *data->src++ << shift;
	}
    }

    /* read and build the huffman literal table */

    if (!abort && data->method != 1) {
	pos = 0;
	fix = 1;
	max_symbol = 256;

	do {
	    for (temp = 0; temp < 20; temp++) {
		data->huffman20_len[temp] = control & 15;
		control >>= 4;
		if ((shift -= 4) < 0) {
		    shift += 16;
		    control += *data->src++ << (8 + shift);
		    control += *data->src++ << shift;
		}
	    }
	    abort = make_decode_table(20, 6, data->huffman20_len,
				  data->huffman20_table);

	    if (abort)
		break;		/* argh! table is corrupt! */

	    do {
		if ((symbol = data->huffman20_table[control & 63]) >= 20) {
		    do {	/* symbol is longer than 6 bits */
			symbol = data->huffman20_table[((control >> 6) & 1) +
						  (symbol << 1)];
			if (!shift--) {
			    shift += 16;
			    control += *data->src++ << 24;
			    control += *data->src++ << 16;
			}
			control >>= 1;
		    } while (symbol >= 20);
		    temp = 6;
		} else {
		    temp = data->huffman20_len[symbol];
		}

		control >>= temp;
		if ((shift -= temp) < 0) {
		    shift += 16;
		    control += *data->src++ << (8 + shift);
		    control += *data->src++ << shift;
		}

		switch (symbol) {
		case 17:
		case 18:
		    if (symbol == 17) {
			temp = 4;
			count = 3;
		    } else {	/* symbol == 18 */
			temp = 6 - fix;
			count = 19;
		    }

		    count += (control & table_three[temp]) + fix;
		    control >>= temp;

		    if ((shift -= temp) < 0) {
			shift += 16;
			control += *data->src++ << (8 + shift);
			control += *data->src++ << shift;
		    }

		    while ((pos < max_symbol) && (count--))
			data->literal_len[pos++] = 0;

		    break;
		case 19:
		    count = (control & 1) + 3 + fix;
		    if (!shift--) {
			shift += 16;
			control += *data->src++ << 24;
			control += *data->src++ << 16;
		    }

		    control >>= 1;
		    if ((symbol = data->huffman20_table[control & 63]) >= 20) {
			do {	/* symbol is longer than 6 bits */
			    symbol =
				data->huffman20_table[((control >> 6) & 1) +
						      (symbol << 1)];
			    if (!shift--) {
				shift += 16;
				control += *data->src++ << 24;
				control += *data->src++ << 16;
			    }
			    control >>= 1;
			} while (symbol >= 20);
			temp = 6;
		    } else {
			temp = data->huffman20_len[symbol];
		    }

		    control >>= temp;

		    if ((shift -= temp) < 0) {
			shift += 16;
			control += *data->src++ << (8 + shift);
			control += *data->src++ << shift;
		    }
		    symbol = table_four[data->literal_len[pos] + 17 - symbol];

		    while (pos < max_symbol && count--)
			data->literal_len[pos++] = symbol;

		    break;
		default:
		    symbol = table_four[data->literal_len[pos] + 17 - symbol];
		    data->literal_len[pos++] = symbol;
		    break;
		}
	    } while (pos < max_symbol);

	    fix--;
	    max_symbol += 512;
	} while (max_symbol == 768);

	if (!abort)
	    abort = make_decode_table(768, 12, data->literal_len,
				  data->literal_table);
    }

    data->control = control;
    data->shift = shift;

    return abort;
}

/* ---------------------------------------------------------------------- */

/* Fill up the decrunch buffer. Needs lots of overrun for both data->dest */
/* and src buffers. Most of the time is spent in this routine so it's  */
/* pretty damn optimized. */

static void decrunch(struct local_data *data)
{
    register unsigned int control;
    register int shift;
    unsigned int temp;		/* could be a register */
    unsigned int symbol, count;
    unsigned char *string;

    control = data->control;
    shift = data->shift;

    do {
	if ((symbol = data->literal_table[control & 4095]) >= 768) {
	    control >>= 12;

	    if ((shift -= 12) < 0) {
		shift += 16;
		control += *data->src++ << (8 + shift);
		control += *data->src++ << shift;
	    }
	    do {		/* literal is longer than 12 bits */
		symbol = data->literal_table[(control & 1) + (symbol << 1)];
		if (!shift--) {
		    shift += 16;
		    control += *data->src++ << 24;
		    control += *data->src++ << 16;
		}
		control >>= 1;
	    } while (symbol >= 768);
	} else {
	    temp = data->literal_len[symbol];
	    control >>= temp;

	    if ((shift -= temp) < 0) {
		shift += 16;
		control += *data->src++ << (8 + shift);
		control += *data->src++ << shift;
	    }
	}

	if (symbol < 256) {
	    *data->dest++ = symbol;
	} else {
	    symbol -= 256;
	    count = table_two[temp = symbol & 31];
	    temp = table_one[temp];

	    if ((temp >= 3) && (data->method == 3)) {
		temp -= 3;
		count += ((control & table_three[temp]) << 3);
		control >>= temp;
		if ((shift -= temp) < 0) {
		    shift += 16;
		    control += *data->src++ << (8 + shift);
		    control += *data->src++ << shift;
		}
		count += (temp = data->offset_table[control & 127]);
		temp = data->offset_len[temp];
	    } else {
		count += control & table_three[temp];
		if (!count)
		    count = data->last_offset;
	    }

	    control >>= temp;

	    if ((shift -= temp) < 0) {
		shift += 16;
		control += *data->src++ << (8 + shift);
		control += *data->src++ << shift;
	    }

	    data->last_offset = count;

	    count = table_two[temp = (symbol >> 5) & 15] + 3;
	    temp = table_one[temp];
	    count += (control & table_three[temp]);
	    control >>= temp;

	    shift -= temp;
	    if (shift < 0) {
		shift += 16;
		control += *data->src++ << (8 + shift);
		control += *data->src++ << shift;
	    }

	    string = (data->buffer + data->last_offset < data->dest) ?
			data->dest - data->last_offset :
			data->dest + 65536 - data->last_offset;

	    do {
		*data->dest++ = *string++;
	    } while (--count);
	}
    } while (data->dest < data->dest_end && data->src < data->src_end);

    data->control = control;
    data->shift = shift;
}

/* ---------------------------------------------------------------------- */

/* Trying to understand this function is hazardous. */

static int extract_normal(FILE * in_file, struct local_data *data)
{
    struct filename_node *node;
    FILE *out_file = 0;
    unsigned char *pos;
    unsigned char *temp;
    unsigned int count;
    int abort = 0;

    data->control = 0;	/* initial control word */
    data->shift = -16;
    data->last_offset = 1;
    data->unpack_size = 0;
    data->decrunch_length = 0;

    memset(data->offset_len, 0, 8);
    memset(data->literal_len, 0, 768);

    data->src = data->read_buffer + 16384;
    data->src_end = data->src - 1024;
    pos = data->dest_end = data->dest = data->buffer + 258 + 65536;

    for (node = data->filename_list; (!abort) && node; node = node->next) {
	/*printf("Extracting \"%s\"...", node->filename);
	   fflush(stdout); */

	if (exclude_match(node->filename)) {
	    out_file = NULL;
	} else {
	    out_file = data->outfile;
	}

	data->sum = 0;		/* reset CRC */
	data->unpack_size = node->length;

	while (data->unpack_size > 0) {

	    if (pos == data->dest) {	/* time to fill the buffer? */
/* check if we have enough data and read some if not */
		if (data->src >= data->src_end) {	/* have we exhausted the current read buffer? */
		    temp = data->read_buffer;
		    if ((count = temp - data->src + 16384)) {
			do {	/* copy the remaining overrun to the start of the buffer */
			    *temp++ = *data->src++;
			} while (--count);
		    }
		    data->src = data->read_buffer;
		    count = data->src - temp + 16384;

		    if (data->pack_size < count)
			count = data->pack_size;	/* make sure we don't read too much */

		    if (fread(temp, 1, count, in_file) != count) {
			/* printf("\n");
			if (ferror(in_file))
			    perror("FRead(Data)");
			else
			    fprintf(stderr, "EOF: Data\n"); */
			abort = 1;
			break;	/* fatal error */
		    }
		    data->pack_size -= count;

		    temp += count;
		    if (data->src >= temp)
			break;	/* argh! no more data! */
		}

		/* if(src >= data->src_end) */
		/* check if we need to read the tables */
		if (data->decrunch_length <= 0) {
		    if (read_literal_table(data))
			break;	/* argh! can't make huffman tables! */
		}

                /* unpack some data */
		if (data->dest >= data->buffer + 258 + 65536) {
		    if ((count =
			 data->dest - data->buffer - 65536)) {
			temp = (data->dest =
				data->buffer) + 65536;
			do {	/* copy the overrun to the start of the buffer */
			    *data->dest++ = *temp++;
			} while (--count);
		    }
		    pos = data->dest;
		}
		data->dest_end = data->dest + data->decrunch_length;
		if (data->dest_end > data->buffer + 258 + 65536)
		    data->dest_end = data->buffer + 258 + 65536;
		temp = data->dest;

		decrunch(data);

		data->decrunch_length -= (data->dest - temp);
	    }

            /* calculate amount of data we can use before we need to
             * fill the buffer again
             */
	    count = data->dest - pos;
	    if (count > data->unpack_size)
		count = data->unpack_size;	/* take only what we need */

	    data->sum = crc32_A1(pos, count, data->sum);

	    if (out_file) {	/* Write the data to the file */
		abort = 1;
		if (fwrite(pos, 1, count, out_file) != count) {
#if 0
		    perror("FWrite");	/* argh! write error */
		    fclose(out_file);
		    out_file = 0;
#endif
		}
	    }
	    data->unpack_size -= count;
	    pos += count;
	}

#if 0
	if (out_file) {
	    fclose(out_file);
	    if (!abort)
		printf(" crc %s\n", (node->crc == sum) ? "good" : "bad");
	}
#endif
    }				/* for */

    return (abort);
}

/* ---------------------------------------------------------------------- */

/* This is less complex than extract_normal. Almost decipherable. */

static int extract_store(FILE * in_file, struct local_data *data)
{
    struct filename_node *node;
    FILE *out_file;
    unsigned int count;
    int abort = 0;

    for (node = data->filename_list; (!abort) && node; node = node->next) {
	/*printf("Storing \"%s\"...", node->filename);
	   fflush(stdout); */

	if (exclude_match(node->filename)) {
	    out_file = NULL;
	} else {
	    out_file = data->outfile;
	}

	data->sum = 0;		/* reset CRC */

	data->unpack_size = node->length;
	if (data->unpack_size > data->pack_size)
	    data->unpack_size = data->pack_size;

	while (data->unpack_size > 0) {
	    count = (data->unpack_size > 16384) ? 16384 : data->unpack_size;

	    if (fread(data->read_buffer, 1, count, in_file) != count) {
		/* printf("\n");
		if (ferror(in_file))
		    perror("FRead(Data)");
		else
		    fprintf(stderr, "EOF: Data\n"); */
		abort = 1;
		break;		/* fatal error */
	    }
	    data->pack_size -= count;

	    data->sum = crc32_A1(data->read_buffer, count, data->sum);

	    if (out_file) {	/* Write the data to the file */
		abort = 1;
		if (fwrite(data->read_buffer, 1, count, out_file) != count) {
#if 0
		    perror("FWrite");	/* argh! write error */
		    fclose(out_file);
		    out_file = 0;
#endif
		}
	    }
	    data->unpack_size -= count;
	}
    }				/* for */

    return (abort);
}

/* ---------------------------------------------------------------------- */

/* Easiest of the three. Just print the file(s) we didn't understand. */

static int extract_unknown(FILE * in_file, struct local_data *data)
{
    struct filename_node *node;
    int abort = 0;

    for (node = data->filename_list; node; node = node->next) {
	fprintf(stderr, "unlzx: unknown \"%s\"\n", node->filename);
    }

    return abort;
}

/* ---------------------------------------------------------------------- */

/* Read the archive and build a linked list of names. Merged files is     */
/* always assumed. Will fail if there is no memory for a node. Sigh.      */

static int extract_archive(FILE * in_file, struct local_data *data)
{
    unsigned int temp;
    struct filename_node **filename_next;
    struct filename_node *node;
    struct filename_node *temp_node;
    int actual;
    int abort;
    int result = 1;		/* assume an error */

    data->filename_list = 0;	/* clear the list */
    filename_next = &data->filename_list;

    do {
	abort = 1;		/* assume an error */
	actual = fread(data->archive_header, 1, 31, in_file);
	if (ferror(in_file)) {
	    /* perror("FRead(Archive_Header)"); */
	    continue;
	}

	if (actual == 0) {	/* 0 is normal and means EOF */
	    result = 0;		/* normal termination */
	    continue;
	}

	if (actual != 31) {
	    /* fprintf(stderr, "EOF: Archive_Header\n"); */
	    continue;
	}

	data->sum = 0;		/* reset CRC */
	data->crc = readmem32l(data->archive_header + 26);

	/* Must set the field to 0 before calculating the crc */
	memset(data->archive_header + 26, 0, 4);
	data->sum = crc32_A1(data->archive_header, 31, data->sum);
	temp = data->archive_header[30];	/* filename length */
	actual = fread(data->header_filename, 1, temp, in_file);

	if (ferror(in_file)) {
	    /* perror("FRead(Header_Filename)"); */
	    continue;
	}

	if (actual != temp) {
	    /* fprintf(stderr, "EOF: Header_Filename\n"); */
	    continue;
	}

	data->header_filename[temp] = 0;
	data->sum = crc32_A1(data->header_filename, temp, data->sum);
	temp = data->archive_header[14];	/* comment length */
	actual = fread(data->header_comment, 1, temp, in_file);

	if (ferror(in_file)) {
	    /* perror("FRead(Header_Comment)"); */
	    continue;
	}

	if (actual != temp) {
	    /* fprintf(stderr, "EOF: Header_Comment\n"); */
	    continue;
	}

	data->header_comment[temp] = 0;
	data->sum = crc32_A1(data->header_comment, temp, data->sum);

	if (data->sum != data->crc) {
	    /* fprintf(stderr, "CRC: Archive_Header\n"); */
	    continue;
	}

	data->unpack_size = readmem32l(data->archive_header + 2);
	data->pack_size = readmem32l(data->archive_header + 6);
	data->pack_mode = data->archive_header[11];
	data->crc = readmem32l(data->archive_header + 22);

	/* allocate a filename node */
	node = malloc(sizeof(struct filename_node));
	if (node == NULL) {
	    /* fprintf(stderr, "MAlloc(Filename_node)\n"); */
	    continue;
	}

	*filename_next = node;	/* add this node to the list */
	filename_next = &(node->next);
	node->next = 0;
	node->length = data->unpack_size;
	node->crc = data->crc;
	for (temp = 0; (node->filename[temp] = data->header_filename[temp]);
	     temp++) ;

	if (data->pack_size == 0) {
	    abort = 0;		/* continue */
	    continue;
	}

	switch (data->pack_mode) {
	case 0:		/* store */
	    abort = extract_store(in_file, data);
	    abort = 1;		/* for xmp */
	    break;
	case 2:		/* normal */
	    abort = extract_normal(in_file, data);
	    abort = 1;		/* for xmp */
	    break;
	default:		/* unknown */
	    abort = extract_unknown(in_file, data);
	    break;
	}

	if (abort)
	    break;		/* a read error occured */

	temp_node = data->filename_list;	/* free the list now */
	while ((node = temp_node)) {
	    temp_node = node->next;
	    free(node);
	}
	data->filename_list = 0;	/* clear the list */
	filename_next = &data->filename_list;

	if (fseek(in_file, data->pack_size, SEEK_CUR)) {
	    /* perror("FSeek(Data)"); */
	    break;
	}

    } while (!abort);

    /* free the filename list in case an error occured */
    temp_node = data->filename_list;
    while ((node = temp_node)) {
	temp_node = node->next;
	free(node);
    }

    return result;
}

int decrunch_lzx(FILE * f, FILE * fo)
{
	struct local_data *data;

	if (fo == NULL)
		return -1;

	data = malloc(sizeof(struct local_data));
	if (data == NULL)
		return -1;

	fseek(f, 10, SEEK_CUR);	/* skip header */

	crc32_init_A();
	data->outfile = fo;
	extract_archive(f, data);

	free(data);

	return 0;
}
