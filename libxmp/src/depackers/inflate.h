#ifndef LIBXMP_INFLATE_H
#define LIBXMP_INFLATE_H

struct inflate_data {
	struct huffman_tree_t *huffman_tree_len_static;
};

int	libxmp_inflate	(FILE *, FILE *, uint32 *, int);

#endif
