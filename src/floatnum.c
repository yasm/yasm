/* $IdPath$
 * Floating point number functions.
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "util.h"

#include <stdio.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#endif

#include <libintl.h>
#define _(String)	gettext(String)
#ifdef gettext_noop
#define N_(String)	gettext_noop(String)
#else
#define N_(String)	(String)
#endif

#include "bitvect.h"
#include "file.h"

#include "errwarn.h"
#include "floatnum.h"

RCSID("$IdPath$");

floatnum *
floatnum_new(char *str)
{
    floatnum *flt = malloc(sizeof(floatnum));
    if (!flt)
	Fatal(FATAL_NOMEM);

    flt->mantissa = BitVector_Create(64, TRUE);
    if (!flt->mantissa)
	Fatal(FATAL_NOMEM);

    /* check for + or - character and skip */
    if (*str == '-') {
	flt->sign = 1;
	str++;
    } else if (*str == '+') {
	flt->sign = 0;
	str++;
    } else
	flt->sign = 0;

    return flt;
}

int
floatnum_get_int(unsigned long *ret_val, const floatnum *flt)
{
    unsigned char t[4];

    if (floatnum_get_single(t, flt))
	return 1;

    LOAD_LONG(*ret_val, &t[0]);
    return 0;
}

/* IEEE-754 (Intel) "single precision" format:
 * 32 bits:
 * Bit 31    Bit 22              Bit 0
 * |         |                       |
 * seeeeeee emmmmmmm mmmmmmmm mmmmmmmm
 *
 * e = bias 127 exponent
 * s = sign bit
 * m = mantissa bits, bit 23 is an implied one bit.
 */
int
floatnum_get_single(unsigned char *ptr, const floatnum *flt)
{
    unsigned long exponent = flt->exponent;
    unsigned int *output;
    unsigned char *buf;
    unsigned int len;

    output = BitVector_Create(32, TRUE);
    if (!output)
	Fatal(FATAL_NOMEM);

    /* copy mantissa */
    BitVector_Interval_Copy(output, flt->mantissa, 0, 40, 23);

    /* round mantissa */
    BitVector_increment(output);

    if (BitVector_bit_test(output, 23)) {
	/* overflowed, so divide mantissa by 2 (shift right) */
	BitVector_shift_right(output, 0);
	/* and up the exponent (checking for overflow) */
	if (exponent+1 >= 0x10000) {
	    BitVector_Destroy(output);
	    return 1;	    /* exponent too large */
	}
	exponent++;
    }

    /* adjust the exponent to bias 127 */
    exponent -= 32767-127;
    if (exponent >= 256) {
	BitVector_Destroy(output);
	return 1;	    /* exponent too large */
    }

    /* move exponent into place */
    BitVector_Chunk_Store(output, 8, 23, exponent);

    /* merge in sign bit */
    BitVector_Bit_Copy(output, 31, flt->sign);

    /* get little-endian bytes */
    buf = BitVector_Block_Read(output, &len);
    if (!buf)
	Fatal(FATAL_NOMEM);
    if (len != 4)
	InternalError(__LINE__, __FILE__,
		      _("Length of 32-bit BitVector not 4"));

    /* copy to output */
    memcpy(ptr, buf, 4*sizeof(unsigned char));

    /* free allocated resources */
    free(buf);

    BitVector_Destroy(output);

    return 0;
}

/* IEEE-754 (Intel) "double precision" format:
 * 64 bits:
 * bit 63       bit 51                                               bit 0
 * |            |                                                        |
 * seeeeeee eeeemmmm mmmmmmmm mmmmmmmm mmmmmmmm mmmmmmmm mmmmmmmm mmmmmmmm
 *
 * e = bias 1023 exponent.
 * s = sign bit.
 * m = mantissa bits.  Bit 52 is an implied one bit.
 */
int
floatnum_get_double(unsigned char *ptr, const floatnum *flt)
{
    unsigned long exponent = flt->exponent;
    unsigned int *output;
    unsigned char *buf;
    unsigned int len;

    output = BitVector_Create(64, TRUE);
    if (!output)
	Fatal(FATAL_NOMEM);

    /* copy mantissa */
    BitVector_Interval_Copy(output, flt->mantissa, 0, 11, 52);

    /* round mantissa */
    BitVector_increment(output);

    if (BitVector_bit_test(output, 52)) {
	/* overflowed, so divide mantissa by 2 (shift right) */
	BitVector_shift_right(output, 0);
	/* and up the exponent (checking for overflow) */
	if (exponent+1 >= 0x10000) {
	    BitVector_Destroy(output);
	    return 1;	    /* exponent too large */
	}
	exponent++;
    }

    /* adjust the exponent to bias 1023 */
    exponent -= 32767-1023;
    if (exponent >= 2048) {
	BitVector_Destroy(output);
	return 1;	    /* exponent too large */
    }

    /* move exponent into place */
    BitVector_Chunk_Store(output, 11, 52, exponent);

    /* merge in sign bit */
    BitVector_Bit_Copy(output, 63, flt->sign);

    /* get little-endian bytes */
    buf = BitVector_Block_Read(output, &len);
    if (!buf)
	Fatal(FATAL_NOMEM);
    if (len != 8)
	InternalError(__LINE__, __FILE__,
		      _("Length of 64-bit BitVector not 8"));

    /* copy to output */
    memcpy(ptr, buf, 8*sizeof(unsigned char));

    /* free allocated resources */
    free(buf);

    BitVector_Destroy(output);

    return 0;
}

/* IEEE-754 (Intel) "extended precision" format:
 * 80 bits:
 * bit 79            bit 63                           bit 0
 * |                 |                                    |
 * seeeeeee eeeeeeee mmmmmmmm m...m m...m m...m m...m m...m
 *
 * e = bias 16384 exponent
 * m = 64 bit mantissa with NO implied bit!
 * s = sign (for mantissa)
 */
int
floatnum_get_extended(unsigned char *ptr, const floatnum *flt)
{
    unsigned short exponent = flt->exponent;
    unsigned char *buf;
    unsigned int len;

    /* Adjust the exponent to bias 16384 */
    exponent -= 0x4000;
    if (exponent >= 0x8000)
	return 1;	    /* exponent too large */

    /* get little-endian bytes */
    buf = BitVector_Block_Read(flt->mantissa, &len);
    if (!buf)
	Fatal(FATAL_NOMEM);
    if (len != 8)
	InternalError(__LINE__, __FILE__,
		      _("Length of 64-bit BitVector not 8"));

    /* copy to output */
    memcpy(ptr, buf, 8*sizeof(unsigned char));

    /* Save exponent and sign in proper location */
    SAVE_SHORT(&ptr[8], exponent);
    if (flt->sign)
	ptr[9] |= 0x80;

    /* free allocated resources */
    free(buf);

    return 0;
}

void
floatnum_print(const floatnum *flt)
{
    /* TODO */
}
