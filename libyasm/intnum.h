/**
 * \file intnum.h
 * \brief YASM integer number interface.
 *
 * $IdPath$
 *
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
 */
#ifndef YASM_INTNUM_H
#define YASM_INTNUM_H

/** Initialize intnum internal data structures. */
void yasm_intnum_initialize(void);

/** Clean up internal intnum allocations. */
void yasm_intnum_cleanup(void);

/** Create a new intnum from a decimal string.
 * \param str	    decimal string
 * \param lindex    line index (where the number came from)
 * \return Newly allocated intnum.
 */
/*@only@*/ yasm_intnum *yasm_intnum_new_dec(char *str, unsigned long lindex);

/** Create a new intnum from a binary string.
 * \param str	    binary string
 * \param lindex    line index (where the number came from)
 * \return Newly allocated intnum.
 */
/*@only@*/ yasm_intnum *yasm_intnum_new_bin(char *str, unsigned long lindex);

/** Create a new intnum from an octal string.
 * \param str	    octal string
 * \param lindex    line index (where the number came from)
 * \return Newly allocated intnum.
 */
/*@only@*/ yasm_intnum *yasm_intnum_new_oct(char *str, unsigned long lindex);

/** Create a new intnum from a hexidecimal string.
 * \param str	    hexidecimal string
 * \param lindex    line index (where the number came from)
 * \return Newly allocated intnum.
 */
/*@only@*/ yasm_intnum *yasm_intnum_new_hex(char *str, unsigned long lindex);

/** Convert character constant to integer value, using NASM rules.  NASM syntax
 * supports automatic conversion from strings such as 'abcd' to a 32-bit
 * integer value.  This function performs those conversions.
 * \param str	    character constant string
 * \param lindex    line index (where the number came from)
 * \return Newly allocated intnum.
 */
/*@only@*/ yasm_intnum *yasm_intnum_new_charconst_nasm(const char *str,
						       unsigned long lindex);

/** Create a new intnum from an unsigned integer value.
 * \param i	    unsigned integer value
 * \return Newly allocated intnum.
 */
/*@only@*/ yasm_intnum *yasm_intnum_new_uint(unsigned long i);

/** Create a new intnum from an signed integer value.
 * \param i	    signed integer value
 * \return Newly allocated intnum.
 */
/*@only@*/ yasm_intnum *yasm_intnum_new_int(long i);

/** Duplicate an intnum.
 * \param intn	intnum
 * \return Newly allocated intnum with the same value as intn.
 */
/*@only@*/ yasm_intnum *yasm_intnum_copy(const yasm_intnum *intn);

/** Destroy (free allocated memory for) an intnum.
 * \param intn	intnum
 */
void yasm_intnum_delete(/*@only@*/ yasm_intnum *intn);

/** Floating point calculation function: acc = acc op operand.
 * \note Not all operations in yasm_expr_op may be supported; unsupported
 *       operations will result in an error.
 * \param acc	    intnum accumulator
 * \param op	    operation
 * \param operand   intnum operand
 * \param lindex    line index (of expression)
 */
void yasm_intnum_calc(yasm_intnum *acc, yasm_expr_op op, yasm_intnum *operand,
		      unsigned long lindex);

/** Simple value check for 0.
 * \param acc	    intnum
 * \return Nonzero if acc==0.
 */
int yasm_intnum_is_zero(yasm_intnum *acc);

/** Simple value check for 1.
 * \param acc	    intnum
 * \return Nonzero if acc==1.
 */
int yasm_intnum_is_pos1(yasm_intnum *acc);

/** Simple value check for -1.
 * \param acc	    intnum
 * \return Nonzero if acc==-1.
 */
int yasm_intnum_is_neg1(yasm_intnum *acc);

/** Convert an intnum to an unsigned 32-bit value.  The value is in "standard"
 * C format (eg, of unknown endian).
 * \note Parameter intnum is truncated to fit into 32 bits.  Use
 *       intnum_check_size() to check for overflow.
 * \param intn	intnum
 * \return Unsigned 32-bit value of intn.
 */
unsigned long yasm_intnum_get_uint(const yasm_intnum *intn);

/** Convert an intnum to a signed 32-bit value.  The value is in "standard" C
 * format (eg, of unknown endian).
 * \note Parameter intnum is truncated to fit into 32 bits.  Use
 *       intnum_check_size() to check for overflow.
 * \param intn	intnum
 * \return Signed 32-bit value of intn.
 */
long yasm_intnum_get_int(const yasm_intnum *intn);

/** Convert an intnum to Intel-format little-endian byte string.
 * [0] should be the first byte output to an Intel-format file.
 * \note Parameter intnum is truncated to fit into specified size.  Use
 *       intnum_check_size() to check for overflow.
 * \param intn	    intnum
 * \param ptr	    pointer to storage for size bytes of output
 * \param size	    size (in bytes) of desired output.
 */
void yasm_intnum_get_sized(const yasm_intnum *intn, unsigned char *ptr,
			   size_t size);

/** Check to see if intnum will fit without overflow into size bytes.
 * If is_signed is 1, intnum is treated as a signed number.
 * \param intn	    intnum
 * \param size	    number of bytes of output space
 * \param is_signed nonzero, intnum should be treated as signed
 * \return Nonzero if intnum will fit.
 */
int yasm_intnum_check_size(const yasm_intnum *intn, size_t size,
			   int is_signed);

/** Print an intnum.  For debugging purposes.
 * \param f	file
 * \param intn	intnum
 */
void yasm_intnum_print(FILE *f, const yasm_intnum *intn);

#endif
