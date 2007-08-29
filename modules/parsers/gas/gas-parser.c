/*
 * GAS-compatible parser
 *
 *  Copyright (C) 2005-2007  Peter Johnson
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

#include <libyasm.h>

#include "gas-parser.h"


static void
gas_parser_do_parse(yasm_object *object, yasm_preproc *pp, FILE *f,
                    int save_input, yasm_linemap *linemap,
                    yasm_errwarns *errwarns)
{
    yasm_parser_gas parser_gas;
    int i;

    parser_gas.object = object;
    parser_gas.linemap = linemap;

    parser_gas.in = f;

    parser_gas.locallabel_base = (char *)NULL;
    parser_gas.locallabel_base_len = 0;

    parser_gas.dir_fileline = 0;
    parser_gas.dir_file = NULL;
    parser_gas.dir_line = 0;

    parser_gas.preproc = pp;
    parser_gas.errwarns = errwarns;

    parser_gas.prev_bc = yasm_section_bcs_first(object->cur_section);

    parser_gas.save_input = save_input;
    parser_gas.save_last = 0;

    parser_gas.peek_token = NONE;

    /* initialize scanner structure */
    yasm_scanner_initialize(&parser_gas.s);

    parser_gas.state = INITIAL;

    parser_gas.rept = NULL;

    for (i=0; i<10; i++)
        parser_gas.local[i] = 0;

    /* yacc debugging, needs YYDEBUG set in bison.y.in to work */
    parser_gas.debug = 1;

    gas_parser_parse(&parser_gas);

    /* Check for ending inside a rept */
    if (parser_gas.rept) {
        yasm_error_set(YASM_ERROR_SYNTAX, N_("rept without matching endr"));
        yasm_errwarn_propagate(errwarns, parser_gas.rept->startline);
    }

    /* Check for ending inside a comment */
    if (parser_gas.state == COMMENT) {
        yasm_warn_set(YASM_WARN_GENERAL, N_("end of file in comment"));
        /* XXX: Minus two to compensate for already having moved past the EOF
         * in the linemap.
         */
        yasm_errwarn_propagate(errwarns,
                               yasm_linemap_get_current(parser_gas.linemap)-2);
    }

    yasm_scanner_delete(&parser_gas.s);

    /* Free locallabel base if necessary */
    if (parser_gas.locallabel_base)
        yasm_xfree(parser_gas.locallabel_base);

    if (parser_gas.dir_file)
        yasm_xfree(parser_gas.dir_file);

    /* Convert all undefined symbols into extern symbols */
    yasm_symtab_parser_finalize(object->symtab, 1, errwarns);
}

/* Define valid preprocessors to use with this parser */
static const char *gas_parser_preproc_keywords[] = {
    "raw",
    "cpp",
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
yasm_parser_module yasm_gnu_LTX_parser = {
    "GNU AS (GAS)-compatible parser",
    "gnu",
    gas_parser_preproc_keywords,
    "raw",
    gas_parser_do_parse
};
