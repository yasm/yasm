/*
 * Little-endian file functions.
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
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#include "file.h"


size_t
fwrite_16_l(unsigned short val, FILE *f)
{
    if (fputc(val & 0xFF, f) == EOF)
	return 0;
    if (fputc((val >> 8) & 0xFF, f) == EOF)
	return 0;
    return 1;
}

size_t
fwrite_32_l(unsigned long val, FILE *f)
{
    if (fputc((int)(val & 0xFF), f) == EOF)
	return 0;
    if (fputc((int)((val >> 8) & 0xFF), f) == EOF)
	return 0;
    if (fputc((int)((val >> 16) & 0xFF), f) == EOF)
	return 0;
    if (fputc((int)((val >> 24) & 0xFF), f) == EOF)
	return 0;
    return 1;
}

size_t
fwrite_16_b(unsigned short val, FILE *f)
{
    if (fputc((val >> 8) & 0xFF, f) == EOF)
	return 0;
    if (fputc(val & 0xFF, f) == EOF)
	return 0;
    return 1;
}

size_t
fwrite_32_b(unsigned long val, FILE *f)
{
    if (fputc((int)((val >> 24) & 0xFF), f) == EOF)
	return 0;
    if (fputc((int)((val >> 16) & 0xFF), f) == EOF)
	return 0;
    if (fputc((int)((val >> 8) & 0xFF), f) == EOF)
	return 0;
    if (fputc((int)(val & 0xFF), f) == EOF)
	return 0;
    return 1;
}
