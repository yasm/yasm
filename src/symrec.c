/* $IdPath$
 * Symbol table handling
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "util.h"

#include <stdio.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#endif

#include <libintl.h>
#define _(String)	gettext(String)

#include "globals.h"
#include "errwarn.h"
#include "floatnum.h"
#include "expr.h"
#include "symrec.h"

#include "bytecode.h"
#include "section.h"

RCSID("$IdPath$");

/* DEFINED is set with EXTERN and COMMON below */
typedef enum {
    SYM_NOSTATUS = 0,
    SYM_USED = 1 << 0,		/* for using variables before definition */
    SYM_DEFINED = 1 << 1,	/* once it's been defined in the file */
    SYM_VALUED = 1 << 2,	/* once its value has been determined */
    SYM_NOTINTABLE = 1 << 3	/* if it's not in sym_table (ex. '$') */
} SymStatus;

typedef enum {
    SYM_UNKNOWN,		/* for unknown type (COMMON/EXTERN) */
    SYM_EQU,			/* for EQU defined symbols (expressions) */
    SYM_LABEL			/* for labels */
} SymType;

struct symrec {
    char *name;
    SymType type;
    SymStatus status;
    SymVisibility visibility;
    char *filename;		/* file and line */
    unsigned long line;		/*  symbol was first declared or used on */
    union {
	expr *expn;		/* equ value */
	struct label_s {	/* bytecode immediately preceding a label */
	    section *sect;
	    bytecode *bc;
	} label;
    } value;
};

/* private functions */
static symrec *symrec_get_or_new(const char *name, int in_table);
static symrec *symrec_define(const char *name, SymType type, int in_table);

/* The symbol table: a ternary tree. */
static ternary_tree sym_table = (ternary_tree)NULL;

/* create a new symrec */
static symrec *
symrec_get_or_new(const char *name, int in_table)
{
    symrec *rec, *rec2;

    rec = xmalloc(sizeof(symrec));
    if (in_table) {
	rec2 = ternary_insert(&sym_table, name, rec, 0);

	if (rec2 != rec) {
	    free(rec);
	    return rec2;
	}
	rec->status = SYM_NOSTATUS;
    } else
	rec->status = SYM_NOTINTABLE;

    rec->name = xstrdup(name);
    rec->type = SYM_UNKNOWN;
    rec->filename = xstrdup(in_filename);
    rec->line = line_number;
    rec->visibility = SYM_LOCAL;

    return rec;
}

/* Call a function with each symrec.  Stops early if 0 returned by func.
   Returns 0 if stopped early. */
int
symrec_foreach(int (*func) (symrec *sym))
{
    return ternary_traverse(sym_table, (int (*) (void *))func);
}

symrec *
symrec_use(const char *name)
{
    symrec *rec = symrec_get_or_new(name, 1);

    rec->status |= SYM_USED;
    return rec;
}

static symrec *
symrec_define(const char *name, SymType type, int in_table)
{
    symrec *rec = symrec_get_or_new(name, in_table);

    /* Has it been defined before (either by DEFINED or COMMON/EXTERN)? */
    if (rec->status & SYM_DEFINED) {
	Error(_("duplicate definition of `%s'; first defined on line %d"),
	      name, rec->line);
    } else {
	rec->line = line_number;	/* set line number of definition */
	rec->type = type;
	rec->status |= SYM_DEFINED;
    }
    return rec;
}

symrec *
symrec_define_equ(const char *name, expr *e)
{
    symrec *rec = symrec_define(name, SYM_EQU, 1);
    rec->value.expn = e;
    rec->status |= SYM_VALUED;
    return rec;
}

symrec *
symrec_define_label(const char *name, section *sect, bytecode *precbc,
		    int in_table)
{
    symrec *rec = symrec_define(name, SYM_LABEL, in_table);
    rec->value.label.sect = sect;
    rec->value.label.bc = precbc;
    return rec;
}

symrec *
symrec_declare(const char *name, SymVisibility vis)
{
    symrec *rec = symrec_get_or_new(name, 1);

    /* Don't allow EXTERN and COMMON if symbol has already been DEFINED. */
    /* Also, EXTERN and COMMON are mutually exclusive. */
    if ((rec->status & SYM_DEFINED) ||
	((rec->visibility & SYM_COMMON) && (vis == SYM_EXTERN)) ||
	((rec->visibility & SYM_EXTERN) && (vis == SYM_COMMON))) {
	Error(_("duplicate definition of `%s'; first defined on line %d"),
	      name, rec->line);
    } else {
	rec->line = line_number;	/* set line number of declaration */
	rec->visibility |= vis;

	/* If declared as COMMON or EXTERN, set as DEFINED. */
	if ((vis == SYM_COMMON) || (vis == SYM_EXTERN))
	    rec->status |= SYM_DEFINED;
    }
    return rec;
}
#if 0
int
symrec_get_int_value(const symrec *sym, unsigned long *ret_val,
		     int resolve_label)
{
    /* If we already know the value, just return it. */
    if (sym->status & SYM_VALUED) {
	switch (sym->type) {
	    case SYM_CONSTANT_INT:
		*ret_val = sym->value.int_val;
		break;
	    case SYM_CONSTANT_FLOAT:
		/* FIXME: Line number on this error will be incorrect. */
		if (floatnum_get_int(ret_val, sym->value.flt))
		    Error(_("Floating point value cannot fit in 32-bit single precision"));
		break;
	    case SYM_LABEL:
		if (!bytecode_get_offset(sym->value.label.sect,
					 sym->value.label.bc, ret_val))
		    InternalError(__LINE__, __FILE__,
				  _("Label symbol is valued but cannot get offset"));
	    case SYM_UNKNOWN:
		InternalError(__LINE__, __FILE__,
			      _("Have a valued symbol but of unknown type"));
	}
	return 1;
    }

    /* Try to get offset of unvalued label */
    if (resolve_label && sym->type == SYM_LABEL)
	return bytecode_get_offset(sym->value.label.sect, sym->value.label.bc,
				   ret_val);

    /* We can't get the value right now. */
    return 0;
}
#endif
const char *
symrec_get_name(const symrec *sym)
{
    return sym->name;
}

SymVisibility
symrec_get_visibility(const symrec *sym)
{
    return sym->visibility;
}

const expr *
symrec_get_equ(const symrec *sym)
{
    if (sym->type == SYM_EQU)
	return sym->value.expn;
    return (const expr *)NULL;
}

void
symrec_print(const symrec *sym)
{
    printf("---Symbol `%s'---\n", sym->name);

    switch (sym->type) {
	case SYM_UNKNOWN:
	    printf("-Unknown (Common/Extern)-\n");
	    break;
	case SYM_EQU:
	    printf("_EQU_\n");
	    printf("Expn=");
	    expr_print(sym->value.expn);
	    printf("\n");
	    break;
	case SYM_LABEL:
	    printf("_Label_\n");
	    printf("Section=`%s'\n", section_get_name(sym->value.label.sect));
	    if (!sym->value.label.bc)
		printf("[First bytecode]\n");
	    else {
		printf("[Preceding bytecode]\n");
		bytecode_print(sym->value.label.bc);
	    }
	    break;
    }

    printf("Status=");
    if (sym->status == SYM_NOSTATUS)
	printf("None\n");
    else {
	if (sym->status & SYM_USED)
	    printf("Used,");
	if (sym->status & SYM_DEFINED)
	    printf("Defined,");
	if (sym->status & SYM_VALUED)
	    printf("Valued,");
	if (sym->status & SYM_NOTINTABLE)
	    printf("Not in Table,");
	printf("\n");
    }

    printf("Visibility=");
    if (sym->visibility == SYM_LOCAL)
	printf("Local\n");
    else {
	if (sym->visibility & SYM_GLOBAL)
	    printf("Global,");
	if (sym->visibility & SYM_COMMON)
	    printf("Common,");
	if (sym->visibility & SYM_EXTERN)
	    printf("Extern,");
	printf("\n");
    }

    printf("Filename=\"%s\" Line Number=%lu\n", sym->filename, sym->line);
}
