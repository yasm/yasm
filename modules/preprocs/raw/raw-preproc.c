/*
 * Raw preprocessor (preforms NO preprocessing)
 *
 *  Copyright (C) 2001  Peter Johnson
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


typedef struct yasm_preproc_raw {
    yasm_preproc_base preproc;   /* base structure */

    int is_interactive;
    FILE *in;
    yasm_linemap *cur_lm;
} yasm_preproc_raw;

yasm_preproc_module yasm_raw_LTX_preproc;

int isatty(int);

static yasm_preproc *
raw_preproc_create(FILE *f, const char *in_filename, yasm_linemap *lm)
{
    yasm_preproc_raw *preproc_raw = yasm_xmalloc(sizeof(yasm_preproc_raw));

    preproc_raw->preproc.module = &yasm_raw_LTX_preproc;
    preproc_raw->in = f;
    preproc_raw->cur_lm = lm;
    /*@-unrecog@*/
    preproc_raw->is_interactive = f ? (isatty(fileno(f)) > 0) : 0;
    /*@=unrecog@*/

    return (yasm_preproc *)preproc_raw;
}

static void
raw_preproc_destroy(yasm_preproc *preproc)
{
    yasm_xfree(preproc);
}

static size_t
raw_preproc_input(yasm_preproc *preproc, char *buf, size_t max_size)
{
    yasm_preproc_raw *preproc_raw = (yasm_preproc_raw *)preproc;
    int c = '*';
    size_t n;

    if (preproc_raw->is_interactive) {
	for (n = 0; n < max_size && (c = getc(preproc_raw->in)) != EOF &&
	     c != '\n'; n++)
	    buf[n] = (char)c;
	if (c == '\n')
	    buf[n++] = (char)c;
	if (c == EOF && ferror(preproc_raw->in))
	    yasm__error(yasm_linemap_get_current(preproc_raw->cur_lm),
			N_("error when reading from file"));
    } else if (((n = fread(buf, 1, max_size, preproc_raw->in)) == 0) &&
	       ferror(preproc_raw->in))
	yasm__error(yasm_linemap_get_current(preproc_raw->cur_lm),
		    N_("error when reading from file"));

    return n;
}

static void
raw_preproc_add_include_path(yasm_preproc *preproc, const char *path)
{
    /* no include paths */
}

static void
raw_preproc_add_include_file(yasm_preproc *preproc, const char *filename)
{
    /* no pre-include files */
}

static void
raw_preproc_predefine_macro(yasm_preproc *preproc, const char *macronameval)
{
    /* no pre-defining macros */
}

static void
raw_preproc_undefine_macro(yasm_preproc *preproc, const char *macroname)
{
    /* no undefining macros */
}

static void
raw_preproc_define_builtin(yasm_preproc *preproc, const char *macronameval)
{
    /* no builtin defines */
}


/* Define preproc structure -- see preproc.h for details */
yasm_preproc_module yasm_raw_LTX_preproc = {
    "Disable preprocessing",
    "raw",
    raw_preproc_create,
    raw_preproc_destroy,
    raw_preproc_input,
    raw_preproc_add_include_path,
    raw_preproc_add_include_file,
    raw_preproc_predefine_macro,
    raw_preproc_undefine_macro,
    raw_preproc_define_builtin
};
