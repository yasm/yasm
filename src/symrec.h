/* $Id: symrec.h,v 1.4 2001/06/28 21:22:01 peter Exp $
 * Symbol table handling header file
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
#ifndef YASM_SYMREC_H
#define YASM_SYMREC_H

typedef enum {
    SYM_NOSTATUS = 0,
    SYM_USED	 = 1 << 0,  /* for using variables before declared */
    SYM_DECLARED = 1 << 1,  /* once it's been declared */
    SYM_VALUED	 = 1 << 2   /* once its value has been determined */
} SymStatus;

typedef enum {
    SYM_CONSTANT,	    /* for EQU defined symbols */
    SYM_LABEL,		    /* for labels */
    SYM_DATA		    /* for variables */
} SymType;

typedef struct symrec_s {
    char *name;
    SymType type;
    SymStatus status;
    int line;
    double value;
} symrec;

typedef struct symtab_s {
    symrec rec;
    struct symtab_s *next;
} symtab;

extern symtab *sym_table;

/*symrec *putsym(char *, SymType);*/
/*symrec *getsym(char *);*/

symrec *sym_use_get (char *, SymType);
symrec *sym_def_get (char *, SymType);
void sym_foreach (int(*)(symrec *));

#endif
