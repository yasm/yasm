/* $Id: nasm-parser.c,v 1.6 2001/08/19 07:46:52 peter Exp $
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

#include "bytecode.h"
#include "section.h"
#include "outfmt.h"
#include "preproc.h"
#include "parser.h"

RCSID("$Id: nasm-parser.c,v 1.6 2001/08/19 07:46:52 peter Exp $");

extern FILE *nasm_parser_in;
extern int nasm_parser_debug;

extern int nasm_parser_parse(void);

int (*nasm_parser_yyinput) (char *buf, int max_size);

static section *
doparse(preproc *pp, outfmt *of, FILE *f)
{
    pp->initialize(of, f);
    nasm_parser_in = f;
    nasm_parser_yyinput = pp->input;

    /* only temporary */
    nasm_parser_debug = 1;

    nasm_parser_parse();

    return NULL;
}

/* Define valid preprocessors to use with this parser */
static preproc *preprocs[] = {
    &raw_preproc,
    NULL
};

/* Define parser structure -- see parser.h for details */
parser nasm_parser = {
    "NASM-compatible parser",
    "nasm",
    preprocs,
    &raw_preproc,
    doparse
};
