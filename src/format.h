
#ifndef __XMP_LOADER_H
#define __XMP_LOADER_H

#include <stdio.h>
#include "common.h"

struct format_loader {
	char *id;
	char *name;
	int (*test)(FILE *, char *, const int);
	int (*loader)(struct module_data *, FILE *, const int);
};

int format_init(void);
void format_deinit(void);
void format_list(char ***);

#endif

