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

struct objfmt_s;

typedef STAILQ_HEAD(sectionhead, section) sectionhead;

#ifndef YASM_SECTION
#define YASM_SECTION
typedef struct section section;
#endif

section *sections_initialize(sectionhead *headp, struct objfmt_s *of);

section *sections_switch(sectionhead *headp, struct objfmt_s *of,
			 const char *name);

void sections_print(const sectionhead *headp);

void sections_parser_finalize(sectionhead *headp);

bytecodehead *section_get_bytecodes(section *sect);

const char *section_get_name(const section *sect);

void section_print(const section *sect);
#endif
