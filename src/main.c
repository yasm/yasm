/* $IdPath$
 * Program entry point, command line parsing
 *
 *  Copyright (C) 2001  Peter Johnson
 *
 *  This file is part of YASM.
 *
 *  YASM is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  YASM is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "util.h"

#include <stdio.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#endif

#include <libintl.h>
#define _(String)	gettext(String)
#ifdef gettext_noop
#define N_(String)	gettext_noop(String)
#else
#define N_(String)	(String)
#endif

#include "bitvect.h"

#include "globals.h"
#include "options.h"
#include "errwarn.h"
#include "symrec.h"

#include "bytecode.h"
#include "section.h"
#include "objfmt.h"
#include "preproc.h"
#include "parser.h"

RCSID("$IdPath$");

#ifndef countof
#define countof(x,y)	(sizeof(x)/sizeof(y))
#endif

static int files_open = 0;
static FILE *in;

/* Forward declarations: cmd line parser handlers */
int opt_option_handler(char *cmd, char *param, int extra);
int opt_format_handler(char *cmd, char *param, int extra);
/* Fake handlers: remove them */
int boo_boo_handler(char *cmd, char *param, int extra);
int b_handler(char *cmd, char *param, int extra);

/* values for asm_options */
#define OPT_SHOW_HELP 0x01

/* command line options */
opt_option options[] =
{
    { 'h', "help",    0, opt_option_handler, OPT_SHOW_HELP, "show help text" },
    { 'f', "oformat", 1, opt_format_handler, 0, "select output format", "<format>" },
    /* Fake handlers: remove them */
    { 'b', NULL,      0, b_handler, 0, "says boom!" },
    {  0,  "boo-boo", 0, boo_boo_handler, 0, "says boo-boo!" },
};

/* help messages */
char help_head[] = "yasm version " VERSION " compiled " __DATE__ "\n"
		   "copyright (c) 2001 Peter Johnson and " PACKAGE " developers\n"
		   "mailto: asm-devel@bilogic.org\n"
		   "\n"
		   "usage: yasm [options|files]+\n"
		   "where options are:\n";
char help_tail[] = "\n"
		   "   files are asm sources to be assembled\n"
		   "\n"
		   "sample invocation:\n"
		   "   yasm -b --boo-boo -f elf -o test.o impl.asm\n"
		   "\n";

/* main function */
int
main(int argc, char *argv[])
{
    sectionhead *sections;

    if (parse_cmdline(argc, argv, options, countof(options, opt_option)))
	return EXIT_FAILURE;

    if (asm_options & OPT_SHOW_HELP) {
	help_msg(help_head, help_tail, options, countof(options, opt_option));
	return EXIT_FAILURE;
    }

    /* Initialize BitVector (needed for floating point). */
    if (BitVector_Boot() != ErrCode_Ok) {
	ErrorNow(_("Could not initialize BitVector"));
	return EXIT_FAILURE;
    }

    /* if no files were specified, fallback to reading stdin */
    if (!files_open)
    {
	in = stdin;
	in_filename = xstrdup("<STDIN>");
    }

    /* Get initial BITS setting from object format */
    mode_bits = dbg_objfmt.default_mode_bits;

    sections = nasm_parser.do_parse(&nasm_parser, &dbg_objfmt, in);

    if (OutputAllErrorWarning() > 0)
	return EXIT_FAILURE;

    sections_print(sections);
    printf("\n***Symbol Table***\n");
    symrec_foreach((int (*) (symrec *))symrec_print);

    symrec_parser_finalize();
    sections_parser_finalize(sections);

    printf("Post-parser-finalization:\n");
    sections_print(sections);

    if (in_filename)
	free(in_filename);
    return EXIT_SUCCESS;
}

/*
 *  Command line options handlers
 */
int
not_an_option_handler(char *param)
{
    if (files_open > 0) {
	WarningNow("can open only one input file, only latest file will be processed");
	free(in_filename);
	fclose(in);
    }

    in = fopen(param, "rt");
    if (!in) {
	ErrorNow(_("could not open file `%s'"), param);
	return 1;
    }
    in_filename = xstrdup(param);

    files_open++;
    return 0;
}

int
opt_option_handler(char *cmd, char *param, int extra)
{
    asm_options |= extra;
    return 0;
}

int
opt_format_handler(char *cmd, char *param, int extra)
{
    printf("selected format: %s\n", param);
    return 0;
}

/* Fake handlers: remove them */
int
boo_boo_handler(char *cmd, char *param, int extra)
{
    printf("boo-boo!\n");
    return 0;
}

int
b_handler(char *cmd, char *param, int extra)
{
    fprintf(stdout, "boom!\n");
    return 0;
}
