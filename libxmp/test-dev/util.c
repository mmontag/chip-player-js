#include <math.h>
#include "../src/hio.h"
#include "test.h"

#define BUFLEN 16384

void read_file_to_memory(const char *filename, void **_buffer, long *_size)
{
	HIO_HANDLE *f = hio_open(filename, "rb");
	void *buffer = NULL;
	long size;

	*_buffer = NULL;
	*_size = 0;

	if (f == NULL)
		return;

	size = hio_size(f);
	if (size <= 0) {
		hio_close(f);
		return;
	}

	buffer = malloc(size);
	if (buffer == NULL) {
		hio_close(f);
		return;
	}

	if (hio_read(buffer, 1, size, f) != size) {
		hio_close(f);
		free(buffer);
		return;
	}

	hio_close(f);
	*_buffer = buffer;
	*_size = size;
}

int check_randomness(int *array, int size, double sdev)
{
	int i;
	double avg = 0.0;
	double dev = 0.0;

	for (i = 0; i < size; i++) {
		avg += array[i];
	}
	avg /= size;

	for (i = 0; i < size; i++) {
		dev += pow(avg - array[i], 2);
	}

	dev = sqrt(dev / size);

	return dev > sdev;
}

int compare_md5(const unsigned char *d, const char *digest)
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

int check_md5(const char *path, const char *digest)
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

	if (!fgets(line, size, f)) {
		line[0] = '\0';
		return 0;
	}
	pos = strlen(line);

	if (pos > 0 && line[pos - 1] == '\n')
		line[--pos] = 0;

	return pos;
}

static void check_envelope(struct xmp_envelope *env, char *line, FILE *f)
{
	int i, x;
	char *s;

	/* read envelope parameters */
	read_line(line, 1024, f);
	x = strtoul(line, &s, 0);
	fail_unless(x == env->flg, "envelope flags");
	x = strtoul(s, &s, 0);
	fail_unless(x == env->npt, "envelope number of points");
	x = strtoul(s, &s, 0);
	fail_unless(x == env->scl, "envelope scaling");
	x = strtoul(s, &s, 0);
	fail_unless(x == env->sus, "envelope sustain start");
	x = strtoul(s, &s, 0);
	fail_unless(x == env->sue, "envelope sustain end");
	x = strtoul(s, &s, 0);
	fail_unless(x == env->lps, "envelope loop start");
	x = strtoul(s, &s, 0);
	fail_unless(x == env->lpe, "envelope loop end");

	if (env->npt > 0) {
		read_line(line, 1024, f);
		s = line;
		for (i = 0; i < env->npt * 2; i++) {
			x = strtoul(s, &s, 0);
			fail_unless(x == env->data[i], "envelope point");
		}
	}

}

int compare_module(struct xmp_module *mod, FILE *f)
{
	char line[1024];
	char *s;
	int i, j, x;

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

	/* Check orders */
	if (mod->len > 0) {
		read_line(line, 1024, f);
		s = line;
		for (i = 0; i < mod->len; i++) {
			x = strtoul(s, &s, 0);
			fail_unless(x == mod->xxo[i], "orders");
		}
	}

	/* Check instruments */
	for (i = 0; i < mod->ins; i++) {
		struct xmp_instrument *xxi = &mod->xxi[i];

		read_line(line, 1024, f);
		x = strtoul(line, &s, 0);
		fail_unless(x == xxi->vol, "instrument volume");
		x = strtoul(s, &s, 0);
		fail_unless(x == xxi->nsm, "number of subinstruments");
		x = strtoul(s, &s, 0);
		fail_unless(x == xxi->rls, "instrument release");
		x = strncmp(++s, xxi->name, 32);
		fail_unless(x == 0, "instrument name");

		/* check envelopes */
		check_envelope(&xxi->aei, line, f);
		check_envelope(&xxi->fei, line, f);
		check_envelope(&xxi->pei, line, f);

		/* check mapping */
		read_line(line, 1024, f);
		s = line;
		for (j = 0; j < XMP_MAX_KEYS; j++) {
			x = strtoul(s, &s, 0);
			fail_unless(x == xxi->map[j].ins, "instrument map");
		}
		read_line(line, 1024, f);
		s = line;
		for (j = 0; j < XMP_MAX_KEYS; j++) {
			x = strtoul(s, &s, 0);
			fail_unless(x == xxi->map[j].xpo, "transpose map");
		}

		/* check subinstruments */
		for (j = 0; j < xxi->nsm; j++) {
			struct xmp_subinstrument *sub = &xxi->sub[j];

			read_line(line, 1024, f);
			x = strtoul(line, &s, 0);
			fail_unless(x == sub->vol, "subinst volume");
			x = strtoul(s, &s, 0);
			fail_unless(x == sub->gvl, "subinst gl volume");
			x = strtoul(s, &s, 0);
			fail_unless(x == sub->pan, "subinst pan");
			x = strtoul(s, &s, 0);
			fail_unless(x == sub->xpo, "subinst transpose");
			x = strtoul(s, &s, 0);
			fail_unless(x == sub->fin, "subinst finetune");
			x = strtoul(s, &s, 0);
			fail_unless(x == sub->vwf, "subinst vibr wf");
			x = strtoul(s, &s, 0);
			fail_unless(x == sub->vde, "subinst vibr depth");
			x = strtoul(s, &s, 0);
			fail_unless(x == sub->vra, "subinst vibr rate");
			x = strtoul(s, &s, 0);
			fail_unless(x == sub->vsw, "subinst vibr sweep");
			x = strtoul(s, &s, 0);
			fail_unless(x == sub->rvv, "subinst vol var");
			x = strtoul(s, &s, 0);
			fail_unless(x == sub->sid, "subinst sample nr");
			x = strtoul(s, &s, 0);
			fail_unless(x == sub->nna, "subinst NNA");
			x = strtoul(s, &s, 0);
			fail_unless(x == sub->dct, "subinst DCT");
			x = strtoul(s, &s, 0);
			fail_unless(x == sub->dca, "subinst DCA");
			x = strtoul(s, &s, 0);
			fail_unless(x == sub->ifc, "subinst cutoff");
			x = strtoul(s, &s, 0);
			fail_unless(x == sub->ifr, "subinst resonance");
		}
	}

	/* Check patterns */
	for (i = 0; i < mod->pat; i++) {
		struct xmp_pattern *xxp = mod->xxp[i];

		read_line(line, 1024, f);
		x = strtoul(line, &s, 0);
		fail_unless(x == xxp->rows, "pattern rows");

		for (j = 0; j < mod->chn; j++) {
			x = strtoul(s, &s, 0);
			fail_unless(x == xxp->index[j], "pattern index");
		}

	}

	/* Check tracks */
	for (i = 0; i < mod->trk; i++) {
		struct xmp_track *xxt = mod->xxt[i];
		unsigned char d[16];
		MD5_CTX ctx;

		read_line(line, 1024, f);
		x = strtoul(line, &s, 0);
		fail_unless(x == xxt->rows, "track rows");

		MD5Init(&ctx);
		MD5Update(&ctx, (const unsigned char *)xxt->event,
				xxt->rows * sizeof (struct xmp_event));
		MD5Final(d, &ctx);

		fail_unless(compare_md5(d, ++s) == 0, "track data");
	}

	/* Check samples */
	for (i = 0; i < mod->smp; i++) {
		struct xmp_sample *xxs = &mod->xxs[i];
		unsigned char d[16];
		MD5_CTX ctx;
		int len = xxs->len;

		if (xxs->flg & XMP_SAMPLE_16BIT) {
			len *= 2;
			/* Normalize data to little endian for the hash. */
			if (is_big_endian()) {
				convert_endian(xxs->data, xxs->len);
			}
		}

		read_line(line, 1024, f);
		x = strtoul(line, &s, 0);
		fail_unless(x == xxs->len, "sample length");
		x = strtoul(s, &s, 0);
		fail_unless(x == xxs->lps, "sample loop start");
		x = strtoul(s, &s, 0);
		fail_unless(x == xxs->lpe, "sample loop end");
		x = strtoul(s, &s, 0);
		fail_unless(x == xxs->flg, "sample flags");

		s++;
		if (len > 0 && xxs->data != NULL) {
			MD5Init(&ctx);
			MD5Update(&ctx, xxs->data, len);
			MD5Final(d, &ctx);
			fail_unless(compare_md5(d, s) == 0, "sample data");
		}

		s += 32;
		fail_unless(strcmp(xxs->name, ++s) == 0, "sample name");
	}

	/* Check channels */
	for (i = 0; i < mod->chn; i++) {
		struct xmp_channel *xxc = &mod->xxc[i];

		read_line(line, 1024, f);
		x = strtoul(line, &s, 0);
		fail_unless(x == xxc->pan, "channel pan");
		x = strtoul(s, &s, 0);
		fail_unless(x == xxc->vol, "channel volume");
		x = strtoul(s, &s, 0);
		fail_unless(x == xxc->flg, "channel flags");
	}

	return 0;
}

static void dump_envelope(struct xmp_envelope *env, FILE *f)
{
	int i;

	/* dump envelope parameters */
	fprintf(f, "%d %d %d %d %d %d %d\n",
		env->flg, /* envelope flags */
		env->npt, /* envelope number of points */
		env->scl, /* envelope scaling */
		env->sus, /* envelope sustain start */
		env->sue, /* envelope sustain end */
		env->lps, /* envelope loop start */
		env->lpe  /* envelope loop end */
	);

	if (env->npt > 0) {
		for (i = 0; i < env->npt * 2; i++) {
			fprintf(f, "%s%d", i ? " " : "", env->data[i]);
		}
		fprintf(f, "\n");
	}
}

void dump_module(struct xmp_module *mod, FILE *f)
{
	int i, j;

	/* Write title and format */
	fprintf(f, "%s\n", mod->name);
	fprintf(f, "%s\n", mod->type);

	/* Write module attributes */
	fprintf(f, "%u %u %u %u %u %u %u %u %u %u\n",
		mod->pat, /* number of patterns */
		mod->trk, /* number of tracks */
		mod->chn, /* number of channels */
		mod->ins, /* number of instruments */
		mod->smp, /* number of samples */
		mod->spd, /* initial speed */
		mod->bpm, /* initial tempo */
		mod->len, /* module length */
		mod->rst, /* restart position */
		mod->gvl  /* global volume */
	);

	/* Write orders */
	for (i = 0; i < mod->len; i++) {
		fprintf(f, "%s%u", i ? " " : "", mod->xxo[i]);
	}
	fprintf(f, "\n");

	/* Write instruments */
	for (i = 0; i < mod->ins; i++) {
		struct xmp_instrument *xxi = &mod->xxi[i];
		fprintf(f, "%u %u %u %s\n",
			xxi->vol, /* instrument volume */
			xxi->nsm, /* number of subinstruments */
			xxi->rls, /* instrument release */
			xxi->name /* instrument name */
		);

		/* Write envelopes */
		dump_envelope(&xxi->aei, f);
		dump_envelope(&xxi->fei, f);
		dump_envelope(&xxi->pei, f);

		/* Write mapping */
		for (j = 0; j < XMP_MAX_KEYS; j++) {
			fprintf(f, "%s%d", j ? " " : "", xxi->map[j].ins);
		}
		fprintf(f, "\n");
		for (j = 0; j < XMP_MAX_KEYS; j++) {
			fprintf(f, "%s%d", j ? " " : "", xxi->map[j].xpo);
		}
		fprintf(f, "\n");

		/* Write subinstruments */
		for (j = 0; j < xxi->nsm; j++) {
			struct xmp_subinstrument *sub = &xxi->sub[j];

			fprintf(f, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
				sub->vol, /* subinst volume */
				sub->gvl, /* subinst gl volume */
				sub->pan, /* subinst pan */
				sub->xpo, /* subinst transpose */
				sub->fin, /* subinst finetune */
				sub->vwf, /* subinst vibr wf */
				sub->vde, /* subinst vibr depth */
				sub->vra, /* subinst vibr rate */
				sub->vsw, /* subinst vibr sweep */
				sub->rvv, /* subinst vol var */
				sub->sid, /* subinst sample nr */
				sub->nna, /* subinst NNA */
				sub->dct, /* subinst DCT */
				sub->dca, /* subinst DCA */
				sub->ifc, /* subinst cutoff */
				sub->ifr  /* subinst resonance */
			);
		}
	}

	/* Write patterns */
	for (i = 0; i < mod->pat; i++) {
		struct xmp_pattern *xxp = mod->xxp[i];

		fprintf(f, "%u", xxp->rows); /* pattern rows */
		for (j = 0; j  < mod->chn; j++) {
			fprintf(f, " %u", xxp->index[j]); /* pattern index */
		}
		fprintf(f, "\n");
	}

	/* Write tracks */
	for (i = 0; i < mod->trk; i++) {
		struct xmp_track *xxt = mod->xxt[i];
		unsigned char d[MD5_DIGEST_LENGTH];
		MD5_CTX ctx;

		MD5Init(&ctx);
		MD5Update(&ctx, (const unsigned char *)xxt->event,
				xxt->rows * sizeof (struct xmp_event));
		MD5Final(d, &ctx);

		fprintf(f, "%u ", xxt->rows); /* track rows */

		for (j = 0; j < MD5_DIGEST_LENGTH; j++) {
			fprintf(f, "%02x", d[j]); /* track data */
		}
		fprintf(f, "\n");
	}

	/* Write samples */
	for (i = 0; i < mod->smp; i++) {
		struct xmp_sample *xxs = &mod->xxs[i];
		unsigned char d[MD5_DIGEST_LENGTH];
		MD5_CTX ctx;
		int len = xxs->len;

		if (xxs->flg & XMP_SAMPLE_16BIT) {
			len *= 2;
			/* Normalize data to little endian for the hash. */
			if (is_big_endian()) {
				convert_endian(xxs->data, xxs->len);
			}
		}

		fprintf(f, "%d %d %d %d",
			xxs->len, /* sample length */
			xxs->lps, /* sample loop start */
			xxs->lpe, /* sample loop end */
			xxs->flg  /* sample flags */
		);

		MD5Init(&ctx);
		if (len > 0 && xxs->data != NULL) {
			MD5Update(&ctx, xxs->data, len);
		} else if (len > 0) {
			/* The data file for fracture.stm had this hash for empty samples... */
			MD5Update(&ctx, (unsigned char *)"1", 1);
		}
		MD5Final(d, &ctx);
		fprintf(f, " ");
		for (j = 0; j < MD5_DIGEST_LENGTH; j++) {
			fprintf(f, "%02x", d[j]); /* sample data */
		}
		fprintf(f, " %s\n", xxs->name); /* sample name */
	}

	/* Write channels */
	for (i = 0; i < mod->chn; i++) {
		struct xmp_channel *xxc = &mod->xxc[i];

		fprintf(f, "%u %u %u\n",
			xxc->pan, /* channel pan */
			xxc->vol, /* channel volume */
			xxc->flg  /* channel flags */
		);
	}
}

void compare_playback(const char *filename, const struct playback_sequence *sequence,
		      int rate, int flags, int interp)
{
	xmp_context opaque;
	int count, ret;

	opaque = xmp_create_context();
	fail_unless(opaque, "create context");

	ret = xmp_load_module(opaque, filename);
	fail_unless(ret == 0, "module load");

	ret = xmp_start_player(opaque, rate, flags);
	fail_unless(ret == 0, "start player");

	ret = xmp_set_player(opaque, XMP_PLAYER_INTERP, interp);
	fail_unless(ret == 0, "set interp");

	while (sequence->action != PLAY_END) {
		switch (sequence->action) {
		case PLAY_END: /* silence warning */
			break;

		case PLAY_FRAMES:
			for (count = sequence->value; count > 0; count--)
				ret = xmp_play_frame(opaque);

			fail_unless(ret == sequence->result, "play frames");
			break;
		}
		sequence++;
	}

	xmp_free_context(opaque);
}
