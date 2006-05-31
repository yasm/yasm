/**
 * \file libyasm/parser.h
 * \brief YASM parser module interface.
 *
 * \rcs
 * $Id$
 * \endrcs
 *
 * \license
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
 * \endlicense
 */
#ifndef YASM_PARSER_H
#define YASM_PARSER_H

/** YASM parser module interface.  The "front end" of the assembler. */
typedef struct yasm_parser_module {
    /** One-line description of the parser */
    const char *name;

    /** Keyword used to select parser on the command line */
    const char *keyword;

    /** NULL-terminated list of preprocessors that are valid to use with this
     * parser.  The raw preprocessor (raw_preproc) should always be in this
     * list so it's always possible to have no preprocessing done.
     */
    const char **preproc_keywords;

    /** Default preprocessor. */
    const char *default_preproc_keyword;

    /** Parse a source file into an object.
     * \param object	object to parse into (already created)
     * \param pp	preprocessor
     * \param a		architecture; architecture-specific directives are
     *			obtained from this.
     * \param of	object format; objfmt-specific directives and segment
     *			names are obtained from this.
     * \param f		initial starting file
     * \param in_filename	initial starting file's filename
     * \param save_input	nonzero if the parser should save the original
     *				lines of source into the object's linemap (via
     *				yasm_linemap_add_data()).
     * \param def_sect	default (starting) section in the object
     * \param errwarns	error/warning set
     * \note Parse errors and warnings are stored into errwarns.
     */
    void (*do_parse)
	(yasm_object *object, yasm_preproc *pp, yasm_arch *a, yasm_objfmt *of,
	 yasm_dbgfmt *df, FILE *f, const char *in_filename, int save_input,
	 yasm_section *def_sect, yasm_errwarns *errwarns);
} yasm_parser_module;

#endif
