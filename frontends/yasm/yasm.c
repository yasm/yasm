/* $IdPath$
 * Program entry point, command line parsing
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

#include <libintl.h>
#define _(String)	gettext(String)
#ifdef gettext_noop
#define N_(String)	gettext_noop(String)
#else
#define N_(String)	(String)
#endif

#include "bitvect.h"

#include "globals.h"
#include "errwarn.h"

#include "bytecode.h"
#include "section.h"
#include "objfmt.h"
#include "preproc.h"
#include "parser.h"

RCSID("$IdPath$");

int
main(int argc, char *argv[])
{
    FILE *in;

    /* Initialize BitVector (needed for floating point). */
    if (BitVector_Boot() != ErrCode_Ok) {
	fprintf(stderr, _("Could not initialize BitVector"));
	return EXIT_FAILURE;
    }

    if (argc == 2) {
	in = fopen(argv[1], "rt");
	if (!in) {
	    fprintf(stderr, "could not open file `%s'\n", argv[1]);
	    return EXIT_FAILURE;
	}
	filename = strdup(argv[1]);
    } else {
	in = stdin;
	filename = strdup("<STDIN>");
    }

    /* Get initial BITS setting from object format */
    mode_bits = dbg_objfmt.default_mode_bits;

    nasm_parser.do_parse(&nasm_parser, &dbg_objfmt, in);

    if (OutputAllErrorWarning() > 0)
	return EXIT_FAILURE;

    if (filename)
	free(filename);
    return EXIT_SUCCESS;
}
