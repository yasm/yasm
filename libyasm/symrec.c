/* $Id: symrec.c,v 1.4 2001/07/04 20:57:53 peter Exp $
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
#include <stdlib.h>
#include <string.h>
#include "symrec.h"
#include "globals.h"
#include "errwarn.h"

/* private functions */
static symtab *symtab_get (char *);
static symtab *symtab_new (char *, SymType);
static symtab *symtab_get_or_new (char *, SymType);
static void symtab_insert (symtab *);

/* The symbol table: a chain of `symtab'. */
symtab *sym_table = (symtab *)NULL;

/* insert a symtab into the global sym_table */
void symtab_insert (symtab *tab)
{
    tab->next = (symtab *)sym_table;
    sym_table = tab;
}

/* find a symtab in the global sym_table */
symtab *symtab_get (char *name)
{
    symtab *tab;
    for (tab = sym_table; tab != NULL; tab = tab->next)
    {
	if (strcmp (tab->rec.name, name) == 0)
	{
	    return tab;
	}
    }
    return NULL;
}

/* call a function with each symrec.  stop early if 0 returned */
void sym_foreach (int(*mapfunc)(symrec *))
{
    symtab *tab;
    for (tab = sym_table; tab != NULL; tab = tab->next)
    {
	if (mapfunc(&(tab->rec)) == 0)
	{
	    return;
	}
    }
}

/* create a new symtab */
symtab *symtab_new (char *name, SymType type)
{
    symtab *tab;
    tab = malloc(sizeof(symtab));
    if (tab == NULL) return NULL;
    tab->rec.name = malloc(strlen(name)+1);
    if (tab->rec.name == NULL)
    {
	free (tab);
	return NULL;
    }
    strcpy(tab->rec.name, name);
    tab->rec.type = type;
    tab->rec.value = 0;
    tab->rec.line = line_number;
    tab->rec.status = SYM_NOSTATUS;
    symtab_insert (tab);
    return tab;
}

symtab *symtab_get_or_new (char *name, SymType type)
{
    symtab *tab;
    tab = symtab_get (name);
    if (tab == NULL)
    {
	tab = symtab_new (name, type);
	if (tab == NULL)
	{
	    Fatal (FATAL_NOMEM);
	}
    }
    return tab;
}

symrec *sym_use_get (char *name, SymType type)
{
    symtab *tab;
    tab = symtab_get_or_new (name, type);
    tab->rec.status |= SYM_USED;
    return &(tab->rec);
}

symrec *sym_def_get (char *name, SymType type)
{
    symtab *tab;
    tab = symtab_get_or_new (name, type);
    if (tab->rec.status & SYM_DECLARED)
	Error (ERR_DUPLICATE_DEF, "%s%d", tab->rec.name, tab->rec.line);
    tab->rec.status |= SYM_DECLARED;
    return &(tab->rec);
}

#if 0
symrec *putsym(char *sym_name, int sym_type)
{
    symrec *ptr;
    ptr = malloc(sizeof(symrec));
    ptr->name = malloc(strlen(sym_name)+1);
    strcpy(ptr->name, sym_name);
    ptr->type = sym_type;
    ptr->value.var = 0; /* set value to 0 even if fctn.  */
    ptr->next = (symrec *)sym_table;
    sym_table = ptr;
    return ptr;
}

symrec *getsym(char *sym_name)
{
    symrec *ptr;
    for(ptr=sym_table; ptr != (symrec *)NULL; ptr=(symrec *)ptr->next)
	if(strcmp(ptr->name, sym_name) == 0)
	    return ptr;
    return 0;
}
#endif /* 0 */
