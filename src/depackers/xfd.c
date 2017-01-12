/* xfdmaster.library decruncher for XMP
 * Copyright (C) 2007 Chris Young
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#ifdef __SUNPRO_C
#pragma error_messages (off,E_EMPTY_TRANSLATION_UNIT)
#endif

#ifdef AMIGA
#define __USE_INLINE__
#include <proto/exec.h>
#include <proto/xfdmaster.h>
#include <exec/types.h>
#include <stdio.h>
#include <sys/stat.h>
#include "common.h"
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

static int decrunch_xfd(FILE *f1, FILE *f2)
{
    struct xfdBufferInfo *xfdobj;
    uint8 *packed;
    int plen,ret=-1;
    struct stat st;

    if (xfdMasterBase == NULL)
	return -1;

    if (f2 == NULL)
	return -1;

    fstat(fileno(f1), &st);
    plen = st.st_size;

    packed = AllocVec(plen,MEMF_CLEAR);
    if (!packed) return -1;

    fread(packed,plen,1,f1);

	xfdobj = (struct xfdBufferInfo *) xfdAllocObject(XFDOBJ_BUFFERINFO);
	if(xfdobj)
	{
		xfdobj->xfdbi_SourceBufLen = plen;
		xfdobj->xfdbi_SourceBuffer = packed;
		xfdobj->xfdbi_Flags = XFDFF_RECOGEXTERN | XFDFF_RECOGTARGETLEN;
		/* xfdobj->xfdbi_PackerFlags = XFDPFF_RECOGLEN; */
		if(xfdRecogBuffer(xfdobj))
		{
			xfdobj->xfdbi_TargetBufMemType = MEMF_ANY;
			if(xfdDecrunchBuffer(xfdobj))
			{
				if(fwrite(xfdobj->xfdbi_TargetBuffer,1,xfdobj->xfdbi_TargetBufSaveLen,f2) == xfdobj->xfdbi_TargetBufSaveLen) ret=0;
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
	decrunch_xfd
};

#endif /* AMIGA */
