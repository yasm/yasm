/* $IdPath$
 * Section header file
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
#ifndef YASM_SECTION_H
#define YASM_SECTION_H

#ifdef YASM_INTERNAL
/*@reldef@*/ STAILQ_HEAD(yasm_sectionhead, yasm_section);
#endif

/*@dependent@*/ yasm_section *yasm_sections_initialize(yasm_sectionhead *headp,
						       yasm_objfmt *of);

/*@dependent@*/ yasm_section *yasm_sections_switch_general
    (yasm_sectionhead *headp, const char *name, unsigned long start,
     int res_only, /*@out@*/ int *isnew, unsigned long lindex);

/*@dependent@*/ yasm_section *yasm_sections_switch_absolute
    (yasm_sectionhead *headp, /*@keep@*/ yasm_expr *start);

int yasm_section_is_absolute(yasm_section *sect);

/* Get and set optimizer flags */
unsigned long yasm_section_get_opt_flags(const yasm_section *sect);
void yasm_section_set_opt_flags(yasm_section *sect, unsigned long opt_flags);

void yasm_section_set_of_data(yasm_section *sect, yasm_objfmt *of,
			      /*@null@*/ /*@only@*/ void *of_data);
/*@dependent@*/ /*@null@*/ void *yasm_section_get_of_data(yasm_section *sect);

void yasm_sections_delete(yasm_sectionhead *headp);

void yasm_sections_print(FILE *f, int indent_level,
			 const yasm_sectionhead *headp);

/* Calls func for each section in the linked list of sections pointed to by
 * headp.  The data pointer d is passed to each func call.
 *
 * Stops early (and returns func's return value) if func returns a nonzero
 * value.  Otherwise returns 0.
 */
int yasm_sections_traverse(yasm_sectionhead *headp, /*@null@*/ void *d,
			   int (*func) (yasm_section *sect,
					/*@null@*/ void *d));

/*@dependent@*/ /*@null@*/ yasm_section *yasm_sections_find_general
    (yasm_sectionhead *headp, const char *name);

/*@dependent@*/ yasm_bytecodehead *yasm_section_get_bytecodes
    (yasm_section *sect);

/*@observer@*/ /*@null@*/ const char *yasm_section_get_name
    (const yasm_section *sect);

void yasm_section_set_start(yasm_section *sect, unsigned long start,
			    unsigned long lindex);
/*@observer@*/ const yasm_expr *yasm_section_get_start
    (const yasm_section *sect);

void yasm_section_delete(/*@only@*/ yasm_section *sect);

void yasm_section_print(FILE *f, int indent_level,
			/*@null@*/ const yasm_section *sect, int print_bcs);
#endif
