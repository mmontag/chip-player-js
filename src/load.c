/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __native_client__
#include <sys/syslimits.h>
#else
#include <limits.h>
#endif

#if !defined(HAVE_POPEN) && defined(WIN32)
#include "ptpopen.h"
#endif

#include "convert.h"
#include "format.h"
#include "synth.h"

#include "list.h"


extern struct format_loader *format_loader[];


LIST_HEAD(tmpfiles_list);

struct tmpfilename {
	char *name;
	struct list_head list;
};

int decrunch_arc	(FILE *, FILE *);
int decrunch_arcfs	(FILE *, FILE *);
int decrunch_sqsh	(FILE *, FILE *);
int decrunch_pp		(FILE *, FILE *);
int decrunch_mmcmp	(FILE *, FILE *);
int decrunch_muse	(FILE *, FILE *);
int decrunch_lzx	(FILE *, FILE *);
int decrunch_oxm	(FILE *, FILE *);
int decrunch_xfd	(FILE *, FILE *);
int decrunch_s404	(FILE *, FILE *);
int test_oxm		(FILE *);
char *test_xfd		(unsigned char *, int);

#define BUILTIN_PP	0x01
#define BUILTIN_SQSH	0x02
#define BUILTIN_MMCMP	0x03
#define BUILTIN_ARC	0x05
#define BUILTIN_ARCFS	0x06
#define BUILTIN_S404	0x07
#define BUILTIN_OXM	0x08
#define BUILTIN_XFD	0x09
#define BUILTIN_MUSE	0x0a
#define BUILTIN_LZX	0x0b


#if defined __EMX__ || defined WIN32
#define REDIR_STDERR "2>NUL"
#elif defined unix || defined __unix__
#define REDIR_STDERR "2>/dev/null"
#else
#define REDIR_STDERR
#endif

#define DECRUNCH_MAX 5 /* don't recurse more than this */

static int decrunch(struct context_data *ctx, FILE **f, char **s, int ttl)
{
    unsigned char b[1024];
    char *cmd;
    FILE *t;
    int fd, builtin, res;
    char *packer, *temp2, tmp[PATH_MAX];
    struct tmpfilename *temp;
    int headersize;

    packer = cmd = NULL;
    builtin = res = 0;

    if (get_temp_dir(tmp, PATH_MAX) < 0)
	return 0;

    strncat(tmp, "xmp_XXXXXX", PATH_MAX);

    fseek(*f, 0, SEEK_SET);
    if ((headersize = fread(b, 1, 1024, *f)) < 100)	/* minimum valid file size */
	return 0;

#if defined __AMIGA__ && !defined __AROS__
    if (packer = test_xfd(b, 1024)) {
	builtin = BUILTIN_XFD;
    } else
#endif

    if (b[0] == 'P' && b[1] == 'K' &&
	((b[2] == 3 && b[3] == 4) || (b[2] == '0' && b[3] == '0' &&
	b[4] == 'P' && b[5] == 'K' && b[6] == 3 && b[7] == 4))) {

	packer = "Zip";
#if defined WIN32
	cmd = "unzip -pqqC \"%s\" -x readme *.diz *.nfo *.txt *.exe *.com "
		"README *.DIZ *.NFO *.TXT *.EXE *.COM " REDIR_STDERR;
#else
	cmd = "unzip -pqqC \"%s\" -x readme '*.diz' '*.nfo' '*.txt' '*.exe' "
		"'*.com' README '*.DIZ' '*.NFO' '*.TXT' '*.EXE' '*.COM' "
		REDIR_STDERR;
#endif
    } else if (b[2] == '-' && b[3] == 'l' && b[4] == 'h') {
	packer = "LHa";
#if defined __EMX__
	fprintf( stderr, "LHA for OS/2 does NOT support output to stdout.\n" );
#elif defined __AMIGA__
	cmd = "lha p -q \"%s\"";
#else
	cmd = "lha -pq \"%s\"";
#endif
    } else if (b[0] == 31 && b[1] == 139) {
	packer = "gzip";
	cmd = "gzip -dc \"%s\"";
    } else if (b[0] == 'B' && b[1] == 'Z' && b[2] == 'h') {
	packer = "bzip2";
	cmd = "bzip2 -dc \"%s\"";
    } else if (b[0] == 0x5d && b[1] == 0 && b[2] == 0 && b[3] == 0x80) {
	packer = "lzma";
	cmd = "lzma -dc \"%s\"";
    } else if (b[0] == 0xfd && b[3] == 'X' && b[4] == 'Z' && b[5] == 0x00) {
	packer = "xz";
	cmd = "xz -dc \"%s\"";
    } else if (b[0] == 'Z' && b[1] == 'O' && b[2] == 'O' && b[3] == ' ') {
	packer = "zoo";
	cmd = "zoo xpq \"%s\"";
    } else if (b[0] == 'M' && b[1] == 'O' && b[2] == '3') {
	packer = "MO3";
	cmd = "unmo3 -s \"%s\" STDOUT";
    } else if (headersize > 300 &&
	       b[257] == 'u' && b[258] == 's' && b[259] == 't' &&
	       b[260] == 'a' && b[261] == 'r' &&
	       ((b[262] == 0) || (b[262] == ' ' && b[263] == ' ' &&
				  b[264] == 0))) {
	packer = "tar";
	cmd = "tar -xOf \"%s\"";
    } else if (b[0] == 31 && b[1] == 157) {
	packer = "compress";
#ifdef __EMX__
	fprintf( stderr, "I was unable to find a OS/2 version of UnCompress...\n" );
	fprintf( stderr, "I hope that the default command will work if a OS/2 version is found/created!\n" );
#endif
	cmd = "uncompress -c \"%s\"";
    } else if (memcmp(b, "PP20", 4) == 0) {
	packer = "PowerPack";
	builtin = BUILTIN_PP;
    } else if (memcmp(b, "XPKF", 4) == 0 && memcmp(b + 8, "SQSH", 4) == 0) {
	packer = "SQSH";
	builtin = BUILTIN_SQSH;
    } else if (!memcmp(b, "Archive\0", 8)) {
	packer = "ArcFS";
	builtin = BUILTIN_ARCFS;
    } else if (memcmp(b, "ziRCONia", 8) == 0) {
	packer = "MMCMP";
	builtin = BUILTIN_MMCMP;
    } else if (memcmp(b, "MUSE", 4) == 0 && readmem32b(b + 4) == 0xdeadbeaf) {
	packer = "J2B MUSE";
	builtin = BUILTIN_MUSE;
    } else if (memcmp(b, "MUSE", 4) == 0 && readmem32b(b + 4) == 0xdeadbabe) {
	packer = "MOD2J2B MUSE";
	builtin = BUILTIN_MUSE;
    } else if (memcmp(b, "LZX", 3) == 0) {
	packer = "LZX";
	builtin = BUILTIN_LZX;
    } else if (memcmp(b, "Rar", 3) == 0) {
	packer = "rar";
	cmd = "unrar p -inul -xreadme -x*.diz -x*.nfo -x*.txt "
	    "-x*.exe -x*.com \"%s\"";
    } else if (memcmp(b, "S404", 4) == 0) {
	packer = "Stonecracker";
	builtin = BUILTIN_S404;
#if !defined WIN32 && !defined __AMIGA__ && !defined __native_client__
    } else if (test_oxm(*f) == 0) {
	packer = "oggmod";
	builtin = BUILTIN_OXM;
#endif
    }

    if (packer == NULL && b[0] == 0x1a) {
	int x = b[1] & 0x7f;
	int i, flag = 0;
	long size;
	
	/* check file name */
	for (i = 0; i < 13; i++) {
	    if (b[2 + i] == 0) {
		if (i == 0)		/* name can't be empty */
		    flag = 1;
		break;
	    }
	    if (!isprint(b[2 + i])) {	/* name must be printable */
		flag = 1;
		break;
	    }
	}

	size = readmem32l(b + 15);	/* max file size is 512KB */
	if (size < 0 || size > 512 * 1024)
		flag = 1;

        if (flag == 0) {
	    if (x >= 1 && x <= 9 && x != 7) {
		packer = "Arc";
		builtin = BUILTIN_ARC;
	    } else if (x == 0x7f) {
		packer = "!Spark";
		builtin = BUILTIN_ARC;
	    }
	}
    }

    fseek(*f, 0, SEEK_SET);

    if (packer == NULL)
	return 0;

#if defined ANDROID || defined __native_client__
    /* Don't use external helpers in android */
    if (cmd)
	return 0;
#endif

    _D(_D_WARN "Depacking %s file... ", packer);

    temp = calloc(sizeof (struct tmpfilename), 1);
    if (!temp) {
	_D(_D_CRIT "calloc failed");
	return -1;
    }

    temp->name = strdup(tmp);
    if ((fd = mkstemp(temp->name)) < 0) {
	_D(_D_CRIT "failed");
	return -1;
    }

    list_add_tail(&temp->list, &tmpfiles_list);

    if ((t = fdopen(fd, "w+b")) == NULL) {
	_D(_D_CRIT "failed");
	return -1;
    }

    if (cmd) {
#define BSIZE 0x4000
	int n;
	char line[1024], buf[BSIZE];
	FILE *p;

	snprintf(line, 1024, cmd, *s);

#ifdef WIN32
	/* Note: The _popen function returns an invalid file handle, if
	 * used in a Windows program, that will cause the program to hang
	 * indefinitely. _popen works properly in a Console application.
	 * To create a Windows application that redirects input and output,
	 * read the section "Creating a Child Process with Redirected Input
	 * and Output" in the Win32 SDK. -- Mirko 
	 */
	if ((p = popen(line, "rb")) == NULL) {
#else
	/* Linux popen fails with "rb" */
	if ((p = popen(line, "r")) == NULL) {
#endif
	    _D(_D_CRIT "failed");
	    fclose(t);
	    return -1;
	}
	while ((n = fread(buf, 1, BSIZE, p)) > 0)
	    fwrite(buf, 1, n, t);
	pclose (p);
    } else {
	switch (builtin) {
	case BUILTIN_PP:    
	    res = decrunch_pp(*f, t);
	    break;
	case BUILTIN_ARC:
	    res = decrunch_arc(*f, t);
	    break;
	case BUILTIN_ARCFS:
	    res = decrunch_arcfs(*f, t);
	    break;
	case BUILTIN_SQSH:    
	    res = decrunch_sqsh(*f, t);
	    break;
	case BUILTIN_MMCMP:    
	    res = decrunch_mmcmp(*f, t);
	    break;
	case BUILTIN_MUSE:    
	    res = decrunch_muse(*f, t);
	    break;
	case BUILTIN_LZX:    
	    res = decrunch_lzx(*f, t);
	    break;
	case BUILTIN_S404:
	    res = decrunch_s404(*f, t);
	    break;
#if !defined WIN32 && !defined __MINGW32__ && !defined __AMIGA__ && !defined __native_client__
	case BUILTIN_OXM:
	    res = decrunch_oxm(*f, t);
	    break;
#endif
#ifdef AMIGA
	case BUILTIN_XFD:
	    res = decrunch_xfd(*f, t);
	    break;
#endif
	}
    }

    if (res < 0) {
	_D(_D_CRIT "failed");
	fclose(t);
	return -1;
    }

    _D(_D_INFO "done");

    fclose(*f);
    *f = t;
 
    if (!--ttl) {
	    return -1;
    }
    
    temp2 = strdup(temp->name);
    res = decrunch(ctx, f, &temp->name, ttl);
    unlink(temp2);
    free(temp2);
    /* Mirko: temp is now deallocated in unlink_tempfiles()
     * not a problem, since unlink_tempfiles() is called after decrunch
     * in loader routines
     *
     * free(temp);
     */

    return res;
}


/*
 * Windows doesn't allow you to unlink an open file, so we changed the
 * temp file cleanup system to remove temporary files after we close it
 */
static void unlink_tempfiles(void)
{
	struct tmpfilename *li;
	struct list_head *tmp;

	/* can't use list_for_each when freeing the node! */
	for (tmp = (&tmpfiles_list)->next; tmp != (&tmpfiles_list); ) {
		li = list_entry(tmp, struct tmpfilename, list);
		_D(_D_INFO "unlink tmpfile %s", li->name);
		unlink(li->name);
		free(li->name);
		list_del(&li->list);
		tmp = tmp->next;
		free(li);
	}
}


int xmp_test_module(xmp_context ctx, char *path, struct xmp_test_info *info)
{
	FILE *f;
	struct stat st;
	char buf[XMP_NAME_SIZE];
	int i;

	if ((f = fopen(path, "rb")) == NULL)
		return -1;

	if (fstat(fileno(f), &st) < 0)
		goto err;

	if (S_ISDIR(st.st_mode))
		goto err;

	if (decrunch((struct context_data *)ctx, &f, &path, DECRUNCH_MAX) < 0)
		goto err;

	if (fstat(fileno(f), &st) < 0)	/* get size after decrunch */
		goto err;

	if (st.st_size < 500)		/* set minimum valid module size */
		goto err;

	if (info != NULL) {
		*info->name = 0;	/* reset name prior to testing */
		*info->type = 0;	/* reset name prior to testing */
	}

	for (i = 0; format_loader[i] != NULL; i++) {
		fseek(f, 0, SEEK_SET);
		if (format_loader[i]->test(f, buf, 0) == 0) {
			fclose(f);
			unlink_tempfiles();
			if (info != NULL) {
				strncpy(info->name, buf, XMP_NAME_SIZE);
				strncpy(info->type, format_loader[i]->name,
							XMP_NAME_SIZE);
			}
			return 0;
		}
	}

      err:
	fclose(f);
	unlink_tempfiles();
	return -1;
}


/* FIXME: ugly code, check allocations */
static void split_name(char *s, char **d, char **b)
{
	char tmp, *div;

	_D("alloc dirname/basename");
	if ((div = strrchr(s, '/'))) {
		tmp = div - s + 1;
		*d = malloc(tmp + 1);
		memcpy(*d, s, tmp);
		(*d)[tmp + 1] = 0;
		*b = strdup(div + 1);
	} else {
		*d = strdup("");
		*b = strdup(s);
	}
}

/**
 * @brief Load module
 *
 * Load module into the player context referenced by handle.
 *
 * @param handle Player context handle.
 * @param path Module file name.
 *
 * @return 0 if the module is successfully loaded, -1 otherwise.
 */
int xmp_load_module(xmp_context handle, char *path)
{
    struct context_data *ctx = (struct context_data *)handle;
    FILE *f;
    int i, t, val;
    struct stat st;
    struct module_data *m = &ctx->m;

    _D(_D_WARN "path = %s", path);

    if ((f = fopen(path, "rb")) == NULL)
	return -1;

    if (fstat(fileno (f), &st) < 0)
	goto err;

    if (S_ISDIR(st.st_mode))
	goto err;

    _D(_D_INFO "decrunch");
    if ((t = decrunch((struct context_data *)ctx, &f, &path, DECRUNCH_MAX)) < 0)
	goto err;

    if (fstat(fileno(f), &st) < 0)	/* get size after decrunch */
	goto err;

    split_name(path, &m->dirname, &m->basename);

    /* Reset variables */
    memset(m->mod.name, 0, XMP_NAME_SIZE);
    memset(m->mod.type, 0, XMP_NAME_SIZE);
    /* memset(m->author, 0, XMP_NAME_SIZE); */
    m->filename = path;		/* For ALM, SSMT, etc */
    m->size = st.st_size;
    m->rrate = PAL_RATE;
    m->c4rate = C4_PAL_RATE;
    m->volbase = 0x40;
    m->vol_table = NULL;
    /* Reset control for next module */
    m->quirk = 0;
    m->comment = NULL;

    /* Set defaults */
    m->mod.tpo = 6;
    m->mod.bpm = 125;
    m->mod.chn = 4;
    m->synth = &synth_null;
    m->extra = NULL;
    m->time_factor = DEFAULT_TIME_FACTOR;

    for (i = 0; i < 64; i++) {
	m->mod.xxc[i].pan = (((i + 1) / 2) % 2) * 0xff;
	m->mod.xxc[i].vol = 0x40;
	m->mod.xxc[i].flg = 0;
    }

    _D(_D_WARN "load");
    val = 0;
    for (i = 0; format_loader[i] != NULL; i++) {
	fseek(f, 0, SEEK_SET);
	val = format_loader[i]->test(f, NULL, 0);
   	if (val == 0) {
	    fseek(f, 0, SEEK_SET);
	    _D(_D_WARN "load format: %s", format_loader[i]->name);

	    val = format_loader[i]->loader(m, f, 0);
	    if (val != 0) {
		_D(_D_CRIT "can't load module, possibly corrupted file");
	    }
	    break;
	}
    }

    fclose(f);
    unlink_tempfiles();

    if (val < 0) {
	free(m->basename);
	free(m->dirname);
	return val;
    }

    /* Fix cases where the restart value is invalid e.g. kc_fall8.xm
     * from http://aminet.net/mods/mvp/mvp_0002.lha (reported by
     * Ralf Hoffmann <ralf@boomerangsworld.de>)
     */
    if (m->mod.rst >= m->mod.len)
	m->mod.rst = 0;

    str_adj(m->mod.name);
    if (!*m->mod.name) {
	strncpy(m->mod.name, m->basename, XMP_NAME_SIZE);
    }

    return 0;

err:
    fclose(f);
    unlink_tempfiles();
    return -1;
}


/**
 * @brief Free current module data
 *
 * Release all data from the currently loaded module in the given player
 * context.
 *
 * @param handle Player context handle.
 */
void xmp_release_module(xmp_context handle)
{
	struct context_data *ctx = (struct context_data *)handle;
	struct module_data *m = &ctx->m;
	int i;

	_D(_D_INFO "Freeing memory");

	if (m->extra)
		free(m->extra);

	if (m->med_vol_table) {
		for (i = 0; i < m->mod.ins; i++)
			if (m->med_vol_table[i])
				free(m->med_vol_table[i]);
		free(m->med_vol_table);
	}

	if (m->med_wav_table) {
		for (i = 0; i < m->mod.ins; i++)
			if (m->med_wav_table[i])
				free(m->med_wav_table[i]);
		free(m->med_wav_table);
	}

	for (i = 0; i < m->mod.trk; i++)
		free(m->mod.xxt[i]);

	for (i = 0; i < m->mod.pat; i++)
		free(m->mod.xxp[i]);

	for (i = 0; i < m->mod.ins; i++)
		free(m->mod.xxi[i].sub);

	free(m->mod.xxt);
	free(m->mod.xxp);
	if (m->mod.smp > 0) {
		for (i = 0; i < m->mod.smp; i++) {
			if (m->mod.xxs[i].data != NULL)
				free(m->mod.xxs[i].data);
		}
		free(m->mod.xxs);
	}
	free(m->mod.xxi);
	if (m->comment)
		free(m->comment);

	_D("free dirname/basename");
	free(m->dirname);
	free(m->basename);
}
