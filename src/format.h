#ifndef XMP_FORMAT_H
#define XMP_FORMAT_H

#include <stdio.h>
#include "common.h"

#define MAX_FORMATS 110

struct format_loader {
	const char *name;
	int (*const test)(FILE *, char *, const int);
	int (*const loader)(struct module_data *, FILE *, const int);
};

char **format_list(void);
int pw_test_format(FILE *, char *, const int, struct xmp_test_info *);

#endif

