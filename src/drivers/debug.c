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

#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "driver.h"

static void starttimer(void);
static void stoptimer(void);
static void bufdump(void);
static int init(struct xmp_context *ctx);
static void shutdown(struct xmp_context *);

struct xmp_drv_info drv_debug = {
	"debug",		/* driver ID */
	"Driver debugger",	/* driver description */
	NULL,			/* help */
	init,			/* init */
	shutdown,		/* shutdown */
	starttimer,		/* settimer */
	stoptimer,		/* stoptimer */
	bufdump,		/* bufdump */
};

static void starttimer()
{
	printf("** starttimer\n");
}

static void stoptimer()
{
	printf("** stoptimer\n");
}

static void bufdump()
{
}

static int init(struct xmp_context *ctx)
{
	return 0;
}

static void shutdown(struct xmp_context *ctx)
{
	printf("** shutdown\n");
}
