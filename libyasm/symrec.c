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

#include "hamt.h"

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
	struct label_s {	/* bytecode immediately preceding a label */
	    /*@dependent@*/ /*@null@*/ yasm_section *sect;
	    /*@dependent@*/ /*@null@*/ yasm_bytecode *bc;
	} label;
    } value;

    /* objfmt-specific data */
    /*@null@*/ /*@dependent@*/ yasm_objfmt *of;
    /*@null@*/ /*@owned@*/ void *of_data;

    /* storage for optimizer flags */
    unsigned long opt_flags;
};

/* The symbol table: a hash array mapped trie (HAMT). */
static /*@only@*/ HAMT *sym_table;

/* Linked list of symbols not in the symbol table. */
typedef struct non_table_symrec_s {
     /*@reldef@*/ SLIST_ENTRY(non_table_symrec_s) link;
     /*@owned@*/ yasm_symrec *rec;
} non_table_symrec;
typedef /*@reldef@*/ SLIST_HEAD(nontablesymhead_s, non_table_symrec_s)
	nontablesymhead;
static /*@only@*/ nontablesymhead *non_table_syms;


void
yasm_symrec_initialize(void)
{
    sym_table = HAMT_new(yasm_internal_error_);
    non_table_syms = yasm_xmalloc(sizeof(nontablesymhead));
    SLIST_INIT(non_table_syms);
}

static void
symrec_delete_one(/*@only@*/ void *d)
{
    yasm_symrec *sym = d;
    yasm_xfree(sym->name);
    if (sym->type == SYM_EQU)
	yasm_expr_delete(sym->value.expn);
    if (sym->of_data && sym->of) {
	if (sym->of->symrec_data_delete)
	    sym->of->symrec_data_delete(sym->of_data);
	else
	    yasm_internal_error(
		N_("don't know how to delete objfmt-specific data"));
    }
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
    rec->of = NULL;
    rec->of_data = NULL;
    rec->opt_flags = 0;
    return rec;
}

static /*@partial@*/ /*@dependent@*/ yasm_symrec *
symrec_get_or_new_in_table(/*@only@*/ char *name)
{
    yasm_symrec *rec = symrec_new_common(name);
    int replace = 0;

    rec->status = SYM_NOSTATUS;

    return HAMT_insert(sym_table, name, rec, &replace, symrec_delete_one);
}

static /*@partial@*/ /*@dependent@*/ yasm_symrec *
symrec_get_or_new_not_in_table(/*@only@*/ char *name)
{
    non_table_symrec *sym = yasm_xmalloc(sizeof(non_table_symrec));
    sym->rec = symrec_new_common(name);

    sym->rec->status = SYM_NOTINTABLE;

    SLIST_INSERT_HEAD(non_table_syms, sym, link);

    return sym->rec;
}

/* create a new symrec */
/*@-freshtrans -mustfree@*/
static /*@partial@*/ /*@dependent@*/ yasm_symrec *
symrec_get_or_new(const char *name, int in_table)
{
    char *symname = yasm__xstrdup(name);

    if (in_table)
	return symrec_get_or_new_in_table(symname);
    else
	return symrec_get_or_new_not_in_table(symname);
}
/*@=freshtrans =mustfree@*/

/* Call a function with each symrec.  Stops early if 0 returned by func.
   Returns 0 if stopped early. */
int
yasm_symrec_traverse(void *d, int (*func) (yasm_symrec *sym, void *d))
{
    return HAMT_traverse(sym_table, d, (int (*) (void *, void *))func);
}

yasm_symrec *
yasm_symrec_use(const char *name, unsigned long lindex)
{
    yasm_symrec *rec = symrec_get_or_new(name, 1);
    if (rec->line == 0)
	rec->line = lindex;	/* set line number of first use */
    rec->status |= SYM_USED;
    return rec;
}

static /*@dependent@*/ yasm_symrec *
symrec_define(const char *name, sym_type type, int in_table,
	      unsigned long lindex)
{
    yasm_symrec *rec = symrec_get_or_new(name, in_table);

    /* Has it been defined before (either by DEFINED or COMMON/EXTERN)? */
    if ((rec->status & SYM_DEFINED) ||
	(rec->visibility & (YASM_SYM_COMMON | YASM_SYM_EXTERN))) {
	yasm__error(lindex,
	    N_("duplicate definition of `%s'; first defined on line %lu"),
	    name, rec->line);
    } else {
	rec->line = lindex;	/* set line number of definition */
	rec->type = type;
	rec->status |= SYM_DEFINED;
    }
    return rec;
}

yasm_symrec *
yasm_symrec_define_equ(const char *name, yasm_expr *e, unsigned long lindex)
{
    yasm_symrec *rec = symrec_define(name, SYM_EQU, 1, lindex);
    rec->value.expn = e;
    rec->status |= SYM_VALUED;
    return rec;
}

yasm_symrec *
yasm_symrec_define_label(const char *name, yasm_section *sect,
			 yasm_bytecode *precbc, int in_table,
			 unsigned long lindex)
{
    yasm_symrec *rec = symrec_define(name, SYM_LABEL, in_table, lindex);
    rec->value.label.sect = sect;
    rec->value.label.bc = precbc;
    return rec;
}

yasm_symrec *
yasm_symrec_declare(const char *name, yasm_sym_vis vis, unsigned long lindex)
{
    yasm_symrec *rec = symrec_get_or_new(name, 1);

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
	yasm__error(lindex,
	    N_("duplicate definition of `%s'; first defined on line %lu"),
	    name, rec->line);
    return rec;
}

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
		      yasm_symrec_get_label_sectionp *sect,
		      yasm_symrec_get_label_bytecodep *precbc)
{
    if (sym->type != SYM_LABEL) {
	*sect = (yasm_symrec_get_label_sectionp)0xDEADBEEF;
	*precbc = (yasm_symrec_get_label_bytecodep)0xDEADBEEF;
	return 0;
    }
    *sect = sym->value.label.sect;
    *precbc = sym->value.label.bc;
    return 1;
}

unsigned long
yasm_symrec_get_opt_flags(const yasm_symrec *sym)
{
    return sym->opt_flags;
}

void
yasm_symrec_set_opt_flags(yasm_symrec *sym, unsigned long opt_flags)
{
    sym->opt_flags = opt_flags;
}

void *
yasm_symrec_get_of_data(yasm_symrec *sym)
{
    return sym->of_data;
}

void
yasm_symrec_set_of_data(yasm_symrec *sym, yasm_objfmt *of, void *of_data)
{
    if (sym->of_data && sym->of) {
	if (sym->of->symrec_data_delete)
	    sym->of->symrec_data_delete(sym->of_data);
	else
	    yasm_internal_error(
		N_("don't know how to delete objfmt-specific data"));
    }
    sym->of = of;
    sym->of_data = of_data;
}

static unsigned long firstundef_line;
static int
symrec_parser_finalize_checksym(yasm_symrec *sym,
				/*@unused@*/ /*@null@*/ void *d)
{
    /* error if a symbol is used but never defined or extern/common declared */
    if ((sym->status & SYM_USED) && !(sym->status & SYM_DEFINED) &&
	!(sym->visibility & (YASM_SYM_EXTERN | YASM_SYM_COMMON))) {
	yasm__error(sym->line, N_("undefined symbol `%s' (first use)"),
		    sym->name);
	if (sym->line < firstundef_line)
	    firstundef_line = sym->line;
    }

    return 1;
}

void
yasm_symrec_parser_finalize(void)
{
    firstundef_line = ULONG_MAX;
    yasm_symrec_traverse(NULL, symrec_parser_finalize_checksym);
    if (firstundef_line < ULONG_MAX)
	yasm__error(firstundef_line,
		    N_(" (Each undefined symbol is reported only once.)"));
}

void
yasm_symrec_cleanup(void)
{
    HAMT_delete(sym_table, symrec_delete_one);

    while (!SLIST_EMPTY(non_table_syms)) {
	non_table_symrec *sym = SLIST_FIRST(non_table_syms);
	SLIST_REMOVE_HEAD(non_table_syms, link);
	symrec_delete_one(sym->rec);
	yasm_xfree(sym);
    }
    yasm_xfree(non_table_syms);
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
    yasm_symrec_print(data->f, data->indent_level+1, sym);
    return 1;
}

void
yasm_symrec_print_all(FILE *f, int indent_level)
{
    symrec_print_data data;
    data.f = f;
    data.indent_level = indent_level;
    yasm_symrec_traverse(&data, symrec_print_wrapper);
}
/*@=voidabstract@*/

void
yasm_symrec_print(FILE *f, int indent_level, const yasm_symrec *sym)
{
    switch (sym->type) {
	case SYM_UNKNOWN:
	    fprintf(f, "%*s-Unknown (Common/Extern)-\n", indent_level, "");
	    break;
	case SYM_EQU:
	    fprintf(f, "%*s_EQU_\n", indent_level, "");
	    fprintf(f, "%*sExpn=", indent_level, "");
	    yasm_expr_print(f, sym->value.expn);
	    fprintf(f, "\n");
	    break;
	case SYM_LABEL:
	    fprintf(f, "%*s_Label_\n%*sSection:\n", indent_level, "",
		    indent_level, "");
	    yasm_section_print(f, indent_level+1, sym->value.label.sect, 0);
	    if (!sym->value.label.bc)
		fprintf(f, "%*sFirst bytecode\n", indent_level, "");
	    else {
		fprintf(f, "%*sPreceding bytecode:\n", indent_level, "");
		yasm_bc_print(f, indent_level+1, sym->value.label.bc);
	    }
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

    if (sym->of_data && sym->of) {
	fprintf(f, "%*sObject format-specific data:\n", indent_level, "");
	if (sym->of->symrec_data_print)
	    sym->of->symrec_data_print(f, indent_level+1, sym->of_data);
	else
	    fprintf(f, "%*sUNKNOWN\n", indent_level+1, "");
    }

    fprintf(f, "%*sLine Index=%lu\n", indent_level, "", sym->line);
}
