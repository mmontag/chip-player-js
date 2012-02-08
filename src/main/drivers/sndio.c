/*
 * Copyright (c) 2009 Thomas Pfaff <tpfaff@tp76.info>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include <sndio.h>

#include "driver.h"
#include "mixer.h"
#include "common.h"

static struct sio_hdl *hdl;

static int init (struct context_data *);
static void bufdump (struct context_data *, void *, int);
static void shutdown (struct context_data *);
static void dummy (void);

struct xmp_drv_info drv_sndio = {
	 "sndio",		/* driver ID */
	 "OpenBSD sndio",	/* driver description */
	 NULL,		   	/* help */
	 init,		   	/* init */
	 shutdown,		/* shutdown */
	 dummy,			/* starttimer */
	 dummy,			/* stoptimer */
	 bufdump,		/* bufdump */
};

static void
dummy (void)
{
}

static int
init (struct context_data *ctx)
{
	 struct sio_par par, askpar;
	 struct xmp_options *opt = &ctx->o;

	 hdl = sio_open (NULL, SIO_PLAY, 0);
	 if (hdl == NULL) {
		 fprintf (stderr, "%s: failed to open audio device\n",
			 __func__);
		 return XMP_ERR_DINIT;
	 }

	 sio_initpar (&par);
	 par.pchan = opt->outfmt & XMP_FORMAT_MONO ? 1 : 2;
	 par.rate = opt->freq;
	 par.bits = opt->resol;
	 par.sig = opt->resol > 8 ? 1 : 0;
	 par.le = SIO_LE_NATIVE;
	 par.appbufsz = par.rate / 4;

	 askpar = par;
	 if (!sio_setpar (hdl, &par) || !sio_getpar (hdl, &par)) {
		 fprintf (stderr, "%s: failed to set parameters\n", __func__);
		 goto error;
	 }

	 if ((par.bits == 16 && par.le != askpar.le) ||
	     par.bits != askpar.bits ||
	     par.sig != askpar.sig ||
	     par.pchan != askpar.pchan ||
	     ((par.rate * 1000 < askpar.rate * 995) ||
	      (par.rate * 1000 > askpar.rate * 1005))) {
		 fprintf (stderr, "%s: parameters not supported\n", __func__);
		 goto error;
	 }

	 if (!sio_start (hdl)) {
		 fprintf (stderr, "%s: failed to start audio device\n",
			 __func__);
		 goto error;
	 }
	 return 0;

error:
	 sio_close (hdl);
	 return XMP_ERR_DINIT;
}

static void
bufdump (struct context_data *ctx, void *b, int len)
{
	 if (b != NULL)
		 sio_write (hdl, buf, len);
}

static void
shutdown (struct context_data *ctx)
{
	 sio_close (hdl);
	 hdl = NULL;
}
