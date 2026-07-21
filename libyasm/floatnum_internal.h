/**
 * \file libyasm/floatnum_internal.h
 * \brief YASM floating point (IEEE) privateinterface.
 *
 * \license
 *  Copyright (C) 2001-2007  Peter Johnson
 *  Copyright (C) 2026       yasm developers
 *
 *  Based on public-domain x86 assembly code by Randall Hyde (8/28/91).
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
#ifndef YASM_FLOATNUM_INTERNAL_H
#define YASM_FLOATNUM_INTERNAL_H

#include <libyasm/bitvect.h>

/* 97-bit internal floating point format:
 * 0000000s eeeeeeee eeeeeeee m.....................................m
 * Sign          exponent     mantissa (80 bits)
 *                            79                                    0
 *
 * Only L.O. bit of Sign byte is significant.  The rest is zero.
 * Exponent is bias 32767.
 * Mantissa does NOT have an implied one bit (it's explicit).
 */
struct yasm_floatnum {
    /*@only@*/ wordptr mantissa;        /* Allocated to MANT_BITS bits */
    unsigned short exponent;
    unsigned char sign;
    unsigned char flags;
};

#endif
