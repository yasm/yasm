/**
 * \file libyasm/dbgfmt.h
 * \brief YASM debug format interface.
 *
 * \rcs
 * $IdPath$
 * \endrcs
 *
 * \license
 *  Copyright (C) 2002  Peter Johnson
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
#ifndef YASM_DBGFMT_H
#define YASM_DBGFMT_H

/** Version number of #yasm_dbgfmt interface.  Any functional change to the
 * #yasm_dbgfmt interface should simultaneously increment this number.  This
 * version should be checked by #yasm_dbgfmt loaders to verify that the
 * expected version (the version defined by its libyasm header files) matches
 * the loaded module version (the version defined by the module's libyasm
 * header files).  Doing this will ensure that the module version's function
 * definitions match the module loader's function definitions.  The version
 * number must never be decreased.
 */
#define YASM_DBGFMT_VERSION	1

/** YASM debug format interface. */
struct yasm_dbgfmt {
    /** Version (see #YASM_DBGFMT_VERSION).  Should always be set to
     * #YASM_DBGFMT_VERSION by the module source and checked against
     * #YASM_DBGFMT_VERSION by the module loader.
     */
    unsigned int version;

    /** One-line description of the debug format. */
    const char *name;

    /** Keyword used to select debug format. */
    const char *keyword;

    /** Initialize debug output for use.  Must call before any other debug
     * format functions.  The filenames are provided solely for informational
     * purposes.  Function may be unimplemented (NULL) if not needed by the
     * debug format.
     * \param in_filename   primary input filename
     * \param obj_filename  object filename
     * \param of	    object format in use
     * \return Nonzero if object format does not provide needed support.
     */
    int (*initialize) (const char *in_filename, const char *obj_filename,
		       yasm_linemgr *lm, yasm_objfmt *of, yasm_arch *a,
		       const char *machine);

    /** Clean up anything allocated by initialize().  Function may be
     * unimplemented (NULL) if not needed by the debug format.
     */
    void (*cleanup) (void);

    /** DEBUG directive support.
     * \param name	    directive name
     * \param valparams	    value/parameters
     * \param lindex	    line index (as from yasm_linemgr)
     * \return Nonzero if directive was not recognized; 0 if directive was
     *	       recognized even if it wasn't valid.
     */
    int (*directive) (const char *name, yasm_valparamhead *valparams,
		      unsigned long lindex);

    /** Generate debugging information bytecodes
     * \param sections	    list of sections
     */
    void (*generate) (yasm_sectionhead *sections);

    /** Output debug format-specific bytecode data (YASM_BC_DBGFMT_DATA).
     * Function may be unimplemented (NULL) if no YASM_BC_DBGFMT_DATA is ever
     * allocated by the debug format.
     * \param type		debug format-specific bytecode type
     * \param data		debug format-specific data
     * \param buf		provided buffer, as long as registered length
     */
    int (*bc_dbgfmt_data_output)(yasm_bytecode *bc, unsigned int type,
				 const void *data, unsigned char **buf,
				 const yasm_section *sect,
				 yasm_output_reloc_func output_reloc,
				 void *objfmt_d);

    /** Delete debug format-specific bytecode data (YASM_BC_DBGFMT_DATA).
     * Funtion may be unimplemented (NULL) if no YASM_BC_DBGFMT_DATA is ever
     * allocated by the debug format.
     * \param type	debug format-specific bytecode type
     * \param data	debug-format specific data
     */
    void (*bc_dbgfmt_data_delete)(unsigned int type, /*@only@*/ void *data);

    /** Print debug format-specific bytecode data (YASM_BC_DBGFMT_DATA).  For
     * debugging purposes.  Function may be unimplemented (NULL) if no
     * YASM_BC_DBGFMT_DATA is ever allocated by the debug format.
     * \param f			file
     * \param indent_level	indentation level
     * \param type		debug format-specific bytecode type
     * \param data		debug format-specific data
     */
    void (*bc_dbgfmt_data_print)(FILE *f, int indent_level, unsigned int type,
				 const void *data);
};

#endif
