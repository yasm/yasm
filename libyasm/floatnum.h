/* $IdPath$
 * Floating point number functions header file.
 *
 *  Copyright (C) 2001  Peter Johnson
 *
 *  Based on public-domain x86 assembly code by Randall Hyde (8/28/91).
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
#ifndef YASM_FLOATNUM_H
#define YASM_FLOATNUM_H

#ifndef YASM_EXPROP
#define YASM_EXPROP
typedef enum {
    EXPR_ADD,
    EXPR_SUB,
    EXPR_MUL,
    EXPR_DIV,
    EXPR_SIGNDIV,
    EXPR_MOD,
    EXPR_SIGNMOD,
    EXPR_NEG,
    EXPR_NOT,
    EXPR_OR,
    EXPR_AND,
    EXPR_XOR,
    EXPR_SHL,
    EXPR_SHR,
    EXPR_LOR,
    EXPR_LAND,
    EXPR_LNOT,
    EXPR_LT,
    EXPR_GT,
    EXPR_EQ,
    EXPR_LE,
    EXPR_GE,
    EXPR_NE,
    EXPR_IDENT	/* if right is IDENT, then the entire expr is just a num */
} ExprOp;
#endif

#ifndef YASM_FLOATNUM
#define YASM_FLOATNUM
typedef struct floatnum floatnum;
#endif

floatnum *floatnum_new(const char *str);
void floatnum_delete(floatnum *flt);

/* calculation function: acc = acc op operand */
void floatnum_calc(floatnum *acc, ExprOp op, floatnum *operand);

/* The get functions return nonzero if flt can't fit into that size format:
 * -1 if underflow occurred, 1 if overflow occurred.
 */

/* Essentially a convert to single-precision and return as 32-bit value.
 * The 32-bit value is a "standard" C value (eg, of unknown endian).
 */
int floatnum_get_int(unsigned long *ret_val, const floatnum *flt);

/* ptr will point to the Intel-format little-endian byte string after a
 * successful call (eg, [0] should be the first byte output to the file).
 */
int floatnum_get_sized(unsigned char *ptr, const floatnum *flt, size_t size);

/* Basic check to see if size is even valid for flt conversion (doesn't
 * actually check for underflow/overflow but rather checks for size=4,8,10).
 * Returns 1 if valid, 0 if not.
 */
int floatnum_check_size(const floatnum *flt, size_t size);

void floatnum_print(const floatnum *flt);

#endif
