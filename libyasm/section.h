/**
 * \file section.h
 * \brief YASM section interface.
 *
 * $IdPath$
 *
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
 */
#ifndef YASM_SECTION_H
#define YASM_SECTION_H

/** Create a new section list.  A default section is created as the
 * first section.
 * \param def	    returned; default section
 * \param of	    object format in use
 * \return Newly allocated section list.
 */
/*@only@*/ yasm_sectionhead *yasm_sections_new
    (/*@out@*/ /*@dependent@*/ yasm_section **def, yasm_objfmt *of);

/** Create a new, or continue an existing, general section.  The section is
 * added to a section list if there's not already a section by that name.
 * \param headp	    section list
 * \param name	    section name
 * \param start	    starting address (ignored if section already exists)
 * \param res_only  if nonzero, only space-reserving bytecodes are allowed in
 *		    the section (ignored if section already exists)
 * \param isnew	    output; set to nonzero if section did not already exist
 * \param lindex    line index of section declaration (ignored if section
 *		    already exists)
 * \return New section.
 */
/*@dependent@*/ yasm_section *yasm_sections_switch_general
    (yasm_sectionhead *headp, const char *name, unsigned long start,
     int res_only, /*@out@*/ int *isnew, unsigned long lindex);

/** Create a new absolute section.  No checking is performed at creation to
 * check for overlaps with other absolute sections.
 * \param headp	    section list
 * \param start	    starting address (expression)
 * \return New section.
 */
/*@dependent@*/ yasm_section *yasm_sections_switch_absolute
    (yasm_sectionhead *headp, /*@keep@*/ yasm_expr *start);

/** Determine if a section is absolute or general.
 * \param sect	    section
 * \return Nonzero if section is absolute.
 */
int yasm_section_is_absolute(yasm_section *sect);

/** Get yasm_optimizer-specific flags.  For yasm_optimizer use only.
 * \param sect	    section
 * \return Optimizer-specific flags.
 */
unsigned long yasm_section_get_opt_flags(const yasm_section *sect);

/** Set yasm_optimizer-specific flags.  For yasm_optimizer use only.
 * \param sect	    section
 * \param opt_flags optimizer-specific flags.
 */
void yasm_section_set_opt_flags(yasm_section *sect, unsigned long opt_flags);

/** Get yasm_objfmt-specific data.  For yasm_objfmt use only.
 * \param sect	    section
 * \return Object format-specific data.
 */
/*@dependent@*/ /*@null@*/ void *yasm_section_get_of_data(yasm_section *sect);

/** Set yasm_objfmt-specific data.  For yasm_objfmt use only.
 * \attention Deletes any existing of_data.
 * \param sect	    section
 * \param of	    object format
 * \param of_data   object format-specific data.
 */
void yasm_section_set_of_data(yasm_section *sect, yasm_objfmt *of,
			      /*@null@*/ /*@only@*/ void *of_data);

/** Delete (free allocated memory for) a section list.  All sections in the
 * section list and all bytecodes within those sections are also deleted.
 * \param headp	    section list
 */
void yasm_sections_delete(/*@only@*/ yasm_sectionhead *headp);

/** Print a section list.  For debugging purposes.
 * \param f		file
 * \param indent_level	indentation level
 * \param headp		section list
 */
void yasm_sections_print(FILE *f, int indent_level,
			 const yasm_sectionhead *headp);

/** Traverses a section list, calling a function on each bytecode.
 * \param headp	bytecode list
 * \param d	data pointer passed to func on each call
 * \param func	function
 * \return Stops early (and returns func's return value) if func returns a
 *	   nonzero value; otherwise 0.
 */
int yasm_sections_traverse(yasm_sectionhead *headp, /*@null@*/ void *d,
			   int (*func) (yasm_section *sect,
					/*@null@*/ void *d));

/** Find a section based on its name.
 * \param   headp   section list
 * \param   name    section name
 * \return Section matching name, or NULL if no match found.
 */
/*@dependent@*/ /*@null@*/ yasm_section *yasm_sections_find_general
    (yasm_sectionhead *headp, const char *name);

/** Get bytecode list of a section.
 * \param   sect    section
 * \return Bytecode list.
 */
/*@dependent@*/ yasm_bytecodehead *yasm_section_get_bytecodes
    (yasm_section *sect);

/** Get name of a section.
 * \param   sect    section
 * \return Section name, or NULL if section is absolute.
 */
/*@observer@*/ /*@null@*/ const char *yasm_section_get_name
    (const yasm_section *sect);

/** Change starting address of a section.
 * \param sect	    section
 * \param start	    starting address
 * \param lindex    line index
 */
void yasm_section_set_start(yasm_section *sect, unsigned long start,
			    unsigned long lindex);

/** Get starting address of a section.
 * \param sect	    section
 * \return Starting address (may be a complex expression if section is
 *	   absolute).
 */
/*@observer@*/ const yasm_expr *yasm_section_get_start
    (const yasm_section *sect);

/** Print a section.  For debugging purposes.
 * \param f		file
 * \param indent_level	indentation level
 * \param sect		section
 * \param print_bcs	if nonzero, print bytecodes within section
 */
void yasm_section_print(FILE *f, int indent_level,
			/*@null@*/ const yasm_section *sect, int print_bcs);
#endif
