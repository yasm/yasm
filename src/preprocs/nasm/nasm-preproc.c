/*
 * Imported NASM preprocessor - glue code
 *
 *  Copyright (C) 2002  Peter Johnson
 *
 *  This file is part of YASM.
 *
 *  YASM is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  YASM is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#include "errwarn.h"
#include "linemgr.h"

#include "preproc.h"
#include "nasm.h"
#include "nasmlib.h"
#include "nasm-pp.h"
#include "nasm-eval.h"

static FILE *in;
static linemgr *cur_lm;
static errwarn *cur_we;
static char *line, *linepos;
static size_t lineleft;
static char *file_name;
static long prior_linnum;
static int lineinc;
int tasm_compatible_mode = 0;


static void
nil_listgen_init(char *p, efunc e)
{
}

static void
nil_listgen_cleanup(void)
{
}

static void
nil_listgen_output(long v, const void *d, unsigned long v2)
{
}

static void
nil_listgen_line(int v, char *p)
{
}

static void
nil_listgen_uplevel(int v)
{
}

static void
nil_listgen_downlevel(int v)
{
}

static ListGen nil_list = {
    nil_listgen_init,
    nil_listgen_cleanup,
    nil_listgen_output,
    nil_listgen_line,
    nil_listgen_uplevel,
    nil_listgen_downlevel
};


static void
nasm_efunc(int severity, const char *fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    switch (severity & ERR_MASK) {
	case ERR_WARNING:
	    cur_we->warning_va(WARN_PREPROC, cur_lm->get_current(), fmt, va);
	    break;
	case ERR_NONFATAL:
	    cur_we->error_va(cur_lm->get_current(), fmt, va);
	    break;
	case ERR_FATAL:
	    cur_we->fatal(FATAL_UNKNOWN);   /* FIXME */
	    break;
	case ERR_PANIC:
	    cur_we->internal_error(fmt);    /* FIXME */
	    break;
	case ERR_DEBUG:
	    break;
    }
    va_end(va);
}

static void
nasm_preproc_initialize(FILE *f, const char *in_filename, linemgr *lm,
			errwarn *we)
{
    in = f;
    cur_lm = lm;
    cur_we = we;
    line = NULL;
    file_name = NULL;
    prior_linnum = 0;
    lineinc = 0;
    nasmpp.reset(f, in_filename, 2, nasm_efunc, nasm_evaluate, &nil_list);
}

static void
nasm_preproc_cleanup(void)
{
    nasmpp.cleanup(0);
}

static size_t
nasm_preproc_input(char *buf, size_t max_size)
{
    size_t tot = 0, n;
    long linnum = prior_linnum += lineinc;
    int altline;

    if (!line) {
	line = nasmpp.getline();
	if (!line)
	    return 0;
	linepos = line;
	lineleft = strlen(line) + 1;
	line[lineleft-1] = '\n';
    }

    altline = nasm_src_get(&linnum, &file_name);
    if (altline) {
	if (altline == 1 && lineinc == 1) {
	    *buf++ = '\n';
	    max_size--;
	    tot++;
	} else {
	    lineinc = (altline != -1 || lineinc != 1);
	    n = sprintf(buf, "%%line %ld+%d %s\n", linnum, lineinc, file_name);
	    buf += n;
	    max_size -= n;
	    tot += n;
	}
	prior_linnum = linnum;
    }

    n = lineleft<max_size?lineleft:max_size;
    strncpy(buf, linepos, n);
    tot += n;

    if (n == lineleft) {
	xfree(line);
	line = NULL;
    } else {
	lineleft -= n;
	linepos += n;
    }

    return tot;
}


/* Define preproc structure -- see preproc.h for details */
preproc yasm_nasm_LTX_preproc = {
    "Real NASM Preprocessor",
    "nasm",
    nasm_preproc_initialize,
    nasm_preproc_cleanup,
    nasm_preproc_input
};
