/* $IdPath$
 * Big and little endian file functions header file.
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
 * 3. Neither the name of the author nor the names of other contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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
#ifndef YASM_FILE_H
#define YASM_FILE_H

/* These functions only work properly if p is an (unsigned char *) */

#define WRITE_8(ptr, val)			\
	*((ptr)++) = (unsigned char)((val) & 0xFF)

#define WRITE_16_L(ptr, val)			\
	do {					\
	    *((ptr)++) = (unsigned char)((val) & 0xFF);		\
	    *((ptr)++) = (unsigned char)(((val) >> 8) & 0xFF);	\
	} while (0)

#define WRITE_32_L(ptr, val)			\
	do {					\
	    *((ptr)++) = (unsigned char)((val) & 0xFF);		\
	    *((ptr)++) = (unsigned char)(((val) >> 8) & 0xFF);	\
	    *((ptr)++) = (unsigned char)(((val) >> 16) & 0xFF);	\
	    *((ptr)++) = (unsigned char)(((val) >> 24) & 0xFF);	\
	} while (0)

#define WRITE_16_B(ptr, val)			\
	do {					\
	    *((ptr)++) = (unsigned char)(((val) >> 8) & 0xFF);	\
	    *((ptr)++) = (unsigned char)((val) & 0xFF);		\
	} while (0)

#define WRITE_32_B(ptr, val)			\
	do {					\
	    *((ptr)++) = (unsigned char)(((val) >> 24) & 0xFF);	\
	    *((ptr)++) = (unsigned char)(((val) >> 16) & 0xFF);	\
	    *((ptr)++) = (unsigned char)(((val) >> 8) & 0xFF);	\
	    *((ptr)++) = (unsigned char)((val) & 0xFF);		\
	} while (0)


/* Non-incrementing versions of the above. */

#define SAVE_8(ptr, val)			\
	*(ptr) = (unsigned char)((val) & 0xFF)

#define SAVE_16_L(ptr, val)			\
	do {					\
	    *(ptr) = (unsigned char)((val) & 0xFF);		\
	    *((ptr)+1) = (unsigned char)(((val) >> 8) & 0xFF);	\
	} while (0)

#define SAVE_32_L(ptr, val)			\
	do {					\
	    *(ptr) = (unsigned char)((val) & 0xFF);		\
	    *((ptr)+1) = (unsigned char)(((val) >> 8) & 0xFF);	\
	    *((ptr)+2) = (unsigned char)(((val) >> 16) & 0xFF);	\
	    *((ptr)+3) = (unsigned char)(((val) >> 24) & 0xFF);	\
	} while (0)

#define SAVE_16_B(ptr, val)			\
	do {					\
	    *(ptr) = (unsigned char)(((val) >> 8) & 0xFF);	\
	    *((ptr)+1) = (unsigned char)((val) & 0xFF);		\
	} while (0)

#define SAVE_32_B(ptr, val)			\
	do {					\
	    *(ptr) = (unsigned char)(((val) >> 24) & 0xFF);	\
	    *((ptr)+1) = (unsigned char)(((val) >> 16) & 0xFF);	\
	    *((ptr)+2) = (unsigned char)(((val) >> 8) & 0xFF);	\
	    *((ptr)+3) = (unsigned char)((val) & 0xFF);		\
	} while (0)

/* Direct-to-file versions of the above.  Using the above macros and a single
 * fwrite() will probably be faster than calling these functions many times.
 * These functions return 1 if the write was successful, 0 if not (so their
 * return values can be used like the return value from fwrite()).
 */

size_t fwrite_16_l(unsigned short val, FILE *f);
size_t fwrite_32_l(unsigned long val, FILE *f);
size_t fwrite_16_b(unsigned short val, FILE *f);
size_t fwrite_32_b(unsigned long val, FILE *f);

/* Read/Load versions.  val is the variable to receive the data. */

#define READ_8(val, ptr)			\
	(val) = *((ptr)++) & 0xFF

#define READ_16_L(val, ptr)			\
	do {					\
	    (val) = *((ptr)++) & 0xFF;		\
	    (val) |= (*((ptr)++) & 0xFF) << 8;	\
	} while (0)

#define READ_32_L(val, ptr)			\
	do {					\
	    (val) = *((ptr)++) & 0xFF;		\
	    (val) |= (*((ptr)++) & 0xFF) << 8;	\
	    (val) |= (*((ptr)++) & 0xFF) << 16;	\
	    (val) |= (*((ptr)++) & 0xFF) << 24;	\
	} while (0)

#define READ_16_B(val, ptr)			\
	do {					\
	    (val) = (*((ptr)++) & 0xFF) << 8;	\
	    (val) |= *((ptr)++) & 0xFF;		\
	} while (0)

#define READ_32_B(val, ptr)			\
	do {					\
	    (val) = (*((ptr)++) & 0xFF) << 24;	\
	    (val) |= (*((ptr)++) & 0xFF) << 16;	\
	    (val) |= (*((ptr)++) & 0xFF) << 8;	\
	    (val) |= *((ptr)++) & 0xFF;		\
	} while (0)

/* Non-incrementing versions of the above. */

#define LOAD_8(val, ptr)			\
	(val) = *(ptr) & 0xFF

#define LOAD_16_L(val, ptr)			\
	do {					\
	    (val) = *(ptr) & 0xFF;		\
	    (val) |= (*((ptr)+1) & 0xFF) << 8;	\
	} while (0)

#define LOAD_32_L(val, ptr)			\
	do {					\
	    (val) = (unsigned long)(*(ptr) & 0xFF);		    \
	    (val) |= (unsigned long)((*((ptr)+1) & 0xFF) << 8);	    \
	    (val) |= (unsigned long)((*((ptr)+2) & 0xFF) << 16);    \
	    (val) |= (unsigned long)((*((ptr)+3) & 0xFF) << 24);    \
	} while (0)

#define LOAD_16_B(val, ptr)			\
	do {					\
	    (val) = (*(ptr) & 0xFF) << 8;	\
	    (val) |= *((ptr)+1) & 0xFF;		\
	} while (0)

#define LOAD_32_B(val, ptr)			\
	do {					\
	    (val) = (unsigned long)((*(ptr) & 0xFF) << 24);	    \
	    (val) |= (unsigned long)((*((ptr)+1) & 0xFF) << 16);    \
	    (val) |= (unsigned long)((*((ptr)+2) & 0xFF) << 8);	    \
	    (val) |= (unsigned long)(*((ptr)+3) & 0xFF);	    \
	} while (0)

#endif
