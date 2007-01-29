/**
 * \file libyasm.h
 * \brief YASM library primary header file.
 *
 * \rcs
 * $Id$
 * \endrcs
 *
 * \license
 *  Copyright (C) 2003  Peter Johnson
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
 *
 * Define YASM_LIB_INTERNAL to include many internal function and variable
 * definitions that violate the yasm_* namespace.  This includes
 * compat-queue.h, bitvect.h, and hamt.h.
 *
 * Additional parts may be included via:
 *  - YASM_BC_INTERNAL: reveal bytecode internal structures via bc-int.h
 *                      inclusion
 *  - YASM_EXPR_INTERNAL: reveal expr internal structures via expr-int.h
 *                        inclusion
 */
#ifndef YASM_LIB_H
#define YASM_LIB_H

#ifdef YASM_PYXELATOR
typedef struct __FILE FILE;
typedef struct __va_list va_list;
typedef unsigned long size_t;
#else
#include <stdio.h>
#include <stdarg.h>
#endif

#ifdef YASM_LIB_INTERNAL
# include <libyasm/compat-queue.h>
#endif

#include <libyasm/coretype.h>
#include <libyasm/valparam.h>

#include <libyasm/linemap.h>

#include <libyasm/errwarn.h>
#include <libyasm/intnum.h>
#include <libyasm/floatnum.h>
#include <libyasm/expr.h>
#include <libyasm/value.h>
#include <libyasm/symrec.h>

#include <libyasm/bytecode.h>
#include <libyasm/section.h>

#include <libyasm/arch.h>
#include <libyasm/dbgfmt.h>
#include <libyasm/objfmt.h>
#include <libyasm/listfmt.h>
#include <libyasm/parser.h>
#include <libyasm/preproc.h>

#include <libyasm/file.h>
#include <libyasm/module.h>

#ifdef YASM_LIB_INTERNAL
#ifdef YASM_BC_INTERNAL
#include <libyasm/bc-int.h>
#endif
#ifdef YASM_EXPR_INTERNAL
#include <libyasm/expr-int.h>
#endif
#include <libyasm/hamt.h>
#include <libyasm/bitvect.h>
#include <libyasm/md5.h>
#endif

#endif
