/* $IdPath$
 * Bytecode utility functions header file
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
#ifndef YASM_BYTECODE_H
#define YASM_BYTECODE_H

typedef struct effaddr effaddr;
typedef struct immval immval;
typedef /*@reldef@*/ STAILQ_HEAD(datavalhead, dataval) datavalhead;
typedef struct dataval dataval;

/* Additional types may be architecture-defined starting at
 * BYTECODE_TYPE_BASE.
 */
typedef enum {
    BC_EMPTY = 0,
    BC_DATA,
    BC_RESERVE,
    BC_INCBIN
} bytecode_type;
#define BYTECODE_TYPE_BASE  BC_INCBIN+1

/*@only@*/ immval *imm_new_int(unsigned long int_val);
/*@only@*/ immval *imm_new_expr(/*@keep@*/ expr *e);

void ea_set_len(effaddr *ea, unsigned char len);
void ea_set_nosplit(effaddr *ea, unsigned char nosplit);

void bc_set_multiple(bytecode *bc, /*@keep@*/ expr *e);

/*@only@*/ bytecode *bc_new_common(bytecode_type type, size_t datasize);
/*@only@*/ bytecode *bc_new_data(datavalhead *datahead, unsigned char size);
/*@only@*/ bytecode *bc_new_reserve(/*@only@*/ expr *numitems,
				    unsigned char itemsize);
/*@only@*/ bytecode *bc_new_incbin(/*@only@*/ char *filename,
				   /*@only@*/ /*@null@*/ expr *start,
				   /*@only@*/ /*@null@*/ expr *maxlen);

void bc_delete(/*@only@*/ /*@null@*/ bytecode *bc);

void bc_print(FILE *f, const bytecode *bc);

/* Calculates length of bytecode, saving in bc structure.
 * Returns whether the length is the minimum possible (1=yes, 0=no).
 * resolve_label is the function used to determine the value (offset) of a
 *  in-file label (eg, not an EXTERN variable, which is indeterminate).
 */
int bc_calc_len(bytecode *bc, intnum *(*resolve_label) (symrec *sym));

/* void bcs_initialize(bytecodehead *headp); */
#define	bcs_initialize(headp)	STAILQ_INIT(headp)

/* bytecode *bcs_first(bytecodehead *headp); */
#define bcs_first(headp)	STAILQ_FIRST(headp)

void bcs_delete(bytecodehead *headp);

/* Adds bc to the list of bytecodes headp.
 * NOTE: Does not make a copy of bc; so don't pass this function
 * static or local variables, and discard the bc pointer after calling
 * this function.  If bc was actually appended (it wasn't NULL or empty),
 * then returns bc, otherwise returns NULL.
 */
/*@only@*/ /*@null@*/ bytecode *bcs_append(bytecodehead *headp,
					   /*@returned@*/ /*@only@*/ /*@null@*/
					   bytecode *bc);

void bcs_print(FILE *f, const bytecodehead *headp);

int bcs_traverse(bytecodehead *headp, /*@null@*/ void *d,
		 int (*func) (bytecode *bc, /*@null@*/ void *d));

dataval *dv_new_expr(/*@keep@*/ expr *expn);
dataval *dv_new_float(/*@keep@*/ floatnum *flt);
dataval *dv_new_string(/*@keep@*/ char *str_val);

void dvs_initialize(datavalhead *headp);
#define	dvs_initialize(headp)	STAILQ_INIT(headp)

void dvs_delete(datavalhead *headp);

/* Adds dv to the list of datavals headp.
 * NOTE: Does not make a copy of dv; so don't pass this function
 * static or local variables, and discard the dv pointer after calling
 * this function.  If dv was actually appended (it wasn't NULL), then
 * returns dv, otherwise returns NULL.
 */
/*@null@*/ dataval *dvs_append(datavalhead *headp,
			       /*@returned@*/ /*@null@*/ dataval *dv);

void dvs_print(FILE *f, const datavalhead *head);

#endif
