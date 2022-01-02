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

#ifdef __amigaos4__
#define __USE_INLINE__
#endif
#include <proto/exec.h>
#include <proto/xfdmaster.h>
#include <exec/types.h>

#if defined(__amigaos4__) || defined(__MORPHOS__)
struct Library *xfdMasterBase;
#else
struct xfdMasterBase *xfdMasterBase;
#endif

#ifdef __amigaos4__
struct xfdMasterIFace *IxfdMaster;
/*struct ExecIFace *IExec;*/
#endif

#ifdef __GNUC__
void INIT_8_open_xfd(void) __attribute__ ((constructor));
void EXIT_8_close_xfd(void) __attribute__ ((destructor));
#endif
#ifdef __VBCC__
#define INIT_8_open_xfd _INIT_8_open_xfd
#define EXIT_8_close_xfd _EXIT_8_close_xfd
#endif

void EXIT_8_close_xfd(void) {
#ifdef __amigaos4__
	if (IxfdMaster) {
		DropInterface((struct Interface *) IxfdMaster);
		IxfdMaster = NULL;
	}
#endif
	if (xfdMasterBase) {
		CloseLibrary((struct Library *) xfdMasterBase);
		xfdMasterBase = NULL;
	}
}

void INIT_8_open_xfd(void) {
#ifdef __amigaos4__
	/*IExec = (struct ExecIFace *)(*(struct ExecBase **)4)->MainInterface;*/
#endif

#if defined(__amigaos4__) || defined(__MORPHOS__)
	xfdMasterBase = OpenLibrary("xfdmaster.library",38);
#else
	xfdMasterBase = (struct xfdMasterBase *) OpenLibrary("xfdmaster.library",38);
#endif
	if (!xfdMasterBase) return;

#ifdef __amigaos4__
	IxfdMaster = (struct xfdMasterIFace *) GetInterface(xfdMasterBase,"main",1,NULL);
	if (!IxfdMaster) {
		CloseLibrary(xfdMasterBase);
		xfdMasterBase = NULL;
	}
#endif
}

#endif /* AMIGA */
