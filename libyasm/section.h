/**
 * \file libyasm/section.h
 * \brief YASM section interface.
 *
 * \rcs
 * $Id$
 * \endrcs
 *
 * \license
 *  Copyright (C) 2001-2007  Peter Johnson
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
#ifndef YASM_SECTION_H
#define YASM_SECTION_H

/** Basic YASM relocation.  Object formats will need to extend this
 * structure with additional fields for relocation type, etc.
 */
typedef struct yasm_reloc yasm_reloc;

#ifdef YASM_LIB_INTERNAL
struct yasm_reloc {
    /*@reldef@*/ STAILQ_ENTRY(yasm_reloc) link;
    yasm_intnum *addr;          /**< Offset (address) within section */
    /*@dependent@*/ yasm_symrec *sym;       /**< Relocated symbol */
};
#endif

/** An object.  This is the internal representation of an object file. */
struct yasm_object {
    /*@owned@*/ char *src_filename;     /**< Source filename */
    /*@owned@*/ char *obj_filename;     /**< Object filename */

    /*@owned@*/ yasm_symtab *symtab;    /**< Symbol table */
    /*@owned@*/ yasm_arch *arch;        /**< Target architecture */
    /*@owned@*/ yasm_objfmt *objfmt;    /**< Object format */
    /*@owned@*/ yasm_dbgfmt *dbgfmt;    /**< Debug format */

    /** Currently active section.  Used by some directives. */
    /*@dependent@*/ yasm_section *cur_section;

#ifdef YASM_LIB_INTERNAL
    /** Linked list of sections. */
    /*@reldef@*/ STAILQ_HEAD(yasm_sectionhead, yasm_section) sections;

    /** Directives, organized as two level HAMT; first level is parser,
     * second level is directive name.
     */
    /*@owned@*/ struct HAMT *directives;
#endif
};

/** Create a new object.  A default section is created as the first section.
 * An empty symbol table (yasm_symtab) and line mapping (yasm_linemap) are
 * automatically created.
 * \param src_filename  source filename (e.g. "file.asm")
 * \param obj_filename  object filename (e.g. "file.o")
 * \param arch          architecture
 * \param objfmt_module object format module
 * \param dbgfmt_module debug format module
 * \return Newly allocated object, or NULL on error.
 */
/*@null@*/ /*@only@*/ yasm_object *yasm_object_create
    (const char *src_filename, const char *obj_filename,
     /*@kept@*/ yasm_arch *arch,
     const yasm_objfmt_module *objfmt_module,
     const yasm_dbgfmt_module *dbgfmt_module);

/** Create a new, or continue an existing, general section.  The section is
 * added to the object if there's not already a section by that name.
 * \param object    object
 * \param name      section name
 * \param start     starting address (ignored if section already exists),
 *                  NULL if 0 or don't care.
 * \param align     alignment in bytes (0 if none)
 * \param code      if nonzero, section is intended to contain code
 *                  (e.g. alignment should be made with NOP instructions, not 0)
 * \param res_only  if nonzero, only space-reserving bytecodes are allowed in
 *                  the section (ignored if section already exists)
 * \param isnew     output; set to nonzero if section did not already exist
 * \param line      virtual line of section declaration (ignored if section
 *                  already exists)
 * \return New section.
 */
/*@dependent@*/ yasm_section *yasm_object_get_general
    (yasm_object *object, const char *name,
     /*@null@*/ /*@only@*/ yasm_expr *start, unsigned long align, int code,
     int res_only, /*@out@*/ int *isnew, unsigned long line);

/** Create a new absolute section.  No checking is performed at creation to
 * check for overlaps with other absolute sections.
 * \param object    object
 * \param start     starting address (expression)
 * \param line      virtual line of section declaration
 * \return New section.
 */
/*@dependent@*/ yasm_section *yasm_object_create_absolute
    (yasm_object *object, /*@keep@*/ yasm_expr *start, unsigned long line);

/** Handle a directive.  Passed down to object format, debug format, or
 * architecture as appropriate.
 * \param object                object
 * \param name                  directive name
 * \param parser                parser keyword
 * \param valparams             value/parameters
 * \param objext_valparams      "object format-specific" value/parameters
 * \param line                  virtual line (from yasm_linemap)
 * \return 0 if directive recognized, nonzero if unrecognized.
 */
int yasm_object_directive(yasm_object *object, const char *name,
                          const char *parser, yasm_valparamhead *valparams,
                          yasm_valparamhead *objext_valparams,
                          unsigned long line);

/** Delete (free allocated memory for) an object.  All sections in the
 * object and all bytecodes within those sections are also deleted.
 * \param object        object
 */
void yasm_object_destroy(/*@only@*/ yasm_object *object);

/** Print an object.  For debugging purposes.
 * \param object        object
 * \param f             file
 * \param indent_level  indentation level
 */
void yasm_object_print(const yasm_object *object, FILE *f, int indent_level);

/** Finalize an object after parsing.
 * \param object        object
 * \param errwarns      error/warning set
 * \note Errors/warnings are stored into errwarns.
 */
void yasm_object_finalize(yasm_object *object, yasm_errwarns *errwarns);

/** Traverses all sections in an object, calling a function on each section.
 * \param object        object
 * \param d             data pointer passed to func on each call
 * \param func          function
 * \return Stops early (and returns func's return value) if func returns a
 *         nonzero value; otherwise 0.
 */
int yasm_object_sections_traverse
    (yasm_object *object, /*@null@*/ void *d,
     int (*func) (yasm_section *sect, /*@null@*/ void *d));

/** Find a general section in an object, based on its name.
 * \param object        object
 * \param name          section name
 * \return Section matching name, or NULL if no match found.
 */
/*@dependent@*/ /*@null@*/ yasm_section *yasm_object_find_general
    (yasm_object *object, const char *name);

/** Change the source filename for an object.
 * \param object        object
 * \param src_filename  new source filename (e.g. "file.asm")
 */
void yasm_object_set_source_fn(yasm_object *object, const char *src_filename);

/** Optimize an object.  Takes the unoptimized object and optimizes it.
 * If successful, the object is ready for output to an object file.
 * \param object        object
 * \param errwarns      error/warning set
 * \note Optimization failures are stored into errwarns.
 */
void yasm_object_optimize(yasm_object *object, yasm_errwarns *errwarns);

/** Determine if a section is absolute or general.
 * \param sect      section
 * \return Nonzero if section is absolute.
 */
int yasm_section_is_absolute(yasm_section *sect);

/** Determine if a section is flagged to contain code.
 * \param sect      section
 * \return Nonzero if section is flagged to contain code.
 */
int yasm_section_is_code(yasm_section *sect);

/** Get yasm_optimizer-specific flags.  For yasm_optimizer use only.
 * \param sect      section
 * \return Optimizer-specific flags.
 */
unsigned long yasm_section_get_opt_flags(const yasm_section *sect);

/** Set yasm_optimizer-specific flags.  For yasm_optimizer use only.
 * \param sect      section
 * \param opt_flags optimizer-specific flags.
 */
void yasm_section_set_opt_flags(yasm_section *sect, unsigned long opt_flags);

/** Determine if a section was declared as the "default" section (e.g. not
 * created through a section directive).
 * \param sect      section
 * \return Nonzero if section was declared as default.
 */
int yasm_section_is_default(const yasm_section *sect);

/** Set section "default" flag to a new value.
 * \param sect      section
 * \param def       new value of default flag
 */
void yasm_section_set_default(yasm_section *sect, int def);

/** Get object owner of a section.
 * \param sect      section
 * \return Object this section is a part of.
 */
yasm_object *yasm_section_get_object(const yasm_section *sect);

/** Get assocated data for a section and data callback.
 * \param sect      section
 * \param callback  callback used when adding data
 * \return Associated data (NULL if none).
 */
/*@dependent@*/ /*@null@*/ void *yasm_section_get_data
    (yasm_section *sect, const yasm_assoc_data_callback *callback);

/** Add associated data to a section.
 * \attention Deletes any existing associated data for that data callback.
 * \param sect      section
 * \param callback  callback
 * \param data      data to associate
 */
void yasm_section_add_data(yasm_section *sect,
                           const yasm_assoc_data_callback *callback,
                           /*@null@*/ /*@only@*/ void *data);

/** Add a relocation to a section.
 * \param sect          section
 * \param reloc         relocation
 * \param destroy_func  function that can destroy the relocation
 * \note Does not make a copy of reloc.  The same destroy_func must be
 * used for all relocations in a section or an internal error will occur.
 * The section will destroy the relocation address; it is the caller's
 * responsibility to destroy any other allocated data.
 */
void yasm_section_add_reloc(yasm_section *sect, yasm_reloc *reloc,
    void (*destroy_func) (/*@only@*/ void *reloc));

/** Get the first relocation for a section.
 * \param sect          section
 * \return First relocation for section.  NULL if no relocations.
 */
/*@null@*/ yasm_reloc *yasm_section_relocs_first(yasm_section *sect);

/** Get the next relocation for a section.
 * \param reloc         previous relocation
 * \return Next relocation for section.  NULL if no more relocations.
 */
/*@null@*/ yasm_reloc *yasm_section_reloc_next(yasm_reloc *reloc);

#ifdef YASM_LIB_INTERNAL
#define yasm_section_reloc_next(x)      STAILQ_NEXT((x), link)
#endif

/** Get the basic relocation information for a relocation.
 * \param reloc         relocation
 * \param addrp         address of relocation within section (returned)
 * \param symp          relocated symbol (returned)
 */
void yasm_reloc_get(yasm_reloc *reloc, yasm_intnum **addrp,
                    /*@dependent@*/ yasm_symrec **symp);

/** Get the first bytecode in a section.
 * \param sect          section
 * \return First bytecode in section (at least one empty bytecode is always
 *         present).
 */
yasm_bytecode *yasm_section_bcs_first(yasm_section *sect);

/** Get the last bytecode in a section.
 * \param sect          section
 * \return Last bytecode in section (at least one empty bytecode is always
 *         present).
 */
yasm_bytecode *yasm_section_bcs_last(yasm_section *sect);

/** Add bytecode to the end of a section.
 * \note Does not make a copy of bc; so don't pass this function static or
 *       local variables, and discard the bc pointer after calling this
 *       function.
 * \param sect          section
 * \param bc            bytecode (may be NULL)
 * \return If bytecode was actually appended (it wasn't NULL or empty), the
 *         bytecode; otherwise NULL.
 */
/*@only@*/ /*@null@*/ yasm_bytecode *yasm_section_bcs_append
    (yasm_section *sect,
     /*@returned@*/ /*@only@*/ /*@null@*/ yasm_bytecode *bc);

/** Traverses all bytecodes in a section, calling a function on each bytecode.
 * \param sect      section
 * \param errwarns  error/warning set (may be NULL)
 * \param d         data pointer passed to func on each call (may be NULL)
 * \param func      function
 * \return Stops early (and returns func's return value) if func returns a
 *         nonzero value; otherwise 0.
 * \note If errwarns is non-NULL, yasm_errwarn_propagate() is called after
 *       each call to func (with the bytecode's line number).
 */
int yasm_section_bcs_traverse
    (yasm_section *sect, /*@null@*/ yasm_errwarns *errwarns,
     /*@null@*/ void *d, int (*func) (yasm_bytecode *bc, /*@null@*/ void *d));

/** Get name of a section.
 * \param   sect    section
 * \return Section name, or NULL if section is absolute.
 */
/*@observer@*/ /*@null@*/ const char *yasm_section_get_name
    (const yasm_section *sect);

/** Get starting symbol of an absolute section.
 * \param   sect    section
 * \return Starting symrec, or NULL if section is not absolute.
 */
/*@dependent@*/ /*@null@*/ yasm_symrec *yasm_section_abs_get_sym
    (const yasm_section *sect);

/** Change starting address of a section.
 * \param sect      section
 * \param start     starting address
 * \param line      virtual line
 */
void yasm_section_set_start(yasm_section *sect, /*@only@*/ yasm_expr *start,
                            unsigned long line);

/** Get starting address of a section.
 * \param sect      section
 * \return Starting address (may be a complex expression if section is
 *         absolute).
 */
/*@observer@*/ const yasm_expr *yasm_section_get_start
    (const yasm_section *sect);

/** Change alignment of a section.
 * \param sect      section
 * \param align     alignment in bytes
 * \param line      virtual line
 */
void yasm_section_set_align(yasm_section *sect, unsigned long align,
                            unsigned long line);

/** Get alignment of a section.
 * \param sect      section
 * \return Alignment in bytes (0 if none).
 */
unsigned long yasm_section_get_align(const yasm_section *sect);

/** Print a section.  For debugging purposes.
 * \param f             file
 * \param indent_level  indentation level
 * \param sect          section
 * \param print_bcs     if nonzero, print bytecodes within section
 */
void yasm_section_print(/*@null@*/ const yasm_section *sect, FILE *f,
                        int indent_level, int print_bcs);

#endif
