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

#include "ltdl.h"

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


/* Extra path to search for our modules. */
#ifndef YASM_MODULE_PATH_ENV
# define YASM_MODULE_PATH_ENV	"YASM_MODULE_PATH"
#endif

/* Preprocess-only buffer size */
#define PREPROC_BUF_SIZE    16384

/*@null@*/ /*@only@*/ static char *obj_filename = NULL, *in_filename = NULL;
static int special_options = 0;
/*@null@*/ /*@dependent@*/ static preproc *cur_preproc = NULL;
static int preproc_only = 0;

/*@null@*/ /*@dependent@*/ static FILE *open_obj(void);
static void cleanup(sectionhead *sections);

/* Forward declarations: cmd line parser handlers */
static int opt_special_handler(char *cmd, /*@null@*/ char *param, int extra);
static int opt_parser_handler(char *cmd, /*@null@*/ char *param, int extra);
static int opt_preproc_handler(char *cmd, /*@null@*/ char *param, int extra);
static int opt_objfmt_handler(char *cmd, /*@null@*/ char *param, int extra);
static int opt_objfile_handler(char *cmd, /*@null@*/ char *param, int extra);
static int opt_warning_handler(char *cmd, /*@null@*/ char *param, int extra);
static int preproc_only_handler(char *cmd, /*@null@*/ char *param, int extra);

/* values for special_options */
#define SPECIAL_SHOW_HELP 0x01
#define SPECIAL_SHOW_VERSION 0x02

/* command line options */
static opt_option options[] =
{
    {  0,  "version", 0, opt_special_handler, SPECIAL_SHOW_VERSION, N_("show version text"), NULL },
    { 'h', "help",    0, opt_special_handler, SPECIAL_SHOW_HELP, N_("show help text"), NULL },
    { 'p', "parser", 1, opt_parser_handler, 0, N_("select parser"), N_("parser") },
    { 'r', "preproc", 1, opt_preproc_handler, 0, N_("select preprocessor"), N_("preproc") },
    { 'f', "oformat", 1, opt_objfmt_handler, 0, N_("select object format"), N_("format") },
    { 'o', "objfile", 1, opt_objfile_handler, 0, N_("name of object-file output"), N_("filename") },
    { 'w', NULL,      0, opt_warning_handler, 1, N_("inhibits warning messages"), NULL },
    { 'W', NULL,      0, opt_warning_handler, 0, N_("enables/disables warning"), NULL },
    { 'e', "preproc-only", 0, preproc_only_handler, 0, N_("preprocess only (writes output to stdout by default)"), NULL },
};

/* version message */
/*@observer@*/ static const char *version_msg[] = {
    N_("yasm " VERSION "\n"),
    N_("Copyright (c) 2001-2002 Peter Johnson and other " PACKAGE " developers\n"),
    N_("This program is free software; you may redistribute it under the\n"),
    N_("terms of the GNU General Public License.  Portions of this program\n"),
    N_("are licensed under the GNU Lesser General Public License or the\n"),
    N_("3-clause BSD license; see individual file comments for details.\n"),
    N_("This program has absolutely no warranty; not even for\n"),
    N_("merchantibility or fitness for a particular purpose.\n"),
    N_("Compiled on " __DATE__ ".\n"),
};

/* help messages */
/*@observer@*/ static const char help_head[] = N_(
    "usage: yasm [option]* file\n"
    "Options:\n");
/*@observer@*/ static const char help_tail[] = N_(
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
    /*@null@*/ FILE *in = NULL, *obj = NULL;
    sectionhead *sections;
    size_t i;
    int errors;

#if defined(HAVE_SETLOCALE) && defined(HAVE_LC_MESSAGES)
    setlocale(LC_MESSAGES, "");
#endif
#if defined(LOCALEDIR)
    bindtextdomain(PACKAGE, LOCALEDIR);
#endif
    textdomain(PACKAGE);

    /* Set libltdl malloc/free functions. */
#ifdef DMALLOC
    lt_dlmalloc = malloc;
    lt_dlfree = free;
#else
    lt_dlmalloc = xmalloc;
    lt_dlfree = xfree;
#endif

    /* Initialize preloaded symbol lookup table. */
    LTDL_SET_PRELOADED_SYMBOLS();

    /* Initialize libltdl. */
    errors = lt_dlinit();

    /* Set up extra module search directories. */
    if (errors == 0) {
	const char *path = getenv(YASM_MODULE_PATH_ENV);
	if (path)
	    errors = lt_dladdsearchdir(path);
    }
    if (errors != 0) {
	ErrorNow(_("Module loader initialization failed"));
	return EXIT_FAILURE;
    }

    if (parse_cmdline(argc, argv, options, NELEMS(options)))
	return EXIT_FAILURE;

    switch (special_options) {
	case SPECIAL_SHOW_HELP:
	    /* Does gettext calls internally */
	    help_msg(help_head, help_tail, options, NELEMS(options));
	    return EXIT_SUCCESS;
	case SPECIAL_SHOW_VERSION:
	    for (i=0; i<sizeof(version_msg)/sizeof(char *); i++)
		printf("%s", gettext(version_msg[i]));
	    return EXIT_SUCCESS;
    }

    /* Initialize BitVector (needed for floating point). */
    if (BitVector_Boot() != ErrCode_Ok) {
	ErrorNow(_("Could not initialize BitVector"));
	return EXIT_FAILURE;
    }

    if (in_filename && strcmp(in_filename, "-") != 0) {
	/* Open the input file (if not standard input) */
	in = fopen(in_filename, "rt");
	if (!in) {
	    ErrorNow(_("could not open file `%s'"), in_filename);
	    return EXIT_FAILURE;
	}
    } else {
	/* If no files were specified, fallback to reading stdin */
	in = stdin;
	if (!in_filename)
	    in_filename = xstrdup("-");
	/* Default to stdout if no obj filename specified */
	if (!obj_filename)
	    obj_filename = xstrdup("-");
    }

    /* Initialize line info */
    line_set(in_filename, 1, 1);

    /* Set x86 as the architecture */
    cur_arch = &x86_arch;

    /* If not already specified, default to dbg as the object format. */
    if (!cur_objfmt)
	cur_objfmt = find_objfmt("dbg");
    assert(cur_objfmt != NULL);

    /* handle preproc-only case here */
    if (preproc_only) {
	char *preproc_buf = xmalloc(PREPROC_BUF_SIZE);
	size_t got;

	/* Default output to stdout if not specified */
	if (!obj_filename)
	    obj_filename = xstrdup("-");

	/* Open output (object) file */
	obj = open_obj();
	if (!obj) {
	    xfree(preproc_buf);
	    return EXIT_FAILURE;
	}

	/* If not already specified, default to yapp preproc. */
	if (!cur_preproc)
	    cur_preproc = find_preproc("yapp");
	assert(cur_preproc != NULL);

	/* Pre-process until done */
	cur_preproc->initialize(in, in_filename);
	while ((got = cur_preproc->input(preproc_buf, PREPROC_BUF_SIZE)) != 0)
	    fwrite(preproc_buf, got, 1, obj);

	if (in != stdin)
	    fclose(in);
	xfree(in_filename);

	if (obj != stdout)
	    fclose(obj);

	if (GetNumErrors() > 0) {
	    OutputAllErrorWarning();
	    if (obj != stdout)
		remove(obj_filename);
	    xfree(preproc_buf);
	    return EXIT_FAILURE;
	}
	xfree(obj_filename);
	xfree(preproc_buf);

	return EXIT_SUCCESS;
    }

    /* determine the object filename if not specified or stdout defaulted */
    if (!obj_filename)
	/* replace (or add) extension */
	obj_filename = replace_extension(in_filename, cur_objfmt->extension,
					 "yasm.out");

    /* Pre-open the object file as debug_file if we're using a debug-type
     * format.  (This is so the format can output ALL function call info).
     */
    if (strcmp(cur_objfmt->keyword, "dbg") == 0) {
	obj = open_obj();
	if (!obj)
	    return EXIT_FAILURE;
	debug_file = obj;
    }

    /* Initialize the object format */
    if (cur_objfmt->initialize)
	cur_objfmt->initialize(in_filename, obj_filename);

    /* Set NASM as the parser */
    cur_parser = find_parser("nasm");
    if (!cur_parser) {
	ErrorNow(_("unrecognized parser `%s'"), "nasm");
	return EXIT_FAILURE;
    }

    /* (Try to) set a preprocessor, if requested */
    if (cur_preproc) {
	if (parser_setpp(cur_parser, cur_preproc->keyword)) {
	    if (in != stdin)
		fclose(in);
	    xfree(in_filename);
	    return EXIT_FAILURE;
	}
    }

    /* Get initial BITS setting from object format */
    x86_mode_bits = cur_objfmt->default_mode_bits;

    /* Parse! */
    sections = cur_parser->do_parse(cur_parser, in, in_filename);

    /* Close input file */
    if (in != stdin)
	fclose(in);
    xfree(in_filename);

    if (GetNumErrors() > 0) {
	OutputAllErrorWarning();
	cleanup(sections);
	return EXIT_FAILURE;
    }

    symrec_parser_finalize();
    basic_optimizer.optimize(sections);

    if (GetNumErrors() > 0) {
	OutputAllErrorWarning();
	cleanup(sections);
	return EXIT_FAILURE;
    }

    /* open the object file for output (if not already opened above) */
    if (!obj) {
	obj = open_obj();
	if (!obj) {
	    cleanup(sections);
	    return EXIT_FAILURE;
	}
    }

    /* Write the object file */
    cur_objfmt->output(obj, sections);

    /* Close object file */
    if (obj != stdout)
	fclose(obj);

    /* If we had an error at this point, we also need to delete the output
     * object file (to make sure it's not left newer than the source).
     */
    if (GetNumErrors() > 0) {
	OutputAllErrorWarning();
	cleanup(sections);
	if (obj != stdout)
	    remove(obj_filename);
	return EXIT_FAILURE;
    }

    OutputAllErrorWarning();
    xfree(obj_filename);

    cleanup(sections);
    return EXIT_SUCCESS;
}
/*@=globstate =unrecog@*/

/* Open the object file.  Returns 0 on failure. */
static FILE *
open_obj(void)
{
    FILE *obj;

    assert(obj_filename != NULL);

    if (strcmp(obj_filename, "-") == 0)
	obj = stdout;
    else {
	obj = fopen(obj_filename, "wb");
	if (!obj)
	    ErrorNow(_("could not open file `%s'"), obj_filename);
    }
    return obj;
}

/* Cleans up all allocated structures. */
static void
cleanup(sectionhead *sections)
{
    sections_delete(sections);
    symrec_delete_all();
    if (cur_objfmt && cur_objfmt->cleanup)
	cur_objfmt->cleanup();
    line_shutdown();

    floatnum_shutdown();
    intnum_shutdown();

    BitVector_Shutdown();

    /* Finish with libltdl. */
    lt_dlexit();
}

/*
 *  Command line options handlers
 */
int
not_an_option_handler(char *param)
{
    if (in_filename) {
	WarningNow("can open only one input file, only the last file will be processed");
	xfree(in_filename);
    }

    in_filename = xstrdup(param);

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
opt_parser_handler(/*@unused@*/ char *cmd, char *param, /*@unused@*/ int extra)
{
    assert(param != NULL);
    cur_parser = find_parser(param);
    if (!cur_parser) {
	ErrorNow(_("unrecognized parser `%s'"), param);
	return 1;
    }
    return 0;
}

static int
opt_preproc_handler(/*@unused@*/ char *cmd, char *param, /*@unused@*/ int extra)
{
    assert(param != NULL);
    cur_preproc = find_preproc(param);
    if (!cur_preproc) {
	ErrorNow(_("unrecognized preprocessor `%s'"), param);
	return 1;
    }
    return 0;
}

static int
opt_objfmt_handler(/*@unused@*/ char *cmd, char *param, /*@unused@*/ int extra)
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
    if (obj_filename) {
	WarningNow(_("can output to only one object file, last specified used"));
	xfree(obj_filename);
    }

    assert(param != NULL);
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

static int
preproc_only_handler(/*@unused@*/ char *cmd, /*@unused@*/ char *param,
		     /*@unused@*/ int extra)
{
    preproc_only = 1;
    return 0;
}
