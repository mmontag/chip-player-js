/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#if !defined _WIN32 && !defined __QNXNTO__ && !defined B_BEOS_VERSION

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>

#include <sys/ipc.h>
#ifndef __EMX__
#  include <sys/shm.h>
#endif

#include <unistd.h>
#include "xmpi.h"

#ifdef __EMX__

#define INCL_DOSMEMMGR
#include <os2.h>
#include <io.h>

static char *os2mem;

int shmget(int flag1, int amt, int flag2)
{
	DosAllocSharedMem((PVOID *) & os2mem, (PSZ) NULL, amt,
			  fALLOC | OBJ_GETTABLE);
	return (1);
}

char *shmat(int id, int a, int b)
{
	return (os2mem);
}

void shmctl(int id, int flag, char *tmp)
{
	return;
}

void shmdt(char *memptr)
{
	DosFreeMem(memptr);
}

#endif

/* shared memory */
static int shmid;

void *xmp_get_shared_mem(int n)
{
	char *p;

	if ((shmid = shmget(IPC_PRIVATE, n, IPC_CREAT | 0600)) == -1)
		return NULL;

	p = shmat(shmid, 0, 0);
	memset(p, 0, n);

	return p;
}

void xmp_detach_shared_mem(void *p)
{
	shmctl(shmid, IPC_RMID, NULL);
	shmdt(p);
}

/* pipes for parent-child synchronization */
static int pfd1[2], pfd2[2];

int xmpi_tell_wait()
{
	return (pipe(pfd1) || pipe(pfd2));
}

int xmpi_select_read(int fd, int msec)
{
	fd_set rfds;
	struct timeval tv;

	tv.tv_sec = msec / 1000;
	tv.tv_usec = (msec % 1000) * 1000;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	return select(fd + 1, &rfds, NULL, NULL, &tv);
}

int xmp_tell_parent()
{
	return write(pfd2[1], "c", 1) != 1;
}

int xmp_wait_parent()
{
	char c;

	return !((read(pfd1[0], &c, 1) == 1) && (c == 'p'));
}

int xmp_check_parent(int msec)
{
	return xmpi_select_read(pfd1[0], msec);
}

int xmp_tell_child()
{
	return write(pfd1[1], "p", 1) != 1;
}

int xmp_wait_child()
{
	char c;

	return !((read(pfd2[0], &c, 1) == 1) && (c == 'c'));
}

int xmp_check_child(int msec)
{
	return xmpi_select_read(pfd2[0], msec);
}

#else

int xmpi_select_read(int fd, int msec)
{
	return 0;
}

int xmpi_tell_wait()
{
	return 0;
}

#endif
