/* $Id: main.c,v 1.5 2001/08/18 22:15:12 peter Exp $
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

extern int yydebug;
extern FILE *yyin;

extern int yyparse(void);

char *filename = (char *)NULL;
unsigned int line_number = 1;
unsigned int mode_bits = 32;

int
main (int argc, char *argv[])
{
    FILE *in;

    yydebug = 1;

    if(argc==2) {
	in = fopen(argv[1], "rt");
	if(!in) {
	    fprintf(stderr, "could not open file `%s'\n", argv[1]);
	    return EXIT_FAILURE;
	}
	filename = strdup(argv[1]);
	yyin = in;
    } else
	filename = strdup("<UNKNOWN>");

    yyparse();

    if(filename)
	free(filename);
    return EXIT_SUCCESS;
}

