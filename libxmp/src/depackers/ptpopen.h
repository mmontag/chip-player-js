#ifndef _PT_POPEN_H
#define _PT_POPEN_H 1

#ifdef _WIN32

#include <stdio.h>

#undef  popen
#define popen(cmd, mode) pt_popen(cmd, mode, NULL)
#undef  pclose
#define pclose(f) pt_pclose(f, NULL)

#ifdef __cplusplus
extern "C" {
#endif

struct pt_popen_data;

FILE *	pt_popen(const char *cmd, const char *mode, struct pt_popen_data **data);
int	pt_pclose(FILE *fle, struct pt_popen_data **data);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* _PT_POPEN_H */
