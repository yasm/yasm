/*
 * Integer number functions.
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
#define YASM_LIB_INTERNAL
#include "util.h"
/*@unused@*/ RCSID("$Id$");

#include <ctype.h>
#include <limits.h>

#include "coretype.h"
#include "bitvect.h"
#include "file.h"

#include "errwarn.h"
#include "intnum.h"


/* "Native" "word" size for intnum calculations. */
#define BITVECT_NATIVE_SIZE	128

struct yasm_intnum {
    union val {
	unsigned long ul;	/* integer value (for integers <=32 bits) */
	wordptr bv;		/* bit vector (for integers >32 bits) */
    } val;
    enum { INTNUM_UL, INTNUM_BV } type;
    unsigned char origsize;	/* original (parsed) size, in bits */
};

/* static bitvect used for conversions */
static /*@only@*/ wordptr conv_bv;

/* static bitvects used for computation */
static /*@only@*/ wordptr result, spare, op1static, op2static;

static /*@only@*/ BitVector_from_Dec_static_data *from_dec_data;


void
yasm_intnum_initialize(void)
{
    conv_bv = BitVector_Create(BITVECT_NATIVE_SIZE, FALSE);
    result = BitVector_Create(BITVECT_NATIVE_SIZE, FALSE);
    spare = BitVector_Create(BITVECT_NATIVE_SIZE, FALSE);
    op1static = BitVector_Create(BITVECT_NATIVE_SIZE, FALSE);
    op2static = BitVector_Create(BITVECT_NATIVE_SIZE, FALSE);
    from_dec_data = BitVector_from_Dec_static_Boot(BITVECT_NATIVE_SIZE);
}

void
yasm_intnum_cleanup(void)
{
    BitVector_from_Dec_static_Shutdown(from_dec_data);
    BitVector_Destroy(op2static);
    BitVector_Destroy(op1static);
    BitVector_Destroy(spare);
    BitVector_Destroy(result);
    BitVector_Destroy(conv_bv);
}

yasm_intnum *
yasm_intnum_create_dec(char *str, unsigned long line)
{
    yasm_intnum *intn = yasm_xmalloc(sizeof(yasm_intnum));

    intn->origsize = 0;	    /* no reliable way to figure this out */

    if (BitVector_from_Dec_static(from_dec_data, conv_bv,
				  (unsigned char *)str) == ErrCode_Ovfl)
	yasm__warning(YASM_WARN_GENERAL, line,
		      N_("Numeric constant too large for internal format"));
    if (Set_Max(conv_bv) < 32) {
	intn->type = INTNUM_UL;
	intn->val.ul = BitVector_Chunk_Read(conv_bv, 32, 0);
    } else {
	intn->type = INTNUM_BV;
	intn->val.bv = BitVector_Clone(conv_bv);
    }

    return intn;
}

yasm_intnum *
yasm_intnum_create_bin(char *str, unsigned long line)
{
    yasm_intnum *intn = yasm_xmalloc(sizeof(yasm_intnum));

    intn->origsize = (unsigned char)strlen(str);

    if(intn->origsize > BITVECT_NATIVE_SIZE)
	yasm__warning(YASM_WARN_GENERAL, line,
		      N_("Numeric constant too large for internal format"));

    BitVector_from_Bin(conv_bv, (unsigned char *)str);
    if (Set_Max(conv_bv) < 32) {
	intn->type = INTNUM_UL;
	intn->val.ul = BitVector_Chunk_Read(conv_bv, 32, 0);
    } else {
	intn->type = INTNUM_BV;
	intn->val.bv = BitVector_Clone(conv_bv);
    }

    return intn;
}

yasm_intnum *
yasm_intnum_create_oct(char *str, unsigned long line)
{
    yasm_intnum *intn = yasm_xmalloc(sizeof(yasm_intnum));

    intn->origsize = strlen(str)*3;

    if(intn->origsize > BITVECT_NATIVE_SIZE)
	yasm__warning(YASM_WARN_GENERAL, line,
		      N_("Numeric constant too large for internal format"));

    BitVector_from_Oct(conv_bv, (unsigned char *)str);
    if (Set_Max(conv_bv) < 32) {
	intn->type = INTNUM_UL;
	intn->val.ul = BitVector_Chunk_Read(conv_bv, 32, 0);
    } else {
	intn->type = INTNUM_BV;
	intn->val.bv = BitVector_Clone(conv_bv);
    }

    return intn;
}

yasm_intnum *
yasm_intnum_create_hex(char *str, unsigned long line)
{
    yasm_intnum *intn = yasm_xmalloc(sizeof(yasm_intnum));

    intn->origsize = strlen(str)*4;

    if(intn->origsize > BITVECT_NATIVE_SIZE)
	yasm__warning(YASM_WARN_GENERAL, line,
		      N_("Numeric constant too large for internal format"));

    BitVector_from_Hex(conv_bv, (unsigned char *)str);
    if (Set_Max(conv_bv) < 32) {
	intn->type = INTNUM_UL;
	intn->val.ul = BitVector_Chunk_Read(conv_bv, 32, 0);
    } else {
	intn->type = INTNUM_BV;
	intn->val.bv = BitVector_Clone(conv_bv);
    }

    return intn;
}

/*@-usedef -compdef -uniondef@*/
yasm_intnum *
yasm_intnum_create_charconst_nasm(const char *str, unsigned long line)
{
    yasm_intnum *intn = yasm_xmalloc(sizeof(yasm_intnum));
    size_t len = strlen(str);

    intn->origsize = len*8;

    if(intn->origsize > BITVECT_NATIVE_SIZE)
	yasm__warning(YASM_WARN_GENERAL, line,
		      N_("Character constant too large for internal format"));

    if (len > 4) {
	BitVector_Empty(conv_bv);
	intn->type = INTNUM_BV;
    } else {
	intn->val.ul = 0;
	intn->type = INTNUM_UL;
    }

    switch (len) {
	case 4:
	    intn->val.ul |= (unsigned long)str[3];
	    intn->val.ul <<= 8;
	    /*@fallthrough@*/
	case 3:
	    intn->val.ul |= (unsigned long)str[2];
	    intn->val.ul <<= 8;
	    /*@fallthrough@*/
	case 2:
	    intn->val.ul |= (unsigned long)str[1];
	    intn->val.ul <<= 8;
	    /*@fallthrough@*/
	case 1:
	    intn->val.ul |= (unsigned long)str[0];
	case 0:
	    break;
	default:
	    /* >32 bit conversion */
	    while (len) {
		BitVector_Move_Left(conv_bv, 8);
		BitVector_Chunk_Store(conv_bv, 8, 0,
				      (unsigned long)str[--len]);
	    }
	    intn->val.bv = BitVector_Clone(conv_bv);
    }

    return intn;
}
/*@=usedef =compdef =uniondef@*/

yasm_intnum *
yasm_intnum_create_uint(unsigned long i)
{
    yasm_intnum *intn = yasm_xmalloc(sizeof(yasm_intnum));

    intn->val.ul = i;
    intn->type = INTNUM_UL;
    intn->origsize = 0;

    return intn;
}

yasm_intnum *
yasm_intnum_create_int(long i)
{
    yasm_intnum *intn;

    /* positive numbers can go through the uint() function */
    if (i >= 0)
	return yasm_intnum_create_uint((unsigned long)i);

    BitVector_Empty(conv_bv);
    BitVector_Chunk_Store(conv_bv, 32, 0, (unsigned long)(-i));
    BitVector_Negate(conv_bv, conv_bv);

    intn = yasm_xmalloc(sizeof(yasm_intnum));
    intn->val.bv = BitVector_Clone(conv_bv);
    intn->type = INTNUM_BV;
    intn->origsize = 0;

    return intn;
}

yasm_intnum *
yasm_intnum_copy(const yasm_intnum *intn)
{
    yasm_intnum *n = yasm_xmalloc(sizeof(yasm_intnum));

    switch (intn->type) {
	case INTNUM_UL:
	    n->val.ul = intn->val.ul;
	    break;
	case INTNUM_BV:
	    n->val.bv = BitVector_Clone(intn->val.bv);
	    break;
    }
    n->type = intn->type;
    n->origsize = intn->origsize;

    return n;
}

void
yasm_intnum_destroy(yasm_intnum *intn)
{
    if (intn->type == INTNUM_BV)
	BitVector_Destroy(intn->val.bv);
    yasm_xfree(intn);
}

/*@-nullderef -nullpass -branchstate@*/
void
yasm_intnum_calc(yasm_intnum *acc, yasm_expr_op op, yasm_intnum *operand,
		 unsigned long line)
{
    boolean carry = 0;
    wordptr op1, op2 = NULL;

    /* Always do computations with in full bit vector.
     * Bit vector results must be calculated through intermediate storage.
     */
    if (acc->type == INTNUM_BV)
	op1 = acc->val.bv;
    else {
	op1 = op1static;
	BitVector_Empty(op1);
	BitVector_Chunk_Store(op1, 32, 0, acc->val.ul);
    }

    if (operand) {
	if (operand->type == INTNUM_BV)
	    op2 = operand->val.bv;
	else {
	    op2 = op2static;
	    BitVector_Empty(op2);
	    BitVector_Chunk_Store(op2, 32, 0, operand->val.ul);
	}
    }

    if (!operand && op != YASM_EXPR_NEG && op != YASM_EXPR_NOT &&
	op != YASM_EXPR_LNOT)
	yasm_internal_error(N_("Operation needs an operand"));

    /* A operation does a bitvector computation if result is allocated. */
    switch (op) {
	case YASM_EXPR_ADD:
	    BitVector_add(result, op1, op2, &carry);
	    break;
	case YASM_EXPR_SUB:
	    BitVector_sub(result, op1, op2, &carry);
	    break;
	case YASM_EXPR_MUL:
	    BitVector_Multiply(result, op1, op2);
	    break;
	case YASM_EXPR_DIV:
	    /* TODO: make sure op1 and op2 are unsigned */
	    BitVector_Divide(result, op1, op2, spare);
	    break;
	case YASM_EXPR_SIGNDIV:
	    BitVector_Divide(result, op1, op2, spare);
	    break;
	case YASM_EXPR_MOD:
	    /* TODO: make sure op1 and op2 are unsigned */
	    BitVector_Divide(spare, op1, op2, result);
	    break;
	case YASM_EXPR_SIGNMOD:
	    BitVector_Divide(spare, op1, op2, result);
	    break;
	case YASM_EXPR_NEG:
	    BitVector_Negate(result, op1);
	    break;
	case YASM_EXPR_NOT:
	    Set_Complement(result, op1);
	    break;
	case YASM_EXPR_OR:
	    Set_Union(result, op1, op2);
	    break;
	case YASM_EXPR_AND:
	    Set_Intersection(result, op1, op2);
	    break;
	case YASM_EXPR_XOR:
	    Set_ExclusiveOr(result, op1, op2);
	    break;
	case YASM_EXPR_NOR:
	    Set_Union(result, op1, op2);
	    Set_Complement(result, result);
	    break;
	case YASM_EXPR_SHL:
	    if (operand->type == INTNUM_UL) {
		BitVector_Copy(result, op1);
		BitVector_Move_Left(result, (N_int)operand->val.ul);
	    } else	/* don't even bother, just zero result */
		BitVector_Empty(result);
	    break;
	case YASM_EXPR_SHR:
	    if (operand->type == INTNUM_UL) {
		BitVector_Copy(result, op1);
		BitVector_Move_Right(result, (N_int)operand->val.ul);
	    } else	/* don't even bother, just zero result */
		BitVector_Empty(result);
	    break;
	case YASM_EXPR_LOR:
	    BitVector_Empty(result);
	    BitVector_LSB(result, !BitVector_is_empty(op1) ||
			  !BitVector_is_empty(op2));
	    break;
	case YASM_EXPR_LAND:
	    BitVector_Empty(result);
	    BitVector_LSB(result, !BitVector_is_empty(op1) &&
			  !BitVector_is_empty(op2));
	    break;
	case YASM_EXPR_LNOT:
	    BitVector_Empty(result);
	    BitVector_LSB(result, BitVector_is_empty(op1));
	    break;
	case YASM_EXPR_EQ:
	    BitVector_Empty(result);
	    BitVector_LSB(result, BitVector_equal(op1, op2));
	    break;
	case YASM_EXPR_LT:
	    BitVector_Empty(result);
	    BitVector_LSB(result, BitVector_Lexicompare(op1, op2) < 0);
	    break;
	case YASM_EXPR_GT:
	    BitVector_Empty(result);
	    BitVector_LSB(result, BitVector_Lexicompare(op1, op2) > 0);
	    break;
	case YASM_EXPR_LE:
	    BitVector_Empty(result);
	    BitVector_LSB(result, BitVector_Lexicompare(op1, op2) <= 0);
	    break;
	case YASM_EXPR_GE:
	    BitVector_Empty(result);
	    BitVector_LSB(result, BitVector_Lexicompare(op1, op2) >= 0);
	    break;
	case YASM_EXPR_NE:
	    BitVector_Empty(result);
	    BitVector_LSB(result, !BitVector_equal(op1, op2));
	    break;
	case YASM_EXPR_SEG:
	    yasm__error(line, N_("invalid use of '%s'"), "SEG");
	    break;
	case YASM_EXPR_WRT:
	    yasm__error(line, N_("invalid use of '%s'"), "WRT");
	    break;
	case YASM_EXPR_SEGOFF:
	    yasm__error(line, N_("invalid use of '%s'"), ":");
	    break;
	case YASM_EXPR_IDENT:
	    if (result)
		BitVector_Copy(result, op1);
	    break;
	default:
	    yasm_internal_error(N_("invalid operation in intnum calculation"));
    }

    /* Try to fit the result into 32 bits if possible */
    if (Set_Max(result) < 32) {
	if (acc->type == INTNUM_BV) {
	    BitVector_Destroy(acc->val.bv);
	    acc->type = INTNUM_UL;
	}
	acc->val.ul = BitVector_Chunk_Read(result, 32, 0);
    } else {
	if (acc->type == INTNUM_BV) {
	    BitVector_Copy(acc->val.bv, result);
	} else {
	    acc->type = INTNUM_BV;
	    acc->val.bv = BitVector_Clone(result);
	}
    }
}
/*@=nullderef =nullpass =branchstate@*/

void
yasm_intnum_zero(yasm_intnum *intn)
{
    yasm_intnum_set_uint(intn, 0);
}

void
yasm_intnum_set_uint(yasm_intnum *intn, unsigned long val)
{
    if (intn->type == INTNUM_BV) {
	BitVector_Destroy(intn->val.bv);
	intn->type = INTNUM_UL;
    }
    intn->val.ul = val;
}

int
yasm_intnum_is_zero(const yasm_intnum *intn)
{
    return (intn->type == INTNUM_UL && intn->val.ul == 0);
}

int
yasm_intnum_is_pos1(const yasm_intnum *intn)
{
    return (intn->type == INTNUM_UL && intn->val.ul == 1);
}

int
yasm_intnum_is_neg1(const yasm_intnum *intn)
{
    return (intn->type == INTNUM_BV && BitVector_is_full(intn->val.bv));
}

int
yasm_intnum_sign(const yasm_intnum *intn)
{
    if (intn->type == INTNUM_UL) {
	if (intn->val.ul == 0)
	    return 0;
	else
	    return 1;
    } else
	return BitVector_Sign(intn->val.bv);
}

unsigned long
yasm_intnum_get_uint(const yasm_intnum *intn)
{
    switch (intn->type) {
	case INTNUM_UL:
	    return intn->val.ul;
	case INTNUM_BV:
	    return BitVector_Chunk_Read(intn->val.bv, 32, 0);
	default:
	    yasm_internal_error(N_("unknown intnum type"));
	    /*@notreached@*/
	    return 0;
    }
}

long
yasm_intnum_get_int(const yasm_intnum *intn)
{
    switch (intn->type) {
	case INTNUM_UL:
	    /* unsigned long values are always positive; max out if needed */
	    return (intn->val.ul & 0x80000000) ? LONG_MAX : (long)intn->val.ul;
	case INTNUM_BV:
	    if (BitVector_msb_(intn->val.bv)) {
		/* it's negative: negate the bitvector to get a positive
		 * number, then negate the positive number.
		 */
		unsigned long ul;

		BitVector_Negate(conv_bv, intn->val.bv);
		if (Set_Max(conv_bv) >= 32) {
		    /* too negative */
		    return LONG_MIN;
		}
		ul = BitVector_Chunk_Read(conv_bv, 32, 0);
		/* check for too negative */
		return (ul & 0x80000000) ? LONG_MIN : -((long)ul);
	    }

	    /* it's positive, and since it's a BV, it must be >0x7FFFFFFF */
	    return LONG_MAX;
	default:
	    yasm_internal_error(N_("unknown intnum type"));
	    /*@notreached@*/
	    return 0;
    }
}

void
yasm_intnum_get_sized(const yasm_intnum *intn, unsigned char *ptr,
		      size_t destsize, size_t valsize, int shift,
		      int bigendian, int warn, unsigned long line)
{
    wordptr op1 = op1static, op2;
    unsigned char *buf;
    unsigned int len;
    size_t rshift = shift < 0 ? (size_t)(-shift) : 0;
    int carry_in;

    /* Currently don't support destinations larger than our native size */
    if (destsize*8 > BITVECT_NATIVE_SIZE)
	yasm_internal_error(N_("destination too large"));

    /* General size warnings */
    if (warn && !yasm_intnum_check_size(intn, valsize, rshift, 2))
	yasm__warning(YASM_WARN_GENERAL, line,
		      N_("value does not fit in %d bit field"), valsize);

    /* Read the original data into a bitvect */
    if (bigendian) {
	/* TODO */
	yasm_internal_error(N_("big endian not implemented"));
    } else
	BitVector_Block_Store(op1, ptr, destsize);

    /* If not already a bitvect, convert value to be written to a bitvect */
    if (intn->type == INTNUM_BV)
	op2 = intn->val.bv;
    else {
	op2 = op2static;
	BitVector_Empty(op2);
	BitVector_Chunk_Store(op2, 32, 0, intn->val.ul);
    }

    /* Check low bits if right shifting and warnings enabled */
    if (warn && rshift > 0) {
	BitVector_Copy(conv_bv, op2);
	BitVector_Move_Left(conv_bv, BITVECT_NATIVE_SIZE-rshift);
	if (!BitVector_is_empty(conv_bv))
	    yasm__warning(YASM_WARN_GENERAL, line,
			  N_("misaligned value, truncating to boundary"));
    }

    /* Shift right if needed */
    if (rshift > 0) {
	carry_in = BitVector_msb_(op2);
	while (rshift-- > 0)
	    BitVector_shift_right(op2, carry_in);
	shift = 0;
    }

    /* Write the new value into the destination bitvect */
    BitVector_Interval_Copy(op1, op2, (unsigned int)shift, 0, valsize);

    /* Write out the new data */
    buf = BitVector_Block_Read(op1, &len);
    if (bigendian) {
	/* TODO */
	yasm_internal_error(N_("big endian not implemented"));
    } else
	memcpy(ptr, buf, destsize);
    yasm_xfree(buf);
}

/* Return 1 if okay size, 0 if not */
int
yasm_intnum_check_size(const yasm_intnum *intn, size_t size, size_t rshift,
		       int rangetype)
{
    wordptr val;

    /* If not already a bitvect, convert value to a bitvect */
    if (intn->type == INTNUM_BV) {
	if (rshift > 0) {
	    val = conv_bv;
	    BitVector_Copy(val, intn->val.bv);
	} else
	    val = intn->val.bv;
    } else {
	val = conv_bv;
	BitVector_Empty(val);
	BitVector_Chunk_Store(val, 32, 0, intn->val.ul);
    }

    if (size >= BITVECT_NATIVE_SIZE)
	return 1;

    if (rshift > 0) {
	int carry_in = BitVector_msb_(val);
	while (rshift-- > 0)
	    BitVector_shift_right(val, carry_in);
    }

    if (rangetype > 0) {
	if (BitVector_msb_(val)) {
	    /* it's negative */
	    int retval;

	    BitVector_Negate(conv_bv, val);
	    BitVector_dec(conv_bv, conv_bv);
	    retval = Set_Max(conv_bv) < (long)size-1;

	    return retval;
	}
	
	if (rangetype == 1)
	    size--;
    }
    return (Set_Max(val) < (long)size);
}

unsigned long
yasm_intnum_get_leb128(const yasm_intnum *intn, unsigned char *ptr, int sign)
{
    wordptr val = op1static;
    unsigned long i, size;
    unsigned char *ptr_orig = ptr;

    /* Shortcut 0 */
    if (intn->type == INTNUM_UL && intn->val.ul == 0) {
	*ptr = 0;
	return 1;
    }

    /* If not already a bitvect, convert value to be written to a bitvect */
    if (intn->type == INTNUM_BV)
	val = intn->val.bv;
    else {
	BitVector_Empty(val);
	BitVector_Chunk_Store(val, 32, 0, intn->val.ul);
    }

    if (sign) {
	/* Signed mode */
	if (BitVector_msb_(val)) {
	    /* Negative */
	    BitVector_Negate(conv_bv, val);
	    size = Set_Max(conv_bv)+2;
	} else {
	    /* Positive */
	    size = Set_Max(val)+2;
	}
    } else {
	/* Unsigned mode */
	size = Set_Max(val)+1;
    }

    /* Positive/Unsigned write */
    for (i=0; i<size; i += 7) {
	*ptr = (unsigned char)BitVector_Chunk_Read(val, 7, i);
	*ptr |= 0x80;
	ptr++;
    }
    *(ptr-1) &= 0x7F;	/* Clear MSB of last byte */
    return (unsigned long)(ptr-ptr_orig);
}

unsigned long
yasm_intnum_size_leb128(const yasm_intnum *intn, int sign)
{
    wordptr val = op1static;

    /* Shortcut 0 */
    if (intn->type == INTNUM_UL && intn->val.ul == 0) {
	return 1;
    }

    /* If not already a bitvect, convert value to a bitvect */
    if (intn->type == INTNUM_BV)
	val = intn->val.bv;
    else {
	BitVector_Empty(val);
	BitVector_Chunk_Store(val, 32, 0, intn->val.ul);
    }

    if (sign) {
	/* Signed mode */
	if (BitVector_msb_(val)) {
	    /* Negative */
	    BitVector_Negate(conv_bv, val);
	    return (Set_Max(conv_bv)+8)/7;
	} else {
	    /* Positive */
	    return (Set_Max(val)+8)/7;
	}
    } else {
	/* Unsigned mode */
	return (Set_Max(val)+7)/7;
    }
}

void
yasm_intnum_print(const yasm_intnum *intn, FILE *f)
{
    unsigned char *s;

    switch (intn->type) {
	case INTNUM_UL:
	    fprintf(f, "0x%lx/%u", intn->val.ul, (unsigned int)intn->origsize);
	    break;
	case INTNUM_BV:
	    s = BitVector_to_Hex(intn->val.bv);
	    fprintf(f, "0x%s/%u", (char *)s, (unsigned int)intn->origsize);
	    yasm_xfree(s);
	    break;
    }
}
