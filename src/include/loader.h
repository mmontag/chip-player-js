
#ifndef __XMP_LOADER_H
#define __XMP_LOADER_H

#include <stdio.h>
#include "list.h"

struct xmp_loader_info {
	char *id;
	char *name;
	int (*test)(FILE *, char *);
	int (*loader)(struct xmp_mod_context *, FILE *);
	struct list_head list;
};

#endif

