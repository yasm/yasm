/* $IdPath$
 * YASM object format module interface header file
 *
 *  Copyright (C) 2001  Peter Johnson
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
#ifndef YASM_OBJFMT_H
#define YASM_OBJFMT_H

/* Interface to the object format module(s) */
struct yasm_objfmt {
    /* one-line description of the format */
    const char *name;

    /* keyword used to select format on the command line */
    const char *keyword;

    /* default output file extension (without the '.').
     * NULL means no extension, with no '.', while "" includes the '.'.
     */
    /*@null@*/ const char *extension;

    /* default (starting) section name */
    const char *default_section_name;

    /* default (starting) x86 BITS setting */
    const unsigned char default_x86_mode_bits;

    /* NULL-terminated list of debugging formats that are valid to use with
     * this object format.  The null debugging format (null_dbgfmt) should
     * always be in this list so it's possible to have no debugging output.
     */
    const char **dbgfmt_keywords;

    /* Default debugging format (set even if there's only one available to
     * use).
     */
    const char *default_dbgfmt_keyword;

    /* Initializes object output.  Must be called before any other object
     * format functions.  Should NOT open the object file; the filenames are
     * provided solely for informational purposes.
     */
    void (*initialize) (const char *in_filename, const char *obj_filename,
			yasm_dbgfmt *df, yasm_arch *a);

    /* Write out (post-optimized) sections to the object file.
     * This function may call symrec functions as necessary (including
     * symrec_traverse) to retrieve symbolic information.
     */
    void (*output) (FILE *f, yasm_sectionhead *sections);

    /* Cleans up anything allocated by initialize.
     * May be NULL if not needed by the object format.
     */
    void (*cleanup) (void);

    /* Switch object file sections.  The first val of the valparams should
     * be the section name.  Returns NULL if something's wrong, otherwise
     * returns the new section.
     */
    /*@observer@*/ /*@null@*/ yasm_section *
	(*sections_switch)(yasm_sectionhead *headp,
			   yasm_valparamhead *valparams,
			   /*@null@*/ yasm_valparamhead *objext_valparams,
			   unsigned long lindex);

    /* Object format-specific data handling functions for sections.
     * May be NULL if no data is ever allocated in sections_switch().
     */
    void (*section_data_delete)(/*@only@*/ void *data);
    void (*section_data_print)(FILE *f, int indent_level, void *data);

    /* These functions should call symrec_set_of_data() to store data.
     * May be NULL if objfmt doesn't care about such declarations.
     */
    void (*extern_declare)(yasm_symrec *sym,
			   /*@null@*/ yasm_valparamhead *objext_valparams,
			   unsigned long lindex);
    void (*global_declare)(yasm_symrec *sym,
			   /*@null@*/ yasm_valparamhead *objext_valparams,
			   unsigned long lindex);
    void (*common_declare)(yasm_symrec *sym, /*@only@*/ yasm_expr *size,
			   /*@null@*/ yasm_valparamhead *objext_valparams,
			   unsigned long lindex);

    /* May be NULL if symrec_set_of_data() is never called. */
    void (*symrec_data_delete)(/*@only@*/ void *data);
    void (*symrec_data_print)(FILE *f, int indent_level, void *data);

    /* Object format-specific directive support.  Returns 1 if directive was
     * not recognized.  Returns 0 if directive was recognized, even if it
     * wasn't valid.
     */
    int (*directive)(const char *name, yasm_valparamhead *valparams,
		     /*@null@*/ yasm_valparamhead *objext_valparams,
		     yasm_sectionhead *headp, unsigned long lindex);

    /* Bytecode objfmt data (BC_OBJFMT_DATA) handling functions.
     * May be NULL if no BC_OBJFMT_DATA is ever allocated by the object format.
     */
    void (*bc_objfmt_data_delete)(unsigned int type, /*@only@*/ void *data);
    void (*bc_objfmt_data_print)(FILE *f, int indent_level, unsigned int type,
				 const void *data);
};
#endif
