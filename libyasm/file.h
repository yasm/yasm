/**
 * \file file.h
 * \brief YASM big and little endian file interface.
 *
 * $IdPath: yasm/libyasm/file.h,v 1.12 2003/03/13 06:54:19 peter Exp $
 *
 *  Copyright (C) 2001  Peter Johnson
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
 */
#ifndef YASM_FILE_H
#define YASM_FILE_H

/** Write an 8-bit value to a buffer, incrementing buffer pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	8-bit value
 */
#define YASM_WRITE_8(ptr, val)			\
	*((ptr)++) = (unsigned char)((val) & 0xFF)

/** Write a 16-bit value to a buffer in little endian, incrementing buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	16-bit value
 */
#define YASM_WRITE_16_L(ptr, val)		\
	do {					\
	    *((ptr)++) = (unsigned char)((val) & 0xFF);		\
	    *((ptr)++) = (unsigned char)(((val) >> 8) & 0xFF);	\
	} while (0)

/** Write a 32-bit value to a buffer in little endian, incrementing buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	32-bit value
 */
#define YASM_WRITE_32_L(ptr, val)		\
	do {					\
	    *((ptr)++) = (unsigned char)((val) & 0xFF);		\
	    *((ptr)++) = (unsigned char)(((val) >> 8) & 0xFF);	\
	    *((ptr)++) = (unsigned char)(((val) >> 16) & 0xFF);	\
	    *((ptr)++) = (unsigned char)(((val) >> 24) & 0xFF);	\
	} while (0)

/** Write a 16-bit value to a buffer in big endian, incrementing buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	16-bit value
 */
#define YASM_WRITE_16_B(ptr, val)		\
	do {					\
	    *((ptr)++) = (unsigned char)(((val) >> 8) & 0xFF);	\
	    *((ptr)++) = (unsigned char)((val) & 0xFF);		\
	} while (0)

/** Write a 32-bit value to a buffer in big endian, incrementing buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	32-bit value
 */
#define YASM_WRITE_32_B(ptr, val)		\
	do {					\
	    *((ptr)++) = (unsigned char)(((val) >> 24) & 0xFF);	\
	    *((ptr)++) = (unsigned char)(((val) >> 16) & 0xFF);	\
	    *((ptr)++) = (unsigned char)(((val) >> 8) & 0xFF);	\
	    *((ptr)++) = (unsigned char)((val) & 0xFF);		\
	} while (0)


/** Write an 8-bit value to a buffer.  Does not increment buffer pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	8-bit value
 */
#define YASM_SAVE_8(ptr, val)			\
	*(ptr) = (unsigned char)((val) & 0xFF)

/** Write a 16-bit value to a buffer in little endian.  Does not increment
 * buffer pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	16-bit value
 */
#define YASM_SAVE_16_L(ptr, val)		\
	do {					\
	    *(ptr) = (unsigned char)((val) & 0xFF);		\
	    *((ptr)+1) = (unsigned char)(((val) >> 8) & 0xFF);	\
	} while (0)

/** Write a 32-bit value to a buffer in little endian.  Does not increment
 * buffer pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	32-bit value
 */
#define YASM_SAVE_32_L(ptr, val)		\
	do {					\
	    *(ptr) = (unsigned char)((val) & 0xFF);		\
	    *((ptr)+1) = (unsigned char)(((val) >> 8) & 0xFF);	\
	    *((ptr)+2) = (unsigned char)(((val) >> 16) & 0xFF);	\
	    *((ptr)+3) = (unsigned char)(((val) >> 24) & 0xFF);	\
	} while (0)

/** Write a 16-bit value to a buffer in big endian.  Does not increment buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	16-bit value
 */
#define YASM_SAVE_16_B(ptr, val)		\
	do {					\
	    *(ptr) = (unsigned char)(((val) >> 8) & 0xFF);	\
	    *((ptr)+1) = (unsigned char)((val) & 0xFF);		\
	} while (0)

/** Write a 32-bit value to a buffer in big endian.  Does not increment buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	32-bit value
 */
#define YASM_SAVE_32_B(ptr, val)		\
	do {					\
	    *(ptr) = (unsigned char)(((val) >> 24) & 0xFF);	\
	    *((ptr)+1) = (unsigned char)(((val) >> 16) & 0xFF);	\
	    *((ptr)+2) = (unsigned char)(((val) >> 8) & 0xFF);	\
	    *((ptr)+3) = (unsigned char)((val) & 0xFF);		\
	} while (0)

/** Direct-to-file version of YASM_SAVE_16_L().
 * \note Using the macro multiple times with a single fwrite() call will
 *       probably be faster than calling this function many times.
 * \param val	16-bit value
 * \param f	file
 * \return 1 if the write was successful, 0 if not (just like fwrite()).
 */
size_t yasm_fwrite_16_l(unsigned short val, FILE *f);

/** Direct-to-file version of YASM_SAVE_32_L().
 * \note Using the macro multiple times with a single fwrite() call will
 *       probably be faster than calling this function many times.
 * \param val	32-bit value
 * \param f	file
 * \return 1 if the write was successful, 0 if not (just like fwrite()).
 */
size_t yasm_fwrite_32_l(unsigned long val, FILE *f);

/** Direct-to-file version of YASM_SAVE_16_B().
 * \note Using the macro multiple times with a single fwrite() call will
 *       probably be faster than calling this function many times.
 * \param val	16-bit value
 * \param f	file
 * \return 1 if the write was successful, 0 if not (just like fwrite()).
 */
size_t yasm_fwrite_16_b(unsigned short val, FILE *f);

/** Direct-to-file version of YASM_SAVE_32_B().
 * \note Using the macro multiple times with a single fwrite() call will
 *       probably be faster than calling this function many times.
 * \param val	32-bit value
 * \param f	file
 * \return 1 if the write was successful, 0 if not (just like fwrite()).
 */
size_t yasm_fwrite_32_b(unsigned long val, FILE *f);

/** Read an 8-bit value from a buffer, incrementing buffer pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	8-bit value
 */
#define YASM_READ_8(val, ptr)			\
	(val) = *((ptr)++) & 0xFF

/** Read a 16-bit value from a buffer in little endian, incrementing buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	16-bit value
 */
#define YASM_READ_16_L(val, ptr)		\
	do {					\
	    (val) = *((ptr)++) & 0xFF;		\
	    (val) |= (*((ptr)++) & 0xFF) << 8;	\
	} while (0)

/** Read a 32-bit value from a buffer in little endian, incrementing buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	32-bit value
 */
#define YASM_READ_32_L(val, ptr)		\
	do {					\
	    (val) = *((ptr)++) & 0xFF;		\
	    (val) |= (*((ptr)++) & 0xFF) << 8;	\
	    (val) |= (*((ptr)++) & 0xFF) << 16;	\
	    (val) |= (*((ptr)++) & 0xFF) << 24;	\
	} while (0)

/** Read a 16-bit value from a buffer in big endian, incrementing buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	16-bit value
 */
#define YASM_READ_16_B(val, ptr)		\
	do {					\
	    (val) = (*((ptr)++) & 0xFF) << 8;	\
	    (val) |= *((ptr)++) & 0xFF;		\
	} while (0)

/** Read a 32-bit value from a buffer in big endian, incrementing buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	32-bit value
 */
#define YASM_READ_32_B(val, ptr)		\
	do {					\
	    (val) = (*((ptr)++) & 0xFF) << 24;	\
	    (val) |= (*((ptr)++) & 0xFF) << 16;	\
	    (val) |= (*((ptr)++) & 0xFF) << 8;	\
	    (val) |= *((ptr)++) & 0xFF;		\
	} while (0)

/** Read an 8-bit value from a buffer.  Does not increment buffer pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	8-bit value
 */
#define YASM_LOAD_8(val, ptr)			\
	(val) = *(ptr) & 0xFF

/** Read a 16-bit value from a buffer in little endian.  Does not increment
 * buffer pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	16-bit value
 */
#define YASM_LOAD_16_L(val, ptr)		\
	do {					\
	    (val) = *(ptr) & 0xFF;		\
	    (val) |= (*((ptr)+1) & 0xFF) << 8;	\
	} while (0)

/** Read a 32-bit value from a buffer in little endian.  Does not increment
 * buffer pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	32-bit value
 */
#define YASM_LOAD_32_L(val, ptr)		\
	do {					\
	    (val) = (unsigned long)(*(ptr) & 0xFF);		    \
	    (val) |= (unsigned long)((*((ptr)+1) & 0xFF) << 8);	    \
	    (val) |= (unsigned long)((*((ptr)+2) & 0xFF) << 16);    \
	    (val) |= (unsigned long)((*((ptr)+3) & 0xFF) << 24);    \
	} while (0)

/** Read a 16-bit value from a buffer in big endian.  Does not increment buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	16-bit value
 */
#define YASM_LOAD_16_B(val, ptr)		\
	do {					\
	    (val) = (*(ptr) & 0xFF) << 8;	\
	    (val) |= *((ptr)+1) & 0xFF;		\
	} while (0)

/** Read a 32-bit value from a buffer in big endian.  Does not increment buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	32-bit value
 */
#define YASM_LOAD_32_B(val, ptr)		\
	do {					\
	    (val) = (unsigned long)((*(ptr) & 0xFF) << 24);	    \
	    (val) |= (unsigned long)((*((ptr)+1) & 0xFF) << 16);    \
	    (val) |= (unsigned long)((*((ptr)+2) & 0xFF) << 8);	    \
	    (val) |= (unsigned long)(*((ptr)+3) & 0xFF);	    \
	} while (0)

#endif
