/*
 * Global variables
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

#include "ternary.h"

#include "globals.h"


/*@null@*/ /*@dependent@*/ const char *in_filename = (const char *)NULL;
unsigned int line_number = 1;
unsigned int asm_options = 0;

static /*@only@*/ /*@null@*/ ternary_tree filename_table = (ternary_tree)NULL;

void
switch_filename(const char *filename)
{
    char *copy = xstrdup(filename);
    in_filename = ternary_insert(&filename_table, filename, copy, 0);
    /*@-branchstate@*/
    if (in_filename != copy)
	xfree(copy);
    /*@=branchstate@*/
}

static void
filename_delete_one(/*@only@*/ void *d)
{
    xfree(d);
}

void
filename_delete_all(void)
{
    in_filename = NULL;
    ternary_cleanup(filename_table, filename_delete_one);
    filename_table = NULL;
}
