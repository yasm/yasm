/* $IdPath$
 * Symbol table handling
 *
 *  Copyright (C) 2001  Michael Urman
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

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#endif

#include <libintl.h>
#define _(String)	gettext(String)

#include "globals.h"
#include "errwarn.h"
#include "symrec.h"

RCSID("$IdPath$");

/* private functions */
static symrec *symrec_get_or_new(char *, SymType);

/* The symbol table: a ternary tree. */
static ternary_tree sym_table = (ternary_tree)NULL;

/* create a new symrec */
static symrec *
symrec_get_or_new(char *name, SymType type)
{
    symrec *rec, *rec2;

    rec = malloc(sizeof(symrec));
    if (!rec)
	Fatal(FATAL_NOMEM);

    rec2 = ternary_insert(&sym_table, name, rec, 0);

    if (rec2 != rec) {
	free(rec);
	return rec2;
    }

    rec->name = strdup(name);
    if (!rec->name)
	Fatal(FATAL_NOMEM);
    rec->type = type;
    rec->value = 0;
    rec->filename = strdup(filename);
    rec->line = line_number;
    rec->status = SYM_NOSTATUS;

    return rec;
}

/* call a function with each symrec.  stop early if 0 returned */
void
symrec_foreach(int (*func) (char *name, symrec *rec))
{
}

symrec *
symrec_use(char *name, SymType type)
{
    symrec *rec;

    rec = symrec_get_or_new(name, type);
    rec->status |= SYM_USED;
    return rec;
}

symrec *
symrec_define(char *name, SymType type)
{
    symrec *rec;

    rec = symrec_get_or_new(name, type);
    if (rec->status & SYM_DECLARED)
	Error(_("duplicate definition of `%s'; previously defined on line %d"),
	      name, rec->line);
    rec->line = line_number;	/* set line number of definition */
    rec->status |= SYM_DECLARED;
    return rec;
}
