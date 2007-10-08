
#ifndef __XMP_LOADER_H
#define __XMP_LOADER_H

#include "list.h"

struct xmp_loader_info {
    struct list_head list;
    char *suffix;
    char *tracker;
    int (*loader)();
};

#endif

