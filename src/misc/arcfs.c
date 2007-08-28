
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <utime.h>
#include <unistd.h>

#include "xmpi.h"
#include "readrle.h"
#include "readhuff.h"
#include "readlzw.h"


struct archived_file_header_tag {
	unsigned char method;
	unsigned char bits;
	char name[13];
	unsigned long compressed_size;
	unsigned int date, time, crc;
	unsigned long orig_size;
	unsigned long offset;
};


static int read_file_header(FILE *in, struct archived_file_header_tag *hdrp)
{
	int hlen, start, ver;
	int i;

	fseek(in, 8, SEEK_CUR);			/* skip magic */
	hlen = read32l(in) / 36;
	start = read32l(in);
	ver = read32l(in);

	//report("v%d.%02d/", ver / 100, ver % 100);

	read32l(in);
	ver = read32l(in);
	//report("%d... ", ver);

	fseek(in, 68, SEEK_CUR);		/* reserved */

	for (i = 0; i < hlen; i++) {
		int x = read8(in);

		if (x == 0)			/* end? */
			break;

		fread(hdrp->name, 1, 11, in);
		hdrp->name[12] = 0;
		hdrp->orig_size = read32l(in);
		read32l(in);
		read32l(in);
		x = read32l(in);
		hdrp->compressed_size = read32l(in);
		hdrp->offset = read32l(in);

		//printf("name = %s\n", hdrp->name);
		//printf("orig_size = %d\n", hdrp->orig_size);
		//printf("compressed_size = %d\n", hdrp->compressed_size);

		if (x == 1)			/* deleted */
			continue;

		if (hdrp->offset & 0x80000000)		/* directory */
			continue;
		
		hdrp->crc = x >> 16;
		hdrp->bits = (x & 0xff00) >> 8;
		hdrp->offset &= 0x7fffffff;	
		hdrp->offset += start;	

		//printf("crc = 0x%04x\n", hdrp->crc);
		//printf("bits = %d\n", hdrp->bits);
		//printf("offset = %d\n", hdrp->offset);

#if 0
		/* We don't files named 'From?' */
		while (!strcmp(hdr.name, "From?") || *hdr.name == '!') {
			if (!skip_file_data(in,&hdr))
				return -1;
			if (!read_file_header(in, &hdr))
				return -1;
		}
#endif
		break;
	}

	return 1;
}

/* read file data, assuming header has just been read from in
 * and hdrp's data matches it. Caller is responsible for freeing
 * the memory allocated.
 * Returns NULL for file I/O error only; OOM is fatal (doesn't return).
 */
static unsigned char *read_file_data(FILE *in,
				     struct archived_file_header_tag *hdrp)
{
	unsigned char *data;
	int siz = hdrp->compressed_size;

	if ((data = malloc(siz)) == NULL)
		fprintf(stderr, "nomarch: out of memory!\n"), exit(1);

	fseek(in, hdrp->offset, SEEK_SET);
	if (fread(data, 1, siz, in) != siz) {
		free(data);
		data = NULL;
	}

	return data;
}

static int arcfs_extract(FILE *in, FILE *out)
{
	struct archived_file_header_tag hdr;
	/* int done = 0; */
	unsigned char *data, *orig_data;
	int exitval = 0;

	if (!read_file_header(in, &hdr))
		return -1;

	if ((data = read_file_data(in, &hdr)) == NULL) {
		fprintf(stderr, "nomarch: error reading data (hit EOF)\n");
		return -1;
	}

#if 0
out = fopen("data.out", "w");
fwrite(data, 1, hdr.compressed_size, out);
fclose(out);
#endif

	orig_data = convert_lzw_dynamic(data, hdr.bits + 1, 1,
					hdr.compressed_size, hdr.orig_size, 0);


	if (orig_data == NULL) {
		fprintf(stderr, "error extracting file");
		exitval = 1;
	} else {
		char *ptr;

		/* CP/M stuff in particular likes those slashes... */
		while ((ptr = strchr(hdr.name, '/')) != NULL)
			*ptr = '_';

		if (fwrite(orig_data, 1, hdr.orig_size, out) != hdr.orig_size) {
			fprintf(stderr, "error, %s\n", strerror(errno));
			exitval = 1;
		}

		if (orig_data != data)	/* don't free uncompressed stuff twice :-) */
			free(orig_data);
	}

	free(data);

	return exitval;
}

int decrunch_arcfs(FILE * f, FILE * fo)
{
	int ret;

	if (fo == NULL)
		return -1;

	ret = arcfs_extract(f, fo);
	if (ret < 0)
		return -1;

	return 0;
}
