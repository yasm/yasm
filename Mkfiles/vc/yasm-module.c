/*
 * YASM module loader for Win32
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
#include <util.h>
/*@unused@*/ RCSID("$IdPath$");

#include <libyasm.h>

#include "yasm-module.h"


typedef struct module {
    module_type type;		    /* module type */
    const char *keyword;	    /* module keyword */
    const char *symbol;		    /* module symbol */
    void *data;			    /* associated data */
} module;

extern yasm_arch_module yasm_x86_LTX_arch;
extern yasm_arch_module yasm_lc3b_LTX_arch;
extern yasm_dbgfmt_module yasm_null_LTX_dbgfmt;
extern yasm_objfmt_module yasm_bin_LTX_objfmt;
extern yasm_objfmt_module yasm_coff_LTX_objfmt;
extern yasm_objfmt_module yasm_win32_LTX_objfmt;
extern yasm_objfmt_module yasm_dbg_LTX_objfmt;
extern yasm_objfmt_module yasm_elf_LTX_objfmt;
extern yasm_optimizer_module yasm_basic_LTX_optimizer;
extern yasm_parser_module yasm_nasm_LTX_parser;
extern yasm_preproc_module yasm_nasm_LTX_preproc;
extern yasm_preproc_module yasm_raw_LTX_preproc;
extern yasm_preproc_module yasm_yapp_LTX_preproc;

static module modules[] = {
    {MODULE_ARCH, "x86", "arch", &yasm_x86_LTX_arch},
    {MODULE_ARCH, "lc3b", "arch", &yasm_lc3b_LTX_arch},
    {MODULE_DBGFMT, "null", "dbgfmt", &yasm_null_LTX_dbgfmt},
    {MODULE_OBJFMT, "bin",  "objfmt", &yasm_bin_LTX_objfmt},
    {MODULE_OBJFMT, "coff", "objfmt", &yasm_coff_LTX_objfmt},
    {MODULE_OBJFMT, "dbg", "objfmt", &yasm_dbg_LTX_objfmt},
    {MODULE_OBJFMT, "win32", "objfmt", &yasm_win32_LTX_objfmt},
    {MODULE_OBJFMT, "elf", "objfmt", &yasm_elf_LTX_objfmt},
    {MODULE_OPTIMIZER, "basic", "optimizer", &yasm_basic_LTX_optimizer},
    {MODULE_PARSER, "nasm", "parser", &yasm_nasm_LTX_parser},
    {MODULE_PREPROC, "nasm", "preproc", &yasm_nasm_LTX_preproc},
    {MODULE_PREPROC, "raw", "preproc", &yasm_raw_LTX_preproc},
};


static /*@dependent@*/ /*@null@*/ module *
load_module(module_type type, const char *keyword)
{
    int i;

    /* Look for the module. */
    for (i=0; i<sizeof(modules)/sizeof(modules[0]); i++) {
	if (modules[i].type == type &&
	    yasm__strcasecmp(modules[i].keyword, keyword) == 0)
	    return &modules[i];
    }

    return NULL;
}

void
unload_modules(void)
{
}

void *
get_module_data(module_type type, const char *keyword, const char *symbol)
{
    int i;

    /* Look for the module/symbol. */
    for (i=0; i<sizeof(modules)/sizeof(modules[0]); i++) {
	if (modules[i].type == type &&
	    yasm__strcasecmp(modules[i].keyword, keyword) == 0 &&
	    strcmp(modules[i].symbol, symbol) == 0)
	    return modules[i].data;
    }

    return NULL;
}

void
list_modules(module_type type,
	     void (*printfunc) (const char *name, const char *keyword))
{
    int i;
    yasm_arch_module *arch;
    yasm_dbgfmt_module *dbgfmt;
    yasm_objfmt_module *objfmt;
    yasm_optimizer_module *optimizer;
    yasm_parser_module *parser;
    yasm_preproc_module *preproc;

    /* Go through available list, and try to load each one */
    for (i=0; i<sizeof(modules)/sizeof(modules[0]); i++) {
	if (modules[i].type == type) {
	    switch (type) {
		case MODULE_ARCH:
		    arch = modules[i].data;
		    printfunc(arch->name, arch->keyword);
		    break;
		case MODULE_DBGFMT:
		    dbgfmt = modules[i].data;
		    printfunc(dbgfmt->name, dbgfmt->keyword);
		    break;
		case MODULE_OBJFMT:
		    objfmt = modules[i].data;
		    printfunc(objfmt->name, objfmt->keyword);
		    break;
		case MODULE_OPTIMIZER:
		    optimizer = modules[i].data;
		    printfunc(optimizer->name, optimizer->keyword);
		    break;
		case MODULE_PARSER:
		    parser = modules[i].data;
		    printfunc(parser->name, parser->keyword);
		    break;
		case MODULE_PREPROC:
		    preproc = modules[i].data;
		    printfunc(preproc->name, preproc->keyword);
		    break;
	    }
	}
    }
}
