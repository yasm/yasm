/*
 * Symbol table handling
 *
 *  Copyright (C) 2001  Michael Urman, Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#define YASM_LIB_INTERNAL
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#ifdef STDC_HEADERS
# include <limits.h>
#endif

#include "coretype.h"
#include "hamt.h"
#include "assocdat.h"

#include "errwarn.h"
#include "floatnum.h"
#include "expr.h"
#include "symrec.h"

#include "bytecode.h"
#include "section.h"
#include "objfmt.h"


/* DEFINED is set with EXTERN and COMMON below */
typedef enum {
    SYM_NOSTATUS = 0,
    SYM_USED = 1 << 0,		/* for using variables before definition */
    SYM_DEFINED = 1 << 1,	/* once it's been defined in the file */
    SYM_VALUED = 1 << 2,	/* once its value has been determined */
    SYM_NOTINTABLE = 1 << 3	/* if it's not in sym_table (ex. '$') */
} sym_status;

typedef enum {
    SYM_UNKNOWN,		/* for unknown type (COMMON/EXTERN) */
    SYM_EQU,			/* for EQU defined symbols (expressions) */
    SYM_LABEL			/* for labels */
} sym_type;

struct yasm_symrec {
    char *name;
    sym_type type;
    sym_status status;
    yasm_sym_vis visibility;
    unsigned long line;		/*  symbol was first declared or used on */
    union {
	yasm_expr *expn;	/* equ value */

	/* bytecode immediately preceding a label */
	/*@dependent@*/ yasm_bytecode *precbc;
    } value;

    /* associated data; NULL if none */
    /*@null@*/ /*@only@*/ yasm__assoc_data *assoc_data;
};

/* Linked list of symbols not in the symbol table. */
typedef struct non_table_symrec_s {
     /*@reldef@*/ SLIST_ENTRY(non_table_symrec_s) link;
     /*@owned@*/ yasm_symrec *rec;
} non_table_symrec;

struct yasm_symtab {
    /* The symbol table: a hash array mapped trie (HAMT). */
    /*@only@*/ HAMT *sym_table;
    /* Symbols not in the table */
    SLIST_HEAD(nontablesymhead_s, non_table_symrec_s) non_table_syms;
};


yasm_symtab *
yasm_symtab_create(void)
{
    yasm_symtab *symtab = yasm_xmalloc(sizeof(yasm_symtab));
    symtab->sym_table = HAMT_create(yasm_internal_error_);
    SLIST_INIT(&symtab->non_table_syms);
    return symtab;
}

static void
symrec_destroy_one(/*@only@*/ void *d)
{
    yasm_symrec *sym = d;
    yasm_xfree(sym->name);
    if (sym->type == SYM_EQU)
	yasm_expr_destroy(sym->value.expn);
    yasm__assoc_data_destroy(sym->assoc_data);
    yasm_xfree(sym);
}

static /*@partial@*/ yasm_symrec *
symrec_new_common(/*@keep@*/ char *name)
{
    yasm_symrec *rec = yasm_xmalloc(sizeof(yasm_symrec));
    rec->name = name;
    rec->type = SYM_UNKNOWN;
    rec->line = 0;
    rec->visibility = YASM_SYM_LOCAL;
    rec->assoc_data = NULL;
    return rec;
}

static /*@partial@*/ /*@dependent@*/ yasm_symrec *
symtab_get_or_new_in_table(yasm_symtab *symtab, /*@only@*/ char *name)
{
    yasm_symrec *rec = symrec_new_common(name);
    int replace = 0;

    rec->status = SYM_NOSTATUS;

    return HAMT_insert(symtab->sym_table, name, rec, &replace,
		       symrec_destroy_one);
}

static /*@partial@*/ /*@dependent@*/ yasm_symrec *
symtab_get_or_new_not_in_table(yasm_symtab *symtab, /*@only@*/ char *name)
{
    non_table_symrec *sym = yasm_xmalloc(sizeof(non_table_symrec));
    sym->rec = symrec_new_common(name);

    sym->rec->status = SYM_NOTINTABLE;

    SLIST_INSERT_HEAD(&symtab->non_table_syms, sym, link);

    return sym->rec;
}

/* create a new symrec */
/*@-freshtrans -mustfree@*/
static /*@partial@*/ /*@dependent@*/ yasm_symrec *
symtab_get_or_new(yasm_symtab *symtab, const char *name, int in_table)
{
    char *symname = yasm__xstrdup(name);

    if (in_table)
	return symtab_get_or_new_in_table(symtab, symname);
    else
	return symtab_get_or_new_not_in_table(symtab, symname);
}
/*@=freshtrans =mustfree@*/

/* Call a function with each symrec.  Stops early if 0 returned by func.
   Returns 0 if stopped early. */
int
yasm_symtab_traverse(yasm_symtab *symtab, void *d,
		     int (*func) (yasm_symrec *sym, void *d))
{
    return HAMT_traverse(symtab->sym_table, d, (int (*) (void *, void *))func);
}

yasm_symrec *
yasm_symtab_use(yasm_symtab *symtab, const char *name, unsigned long line)
{
    yasm_symrec *rec = symtab_get_or_new(symtab, name, 1);
    if (rec->line == 0)
	rec->line = line;	/* set line number of first use */
    rec->status |= SYM_USED;
    return rec;
}

static /*@dependent@*/ yasm_symrec *
symtab_define(yasm_symtab *symtab, const char *name, sym_type type,
	      int in_table, unsigned long line)
{
    yasm_symrec *rec = symtab_get_or_new(symtab, name, in_table);

    /* Has it been defined before (either by DEFINED or COMMON/EXTERN)? */
    if ((rec->status & SYM_DEFINED) ||
	(rec->visibility & (YASM_SYM_COMMON | YASM_SYM_EXTERN))) {
	yasm__error(line, N_("redefinition of `%s'"), name);
	yasm__error_at(line, rec->line, N_("`%s' previously defined here"),
		       name);
    } else {
	rec->line = line;	/* set line number of definition */
	rec->type = type;
	rec->status |= SYM_DEFINED;
    }
    return rec;
}

yasm_symrec *
yasm_symtab_define_equ(yasm_symtab *symtab, const char *name, yasm_expr *e,
		       unsigned long line)
{
    yasm_symrec *rec = symtab_define(symtab, name, SYM_EQU, 1, line);
    rec->value.expn = e;
    rec->status |= SYM_VALUED;
    return rec;
}

yasm_symrec *
yasm_symtab_define_label(yasm_symtab *symtab, const char *name,
			 yasm_bytecode *precbc, int in_table,
			 unsigned long line)
{
    yasm_symrec *rec;
    rec = symtab_define(symtab, name, SYM_LABEL, in_table, line);
    rec->value.precbc = precbc;
    return rec;
}

yasm_symrec *
yasm_symtab_define_label2(const char *name, yasm_bytecode *precbc,
			  int in_table, unsigned long line)
{
    return yasm_symtab_define_label(yasm_object_get_symtab(
	yasm_section_get_object(yasm_bc_get_section(precbc))), name, precbc,
	in_table, line);
}

yasm_symrec *
yasm_symtab_declare(yasm_symtab *symtab, const char *name, yasm_sym_vis vis,
		    unsigned long line)
{
    yasm_symrec *rec = symtab_get_or_new(symtab, name, 1);

    /* Allowable combinations:
     *  Existing State--------------  vis  New State-------------------
     *  DEFINED GLOBAL COMMON EXTERN  GCE  DEFINED GLOBAL COMMON EXTERN
     *     0      -      0      0     GCE     0      G      C      E
     *     0      -      0      1     GE      0      G      0      E
     *     0      -      1      0     GC      0      G      C      0
     * X   0      -      1      1
     *     1      -      0      0      G      1      G      0      0
     * X   1      -      -      1
     * X   1      -      1      -
     */
    if ((vis == YASM_SYM_GLOBAL) ||
	(!(rec->status & SYM_DEFINED) &&
	 (!(rec->visibility & (YASM_SYM_COMMON | YASM_SYM_EXTERN)) ||
	  ((rec->visibility & YASM_SYM_COMMON) && (vis == YASM_SYM_COMMON)) ||
	  ((rec->visibility & YASM_SYM_EXTERN) && (vis == YASM_SYM_EXTERN)))))
	rec->visibility |= vis;
    else
	yasm__error(line,
	    N_("duplicate definition of `%s'; first defined on line %lu"),
	    name, rec->line);
    return rec;
}

static int
symtab_parser_finalize_checksym(yasm_symrec *sym, /*@null@*/ void *d)
{
    unsigned long *firstundef_line = d;
    /* error if a symbol is used but never defined or extern/common declared */
    if ((sym->status & SYM_USED) && !(sym->status & SYM_DEFINED) &&
	!(sym->visibility & (YASM_SYM_EXTERN | YASM_SYM_COMMON))) {
	yasm__error(sym->line, N_("undefined symbol `%s' (first use)"),
		    sym->name);
	if (sym->line < *firstundef_line)
	    *firstundef_line = sym->line;
    }

    return 1;
}

void
yasm_symtab_parser_finalize(yasm_symtab *symtab)
{
    unsigned long firstundef_line = ULONG_MAX;
    yasm_symtab_traverse(symtab, &firstundef_line,
			 symtab_parser_finalize_checksym);
    if (firstundef_line < ULONG_MAX)
	yasm__error(firstundef_line,
		    N_(" (Each undefined symbol is reported only once.)"));
}

void
yasm_symtab_destroy(yasm_symtab *symtab)
{
    HAMT_destroy(symtab->sym_table, symrec_destroy_one);

    while (!SLIST_EMPTY(&symtab->non_table_syms)) {
	non_table_symrec *sym = SLIST_FIRST(&symtab->non_table_syms);
	SLIST_REMOVE_HEAD(&symtab->non_table_syms, link);
	symrec_destroy_one(sym->rec);
	yasm_xfree(sym);
    }

    yasm_xfree(symtab);
}

typedef struct symrec_print_data {
    FILE *f;
    int indent_level;
} symrec_print_data;

/*@+voidabstract@*/
static int
symrec_print_wrapper(yasm_symrec *sym, /*@null@*/ void *d)
{
    symrec_print_data *data = (symrec_print_data *)d;
    assert(data != NULL);
    fprintf(data->f, "%*sSymbol `%s'\n", data->indent_level, "", sym->name);
    yasm_symrec_print(sym, data->f, data->indent_level+1);
    return 1;
}

void
yasm_symtab_print(yasm_symtab *symtab, FILE *f, int indent_level)
{
    symrec_print_data data;
    data.f = f;
    data.indent_level = indent_level;
    yasm_symtab_traverse(symtab, &data, symrec_print_wrapper);
}
/*@=voidabstract@*/

const char *
yasm_symrec_get_name(const yasm_symrec *sym)
{
    return sym->name;
}

yasm_sym_vis
yasm_symrec_get_visibility(const yasm_symrec *sym)
{
    return sym->visibility;
}

const yasm_expr *
yasm_symrec_get_equ(const yasm_symrec *sym)
{
    if (sym->type == SYM_EQU)
	return sym->value.expn;
    return (const yasm_expr *)NULL;
}

int
yasm_symrec_get_label(const yasm_symrec *sym,
		      yasm_symrec_get_label_bytecodep *precbc)
{
    if (sym->type != SYM_LABEL) {
	*precbc = (yasm_symrec_get_label_bytecodep)0xDEADBEEF;
	return 0;
    }
    *precbc = sym->value.precbc;
    return 1;
}

void *
yasm_symrec_get_data(yasm_symrec *sym,
		     const yasm_assoc_data_callback *callback)
{
    return yasm__assoc_data_get(sym->assoc_data, callback);
}

void
yasm_symrec_add_data(yasm_symrec *sym,
		     const yasm_assoc_data_callback *callback, void *data)
{
    sym->assoc_data = yasm__assoc_data_add(sym->assoc_data, callback, data);
}

void
yasm_symrec_print(const yasm_symrec *sym, FILE *f, int indent_level)
{
    switch (sym->type) {
	case SYM_UNKNOWN:
	    fprintf(f, "%*s-Unknown (Common/Extern)-\n", indent_level, "");
	    break;
	case SYM_EQU:
	    fprintf(f, "%*s_EQU_\n", indent_level, "");
	    fprintf(f, "%*sExpn=", indent_level, "");
	    yasm_expr_print(sym->value.expn, f);
	    fprintf(f, "\n");
	    break;
	case SYM_LABEL:
	    fprintf(f, "%*s_Label_\n%*sSection:\n", indent_level, "",
		    indent_level, "");
	    yasm_section_print(yasm_bc_get_section(sym->value.precbc), f,
			       indent_level+1, 0);
	    fprintf(f, "%*sPreceding bytecode:\n", indent_level, "");
	    yasm_bc_print(sym->value.precbc, f, indent_level+1);
	    break;
    }

    fprintf(f, "%*sStatus=", indent_level, "");
    if (sym->status == SYM_NOSTATUS)
	fprintf(f, "None\n");
    else {
	if (sym->status & SYM_USED)
	    fprintf(f, "Used,");
	if (sym->status & SYM_DEFINED)
	    fprintf(f, "Defined,");
	if (sym->status & SYM_VALUED)
	    fprintf(f, "Valued,");
	if (sym->status & SYM_NOTINTABLE)
	    fprintf(f, "Not in Table,");
	fprintf(f, "\n");
    }

    fprintf(f, "%*sVisibility=", indent_level, "");
    if (sym->visibility == YASM_SYM_LOCAL)
	fprintf(f, "Local\n");
    else {
	if (sym->visibility & YASM_SYM_GLOBAL)
	    fprintf(f, "Global,");
	if (sym->visibility & YASM_SYM_COMMON)
	    fprintf(f, "Common,");
	if (sym->visibility & YASM_SYM_EXTERN)
	    fprintf(f, "Extern,");
	fprintf(f, "\n");
    }

    if (sym->assoc_data) {
	fprintf(f, "%*sAssociated data:\n", indent_level, "");
	yasm__assoc_data_print(sym->assoc_data, f, indent_level+1);
    }

    fprintf(f, "%*sLine Index=%lu\n", indent_level, "", sym->line);
}
