/* $IdPath$
 * Bytecode internal structures header file
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
#ifndef YASM_BC_INT_H
#define YASM_BC_INT_H

struct effaddr {
    /*@only@*/ /*@null@*/ expr *disp;	/* address displacement */
    unsigned char len;		/* length of disp (in bytes), 0 if unknown,
				 * 0xff if unknown and required to be >0.
				 */
    unsigned char nosplit;	/* 1 if reg*2 should not be split into
				   reg+reg. (0 if not) */

    /* architecture-dependent data may be appended */
};
void *ea_get_data(effaddr *);
#define ea_get_data(x)		(void *)(((char *)x)+sizeof(effaddr))
const void *ea_get_const_data(const effaddr *);
#define ea_get_const_data(x)	(const void *)(((const char *)x)+sizeof(effaddr))

struct immval {
    /*@only@*/ /*@null@*/ expr *val;

    unsigned char len;		/* length of val (in bytes), 0 if unknown */
    unsigned char isneg;	/* the value has been explicitly negated */

    unsigned char f_len;	/* final imm length */
    unsigned char f_sign;	/* 1 if final imm should be signed */
};

struct bytecode {
    /*@reldef@*/ STAILQ_ENTRY(bytecode) link;

    bytecode_type type;

    /* number of times bytecode is repeated, NULL=1. */
    /*@only@*/ /*@null@*/ expr *multiple;

    unsigned long len;		/* total length of entire bytecode (including
				   multiple copies), 0 if unknown */

    /* where it came from */
    /*@dependent@*/ /*@null@*/ const char *filename;
    unsigned int lineno;

    /* other assembler state info */
    unsigned long offset;	/* 0 if unknown */

    /* architecture-dependent data may be appended */
};
void *bc_get_data(bytecode *);
#define bc_get_data(x)		(void *)(((char *)x)+sizeof(bytecode))
const void *bc_get_const_data(const bytecode *);
#define bc_get_const_data(x)	(const void *)(((const char *)x)+sizeof(bytecode))

#endif
