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
#define YASM_LIB_INTERNAL
#include <libyasm.h>
/*@unused@*/ RCSID("$IdPath$");

#include "yasm-module.h"


typedef struct module {
    const char *keyword;	    /* module keyword */
    const char *symbol;		    /* module symbol */
    void *data;			    /* associated data */
} module;

extern yasm_arch yasm_x86_LTX_arch;
extern int yasm_x86_LTX_mode_bits;
extern yasm_dbgfmt yasm_null_LTX_dbgfmt;
extern yasm_objfmt yasm_bin_LTX_objfmt;
extern yasm_objfmt yasm_coff_LTX_objfmt;
extern yasm_objfmt yasm_win32_LTX_objfmt;
extern yasm_objfmt yasm_dbg_LTX_objfmt;
extern yasm_optimizer yasm_basic_LTX_optimizer;
extern yasm_parser yasm_nasm_LTX_parser;
extern yasm_preproc yasm_nasm_LTX_preproc;
extern yasm_preproc yasm_raw_LTX_preproc;
extern yasm_preproc yasm_yapp_LTX_preproc;

static module modules[] = {
    {"x86", "arch", &yasm_x86_LTX_arch},
    {"x86", "mode_bits", &yasm_x86_LTX_mode_bits},
    {"null", "dbgfmt", &yasm_null_LTX_dbgfmt},
    {"bin",  "objfmt", &yasm_bin_LTX_objfmt},
    {"coff", "objfmt", &yasm_coff_LTX_objfmt},
    {"win32", "objfmt", &yasm_win32_LTX_objfmt},
    {"dbg", "objfmt", &yasm_dbg_LTX_objfmt},
    {"basic", "optimizer", &yasm_basic_LTX_optimizer},
    {"nasm", "parser", &yasm_nasm_LTX_parser},
    {"nasm", "preproc", &yasm_nasm_LTX_preproc},
    {"raw", "preproc", &yasm_raw_LTX_preproc},
};


static /*@dependent@*/ /*@null@*/ module *
load_module(const char *keyword)
{
    int i;

    /* Look for the module. */
    for (i=0; i<sizeof(modules)/sizeof(modules[0]); i++) {
	if (yasm__strcasecmp(modules[i].keyword, keyword) == 0)
	    return &modules[i];
    }

    return NULL;
}

void
unload_modules(void)
{
}

void *
get_module_data(const char *keyword, const char *symbol)
{
    int i;

    /* Look for the module/symbol. */
    for (i=0; i<sizeof(modules)/sizeof(modules[0]); i++) {
	if (yasm__strcasecmp(modules[i].keyword, keyword) == 0 &&
	    strcmp(modules[i].symbol, symbol) == 0)
	    return modules[i].data;
    }

    return NULL;
}

void
list_objfmts(void (*printfunc) (const char *name, const char *keyword))
{
    int i;

    /* Go through available list, and try to load each one */
    for (i=0; i<sizeof(modules)/sizeof(modules[0]); i++) {
	if (strcmp(modules[i].symbol, "objfmt") == 0) {
	    yasm_objfmt *of = modules[i].data;
	    printfunc(of->name, of->keyword);
	}
    }
}

void
list_parsers(void (*printfunc) (const char *name, const char *keyword))
{
    int i;

    /* Go through available list, and try to load each one */
    for (i=0; i<sizeof(modules)/sizeof(modules[0]); i++) {
	if (strcmp(modules[i].symbol, "parser") == 0) {
	    yasm_parser *p = modules[i].data;
	    printfunc(p->name, p->keyword);
	}
    }
}
