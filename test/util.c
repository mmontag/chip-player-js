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
	MD5Final(&ctx);

	fclose(f);

	return compare_md5(ctx.digest, digest);

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

