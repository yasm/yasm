/* $IdPath$
 * Globals header file
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
#ifndef YASM_GLOBALS_H
#define YASM_GLOBALS_H

/* Current (selected) parser */
extern /*@null@*/ parser *cur_parser;

/* Current (selected) object format */
extern /*@null@*/ objfmt *cur_objfmt;

/* Virtual line number.  Uniquely specifies every line read by the parser. */
extern unsigned long line_index;

/* Global assembler options. */
extern unsigned int asm_options;

/* Indentation level for assembler *_print() routines */
extern int indent_level;

void line_set(const char *filename, unsigned long line,
	      unsigned long line_inc);
void line_shutdown(void);
void line_lookup(unsigned long index, const char **filename,
		 unsigned long *line);

#endif
