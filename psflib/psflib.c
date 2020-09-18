/*
PSFLIB - Main PSF parser implementation

Copyright (c) 2012-2015 Christopher Snowhill

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "psflib.h"

#include <string.h>
#include <stdlib.h>

#include <zlib.h>

#ifdef _MSC_VER
#define snprintf sprintf_s
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

#undef strdup
#define strdup(s) my_strdup(s)

static char * my_strdup(const char * s)
{
	size_t l;
	char * r;
	if (!s) return NULL;
	l = strlen(s) + 1;
	r = (char *)malloc(l);
	if (!r) return NULL;
	memcpy(r, s, l);
	return r;
}

const char * strrpbrk(const char * s, const char * accept)
{
	const char * start;

	if (!s || !*s || !accept || !*accept) return NULL;

	start = s;
	s += strlen(s) - 1;

	while (s >= start)
	{
		const char *a = accept;
		while (*a != '\0')
			if (*a++ == *s)
				return s;
		--s;
	}

	return NULL;
}

enum { max_recursion_depth = 10 };

typedef struct psf_load_state
{
	int                        depth;

	unsigned char              allowed_version;

	char                     * base_path;
	const psf_file_callbacks * file_callbacks;

	psf_load_callback          load_target;
	void                     * load_context;

	psf_info_callback          info_target;
	void                     * info_context;
	int                        info_want_nested_tags;

	psf_status_callback        status_target;
	void                     * status_context;

	char                       lib_name_temp[32];
} psf_load_state;

static int psf_load_internal(psf_load_state * state, const char * file_name);


static int psf_want_status(psf_load_state * state);
static void psf_status(psf_load_state * state, const char * message, int indent);

int psf_load(const char * uri, const psf_file_callbacks * file_callbacks, uint8_t allowed_version,
	psf_load_callback load_target, void * load_context, psf_info_callback info_target,
	void * info_context, int info_want_nested_tags, psf_status_callback status_target,
	void * status_context)
{
	int rval;

	psf_load_state state;

	const char * file_name;

	if (!uri || !*uri || !file_callbacks || !file_callbacks->path_separators || !*file_callbacks->path_separators || !file_callbacks->fopen ||
		!file_callbacks->fread || !file_callbacks->fseek || !file_callbacks->fclose || !file_callbacks->ftell) return -1;

	state.depth = 0;
	state.allowed_version = allowed_version;
	state.file_callbacks = file_callbacks;
	state.load_target = load_target;
	state.load_context = load_context;
	state.info_target = info_target;
	state.info_context = info_context;
	state.info_want_nested_tags = info_want_nested_tags;
	state.status_target = status_target;
	state.status_context = status_context;

	state.base_path = strdup(uri);
	if (!state.base_path)
	{
		psf_status(&state, "Out of memory allocating state.base_path\n", 1);
		return -1;
	}

	file_name = strrpbrk(uri, file_callbacks->path_separators);

	if (file_name)
	{
		++file_name;
		state.base_path[file_name - uri] = '\0';
	}
	else
	{
		state.base_path[0] = '\0';
		file_name = uri;
	}

	rval = psf_load_internal(&state, file_name);

	free(state.base_path);

	psf_status(&state, "Done.", 0);

	return rval;
}

typedef struct psf_tag psf_tag;

struct psf_tag {
	char * name;
	char * value;
	psf_tag * next, *prev;
};

#define FIELDS_SPLIT 6
static const char * fields_to_split[FIELDS_SPLIT] = { "ARTIST", "ALBUM ARTIST", "PRODUCER", "COMPOSER", "PERFORMER", "GENRE" };

static int check_split_value(const char * name)
{
	unsigned i;
	for (i = 0; i < FIELDS_SPLIT; i++)
	{
		if (!strcasecmp(name, fields_to_split[i])) return 1;
	}
	return 0;
}

static psf_tag * find_tag(psf_tag * tags, const char * name)
{
	if (tags && name && *name)
	{
		while (tags)
		{
			if (!strcasecmp(tags->name, name)) return tags;
			tags = tags->next;
		}
	}

	return NULL;
}

static void free_tags(psf_tag * tags)
{
	psf_tag * tag = tags, *next;
	while (tag)
	{
		next = tag->next;
		if (tag->name) free(tag->name);
		if (tag->value) free(tag->value);
		free(tag);
		tag = next;
	}
}

static psf_tag * add_tag_multi(psf_tag * tags, const char * name, const char ** values, int values_count)
{
	psf_tag * footer;
	psf_tag * tag;

	int i;

	if (!name || !*name || !values || !values_count || !*values) return NULL;

	footer = tags;

	tag = find_tag(tags, name);
	if (!tag)
	{
		tag = calloc(1, sizeof(psf_tag));
		if (!tag) return footer;
		tag->name = strdup(name);
		if (!tag->name)
		{
			free(tag);
			return footer;
		}
		tag->next = tags;
		if (tags) tags->prev = tag;
		footer = tag;
	}
	if (tag->value)
	{
		size_t old_length = strlen(tag->value);
		size_t new_length = strlen(values[0]);
		char * new_value = (char *)realloc(tag->value, old_length + new_length + 2);
		if (!new_value) return footer;
		tag->value = new_value;
		new_value[old_length] = '\n';
#if _MSC_VER >= 1300
		strcpy_s(new_value + old_length + 1, new_length + 1, values[0]);
#else
		strcpy(new_value + old_length + 1, values[0]);
#endif
	}
	else
	{
		tag->value = strdup(values[0]);
		if (!tag->value) return footer;
	}

	for (i = 1; i < values_count; i++)
	{
		tag = calloc(1, sizeof(psf_tag));
		if (!tag) return footer;
		tag->name = strdup(name);
		if (!tag->name)
		{
			free(tag);
			return footer;
		}
		tag->value = strdup(values[i]);
		if (!tag->value)
		{
			free(tag->name);
			free(tag);
			return footer;
		}
		tag->next = footer;
		if (footer) footer->prev = tag;
		footer = tag;
	}

	return footer;
}

static psf_tag * add_tag(psf_tag * tags, const char * name, const char * value)
{
	int values_count;
	const char ** values;
	char * value_split;

	if (!name || !*name || !value || !*value) return tags;

	if (check_split_value(name))
	{
		char * split_point, *remain;
		const char ** new_values;
		values_count = 0;
		values = NULL;
		value_split = strdup(value);
		if (!value_split) return tags;
		remain = value_split;
		split_point = strstr(value_split, "; ");
		while (split_point)
		{
			values_count++;
			new_values = (const char **)realloc((void *)values, sizeof(const char*) * ((values_count + 3) & ~3));
			if (!new_values)
			{
				if (values) free((void *)values);
				free(value_split);
				return tags;
			}
			values = new_values;
			*split_point = '\0';
			values[values_count - 1] = remain;
			remain = split_point + 2;
			split_point = strstr(remain, "; ");
		}
		if (*remain)
		{
			values_count++;
			new_values = (const char **)realloc((void *)values, sizeof(char*) * ((values_count + 3) & ~3));
			if (!new_values)
			{
				if (values) free((void *)values);
				free(value_split);
				return tags;
			}
			values = new_values;
			values[values_count - 1] = remain;
		}
	}
	else
	{
		values_count = 1;
		value_split = NULL;
		values = (const char **)malloc(sizeof(const char *));
		if (!values) return tags;
		values[0] = value;
	}

	tags = add_tag_multi(tags, name, values, values_count);

	if (value_split) free(value_split);
	free((void *)values);

	return tags;
}

/* Split line on first equals sign, and remove any whitespace surrounding the name and value fields */

static psf_tag * process_tag_line(psf_tag * tags, char * line)
{
	char * name, *value, *end;
	char * equals = strchr(line, '=');
	if (!equals) return tags;

	name = line;
	value = equals + 1;
	end = line + strlen(line);

	while (name < equals && *name > 0 && *name <= ' ') name++;
	if (name == equals) return tags;

	--equals;
	while (equals > name && *equals > 0 && *equals <= ' ') --equals;
	equals[1] = '\0';

	while (value < end && *value > 0 && *value <= ' ') value++;
	if (value == end) return tags;

	--end;
	while (end > value && *end > 0 && *end <= ' ') --end;
	end[1] = '\0';

	if (*name == '_')
	{
		psf_tag * tag = find_tag(tags, name);
		if (tag) return tags;
	}

	return add_tag(tags, name, value);
}

static psf_tag * process_tags(char * buffer)
{
	psf_tag * tags = NULL;
	char * line_end;
	if (!buffer || !*buffer) return NULL;

	line_end = strpbrk(buffer, "\n\r");
	while (line_end)
	{
		*line_end++ = '\0';
		tags = process_tag_line(tags, buffer);
		while (*line_end && (*line_end == '\n' || *line_end == '\r')) line_end++;
		buffer = line_end;
		line_end = strpbrk(buffer, "\n\r");
	}
	if (*buffer) tags = process_tag_line(tags, buffer);

	return tags;
}

static int psf_load_internal(psf_load_state * state, const char * file_name)
{
	psf_tag * tags = NULL;
	psf_tag * tag;

	char * full_path;

	void * file;

	long file_size, tag_size;

	int n;

	uint8_t header_buffer[16];

	uint8_t * exe_compressed_buffer = NULL;
	uint8_t * exe_decompressed_buffer = NULL;
	uint8_t * reserved_buffer = NULL;
	char * tag_buffer = NULL;

	uint32_t exe_compressed_size, exe_crc32, reserved_size;
	uLong exe_decompressed_size, try_exe_decompressed_size;

	int zerr;

	size_t full_path_size;

	if (++state->depth > max_recursion_depth)
	{
		psf_status(state, "Exceeded maximum file nesting depth.\n", 1);
		return -1;
	}

	full_path_size = strlen(state->base_path) + strlen(file_name) + 1;
	full_path = (char *)malloc(full_path_size);
	if (!full_path) return -1;

#if _MSC_VER >= 1300
	strcpy_s(full_path, full_path_size, state->base_path);
	strcat_s(full_path, full_path_size, file_name);
#else
	strcpy(full_path, state->base_path);
	strcat(full_path, file_name);
#endif

	file = state->file_callbacks->fopen(state->file_callbacks->context, full_path);

	free(full_path);

	if (!file)
	{
		if (psf_want_status(state))
		{
			psf_status(state, "Error opening file: ", 1);
			psf_status(state, file_name, 0);
			psf_status(state, "\n", 0);
			psf_status(state, "From base path: ", 1);
			psf_status(state, state->base_path, 0);
			psf_status(state, "\n", 0);
		}
		return -1;
	}

	if (psf_want_status(state))
	{
		psf_status(state, "Opened file: ", 1);
		psf_status(state, file_name, 0);
		psf_status(state, "\n", 0);
		psf_status(state, "From base path: ", 1);
		psf_status(state, state->base_path, 0);
		psf_status(state, "\n", 0);
	}

	if (state->file_callbacks->fread(header_buffer, 1, 16, file) < 16)
	{
		psf_status(state, "File too small to contain a valid header.\n", 1);
		goto error_close_file;
	}

	if (memcmp(header_buffer, "PSF", 3))
	{
		psf_status(state, "File does not contain a valid PSF signature.\n", 1);
		goto error_close_file;
	}

	if (state->allowed_version && (header_buffer[3] != state->allowed_version))
	{
		if (psf_want_status(state))
		{
			char *end;
			char temp[8];
			psf_status(state, "Expected PSF version ", 1);
			snprintf(temp, 7, "%d", (int)state->allowed_version);
			temp[7] = '\0';
			psf_status(state, temp, 0);
			psf_status(state, ", got ", 0);
			snprintf(temp, 7, "%d", (int)header_buffer[3]);
			temp[7] = '\0';
			psf_status(state, temp, 0);
			psf_status(state, "\n", 0);
		}
		goto error_close_file;
	}

	reserved_size = header_buffer[4] | (header_buffer[5] << 8) | (header_buffer[6] << 16) | (header_buffer[7] << 24);
	exe_compressed_size = header_buffer[8] | (header_buffer[9] << 8) | (header_buffer[10] << 16) | (header_buffer[11] << 24);
	exe_crc32 = header_buffer[12] | (header_buffer[13] << 8) | (header_buffer[14] << 16) | (header_buffer[15] << 24);

	if (state->file_callbacks->fseek(file, 0, SEEK_END))
	{
		psf_status(state, "Could not seek to end of file to determine file size.\n", 1);
		goto error_close_file;
	}

	file_size = state->file_callbacks->ftell(file);

	if (file_size <= 0)
	{
		psf_status(state, "Could not determine file size.\n", 1);
		goto error_close_file;
	}

	if ((unsigned long)file_size >= 16 + reserved_size + exe_compressed_size + 5)
	{
		psf_status(state, "Tag detected, attempting to read it.\n", 1);

		tag_size = file_size - (16 + reserved_size + exe_compressed_size);
		if (state->file_callbacks->fseek(file, -tag_size, SEEK_CUR))
		{
			psf_status(state, "Could not seek back to read tag.\n", 1);
			goto error_close_file;
		}
		tag_buffer = (char *)malloc(tag_size + 1);
		if (!tag_buffer)
		{
			psf_status(state, "Out of memory allocating tag buffer.\n", 1);
			goto error_close_file;
		}
		if (state->file_callbacks->fread(tag_buffer, 1, tag_size, file) < (size_t)tag_size)
		{
			psf_status(state, "Could not read tag.\n", 1);
			goto error_free_buffers;
		}
		tag_buffer[tag_size] = 0;
		if (!memcmp(tag_buffer, "[TAG]", 5)) tags = process_tags(tag_buffer + 5);
		free(tag_buffer);
		tag_buffer = NULL;

		if (tags && state->info_target && (state->depth == 1 || state->info_want_nested_tags))
		{
			tag = tags;
			while (tag->next) tag = tag->next;
			while (tag)
			{
				if (state->info_target(state->info_context, tag->name, tag->value))
				{
					if (psf_want_status(state))
					{
						psf_status(state, "Caller rejected tag: ", 1);
						psf_status(state, tag->name, 0);
						psf_status(state, "=", 0);
						psf_status(state, tag->value, 0);
						psf_status(state, "\n", 0);
					}
					goto error_free_tags;
				}
				tag = tag->prev;
			}
		}
	}

	if (!state->load_target) goto done;

	tag = find_tag(tags, "_lib");
	if (tag)
	{
		if (psf_want_status(state))
		{
			psf_status(state, "Found _lib: ", 1);
			psf_status(state, tag->value, 0);
			psf_status(state, "\n", 0);
		}
		if (psf_load_internal(state, tag->value) < 0) goto error_free_tags;
	}

	reserved_buffer = (uint8_t *)malloc(reserved_size);
	if (!reserved_buffer)
	{
		psf_status(state, "Out of memory allocating buffer for reserved section.\n", 1);
		goto error_free_tags;
	}
	exe_compressed_buffer = (uint8_t *)malloc(exe_compressed_size);
	if (!exe_compressed_buffer)
	{
		psf_status(state, "Out of memory allocating buffer for compressed exe section.\n", 1);
		goto error_free_tags;
	}

	if (state->file_callbacks->fseek(file, 16, SEEK_SET))
	{
		psf_status(state, "Could not seek back to main data section of file.", 1);
		goto error_free_tags;
	}
	if (reserved_size && state->file_callbacks->fread(reserved_buffer, 1, reserved_size, file) < reserved_size)
	{
		psf_status(state, "Could not read reserved section.\n", 1);
		goto error_free_tags;
	}
	if (exe_compressed_size && state->file_callbacks->fread(exe_compressed_buffer, 1, exe_compressed_size, file) < exe_compressed_size)
	{
		psf_status(state, "Could not read compressed exe section.\n", 1);
		goto error_free_tags;
	}
	state->file_callbacks->fclose(file);
	file = NULL;

	psf_status(state, "File closed.\n", 1);

	if (exe_compressed_size)
	{
		uint32_t got_crc32 = crc32(crc32(0L, Z_NULL, 0), exe_compressed_buffer, exe_compressed_size);
		if (exe_crc32 != got_crc32)
		{
			if (psf_want_status(state))
			{
				char temp[16];
				psf_status(state, "CRC mismatch on compressed exe section.\nWanted: 0x", 1);
				snprintf(temp, 15, "%X", exe_crc32);
				temp[15] = '\0';
				psf_status(state, temp, 0);
				psf_status(state, ", got 0x", 0);
				snprintf(temp, 15, "%X", got_crc32);
				temp[15] = '\0';
				psf_status(state, temp, 0);
				psf_status(state, "\n", 0);
			}
			goto error_free_tags;
		}

		exe_decompressed_size = try_exe_decompressed_size = exe_compressed_size * 3;
		exe_decompressed_buffer = (uint8_t *)malloc(exe_decompressed_size);
		if (!exe_decompressed_buffer)
		{
			psf_status(state, "Out of memory allocating buffer for decompressed exe section.\n", 1);
			goto error_free_tags;
		}

		while (Z_OK != (zerr = uncompress(exe_decompressed_buffer, &exe_decompressed_size, exe_compressed_buffer, exe_compressed_size)))
		{
			void * try_exe_decompressed_buffer;

			if (Z_MEM_ERROR != zerr && Z_BUF_ERROR != zerr)
			{
				psf_status(state, "Could not decompress exe section.\n", 1);
				goto error_free_tags;
			}

			if (try_exe_decompressed_size < 1 * 1024 * 1024)
				try_exe_decompressed_size += 1 * 1024 * 1024;
			else
				try_exe_decompressed_size += try_exe_decompressed_size;

			exe_decompressed_size = try_exe_decompressed_size;

			try_exe_decompressed_buffer = realloc(exe_decompressed_buffer, exe_decompressed_size);
			if (!try_exe_decompressed_buffer)
			{
				psf_status(state, "Out of memory reallocating buffer for decompressed exe section.\n", 1);
				goto error_free_tags;
			}

			exe_decompressed_buffer = (uint8_t *)try_exe_decompressed_buffer;
		}
	}
	else
	{
		exe_decompressed_size = 0;
		exe_decompressed_buffer = (uint8_t *)malloc(exe_decompressed_size);
		if (!exe_decompressed_buffer)
		{
			psf_status(state, "Out of memory allocating dummy buffer for exe section.\n", 1);
			goto error_free_tags;
		}
	}

	free(exe_compressed_buffer);
	exe_compressed_buffer = NULL;

	psf_status(state, "Passing exe and reserved back out.\n", 1);

	if (state->load_target(state->load_context, exe_decompressed_buffer, exe_decompressed_size, reserved_buffer, reserved_size))
	{
		psf_status(state, "Data handler returned an error.\n", 1);
		goto error_free_tags;
	}

	free(reserved_buffer);
	reserved_buffer = NULL;

	free(exe_decompressed_buffer);
	exe_decompressed_buffer = NULL;

	n = 2;
	snprintf(state->lib_name_temp, 31, "_lib%u", n);
	state->lib_name_temp[31] = '\0';
	tag = find_tag(tags, state->lib_name_temp);
	while (tag)
	{
		if (psf_want_status(state))
		{
			psf_status(state, "Found ", 1);
			psf_status(state, tag->name, 0);
			psf_status(state, ": ", 0);
			psf_status(state, tag->value, 0);
			psf_status(state, "\n", 0);
		}
		if (psf_load_internal(state, tag->value) < 0) goto error_free_tags;
		++n;
		snprintf(state->lib_name_temp, 31, "_lib%u", n);
		state->lib_name_temp[31] = '\0';
		tag = find_tag(tags, state->lib_name_temp);
	}

done:
	if (file) state->file_callbacks->fclose(file);

	free_tags(tags);

	--state->depth;

	return header_buffer[3];

error_free_tags:
	free_tags(tags);
error_free_buffers:
	if (exe_compressed_buffer) free(exe_compressed_buffer);
	if (exe_decompressed_buffer) free(exe_decompressed_buffer);
	if (reserved_buffer) free(reserved_buffer);
	if (tag_buffer) free(tag_buffer);
error_close_file:
	if (file) state->file_callbacks->fclose(file);
	return -1;
}

int psf_want_status(psf_load_state * state)
{
	return !!state->status_target;
}

void psf_status(psf_load_state * state, const char * message, int indent)
{
	if (state->status_target)
	{
		if (indent)
		{
			char indent[16];
			int indent_level = state->depth > 1 ? state->depth - 1 : 0;
			memset(indent, ' ', indent_level);
			indent[indent_level] = '\0';
			state->status_target(state->status_context, indent);
		}
		state->status_target(state->status_context, message);
	}
}
