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

/*@dependent@*/ section *sections_initialize(sectionhead *headp, objfmt *of);

/*@dependent@*/ section *sections_switch_general(sectionhead *headp,
    const char *name, unsigned long start, int res_only, /*@out@*/ int *isnew,
    unsigned long lindex,
    /*@exits@*/ void (*error_func) (const char *file, unsigned int line,
				    const char *message));

/*@dependent@*/ section *sections_switch_absolute(sectionhead *headp,
    /*@keep@*/ expr *start,
    /*@exits@*/ void (*error_func) (const char *file, unsigned int line,
				    const char *message));

int section_is_absolute(section *sect);

/* Get and set optimizer flags */
unsigned long section_get_opt_flags(const section *sect);
void section_set_opt_flags(section *sect, unsigned long opt_flags);

void section_set_of_data(section *sect, objfmt *of,
			 /*@null@*/ /*@only@*/ void *of_data);
/*@dependent@*/ /*@null@*/ void *section_get_of_data(section *sect);

void sections_delete(sectionhead *headp);

void sections_print(FILE *f, int indent_level, const sectionhead *headp);

/* Calls func for each section in the linked list of sections pointed to by
 * headp.  The data pointer d is passed to each func call.
 *
 * Stops early (and returns func's return value) if func returns a nonzero
 * value.  Otherwise returns 0.
 */
int sections_traverse(sectionhead *headp, /*@null@*/ void *d,
		      int (*func) (section *sect, /*@null@*/ void *d));

/*@dependent@*/ /*@null@*/ section *sections_find_general(sectionhead *headp,
							  const char *name);

/*@dependent@*/ bytecodehead *section_get_bytecodes(section *sect);

/*@observer@*/ /*@null@*/ const char *section_get_name(const section *sect);

void section_set_start(section *sect, unsigned long start,
		       unsigned long lindex);
/*@observer@*/ const expr *section_get_start(const section *sect);

void section_delete(/*@only@*/ section *sect);

void section_print(FILE *f, int indent_level, /*@null@*/ const section *sect,
		   int print_bcs);
#endif
