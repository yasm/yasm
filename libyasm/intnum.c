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
/*@unused@*/ RCSID("$IdPath$");

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


void
yasm_intnum_initialize(void)
{
    conv_bv = BitVector_Create(BITVECT_NATIVE_SIZE, FALSE);
    result = BitVector_Create(BITVECT_NATIVE_SIZE, FALSE);
    spare = BitVector_Create(BITVECT_NATIVE_SIZE, FALSE);
    op1static = BitVector_Create(BITVECT_NATIVE_SIZE, FALSE);
    op2static = BitVector_Create(BITVECT_NATIVE_SIZE, FALSE);
    BitVector_from_Dec_static_Boot(BITVECT_NATIVE_SIZE);
}

void
yasm_intnum_cleanup(void)
{
    BitVector_from_Dec_static_Shutdown();
    BitVector_Destroy(op2static);
    BitVector_Destroy(op1static);
    BitVector_Destroy(spare);
    BitVector_Destroy(result);
    BitVector_Destroy(conv_bv);
}

yasm_intnum *
yasm_intnum_new_dec(char *str, unsigned long lindex)
{
    yasm_intnum *intn = yasm_xmalloc(sizeof(yasm_intnum));

    intn->origsize = 0;	    /* no reliable way to figure this out */

    if (BitVector_from_Dec_static(conv_bv,
				  (unsigned char *)str) == ErrCode_Ovfl)
	yasm__warning(YASM_WARN_GENERAL, lindex,
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
yasm_intnum_new_bin(char *str, unsigned long lindex)
{
    yasm_intnum *intn = yasm_xmalloc(sizeof(yasm_intnum));

    intn->origsize = (unsigned char)strlen(str);

    if(intn->origsize > BITVECT_NATIVE_SIZE)
	yasm__warning(YASM_WARN_GENERAL, lindex,
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
yasm_intnum_new_oct(char *str, unsigned long lindex)
{
    yasm_intnum *intn = yasm_xmalloc(sizeof(yasm_intnum));

    intn->origsize = strlen(str)*3;

    if(intn->origsize > BITVECT_NATIVE_SIZE)
	yasm__warning(YASM_WARN_GENERAL, lindex,
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
yasm_intnum_new_hex(char *str, unsigned long lindex)
{
    yasm_intnum *intn = yasm_xmalloc(sizeof(yasm_intnum));

    intn->origsize = strlen(str)*4;

    if(intn->origsize > BITVECT_NATIVE_SIZE)
	yasm__warning(YASM_WARN_GENERAL, lindex,
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
yasm_intnum_new_charconst_nasm(const char *str, unsigned long lindex)
{
    yasm_intnum *intn = yasm_xmalloc(sizeof(yasm_intnum));
    size_t len = strlen(str);

    intn->origsize = len*8;

    if(intn->origsize > BITVECT_NATIVE_SIZE)
	yasm__warning(YASM_WARN_GENERAL, lindex,
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
		BitVector_Chunk_Store(conv_bv, 8, 0,
				      (unsigned long)str[--len]);
		BitVector_Move_Left(conv_bv, 8);
	    }
	    intn->val.bv = BitVector_Clone(conv_bv);
    }

    return intn;
}
/*@=usedef =compdef =uniondef@*/

yasm_intnum *
yasm_intnum_new_uint(unsigned long i)
{
    yasm_intnum *intn = yasm_xmalloc(sizeof(yasm_intnum));

    intn->val.ul = i;
    intn->type = INTNUM_UL;
    intn->origsize = 0;

    return intn;
}

yasm_intnum *
yasm_intnum_new_int(long i)
{
    yasm_intnum *intn;

    /* positive numbers can go through the uint() function */
    if (i >= 0)
	return yasm_intnum_new_uint((unsigned long)i);

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
yasm_intnum_delete(yasm_intnum *intn)
{
    if (intn->type == INTNUM_BV)
	BitVector_Destroy(intn->val.bv);
    yasm_xfree(intn);
}

/*@-nullderef -nullpass -branchstate@*/
void
yasm_intnum_calc(yasm_intnum *acc, yasm_expr_op op, yasm_intnum *operand,
		 unsigned long lindex)
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
	    yasm__error(lindex, N_("invalid use of '%s'"), "SEG");
	    break;
	case YASM_EXPR_WRT:
	    yasm__error(lindex, N_("invalid use of '%s'"), "WRT");
	    break;
	case YASM_EXPR_SEGOFF:
	    yasm__error(lindex, N_("invalid use of '%s'"), ":");
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

int
yasm_intnum_is_zero(yasm_intnum *intn)
{
    return (intn->type == INTNUM_UL && intn->val.ul == 0);
}

int
yasm_intnum_is_pos1(yasm_intnum *intn)
{
    return (intn->type == INTNUM_UL && intn->val.ul == 1);
}

int
yasm_intnum_is_neg1(yasm_intnum *intn)
{
    return (intn->type == INTNUM_BV && BitVector_is_full(intn->val.bv));
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
		wordptr abs_bv = BitVector_Create(BITVECT_NATIVE_SIZE, FALSE);
		unsigned long ul;

		BitVector_Negate(abs_bv, intn->val.bv);
		if (Set_Max(abs_bv) >= 32) {
		    /* too negative */
		    BitVector_Destroy(abs_bv);
		    return LONG_MIN;
		}
		ul = BitVector_Chunk_Read(abs_bv, 32, 0);
		BitVector_Destroy(abs_bv);
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
yasm_intnum_get_sized(const yasm_intnum *intn, unsigned char *ptr, size_t size)
{
    unsigned long ul;
    unsigned char *buf;
    unsigned int len;

    switch (intn->type) {
	case INTNUM_UL:
	    ul = intn->val.ul;
	    while (size-- > 0) {
		YASM_WRITE_8(ptr, ul);
		if (ul != 0)
		    ul >>= 8;
	    }
	    break;
	case INTNUM_BV:
	    buf = BitVector_Block_Read(intn->val.bv, &len);
	    if (len < (unsigned int)size)
		yasm_internal_error(N_("Invalid size specified (too large)"));
	    memcpy(ptr, buf, size);
	    yasm_xfree(buf);
	    break;
    }
}

/* Return 1 if okay size, 0 if not */
int
yasm_intnum_check_size(const yasm_intnum *intn, size_t size, int is_signed)
{
    if (is_signed) {
	switch (intn->type) {
	    case INTNUM_UL:
		if (size >= 4)
		    return 1;

		/* INTNUM_UL is always positive */
		switch (size) {
		    case 4:
			return (intn->val.ul <= 0x7FFFFFFF);
		    case 3:
			return (intn->val.ul <= 0x007FFFFF);
		    case 2:
			return (intn->val.ul <= 0x00007FFF);
		    case 1:
			return (intn->val.ul <= 0x0000007F);
		}
		break;
	    case INTNUM_BV:
		if (size >= BITVECT_NATIVE_SIZE/8)
		    return 1;
		if (BitVector_msb_(intn->val.bv)) {
		    /* it's negative */
		    wordptr abs_bv = BitVector_Create(BITVECT_NATIVE_SIZE,
						      FALSE);
		    int retval;

		    BitVector_Negate(abs_bv, intn->val.bv);
		    retval = Set_Max(abs_bv) < (long)(size*8);

		    BitVector_Destroy(abs_bv);
		    return retval;
		} else
		    return (Set_Max(intn->val.bv) < (long)(size*8));
	}
    } else {
	switch (intn->type) {
	    case INTNUM_UL:
		if (size >= 4)
		    return 1;
		switch (size) {
		    case 3:
			return ((intn->val.ul & 0x00FFFFFF) == intn->val.ul);
		    case 2:
			return ((intn->val.ul & 0x0000FFFF) == intn->val.ul);
		    case 1:
			return ((intn->val.ul & 0x000000FF) == intn->val.ul);
		}
		break;
	    case INTNUM_BV:
		if (size >= BITVECT_NATIVE_SIZE/8)
		    return 1;
		else
		    return (Set_Max(intn->val.bv) < (long)(size*8));
	}
    }
    return 0;
}

void
yasm_intnum_print(FILE *f, const yasm_intnum *intn)
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
