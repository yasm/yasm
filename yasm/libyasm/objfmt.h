/**
 * \file libyasm/objfmt.h
 * \brief YASM object format module interface.
 *
 * \rcs
 * $IdPath$
 * \endrcs
 *
 * \license
 *  Copyright (C) 2001  Peter Johnson
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
#ifndef YASM_OBJFMT_H
#define YASM_OBJFMT_H

/** Version number of #yasm_objfmt interface.  Any functional change to the
 * #yasm_objfmt interface should simultaneously increment this number.  This
 * version should be checked by #yasm_objfmt loaders to verify that the
 * expected version (the version defined by its libyasm header files) matches
 * the loaded module version (the version defined by the module's libyasm
 * header files).  Doing this will ensure that the module version's function
 * definitions match the module loader's function definitions.  The version
 * number must never be decreased.
 */
#define YASM_OBJFMT_VERSION	0

/** YASM object format interface. */
struct yasm_objfmt {
    /** Version (see #YASM_OBJFMT_VERSION).  Should always be set to
     * #YASM_OBJFMT_VERSION by the module source and checked against
     * #YASM_OBJFMT_VERSION by the module loader.
     */
    unsigned int version;

    /** One-line description of the object format. */
    const char *name;

    /** Keyword used to select object format. */
    const char *keyword;

    /** Default output file extension (without the '.').
     * NULL means no extension, with no '.', while "" includes the '.'.
     */
    /*@null@*/ const char *extension;

    /** Default (starting) section name. */
    const char *default_section_name;

    /** Default (starting) x86 BITS setting.  This only appies to the x86
     * architecture; other architectures ignore this setting.
     */
    const unsigned char default_x86_mode_bits;

    /** NULL-terminated list of debug format (yasm_dbgfmt) keywords that are
     * valid to use with this object format.  The null debug format
     * (null_dbgfmt, "null") should always be in this list so it's possible to
     * have no debug output.
     */
    const char **dbgfmt_keywords;

    /** Default debug format keyword (set even if there's only one available to
     * use).
     */
    const char *default_dbgfmt_keyword;

    /** Initialize object output for use.  Must be called before any other
     * object format functions.  Should /not/ open the object file; the
     * filenames are provided solely for informational purposes.
     * \param in_filename	main input filename (e.g. "file.asm")
     * \param obj_filename	object filename (e.g. "file.o")
     * \param df		debug format in use
     * \param a			architecture in use
     * \param machine		machine (of architecture) in use
     * \return Nonzero if architecture/machine combination not supported.
     */
    int (*initialize) (const char *in_filename, const char *obj_filename,
		       yasm_dbgfmt *df, yasm_arch *a, const char *machine);

    /** Write out (post-optimized) sections to the object file.
     * This function may call yasm_symrec_* functions as necessary (including
     * yasm_symrec_traverse()) to retrieve symbolic information.
     * \param f		output object file
     * \param sections	list of sections
     * \param all_syms	if nonzero, all symbols should be included in the
     *                  object file
     * \note The list of sections may include sections that will not actually
     *       be output into the object file.
     */
    void (*output) (FILE *f, yasm_sectionhead *sections, int all_syms);

    /** Cleans up anything allocated by initialize().  Function may be
     * unimplemented (NULL) if not needed by the object format.
     */
    void (*cleanup) (void);

    /** Switch object file sections.  The first val of the valparams should
     * be the section name.  Calls yasm_section_switch() to actually change
     * the active section.
     * \param headp		list of sections
     * \param valparams		value/parameters
     * \param objext_valparams	object format-specific value/parameters
     * \param lindex		line index (as from yasm_linemgr)
     * \return NULL on error, otherwise new section.
     */
    /*@observer@*/ /*@null@*/ yasm_section *
	(*sections_switch)(yasm_sectionhead *headp,
			   yasm_valparamhead *valparams,
			   /*@null@*/ yasm_valparamhead *objext_valparams,
			   unsigned long lindex);

    /** Delete yasm_section object format-specific data.  Function may be
     * unimplemented (NULL) if no data is ever allocated in sections_switch().
     * \param data	object format-specific data
     */
    void (*section_data_delete)(/*@only@*/ void *data);

    /** Print yasm_section object format-specific data.  For debugging
     * purposes.  Function may be unimplemented (NULL) if no data is ever
     * allocated in sections_switch() by the object format.
     * \param f			file
     * \param indent_level	indentation level
     * \param data		object format-specific data
     */
    void (*section_data_print)(FILE *f, int indent_level, void *data);

    /** Declare an "extern" (importing from another module) symbol.  Should
     * call yasm_symrec_set_of_data() to store data.  Function may be
     * unimplemented (NULL) if object format doesn't care about such a
     * declaration.
     * \param sym		symbol
     * \param objext_valparams	object format-specific value/paramaters
     * \param lindex		line index (from yasm_linemgr)
     */
    void (*extern_declare)(yasm_symrec *sym,
			   /*@null@*/ yasm_valparamhead *objext_valparams,
			   unsigned long lindex);

    /** Declare a "global" (exporting to other modules) symbol.  Should call
     * yasm_symrec_set_of_data() to store data.  Function may be unimplemented
     * (NULL) if object format doesn't care about such a declaration.
     * \param sym		symbol
     * \param objext_valparams	object format-specific value/paramaters
     * \param lindex		line index (from yasm_linemgr)
     */
    void (*global_declare)(yasm_symrec *sym,
			   /*@null@*/ yasm_valparamhead *objext_valparams,
			   unsigned long lindex);

    /** Declare a "common" (shared space with other modules) symbol.  Should
     * call yasm_symrec_set_of_data() to store data.  Function may be
     * unimplemented (NULL) if object format doesn't care about such a
     * declaration.
     * \param sym		symbol
     * \param size		common data size
     * \param objext_valparams	object format-specific value/paramaters
     * \param lindex		line index (from yasm_linemgr)
     */
    void (*common_declare)(yasm_symrec *sym, /*@only@*/ yasm_expr *size,
			   /*@null@*/ yasm_valparamhead *objext_valparams,
			   unsigned long lindex);

    /** Delete object format-specific symbol data.  Function may be
     * unimplemented (NULL) if yasm_symrec_set_of_data() is never called by the
     * object format.
     * \param data	object format-specific data
     */
    void (*symrec_data_delete)(/*@only@*/ void *data);

    /** Print object format-specific symbol data.  For debugging purposes.
     * Function may be unimplemented (NULL) if yasm_symrec_set_of_data() is
     * never called by the object format.
     * \param f			file
     * \param indent_level	indentation level
     * \param data		object format-specific data
     */
    void (*symrec_data_print)(FILE *f, int indent_level, void *data);

    /** Handle object format-specific directives.
     * \param name		directive name
     * \param valparams		value/parameters
     * \param objext_valparams	object format-specific value/parameters
     * \param headp		list of sections
     * \param lindex		line index (as from yasm_linemgr)
     * \return Nonzero if directive was not recognized; 0 if directive was
     *         recognized, even if it wasn't valid.
     */
    int (*directive)(const char *name, yasm_valparamhead *valparams,
		     /*@null@*/ yasm_valparamhead *objext_valparams,
		     yasm_sectionhead *headp, unsigned long lindex);

    /** Delete object format-specific bytecode data (YASM_BC_OBJFMT_DATA).
     * Function may be unimplemented (NULL) if no YASM_BC_OBJFMT_DATA is ever
     * allocated by the object format.
     * \param type	object format-specific bytecode type
     * \param data	object format-specific data
     */
    void (*bc_objfmt_data_delete)(unsigned int type, /*@only@*/ void *data);

    /** Print object format-specific bytecode data (YASM_BC_OBJFMT_DATA).  For
     * debugging purposes.  Function may be unimplemented (NULL) if no
     * YASM_BC_OBJFMT_DATA is ever allocated by the object format.
     * \param f			file
     * \param indent_level	indentation level
     * \param type		object format-specific bytecode type
     * \param data		object format-specific data
     */
    void (*bc_objfmt_data_print)(FILE *f, int indent_level, unsigned int type,
				 const void *data);
};
#endif
