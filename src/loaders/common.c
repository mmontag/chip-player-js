/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include <ctype.h>
#include <sys/types.h>
#include <stdarg.h>
#ifndef WIN32
#include <dirent.h>
#endif

#include "xmp.h"
#include "common.h"
#include "period.h"
#include "loader.h"

int instrument_init(struct xmp_module *mod)
{
	mod->xxi = calloc(sizeof (struct xmp_instrument), mod->ins);
	if (mod->xxi == NULL)
		return -1;

	if (mod->smp) {
		mod->xxs = calloc (sizeof (struct xmp_sample), mod->smp);
		if (mod->xxs == NULL)
			return -1;
	}

	return 0;
}

int subinstrument_alloc(struct xmp_module *mod, int i, int num)
{
	if (num == 0)
		return 0;

	mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), num);
	if (mod->xxi[i].sub == NULL)
		return -1;

	return 0;
}

int pattern_init(struct xmp_module *mod)
{
	mod->xxt = calloc(sizeof (struct xmp_track *), mod->trk);
	if (mod->xxt == NULL)
		return -1;

	mod->xxp = calloc(sizeof (struct xmp_pattern *), mod->pat);
	if (mod->xxp == NULL)
		return -1;

	return 0;
}

int pattern_alloc(struct xmp_module *mod, int num)
{
	mod->xxp[num] = calloc(1, sizeof (struct xmp_pattern) +
        				sizeof (int) * (mod->chn - 1));
	if (mod->xxp[num] == NULL)
		return -1;

	return 0;
}

int track_alloc(struct xmp_module *mod, int num, int rows)
{
	mod->xxt[num] = calloc (sizeof (struct xmp_track) +
				sizeof (struct xmp_event) * (rows - 1), 1);
	if (mod->xxt[num] == NULL)
		return -1;

	mod->xxt[num]->rows = rows;

	return 0;
}

int tracks_in_pattern_alloc(struct xmp_module *mod, int num)
{
	int i;

	for (i = 0; i < mod->chn; i++) {
		int t = num * mod->chn + i;
		int rows = mod->xxp[num]->rows;

		if (track_alloc(mod, t, rows) < 0)
			return -1;

		mod->xxp[num]->index[i] = t;
	}

	return 0;
}

int pattern_tracks_alloc(struct xmp_module *mod, int num, int rows)
{
	if (pattern_alloc(mod, num) < 0)
		return -1;
	mod->xxp[num]->rows = rows;

	if (tracks_in_pattern_alloc(mod, num) < 0)
		return -1;

	return 0;
}

char *instrument_name(struct xmp_module *mod, int i, uint8 *r, int n)
{
	CLAMP(n, 0, 31);

	return copy_adjust(mod->xxi[i].name, r, n);
}

char *copy_adjust(char *s, uint8 *r, int n)
{
	int i;

	memset(s, 0, n + 1);
	strncpy(s, (char *)r, n);

	for (i = 0; s[i] && i < n; i++) {
		if (!isprint((int)s[i]) || ((uint8)s[i] > 127))
			s[i] = '.';
	}

	while (*s && (s[strlen(s) - 1] == ' '))
		s[strlen(s) - 1] = 0;

	return s;
}

int test_name(uint8 *s, int n)
{
	int i;

	/* ACS_Team2.mod has a backspace in instrument name */
	for (i = 0; i < n; i++) {
		if (s[i] > 0x7f)
			return -1;
		if (s[i] > 0 && s[i] < 32 && s[i] != 0x08)
			return -1;
	}

	return 0;
}

void read_title(HIO_HANDLE *f, char *t, int s)
{
	uint8 buf[XMP_NAME_SIZE];

	if (t == NULL)
		return;

	if (s >= XMP_NAME_SIZE)
		s = XMP_NAME_SIZE -1;

	memset(t, 0, s + 1);

	hio_read(buf, 1, s, f);
	buf[s] = 0;
	copy_adjust(t, buf, s);
}

/*
 * Honor Noisetracker effects:
 *
 *  0 - arpeggio
 *  1 - portamento up
 *  2 - portamento down
 *  3 - Tone-portamento
 *  4 - Vibrato
 *  A - Slide volume
 *  B - Position jump
 *  C - Set volume
 *  D - Pattern break
 *  E - Set filter (keep the led off, please!)
 *  F - Set speed (now up to $1F)
 *
 * Pex Tufvesson's notes from http://www.livet.se/mahoney/:
 *
 * Note that some of the modules will have bugs in the playback with all
 * known PC module players. This is due to that in many demos where I synced
 * events in the demo with the music, I used commands that these newer PC
 * module players erroneously interpret as "newer-version-trackers commands".
 * Which they aren't.
 */
void decode_noisetracker_event(struct xmp_event *event, uint8 *mod_event)
{
	int fxt;

	memset(event, 0, sizeof (struct xmp_event));
	event->note = period_to_note((LSN(mod_event[0]) << 8) + mod_event[1]);
	event->ins = ((MSN(mod_event[0]) << 4) | MSN(mod_event[2]));
	fxt = LSN(mod_event[2]);

	if (fxt <= 0x06 || (fxt >= 0x0a && fxt != 0x0e)) {
		event->fxt = fxt;
		event->fxp = mod_event[3];
	}

	disable_continue_fx(event);
}

void decode_protracker_event(struct xmp_event *event, uint8 *mod_event)
{
	memset(event, 0, sizeof (struct xmp_event));
	event->note = period_to_note((LSN(mod_event[0]) << 8) + mod_event[1]);
	event->ins = ((MSN(mod_event[0]) << 4) | MSN(mod_event[2]));

	if (event->fxt != 0x08) {
		event->fxt = LSN(mod_event[2]);
		event->fxp = mod_event[3];
	}

	disable_continue_fx(event);
}

void disable_continue_fx(struct xmp_event *event)
{
	if (!event->fxp) {
		switch (event->fxt) {
		case 0x05:
			event->fxt = 0x03;
			break;
		case 0x06:
			event->fxt = 0x04;
			break;
		case 0x01:
		case 0x02:
		case 0x0a:
			event->fxt = 0x00;
		}
	}
}

#ifndef WIN32

/* Given a directory, see if file exists there, ignoring case */

int check_filename_case(char *dir, char *name, char *new_name, int size)
{
	int found = 0;
	DIR *dirfd;
	struct dirent *d;

	dirfd = opendir(dir);
	if (dirfd == NULL)
		return 0;
 
	while ((d = readdir(dirfd))) {
		if (!strcasecmp(d->d_name, name)) {
			found = 1;
			break;
		}
	}

	if (found)
		strncpy(new_name, d->d_name, size);

	closedir(dirfd);

	return found;
}

#else

/* FIXME: implement functionality for Win32 */

int check_filename_case(char *dir, char *name, char *new_name, int size)
{
	return 0;
}

#endif

void get_instrument_path(struct module_data *m, char *path, int size)
{
	if (m->instrument_path) {
		strncpy(path, m->instrument_path, size);
	} else if (getenv("XMP_INSTRUMENT_PATH")) {
		strncpy(path, getenv("XMP_INSTRUMENT_PATH"), size);
	} else {
		strncpy(path, ".", size);
	}
}

void set_type(struct module_data *m, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	vsnprintf(m->mod.type, XMP_NAME_SIZE, fmt, ap);
	va_end(ap);
}
