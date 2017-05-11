#ifndef _RAR_RESOURCE_
#define _RAR_RESOURCE_

#if defined(SILENT) && defined(RARDLL)
#define St(x) ("")
#else
const char *St(MSGID StringId);
#endif


#endif
