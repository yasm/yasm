/* $IdPath$
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

void unload_modules(void);
/*@dependent@*/ /*@null@*/ void *get_module_data(const char *keyword,
						 const char *symbol);

#define load_arch(keyword)	get_module_data(keyword, "arch")
#define load_dbgfmt(keyword)	get_module_data(keyword, "dbgfmt")
#define load_objfmt(keyword)	get_module_data(keyword, "objfmt")
#define load_optimizer(keyword)	get_module_data(keyword, "optimizer")
#define load_parser(keyword)	get_module_data(keyword, "parser")
#define load_preproc(keyword)	get_module_data(keyword, "preproc")

/* Lists all available object formats.  Calls printfunc with the name and
 * keyword of each available format.
 */
void list_objfmts(void (*printfunc) (const char *name, const char *keyword));

/* Lists all available parsers.  Calls printfunc with the name and keyword
 * of each available parser.
 */
void list_parsers(void (*printfunc) (const char *name, const char *keyword));

void list_preprocs(void (*printfunc) (const char *name, const char *keyword));
void list_dbgfmts(void (*printfunc) (const char *name, const char *keyword));

#endif
