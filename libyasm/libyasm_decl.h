/**
 * \file libyasm.h
 * \brief YASM library decl macro.
 *
 * \license
 *  Copyright (C) 2026       yasm developers
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
#ifndef YASM_DECL_H
#define YASM_DECL_H

#ifndef YASM_LIB_DECL
#ifdef _WIN32
#ifndef LIBYASM_STATIC
#ifdef YASM_LIB_SOURCE
#define YASM_LIB_DECL __declspec(dllexport)
#else
#define YASM_LIB_DECL __declspec(dllimport)
#endif
#endif
#else
#ifndef LIBYASM_STATIC
#ifdef YASM_LIB_SOURCE
#define YASM_LIB_DECL __attribute__((visibility("default")))
#endif
#endif
#endif
#endif

#ifndef YASM_LIB_DECL
#define YASM_LIB_DECL
#endif

#endif
