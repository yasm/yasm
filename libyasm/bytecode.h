/**
 * \file libyasm/bytecode.h
 * \brief YASM bytecode interface.
 *
 * \rcs
 * $Id$
 * \endrcs
 *
 * \license
 *  Copyright (C) 2001  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
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
 * \endlicense
 */
#ifndef YASM_BYTECODE_H
#define YASM_BYTECODE_H

/** An effective address (opaque type). */
typedef struct yasm_effaddr yasm_effaddr;
/** An immediate value (opaque type). */
typedef struct yasm_immval yasm_immval;
/** A data value (opaque type). */
typedef struct yasm_dataval yasm_dataval;
/** A list of data values (opaque type). */
typedef struct yasm_datavalhead yasm_datavalhead;

#ifdef YASM_LIB_INTERNAL
/*@reldef@*/ STAILQ_HEAD(yasm_datavalhead, yasm_dataval);
#endif

/** Return value flags for yasm_bc_resolve(). */
typedef enum {
    YASM_BC_RESOLVE_NONE = 0,		/**< Ok, but length is not minimum. */
    YASM_BC_RESOLVE_ERROR = 1<<0,	/**< Error found, output. */
    YASM_BC_RESOLVE_MIN_LEN = 1<<1,	/**< Length is minimum possible. */
    YASM_BC_RESOLVE_UNKNOWN_LEN = 1<<2	/**< Length indeterminate. */
} yasm_bc_resolve_flags;

/** Create an immediate value from an unsigned integer.
 * \param int_val   unsigned integer
 * \param line	    virtual line (from yasm_linemap)
 * \return Newly allocated immediate value.
 */
/*@only@*/ yasm_immval *yasm_imm_create_int(unsigned long int_val,
					    unsigned long line);

/** Create an immediate value from an expression.
 * \param e	expression (kept, do not free).
 * \return Newly allocated immediate value.
 */
/*@only@*/ yasm_immval *yasm_imm_create_expr(/*@keep@*/ yasm_expr *e);

/** Get the displacement portion of an effective address.
 * \param ea	effective address
 * \return Expression representing the displacement (read-only).
 */
/*@observer@*/ const yasm_expr *yasm_ea_get_disp(const yasm_effaddr *ea);

/** Set the length of the displacement portion of an effective address.
 * The length is specified in bytes.
 * \param ea	effective address
 * \param len	length in bytes
 */
void yasm_ea_set_len(yasm_effaddr *ea, unsigned int len);

/** Set/clear nosplit flag of an effective address.
 * The nosplit flag indicates (for architectures that support complex effective
 * addresses such as x86) if various types of complex effective addresses can
 * be split into different forms in order to minimize instruction length.
 * \param ea		effective address
 * \param nosplit	nosplit flag setting (0=splits allowed, nonzero=splits
 *			not allowed)
 */
void yasm_ea_set_nosplit(yasm_effaddr *ea, unsigned int nosplit);

/** Delete (free allocated memory for) an effective address.
 * \param ea	effective address (only pointer to it).
 */
void yasm_ea_destroy(/*@only@*/ yasm_effaddr *ea);

/** Print an effective address.  For debugging purposes.
 * \param f		file
 * \param indent_level	indentation level
 * \param ea		effective address
 */
void yasm_ea_print(const yasm_effaddr *ea, FILE *f, int indent_level);

/** Set multiple field of a bytecode.
 * A bytecode can be repeated a number of times when output.  This function
 * sets that multiple.
 * \param bc	bytecode
 * \param e	multiple (kept, do not free)
 */
void yasm_bc_set_multiple(yasm_bytecode *bc, /*@keep@*/ yasm_expr *e);

/** Create a bytecode containing data value(s).
 * \param datahead	list of data values (kept, do not free)
 * \param size		storage size (in bytes) for each data value
 * \param line		virtual line (from yasm_linemap)
 * \return Newly allocated bytecode.
 */
/*@only@*/ yasm_bytecode *yasm_bc_create_data
    (yasm_datavalhead *datahead, unsigned int size, unsigned long line);

/** Create a bytecode reserving space.
 * \param numitems	number of reserve "items" (kept, do not free)
 * \param itemsize	reserved size (in bytes) for each item
 * \param line		virtual line (from yasm_linemap)
 * \return Newly allocated bytecode.
 */
/*@only@*/ yasm_bytecode *yasm_bc_create_reserve
    (/*@only@*/ yasm_expr *numitems, unsigned int itemsize,
     unsigned long line);

/** Create a bytecode that includes a binary file verbatim.
 * \param filename	full path to binary file (kept, do not free)
 * \param start		starting location in file (in bytes) to read data from
 *			(kept, do not free); may be NULL to indicate 0
 * \param maxlen	maximum number of bytes to read from the file (kept, do
 *			do not free); may be NULL to indicate no maximum
 * \param line		virtual line (from yasm_linemap) for the bytecode
 * \return Newly allocated bytecode.
 */
/*@only@*/ yasm_bytecode *yasm_bc_create_incbin
    (/*@only@*/ char *filename, /*@only@*/ /*@null@*/ yasm_expr *start,
     /*@only@*/ /*@null@*/ yasm_expr *maxlen, unsigned long line);

/** Create a bytecode that aligns the following bytecode to a boundary.
 * \param boundary	byte alignment (must be a power of two)
 * \param line		virtual line (from yasm_linemap)
 * \return Newly allocated bytecode.
 */
/*@only@*/ yasm_bytecode *yasm_bc_create_align
    (unsigned long boundary, unsigned long line);

/** Get the section that contains a particular bytecode.
 * \param bc	bytecode
 * \return Section containing bc (can be NULL if bytecode is not part of a
 *	   section).
 */
/*@dependent@*/ /*@null@*/ yasm_section *yasm_bc_get_section
    (yasm_bytecode *bc);

#ifdef YASM_LIB_INTERNAL
/** Add to the list of symrecs that reference a bytecode.  For symrec use
 * only.
 * \param bc	bytecode
 * \param sym	symbol
 */
void yasm_bc__add_symrec(yasm_bytecode *bc, /*@dependent@*/ yasm_symrec *sym);
#endif
    
/** Delete (free allocated memory for) a bytecode.
 * \param bc	bytecode (only pointer to it); may be NULL
 */
void yasm_bc_destroy(/*@only@*/ /*@null@*/ yasm_bytecode *bc);

/** Print a bytecode.  For debugging purposes.
 * \param f		file
 * \param indent_level	indentation level
 * \param bc		bytecode
 */
void yasm_bc_print(const yasm_bytecode *bc, FILE *f, int indent_level);

/** Common version of calc_bc_dist that takes offsets from bytecodes.
 * Should be used for the final stages of optimizers as well as in yasm_objfmt
 * yasm_expr output functions.
 * \see yasm_calc_bc_dist_func for parameter descriptions.
 */
/*@null@*/ yasm_intnum *yasm_common_calc_bc_dist
    (/*@null@*/ yasm_bytecode *precbc1, /*@null@*/ yasm_bytecode *precbc2);

/** Resolve labels in a bytecode, and calculate its length.
 * Tries to minimize the length as much as possible.
 * \note Sometimes it's impossible to determine if a length is the minimum
 *       possible.  In this case, this function returns that the length is NOT
 *       the minimum.
 * \param bc		bytecode
 * \param save		when zero, this function does \em not modify bc other
 *			than the length/size values (i.e. it doesn't keep the
 *			values returned by calc_bc_dist except temporarily to
 *			try to minimize the length); when nonzero, all fields
 *			in bc may be modified by this function
 * \param calc_bc_dist	function used to determine bytecode distance
 * \return Flags indicating whether the length is the minimum possible,
 *	   indeterminate, and if there was an error recognized (and output)
 *	   during execution.
 */
yasm_bc_resolve_flags yasm_bc_resolve(yasm_bytecode *bc, int save,
				      yasm_calc_bc_dist_func calc_bc_dist);

/** Convert a bytecode into its byte representation.
 * \param bc	 	bytecode
 * \param buf		byte representation destination buffer
 * \param bufsize	size of buf (in bytes) prior to call; size of the
 *			generated data after call
 * \param multiple	number of times the data should be duplicated when
 *			written to the object file [output]
 * \param gap		if nonzero, indicates the data does not really need to
 *			exist in the object file; if nonzero, contents of buf
 *			are undefined [output]
 * \param d		data to pass to each call to output_expr/output_reloc
 * \param output_expr	function to call to convert expressions into their byte
 *			representation
 * \param output_reloc	function to call to output relocation entries
 *			for a single sym
 * \return Newly allocated buffer that should be used instead of buf for
 *	   reading the byte representation, or NULL if buf was big enough to
 *	   hold the entire byte representation.
 * \note Essentially destroys contents of bytecode, so it's \em not safe to
 *       call twice on the same bytecode.
 */
/*@null@*/ /*@only@*/ unsigned char *yasm_bc_tobytes
    (yasm_bytecode *bc, unsigned char *buf, unsigned long *bufsize,
     /*@out@*/ unsigned long *multiple, /*@out@*/ int *gap, void *d,
     yasm_output_expr_func output_expr,
     /*@null@*/ yasm_output_reloc_func output_reloc)
    /*@sets *buf@*/;

/** Create a new data value from an expression.
 * \param expn	expression
 * \return Newly allocated data value.
 */
yasm_dataval *yasm_dv_create_expr(/*@keep@*/ yasm_expr *expn);

/** Create a new data value from a float.
 * \param flt	floating point value
 * \return Newly allocated data value.
 */
yasm_dataval *yasm_dv_create_float(/*@keep@*/ yasm_floatnum *flt);

/** Create a new data value from a string.
 * \param str_val	string
 * \return Newly allocated data value.
 */
yasm_dataval *yasm_dv_create_string(/*@keep@*/ char *str_val);

/** Initialize a list of data values.
 * \param headp	list of data values
 */
void yasm_dvs_initialize(yasm_datavalhead *headp);
#ifdef YASM_LIB_INTERNAL
#define	yasm_dvs_initialize(headp)	STAILQ_INIT(headp)
#endif

/** Delete (free allocated memory for) a list of data values.
 * \param headp	list of data values
 */
void yasm_dvs_destroy(yasm_datavalhead *headp);

/** Add data value to the end of a list of data values.
 * \note Does not make a copy of the data value; so don't pass this function
 *	 static or local variables, and discard the dv pointer after calling
 *	 this function.
 * \param headp		data value list
 * \param dv		data value (may be NULL)
 * \return If data value was actually appended (it wasn't NULL), the data
 *	   value; otherwise NULL.
 */
/*@null@*/ yasm_dataval *yasm_dvs_append
    (yasm_datavalhead *headp, /*@returned@*/ /*@null@*/ yasm_dataval *dv);

/** Print a data value list.  For debugging purposes.
 * \param f		file
 * \param indent_level	indentation level
 * \param headp		data value list
 */
void yasm_dvs_print(const yasm_datavalhead *headp, FILE *f, int indent_level);

#endif
