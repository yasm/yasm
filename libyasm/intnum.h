/**
 * \file libyasm/intnum.h
 * \brief YASM integer number interface.
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
#ifndef YASM_INTNUM_H
#define YASM_INTNUM_H

/** Initialize intnum internal data structures. */
void yasm_intnum_initialize(void);

/** Clean up internal intnum allocations. */
void yasm_intnum_cleanup(void);

/** Create a new intnum from a decimal string.
 * \param str	    decimal string
 * \param line	    virtual line (where the number came from)
 * \return Newly allocated intnum.
 */
/*@only@*/ yasm_intnum *yasm_intnum_create_dec(char *str, unsigned long line);

/** Create a new intnum from a binary string.
 * \param str	    binary string
 * \param line	    virtual line (where the number came from)
 * \return Newly allocated intnum.
 */
/*@only@*/ yasm_intnum *yasm_intnum_create_bin(char *str, unsigned long line);

/** Create a new intnum from an octal string.
 * \param str	    octal string
 * \param line	    virtual line (where the number came from)
 * \return Newly allocated intnum.
 */
/*@only@*/ yasm_intnum *yasm_intnum_create_oct(char *str, unsigned long line);

/** Create a new intnum from a hexidecimal string.
 * \param str	    hexidecimal string
 * \param line	    virtual line (where the number came from)
 * \return Newly allocated intnum.
 */
/*@only@*/ yasm_intnum *yasm_intnum_create_hex(char *str, unsigned long line);

/** Convert character constant to integer value, using NASM rules.  NASM syntax
 * supports automatic conversion from strings such as 'abcd' to a 32-bit
 * integer value.  This function performs those conversions.
 * \param str	    character constant string
 * \param line      virtual line (where the number came from)
 * \return Newly allocated intnum.
 */
/*@only@*/ yasm_intnum *yasm_intnum_create_charconst_nasm(const char *str,
							  unsigned long line);

/** Create a new intnum from an unsigned integer value.
 * \param i	    unsigned integer value
 * \return Newly allocated intnum.
 */
/*@only@*/ yasm_intnum *yasm_intnum_create_uint(unsigned long i);

/** Create a new intnum from an signed integer value.
 * \param i	    signed integer value
 * \return Newly allocated intnum.
 */
/*@only@*/ yasm_intnum *yasm_intnum_create_int(long i);

/** Duplicate an intnum.
 * \param intn	intnum
 * \return Newly allocated intnum with the same value as intn.
 */
/*@only@*/ yasm_intnum *yasm_intnum_copy(const yasm_intnum *intn);

/** Destroy (free allocated memory for) an intnum.
 * \param intn	intnum
 */
void yasm_intnum_destroy(/*@only@*/ yasm_intnum *intn);

/** Floating point calculation function: acc = acc op operand.
 * \note Not all operations in yasm_expr_op may be supported; unsupported
 *       operations will result in an error.
 * \param acc	    intnum accumulator
 * \param op	    operation
 * \param operand   intnum operand
 * \param line      virtual line (of expression)
 */
void yasm_intnum_calc(yasm_intnum *acc, yasm_expr_op op, yasm_intnum *operand,
		      unsigned long line);

/** Zero an intnum.
 * \param intn	    intnum
 */
void yasm_intnum_zero(yasm_intnum *intn);

/** Set an intnum to an unsigned integer.
 * \param intn	    intnum
 * \param val	    integer value
 */
void yasm_intnum_set_uint(yasm_intnum *intn, unsigned long val);

/** Simple value check for 0.
 * \param acc	    intnum
 * \return Nonzero if acc==0.
 */
int yasm_intnum_is_zero(const yasm_intnum *acc);

/** Simple value check for 1.
 * \param acc	    intnum
 * \return Nonzero if acc==1.
 */
int yasm_intnum_is_pos1(const yasm_intnum *acc);

/** Simple value check for -1.
 * \param acc	    intnum
 * \return Nonzero if acc==-1.
 */
int yasm_intnum_is_neg1(const yasm_intnum *acc);

/** Simple sign check.
 * \param acc	    intnum
 * \return -1 if negative, 0 if zero, +1 if positive
 */
int yasm_intnum_sign(const yasm_intnum *acc);

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

/** Output #yasm_intnum to buffer in little-endian or big-endian.  Puts the
 * value into the least significant bits of the destination, or may be shifted
 * into more significant bits by the shift parameter.  The destination bits are
 * cleared before being set.  [0] should be the first byte output to the file.
 * \param intn	    intnum
 * \param ptr	    pointer to storage for size bytes of output
 * \param destsize  destination size (in bytes)
 * \param valsize   size (in bits)
 * \param shift	    left shift (in bits); may be negative to specify right
 *		    shift (standard warnings include truncation to boundary)
 * \param bigendian endianness (nonzero=big, zero=little)
 * \param warn	    enables standard warnings (value doesn't fit into valsize
 *		    bits)
 * \param line      virtual line; may be 0 if warn is 0
 */
void yasm_intnum_get_sized(const yasm_intnum *intn, unsigned char *ptr,
			   size_t destsize, size_t valsize, int shift,
			   int bigendian, int warn, unsigned long line);

/** Check to see if intnum will fit without overflow into size bits.
 * \param intn	    intnum
 * \param size	    number of bits of output space
 * \param rshift    right shift
 * \param rangetype signed/unsigned range selection:
 *		    0 => (0, unsigned max);
 *		    1 => (signed min, signed max);
 *		    2 => (signed min, unsigned max)
 * \return Nonzero if intnum will fit.
 */
int yasm_intnum_check_size(const yasm_intnum *intn, size_t size,
			   size_t rshift, int rangetype);

/** Output #yasm_intnum to buffer in LEB128-encoded form.
 * \param intn	    intnum
 * \param ptr	    pointer to storage for output bytes
 * \param sign	    signedness of LEB128 encoding (0=unsigned, 1=signed)
 * \return Number of bytes generated.
 */
unsigned long yasm_intnum_get_leb128(const yasm_intnum *intn,
				     unsigned char *ptr, int sign);

/** Calculate number of bytes LEB128-encoded form of #yasm_intnum will take.
 * \param intn	    intnum
 * \param sign	    signedness of LEB128 encoding (0=unsigned, 1=signed)
 * \return Number of bytes.
 */
unsigned long yasm_intnum_size_leb128(const yasm_intnum *intn, int sign);

/** Output integer to buffer in signed LEB128-encoded form.
 * \param v	    integer
 * \param ptr	    pointer to storage for output bytes
 * \return Number of bytes generated.
 */
unsigned long yasm_get_sleb128(long v, unsigned char *ptr);

/** Calculate number of bytes signed LEB128-encoded form of integer will take.
 * \param v	    integer
 * \return Number of bytes.
 */
unsigned long yasm_size_sleb128(long v);

/** Output integer to buffer in unsigned LEB128-encoded form.
 * \param v	    integer
 * \param ptr	    pointer to storage for output bytes
 * \return Number of bytes generated.
 */
unsigned long yasm_get_uleb128(unsigned long v, unsigned char *ptr);

/** Calculate number of bytes unsigned LEB128-encoded form of integer will take.
 * \param v	    integer
 * \return Number of bytes.
 */
unsigned long yasm_size_uleb128(unsigned long v);

/** Print an intnum.  For debugging purposes.
 * \param f	file
 * \param intn	intnum
 */
void yasm_intnum_print(const yasm_intnum *intn, FILE *f);

#endif
