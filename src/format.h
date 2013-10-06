#ifndef LIBXMP_FORMAT_H
#define LIBXMP_FORMAT_H

#include <stdio.h>
#include "common.h"
#include "hio.h"

#define NUM_FORMATS 58
#define NUM_PW_FORMATS 40

struct format_loader {
	const char *name;
	int (*const test)(HIO_HANDLE *, char *, const int);
	int (*const loader)(struct module_data *, HIO_HANDLE *, const int);
};

char **format_list(void);
int pw_test_format(FILE *, char *, const int, struct xmp_test_info *);

#endif

