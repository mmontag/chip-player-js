/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include <unistd.h>
#ifdef __native_client__
#include <sys/syslimits.h>
#else
#include <limits.h>
#endif
#include "loader.h"
#include "mod.h"
#include "period.h"
#include "prowizard/prowiz.h"

extern struct list_head *checked_format;

static int pw_test(HIO_HANDLE *, char *, const int);
static int pw_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader pw_loader = {
	"prowizard",
	pw_test,
	pw_load
};

#define BUF_SIZE 0x10000

int pw_test_format(FILE *f, char *t, const int start,
		   struct xmp_test_info *info)
{
	unsigned char *b;
	int extra;
	int s = BUF_SIZE;

	b = calloc(1, BUF_SIZE);
	if (b == NULL)
		return -1;

	fread(b, s, 1, f);

	while ((extra = pw_check(b, s, info)) > 0) {
		unsigned char *buf = realloc(b, s + extra);
		if (buf == NULL) {
			free(b);
			return -1;
		}
		b = buf;
		fread(b + s, extra, 1, f);
		s += extra;
	}

	free(b);

	return extra == 0 ? 0 : -1;
}

static int pw_test(HIO_HANDLE *f, char *t, const int start)
{
	if (HIO_HANDLE_TYPE(f) != HIO_HANDLE_TYPE_FILE)
		return -1;

	return pw_test_format(f->f, t, start, NULL);
}

static int pw_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event;
	struct mod_header mh;
	uint8 mod_event[4];
	char *name;
	char tmp[PATH_MAX];
	int i, j;
	int fd;

	/* Prowizard depacking */

	if (get_temp_dir(tmp, PATH_MAX) < 0)
		return -1;

	strncat(tmp, "xmp_XXXXXX", PATH_MAX);

	if ((fd = mkstemp(tmp)) < 0)
		return -1;

	if (pw_wizardry(fileno(f->f), fd, &name) < 0) {
		close(fd);
		unlink(tmp);
		return -1;
	}

	if ((f = hio_open_fd(fd, "w+b")) == NULL) {
		close(fd);
		unlink(tmp);
		return -1;
	}


	/* Module loading */

	LOAD_INIT();

	hio_read(&mh.name, 20, 1, f);
	for (i = 0; i < 31; i++) {
		hio_read(&mh.ins[i].name, 22, 1, f);
		mh.ins[i].size = hio_read16b(f);
		mh.ins[i].finetune = hio_read8(f);
		mh.ins[i].volume = hio_read8(f);
		mh.ins[i].loop_start = hio_read16b(f);
		mh.ins[i].loop_size = hio_read16b(f);
	}
	mh.len = hio_read8(f);
	mh.restart = hio_read8(f);
	hio_read(&mh.order, 128, 1, f);
	hio_read(&mh.magic, 4, 1, f);

	if (memcmp(mh.magic, "M.K.", 4))
		goto err;
		
	mod->ins = 31;
	mod->smp = mod->ins;
	mod->chn = 4;
	mod->len = mh.len;
	mod->rst = mh.restart;
	memcpy(mod->xxo, mh.order, 128);

	for (i = 0; i < 128; i++) {
		if (mod->chn > 4)
			mod->xxo[i] >>= 1;
		if (mod->xxo[i] > mod->pat)
			mod->pat = mod->xxo[i];
	}

	mod->pat++;

	mod->trk = mod->chn * mod->pat;

	snprintf(mod->name, XMP_NAME_SIZE, "%s", (char *)mh.name);
	snprintf(mod->type, XMP_NAME_SIZE, "%s", name);
	MODULE_INFO();

	if (instrument_init(mod) < 0)
	    return -1;

	for (i = 0; i < mod->ins; i++) {
		if (subinstrument_alloc(mod, i, 1) < 0)
			return -1;

		mod->xxs[i].len = 2 * mh.ins[i].size;
		mod->xxs[i].lps = 2 * mh.ins[i].loop_start;
		mod->xxs[i].lpe = mod->xxs[i].lps + 2 * mh.ins[i].loop_size;
		mod->xxs[i].flg = mh.ins[i].loop_size > 1 ? XMP_SAMPLE_LOOP : 0;
		mod->xxi[i].sub[0].fin = (int8) (mh.ins[i].finetune << 4);
		mod->xxi[i].sub[0].vol = mh.ins[i].volume;
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].sid = i;
		mod->xxi[i].rls = 0xfff;

		if (mod->xxs[i].len > 0)
			mod->xxi[i].nsm = 1;

		instrument_name(mod, i, mh.ins[i].name, 22);

		D_(D_INFO "[%2X] %-22.22s %04x %04x %04x %c V%02x %+d",
			     i, mod->xxi[i].name, mod->xxs[i].len,
			     mod->xxs[i].lps, mod->xxs[i].lpe,
			     mh.ins[i].loop_size > 1 ? 'L' : ' ',
			     mod->xxi[i].sub[0].vol,
			     mod->xxi[i].sub[0].fin >> 4);
	}

	if (pattern_init(mod) < 0)
		return -1;

	/* Load and convert patterns */
	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		if (pattern_tracks_alloc(mod, i, 64) < 0)
			return -1;

		for (j = 0; j < (64 * 4); j++) {
			event = &EVENT(i, j % 4, j / 4);
			hio_read(mod_event, 1, 4, f);
			decode_protracker_event(event, mod_event);
		}
	}

	m->quirk |= QUIRK_MODRNG;

	/* Load samples */

	D_(D_INFO "Stored samples: %d", mod->smp);
	for (i = 0; i < mod->smp; i++) {
		if (load_sample(m, f, 0, &mod->xxs[i], NULL) < 0)
			goto err;
	}

	hio_close(f);
	unlink(tmp);
	return 0;

err:
	hio_close(f);
	unlink(tmp);
	return -1;
}
