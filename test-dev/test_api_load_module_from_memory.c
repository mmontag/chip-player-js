#include <stdio.h>
#include "test.h"

static unsigned char buffer[8192];

TEST(test_api_load_module_from_memory)
{
	xmp_context ctx;
	int ret;
	FILE *f;


	ctx = xmp_create_context();
	f = fopen("data/test.xm", "rb");
	fail_unless(f != NULL, "can't open module");
	fread(buffer, 1, 8192, f);
	fclose(f);

	/* valid file */
	ret = xmp_load_module_from_memory(ctx, buffer);
	fail_unless(ret == 0, "load file");

	
}
END_TEST
