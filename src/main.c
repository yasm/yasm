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
static int special_options = 0;

static int open_obj(void);
static void cleanup(sectionhead *sections);

/* Forward declarations: cmd line parser handlers */
static int opt_special_handler(char *cmd, /*@null@*/ char *param, int extra);
static int opt_format_handler(char *cmd, /*@null@*/ char *param, int extra);
static int opt_objfile_handler(char *cmd, /*@null@*/ char *param, int extra);
static int opt_warning_handler(char *cmd, /*@null@*/ char *param, int extra);
/* Fake handlers: remove them */
static int boo_boo_handler(char *cmd, /*@null@*/ char *param, int extra);
static int b_handler(char *cmd, /*@null@*/ char *param, int extra);

/* values for special_options */
#define SPECIAL_SHOW_HELP 0x01
#define SPECIAL_SHOW_VERSION 0x02

/* command line options */
static opt_option options[] =
{
    {  0,  "version", 0, opt_special_handler, SPECIAL_SHOW_VERSION, N_("show version text"), NULL },
    { 'h', "help",    0, opt_special_handler, SPECIAL_SHOW_HELP, N_("show help text"), NULL },
    { 'f', "oformat", 1, opt_format_handler, 0, N_("select output format"), N_("<format>") },
    { 'o', "objfile", 1, opt_objfile_handler, 0, N_("name of object-file output"), N_("<filename>") },
    { 'w', NULL,      0, opt_warning_handler, 1, N_("inhibits warning messages"), NULL },
    { 'W', NULL,      0, opt_warning_handler, 0, N_("enables/disables warning"), NULL },
    /* Fake handlers: remove them */
    { 'b', NULL,      0, b_handler, 0, "says boom!", NULL },
    {  0,  "boo-boo", 0, boo_boo_handler, 0, "says boo-boo!", NULL },
};

/* version message */
static const char version_msg[] = N_(
    "yasm " VERSION "\n"
    "Copyright (c) 2001-2002 Peter Johnson and other " PACKAGE " developers\n"
    "This program is free software; you may redistribute it under the terms\n"
    "of the GNU General Public License.  Portions of this program are\n"
    "licensed under the GNU Lesser General Public License or the 3-clause\n"
    "BSD license; see individual file comments for details.  This program\n"
    "has absolutely no warranty; not even for merchantability or fitness for\n"
    "a particular purpose.\n"
    "Compiled on " __DATE__ ".\n");

/* help messages */
static const char help_head[] = N_(
    "usage: yasm [options|files]+\n"
    "Options:\n");
static const char help_tail[] = N_(
    "\n"
    "Files are asm sources to be assembled.\n"
    "\n"
    "Sample invocation:\n"
    "   yasm -f elf -o object.o source.asm\n"
    "\n"
    "Report bugs to bug-yasm@tortall.net\n");

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

    switch (special_options) {
	case SPECIAL_SHOW_HELP:
	    /* Does gettext calls internally */
	    help_msg(help_head, help_tail, options,
		     countof(options, opt_option));
	    return EXIT_SUCCESS;
	case SPECIAL_SHOW_VERSION:
	    printf("%s", gettext(version_msg));
	    return EXIT_SUCCESS;
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

    /* If not already specified, default to dbg as the object format. */
    if (!cur_objfmt)
	cur_objfmt = find_objfmt("dbg");
    assert(cur_objfmt != NULL);

    /* open the object file if not specified */
    if (!obj) {
	/* build the object filename */
	if (obj_filename)
	    xfree(obj_filename);
	assert(in_filename != NULL);
	/* replace (or add) extension */
	obj_filename = replace_extension(in_filename, cur_objfmt->extension,
					 "yasm.out");
    }

    /* Pre-open the object file as debug_file if we're using a debug-type
     * format.  (This is so the format can output ALL function call info).
     */
    if (strcmp(cur_objfmt->keyword, "dbg") == 0) {
	if (!open_obj())
	    return EXIT_FAILURE;
	debug_file = obj;
    }

    /* Initialize the object format */
    cur_objfmt->initialize(in_filename, obj_filename);

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
	cleanup(sections);
	return EXIT_FAILURE;
    }

    symrec_parser_finalize();
    basic_optimizer.optimize(sections);

    if (OutputAllErrorWarning() > 0) {
	cleanup(sections);
	return EXIT_FAILURE;
    }

    /* open the object file for output (if not already opened above) */
    if (!debug_file) {
	if (!open_obj())
	    return EXIT_FAILURE;
    }

    /* Write the object file */
    cur_objfmt->output(obj, sections);

    /* Finalize the object output */
    cur_objfmt->cleanup();

    if (obj != stdout)
	fclose(obj);

    /* If we had an error at this point, we also need to delete the output
     * object file (to make sure it's not left newer than the source).
     */
    if (OutputAllErrorWarning() > 0) {
	cleanup(sections);
	if (obj != stdout)
	    remove(obj_filename);
	return EXIT_FAILURE;
    }

    if (obj_filename)
	xfree(obj_filename);

    cleanup(sections);
    return EXIT_SUCCESS;
}
/*@=globstate =unrecog@*/

/* Open the object file.  Returns 0 on failure. */
static int
open_obj(void)
{
    if (obj != stdout) {
	obj = fopen(obj_filename, "wb");
	if (!obj) {
	    ErrorNow(_("could not open file `%s'"), obj_filename);
	    return 0;
	}
    }
    return 1;
}

/* Cleans up all allocated structures. */
static void
cleanup(sectionhead *sections)
{
    sections_delete(sections);
    symrec_delete_all();
    line_shutdown();

    floatnum_shutdown();
    intnum_shutdown();

    BitVector_Shutdown();
}

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
opt_special_handler(/*@unused@*/ char *cmd, /*@unused@*/ char *param, int extra)
{
    if (special_options == 0)
	special_options = extra;
    return 0;
}

static int
opt_format_handler(/*@unused@*/ char *cmd, char *param, /*@unused@*/ int extra)
{
    assert(param != NULL);
    cur_objfmt = find_objfmt(param);
    if (!cur_objfmt) {
	ErrorNow(_("unrecognized object format `%s'"), param);
	return 1;
    }
    return 0;
}

static int
opt_objfile_handler(/*@unused@*/ char *cmd, char *param,
		    /*@unused@*/ int extra)
{
    assert(param != NULL);
    if (strcmp(param, "-") == 0)
	obj = stdout;
    else {
	if (obj) {
	    WarningNow("can open only one output file, last specified used");
	    if (obj != stdout && fclose(obj))
		ErrorNow("could not close old output file");
	}
    }

    if (obj != stdout) {
	obj = fopen(param, "wb");
	if (!obj) {
	    ErrorNow(_("could not open file `%s'"), param);
	    return 1;
	}
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
