/*
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
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#include "module.h"

#include "parser.h"


/* NULL-terminated list of all possibly available parser keywords.
 * Could improve this a little by generating automatically at build-time.
 */
/*@-nullassign@*/
const char *parsers[] = {
    "nasm",
    NULL
};
/*@=nullassign@*/


void
list_parsers(void (*printfunc) (const char *name, const char *keyword))
{
    int i;
    parser *p;

    /* Go through available list, and try to load each one */
    for (i = 0; parsers[i]; i++) {
	p = load_parser(parsers[i]);
	if (p)
	    printfunc(p->name, p->keyword);
    }
}
