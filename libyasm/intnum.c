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

#include "bitvect.h"
#include "file.h"

#include "errwarn.h"
#include "intnum.h"


#define BITVECT_ALLOC_SIZE	80

struct yasm_intnum {
    union val {
	unsigned long ul;	/* integer value (for integers <=32 bits) */
	intptr bv;		/* bit vector (for integers >32 bits) */
    } val;
    enum { INTNUM_UL, INTNUM_BV } type;
    unsigned char origsize;	/* original (parsed) size, in bits */
};

/* static bitvect used for conversions */
static /*@only@*/ wordptr conv_bv;


void
yasm_intnum_initialize(void)
{
    conv_bv = BitVector_Create(BITVECT_ALLOC_SIZE, FALSE);
    BitVector_from_Dec_static_Boot(BITVECT_ALLOC_SIZE);
}

void
yasm_intnum_cleanup(void)
{
    BitVector_Destroy(conv_bv);
    BitVector_from_Dec_static_Shutdown();
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

    if(intn->origsize > BITVECT_ALLOC_SIZE)
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

    if(intn->origsize > BITVECT_ALLOC_SIZE)
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

    if(intn->origsize > BITVECT_ALLOC_SIZE)
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

    if (len > 4)
	yasm__warning(YASM_WARN_GENERAL, lindex,
	    N_("character constant too large, ignoring trailing characters"));

    intn->val.ul = 0;
    intn->type = INTNUM_UL;
    intn->origsize = len*8;

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
    /* FIXME: Better way of handling signed numbers? */
    return yasm_intnum_new_uint((unsigned long)i);
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
yasm_intnum_calc(yasm_intnum *acc, yasm_expr_op op, yasm_intnum *operand)
{
    wordptr result = (wordptr)NULL, op1 = (wordptr)NULL, op2 = (wordptr)NULL;
    wordptr spare = (wordptr)NULL;
    boolean carry;

    /* upsize to bitvector op if one of two parameters is bitvector already.
     * BitVector results must be calculated through intermediate storage.
     */
    if (acc->type == INTNUM_BV || (operand && operand->type == INTNUM_BV)) {
	result = BitVector_Create(BITVECT_ALLOC_SIZE, FALSE);
	spare = BitVector_Create(BITVECT_ALLOC_SIZE, FALSE);

	if (acc->type == INTNUM_BV)
	    op1 = acc->val.bv;
	else {
	    op1 = BitVector_Create(BITVECT_ALLOC_SIZE, TRUE);
	    BitVector_Chunk_Store(op1, 32, 0, acc->val.ul);
	}

	if (operand) {
	    if (operand->type == INTNUM_BV)
		op2 = acc->val.bv;
	    else {
		op2 = BitVector_Create(BITVECT_ALLOC_SIZE, TRUE);
		BitVector_Chunk_Store(op2, 32, 0, operand->val.ul);
	    }
	}
    }

    if (!operand && op != YASM_EXPR_NEG && op != YASM_EXPR_NOT &&
	op != YASM_EXPR_LNOT)
	yasm_internal_error(N_("Operation needs an operand"));

    /* A operation does a bitvector computation if result is allocated. */
    switch (op) {
	case YASM_EXPR_ADD:
	    if (result)
		BitVector_add(result, op1, op2, &carry);
	    else
		acc->val.ul = acc->val.ul + operand->val.ul;
	    break;
	case YASM_EXPR_SUB:
	    if (result)
		BitVector_sub(result, op1, op2, &carry);
	    else
		acc->val.ul = acc->val.ul - operand->val.ul;
	    break;
	case YASM_EXPR_MUL:
	    if (result)
		/* TODO: Make sure result size = op1+op2 */
		BitVector_Multiply(result, op1, op2);
	    else
		acc->val.ul = acc->val.ul * operand->val.ul;
	    break;
	case YASM_EXPR_DIV:
	    if (result) {
		/* TODO: make sure op1 and op2 are unsigned */
		BitVector_Divide(result, op1, op2, spare);
	    } else
		acc->val.ul = acc->val.ul / operand->val.ul;
	    break;
	case YASM_EXPR_SIGNDIV:
	    if (result)
		BitVector_Divide(result, op1, op2, spare);
	    else
		acc->val.ul = (unsigned long)((signed long)acc->val.ul /
					      (signed long)operand->val.ul);
	    break;
	case YASM_EXPR_MOD:
	    if (result) {
		/* TODO: make sure op1 and op2 are unsigned */
		BitVector_Divide(spare, op1, op2, result);
	    } else
		acc->val.ul = acc->val.ul % operand->val.ul;
	    break;
	case YASM_EXPR_SIGNMOD:
	    if (result)
		BitVector_Divide(spare, op1, op2, result);
	    else
		acc->val.ul = (unsigned long)((signed long)acc->val.ul %
					      (signed long)operand->val.ul);
	    break;
	case YASM_EXPR_NEG:
	    if (result)
		BitVector_Negate(result, op1);
	    else
		acc->val.ul = -(acc->val.ul);
	    break;
	case YASM_EXPR_NOT:
	    if (result)
		Set_Complement(result, op1);
	    else
		acc->val.ul = ~(acc->val.ul);
	    break;
	case YASM_EXPR_OR:
	    if (result)
		Set_Union(result, op1, op2);
	    else
		acc->val.ul = acc->val.ul | operand->val.ul;
	    break;
	case YASM_EXPR_AND:
	    if (result)
		Set_Intersection(result, op1, op2);
	    else
		acc->val.ul = acc->val.ul & operand->val.ul;
	    break;
	case YASM_EXPR_XOR:
	    if (result)
		Set_ExclusiveOr(result, op1, op2);
	    else
		acc->val.ul = acc->val.ul ^ operand->val.ul;
	    break;
	case YASM_EXPR_SHL:
	    if (result) {
		if (operand->type == INTNUM_UL) {
		    BitVector_Copy(result, op1);
		    BitVector_Move_Left(result, (N_int)operand->val.ul);
		} else	/* don't even bother, just zero result */
		    BitVector_Empty(result);
	    } else
		acc->val.ul = acc->val.ul << operand->val.ul;
	    break;
	case YASM_EXPR_SHR:
	    if (result) {
		if (operand->type == INTNUM_UL) {
		    BitVector_Copy(result, op1);
		    BitVector_Move_Right(result, (N_int)operand->val.ul);
		} else	/* don't even bother, just zero result */
		    BitVector_Empty(result);
	    } else
		acc->val.ul = acc->val.ul >> operand->val.ul;
	    break;
	case YASM_EXPR_LOR:
	    if (result) {
		BitVector_Empty(result);
		BitVector_LSB(result, !BitVector_is_empty(op1) ||
			      !BitVector_is_empty(op2));
	    } else
		acc->val.ul = acc->val.ul || operand->val.ul;
	    break;
	case YASM_EXPR_LAND:
	    if (result) {
		BitVector_Empty(result);
		BitVector_LSB(result, !BitVector_is_empty(op1) &&
			      !BitVector_is_empty(op2));
	    } else
		acc->val.ul = acc->val.ul && operand->val.ul;
	    break;
	case YASM_EXPR_LNOT:
	    if (result) {
		BitVector_Empty(result);
		BitVector_LSB(result, BitVector_is_empty(op1));
	    } else
		acc->val.ul = !acc->val.ul;
	    break;
	case YASM_EXPR_EQ:
	    if (result) {
		BitVector_Empty(result);
		BitVector_LSB(result, BitVector_equal(op1, op2));
	    } else
		acc->val.ul = acc->val.ul == operand->val.ul;
	    break;
	case YASM_EXPR_LT:
	    if (result) {
		BitVector_Empty(result);
		BitVector_LSB(result, BitVector_Lexicompare(op1, op2) < 0);
	    } else
		acc->val.ul = acc->val.ul < operand->val.ul;
	    break;
	case YASM_EXPR_GT:
	    if (result) {
		BitVector_Empty(result);
		BitVector_LSB(result, BitVector_Lexicompare(op1, op2) > 0);
	    } else
		acc->val.ul = acc->val.ul > operand->val.ul;
	    break;
	case YASM_EXPR_LE:
	    if (result) {
		BitVector_Empty(result);
		BitVector_LSB(result, BitVector_Lexicompare(op1, op2) <= 0);
	    } else
		acc->val.ul = acc->val.ul <= operand->val.ul;
	    break;
	case YASM_EXPR_GE:
	    if (result) {
		BitVector_Empty(result);
		BitVector_LSB(result, BitVector_Lexicompare(op1, op2) >= 0);
	    } else
		acc->val.ul = acc->val.ul >= operand->val.ul;
	    break;
	case YASM_EXPR_NE:
	    if (result) {
		BitVector_Empty(result);
		BitVector_LSB(result, !BitVector_equal(op1, op2));
	    } else
		acc->val.ul = acc->val.ul != operand->val.ul;
	    break;
	case YASM_EXPR_IDENT:
	    if (result)
		BitVector_Copy(result, op1);
	    break;
	default:
	    yasm_internal_error(N_("invalid operation in intnum calculation"));
    }

    /* If we were doing a bitvector computation... */
    if (result) {
	BitVector_Destroy(spare);

	if (op1 && acc->type != INTNUM_BV)
	    BitVector_Destroy(op1);
	if (op2 && operand && operand->type != INTNUM_BV)
	    BitVector_Destroy(op2);

	/* Try to fit the result into 32 bits if possible */
	if (Set_Max(result) < 32) {
	    if (acc->type == INTNUM_BV) {
		BitVector_Destroy(acc->val.bv);
		acc->type = INTNUM_UL;
	    }
	    acc->val.ul = BitVector_Chunk_Read(result, 32, 0);
	    BitVector_Destroy(result);
	} else {
	    if (acc->type == INTNUM_BV) {
		BitVector_Copy(acc->val.bv, result);
		BitVector_Destroy(result);
	    } else {
		acc->type = INTNUM_BV;
		acc->val.bv = result;
	    }
	}
    }
}
/*@=nullderef =nullpass =branchstate@*/

int
yasm_intnum_is_zero(yasm_intnum *intn)
{
    return ((intn->type == INTNUM_UL && intn->val.ul == 0) ||
	    (intn->type == INTNUM_BV && BitVector_is_empty(intn->val.bv)));
}

int
yasm_intnum_is_pos1(yasm_intnum *intn)
{
    return ((intn->type == INTNUM_UL && intn->val.ul == 1) ||
	    (intn->type == INTNUM_BV && Set_Max(intn->val.bv) == 0));
}

int
yasm_intnum_is_neg1(yasm_intnum *intn)
{
    return ((intn->type == INTNUM_UL && (long)intn->val.ul == -1) ||
	    (intn->type == INTNUM_BV && BitVector_is_full(intn->val.bv)));
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
	    return (long)intn->val.ul;
	case INTNUM_BV:
	    if (BitVector_msb_(intn->val.bv)) {
		/* it's negative: negate the bitvector to get a positive
		 * number, then negate the positive number.
		 */
		intptr abs_bv = BitVector_Create(BITVECT_ALLOC_SIZE, FALSE);
		long retval;

		BitVector_Negate(abs_bv, intn->val.bv);
		retval = -((long)BitVector_Chunk_Read(abs_bv, 32, 0));

		BitVector_Destroy(abs_bv);
		return retval;
	    } else
		return (long)BitVector_Chunk_Read(intn->val.bv, 32, 0);
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
	long absl;

	switch (intn->type) {
	    case INTNUM_UL:
		if (size >= 4)
		    return 1;
		/* absl = absolute value of (long)intn->val.ul */
		absl = (long)intn->val.ul;
		if (absl < 0)
		    absl = -absl;

		switch (size) {
		    case 3:
			return ((absl & 0x00FFFFFF) == absl);
		    case 2:
			return ((absl & 0x0000FFFF) == absl);
		    case 1:
			return ((absl & 0x000000FF) == absl);
		}
		break;
	    case INTNUM_BV:
		if (size >= 10)
		    return 1;
		if (BitVector_msb_(intn->val.bv)) {
		    /* it's negative */
		    intptr abs_bv = BitVector_Create(BITVECT_ALLOC_SIZE,
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
		if (size >= 10)
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
