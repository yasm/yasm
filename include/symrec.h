/* $Id: symrec.h,v 1.1 2001/05/15 05:28:06 peter Exp $
 * Symbol table handling header file
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
#ifndef _SYMREC_H_
#define _SYMREC_H_

typedef struct symrec_s {
    char *name;
    int type;
    union {
	double var;
	double (*fnctptr) ();
    } value;
    struct symrec_s *next;
} symrec;

extern symrec *sym_table;

symrec *putsym(char *, int);
symrec *getsym(char *);

#endif
