/*
 * NASM-compatible parser
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
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#include "errwarn.h"

#include "section.h"
#include "objfmt.h"
#include "preproc.h"
#include "parser.h"

#include "nasm-parser.h"


FILE *nasm_parser_in = NULL;
size_t (*nasm_parser_input) (char *buf, size_t max_size);

yasm_sectionhead nasm_parser_sections;
/*@dependent@*/ yasm_section *nasm_parser_cur_section;

/* last "base" label for local (.) labels */
char *nasm_parser_locallabel_base = (char *)NULL;
size_t nasm_parser_locallabel_base_len = 0;

/*@dependent@*/ yasm_arch *nasm_parser_arch;
/*@dependent@*/ yasm_objfmt *nasm_parser_objfmt;
/*@dependent@*/ yasm_linemgr *nasm_parser_linemgr;

int nasm_parser_save_input;

static /*@dependent@*/ yasm_sectionhead *
nasm_parser_do_parse(yasm_preproc *pp, yasm_arch *a, yasm_objfmt *of,
		     yasm_linemgr *lm, FILE *f, const char *in_filename,
		     int save_input)
    /*@globals killed nasm_parser_locallabel_base @*/
{
    pp->initialize(f, in_filename, lm);
    nasm_parser_in = f;
    nasm_parser_input = pp->input;
    nasm_parser_arch = a;
    nasm_parser_objfmt = of;
    nasm_parser_linemgr = lm;
    nasm_parser_save_input = save_input;

    /* Initialize section list */
    nasm_parser_cur_section =
	yasm_sections_initialize(&nasm_parser_sections, of);

    /* yacc debugging, needs YYDEBUG set in bison.y.in to work */
    /* nasm_parser_debug = 1; */

    nasm_parser_parse();

    nasm_parser_cleanup();

    /* Free locallabel base if necessary */
    if (nasm_parser_locallabel_base)
	yasm_xfree(nasm_parser_locallabel_base);

    return &nasm_parser_sections;
}

/* Define valid preprocessors to use with this parser */
static const char *nasm_parser_preproc_keywords[] = {
    "raw",
    NULL
};

/* Define parser structure -- see parser.h for details */
yasm_parser yasm_nasm_LTX_parser = {
    "NASM-compatible parser",
    "nasm",
    nasm_parser_preproc_keywords,
    "raw",
    nasm_parser_do_parse
};
