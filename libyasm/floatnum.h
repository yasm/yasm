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

/*@only@*/ floatnum *floatnum_new(const char *str);
/*@only@*/ floatnum *floatnum_copy(const floatnum *flt);
void floatnum_delete(/*@only@*/ floatnum *flt);

/* calculation function: acc = acc op operand */
void floatnum_calc(floatnum *acc, ExprOp op, floatnum *operand);

/* The get functions return nonzero if flt can't fit into that size format:
 * -1 if underflow occurred, 1 if overflow occurred.
 */

/* Essentially a convert to single-precision and return as 32-bit value.
 * The 32-bit value is a "standard" C value (eg, of unknown endian).
 */
int floatnum_get_int(const floatnum *flt, /*@out@*/ unsigned long *ret_val);

/* ptr will point to the Intel-format little-endian byte string after a
 * successful call (eg, [0] should be the first byte output to the file).
 */
int floatnum_get_sized(const floatnum *flt, /*@out@*/ unsigned char *ptr,
		       size_t size);

/* Basic check to see if size is even valid for flt conversion (doesn't
 * actually check for underflow/overflow but rather checks for size=4,8,10).
 * Returns 1 if valid, 0 if not.
 */
int floatnum_check_size(const floatnum *flt, size_t size);

void floatnum_print(FILE *f, const floatnum *flt);

#endif
