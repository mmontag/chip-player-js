#include "test.h"
#include "../src/hio.h"

/* libxmp requires a seekable input file.
 * Attempting to open a non-seekable file should fail. */

TEST(test_read_file_hio_pipe)
{
#ifdef HAVE_PIPE
	HIO_HANDLE *h;
	int fds[2];

	/* Some platforms implement pipe() as a NOP. If this function
	 * fails just let the test pass. */
	if (!pipe(fds)) {
		FILE *r = fdopen(fds[0], "rb");
		FILE *w = fdopen(fds[1], "wb");
		fail_unless(r && w, "fdopen");

		fputs("sdhfjsdhfsd", w);
		fflush(w);

		h = hio_open_file(r);
		fail_unless(h == NULL, "hio_open_file(pipe read)");
		h = hio_open_file(w);
		fail_unless(h == NULL, "hio_open_file(pipe write)");

		fclose(w);
		fclose(r);
	}
#endif
}
END_TEST
