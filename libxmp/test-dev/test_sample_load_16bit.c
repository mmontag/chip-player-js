#include "test.h"
#include "../src/loaders/loader.h"

#define SET(x,y,z,w) do { \
  s.len = (x); s.lps = (y); s.lpe = (z); s.flg = (w); s.data = NULL; \
} while (0)

#define CLEAR() do { libxmp_free_sample(&s); } while (0)

TEST(test_sample_load_16bit)
{
	struct xmp_sample s;
	HIO_HANDLE *f;
	short buffer[101];
	int i;
	struct module_data m;

	memset(&m, 0, sizeof(struct module_data));

	f = hio_open("data/sample-16bit.raw", "rb");
	fail_unless(f != NULL, "can't open sample file");

	/* read little-endian sample to native-endian buffer */
	for (i = 0; i < 101; i++) {
		buffer[i] = hio_read16l(f);
	}

	/* load zero-length sample */
	SET(0, 0, 101, XMP_SAMPLE_16BIT | XMP_SAMPLE_LOOP);
	libxmp_load_sample(&m, NULL, 0, &s, NULL);

	/* load sample with invalid loop */
	SET(101, 150, 180, XMP_SAMPLE_16BIT | XMP_SAMPLE_LOOP | XMP_SAMPLE_LOOP_BIDIR);
	hio_seek(f, 0, SEEK_SET);
	libxmp_load_sample(&m, f, 0, &s, NULL);
	fail_unless(s.data != NULL, "didn't allocate sample data");
	fail_unless(s.lps == 0, "didn't fix invalid loop start");
	fail_unless(s.lpe == 0, "didn't fix invalid loop end");
	fail_unless(s.flg == XMP_SAMPLE_16BIT, "didn't reset loop flags");
	CLEAR();

	/* load sample with invalid loop */
	SET(101, 50, 40, XMP_SAMPLE_16BIT | XMP_SAMPLE_LOOP | XMP_SAMPLE_LOOP_BIDIR);
	hio_seek(f, 0, SEEK_SET);
	libxmp_load_sample(&m, f, 0, &s, NULL);
	fail_unless(s.data != NULL, "didn't allocate sample data");
	fail_unless(s.lps == 0, "didn't fix invalid loop start");
	fail_unless(s.lpe == 0, "didn't fix invalid loop end");
	fail_unless(s.flg == XMP_SAMPLE_16BIT, "didn't reset loop flags");
	CLEAR();

	/* load sample from file */
	SET(101, 0, 102, XMP_SAMPLE_16BIT);
	hio_seek(f, 0, SEEK_SET);
	libxmp_load_sample(&m, f, 0, &s, NULL);
	fail_unless(s.data != NULL, "didn't allocate sample data");
	fail_unless(s.lpe == 101, "didn't fix invalid loop end");
	fail_unless(memcmp(s.data, buffer, 202) == 0, "sample data error");
	fail_unless(s.data[-2]  == s.data[0],   "sample prologue error");
	fail_unless(s.data[-1]  == s.data[1],   "sample prologue error");
	fail_unless(s.data[202] == s.data[200], "sample epilogue error");
	fail_unless(s.data[203] == s.data[201], "sample epilogue error");
	fail_unless(s.data[204] == s.data[202], "sample epilogue error");
	fail_unless(s.data[205] == s.data[203], "sample epilogue error");
	CLEAR();

	/* load sample from file w/ loop */
	SET(101, 20, 80, XMP_SAMPLE_16BIT | XMP_SAMPLE_LOOP);
	hio_seek(f, 0, SEEK_SET);
	libxmp_load_sample(&m, f, 0, &s, NULL);
	fail_unless(s.data != NULL, "didn't allocate sample data");
	fail_unless(memcmp(s.data, buffer, 202) == 0, "sample data error");
	fail_unless(s.data[-2]  == s.data[0],   "sample prologue error");
	fail_unless(s.data[-1]  == s.data[1],   "sample prologue error");
	fail_unless(s.data[202] == s.data[200], "sample epilogue error");
	fail_unless(s.data[203] == s.data[201], "sample epilogue error");
	fail_unless(s.data[204] == s.data[202], "sample epilogue error");
	fail_unless(s.data[205] == s.data[203], "sample epilogue error");
	CLEAR();

	hio_close(f);
}
END_TEST
