/* $IdPath$
 * Symbol table handling header file
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
#ifndef YASM_SYMREC_H
#define YASM_SYMREC_H

void yasm_symrec_initialize(yasm_errwarn *we);

/*@dependent@*/ yasm_symrec *yasm_symrec_use(const char *name,
					     unsigned long lindex);
/*@dependent@*/ yasm_symrec *yasm_symrec_define_equ
    (const char *name, /*@keep@*/ yasm_expr *e, unsigned long lindex);
/* in_table specifies if the label should be inserted into the symbol table.
 * All labels are memory managed internally.
 */
/*@dependent@*/ yasm_symrec *yasm_symrec_define_label
    (const char *name, /*@dependent@*/ /*@null@*/ yasm_section *sect,
     /*@dependent@*/ /*@null@*/ yasm_bytecode *precbc, int in_table,
     unsigned long lindex);
/*@dependent@*/ yasm_symrec *yasm_symrec_declare
    (const char *name, yasm_sym_vis vis, unsigned long lindex);

/*@observer@*/ const char *yasm_symrec_get_name(const yasm_symrec *sym);
yasm_sym_vis yasm_symrec_get_visibility(const yasm_symrec *sym);

/*@observer@*/ /*@null@*/ const yasm_expr *yasm_symrec_get_equ
    (const yasm_symrec *sym);
/* Returns 0 if not a label or if EXTERN/COMMON (not defined in the file) */
typedef /*@dependent@*/ /*@null@*/ yasm_section *
    yasm_symrec_get_label_sectionp;
typedef /*@dependent@*/ /*@null@*/ yasm_bytecode *
    yasm_symrec_get_label_bytecodep;
int yasm_symrec_get_label(const yasm_symrec *sym,
			  /*@out@*/ yasm_symrec_get_label_sectionp *sect,
			  /*@out@*/ yasm_symrec_get_label_bytecodep *precbc);

/* Get and set optimizer flags */
unsigned long yasm_symrec_get_opt_flags(const yasm_symrec *sym);
void yasm_symrec_set_opt_flags(yasm_symrec *sym, unsigned long opt_flags);

/*@dependent@*/ /*@null@*/ void *yasm_symrec_get_of_data(yasm_symrec *sym);

/* Caution: deletes any existing of_data */
void yasm_symrec_set_of_data(yasm_symrec *sym, yasm_objfmt *of,
			     /*@only@*/ /*@null@*/ void *of_data);

int /*@alt void@*/ yasm_symrec_traverse
    (/*@null@*/ void *d, int (*func) (yasm_symrec *sym, /*@null@*/ void *d));

void yasm_symrec_parser_finalize(void);

void yasm_symrec_cleanup(void);

void yasm_symrec_print_all(FILE *f, int indent_level);

void yasm_symrec_print(FILE *f, int indent_level, const yasm_symrec *sym);
#endif
