/* $IdPath$
 * Integer number functions header file.
 *
 *  Copyright (C) 2001  Peter Johnson
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
#ifndef YASM_INTNUM_H
#define YASM_INTNUM_H

/*@only@*/ intnum *intnum_new_dec(char *str);
/*@only@*/ intnum *intnum_new_bin(char *str);
/*@only@*/ intnum *intnum_new_oct(char *str);
/*@only@*/ intnum *intnum_new_hex(char *str);
/* convert character constant to integer value, using NASM rules */
/*@only@*/ intnum *intnum_new_charconst_nasm(const char *str);
/*@only@*/ intnum *intnum_new_int(unsigned long i);
/*@only@*/ intnum *intnum_copy(const intnum *intn);
void intnum_delete(/*@only@*/ intnum *intn);

/* calculation function: acc = acc op operand */
void intnum_calc(intnum *acc, ExprOp op, intnum *operand);

/* simple value checks (for catching identities and the like) */
int intnum_is_zero(intnum *acc);
int intnum_is_pos1(intnum *acc);
int intnum_is_neg1(intnum *acc);

/* The get functions truncate intn to the size specified; they don't check
 * for overflow.  Use intnum_check_size() to check for overflow.
 */

/* Return a 32-bit value in "standard" C format (eg, of unknown endian).
 * intnum_get_uint() treats intn as an unsigned integer (and returns as such).
 * intnum_get_int() treats intn as a signed integer (and returns as such).
 */
unsigned long intnum_get_uint(const intnum *intn);
long intnum_get_int(const intnum *intn);

/* ptr will point to the Intel-format little-endian byte string after
 * call (eg, [0] should be the first byte output to the file).
 */
void intnum_get_sized(const intnum *intn, unsigned char *ptr, size_t size);

/* Check to see if intn will fit without overflow in size bytes.
 * If is_signed is 1, intn is treated as a signed number.
 * Returns 1 if it will, 0 if not.
 */
int intnum_check_size(const intnum *intn, size_t size, int is_signed);

void intnum_print(FILE *f, const intnum *intn);

#endif
