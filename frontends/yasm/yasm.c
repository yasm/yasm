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

#ifdef STDC_HEADERS
# include <assert.h>
#endif

#ifdef gettext_noop
#define N_(String)	gettext_noop(String)
#else
#define N_(String)	(String)
#endif

#include "bitvect.h"

#include "globals.h"
#include "options.h"
#include "errwarn.h"
#include "floatnum.h"
#include "symrec.h"

#include "bytecode.h"
#include "section.h"
#include "objfmt.h"
#include "preproc.h"
#include "parser.h"

#include "arch.h"


#ifndef countof
#define countof(x,y)	(sizeof(x)/sizeof(y))
#endif

static int files_open = 0;
/*@null@*/ /*@only@*/ static char *obj_filename = NULL;
/*@null@*/ static FILE *in = NULL, *obj = NULL;

/* Forward declarations: cmd line parser handlers */
static int opt_option_handler(char *cmd, /*@null@*/ char *param, int extra);
static int opt_format_handler(char *cmd, /*@null@*/ char *param, int extra);
static int opt_objfile_handler(char *cmd, /*@null@*/ char *param, int extra);
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
/*@-globstate@*/
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
    if (!in) {
	in = stdin;
	switch_filename("<STDIN>");
    }

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
	/* TODO: replace the existing extension; this just appends. */
	if (obj_filename)
	    xfree(obj_filename);
	assert(in_filename != NULL);
	obj_filename = xmalloc(strlen(in_filename)+
			       strlen(cur_objfmt->extension)+2);
	sprintf(obj_filename, "%s.%s", in_filename, cur_objfmt->extension);

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

    fclose(in);

    if (OutputAllErrorWarning() > 0) {
	sections_delete(sections);
	symrec_delete_all();
	filename_delete_all();
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
    sections_parser_finalize(sections);

    fprintf(obj, "\nSections after post-parser-finalization:\n");
    indent_level++;
    sections_print(obj, sections);
    indent_level--;

    /* Finalize the object output */
    cur_objfmt->finalize();

    fclose(obj);
    if (obj_filename)
	xfree(obj_filename);

    sections_delete(sections);
    symrec_delete_all();
    filename_delete_all();

    floatnum_shutdown();

    BitVector_Shutdown();
    return EXIT_SUCCESS;
}
/*@=globstate@*/

/*
 *  Command line options handlers
 */
int
not_an_option_handler(char *param)
{
    if (in) {
	WarningNow("can open only one input file, only latest file will be processed");
	if(fclose(in))
	    ErrorNow("could not close old input file");
    }

    in = fopen(param, "rt");
    if (!in) {
	ErrorNow(_("could not open file `%s'"), param);
	return 1;
    }
    switch_filename(param);

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
