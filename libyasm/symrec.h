/* $IdPath$
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
    SYM_USED = 1 << 0,		/* for using variables before declared */
    SYM_DECLARED = 1 << 1,	/* once it's been declared */
    SYM_VALUED = 1 << 2		/* once its value has been determined */
} SymStatus;

typedef enum {
    SYM_CONSTANT,		/* for EQU defined symbols */
    SYM_LABEL			/* for labels */
} SymType;

typedef struct symrec_s {
    char *name;
    SymType type;
    SymStatus status;
    char *filename;		/* file and line */
    int line;			/*  symbol was first declared or used on */
    double value;
} symrec;

symrec *symrec_use(char *name, SymType type);
symrec *symrec_define(char *name, SymType type);
void symrec_foreach(int (*func) (char *name, symrec *rec));

#endif
