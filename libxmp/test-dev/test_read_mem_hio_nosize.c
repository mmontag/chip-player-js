#include "test.h"
#include "../src/hio.h"

TEST(test_read_mem_hio_nosize)
{
	HIO_HANDLE *h;
	char mem = 0; /* suppress warning */

	h = hio_open_mem(&mem, -1, 0);
	fail_unless(h == NULL, "hio_open");

	h = hio_open_mem(&mem, 0, 0);
	fail_unless(h == NULL, "hio_open");
}
END_TEST
