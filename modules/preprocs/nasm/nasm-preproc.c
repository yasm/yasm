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
/*@unused@*/ RCSID("$Id$");

#define YASM_LIB_INTERNAL
#include <libyasm.h>

#include "nasm.h"
#include "nasmlib.h"
#include "nasm-pp.h"
#include "nasm-eval.h"

typedef struct yasm_preproc_nasm {
    yasm_preproc_base preproc;   /* Base structure */

    FILE *in;
    char *line, *linepos;
    size_t lineleft;
    char *file_name;
    long prior_linnum;
    int lineinc;
} yasm_preproc_nasm;
static yasm_linemap *cur_lm;
static yasm_errwarns *cur_errwarns;
int tasm_compatible_mode = 0;

typedef struct preproc_dep {
    STAILQ_ENTRY(preproc_dep) link;
    char *name;
} preproc_dep;

static STAILQ_HEAD(preproc_dep_head, preproc_dep) *preproc_deps;
static int done_dep_preproc;

yasm_preproc_module yasm_nasm_LTX_preproc;

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
	    yasm_warn_set_va(YASM_WARN_PREPROC, fmt, va);
	    break;
	case ERR_NONFATAL:
	    yasm_error_set_va(YASM_ERROR_GENERAL, fmt, va);
	    break;
	case ERR_FATAL:
	    yasm_fatal(fmt, va);
	    /*@notreached@*/
	    break;
	case ERR_PANIC:
	    yasm_internal_error(fmt);	/* FIXME */
	    break;
	case ERR_DEBUG:
	    break;
    }
    va_end(va);
    yasm_errwarn_propagate(cur_errwarns,
	yasm_linemap_poke(cur_lm, nasm_src_get_fname(),
			  (unsigned long)nasm_src_get_linnum()));
}

static yasm_preproc *
nasm_preproc_create(FILE *f, const char *in_filename, yasm_linemap *lm,
		    yasm_errwarns *errwarns)
{
    yasm_preproc_nasm *preproc_nasm = yasm_xmalloc(sizeof(yasm_preproc_nasm));

    preproc_nasm->preproc.module = &yasm_nasm_LTX_preproc;

    preproc_nasm->in = f;
    cur_lm = lm;
    cur_errwarns = errwarns;
    preproc_deps = NULL;
    done_dep_preproc = 0;
    preproc_nasm->line = NULL;
    preproc_nasm->file_name = NULL;
    preproc_nasm->prior_linnum = 0;
    preproc_nasm->lineinc = 0;
    nasmpp.reset(f, in_filename, 2, nasm_efunc, nasm_evaluate, &nil_list);

    return (yasm_preproc *)preproc_nasm;
}

static void
nasm_preproc_destroy(yasm_preproc *preproc)
{
    yasm_preproc_nasm *preproc_nasm = (yasm_preproc_nasm *)preproc;
    nasmpp.cleanup(0);
    if (preproc_nasm->file_name)
	yasm_xfree(preproc_nasm->file_name);
    yasm_xfree(preproc);
    if (preproc_deps)
	yasm_xfree(preproc_deps);
}

static size_t
nasm_preproc_input(yasm_preproc *preproc, char *buf, size_t max_size)
{
    yasm_preproc_nasm *preproc_nasm = (yasm_preproc_nasm *)preproc;
    size_t tot = 0, n;
    long linnum = preproc_nasm->prior_linnum += preproc_nasm->lineinc;
    int altline;

    if (!preproc_nasm->line) {
	preproc_nasm->line = nasmpp.getline();
	if (!preproc_nasm->line)
	    return 0;
	preproc_nasm->linepos = preproc_nasm->line;
	preproc_nasm->lineleft = strlen(preproc_nasm->line) + 1;
	preproc_nasm->line[preproc_nasm->lineleft-1] = '\n';

	altline = nasm_src_get(&linnum, &preproc_nasm->file_name);
	if (altline) {
	    if (altline == 1 && preproc_nasm->lineinc == 1) {
		*buf++ = '\n';
		max_size--;
		tot++;
	    } else {
		preproc_nasm->lineinc =
		    (altline != -1 || preproc_nasm->lineinc != 1);
		n = sprintf(buf, "%%line %ld+%d %s\n", linnum,
			    preproc_nasm->lineinc, preproc_nasm->file_name);
		buf += n;
		max_size -= n;
		tot += n;
	    }
	    preproc_nasm->prior_linnum = linnum;
	}
    }

    n = preproc_nasm->lineleft<max_size?preproc_nasm->lineleft:max_size;
    strncpy(buf, preproc_nasm->linepos, n);
    tot += n;

    if (n == preproc_nasm->lineleft) {
	yasm_xfree(preproc_nasm->line);
	preproc_nasm->line = NULL;
    } else {
	preproc_nasm->lineleft -= n;
	preproc_nasm->linepos += n;
    }

    return tot;
}

void
nasm_preproc_add_dep(char *name)
{
    preproc_dep *dep;

    /* If not processing dependencies, simply return */
    if (!preproc_deps)
	return;

    /* Save in preproc_deps */
    dep = yasm_xmalloc(sizeof(preproc_dep));
    dep->name = yasm__xstrdup(name);
    STAILQ_INSERT_TAIL(preproc_deps, dep, link);
}

static size_t
nasm_preproc_get_included_file(yasm_preproc *preproc, /*@out@*/ char *buf,
			       size_t max_size)
{
    if (!preproc_deps) {
	preproc_deps = yasm_xmalloc(sizeof(struct preproc_dep_head));
	STAILQ_INIT(preproc_deps);
    }

    for (;;) {
	char *line;

	/* Pull first dep out of preproc_deps and return it if there is one */
	if (!STAILQ_EMPTY(preproc_deps)) {
	    char *name;
	    preproc_dep *dep = STAILQ_FIRST(preproc_deps);
	    STAILQ_REMOVE_HEAD(preproc_deps, link);
	    name = dep->name;
	    yasm_xfree(dep);
	    strncpy(buf, name, max_size);
	    buf[max_size-1] = '\0';
	    yasm_xfree(name);
	    return strlen(buf);
	}

	/* No more preprocessing to do */
	if (done_dep_preproc) {
	    return 0;
	}

	/* Preprocess some more, throwing away the result */
	line = nasmpp.getline();
	if (line)
	    yasm_xfree(line);
	else
	    done_dep_preproc = 1;
    }
}

static void
nasm_preproc_add_include_file(yasm_preproc *preproc, const char *filename)
{
    pp_pre_include(filename);
}

static void
nasm_preproc_predefine_macro(yasm_preproc *preproc, const char *macronameval)
{
    char *mnv = yasm__xstrdup(macronameval);
    pp_pre_define(mnv);
    yasm_xfree(mnv);
}

static void
nasm_preproc_undefine_macro(yasm_preproc *preproc, const char *macroname)
{
    char *mn = yasm__xstrdup(macroname);
    pp_pre_undefine(mn);
    yasm_xfree(mn);
}

static void
nasm_preproc_define_builtin(yasm_preproc *preproc, const char *macronameval)
{
    char *mnv = yasm__xstrdup(macronameval);
    pp_builtin_define(mnv);
    yasm_xfree(mnv);
}

/* Define preproc structure -- see preproc.h for details */
yasm_preproc_module yasm_nasm_LTX_preproc = {
    "Real NASM Preprocessor",
    "nasm",
    nasm_preproc_create,
    nasm_preproc_destroy,
    nasm_preproc_input,
    nasm_preproc_get_included_file,
    nasm_preproc_add_include_file,
    nasm_preproc_predefine_macro,
    nasm_preproc_undefine_macro,
    nasm_preproc_define_builtin
};
