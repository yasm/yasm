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

void symrec_initialize(errwarn *we);

/*@dependent@*/ symrec *symrec_use(const char *name, unsigned long lindex);
/*@dependent@*/ symrec *symrec_define_equ(const char *name, /*@keep@*/ expr *e,
					  unsigned long lindex);
/* in_table specifies if the label should be inserted into the symbol table.
 * All labels are memory managed internally.
 */
/*@dependent@*/ symrec *symrec_define_label(const char *name,
					    /*@dependent@*/ /*@null@*/
					    section *sect,
					    /*@dependent@*/ /*@null@*/
					    bytecode *precbc, int in_table,
					    unsigned long lindex);
/*@dependent@*/ symrec *symrec_declare(const char *name, SymVisibility vis,
				       unsigned long lindex);

/*@observer@*/ const char *symrec_get_name(const symrec *sym);
SymVisibility symrec_get_visibility(const symrec *sym);

/*@observer@*/ /*@null@*/ const expr *symrec_get_equ(const symrec *sym);
/* Returns 0 if not a label or if EXTERN/COMMON (not defined in the file) */
typedef /*@dependent@*/ /*@null@*/ section *symrec_get_label_sectionp;
typedef /*@dependent@*/ /*@null@*/ bytecode *symrec_get_label_bytecodep;
int symrec_get_label(const symrec *sym,
		     /*@out@*/ symrec_get_label_sectionp *sect,
		     /*@out@*/ symrec_get_label_bytecodep *precbc);

/* Get and set optimizer flags */
unsigned long symrec_get_opt_flags(const symrec *sym);
void symrec_set_opt_flags(symrec *sym, unsigned long opt_flags);

/*@dependent@*/ /*@null@*/ void *symrec_get_of_data(symrec *sym);

/* Caution: deletes any existing of_data */
void symrec_set_of_data(symrec *sym, objfmt *of,
			/*@only@*/ /*@null@*/ void *of_data);

int /*@alt void@*/ symrec_traverse(/*@null@*/ void *d,
				   int (*func) (symrec *sym,
						/*@null@*/ void *d));

void symrec_parser_finalize(void);

void symrec_cleanup(void);

void symrec_print_all(FILE *f, int indent_level);

void symrec_print(FILE *f, int indent_level, const symrec *sym);
#endif
