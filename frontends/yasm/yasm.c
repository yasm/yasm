/*
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
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#include "bitvect.h"
#include "file.h"

#include "globals.h"
#include "options.h"
#include "errwarn.h"
#include "intnum.h"
#include "floatnum.h"
#include "symrec.h"

#include "bytecode.h"
#include "section.h"
#include "objfmt.h"
#include "preproc.h"
#include "parser.h"
#include "optimizer.h"

#include "arch.h"


#ifndef countof
#define countof(x,y)	(sizeof(x)/sizeof(y))
#endif

static int files_open = 0;
/*@null@*/ /*@only@*/ static char *obj_filename = NULL, *in_filename = NULL;
/*@null@*/ static FILE *in = NULL, *obj = NULL;

/* Forward declarations: cmd line parser handlers */
static int opt_option_handler(char *cmd, /*@null@*/ char *param, int extra);
static int opt_format_handler(char *cmd, /*@null@*/ char *param, int extra);
static int opt_objfile_handler(char *cmd, /*@null@*/ char *param, int extra);
static int opt_warning_handler(char *cmd, /*@null@*/ char *param, int extra);
/* Fake handlers: remove them */
static int boo_boo_handler(char *cmd, /*@null@*/ char *param, int extra);
static int b_handler(char *cmd, /*@null@*/ char *param, int extra);

/* values for asm_options */
#define OPT_SHOW_HELP 0x01

/* command line options */
static opt_option options[] =
{
    { 'h', "help",    0, opt_option_handler, OPT_SHOW_HELP, "show help text", NULL },
    { 'f', "oformat", 1, opt_format_handler, 0, "select output format", "<format>" },
    { 'o', "objfile", 1, opt_objfile_handler, 0, "name of object-file output", "<filename>" },
    { 'w', NULL,      0, opt_warning_handler, 1, "inhibits warning messages", NULL },
    { 'W', NULL,      0, opt_warning_handler, 0, "enables/disables warning", NULL },
    /* Fake handlers: remove them */
    { 'b', NULL,      0, b_handler, 0, "says boom!", NULL },
    {  0,  "boo-boo", 0, boo_boo_handler, 0, "says boo-boo!", NULL },
};

/* help messages */
static const char *help_head =
    "yasm version " VERSION " compiled " __DATE__ "\n"
    "copyright (c) 2001 Peter Johnson and " PACKAGE " developers\n"
    "mailto: asm-devel@bilogic.org\n"
    "\n"
    "usage: yasm [options|files]+\n"
    "where options are:\n";
static const char *help_tail =
    "\n"
    "   files are asm sources to be assembled\n"
    "\n"
    "sample invocation:\n"
    "   yasm -b --boo-boo -f elf -o test.o impl.asm\n"
    "\n";

/* main function */
/*@-globstate -unrecog@*/
int
main(int argc, char *argv[])
{
    sectionhead *sections;

#if defined(HAVE_SETLOCALE) && defined(HAVE_LC_MESSAGES)
    setlocale(LC_MESSAGES, "");
#endif
#if defined(LOCALEDIR)
    bindtextdomain(PACKAGE, LOCALEDIR);
#endif
    textdomain(PACKAGE);

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
    if (!in || !in_filename) {
	in = stdin;
	if (in_filename)
	    xfree(in_filename);
	in_filename = xstrdup("<STDIN>");
	if (!obj)
	    obj = stdout;
    }

    /* Initialize line info */
    line_set(in_filename, 1, 1);

    /* Set x86 as the architecture */
    cur_arch = &x86_arch;

    /* Set dbg as the object format */
    cur_objfmt = find_objfmt("dbg");
    if (!cur_objfmt) {
	ErrorNow(_("unrecognized output format `%s'"), "dbg");
	return EXIT_FAILURE;
    }

    /* open the object file if not specified */
    if (!obj) {
	/* build the object filename */
	if (obj_filename)
	    xfree(obj_filename);
	assert(in_filename != NULL);
	/* replace (or add) extension */
	obj_filename = replace_extension(in_filename, cur_objfmt->extension,
					 "yasm.out");

	/* open the built filename */
	obj = fopen(obj_filename, "wb");
	if (!obj) {
	    ErrorNow(_("could not open file `%s'"), obj_filename);
	    return EXIT_FAILURE;
	}
    }

    /* Initialize the object format */
    cur_objfmt->initialize(obj);

    /* Set NASM as the parser */
    cur_parser = find_parser("nasm");
    if (!cur_parser) {
	ErrorNow(_("unrecognized parser `%s'"), "nasm");
	return EXIT_FAILURE;
    }

    /* Get initial BITS setting from object format */
    x86_mode_bits = cur_objfmt->default_mode_bits;

    sections = cur_parser->do_parse(cur_parser, in);

    if (in != stdin)
	fclose(in);
    if (in_filename)
	xfree(in_filename);

    if (OutputAllErrorWarning() > 0) {
	sections_delete(sections);
	symrec_delete_all();
	line_shutdown();
	floatnum_shutdown();
	intnum_shutdown();
	BitVector_Shutdown();
	return EXIT_FAILURE;
    }

    /* XXX  Only for temporary debugging! */
    fprintf(obj, "\nSections after parsing:\n");
    indent_level++;
    sections_print(obj, sections);
    indent_level--;

    fprintf(obj, "\nSymbol Table:\n");
    indent_level++;
    symrec_print_all(obj);
    indent_level--;

    symrec_parser_finalize();
    basic_optimizer.optimize(sections);

    fprintf(obj, "\nSections after optimization:\n");
    indent_level++;
    sections_print(obj, sections);
    indent_level--;

    /* Finalize the object output */
    cur_objfmt->finalize();

    if (obj != stdout)
	fclose(obj);

    if (obj_filename)
	xfree(obj_filename);

    sections_delete(sections);
    symrec_delete_all();
    line_shutdown();

    floatnum_shutdown();
    intnum_shutdown();

    BitVector_Shutdown();
    return EXIT_SUCCESS;
}
/*@=globstate =unrecog@*/

/*
 *  Command line options handlers
 */
int
not_an_option_handler(char *param)
{
    if (in) {
	WarningNow("can open only one input file, only latest file will be processed");
	if (fclose(in))
	    ErrorNow("could not close old input file");
    }

    in = fopen(param, "rt");
    if (!in) {
	ErrorNow(_("could not open file `%s'"), param);
	return 1;
    }
    if (in_filename)
	xfree(in_filename);
    in_filename = xstrdup(param);

    files_open++;
    return 0;
}

static int
opt_option_handler(/*@unused@*/ char *cmd, /*@unused@*/ char *param, int extra)
{
    asm_options |= extra;
    return 0;
}

static int
opt_format_handler(/*@unused@*/ char *cmd, char *param, /*@unused@*/ int extra)
{
    assert(param != NULL);
    printf("selected format: %s\n", param);
    return 0;
}

static int
opt_objfile_handler(/*@unused@*/ char *cmd, char *param,
		    /*@unused@*/ int extra)
{
    assert(param != NULL);
    if (strcasecmp(param, "stdout") == 0)
	obj = stdout;
    else if (strcasecmp(param, "stderr") == 0)
	obj = stderr;
    else {
	if (obj) {
	    WarningNow("can open only one output file, last specified used");
	    if (obj != stdout && obj != stderr && fclose(obj))
		ErrorNow("could not close old output file");
	}
    }

    obj = fopen(param, "wb");
    if (!obj) {
	ErrorNow(_("could not open file `%s'"), param);
	return 1;
    }

    if (obj_filename)
	xfree(obj_filename);
    obj_filename = xstrdup(param);

    return 0;
}

static int
opt_warning_handler(char *cmd, /*@unused@*/ char *param, int extra)
{
    int enable = 1;	/* is it disabling the warning instead of enabling? */

    if (extra == 1) {
	/* -w, disable warnings */
	warnings_disabled = 1;
	return 0;
    }

    /* skip past 'W' */
    cmd++;

    /* detect no- prefix to disable the warning */
    if (cmd[0] == 'n' && cmd[1] == 'o' && cmd[2] == '-') {
	enable = 0;
	cmd += 3;   /* skip past it to get to the warning name */
    }

    if (cmd[0] == '\0')
	/* just -W or -Wno-, so definitely not valid */
	return 1;
    else if (strcmp(cmd, "error") == 0) {
	warning_error = enable;
    } else if (strcmp(cmd, "unrecognized-char") == 0) {
	WARN_ENABLE(WARN_UNRECOGNIZED_CHAR, enable);
    } else
	return 1;

    return 0;
}

/* Fake handlers: remove them */
static int
boo_boo_handler(/*@unused@*/ char *cmd, /*@unused@*/ char *param,
		/*@unused@*/ int extra)
{
    printf("boo-boo!\n");
    return 0;
}

static int
b_handler(/*@unused@*/ char *cmd, /*@unused@*/ char *param,
	  /*@unused@*/ int extra)
{
    fprintf(stdout, "boom!\n");
    return 0;
}
