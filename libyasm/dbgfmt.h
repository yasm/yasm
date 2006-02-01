/**
 * \file libyasm/dbgfmt.h
 * \brief YASM debug format interface.
 *
 * \rcs
 * $Id$
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

#ifndef YASM_DOXYGEN
/** Base #yasm_dbgfmt structure.  Must be present as the first element in any
 * #yasm_dbgfmt implementation.
 */
typedef struct yasm_dbgfmt_base {
    /** #yasm_dbgfmt_module implementation for this debug format. */
    const struct yasm_dbgfmt_module *module;
} yasm_dbgfmt_base;
#endif

/** YASM debug format module interface. */
typedef struct yasm_dbgfmt_module {
    /** One-line description of the debug format. */
    const char *name;

    /** Keyword used to select debug format. */
    const char *keyword;

    /** Create debug format.
     * Module-level implementation of yasm_dbgfmt_create().
     * The filenames are provided solely for informational purposes.
     * \param object	    object
     * \param of	    object format in use
     * \param a		    architecture in use
     * \return NULL if object format does not provide needed support.
     */
    /*@null@*/ /*@only@*/ yasm_dbgfmt * (*create)
	(yasm_object *object, yasm_objfmt *of, yasm_arch *a);

    /** Module-level implementation of yasm_dbgfmt_destroy().
     * Call yasm_dbgfmt_destroy() instead of calling this function.
     */
    void (*destroy) (/*@only@*/ yasm_dbgfmt *dbgfmt);

    /** Module-level implementation of yasm_dbgfmt_directive().
     * Call yasm_dbgfmt_directive() instead of calling this function.
     */
    int (*directive) (yasm_dbgfmt *dbgfmt, const char *name,
		      yasm_section *sect, yasm_valparamhead *valparams,
		      unsigned long line);

    /** Module-level implementation of yasm_dbgfmt_generate().
     * Call yasm_dbgfmt_generate() instead of calling this function.
     */
    void (*generate) (yasm_dbgfmt *dbgfmt);
} yasm_dbgfmt_module;

/** Get the keyword used to select a debug format.
 * \param dbgfmt    debug format
 * \return keyword
 */
const char *yasm_dbgfmt_keyword(const yasm_dbgfmt *dbgfmt);

/** Initialize debug output for use.  Must call before any other debug
 * format functions.  The filenames are provided solely for informational
 * purposes.
 * \param module	debug format module
 * \param object	object to generate debugging information for
 * \param of		object format in use
 * \param a		architecture in use
 * \return NULL if object format does not provide needed support.
 */
/*@null@*/ /*@only@*/ yasm_dbgfmt *yasm_dbgfmt_create
    (const yasm_dbgfmt_module *module, yasm_object *object, yasm_objfmt *of,
     yasm_arch *a);

/** Cleans up any allocated debug format memory.
 * \param dbgfmt	debug format
 */
void yasm_dbgfmt_destroy(/*@only@*/ yasm_dbgfmt *dbgfmt);

/** DEBUG directive support.
 * \param dbgfmt	debug format
 * \param name		directive name
 * \param sect		current active section
 * \param valparams	value/parameters
 * \param line		virtual line (from yasm_linemap)
 * \return Nonzero if directive was not recognized; 0 if directive was
 *	       recognized even if it wasn't valid.
 */
int yasm_dbgfmt_directive(yasm_dbgfmt *dbgfmt, const char *name,
			  yasm_section *sect, yasm_valparamhead *valparams,
			  unsigned long line);

/** Generate debugging information bytecodes.
 * \param dbgfmt	debug format
 */
void yasm_dbgfmt_generate(yasm_dbgfmt *dbgfmt);

#ifndef YASM_DOXYGEN

/* Inline macro implementations for dbgfmt functions */

#define yasm_dbgfmt_keyword(dbgfmt) \
    (((yasm_dbgfmt_base *)dbgfmt)->module->keyword)

#define yasm_dbgfmt_create(module, object, of, a) \
    module->create(object, of, a)

#define yasm_dbgfmt_destroy(dbgfmt) \
    ((yasm_dbgfmt_base *)dbgfmt)->module->destroy(dbgfmt)
#define yasm_dbgfmt_directive(dbgfmt, name, sect, valparams, line) \
    ((yasm_dbgfmt_base *)dbgfmt)->module->directive(dbgfmt, name, sect, \
						    valparams, line)
#define yasm_dbgfmt_generate(dbgfmt) \
    ((yasm_dbgfmt_base *)dbgfmt)->module->generate(dbgfmt)

#endif

#endif
