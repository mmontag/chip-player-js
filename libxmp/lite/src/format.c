/* Extended Module Player
 * Copyright (C) 1996-2021 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "format.h"

extern const struct format_loader libxmp_loader_xm;
extern const struct format_loader libxmp_loader_mod;
extern const struct format_loader libxmp_loader_it;
extern const struct format_loader libxmp_loader_s3m;

const struct format_loader *const format_loaders[5] = {
	&libxmp_loader_xm,
	&libxmp_loader_mod,
#ifndef LIBXMP_CORE_DISABLE_IT
	&libxmp_loader_it,
#endif
	&libxmp_loader_s3m,
	NULL
};

static const char *_farray[5] = { NULL };

const char *const *format_list(void)
{
	int count, i;

	if (_farray[0] == NULL) {
		for (count = i = 0; format_loaders[i] != NULL; i++) {
			_farray[count++] = format_loaders[i]->name;
		}

		_farray[count] = NULL;
	}

	return _farray;
}
