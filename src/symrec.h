/* $IdPath$
 * Symbol table handling header file
 *
 *  Copyright (C) 2001  Michael Urman, Peter Johnson
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

#ifndef YASM_EXPR
#define YASM_EXPR
typedef struct expr expr;
#endif

#ifndef YASM_SECTION
#define YASM_SECTION
typedef struct section section;
#endif

#ifndef YASM_BYTECODE
#define YASM_BYTECODE
typedef struct bytecode bytecode;
#endif

/* EXTERN and COMMON are mutually exclusive */
typedef enum {
    SYM_LOCAL = 0,		/* default, local only */
    SYM_GLOBAL = 1 << 0,	/* if it's declared GLOBAL */
    SYM_COMMON = 1 << 1,	/* if it's declared COMMON */
    SYM_EXTERN = 1 << 2		/* if it's declared EXTERN */
} SymVisibility;

#ifndef YASM_SYMREC
#define YASM_SYMREC
typedef struct symrec symrec;
#endif

symrec *symrec_use(const char *name);
symrec *symrec_define_equ(const char *name, expr *e);
/* in_table specifies if the label should be inserted into the symbol table. */
symrec *symrec_define_label(const char *name, section *sect, bytecode *precbc,
			    int in_table);
symrec *symrec_declare(const char *name, SymVisibility vis);

/* Get the numeric 32-bit value of a symbol if possible.
 * Return value is IF POSSIBLE, not the value.
 * If resolve_label is true, tries to get offset of labels, otherwise it
 * returns not possible.
 */
int symrec_get_int_value(const symrec *sym, unsigned long *ret_val,
			 int resolve_label);

const char *symrec_get_name(const symrec *sym);

const expr *symrec_get_equ(const symrec *sym);

int symrec_foreach(int (*func) (symrec *sym));

void symrec_print(const symrec *sym);
#endif
