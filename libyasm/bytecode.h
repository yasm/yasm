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
    BC_INCBIN,
    BC_ALIGN,
    BC_OBJFMT_DATA
} bytecode_type;
#define BYTECODE_TYPE_BASE  BC_OBJFMT_DATA+1

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
/*@only@*/ bytecode *bc_new_align(unsigned long boundary);
/*@only@*/ bytecode *bc_new_objfmt_data(unsigned int type, unsigned long len,
					/*@only@*/ void *data);

void bc_delete(/*@only@*/ /*@null@*/ bytecode *bc);

void bc_print(FILE *f, const bytecode *bc);

/* Return value flags for bc_resolve() */
typedef enum {
    BC_RESOLVE_NONE = 0,		/* Ok, but length is not minimum */
    BC_RESOLVE_ERROR = 1<<0,		/* Error found, output */
    BC_RESOLVE_MIN_LEN = 1<<1,		/* Length is minimum possible */
    BC_RESOLVE_UNKNOWN_LEN = 1<<2	/* Length indeterminate */
} bc_resolve_flags;

/* Resolves labels in bytecode, and calculates its length.
 * Tries to minimize the length as much as possible.
 * Returns whether the length is the minimum possible, indeterminate, and
 *  if there was an error recognized and output during execution (see above
 *  for return flags).
 * Note: sometimes it's impossible to determine if a length is the minimum
 *       possible.  In this case, this function returns that the length is NOT
 *       the minimum.
 * resolve_label is the function used to determine the value (offset) of a
 *  in-file label (eg, not an EXTERN variable, which is indeterminate).
 * When save is zero, this function does *not* modify bc other than the
 * length/size values (i.e. it doesn't keep the values returned by
 * resolve_label except temporarily to try to minimize the length).
 * When save is nonzero, all fields in bc may be modified by this function.
 */
bc_resolve_flags bc_resolve(bytecode *bc, int save, const section *sect,
			    resolve_label_func resolve_label);

/* Converts the bytecode bc into its byte representation.
 * Inputs:
 *  bc            - the bytecode to convert
 *  buf           - where to put the byte representation
 *  bufsize       - the size of buf
 *  d             - the data to pass to each call to output_expr()
 *  output_expr   - the function to call to convert expressions to byte rep
 *  output_bc_objfmt_data - the function to call to convert objfmt data
 *                  bytecodes into their byte representation
 * Outputs:
 *  bufsize       - the size of the generated data.
 *  multiple      - the number of times the data should be dup'ed when output
 *  gap           - indicates the data does not really need to exist in the
 *                  object file.  buf's contents are undefined if true.
 * Returns either NULL (if buf was big enough to hold the entire byte
 * representation), or a newly allocated buffer that should be used instead
 * of buf for reading the byte representation.
 */
/*@null@*/ /*@only@*/ unsigned char *bc_tobytes(bytecode *bc,
    unsigned char *buf, unsigned long *bufsize,
    /*@out@*/ unsigned long *multiple, /*@out@*/ int *gap, const section *sect,
    void *d, output_expr_func output_expr,
    /*@null@*/ output_bc_objfmt_data_func output_bc_objfmt_data)
    /*@sets *buf@*/;

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

/* Calls func for each bytecode in the linked list of bytecodes pointed to by
 * headp.  The data pointer d is passed to each func call.
 *
 * Stops early (and returns func's return value) if func returns a nonzero
 * value.  Otherwise returns 0.
 */
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
