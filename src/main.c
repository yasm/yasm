/* $Id: main.c,v 1.7 2001/08/19 03:52:58 peter Exp $
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bytecode.h"
#include "section.h"
#include "outfmt.h"
#include "preproc.h"
#include "parser.h"

char *filename = (char *)NULL;
unsigned int line_number = 1;
unsigned int mode_bits = 32;

int
main(int argc, char *argv[])
{
    FILE *in;

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

    nasm_parser.doparse(&raw_preproc, &dbg_outfmt, in);

    if (filename)
	free(filename);
    return EXIT_SUCCESS;
}
