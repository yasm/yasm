/* $IdPath$
 * Section header file
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
