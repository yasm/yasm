/* $IdPath$
 * Object format module interface header file
 *
 *  Copyright (C) 2001  Peter Johnson
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
#ifndef YASM_OBJFMT_H
#define YASM_OBJFMT_H

/* Interface to the object format module(s) */
struct objfmt {
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
     * this object format.
     */
/*    debugfmt **debugfmts;*/

    /* Default debugging format (set even if there's only one available to
     * use)
     */
/*    debugfmt *default_df;*/

    /* Initializes object output.  Must be called before any other object
     * format functions.  Should NOT open the object file; the filenames are
     * provided solely for informational purposes.
     * May be NULL if not needed by the object format.
     */
    void (*initialize) (const char *in_filename, const char *obj_filename);

    /* Write out (post-optimized) sections to the object file.
     * This function may call symrec functions as necessary (including
     * symrec_traverse) to retrieve symbolic information.
     */
    void (*output) (FILE *f, sectionhead *sections);

    /* Cleans up anything allocated by initialize.
     * May be NULL if not needed by the object format.
     */
    void (*cleanup) (void);

    /* Switch object file sections.  The first val of the valparams should
     * be the section name.  Returns NULL if something's wrong, otherwise
     * returns the new section.
     */
    /*@observer@*/ /*@null@*/ section *
	(*sections_switch)(sectionhead *headp, valparamhead *valparams,
			   /*@null@*/ valparamhead *objext_valparams);

    /* Object format-specific data handling functions for sections.
     * May be NULL if no data is ever allocated in sections_switch().
     */
    void (*section_data_delete)(/*@only@*/ void *data);
    void (*section_data_print)(FILE *f, void *data);

    /* These functions should call symrec_set_of_data() to store data.
     * May be NULL if objfmt doesn't care about such declarations.
     */
    void (*extern_declare)(symrec *sym,
			   /*@null@*/ valparamhead *objext_valparams);
    void (*global_declare)(symrec *sym,
			   /*@null@*/ valparamhead *objext_valparams);
    void (*common_declare)(symrec *sym, /*@only@*/ expr *size,
			   /*@null@*/ valparamhead *objext_valparams);

    /* May be NULL if symrec_set_of_data() is never called. */
    void (*symrec_data_delete)(/*@only@*/ void *data);
    void (*symrec_data_print)(FILE *f, /*@null@*/ void *data);

    /* Object format-specific directive support.  Returns 1 if directive was
     * not recognized.  Returns 0 if directive was recognized, even if it
     * wasn't valid.
     */
    int (*directive)(const char *name, valparamhead *valparams,
		     /*@null@*/ valparamhead *objext_valparams,
		     sectionhead *headp);

    /* Bytecode objfmt data (BC_OBJFMT_DATA) handling functions.
     * May be NULL if no BC_OBJFMT_DATA is ever allocated by the object format.
     */
    void (*bc_objfmt_data_delete)(unsigned int type, /*@only@*/ void *data);
    void (*bc_objfmt_data_print)(FILE *f, unsigned int type, const void *data);
};

/* Generic functions for all object formats - implemented in src/objfmt.c */

/* Finds an object format based on its keyword.  Returns NULL if no match was
 * found.
 */
/*@null@*/ objfmt *find_objfmt(const char *keyword);

/* Lists all available object formats.  Calls printfunc with the name and
 * keyword of each available format.
 */
void list_objfmts(void (*printfunc) (const char *name, const char *keyword));

#endif
