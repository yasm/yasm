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
#define YASM_LIB_INTERNAL
#include "libyasm.h"
/*@unused@*/ RCSID("$IdPath$");


static int is_interactive;
static FILE *in;
static yasm_linemgr *cur_lm;

int isatty(int);

static void
raw_preproc_initialize(FILE *f, const char *in_filename, yasm_linemgr *lm)
{
    in = f;
    cur_lm = lm;
    /*@-unrecog@*/
    is_interactive = f ? (isatty(fileno(f)) > 0) : 0;
    /*@=unrecog@*/
}

static void
raw_preproc_cleanup(void)
{
}

static size_t
raw_preproc_input(char *buf, size_t max_size)
{
    int c = '*';
    size_t n;

    if (is_interactive) {
	for (n = 0; n < max_size && (c = getc(in)) != EOF && c != '\n'; n++)
	    buf[n] = (char)c;
	if (c == '\n')
	    buf[n++] = (char)c;
	if (c == EOF && ferror(in))
	    yasm__error(cur_lm->get_current(),
			N_("error when reading from file"));
    } else if (((n = fread(buf, 1, max_size, in)) == 0) && ferror(in))
	yasm__error(cur_lm->get_current(), N_("error when reading from file"));

    return n;
}

/* Define preproc structure -- see preproc.h for details */
yasm_preproc yasm_raw_LTX_preproc = {
    "Disable preprocessing",
    "raw",
    raw_preproc_initialize,
    raw_preproc_cleanup,
    raw_preproc_input,
    /* no include paths */ NULL,
    /* no pre-include files */ NULL
};
