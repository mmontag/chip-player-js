#ifndef MINIZ_ZIP_H
#define MINIZ_ZIP_H

#include "miniz.h"

#if 1 /* LIBXMP-SPECIFIC : */
/* change namespace from mz_ to libxmp_ for public functions: */
#define mz_zip_reader_init libxmp_zip_reader_init
#define mz_zip_reader_end libxmp_zip_reader_end
#define mz_zip_get_error_string libxmp_zip_get_error_string
#define mz_zip_reader_is_file_a_directory libxmp_zip_reader_is_file_a_directory
#define mz_zip_reader_is_file_encrypted libxmp_zip_reader_is_file_encrypted
#define mz_zip_reader_is_file_supported libxmp_zip_reader_is_file_supported
#define mz_zip_reader_get_filename libxmp_zip_reader_get_filename
#define mz_zip_reader_locate_file libxmp_zip_reader_locate_file
#define mz_zip_reader_locate_file_v2 libxmp_zip_reader_locate_file_v2
#define mz_zip_reader_file_stat libxmp_zip_reader_file_stat
#define mz_zip_reader_extract_to_mem libxmp_zip_reader_extract_to_mem
#define mz_zip_reader_extract_to_mem_no_alloc libxmp_zip_reader_extract_to_mem_no_alloc
#define mz_zip_reader_extract_to_heap libxmp_zip_reader_extract_to_heap
#define mz_zip_reader_extract_to_callback libxmp_zip_reader_extract_to_callback
#endif /* LIBXMP-SPECIFIC */

/* ------------------- ZIP archive reading/writing */

#ifndef MINIZ_NO_ARCHIVE_APIS

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    /* Note: These enums can be reduced as needed to save memory or stack space - they are pretty conservative. */
    MZ_ZIP_MAX_IO_BUF_SIZE = 64 * 1024,
    MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE = 512,
    MZ_ZIP_MAX_ARCHIVE_FILE_COMMENT_SIZE = 512
};

typedef struct
{
    /* Central directory file index. */
    mz_uint32 m_file_index;

    /* Byte offset of this entry in the archive's central directory. Note we currently only support up to UINT_MAX or less bytes in the central dir. */
    mz_uint64 m_central_dir_ofs;

    /* These fields are copied directly from the zip's central dir. */
    mz_uint16 m_version_made_by;
    mz_uint16 m_version_needed;
    mz_uint16 m_bit_flag;
    mz_uint16 m_method;

    /* CRC-32 of uncompressed data. */
    mz_uint32 m_crc32;

    /* File's compressed size. */
    mz_uint64 m_comp_size;

    /* File's uncompressed size. Note, I've seen some old archives where directory entries had 512 bytes for their uncompressed sizes, but when you try to unpack them you actually get 0 bytes. */
    mz_uint64 m_uncomp_size;

    /* Zip internal and external file attributes. */
    mz_uint16 m_internal_attr;
    mz_uint32 m_external_attr;

    /* Entry's local header file offset in bytes. */
    mz_uint64 m_local_header_ofs;

    /* Size of comment in bytes. */
    mz_uint32 m_comment_size;

    /* MZ_TRUE if the entry appears to be a directory. */
    mz_bool m_is_directory;

    /* MZ_TRUE if the entry uses encryption/strong encryption (which miniz_zip doesn't support) */
    mz_bool m_is_encrypted;

    /* MZ_TRUE if the file is not encrypted, a patch file, and if it uses a compression method we support. */
    mz_bool m_is_supported;

    /* Filename. If string ends in '/' it's a subdirectory entry. */
    /* Guaranteed to be zero terminated, may be truncated to fit. */
    char m_filename[MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE];

    /* Comment field. */
    /* Guaranteed to be zero terminated, may be truncated to fit. */
    char m_comment[MZ_ZIP_MAX_ARCHIVE_FILE_COMMENT_SIZE];

} mz_zip_archive_file_stat;

typedef size_t (*mz_file_read_func)(void *pOpaque, mz_uint64 file_ofs, void *pBuf, size_t n);
typedef size_t (*mz_file_write_func)(void *pOpaque, mz_uint64 file_ofs, const void *pBuf, size_t n);
typedef mz_bool (*mz_file_needs_keepalive)(void *pOpaque);

struct mz_zip_internal_state_tag;
typedef struct mz_zip_internal_state_tag mz_zip_internal_state;

typedef enum {
    MZ_ZIP_MODE_INVALID = 0,
    MZ_ZIP_MODE_READING = 1,
    MZ_ZIP_MODE_WRITING = 2,
    MZ_ZIP_MODE_WRITING_HAS_BEEN_FINALIZED = 3
} mz_zip_mode;

typedef enum {
    MZ_ZIP_FLAG_CASE_SENSITIVE = 0x0100,
    MZ_ZIP_FLAG_IGNORE_PATH = 0x0200,
    MZ_ZIP_FLAG_COMPRESSED_DATA = 0x0400,
    MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY = 0x0800,
    MZ_ZIP_FLAG_VALIDATE_LOCATE_FILE_FLAG = 0x1000, /* if enabled, mz_zip_reader_locate_file() will be called on each file as its validated to ensure the func finds the file in the central dir (intended for testing) */
    MZ_ZIP_FLAG_VALIDATE_HEADERS_ONLY = 0x2000,     /* validate the local headers, but don't decompress the entire file and check the crc32 */
    MZ_ZIP_FLAG_WRITE_ZIP64 = 0x4000,               /* always use the zip64 file format, instead of the original zip file format with automatic switch to zip64. Use as flags parameter with mz_zip_writer_init*_v2 */
    MZ_ZIP_FLAG_WRITE_ALLOW_READING = 0x8000,
    MZ_ZIP_FLAG_ASCII_FILENAME = 0x10000,
    /*After adding a compressed file, seek back
    to local file header and set the correct sizes*/
    MZ_ZIP_FLAG_WRITE_HEADER_SET_SIZE = 0x20000
} mz_zip_flags;

typedef enum {
    MZ_ZIP_TYPE_INVALID = 0,
    MZ_ZIP_TYPE_USER,
    MZ_ZIP_TYPE_MEMORY,
    MZ_ZIP_TYPE_HEAP,
    MZ_ZIP_TYPE_FILE,
    MZ_ZIP_TYPE_CFILE,
    MZ_ZIP_TOTAL_TYPES
} mz_zip_type;

/* miniz error codes. Be sure to update mz_zip_get_error_string() if you add or modify this enum. */
typedef enum {
    MZ_ZIP_NO_ERROR = 0,
    MZ_ZIP_UNDEFINED_ERROR,
    MZ_ZIP_TOO_MANY_FILES,
    MZ_ZIP_FILE_TOO_LARGE,
    MZ_ZIP_UNSUPPORTED_METHOD,
    MZ_ZIP_UNSUPPORTED_ENCRYPTION,
    MZ_ZIP_UNSUPPORTED_FEATURE,
    MZ_ZIP_FAILED_FINDING_CENTRAL_DIR,
    MZ_ZIP_NOT_AN_ARCHIVE,
    MZ_ZIP_INVALID_HEADER_OR_CORRUPTED,
    MZ_ZIP_UNSUPPORTED_MULTIDISK,
    MZ_ZIP_DECOMPRESSION_FAILED,
    MZ_ZIP_COMPRESSION_FAILED,
    MZ_ZIP_UNEXPECTED_DECOMPRESSED_SIZE,
    MZ_ZIP_CRC_CHECK_FAILED,
    MZ_ZIP_UNSUPPORTED_CDIR_SIZE,
    MZ_ZIP_ALLOC_FAILED,
    MZ_ZIP_FILE_OPEN_FAILED,
    MZ_ZIP_FILE_CREATE_FAILED,
    MZ_ZIP_FILE_WRITE_FAILED,
    MZ_ZIP_FILE_READ_FAILED,
    MZ_ZIP_FILE_CLOSE_FAILED,
    MZ_ZIP_FILE_SEEK_FAILED,
    MZ_ZIP_FILE_STAT_FAILED,
    MZ_ZIP_INVALID_PARAMETER,
    MZ_ZIP_INVALID_FILENAME,
    MZ_ZIP_BUF_TOO_SMALL,
    MZ_ZIP_INTERNAL_ERROR,
    MZ_ZIP_FILE_NOT_FOUND,
    MZ_ZIP_ARCHIVE_TOO_LARGE,
    MZ_ZIP_VALIDATION_FAILED,
    MZ_ZIP_WRITE_CALLBACK_FAILED,
    MZ_ZIP_TOTAL_ERRORS
} mz_zip_error;

typedef struct
{
    mz_uint64 m_archive_size;
    mz_uint64 m_central_directory_file_ofs;

    /* We only support up to UINT32_MAX files in zip64 mode. */
    mz_uint32 m_total_files;
    mz_zip_mode m_zip_mode;
    mz_zip_type m_zip_type;
    mz_zip_error m_last_error;

    mz_uint64 m_file_offset_alignment;

    mz_alloc_func m_pAlloc;
    mz_free_func m_pFree;
    mz_realloc_func m_pRealloc;
    void *m_pAlloc_opaque;

    mz_file_read_func m_pRead;
    mz_file_write_func m_pWrite;
    mz_file_needs_keepalive m_pNeeds_keepalive;
    void *m_pIO_opaque;

    mz_zip_internal_state *m_pState;

} mz_zip_archive;

/* -------- ZIP reading */

/* Inits a ZIP archive reader. */
/* These functions read and validate the archive's central directory. */
MINIZ_EXPORT mz_bool mz_zip_reader_init(mz_zip_archive *pZip, mz_uint64 size, mz_uint flags);

/* Ends archive reading, freeing all allocations, and closing the input archive file if mz_zip_reader_init_file() was used. */
MINIZ_EXPORT mz_bool mz_zip_reader_end(mz_zip_archive *pZip);

/* -------- ZIP reading or writing */

#ifdef DEBUG /* libxmp uses this only in debug mode */
MINIZ_EXPORT const char *mz_zip_get_error_string(mz_zip_error mz_err);
#endif

/* MZ_TRUE if the archive file entry is a directory entry. */
MINIZ_EXPORT mz_bool mz_zip_reader_is_file_a_directory(mz_zip_archive *pZip, mz_uint file_index);

/* MZ_TRUE if the file is encrypted/strong encrypted. */
MINIZ_EXPORT mz_bool mz_zip_reader_is_file_encrypted(mz_zip_archive *pZip, mz_uint file_index);

/* MZ_TRUE if the compression method is supported, and the file is not encrypted, and the file is not a compressed patch file. */
MINIZ_EXPORT mz_bool mz_zip_reader_is_file_supported(mz_zip_archive *pZip, mz_uint file_index);

/* Retrieves the filename of an archive file entry. */
/* Returns the number of bytes written to pFilename, or if filename_buf_size is 0 this function returns the number of bytes needed to fully store the filename. */
MINIZ_EXPORT mz_uint mz_zip_reader_get_filename(mz_zip_archive *pZip, mz_uint file_index, char *pFilename, mz_uint filename_buf_size);

/* Attempts to locates a file in the archive's central directory. */
/* Valid flags: MZ_ZIP_FLAG_CASE_SENSITIVE, MZ_ZIP_FLAG_IGNORE_PATH */
/* Returns -1 if the file cannot be found. */
MINIZ_EXPORT int mz_zip_reader_locate_file(mz_zip_archive *pZip, const char *pName, const char *pComment, mz_uint flags);
MINIZ_EXPORT mz_bool mz_zip_reader_locate_file_v2(mz_zip_archive *pZip, const char *pName, const char *pComment, mz_uint flags, mz_uint32 *file_index);

/* Returns detailed information about an archive file entry. */
MINIZ_EXPORT mz_bool mz_zip_reader_file_stat(mz_zip_archive *pZip, mz_uint file_index, mz_zip_archive_file_stat *pStat);

/* Extracts a archive file to a memory buffer. */
MINIZ_EXPORT mz_bool mz_zip_reader_extract_to_mem(mz_zip_archive *pZip, mz_uint file_index, void *pBuf, size_t buf_size, mz_uint flags);

/* Extracts a archive file to a dynamically allocated heap buffer. */
/* The memory will be allocated via the mz_zip_archive's alloc/realloc functions. */
/* Returns NULL and sets the last error on failure. */
MINIZ_EXPORT void *mz_zip_reader_extract_to_heap(mz_zip_archive *pZip, mz_uint file_index, size_t *pSize, mz_uint flags);

/* Extracts a archive file using a callback function to output the file's data. */
MINIZ_EXPORT mz_bool mz_zip_reader_extract_to_callback(mz_zip_archive *pZip, mz_uint file_index, mz_file_write_func pCallback, void *pOpaque, mz_uint flags);

#ifdef __cplusplus
}
#endif

#endif /* MINIZ_NO_ARCHIVE_APIS */

#endif /* MINIZ_ZIP_H */
