/* $IdPath$
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "util.h"

#include <stdio.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
#endif

#include "errwarn.h"

#include "bytecode.h"
#include "section.h"
#include "objfmt.h"
#include "preproc.h"
#include "parser.h"

RCSID("$IdPath$");

extern FILE *nasm_parser_in;
extern int nasm_parser_debug;

extern int nasm_parser_parse(void);

int (*nasm_parser_yyinput) (char *buf, int max_size);

objfmt *nasm_parser_objfmt;
sectionhead nasm_parser_sections;
section *nasm_parser_cur_section;

static sectionhead *
nasm_parser_do_parse(parser *p, objfmt *of, FILE *f)
{
    p->current_pp->initialize(of, f);
    nasm_parser_in = f;
    nasm_parser_yyinput = p->current_pp->input;

    nasm_parser_objfmt = of;

    /* Initialize section list */
    nasm_parser_cur_section = sections_initialize(&nasm_parser_sections, of);

    /* only temporary */
    nasm_parser_debug = 0;

    nasm_parser_parse();

    return &nasm_parser_sections;
}

/* Define valid preprocessors to use with this parser */
static preproc *nasm_parser_preprocs[] = {
    &raw_preproc,
    NULL
};

/* Define parser structure -- see parser.h for details */
parser nasm_parser = {
    "NASM-compatible parser",
    "nasm",
    nasm_parser_preprocs,
    &raw_preproc,
    nasm_parser_do_parse
};
