/* $Id: section.h,v 1.2 2001/06/28 21:22:01 peter Exp $
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

typedef struct section_s {
    struct section_s *next;

    enum type { SECTION, ABSOLUTE };

    union {
	/* SECTION data */
	char *name;
	/* ABSOLUTE data */
	unsigned long start;
    } data;

    bytecode *bc;	/* the bytecodes for the section's contents */
} section;

#endif
