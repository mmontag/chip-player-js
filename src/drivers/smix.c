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

#include "common.h"
#include "driver.h"
#include "mixer.h"

static int init(struct context_data *);
static void shutdown(struct context_data *);

static void dummy()
{
}

struct xmp_drv_info drv_smix = {
	"smix",			/* driver ID */
	"nil softmixer",	/* driver description */
	NULL,			/* help */
	init,			/* init */
	shutdown,		/* shutdown */
	dummy,			/* starttimer */
	dummy,			/* stoptimer */
	dummy,			/* bufdump */
	NULL
};

static int init(struct context_data *ctx)
{
}

static void shutdown(struct context_data *ctx)
{
}
