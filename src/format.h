#ifndef LIBXMP_FORMAT_H
#define LIBXMP_FORMAT_H

#include <stdio.h>
#include "common.h"
#include "hio.h"

#define MAX_FORMATS 110

struct format_loader {
	const char *name;
	int (*const test)(HANDLE *, char *, const int);
	int (*const loader)(struct module_data *, HANDLE *, const int);
};

char **format_list(void);
int pw_test_format(FILE *, char *, const int, struct xmp_test_info *);

#endif

