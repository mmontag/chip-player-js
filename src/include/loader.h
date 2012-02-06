
#ifndef __XMP_LOADER_H
#define __XMP_LOADER_H

#include <stdio.h>

struct format_loader {
	char *id;
	char *name;
	int (*test)(FILE *, char *, const int);
	int (*loader)(struct context_data *, FILE *, const int);
};

#endif

