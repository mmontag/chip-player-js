/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <pulse/simple.h>
#include <pulse/error.h>

#include "common.h"
#include "driver.h"
#include "mixer.h"

static pa_simple *s;

static int init(struct context_data *);
static void bufdump(struct context_data *, void *, int);
static void myshutdown(struct context_data *);
static void flush();

static void dummy()
{
}

struct xmp_drv_info drv_pulseaudio = {
	"pulseaudio",		/* driver ID */
	"PulseAudio",		/* driver description */
	NULL,			/* help */
	init,			/* init */
	myshutdown,		/* shutdown */
	dummy,			/* starttimer */
	flush,			/* flush */
	bufdump,		/* bufdump */
};

static int init(struct context_data *ctx)
{
	struct xmp_options *o = &ctx->o;
	pa_sample_spec ss;
	int error;

	ss.format = PA_SAMPLE_S16NE;
	ss.channels = o->outfmt & XMP_FORMAT_MONO ? 1 : 2;
	ss.rate = o->freq;

	s = pa_simple_new(NULL,		/* Use the default server */
		"xmp",			/* Our application's name */
		PA_STREAM_PLAYBACK,
		NULL,			/* Use the default device */
		"Music",		/* Description of our stream */
		&ss,			/* Our sample format */
		NULL,			/* Use default channel map */
		NULL,			/* Use default buffering attributes */
		&error);		/* Ignore error code */

	if (s == 0) {
		fprintf(stderr, "pulseaudio error: %s\n", pa_strerror(error));
		return XMP_ERR_DINIT;
	}

	return 0;
}

static void flush()
{
	int error;

	if (pa_simple_drain(s, &error) < 0) {
		fprintf(stderr, "pulseaudio error: %s\n", pa_strerror(error));
	}
}

static void bufdump(struct context_data *ctx, void *b, int i)
{
	int j, error;

	do {
		if ((j = pa_simple_write(s, b, i, &error)) > 0) {
			i -= j;
			b += j;
		} else
			break;
	} while (i);

	if (j < 0) {
		fprintf(stderr, "pulseaudio error: %s\n", pa_strerror(error));
	}
}

static void myshutdown(struct context_data *ctx)
{
	if (s)
		pa_simple_free(s);
}
