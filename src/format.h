
#ifndef __XMP_LOADER_H
#define __XMP_LOADER_H

#include <stdio.h>
#include "common.h"

struct format_loader {
	const char *name;
	int (*const test)(FILE *, char *, const int);
	int (*const loader)(struct module_data *, FILE *, const int);
};

int format_list(char **[]);

#endif

