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


extern FILE *nasm_parser_in;
extern int nasm_parser_debug;

extern int nasm_parser_parse(void);

size_t (*nasm_parser_input) (char *buf, size_t max_size);

sectionhead nasm_parser_sections;
/*@dependent@*/ section *nasm_parser_cur_section;

extern /*@only@*/ char *nasm_parser_locallabel_base;

static /*@dependent@*/ sectionhead *
nasm_parser_do_parse(parser *p, FILE *f)
    /*@globals killed nasm_parser_locallabel_base @*/
{
    p->current_pp->initialize(f);
    nasm_parser_in = f;
    nasm_parser_input = p->current_pp->input;

    /* Initialize section list */
    nasm_parser_cur_section = sections_initialize(&nasm_parser_sections);

    /* yacc debugging, needs YYDEBUG set in bison.y.in to work */
    /* nasm_parser_debug = 1; */

    nasm_parser_parse();

    /* Free locallabel base if necessary */
    if (nasm_parser_locallabel_base)
	xfree(nasm_parser_locallabel_base);

    return &nasm_parser_sections;
}

/* Define valid preprocessors to use with this parser */
/*@-nullassign@*/
static preproc *nasm_parser_preprocs[] = {
    &raw_preproc,
    NULL
};
/*@=nullassign@*/

/* Define parser structure -- see parser.h for details */
parser nasm_parser = {
    "NASM-compatible parser",
    "nasm",
    nasm_parser_preprocs,
    &raw_preproc,
    nasm_parser_do_parse
};
