/* $Id$
 *
 * Generate x86 instructions gperf files
 *
 *  Copyright (C) 2006-2007  Peter Johnson
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
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

static char line[1024];
static unsigned int cur_line = 1;
unsigned int num_errors = 0;

static void
report_error(const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "%u: ", cur_line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    num_errors++;
}

static void
parse_insn(FILE *out_nasm, FILE *out_gas)
{
    char *parser = strtok(NULL, " \t\n");
    char *bname = strtok(NULL, " \t\n");
    char *suffix = strtok(NULL, " \t\n");
    char *group = strtok(NULL, " \t\n");
    char *mod = strtok(NULL, " \t\n");
    char *cpu = strtok(NULL, " \t\n");

    if (!suffix) {
        report_error("INSN requires suffix");
        return;
    }

    if (suffix[0] != '"') {
        /* Just one instruction to generate */
        if (parser[0] == '-' || strcmp(parser, "gas") == 0)
            fprintf(out_gas,
                    "%s,\t%s_insn,\t(%sUL<<8)|NELEMS(%s_insn),\t%s,\t%s\n",
                    bname, group, mod, group, cpu, suffix);
        if (parser[0] == '-' || strcmp(parser, "nasm") == 0)
            fprintf(out_nasm,
                    "%s,\t%s_insn,\t(%sUL<<8)|NELEMS(%s_insn),\t%s,\t%s\n",
                    bname, group, mod, group, cpu, suffix);
    } else {
        /* Need to generate with suffixes for gas */
        char *p;
        char sufstr[6];
        size_t bnamelen = strlen(bname);
        char *name = malloc(bnamelen+2);

        strcpy(name, bname);
        strcpy(sufstr, "SUF_X");

        for (p = &suffix[1]; *p != '"'; p++) {
            sufstr[4] = toupper(*p);

            name[bnamelen] = tolower(*p);
            name[bnamelen+1] = '\0';

            fprintf(out_gas,
                    "%s,\t%s_insn,\t(%sUL<<8)|NELEMS(%s_insn),\t%s,\t%s\n",
                    name, group, mod, group, cpu, sufstr);
        }
        free(name);

        /* And finally the version sans suffix */
        if (parser[0] == '-' || strcmp(parser, "gas") == 0)
            fprintf(out_gas,
                    "%s,\t%s_insn,\t(%sUL<<8)|NELEMS(%s_insn),\t%s,\t%s\n",
                    bname, group, mod, group, cpu, "NONE");
        if (parser[0] == '-' || strcmp(parser, "nasm") == 0)
            fprintf(out_nasm,
                    "%s,\t%s_insn,\t(%sUL<<8)|NELEMS(%s_insn),\t%s,\t%s\n",
                    bname, group, mod, group, cpu, "NONE");
    }
}

static void
parse_prefix(FILE *out_nasm, FILE *out_gas)
{
    char *parser = strtok(NULL, " \t\n");
    char *name = strtok(NULL, " \t\n");
    char *type = strtok(NULL, " \t\n");
    char *value = strtok(NULL, " \t\n");

    if (parser[0] == '-' || strcmp(parser, "gas") == 0)
        fprintf(out_gas, "%s,\tNULL,\t%s,\t%s,\tNONE\n", name, type, value);
    if (parser[0] == '-' || strcmp(parser, "nasm") == 0)
        fprintf(out_nasm, "%s,\tNULL,\t%s,\t%s,\tNONE\n", name, type, value);
}

void
output_base(FILE *out, const char *parser)
{
    fputs("%ignore-case\n", out);
    fputs("%language=ANSI-C\n", out);
    fputs("%compare-strncmp\n", out);
    fputs("%readonly-tables\n", out);
    fputs("%enum\n", out);
    fputs("%struct-type\n", out);
    fprintf(out, "%%define hash-function-name insnprefix_%s_hash\n", parser);
    fprintf(out, "%%define lookup-function-name insnprefix_%s_find\n", parser);
    fputs("struct insnprefix_parse_data;\n", out);
    fputs("%%\n", out);
}

int
main(int argc, char *argv[])
{
    FILE *in, *out_nasm, *out_gas;
    char *tok;

    if (argc != 4) {
        fprintf(stderr, "Usage: x86geninsn <in> <out_nasm> <out_gas>\n");
        return EXIT_FAILURE;
    }

    in = fopen(argv[1], "rt");
    if (!in) {
        fprintf(stderr, "Could not open `%s' for reading\n", argv[1]);
        return EXIT_FAILURE;
    }

    out_nasm = fopen(argv[2], "wt");
    if (!out_nasm) {
        fprintf(stderr, "Could not open `%s' for writing\n", argv[2]);
        return EXIT_FAILURE;
    }
    out_gas = fopen(argv[3], "wt");
    if (!out_gas) {
        fprintf(stderr, "Could not open `%s' for writing\n", argv[2]);
        return EXIT_FAILURE;
    }

    output_base(out_nasm, "nasm");
    output_base(out_gas, "gas");

    /* Parse input file */
    while (fgets(line, 1024, in)) {
        tok = strtok(line, " \t\n");
        if (!tok)
            continue;

        /* Comments start with # as the first thing on a line */
        if (tok[0] == '#')
            continue;

        if (strcmp(tok, "INSN") == 0)
            parse_insn(out_nasm, out_gas);
        else if (strcmp(tok, "PREFIX") == 0)
            parse_prefix(out_nasm, out_gas);
        else
            report_error("unknown directive `%s'\n", tok);
        cur_line++;
    }

    if (num_errors > 0)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

