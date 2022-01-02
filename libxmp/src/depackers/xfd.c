/* xfdmaster.library decruncher for XMP
 * Copyright (C) 2007 Chris Young
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

#include "../common.h"

#if defined(LIBXMP_AMIGA) && defined(HAVE_PROTO_XFDMASTER_H)

#define __USE_INLINE__
#include <proto/exec.h>
#include <proto/xfdmaster.h>
#include <exec/types.h>
#include "depacker.h"

static int _test_xfd(unsigned char *buffer, int length)
{
	int ret = 0;
	struct xfdBufferInfo *xfdobj;

	xfdobj = (struct xfdBufferInfo *) xfdAllocObject(XFDOBJ_BUFFERINFO);
	if(xfdobj)
	{
		xfdobj->xfdbi_SourceBuffer = buffer;
		xfdobj->xfdbi_SourceBufLen = length;
		xfdobj->xfdbi_Flags = XFDFB_RECOGTARGETLEN | XFDFB_RECOGEXTERN;

		if(xfdRecogBuffer(xfdobj))
		{
			ret = (xfdobj->xfdbi_PackerName != NULL);
		}
		xfdFreeObject((APTR)xfdobj);
	}
	return(ret);
}

static int test_xfd(unsigned char *b)
{
	if (!xfdMasterBase) return 0;
	return _test_xfd(b, 1024);
}

static int decrunch_xfd(HIO_HANDLE *f, void **outbuf, long inlen, long *outlen)
{
    struct xfdBufferInfo *xfdobj;
    uint8 *packed;
    void *unpacked;
    int ret = -1;

    if (xfdMasterBase == NULL)
	return -1;

    packed = (uint8 *) AllocVec(inlen,MEMF_CLEAR);
    if (!packed) return -1;

    hio_read(packed,inlen,1,f);

	xfdobj = (struct xfdBufferInfo *) xfdAllocObject(XFDOBJ_BUFFERINFO);
	if(xfdobj)
	{
		xfdobj->xfdbi_SourceBufLen = inlen;
		xfdobj->xfdbi_SourceBuffer = packed;
		xfdobj->xfdbi_Flags = XFDFF_RECOGEXTERN | XFDFF_RECOGTARGETLEN;
		/* xfdobj->xfdbi_PackerFlags = XFDPFF_RECOGLEN; */
		if(xfdRecogBuffer(xfdobj))
		{
			xfdobj->xfdbi_TargetBufMemType = MEMF_ANY;
			if(xfdDecrunchBuffer(xfdobj))
			{
				unpacked = malloc(xfdobj->xfdbi_TargetBufSaveLen);
				if (unpacked) {
					memcpy(unpacked, xfdobj->xfdbi_TargetBuffer, xfdobj->xfdbi_TargetBufSaveLen);
					*outbuf = unpacked;
					*outlen = xfdobj->xfdbi_TargetBufSaveLen;
					ret=0;
				}
				else
				{
					ret=-1;
				}
				FreeMem(xfdobj->xfdbi_TargetBuffer,xfdobj->xfdbi_TargetBufLen);
			}
			else
			{
				ret=-1;
			}
		}
		xfdFreeObject((APTR)xfdobj);
	}
	FreeVec(packed);
	return(ret);
}

struct depacker libxmp_depacker_xfd = {
	test_xfd,
	NULL,
	decrunch_xfd
};

#endif /* AMIGA */
