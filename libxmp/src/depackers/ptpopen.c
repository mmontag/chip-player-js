/*
 * pt_popen/pt_pclose functions
 * Written somewhere in the 90s by Kurt Keller
 * Comments translated by Steve Donovan
 * Modified for use in xmp by Mirko Buffoni and Claudio Matsuoka
 * Reentrancy patch added for xmp by Alice Rowan
 */

/*
 * This piece of code is in the public domain. I do not claim any rights
 * on it. Do whatever you want to do with it and I hope it will be still
 * useful. -- Kurt Keller, Aug 2013
 */

#ifdef _WIN32

#include "ptpopen.h"

/*
> Hello,
>      I am currently porting a UNIX program to WINDOWS.
> Most difficulty time I have is to find the popen()-like function under
> WINDOWS. Any help and hints would be greatly appreciated.
>
> Thanks in advance
> Tianlin Wang

This is what I use instead of popen(): (Sorry for the comments in german ;-))
It is not an **EXACT** replacement for popen() but it is OK for me.

Kurt.

---------------------------------------------------
Tel.:   (49)7150/393394    Parity Software GmbH
Fax.:   (49)7150/393351    Stuttgarter Strasse 42/3
E-Mail: kk@parity-soft.de  D-71701 Schwieberdingen
Web:    www.parity-soft.de
---------------------------------------------------
*/

/*----------------------------------------------------------------------------
  Globals for the Routines pt_popen() / pt_pclose()
----------------------------------------------------------------------------*/

#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <errno.h>

#if defined(_MSC_VER) && (_MSC_VER < 1300)
typedef LONG LONG_PTR;
#endif

struct pt_popen_data
{
  HANDLE pipein[2];
  HANDLE pipeout[2];
  HANDLE pipeerr[2];
  char popenmode;
  BOOL is_open;
};

static struct pt_popen_data static_data;

static int my_pipe(HANDLE *readwrite)
{
  SECURITY_ATTRIBUTES sa;

  sa.nLength = sizeof(sa);          /* Length in bytes */
  sa.bInheritHandle = 1;            /* the child must inherit these handles */
  sa.lpSecurityDescriptor = NULL;

  if (! CreatePipe (&readwrite[0],&readwrite[1],&sa,1 << 13))
  {
    errno = EMFILE;
    return -1;
  }

  return 0;
}


/*----------------------------------------------------------------------------
  Replacement for 'popen()' under WIN32.
  NOTE: if cmd contains '2>&1', we connect the standard error file handle
    to the standard output file handle.
  NOTE: a pointer to allocate a pt_popen_data struct to may be provided. If
    this pointer is NULL, a static (non-reentrant) struct will be used instead.
----------------------------------------------------------------------------*/
FILE * pt_popen(const char *cmd, const char *mode, struct pt_popen_data **data)
{
  FILE *fptr = (FILE *)0;
  PROCESS_INFORMATION piProcInfo;
  STARTUPINFO siStartInfo;
  int success, umlenkung;
  struct pt_popen_data *my_data = &static_data;
  BOOL user_data = FALSE;

  if (data) {
    my_data = (struct pt_popen_data *) malloc(sizeof(struct pt_popen_data));
    if (!my_data)
      return NULL;

    user_data = TRUE;
  } else if (static_data.is_open) {
    return NULL;
  }

  my_data->pipein[0]   = INVALID_HANDLE_VALUE;
  my_data->pipein[1]   = INVALID_HANDLE_VALUE;
  my_data->pipeout[0]  = INVALID_HANDLE_VALUE;
  my_data->pipeout[1]  = INVALID_HANDLE_VALUE;
  my_data->pipeerr[0]  = INVALID_HANDLE_VALUE;
  my_data->pipeerr[1]  = INVALID_HANDLE_VALUE;
  my_data->is_open     = TRUE;

  if (!mode || !*mode)
    goto finito;

  my_data->popenmode = *mode;
  if (my_data->popenmode != 'r' && my_data->popenmode != 'w')
    goto finito;

  /*
   * Shall we redirect stderr to stdout ? */
  umlenkung = strstr("2>&1",(char *)cmd) != 0;

  /*
   * Create the Pipes... */
  if (my_pipe(my_data->pipein)  == -1 ||
      my_pipe(my_data->pipeout) == -1)
    goto finito;
  if (!umlenkung && my_pipe(my_data->pipeerr) == -1)
    goto finito;

  /*
   * Now create the child process */
  ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
  siStartInfo.cb           = sizeof(STARTUPINFO);
  siStartInfo.hStdInput    = my_data->pipein[0];
  siStartInfo.hStdOutput   = my_data->pipeout[1];
  if (umlenkung)
    siStartInfo.hStdError  = my_data->pipeout[1];
  else
    siStartInfo.hStdError  = my_data->pipeerr[1];
  siStartInfo.dwFlags    = STARTF_USESTDHANDLES;

  success = CreateProcess(NULL,
     (LPTSTR)cmd,       // command line
     NULL,              // process security attributes
     NULL,              // primary thread security attributes
     TRUE,              // handles are inherited
     DETACHED_PROCESS,  // creation flags: without window (?)
     NULL,              // use parent's environment
     NULL,              // use parent's current directory
     &siStartInfo,      // STARTUPINFO pointer
     &piProcInfo);      // receives PROCESS_INFORMATION

  if (!success)
    goto finito;

  /*
   * These handles listen to the child process */
  CloseHandle(my_data->pipein[0]);  my_data->pipein[0]  = INVALID_HANDLE_VALUE;
  CloseHandle(my_data->pipeout[1]); my_data->pipeout[1] = INVALID_HANDLE_VALUE;
  CloseHandle(my_data->pipeerr[1]); my_data->pipeerr[1] = INVALID_HANDLE_VALUE;

  if (my_data->popenmode == 'r')
    fptr = _fdopen(_open_osfhandle((LONG_PTR)my_data->pipeout[0],_O_BINARY),"r");
  else
    fptr = _fdopen(_open_osfhandle((LONG_PTR)my_data->pipein[1],_O_BINARY),"w");

finito:
  if (!fptr)
  {
    if (my_data->pipein[0]  != INVALID_HANDLE_VALUE)
      CloseHandle(my_data->pipein[0]);
    if (my_data->pipein[1]  != INVALID_HANDLE_VALUE)
      CloseHandle(my_data->pipein[1]);
    if (my_data->pipeout[0] != INVALID_HANDLE_VALUE)
      CloseHandle(my_data->pipeout[0]);
    if (my_data->pipeout[1] != INVALID_HANDLE_VALUE)
      CloseHandle(my_data->pipeout[1]);
    if (my_data->pipeerr[0] != INVALID_HANDLE_VALUE)
      CloseHandle(my_data->pipeerr[0]);
    if (my_data->pipeerr[1] != INVALID_HANDLE_VALUE)
      CloseHandle(my_data->pipeerr[1]);
    my_data->is_open = FALSE;

    if (user_data)
    {
      free(my_data);
      my_data = NULL;
    }
  }
  if (user_data)
    *data = my_data;
  return fptr;
}

/*----------------------------------------------------------------------------
  Replacement for 'pclose()' under WIN32
----------------------------------------------------------------------------*/
int pt_pclose(FILE *fle, struct pt_popen_data **data)
{
  struct pt_popen_data *my_data = &static_data;
  BOOL free_data = FALSE;
  if (data)
  {
    if (!*data)
      return -1;

    my_data = *data;
    free_data = TRUE;
  }

  if (fle && my_data->is_open)
  {
    (void)fclose(fle);

    CloseHandle(my_data->pipeerr[0]);
    if (my_data->popenmode == 'r')
      CloseHandle(my_data->pipein[1]);
    else
      CloseHandle(my_data->pipeout[0]);

    if (free_data)
    {
      free(my_data);
      *data = NULL;
    }
    return 0;
  }
  return -1;
}

#endif /* WIN32 */
