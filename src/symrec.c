/* $Id: symrec.c,v 1.1 2001/05/15 05:24:04 peter Exp $
 * Symbol table handling
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
#include <stdlib.h>
#include <string.h>
#include "symrec.h"

/* The symbol table: a chain of `symrec'. */
symrec *sym_table = (symrec *)NULL;

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

