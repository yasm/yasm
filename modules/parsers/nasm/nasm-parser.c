/*
 * NASM-compatible parser
 *
 *  Copyright (C) 2001  Peter Johnson
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

#include "section.h"
#include "objfmt.h"
#include "preproc.h"
#include "parser.h"

#include "nasm-parser.h"


FILE *nasm_parser_in = NULL;
size_t (*nasm_parser_input) (char *buf, size_t max_size);

sectionhead nasm_parser_sections;
/*@dependent@*/ section *nasm_parser_cur_section;

/* last "base" label for local (.) labels */
char *nasm_parser_locallabel_base = (char *)NULL;
size_t nasm_parser_locallabel_base_len = 0;

/*@dependent@*/ arch *nasm_parser_arch;
/*@dependent@*/ objfmt *nasm_parser_objfmt;
/*@dependent@*/ linemgr *nasm_parser_linemgr;
/*@dependent@*/ errwarn *nasm_parser_errwarn;

static /*@dependent@*/ sectionhead *
nasm_parser_do_parse(preproc *pp, arch *a, objfmt *of, linemgr *lm,
		     errwarn *we, FILE *f, const char *in_filename)
    /*@globals killed nasm_parser_locallabel_base @*/
{
    pp->initialize(f, in_filename, lm, we);
    nasm_parser_in = f;
    nasm_parser_input = pp->input;
    nasm_parser_arch = a;
    nasm_parser_objfmt = of;
    nasm_parser_linemgr = lm;
    nasm_parser_errwarn = we;

    /* Initialize section list */
    nasm_parser_cur_section = sections_initialize(&nasm_parser_sections, of);

    /* yacc debugging, needs YYDEBUG set in bison.y.in to work */
    /* nasm_parser_debug = 1; */

    nasm_parser_parse();

    nasm_parser_cleanup();

    /* Free locallabel base if necessary */
    if (nasm_parser_locallabel_base)
	xfree(nasm_parser_locallabel_base);

    return &nasm_parser_sections;
}

/* Define valid preprocessors to use with this parser */
static const char *nasm_parser_preproc_keywords[] = {
    "raw",
    NULL
};

/* Define parser structure -- see parser.h for details */
parser yasm_nasm_LTX_parser = {
    "NASM-compatible parser",
    "nasm",
    nasm_parser_preproc_keywords,
    "raw",
    nasm_parser_do_parse
};
