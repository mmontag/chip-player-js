#include <stdio.h>
#include <stdlib.h>
#include "test.h"

#define BUFLEN 16384

int compare_md5(unsigned char *d, char *digest)
{
	int i;

	/*for (i = 0; i < 16 ; i++)
		printf("%02x", d[i]);
	printf("\n");*/

	for (i = 0; i < 16 && *digest; i++, digest += 2) {
		char hex[3];
		hex[0] = digest[0];
		hex[1] = digest[1];
		hex[2] = 0;

		if (d[i] != strtoul(hex, NULL, 16))
			return -1;
	}

	return 0;
}

int check_md5(char *path, char *digest)
{
	unsigned char buf[BUFLEN];
	unsigned char d[16];
	MD5_CTX ctx;
	FILE *f;
	int bytes_read;

	f = fopen(path, "rb");
	if (f == NULL)
		return -1;

	MD5Init(&ctx);
	while ((bytes_read = fread(buf, 1, BUFLEN, f)) > 0) {
		MD5Update(&ctx, buf, bytes_read);
	}
	MD5Final(d, &ctx);

	fclose(f);

	return compare_md5(d, digest);
}

int map_channel(struct player_data *p, int chn)
{
	int voc;

	if ((uint32)chn >= p->virt.virt_channels)
		return -1;

	voc = p->virt.virt_channel[chn].map;

	if ((uint32)voc >= p->virt.maxvoc)
		return -1;

	return voc;
}

/* Convert little-endian 16 bit samples to big-endian */
void convert_endian(unsigned char *p, int l)
{
        uint8 b;
        int i;

        for (i = 0; i < l; i++) {
                b = p[0];
                p[0] = p[1];
                p[1] = b;
                p += 2;
        }
}

static int read_line(char *line, int size, FILE *f)
{
	int pos;

	fgets(line, size, f);
	pos = strlen(line);

	if (pos > 0 && line[pos - 1] == '\n')
		line[--pos] = 0;

	return pos;
}

int compare_module(struct xmp_module *mod, FILE *f)
{
	char line[1024];
	char *s;
	int x;

	/* Check title and format */
	read_line(line, 1024, f);
	fail_unless(!strcmp(line, mod->name), "module name");
	read_line(line, 1024, f);
	fail_unless(!strcmp(line, mod->type), "module type");

	/* Check module attributes */
	read_line(line, 1024, f);
	x = strtoul(line, &s, 0);
	fail_unless(x == mod->pat, "number of patterns");
	x = strtoul(s, &s, 0);
	fail_unless(x == mod->trk, "number of tracks");
	x = strtoul(s, &s, 0);
	fail_unless(x == mod->chn, "number of channels");
	x = strtoul(s, &s, 0);
	fail_unless(x == mod->ins, "number of instruments");
	x = strtoul(s, &s, 0);
	fail_unless(x == mod->smp, "number of samples");
	x = strtoul(s, &s, 0);
	fail_unless(x == mod->spd, "initial speed");
	x = strtoul(s, &s, 0);
	fail_unless(x == mod->bpm, "initial tempo");
	x = strtoul(s, &s, 0);
	fail_unless(x == mod->len, "module length");
	x = strtoul(s, &s, 0);
	fail_unless(x == mod->rst, "restart position");
	x = strtoul(s, &s, 0);
	fail_unless(x == mod->gvl, "global volume");

	return 0;
}
