/**
 * \file libyasm/bytecode.h
 * \brief YASM bytecode interface.
 *
 * \rcs
 * $Id$
 * \endrcs
 *
 * \license
 *  Copyright (C) 2001-2007  Peter Johnson
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

/** An effective address. */
typedef struct yasm_effaddr yasm_effaddr;

/** Callbacks for effective address implementations. */
typedef struct yasm_effaddr_callback {
    /** Destroy the effective address (freeing it).
     * \param ea        effective address
     */
    void (*destroy) (/*@only@*/ yasm_effaddr *ea);

    /** Print the effective address.
     * \param ea                effective address
     * \param f                 file to output to
     * \param indent_level      indentation level
     */
    void (*print) (const yasm_effaddr *ea, FILE *f, int indent_level);
} yasm_effaddr_callback;

/** An effective address. */
struct yasm_effaddr {
    const yasm_effaddr_callback *callback;      /**< callback functions */

    yasm_value disp;            /**< address displacement */

    uintptr_t segreg;           /**< segment register override (0 if none) */

    unsigned char need_nonzero_len; /**< 1 if length of disp must be >0. */
    unsigned char need_disp;    /**< 1 if a displacement should be present
                                 *   in the output.
                                 */
    unsigned char nosplit;      /**< 1 if reg*2 should not be split into
                                 *   reg+reg. (0 if not)
                                 */
    unsigned char strong;       /**< 1 if effective address is *definitely*
                                 *   an effective address, e.g. in GAS if
                                 *   expr(,1) form is used vs. just expr.
                                 */
};

/** A data value (opaque type). */
typedef struct yasm_dataval yasm_dataval;
/** A list of data values (opaque type). */
typedef struct yasm_datavalhead yasm_datavalhead;

#ifdef YASM_LIB_INTERNAL
/*@reldef@*/ STAILQ_HEAD(yasm_datavalhead, yasm_dataval);
#endif

/** Get the displacement portion of an effective address.
 * \param ea    effective address
 * \return Expression representing the displacement (read-only).
 */
/*@observer@*/ const yasm_expr *yasm_ea_get_disp(const yasm_effaddr *ea);

/** Set the length of the displacement portion of an effective address.
 * The length is specified in bits.
 * \param ea    effective address
 * \param len   length in bits
 */
void yasm_ea_set_len(yasm_effaddr *ea, unsigned int len);

/** Set/clear nosplit flag of an effective address.
 * The nosplit flag indicates (for architectures that support complex effective
 * addresses such as x86) if various types of complex effective addresses can
 * be split into different forms in order to minimize instruction length.
 * \param ea            effective address
 * \param nosplit       nosplit flag setting (0=splits allowed, nonzero=splits
 *                      not allowed)
 */
void yasm_ea_set_nosplit(yasm_effaddr *ea, unsigned int nosplit);

/** Set/clear strong flag of an effective address.
 * The strong flag indicates if an effective address is *definitely* an
 * effective address.  This is used in e.g. the GAS parser to differentiate
 * between "expr" (which might or might not be an effective address) and
 * "expr(,1)" (which is definitely an effective address).
 * \param ea            effective address
 * \param strong        strong flag setting (0=not strong, nonzero=strong)
 */
void yasm_ea_set_strong(yasm_effaddr *ea, unsigned int strong);

/** Set segment override for an effective address.
 * Some architectures (such as x86) support segment overrides on effective
 * addresses.  A override of an override will result in a warning.
 * \param ea            effective address
 * \param segreg        segment register (0 if none)
 */
void yasm_ea_set_segreg(yasm_effaddr *ea, uintptr_t segreg);

/** Delete (free allocated memory for) an effective address.
 * \param ea    effective address (only pointer to it).
 */
void yasm_ea_destroy(/*@only@*/ yasm_effaddr *ea);

/** Print an effective address.  For debugging purposes.
 * \param f             file
 * \param indent_level  indentation level
 * \param ea            effective address
 */
void yasm_ea_print(const yasm_effaddr *ea, FILE *f, int indent_level);

/** Set multiple field of a bytecode.
 * A bytecode can be repeated a number of times when output.  This function
 * sets that multiple.
 * \param bc    bytecode
 * \param e     multiple (kept, do not free)
 */
void yasm_bc_set_multiple(yasm_bytecode *bc, /*@keep@*/ yasm_expr *e);

/** Create a bytecode containing data value(s).
 * \param datahead      list of data values (kept, do not free)
 * \param size          storage size (in bytes) for each data value
 * \param append_zero   append a single zero byte after each data value
 *                      (if non-zero)
 * \param arch          architecture (optional); if provided, data items
 *                      are directly simplified to bytes if possible
 * \param line          virtual line (from yasm_linemap)
 * \return Newly allocated bytecode.
 */
/*@only@*/ yasm_bytecode *yasm_bc_create_data
    (yasm_datavalhead *datahead, unsigned int size, int append_zero,
     /*@null@*/ yasm_arch *arch, unsigned long line);

/** Create a bytecode containing LEB128-encoded data value(s).
 * \param datahead      list of data values (kept, do not free)
 * \param sign          signedness (1=signed, 0=unsigned) of each data value
 * \param line          virtual line (from yasm_linemap)
 * \return Newly allocated bytecode.
 */
/*@only@*/ yasm_bytecode *yasm_bc_create_leb128
    (yasm_datavalhead *datahead, int sign, unsigned long line);

/** Create a bytecode reserving space.
 * \param numitems      number of reserve "items" (kept, do not free)
 * \param itemsize      reserved size (in bytes) for each item
 * \param line          virtual line (from yasm_linemap)
 * \return Newly allocated bytecode.
 */
/*@only@*/ yasm_bytecode *yasm_bc_create_reserve
    (/*@only@*/ yasm_expr *numitems, unsigned int itemsize,
     unsigned long line);

/** Create a bytecode that includes a binary file verbatim.
 * \param filename      path to binary file (kept, do not free)
 * \param start         starting location in file (in bytes) to read data from
 *                      (kept, do not free); may be NULL to indicate 0
 * \param maxlen        maximum number of bytes to read from the file (kept, do
 *                      do not free); may be NULL to indicate no maximum
 * \param linemap       line mapping repository
 * \param line          virtual line (from yasm_linemap) for the bytecode
 * \return Newly allocated bytecode.
 */
/*@only@*/ yasm_bytecode *yasm_bc_create_incbin
    (/*@only@*/ char *filename, /*@only@*/ /*@null@*/ yasm_expr *start,
     /*@only@*/ /*@null@*/ yasm_expr *maxlen, yasm_linemap *linemap,
     unsigned long line);

/** Create a bytecode that aligns the following bytecode to a boundary.
 * \param boundary      byte alignment (must be a power of two)
 * \param fill          fill data (if NULL, code_fill or 0 is used)
 * \param maxskip       maximum number of bytes to skip
 * \param code_fill     code fill data (if NULL, 0 is used)
 * \param line          virtual line (from yasm_linemap)
 * \return Newly allocated bytecode.
 * \note The precedence on generated fill is as follows:
 *       - from fill parameter (if not NULL)
 *       - from code_fill parameter (if not NULL)
 *       - 0
 */
/*@only@*/ yasm_bytecode *yasm_bc_create_align
    (/*@keep@*/ yasm_expr *boundary, /*@keep@*/ /*@null@*/ yasm_expr *fill,
     /*@keep@*/ /*@null@*/ yasm_expr *maxskip,
     /*@null@*/ const unsigned char **code_fill, unsigned long line);

/** Create a bytecode that puts the following bytecode at a fixed section
 * offset.
 * \param start         section offset of following bytecode
 * \param line          virtual line (from yasm_linemap)
 * \return Newly allocated bytecode.
 */
/*@only@*/ yasm_bytecode *yasm_bc_create_org
    (unsigned long start, unsigned long line);

/** Create a bytecode that represents a single instruction.
 * \param arch          instruction's architecture
 * \param insn_data     data that identifies the type of instruction
 * \param num_operands  number of operands
 * \param operands      instruction operands (may be NULL if no operands)
 * \param line          virtual line (from yasm_linemap)
 * \return Newly allocated bytecode.
 * \note Keeps the list of operands; do not call yasm_ops_delete() after
 *       giving operands to this function.
 */
/*@only@*/ yasm_bytecode *yasm_bc_create_insn
    (yasm_arch *arch, const uintptr_t insn_data[4], int num_operands,
     /*@null@*/ yasm_insn_operands *operands, unsigned long line);

/** Create a bytecode that represents a single empty (0 length) instruction.
 * This is used for handling solitary prefixes.
 * \param arch          instruction's architecture
 * \param line          virtual line (from yasm_linemap)
 * \return Newly allocated bytecode.
 */
/*@only@*/ yasm_bytecode *yasm_bc_create_empty_insn(yasm_arch *arch,
                                                    unsigned long line);

/** Associate a prefix with an instruction bytecode.
 * \param bc            instruction bytecode
 * \param prefix_data   data the identifies the prefix
 */
void yasm_bc_insn_add_prefix(yasm_bytecode *bc,
                             const uintptr_t prefix_data[4]);

/** Associate a segment prefix with an instruction bytecode.
 * \param bc            instruction bytecode
 * \param segreg        data the identifies the segment register
 */
void yasm_bc_insn_add_seg_prefix(yasm_bytecode *bc, uintptr_t segreg);

/** Get the section that contains a particular bytecode.
 * \param bc    bytecode
 * \return Section containing bc (can be NULL if bytecode is not part of a
 *         section).
 */
/*@dependent@*/ /*@null@*/ yasm_section *yasm_bc_get_section
    (yasm_bytecode *bc);

#ifdef YASM_LIB_INTERNAL
/** Add to the list of symrecs that reference a bytecode.  For symrec use
 * only.
 * \param bc    bytecode
 * \param sym   symbol
 */
void yasm_bc__add_symrec(yasm_bytecode *bc, /*@dependent@*/ yasm_symrec *sym);
#endif
    
/** Delete (free allocated memory for) a bytecode.
 * \param bc    bytecode (only pointer to it); may be NULL
 */
void yasm_bc_destroy(/*@only@*/ /*@null@*/ yasm_bytecode *bc);

/** Print a bytecode.  For debugging purposes.
 * \param f             file
 * \param indent_level  indentation level
 * \param bc            bytecode
 */
void yasm_bc_print(const yasm_bytecode *bc, FILE *f, int indent_level);

/** Finalize a bytecode after parsing.
 * \param bc            bytecode
 * \param prev_bc       bytecode directly preceding bc in a list of bytecodes
 */
void yasm_bc_finalize(yasm_bytecode *bc, yasm_bytecode *prev_bc);

/** Determine the distance between the starting offsets of two bytecodes.
 * \param precbc1       preceding bytecode to the first bytecode
 * \param precbc2       preceding bytecode to the second bytecode
 * \return Distance in bytes between the two bytecodes (bc2-bc1), or NULL if
 *         the distance was indeterminate.
 * \warning Only valid /after/ optimization.
 */
/*@null@*/ /*@only@*/ yasm_intnum *yasm_calc_bc_dist
    (yasm_bytecode *precbc1, yasm_bytecode *precbc2);

/** Get the offset of the next bytecode (the next bytecode doesn't have to
 * actually exist).
 * \param precbc        preceding bytecode
 * \return Offset of the next bytecode in bytes.
 * \warning Only valid /after/ optimization.
 */
unsigned long yasm_bc_next_offset(yasm_bytecode *precbc);

/** Add a dependent span for a bytecode.
 * \param add_span_data add_span_data passed into bc_calc_len()
 * \param bc            bytecode containing span
 * \param id            non-zero identifier for span; may be any non-zero value
 *                      if <0, expand is called for any change;
 *                      if >0, expand is only called when exceeds threshold
 * \param value         dependent value for bytecode expansion
 * \param neg_thres     negative threshold for long/short decision
 * \param pos_thres     positive threshold for long/short decision
 */
typedef void (*yasm_bc_add_span_func)
    (void *add_span_data, yasm_bytecode *bc, int id, const yasm_value *value,
     long neg_thres, long pos_thres);

/** Resolve EQUs in a bytecode and calculate its minimum size.
 * Generates dependent bytecode spans for cases where, if the length spanned
 * increases, it could cause the bytecode size to increase.
 * Any bytecode multiple is NOT included in the length or spans generation;
 * this must be handled at a higher level.
 * \param bc            bytecode
 * \param add_span      function to call to add a span
 * \param add_span_data extra data to be passed to add_span function
 * \return 0 if no error occurred, nonzero if there was an error recognized
 *         (and output) during execution.
 * \note May store to bytecode updated expressions and the short length.
 */
int yasm_bc_calc_len(yasm_bytecode *bc, yasm_bc_add_span_func add_span,
                     void *add_span_data);

/** Recalculate a bytecode's length based on an expanded span length.
 * \param bc            bytecode
 * \param span          span ID (as given to yasm_bc_add_span_func in
 *                      yasm_bc_calc_len)
 * \param old_val       previous span value
 * \param new_val       new span value
 * \param neg_thres     negative threshold for long/short decision (returned)
 * \param pos_thres     postivie threshold for long/short decision (returned)
 * \return 0 if bc no longer dependent on this span's length, negative if
 *         there was an error recognized (and output) during execution, and
 *         positive if bc size may increase for this span further based on the
 *         new negative and positive thresholds returned.
 * \note May store to bytecode updated expressions and the updated length.
 */
int yasm_bc_expand(yasm_bytecode *bc, int span, long old_val, long new_val,
                   /*@out@*/ long *neg_thres, /*@out@*/ long *pos_thres);

/** Convert a bytecode into its byte representation.
 * \param bc            bytecode
 * \param buf           byte representation destination buffer
 * \param bufsize       size of buf (in bytes) prior to call; size of the
 *                      generated data after call
 * \param gap           if nonzero, indicates the data does not really need to
 *                      exist in the object file; if nonzero, contents of buf
 *                      are undefined [output]
 * \param d             data to pass to each call to output_value/output_reloc
 * \param output_value  function to call to convert values into their byte
 *                      representation
 * \param output_reloc  function to call to output relocation entries
 *                      for a single sym
 * \return Newly allocated buffer that should be used instead of buf for
 *         reading the byte representation, or NULL if buf was big enough to
 *         hold the entire byte representation.
 * \note Calling twice on the same bytecode may \em not produce the same
 *       results on the second call, as calling this function may result in
 *       non-reversible changes to the bytecode.
 */
/*@null@*/ /*@only@*/ unsigned char *yasm_bc_tobytes
    (yasm_bytecode *bc, unsigned char *buf, unsigned long *bufsize,
     /*@out@*/ int *gap, void *d, yasm_output_value_func output_value,
     /*@null@*/ yasm_output_reloc_func output_reloc)
    /*@sets *buf@*/;

/** Get the bytecode multiple value as an integer.
 * \param bc            bytecode
 * \param multiple      multiple value (output)
 * \param calc_bc_dist  nonzero if distances between bytecodes should be
 *                      calculated, 0 if error should be returned in this case
 * \return 1 on error (set with yasm_error_set), 0 on success.
 */
int yasm_bc_get_multiple(yasm_bytecode *bc, /*@out@*/ long *multiple,
                         int calc_bc_dist);

/** Create a new data value from an expression.
 * \param expn  expression
 * \return Newly allocated data value.
 */
yasm_dataval *yasm_dv_create_expr(/*@keep@*/ yasm_expr *expn);

/** Create a new data value from a string.
 * \param contents      string (may contain NULs)
 * \param len           length of string
 * \return Newly allocated data value.
 */
yasm_dataval *yasm_dv_create_string(/*@keep@*/ char *contents, size_t len);

/** Create a new data value from raw bytes data.
 * \param contents      raw data (may contain NULs)
 * \param len           length
 * \return Newly allocated data value.
 */
yasm_dataval *yasm_dv_create_raw(/*@keep@*/ unsigned char *contents,
                                 unsigned long len);

#ifndef YASM_DOXYGEN
#define yasm_dv_create_string(s, l) yasm_dv_create_raw((unsigned char *)(s), \
                                                       (unsigned long)(l))
#endif

/** Initialize a list of data values.
 * \param headp list of data values
 */
void yasm_dvs_initialize(yasm_datavalhead *headp);
#ifdef YASM_LIB_INTERNAL
#define yasm_dvs_initialize(headp)      STAILQ_INIT(headp)
#endif

/** Delete (free allocated memory for) a list of data values.
 * \param headp list of data values
 */
void yasm_dvs_delete(yasm_datavalhead *headp);

/** Add data value to the end of a list of data values.
 * \note Does not make a copy of the data value; so don't pass this function
 *       static or local variables, and discard the dv pointer after calling
 *       this function.
 * \param headp         data value list
 * \param dv            data value (may be NULL)
 * \return If data value was actually appended (it wasn't NULL), the data
 *         value; otherwise NULL.
 */
/*@null@*/ yasm_dataval *yasm_dvs_append
    (yasm_datavalhead *headp, /*@returned@*/ /*@null@*/ yasm_dataval *dv);

/** Print a data value list.  For debugging purposes.
 * \param f             file
 * \param indent_level  indentation level
 * \param headp         data value list
 */
void yasm_dvs_print(const yasm_datavalhead *headp, FILE *f, int indent_level);

#endif
