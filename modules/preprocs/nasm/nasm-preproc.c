/*
 * Imported NASM preprocessor - glue code
 *
 *  Copyright (C) 2002  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <util.h>
/*@unused@*/ RCSID("$IdPath$");

#define YASM_LIB_INTERNAL
#include <libyasm.h>

#include "nasm.h"
#include "nasmlib.h"
#include "nasm-pp.h"
#include "nasm-eval.h"

static FILE *in;
static yasm_linemap *cur_lm;
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
	    yasm__warning_va(YASM_WARN_PREPROC,
			     yasm_linemap_get_current(cur_lm), fmt, va);
	    break;
	case ERR_NONFATAL:
	    yasm__error_va(yasm_linemap_get_current(cur_lm), fmt, va);
	    break;
	case ERR_FATAL:
	    yasm_fatal(N_("unknown"));	/* FIXME */
	    break;
	case ERR_PANIC:
	    yasm_internal_error(fmt);	/* FIXME */
	    break;
	case ERR_DEBUG:
	    break;
    }
    va_end(va);
}

static void
nasm_preproc_initialize(FILE *f, const char *in_filename, yasm_linemap *lm)
{
    in = f;
    cur_lm = lm;
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
    nasm_eval_cleanup();
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
	yasm_xfree(line);
	line = NULL;
    } else {
	lineleft -= n;
	linepos += n;
    }

    return tot;
}

static void
nasm_preproc_add_include_path (const char *path)
{
    pp_include_path(path);
}

static void
nasm_preproc_add_include_file (const char *filename)
{
    pp_pre_include(filename);
}


/* Define preproc structure -- see preproc.h for details */
yasm_preproc yasm_nasm_LTX_preproc = {
    YASM_PREPROC_VERSION,
    "Real NASM Preprocessor",
    "nasm",
    nasm_preproc_initialize,
    nasm_preproc_cleanup,
    nasm_preproc_input,
    nasm_preproc_add_include_path,
    nasm_preproc_add_include_file
};
