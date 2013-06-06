#include <stdio.h>
#include "test.h"

static unsigned char buffer[8192];

TEST(test_api_load_module_from_memory)
{
	xmp_context ctx;
	struct xmp_frame_info fi;
	int ret, size;
	FILE *f;


	ctx = xmp_create_context();
	f = fopen("data/test.xm", "rb");
	fail_unless(f != NULL, "can't open module");
	size = fread(buffer, 1, 8192, f);
	fclose(f);

	/* valid file */
	ret = xmp_load_module_from_memory(ctx, buffer, size);
	fail_unless(ret == 0, "load file");

	xmp_get_frame_info(ctx, &fi);
	fail_unless(fi.total_time == 15360, "module duration");
}
END_TEST
