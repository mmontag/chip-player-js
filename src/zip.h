#ifndef __ZIP_H
#define __ZIP_H

struct zip_data {
  struct huffman_tree_t *huffman_tree_len_static;
  /* int crc_built; */
  unsigned int crc_table[256];
};


struct zip_local_file_header_t
{
  unsigned int signature;
  int version;
  int general_purpose_bit_flag;
  int compression_method;
  int last_mod_file_time;
  int last_mod_file_date;
  unsigned int crc_32;
  int compressed_size;
  int uncompressed_size;
  int file_name_length;
  int extra_field_length;
  char *file_name;
  unsigned char *extra_field;
};

#define QUIET


int inflate(FILE *, FILE *, unsigned int *, int);
int build_crc32(struct zip_data *data);
unsigned int crc32(unsigned char *buffer, int len, unsigned int crc, struct zip_data *data);

#endif
