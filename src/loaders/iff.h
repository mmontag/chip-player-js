#ifndef __IFF_H
#define __IFF_H

#include "list.h"

#define IFF_NOBUFFER 0x0001

#define IFF_LITTLE_ENDIAN	0x01
#define IFF_FULL_CHUNK_SIZE	0x02
#define IFF_CHUNK_ALIGN2	0x04
#define IFF_CHUNK_ALIGN4	0x08
#define IFF_SKIP_EMBEDDED	0x10
#define IFF_CHUNK_TRUNC4	0x20

typedef void *iff_handle;

struct iff_header {
	char form[4];		/* FORM */
	int len;		/* File length */
	char id[4];		/* IFF type identifier */
};

struct iff_info {
	char id[5];
	void (*loader)(struct module_data *, int, FILE *, void *);
	struct list_head list;
};

iff_handle iff_new(void);
void iff_chunk(iff_handle, struct module_data *, FILE *, void *);
void iff_register(iff_handle, char *,
		  void (*loader)(struct module_data *, int, FILE *, void *));
void iff_id_size(iff_handle, int);
void iff_set_quirk(iff_handle, int);
void iff_release(iff_handle);
int iff_process(iff_handle, struct module_data *, char *, long, FILE *, void *);

#endif /* __IFF_H */
