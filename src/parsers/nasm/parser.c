/* $Id: parser.c,v 1.9 2001/09/16 18:53:47 peter Exp $
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

RCSID("$Id: parser.c,v 1.9 2001/09/16 18:53:47 peter Exp $");

extern FILE *nasm_parser_in;
extern int nasm_parser_debug;

extern int nasm_parser_parse(void);

int (*nasm_parser_yyinput) (char *buf, int max_size);

sectionhead nasm_parser_sections;
section *nasm_parser_cur_section;

static sectionhead *
nasm_parser_doparse(parser *p, objfmt *of, FILE *f)
{
    p->current_pp->initialize(of, f);
    nasm_parser_in = f;
    nasm_parser_yyinput = p->current_pp->input;

    /* Initialize linked list */
    STAILQ_INIT(&nasm_parser_sections);

    /* Add an initial "default" section to the list */
    nasm_parser_cur_section = calloc(1, sizeof(section));
    if (!nasm_parser_cur_section)
	Fatal(FATAL_NOMEM);
    STAILQ_INSERT_TAIL(&nasm_parser_sections, nasm_parser_cur_section, link);

    /* Initialize default section */
    nasm_parser_cur_section->type = SECTION_DEFAULT;
    STAILQ_INIT(&nasm_parser_cur_section->bc);

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
    nasm_parser_doparse
};
