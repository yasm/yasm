/* $IdPath$
 * YASM bytecode utility functions header file
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
#ifndef YASM_BYTECODE_H
#define YASM_BYTECODE_H

typedef struct yasm_effaddr yasm_effaddr;
typedef struct yasm_immval yasm_immval;
typedef struct yasm_dataval yasm_dataval;
typedef struct yasm_datavalhead yasm_datavalhead;

#ifdef YASM_INTERNAL
/*@reldef@*/ STAILQ_HEAD(yasm_bytecodehead, yasm_bytecode);
/*@reldef@*/ STAILQ_HEAD(yasm_datavalhead, yasm_dataval);

/* Additional types may be architecture-defined starting at
 * YASM_BYTECODE_TYPE_BASE.
 */
typedef enum {
    YASM_BC__EMPTY = 0,
    YASM_BC__DATA,
    YASM_BC__RESERVE,
    YASM_BC__INCBIN,
    YASM_BC__ALIGN,
    YASM_BC__OBJFMT_DATA
} yasm_bytecode_type;
#define YASM_BYTECODE_TYPE_BASE		YASM_BC__OBJFMT_DATA+1
#endif

void yasm_bc_initialize(yasm_arch *a);

/*@only@*/ yasm_immval *yasm_imm_new_int(unsigned long int_val,
					 unsigned long lindex);
/*@only@*/ yasm_immval *yasm_imm_new_expr(/*@keep@*/ yasm_expr *e);

/*@observer@*/ const yasm_expr *yasm_ea_get_disp(const yasm_effaddr *ea);
void yasm_ea_set_len(yasm_effaddr *ea, unsigned char len);
void yasm_ea_set_nosplit(yasm_effaddr *ea, unsigned char nosplit);
void yasm_ea_delete(/*@only@*/ yasm_effaddr *ea);
void yasm_ea_print(FILE *f, int indent_level, const yasm_effaddr *ea);

void yasm_bc_set_multiple(yasm_bytecode *bc, /*@keep@*/ yasm_expr *e);

/*@only@*/ yasm_bytecode *yasm_bc_new_common(yasm_bytecode_type type,
					     size_t datasize,
					     unsigned long lindex);
/*@only@*/ yasm_bytecode *yasm_bc_new_data(yasm_datavalhead *datahead,
					   unsigned char size,
					   unsigned long lindex);
/*@only@*/ yasm_bytecode *yasm_bc_new_reserve(/*@only@*/ yasm_expr *numitems,
					      unsigned char itemsize,
					      unsigned long lindex);
/*@only@*/ yasm_bytecode *yasm_bc_new_incbin
    (/*@only@*/ char *filename, /*@only@*/ /*@null@*/ yasm_expr *start,
     /*@only@*/ /*@null@*/ yasm_expr *maxlen, unsigned long lindex);
/*@only@*/ yasm_bytecode *yasm_bc_new_align(unsigned long boundary,
					    unsigned long lindex);
/*@only@*/ yasm_bytecode *yasm_bc_new_objfmt_data
    (unsigned int type, unsigned long len, yasm_objfmt *of,
     /*@only@*/ void *data, unsigned long lindex);

void yasm_bc_delete(/*@only@*/ /*@null@*/ yasm_bytecode *bc);

void yasm_bc_print(FILE *f, int indent_level, const yasm_bytecode *bc);

/* A common version of a calc_bc_dist function that should work for the final
 * stages of optimizers as well as in objfmt expr output functions.  It takes
 * the offsets from the bytecodes.
 */
/*@null@*/ yasm_intnum *yasm_common_calc_bc_dist
    (yasm_section *sect, /*@null@*/ yasm_bytecode *precbc1,
     /*@null@*/ yasm_bytecode *precbc2);

/* Return value flags for bc_resolve() */
typedef enum {
    YASM_BC_RESOLVE_NONE = 0,		/* Ok, but length is not minimum */
    YASM_BC_RESOLVE_ERROR = 1<<0,	/* Error found, output */
    YASM_BC_RESOLVE_MIN_LEN = 1<<1,	/* Length is minimum possible */
    YASM_BC_RESOLVE_UNKNOWN_LEN = 1<<2	/* Length indeterminate */
} yasm_bc_resolve_flags;

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
yasm_bc_resolve_flags yasm_bc_resolve(yasm_bytecode *bc, int save,
				      const yasm_section *sect,
				      yasm_calc_bc_dist_func calc_bc_dist);

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
/*@null@*/ /*@only@*/ unsigned char *yasm_bc_tobytes
    (yasm_bytecode *bc, unsigned char *buf, unsigned long *bufsize,
     /*@out@*/ unsigned long *multiple, /*@out@*/ int *gap,
     const yasm_section *sect, void *d, yasm_output_expr_func output_expr,
     /*@null@*/ yasm_output_bc_objfmt_data_func output_bc_objfmt_data)
    /*@sets *buf@*/;

void yasm_bcs_initialize(yasm_bytecodehead *headp);
#ifdef YASM_INTERNAL
#define	yasm_bcs_initialize(headp)	STAILQ_INIT(headp)
#endif

yasm_bytecode *yasm_bcs_first(yasm_bytecodehead *headp);
#ifdef YASM_INTERNAL
#define yasm_bcs_first(headp)	STAILQ_FIRST(headp)
#endif

/*@null@*/ yasm_bytecode *yasm_bcs_last(yasm_bytecodehead *headp);
void yasm_bcs_delete(yasm_bytecodehead *headp);

/* Adds bc to the list of bytecodes headp.
 * NOTE: Does not make a copy of bc; so don't pass this function
 * static or local variables, and discard the bc pointer after calling
 * this function.  If bc was actually appended (it wasn't NULL or empty),
 * then returns bc, otherwise returns NULL.
 */
/*@only@*/ /*@null@*/ yasm_bytecode *yasm_bcs_append
    (yasm_bytecodehead *headp,
     /*@returned@*/ /*@only@*/ /*@null@*/ yasm_bytecode *bc);

void yasm_bcs_print(FILE *f, int indent_level, const yasm_bytecodehead *headp);

/* Calls func for each bytecode in the linked list of bytecodes pointed to by
 * headp.  The data pointer d is passed to each func call.
 *
 * Stops early (and returns func's return value) if func returns a nonzero
 * value.  Otherwise returns 0.
 */
int yasm_bcs_traverse(yasm_bytecodehead *headp, /*@null@*/ void *d,
		      int (*func) (yasm_bytecode *bc, /*@null@*/ void *d));

yasm_dataval *yasm_dv_new_expr(/*@keep@*/ yasm_expr *expn);
yasm_dataval *yasm_dv_new_float(/*@keep@*/ yasm_floatnum *flt);
yasm_dataval *yasm_dv_new_string(/*@keep@*/ char *str_val);

void yasm_dvs_initialize(yasm_datavalhead *headp);
#ifdef YASM_INTERNAL
#define	yasm_dvs_initialize(headp)	STAILQ_INIT(headp)
#endif

void yasm_dvs_delete(yasm_datavalhead *headp);

/* Adds dv to the list of datavals headp.
 * NOTE: Does not make a copy of dv; so don't pass this function
 * static or local variables, and discard the dv pointer after calling
 * this function.  If dv was actually appended (it wasn't NULL), then
 * returns dv, otherwise returns NULL.
 */
/*@null@*/ yasm_dataval *yasm_dvs_append
    (yasm_datavalhead *headp, /*@returned@*/ /*@null@*/ yasm_dataval *dv);

void yasm_dvs_print(FILE *f, int indent_level, const yasm_datavalhead *head);

#endif
