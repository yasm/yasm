/* $Id$
 * YASM module loader header file
 *
 *  Copyright (C) 2002  Peter Johnson
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
#ifndef YASM_MODULE_H
#define YASM_MODULE_H

typedef enum module_type {
    MODULE_ARCH = 0,
    MODULE_DBGFMT,
    MODULE_OBJFMT,
    MODULE_LISTFMT,
    MODULE_OPTIMIZER,
    MODULE_PARSER,
    MODULE_PREPROC
} module_type;

void unload_modules(void);
/*@dependent@*/ /*@null@*/ void *get_module_data
    (module_type type, const char *keyword, const char *symbol);

#define load_arch_module(keyword)	\
    get_module_data(MODULE_ARCH, keyword, "arch")
#define load_dbgfmt_module(keyword)	\
    get_module_data(MODULE_DBGFMT, keyword, "dbgfmt")
#define load_objfmt_module(keyword)	\
    get_module_data(MODULE_OBJFMT, keyword, "objfmt")
#define load_listfmt_module(keyword)	\
    get_module_data(MODULE_LISTFMT, keyword, "listfmt")
#define load_optimizer_module(keyword)	\
    get_module_data(MODULE_OPTIMIZER, keyword, "optimizer")
#define load_parser_module(keyword)	\
    get_module_data(MODULE_PARSER, keyword, "parser")
#define load_preproc_module(keyword)	\
    get_module_data(MODULE_PREPROC, keyword, "preproc")

void list_modules(module_type type,
		  void (*printfunc) (const char *name, const char *keyword));

#define list_archs(func)	list_modules(MODULE_ARCH, func)
#define list_dbgfmts(func)	list_modules(MODULE_DBGFMT, func)
#define list_objfmts(func)	list_modules(MODULE_OBJFMT, func)
#define list_listfmts(func)	list_modules(MODULE_LISTFMT, func)
#define list_optimizers(func)	list_modules(MODULE_OPTIMIZER, func)
#define list_parsers(func)	list_modules(MODULE_PARSER, func)
#define list_preprocs(func)	list_modules(MODULE_PREPROC, func)

#endif
