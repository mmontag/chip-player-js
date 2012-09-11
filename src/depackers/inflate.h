#ifndef XMP_INFLATE_H
#define XMP_INFLATE_H

struct inflate_data {
  struct huffman_tree_t *huffman_tree_len_static;
  /* int crc_built; */
  unsigned int crc_table[256];
};


int inflate(FILE *, FILE *, uint32 *, int);
int build_crc32(struct inflate_data *data);
unsigned int crc32(unsigned char *, int, unsigned int, struct inflate_data *);

#endif
