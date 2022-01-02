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

#include <errno.h>

#include "../common.h"
#include "depacker.h"
#include "../hio.h"
#include "../tempfile.h"
#include "xfnmatch.h"

#ifdef _WIN32
/* Note: The _popen function returns an invalid file opaque, if
 * used in a Windows program, that will cause the program to hang
 * indefinitely. _popen works properly in a Console application.
 * To create a Windows application that redirects input and output,
 * read the section "Creating a Child Process with Redirected Input
 * and Output" in the Win32 SDK. -- Mirko
 *
 * This popen reimplementation uses CreateProcess instead and should be safe.
 */
#include "ptpopen.h"
#ifndef HAVE_POPEN
#define HAVE_POPEN 1
#endif

#elif defined(__WATCOMC__)

#define popen  _popen
#define pclose _pclose
#define HAVE_POPEN 1

#endif

#define BUFLEN 16384

static struct depacker *depacker_list[] = {
#if defined(LIBXMP_AMIGA) && defined(HAVE_PROTO_XFDMASTER_H)
	&libxmp_depacker_xfd,
#endif
	&libxmp_depacker_zip,
	&libxmp_depacker_lha,
	&libxmp_depacker_gzip,
	&libxmp_depacker_bzip2,
	&libxmp_depacker_xz,
	&libxmp_depacker_compress,
	&libxmp_depacker_pp,
	&libxmp_depacker_sqsh,
	&libxmp_depacker_arcfs,
	&libxmp_depacker_mmcmp,
	&libxmp_depacker_muse,
	&libxmp_depacker_lzx,
	&libxmp_depacker_s404,
	&libxmp_depacker_arc,
	NULL
};

#if defined(HAVE_FORK) && defined(HAVE_PIPE) && defined(HAVE_EXECVP) && \
    defined(HAVE_DUP2) && defined(HAVE_WAIT)
#define DECRUNCH_USE_FORK

#elif defined(HAVE_POPEN) && \
    (defined(_WIN32) || defined(__OS2__) || defined(__EMX__) || defined(__DJGPP__) || defined(__riscos__))
#define DECRUNCH_USE_POPEN

#else
static int execute_command(const char * const cmd[], FILE *t) {
	return -1;
}
#endif

#ifdef DECRUNCH_USE_POPEN
/* TODO: this may not be safe outside of _WIN32 (which uses CreateProcess). */
static int execute_command(const char * const cmd[], FILE *t)
{
#ifdef _WIN32
	struct pt_popen_data *popen_data;
#endif
	char line[1024], buf[BUFLEN];
	FILE *p;
	int pos;
	int n;

	/* Collapse command array into a command line for popen. */
	for (n = 0, pos = 0; cmd[n]; n++) {
		int written = snprintf(line + pos, sizeof(line) - pos, n ? "\"%s\" " : "%s ", cmd[n]);
		pos += written;
		if (pos >= sizeof(line)) {
			D_(D_CRIT "popen command line exceeded buffer size");
			return -1;
		}
	}
	line[sizeof(line) - 1] = '\0';

	D_(D_INFO "popen(%s)", line);

#ifdef _WIN32
	p = pt_popen(line, "rb", &popen_data);
#else
	p = popen(line, "rb");
#endif

	if (p == NULL) {
		D_(D_CRIT "failed popen");
		return -1;
	}

	while ((n = fread(buf, 1, BUFLEN, p)) > 0) {
		fwrite(buf, 1, n, t);
	}

#ifdef _WIN32
	pt_pclose(p, &popen_data);
#else
	pclose(p);
#endif
	return 0;
}
#endif /* USE_PTPOPEN */

#ifdef DECRUNCH_USE_FORK
#include <sys/wait.h>
#include <unistd.h>

static int execute_command(const char * const cmd[], FILE *t)
{
	/* Use pipe/fork/execvp to avoid shell injection vulnerabilities. */
	char buf[BUFLEN];
	FILE *p;
	int n;
	int fds[2];
	pid_t pid;
	int status;

	D_(D_INFO "fork/execvp(%s...)", cmd[0]);

	if (pipe(fds) < 0) {
		D_(D_CRIT "failed pipe");
		return -1;
	}
	if ((pid = fork()) < 0) {
		D_(D_CRIT "failed fork");
		close(fds[0]);
		close(fds[1]);
		return -1;
	}
	if (pid == 0) {
		dup2(fds[1], STDOUT_FILENO);
		close(fds[0]);
		close(fds[1]);
		/* argv param isn't const char * const * for some reason but
		 * exec* only copies the provided arguments. */
		execvp(cmd[0], (char * const *)cmd);
		exit(errno);
	}
	close(fds[1]);
	wait(&status);
	if (!WIFEXITED(status)) {
		D_(D_CRIT "process failed (wstatus = %d)", status);
		close(fds[0]);
		return -1;
	}
	if (WEXITSTATUS(status)) {
		D_(D_CRIT "process exited with status %d", WEXITSTATUS(status));
		close(fds[0]);
		return -1;
	}
	if ((p = fdopen(fds[0], "rb")) == NULL) {
		D_(D_CRIT "failed fdopen");
		close(fds[0]);
		return -1;
	}

	while ((n = fread(buf, 1, BUFLEN, p)) > 0) {
		fwrite(buf, 1, n, t);
	}

	fclose(p);
	return 0;
}
#endif /* USE_FORK */

static int decrunch_command(HIO_HANDLE **h, const char * const cmd[], char **temp)
{
#if defined __ANDROID__ || defined __native_client__
	/* Don't use external helpers in android */
	return 0;
#else
	HIO_HANDLE *tmp;
	FILE *t;

	D_(D_WARN "Depacking file... ");

	if ((t = make_temp_file(temp)) == NULL) {
		goto err;
	}

	/* Depack file */
	D_(D_INFO "External depacker: %s", cmd[0]);
	if (execute_command(cmd, t) < 0) {
		D_(D_CRIT "failed");
		goto err2;
	}

	D_(D_INFO "done");

	if (fseek(t, 0, SEEK_SET) < 0) {
		D_(D_CRIT "fseek error");
		goto err2;
	}

	if ((tmp = hio_open_file2(t)) == NULL)
		return -1;  /* call closes on failure. */

	hio_close(*h);
	*h = tmp;
	return 0;

    err2:
	fclose(t);
    err:
	return -1;
#endif
}

static int decrunch_internal_tempfile(HIO_HANDLE **h, struct depacker *depacker, char **temp)
{
	HIO_HANDLE *tmp;
	FILE *t;

	D_(D_WARN "Depacking file... ");

	if ((t = make_temp_file(temp)) == NULL) {
		goto err;
	}

	/* Depack file */
	D_(D_INFO "Internal depacker");
	if (depacker->depack(*h, t, hio_size(*h)) < 0) {
		D_(D_CRIT "failed");
		goto err2;
	}

	D_(D_INFO "done");

	if (fseek(t, 0, SEEK_SET) < 0) {
		D_(D_CRIT "fseek error");
		goto err2;
	}

	if ((tmp = hio_open_file2(t)) == NULL)
		return -1; /* call closes on failure. */

	hio_close(*h);
	*h = tmp;
	return 0;

    err2:
	fclose(t);
    err:
	return -1;
}

static int decrunch_internal_memory(HIO_HANDLE **h, struct depacker *depacker)
{
	HIO_HANDLE *tmp;
	void *out;
	long outlen;

	D_(D_WARN "Depacking file... ");

	/* Depack file */
	D_(D_INFO "Internal depacker");
	if (depacker->depack_mem(*h, &out, hio_size(*h), &outlen) < 0) {
		D_(D_CRIT "failed");
		return -1;
	}

	D_(D_INFO "done");

	if ((tmp = hio_open_mem(out, outlen, 1)) == NULL) {
		free(out);
		return -1;
	}

	hio_close(*h);
	*h = tmp;
	return 0;
}

int libxmp_decrunch(HIO_HANDLE **h, const char *filename, char **temp)
{
	unsigned char b[1024];
	const char *cmd[32];
	int headersize;
	int i;
	struct depacker *depacker = NULL;

	cmd[0] = NULL;
	*temp = NULL;

	headersize = hio_read(b, 1, 1024, *h);
	if (headersize < 100) {	/* minimum valid file size */
		return 0;
	}

	/* Check built-in depackers */
	for (i = 0; depacker_list[i] != NULL; i++) {
		if (depacker_list[i]->test(b)) {
			depacker = depacker_list[i];
			D_(D_INFO "Use depacker %d", i);
			break;
		}
	}

	/* Check external commands */
	if (depacker == NULL) {
		if (b[0] == 'M' && b[1] == 'O' && b[2] == '3') {
			/* MO3 */
			D_(D_INFO "mo3");
			i = 0;
			cmd[i++] = "unmo3";
			cmd[i++] = "-s";
			cmd[i++] = filename;
			cmd[i++] = "STDOUT";
			cmd[i++] = NULL;
		} else if (memcmp(b, "Rar", 3) == 0) {
			/* rar */
			D_(D_INFO "rar");
			i = 0;
			cmd[i++] = "unrar";
			cmd[i++] = "p";
			cmd[i++] = "-inul";
			cmd[i++] = "-xreadme";
			cmd[i++] = "-x*.diz";
			cmd[i++] = "-x*.nfo";
			cmd[i++] = "-x*.txt";
			cmd[i++] = "-x*.exe";
			cmd[i++] = "-x*.com";
			cmd[i++] = filename;
			cmd[i++] = NULL;
		}
	}

	if (hio_seek(*h, 0, SEEK_SET) < 0) {
		return -1;
	}

	/* Depack file */
	if (cmd[0]) {
		/* When the filename is unknown (because it is a stream) don't use
		 * external helpers
		 */
		if (filename == NULL) {
			return 0;
		}

		return decrunch_command(h, cmd, temp);
	} else if (depacker && depacker->depack) {
		return decrunch_internal_tempfile(h, depacker, temp);
	} else if (depacker && depacker->depack_mem) {
		return decrunch_internal_memory(h, depacker);
	} else {
		D_(D_INFO "Not packed");
		return 0;
	}
}

/*
 * Check whether the given string matches one of the blacklisted glob
 * patterns. Used to filter file names stored in archive files.
 */
int libxmp_exclude_match(const char *name)
{
	int i;

	static const char *const exclude[] = {
		"README", "readme",
		"*.DIZ", "*.diz",
		"*.NFO", "*.nfo",
		"*.DOC", "*.Doc", "*.doc",
		"*.INFO", "*.info", "*.Info",
		"*.TXT", "*.txt",
		"*.EXE", "*.exe",
		"*.COM", "*.com",
		"*.README", "*.readme", "*.Readme", "*.ReadMe",
		NULL
	};

	for (i = 0; exclude[i] != NULL; i++) {
		if (fnmatch(exclude[i], name, 0) == 0) {
			return 1;
		}
	}

	return 0;
}
