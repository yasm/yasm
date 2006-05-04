/*
 * GAS-compatible parser
 *
 *  Copyright (C) 2005  Peter Johnson
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
#include <util.h>
/*@unused@*/ RCSID("$Id$");

#define YASM_LIB_INTERNAL
#include <libyasm.h>

#include "gas-parser.h"


static void
gas_parser_do_parse(yasm_object *object, yasm_preproc *pp, yasm_arch *a,
		    yasm_objfmt *of, yasm_dbgfmt *df, FILE *f,
		    const char *in_filename, int save_input,
		    yasm_section *def_sect, yasm_errwarns *errwarns)
{
    yasm_parser_gas parser_gas;

    parser_gas.object = object;
    parser_gas.linemap = yasm_object_get_linemap(parser_gas.object);
    parser_gas.symtab = yasm_object_get_symtab(parser_gas.object);

    parser_gas.in = f;

    parser_gas.locallabel_base = (char *)NULL;
    parser_gas.locallabel_base_len = 0;

    parser_gas.preproc = pp;
    parser_gas.arch = a;
    parser_gas.objfmt = of;
    parser_gas.dbgfmt = df;
    parser_gas.errwarns = errwarns;

    parser_gas.cur_section = def_sect;
    parser_gas.prev_bc = yasm_section_bcs_first(def_sect);

    parser_gas.save_input = save_input;
    parser_gas.save_last = 0;

    /* initialize scanner structure */
    parser_gas.s.bot = NULL;
    parser_gas.s.tok = NULL;
    parser_gas.s.ptr = NULL;
    parser_gas.s.cur = NULL;
    parser_gas.s.pos = NULL;
    parser_gas.s.lim = NULL;
    parser_gas.s.top = NULL;
    parser_gas.s.eof = NULL;
    parser_gas.s.tchar = 0;
    parser_gas.s.tline = 0;
    parser_gas.s.cline = 1;

    parser_gas.state = INITIAL;

    parser_gas.rept = NULL;

    /* yacc debugging, needs YYDEBUG set in bison.y.in to work */
    parser_gas.debug = 1;

    gas_parser_parse(&parser_gas);

    /* Check for ending inside a rept */
    if (parser_gas.rept) {
	yasm_error_set(YASM_ERROR_SYNTAX, N_("rept without matching endr"));
	yasm_errwarn_propagate(errwarns, parser_gas.rept->startline);
    }

    gas_parser_cleanup(&parser_gas);

    /* Free locallabel base if necessary */
    if (parser_gas.locallabel_base)
	yasm_xfree(parser_gas.locallabel_base);
}

/* Define valid preprocessors to use with this parser */
static const char *gas_parser_preproc_keywords[] = {
    "raw",
    NULL
};

/* Define parser structure -- see parser.h for details */
yasm_parser_module yasm_gas_LTX_parser = {
    "GNU AS (GAS)-compatible parser",
    "gas",
    gas_parser_preproc_keywords,
    "raw",
    gas_parser_do_parse
};
