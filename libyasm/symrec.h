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

/** Create a new symbol table. */
yasm_symtab *yasm_symtab_create(void);

/** Destroy a symbol table and all internal symbols.
 * \param symtab    symbol table
 * \warning All yasm_symrec *'s into this symbol table become invalid after
 * this is called!
 */
void yasm_symtab_destroy(/*@only@*/ yasm_symtab *symtab);

/** Get a reference to (use) a symbol.  The symbol does not necessarily need to
 * be defined before it is used.
 * \param symtab    symbol table
 * \param name	    symbol name
 * \param line      virtual line where referenced
 * \return Symbol (dependent pointer, do not free).
 */
/*@dependent@*/ yasm_symrec *yasm_symtab_use
    (yasm_symtab *symtab, const char *name, unsigned long line);

/** Define a symbol as an EQU value.
 * \param symtab    symbol table
 * \param name	    symbol (EQU) name
 * \param e	    EQU value (expression)
 * \param line      virtual line of EQU
 * \return Symbol (dependent pointer, do not free).
 */
/*@dependent@*/ yasm_symrec *yasm_symtab_define_equ
    (yasm_symtab *symtab, const char *name, /*@keep@*/ yasm_expr *e,
     unsigned long line);

/** Define a symbol as a label.
 * \param symtab    symbol table
 * \param name	    symbol (label) name
 * \param precbc    bytecode preceding label
 * \param in_table  nonzero if the label should be inserted into the symbol
 *                  table (some specially-generated ones should not be)
 * \param line	    virtual line of label
 * \return Symbol (dependent pointer, do not free).
 */
/*@dependent@*/ yasm_symrec *yasm_symtab_define_label
    (yasm_symtab *symtab, const char *name,
     /*@dependent@*/ yasm_bytecode *precbc, int in_table, unsigned long line);

/** Define a symbol as a label, shortcut (no need to find out symtab, it's
 * determined from precbc).
 * \param name	    symbol (label) name
 * \param precbc    bytecode preceding label
 * \param in_table  nonzero if the label should be inserted into the symbol
 *                  table (some specially-generated ones should not be)
 * \param line	    virtual line of label
 * \return Symbol (dependent pointer, do not free).
 */
/*@dependent@*/ yasm_symrec *yasm_symtab_define_label2
    (const char *name, /*@dependent@*/ yasm_bytecode *precbc, int in_table,
     unsigned long line);

/** Declare external visibility of a symbol.
 * \note Not all visibility combinations are allowed.
 * \param symtab    symbol table
 * \param name	    symbol name
 * \param vis	    visibility
 * \param line      virtual line of visibility-setting
 * \return Symbol (dependent pointer, do not free).
 */
/*@dependent@*/ yasm_symrec *yasm_symtab_declare
    (yasm_symtab *symtab, const char *name, yasm_sym_vis vis,
     unsigned long line);

/** Callback function for yasm_symrec_traverse().
 * \param sym	    symbol
 * \param d	    data passed into yasm_symrec_traverse()
 * \return Nonzero to stop symbol traversal.
 */
typedef int (*yasm_symtab_traverse_callback)
    (yasm_symrec *sym, /*@null@*/ void *d);

/** Traverse all symbols in the symbol table.
 * \param symtab    symbol table
 * \param d	    data to pass to each call of callback function
 * \param func	    callback function called on each symbol
 * \return Nonzero value returned by callback function if it ever returned
 *         nonzero.
 */
int /*@alt void@*/ yasm_symtab_traverse
    (yasm_symtab *symtab, /*@null@*/ void *d,
     yasm_symtab_traverse_callback func);

/** Finalize symbol table after parsing stage.  Checks for symbols that are
 * used but never defined or declared #YASM_SYM_EXTERN or #YASM_SYM_COMMON.
 */
void yasm_symtab_parser_finalize(yasm_symtab *symtab);

/** Print the symbol table.  For debugging purposes.
 * \param symtab	symbol table
 * \param f		file
 * \param indent_level	indentation level
 */
void yasm_symtab_print(yasm_symtab *symtab, FILE *f, int indent_level);

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

/** Dependent pointer to a bytecode. */
typedef /*@dependent@*/ yasm_bytecode *yasm_symrec_get_label_bytecodep;

/** Get the label location of a symbol.
 * \param sym	    symbol
 * \param precbc    bytecode preceding label (output)
 * \return 0 if not symbol is not a label or if the symbol's visibility is
 *         #YASM_SYM_EXTERN or #YASM_SYM_COMMON (not defined in the file).
 */
int yasm_symrec_get_label(const yasm_symrec *sym,
			  /*@out@*/ yasm_symrec_get_label_bytecodep *precbc);

/** Get associated data for a symbol and data callback.
 * \param sym	    symbol
 * \param callback  callback used when adding data
 * \return Associated data (NULL if none).
 */
/*@dependent@*/ /*@null@*/ void *yasm_symrec_get_data
    (yasm_symrec *sym, const yasm_assoc_data_callback *callback);

/** Add associated data to a symbol.
 * \attention Deletes any existing associated data for that data callback.
 * \param sym	    symbol
 * \param callback  callback
 * \param data	    data to associate
 */
void yasm_symrec_add_data(yasm_symrec *sym,
			  const yasm_assoc_data_callback *callback,
			  /*@only@*/ /*@null@*/ void *data);

/** Print a symbol.  For debugging purposes.
 * \param f		file
 * \param indent_level	indentation level
 * \param sym		symbol
 */
void yasm_symrec_print(const yasm_symrec *sym, FILE *f, int indent_level);

#endif
