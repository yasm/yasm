/* $IdPath$
 * Libyasm interface primary header file.
 *
 *  Copyright (C) 2003  Peter Johnson
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
#ifndef YASM_LIB_H
#define YASM_LIB_H

/* Define YASM_INTERNAL to include many internal function and variable defs.
 * This includes compat-queue.h, bitvect.h, hamt.h, and util.h, which violate
 * the yasm_* namespace (see individual files for details)!
 *
 * Additional parts may be included via YASM_*_INTERNAL inclusion flags:
 * YASM_BC_INTERNAL: reveal bytecode internal structures via bc-int.h inclusion
 * YASM_EXPR_INTERNAL: reveal expr internal structures via expr-int.h inclusion
 * YASM_AUTOCONF_INTERNAL: include parts of util.h that depend on certain
 *                         autoconfig settings.. see util.h for more details
 * YASM_LIB_AC_INTERNAL: include libyasm/config.h if HAVE_CONFIG_H is defined
 * YASM_GETTEXT_INTERNAL: define typical gettext _() and N_().  Requires
 *                        YASM_AUTOCONF_INTERNAL define.
 *
 * YASM_LIB_INTERNAL: defines YASM_INTERNAL, YASM_AUTOCONF_INTERNAL,
 *                    YASM_LIB_AC_INTERNAL, and YASM_GETTEXT_INTERNAL
 *                    (used when compiling the library itself)
 */

#include <libyasm/util.h>
#include <libyasm/linemgr.h>

#include <libyasm/errwarn.h>
#include <libyasm/intnum.h>
#include <libyasm/floatnum.h>
#include <libyasm/expr.h>
#include <libyasm/symrec.h>

#include <libyasm/bytecode.h>
#include <libyasm/section.h>

#include <libyasm/arch.h>
#include <libyasm/dbgfmt.h>
#include <libyasm/objfmt.h>
#include <libyasm/optimizer.h>
#include <libyasm/parser.h>
#include <libyasm/preproc.h>

#include <libyasm/file.h>

#ifdef YASM_INTERNAL
#ifdef YASM_BC_INTERNAL
#include <libyasm/bc-int.h>
#endif
#ifdef YASM_EXPR_INTERNAL
#include <libyasm/expr-int.h>
#endif
#include <libyasm/hamt.h>
#include <libyasm/bitvect.h>
#endif

#endif
