/* $Id: parser.c,v 1.1 2001/09/16 04:49:46 peter Exp $
 * Generic functions for all parsers
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
# include <string.h>
#endif

#include "globals.h"
#include "errwarn.h"
#include "expr.h"

#include "bytecode.h"
#include "section.h"
#include "objfmt.h"
#include "preproc.h"
#include "parser.h"

RCSID("$Id: parser.c,v 1.1 2001/09/16 04:49:46 peter Exp $");

/* NULL-terminated list of all available parsers.
 * Someday change this if we dynamically load parsers at runtime.
 * Could improve this a little by generating automatically at build-time.
 */
static parser *parsers[] = {
    &nasm_parser,
    NULL
};

int
parser_setpp(parser *p, const char *pp_keyword)
{
    int i;

    /* We're just doing a linear search, as preprocs should be short */
    for (i = 0; p->preprocs[i]; i++) {
	if (strcmp(p->preprocs[i]->keyword, pp_keyword) == 0) {
	    p->current_pp = p->preprocs[i];
	    return 0;
	}
    }

    /* no match found */
    return 1;
}

void
parser_listpp(parser *p,
	      void (*printfunc) (const char *name, const char *keyword))
{
    int i;

    for (i = 0; p->preprocs[i]; i++) {
	printfunc(p->preprocs[i]->name, p->preprocs[i]->keyword);
    }
}

parser *
find_parser(const char *keyword)
{
    int i;

    /* We're just doing a linear search, as there aren't many parsers */
    for (i = 0; parsers[i]; i++) {
	if (strcmp(parsers[i]->keyword, keyword) == 0)
	    return parsers[i];
    }

    /* no match found */
    return NULL;
}

void
list_parsers(void (*printfunc) (const char *name, const char *keyword))
{
    int i;

    for (i = 0; parsers[i]; i++) {
	printfunc(parsers[i]->name, parsers[i]->keyword);
    }
}
