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

#include "hamt.h"

#include "globals.h"


/* Current (selected) parser */
/*@null@*/ parser *cur_parser = NULL;

/* Current (selected) object format) */
/*@null@*/ objfmt *cur_objfmt = NULL;

/*@null@*/ /*@dependent@*/ const char *in_filename = (const char *)NULL;
unsigned int line_number = 1;
unsigned int asm_options = 0;

int indent_level = 0;

static /*@only@*/ /*@null@*/ HAMT *filename_table = NULL;

static void
filename_delete_one(/*@only@*/ void *d)
{
    xfree(d);
}

void
switch_filename(const char *filename)
{
    char *copy = xstrdup(filename);
    int replace = 0;
    if (!filename_table)
	filename_table = HAMT_new();
    /*@-aliasunique@*/
    in_filename = HAMT_insert(filename_table, copy, copy, &replace,
			      filename_delete_one);
    /*@=aliasunique@*/
}

void
filename_delete_all(void)
{
    in_filename = NULL;
    if (filename_table) {
	HAMT_delete(filename_table, filename_delete_one);
	filename_table = NULL;
    }
}
