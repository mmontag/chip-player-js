#include <stdio.h>
#include <stdlib.h>
#include "test.h"

#define BUFLEN 16384

int check_md5(char *path, char *digest)
{
	unsigned char buf[BUFLEN];
	MD5_CTX ctx;
	FILE *f;
	int bytes_read;
	int i;

	f = fopen(path, "rb");
	if (f == NULL)
		return -1;

	MD5Init(&ctx);
	while ((bytes_read = fread(buf, 1, BUFLEN, f)) > 0) {
		MD5Update(&ctx, buf, bytes_read);
	}
	MD5Final(&ctx);

	fclose(f);

	for (i = 0; i < 16 && *digest; i++, digest += 2) {
		char hex[3];
		hex[0] = digest[0];
		hex[1] = digest[1];
		hex[2] = 0;

		if (ctx.digest[i] != strtoul(hex, NULL, 16))
			return -1;
	}

	return 0;
}

