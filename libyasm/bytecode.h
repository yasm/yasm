/**
 * \file libyasm/bytecode.h
 * \brief YASM bytecode interface.
 *
 * \rcs
 * $IdPath$
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
/*@reldef@*/ STAILQ_HEAD(yasm_bytecodehead, yasm_bytecode);
/*@reldef@*/ STAILQ_HEAD(yasm_datavalhead, yasm_dataval);

/** Built-in bytecode types.  Additional types may be #yasm_arch defined
 * starting at #YASM_BYTECODE_TYPE_BASE.
 * \internal
 */
typedef enum {
    YASM_BC__EMPTY = 0,	    /**< Empty; should not exist except temporarily. */
    YASM_BC__DATA,	    /**< One or more data value(s). */
    YASM_BC__RESERVE,	    /**< Reserved space. */
    YASM_BC__INCBIN,	    /**< Included binary file. */
    YASM_BC__ALIGN,	    /**< Alignment to a boundary. */
    YASM_BC__OBJFMT_DATA    /**< yasm_objfmt specific data. */
} yasm_bytecode_type;

/** Starting yasm_bytecode_type numeric value available for yasm_arch use. */
#define YASM_BYTECODE_TYPE_BASE		YASM_BC__OBJFMT_DATA+1
#endif

/** Initialize bytecode utility functions.
 * \param a	architecture used during bytecode operations.
 */
void yasm_bc_initialize(yasm_arch *a);

/** Create an immediate value from an unsigned integer.
 * \param int_val   unsigned integer
 * \param lindex    line index (as from yasm_linemgr) for error/warning
 *		    purposes.
 * \return Newly allocated immediate value.
 */
/*@only@*/ yasm_immval *yasm_imm_new_int(unsigned long int_val,
					 unsigned long lindex);

/** Create an immediate value from an expression.
 * \param e	expression (kept, do not free).
 * \return Newly allocated immediate value.
 */
/*@only@*/ yasm_immval *yasm_imm_new_expr(/*@keep@*/ yasm_expr *e);

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
void yasm_ea_delete(/*@only@*/ yasm_effaddr *ea);

/** Print an effective address.  For debugging purposes.
 * \param f		file
 * \param indent_level	indentation level
 * \param ea		effective address
 */
void yasm_ea_print(FILE *f, int indent_level, const yasm_effaddr *ea);

/** Set multiple field of a bytecode.
 * A bytecode can be repeated a number of times when output.  This function
 * sets that multiple.
 * \param bc	bytecode
 * \param e	multiple (kept, do not free)
 */
void yasm_bc_set_multiple(yasm_bytecode *bc, /*@keep@*/ yasm_expr *e);

#ifdef YASM_LIB_INTERNAL
/** Create a bytecode of any specified type.
 * \param type		bytecode type
 * \param datasize	size of type-specific data (in bytes)
 * \param lindex	line index (as from yasm_linemgr) for the bytecode
 * \return Newly allocated bytecode of the specified type.
 */
/*@only@*/ yasm_bytecode *yasm_bc_new_common(yasm_bytecode_type type,
					     size_t datasize,
					     unsigned long lindex);
#endif

/** Create a bytecode containing data value(s).
 * \param datahead	list of data values (kept, do not free)
 * \param size		storage size (in bytes) for each data value
 * \param lindex	line index (as from yasm_linemgr) for the bytecode
 * \return Newly allocated bytecode.
 */
/*@only@*/ yasm_bytecode *yasm_bc_new_data(yasm_datavalhead *datahead,
					   unsigned int size,
					   unsigned long lindex);

/** Create a bytecode reserving space.
 * \param numitems	number of reserve "items" (kept, do not free)
 * \param itemsize	reserved size (in bytes) for each item
 * \param lindex	line index (as from yasm_linemgr) for the bytecode
 * \return Newly allocated bytecode.
 */
/*@only@*/ yasm_bytecode *yasm_bc_new_reserve(/*@only@*/ yasm_expr *numitems,
					      unsigned int itemsize,
					      unsigned long lindex);

/** Create a bytecode that includes a binary file verbatim.
 * \param filename	full path to binary file (kept, do not free)
 * \param start		starting location in file (in bytes) to read data from
 *			(kept, do not free); may be NULL to indicate 0
 * \param maxlen	maximum number of bytes to read from the file (kept, do
 *			do not free); may be NULL to indicate no maximum
 * \param lindex	line index (as from yasm_linemgr) for the bytecode
 * \return Newly allocated bytecode.
 */
/*@only@*/ yasm_bytecode *yasm_bc_new_incbin
    (/*@only@*/ char *filename, /*@only@*/ /*@null@*/ yasm_expr *start,
     /*@only@*/ /*@null@*/ yasm_expr *maxlen, unsigned long lindex);

/** Create a bytecode that aligns the following bytecode to a boundary.
 * \param boundary	byte alignment (must be a power of two)
 * \param lindex	line index (as from yasm_linemgr) for the bytecode
 * \return Newly allocated bytecode.
 */
/*@only@*/ yasm_bytecode *yasm_bc_new_align(unsigned long boundary,
					    unsigned long lindex);

/** Create a bytecode that includes yasm_objfmt-specific data.
 * \param type		yasm_objfmt-specific type
 * \param len		length (in bytes) of data
 * \param of		yasm_objfmt storing the data
 * \param data		data (kept, do not free)
 * \param lindex	line index (as from yasm_linemgr) for the bytecode
 * \return Newly allocated bytecode.
 */
/*@only@*/ yasm_bytecode *yasm_bc_new_objfmt_data
    (unsigned int type, unsigned long len, yasm_objfmt *of,
     /*@only@*/ void *data, unsigned long lindex);

/** Delete (free allocated memory for) a bytecode.
 * \param bc	bytecode (only pointer to it); may be NULL
 */
void yasm_bc_delete(/*@only@*/ /*@null@*/ yasm_bytecode *bc);

/** Print a bytecode.  For debugging purposes.
 * \param f		file
 * \param indent_level	indentation level
 * \param bc		bytecode
 */
void yasm_bc_print(FILE *f, int indent_level, const yasm_bytecode *bc);

/** Common version of calc_bc_dist that takes offsets from bytecodes.
 * Should be used for the final stages of optimizers as well as in yasm_objfmt
 * yasm_expr output functions.
 * \see yasm_calc_bc_dist_func for parameter descriptions.
 */
/*@null@*/ yasm_intnum *yasm_common_calc_bc_dist
    (yasm_section *sect, /*@null@*/ yasm_bytecode *precbc1,
     /*@null@*/ yasm_bytecode *precbc2);

/** Return value flags for yasm_bc_resolve(). */
typedef enum {
    YASM_BC_RESOLVE_NONE = 0,		/**< Ok, but length is not minimum. */
    YASM_BC_RESOLVE_ERROR = 1<<0,	/**< Error found, output. */
    YASM_BC_RESOLVE_MIN_LEN = 1<<1,	/**< Length is minimum possible. */
    YASM_BC_RESOLVE_UNKNOWN_LEN = 1<<2	/**< Length indeterminate. */
} yasm_bc_resolve_flags;

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
 * \param sect		section containing the bytecode
 * \param calc_bc_dist	function used to determine bytecode distance
 * \return Flags indicating whether the length is the minimum possible,
 *	   indeterminate, and if there was an error recognized (and output)
 *	   during execution.
 */
yasm_bc_resolve_flags yasm_bc_resolve(yasm_bytecode *bc, int save,
				      const yasm_section *sect,
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
 * \param sect		section containing the bytecode
 * \param d		data to pass to each call to output_expr
 * \param output_expr	function to call to convert expressions into their byte
 *			representation
 * \param output_bc_objfmt_data	function to call to convert yasm_objfmt data
 *				bytecodes into their byte representation
 * \return Newly allocated buffer that should be used instead of buf for
 *	   reading the byte representation, or NULL if buf was big enough to
 *	   hold the entire byte representation.
 * \note Essentially destroys contents of bytecode, so it's \em not safe to
 *       call twice on the same bytecode.
 */
/*@null@*/ /*@only@*/ unsigned char *yasm_bc_tobytes
    (yasm_bytecode *bc, unsigned char *buf, unsigned long *bufsize,
     /*@out@*/ unsigned long *multiple, /*@out@*/ int *gap,
     const yasm_section *sect, void *d, yasm_output_expr_func output_expr,
     /*@null@*/ yasm_output_bc_objfmt_data_func output_bc_objfmt_data)
    /*@sets *buf@*/;

/** Create list of bytecodes.
 * \return Newly allocated bytecode list.
 */
/*@only@*/ yasm_bytecodehead *yasm_bcs_new(void);

/** Get the first bytecode in a list of bytecodes.
 * \param headp		bytecode list
 * \return First bytecode in list (NULL if list is empty).
 */
/*@null@*/ yasm_bytecode *yasm_bcs_first(yasm_bytecodehead *headp);
#ifdef YASM_LIB_INTERNAL
#define yasm_bcs_first(headp)	STAILQ_FIRST(headp)
#endif

/** Get the last bytecode in a list of bytecodes.
 * \param headp		bytecode list
 * \return Last bytecode in list (NULL if list is empty).
 */
/*@null@*/ yasm_bytecode *yasm_bcs_last(yasm_bytecodehead *headp);

/** Delete (free allocated memory for) a list of bytecodes.
 * \param headp		bytecode list
 */
void yasm_bcs_delete(/*@only@*/ yasm_bytecodehead *headp);

/** Add bytecode to the end of a list of bytecodes.
 * \note Does not make a copy of bc; so don't pass this function static or
 *	 local variables, and discard the bc pointer after calling this
 *	 function.
 * \param headp		bytecode list
 * \param bc		bytecode (may be NULL)
 * \return If bytecode was actually appended (it wasn't NULL or empty), the
 *	   bytecode; otherwise NULL.
 */
/*@only@*/ /*@null@*/ yasm_bytecode *yasm_bcs_append
    (yasm_bytecodehead *headp,
     /*@returned@*/ /*@only@*/ /*@null@*/ yasm_bytecode *bc);

/** Print a bytecode list.  For debugging purposes.
 * \param f		file
 * \param indent_level	indentation level
 * \param headp		bytecode list
 */
void yasm_bcs_print(FILE *f, int indent_level, const yasm_bytecodehead *headp);

/** Traverses a bytecode list, calling a function on each bytecode.
 * \param headp	bytecode list
 * \param d	data pointer passed to func on each call
 * \param func	function
 * \return Stops early (and returns func's return value) if func returns a
 *	   nonzero value; otherwise 0.
 */
int yasm_bcs_traverse(yasm_bytecodehead *headp, /*@null@*/ void *d,
		      int (*func) (yasm_bytecode *bc, /*@null@*/ void *d));

/** Create a new data value from an expression.
 * \param expn	expression
 * \return Newly allocated data value.
 */
yasm_dataval *yasm_dv_new_expr(/*@keep@*/ yasm_expr *expn);

/** Create a new data value from a float.
 * \param flt	floating point value
 * \return Newly allocated data value.
 */
yasm_dataval *yasm_dv_new_float(/*@keep@*/ yasm_floatnum *flt);

/** Create a new data value from a string.
 * \param str_val	string
 * \return Newly allocated data value.
 */
yasm_dataval *yasm_dv_new_string(/*@keep@*/ char *str_val);

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
void yasm_dvs_delete(yasm_datavalhead *headp);

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
void yasm_dvs_print(FILE *f, int indent_level, const yasm_datavalhead *headp);

#endif
