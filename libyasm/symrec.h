/**
 * \file libyasm/symrec.h
 * \brief YASM symbol table interface.
 *
 * \rcs
 * $IdPath$
 * \endrcs
 *
 * \license
 *  Copyright (C) 2001  Michael Urman, Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
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
 * \endlicense
 */
#ifndef YASM_SYMREC_H
#define YASM_SYMREC_H

/** Initialize symbol table internal data structures. */
void yasm_symrec_initialize(void);

/** Clean up internal symbol table allocations. */
void yasm_symrec_cleanup(void);

/** Get a reference to (use) a symbol.  The symbol does not necessarily need to
 * be defined before it is used.
 * \param name	    symbol name
 * \param lindex    line index where referenced
 * \return Symbol (dependent pointer, do not free).
 */
/*@dependent@*/ yasm_symrec *yasm_symrec_use(const char *name,
					     unsigned long lindex);

/** Define a symbol as an EQU value.
 * \param name	    symbol (EQU) name
 * \param e	    EQU value (expression)
 * \param lindex    line index of EQU
 * \return Symbol (dependent pointer, do not free).
 */
/*@dependent@*/ yasm_symrec *yasm_symrec_define_equ
    (const char *name, /*@keep@*/ yasm_expr *e, unsigned long lindex);

/** Define a symbol as a label.
 * \param name	    symbol (label) name
 * \param sect	    section containing label
 * \param precbc    bytecode preceding label (NULL if label is before first
 *                  bytecode in section)
 * \param in_table  nonzero if the label should be inserted into the symbol
 *                  table (some specially-generated ones should not be)
 * \param lindex    line index of label
 * \return Symbol (dependent pointer, do not free).
 */
/*@dependent@*/ yasm_symrec *yasm_symrec_define_label
    (const char *name, /*@dependent@*/ /*@null@*/ yasm_section *sect,
     /*@dependent@*/ /*@null@*/ yasm_bytecode *precbc, int in_table,
     unsigned long lindex);

/** Declare external visibility of a symbol.
 * \note Not all visibility combinations are allowed.
 * \param name	    symbol name
 * \param vis	    visibility
 * \param lindex    line index of visibility-setting
 * \return Symbol (dependent pointer, do not free).
 */
/*@dependent@*/ yasm_symrec *yasm_symrec_declare
    (const char *name, yasm_sym_vis vis, unsigned long lindex);

/** Get the name of a symbol.
 * \param sym	    symbol
 * \return Symbol name.
 */
/*@observer@*/ const char *yasm_symrec_get_name(const yasm_symrec *sym);

/** Get the visibility of a symbol.
 * \param sym	    symbol
 * \return Symbol visibility.
 */
yasm_sym_vis yasm_symrec_get_visibility(const yasm_symrec *sym);

/** Get EQU value of a symbol.
 * \param sym	    symbol
 * \return EQU value, or NULL if symbol is not an EQU or is not defined.
 */
/*@observer@*/ /*@null@*/ const yasm_expr *yasm_symrec_get_equ
    (const yasm_symrec *sym);

/** Dependent pointer to a section. */
typedef /*@dependent@*/ /*@null@*/ yasm_section *
    yasm_symrec_get_label_sectionp;

/** Dependent pointer to a bytecode. */
typedef /*@dependent@*/ /*@null@*/ yasm_bytecode *
    yasm_symrec_get_label_bytecodep;

/** Get the label location of a symbol.
 * \param sym	    symbol
 * \param sect	    section containing label (output)
 * \param precbc    bytecode preceding label (output); NULL if no preceding
 *                  bytecode in section.
 * \return 0 if not symbol is not a label or if the symbol's visibility is
 *         #YASM_SYM_EXTERN or #YASM_SYM_COMMON (not defined in the file).
 */
int yasm_symrec_get_label(const yasm_symrec *sym,
			  /*@out@*/ yasm_symrec_get_label_sectionp *sect,
			  /*@out@*/ yasm_symrec_get_label_bytecodep *precbc);

/** Get yasm_optimizer-specific flags.  For yasm_optimizer use only.
 * \param sym	    symbol
 * \return Optimizer-specific flags.
 */
unsigned long yasm_symrec_get_opt_flags(const yasm_symrec *sym);

/** Set yasm_optimizer-specific flags.  For yasm_optimizer use only.
 * \param sym	    symbol
 * \param opt_flags optimizer-specific flags.
 */
void yasm_symrec_set_opt_flags(yasm_symrec *sym, unsigned long opt_flags);

/** Get yasm_objfmt-specific data.  For yasm_objfmt use only.
 * \param sym	    symbol
 * \return Object format-specific data.
 */
/*@dependent@*/ /*@null@*/ void *yasm_symrec_get_of_data(yasm_symrec *sym);

/** Set yasm_objfmt-specific data.  For yasm_objfmt use only.
 * \attention Deletes any existing of_data.
 * \param sym	    symbol
 * \param of	    object format
 * \param of_data   object format-specific data.
 */
void yasm_symrec_set_of_data(yasm_symrec *sym, yasm_objfmt *of,
			     /*@only@*/ /*@null@*/ void *of_data);

/** Callback function for yasm_symrec_traverse().
 * \param sym	    symbol
 * \param d	    data passed into yasm_symrec_traverse()
 * \return Nonzero to stop symbol traversal.
 */
typedef int (*yasm_symrec_traverse_callback)
    (yasm_symrec *sym, /*@null@*/ void *d);

/** Traverse all symbols in the symbol table.
 * \param d	data to pass to each call of callback function
 * \param func	callback function called on each symbol
 * \return Nonzero value returned by callback function if it ever returned
 *         nonzero.
 */
int /*@alt void@*/ yasm_symrec_traverse
    (/*@null@*/ void *d, yasm_symrec_traverse_callback func);

/** Finalize symbol table after parsing stage.  Checks for symbols that are
 * used but never defined or declared #YASM_SYM_EXTERN or #YASM_SYM_COMMON.
 */
void yasm_symrec_parser_finalize(void);

/** Print the symbol table.  For debugging purposes.
 * \param f		file
 * \param indent_level	indentation level
 */
void yasm_symrec_print_all(FILE *f, int indent_level);

/** Print a symbol.  For debugging purposes.
 * \param f		file
 * \param indent_level	indentation level
 * \param sym		symbol
 */
void yasm_symrec_print(FILE *f, int indent_level, const yasm_symrec *sym);

#endif
