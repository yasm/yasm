/*
 * Program entry point, command line parsing
 *
 *  Copyright (C) 2001  Peter Johnson
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

#ifndef WIN32
#include "ltdl.h"
#endif
#include "yasm-module.h"
#include "yasm-options.h"


/* Extra path to search for our modules. */
#ifndef YASM_MODULE_PATH_ENV
# define YASM_MODULE_PATH_ENV	"YASM_MODULE_PATH"
#endif

#ifndef WIN32
extern const lt_dlsymlist lt_preloaded_symbols[];
#endif

/* Preprocess-only buffer size */
#define PREPROC_BUF_SIZE    16384

/*@null@*/ /*@only@*/ static char *obj_filename = NULL, *in_filename = NULL;
static int special_options = 0;
/*@null@*/ /*@dependent@*/ static yasm_arch *cur_arch = NULL;
/*@null@*/ /*@dependent@*/ static yasm_parser *cur_parser = NULL;
/*@null@*/ /*@dependent@*/ static yasm_preproc *cur_preproc = NULL;
/*@null@*/ /*@dependent@*/ static yasm_objfmt *cur_objfmt = NULL;
/*@null@*/ /*@dependent@*/ static yasm_optimizer *cur_optimizer = NULL;
/*@null@*/ /*@dependent@*/ static yasm_dbgfmt *cur_dbgfmt = NULL;
static int preproc_only = 0;
static int warning_error = 0;	/* warnings being treated as errors */

/*@null@*/ /*@dependent@*/ static FILE *open_obj(const char *mode);
static void cleanup(/*@null@*/ yasm_sectionhead *sections);

/* Forward declarations: cmd line parser handlers */
static int opt_special_handler(char *cmd, /*@null@*/ char *param, int extra);
static int opt_parser_handler(char *cmd, /*@null@*/ char *param, int extra);
static int opt_preproc_handler(char *cmd, /*@null@*/ char *param, int extra);
static int opt_objfmt_handler(char *cmd, /*@null@*/ char *param, int extra);
static int opt_dbgfmt_handler(char *cmd, /*@null@*/ char *param, int extra);
static int opt_objfile_handler(char *cmd, /*@null@*/ char *param, int extra);
static int opt_warning_handler(char *cmd, /*@null@*/ char *param, int extra);
static int preproc_only_handler(char *cmd, /*@null@*/ char *param, int extra);
static int opt_preproc_include_path(char *cmd, /*@null@*/ char *param,
				    int extra);
static int opt_preproc_include_file(char *cmd, /*@null@*/ char *param,
				    int extra);

static /*@only@*/ char *replace_extension(const char *orig, /*@null@*/
					  const char *ext, const char *def);
static void print_error(const char *fmt, ...);

static /*@exits@*/ void handle_yasm_int_error(const char *file,
					      unsigned int line,
					      const char *message);
static /*@exits@*/ void handle_yasm_fatal(const char *message);
static const char *handle_yasm_gettext(const char *msgid);
static void print_yasm_error(const char *filename, unsigned long line,
			     const char *msg);
static void print_yasm_warning(const char *filename, unsigned long line,
			       const char *msg);

static void apply_preproc_saved_options(void);

/* values for special_options */
#define SPECIAL_SHOW_HELP 0x01
#define SPECIAL_SHOW_VERSION 0x02

/* command line options */
static opt_option options[] =
{
    { 0, "version", 0, opt_special_handler, SPECIAL_SHOW_VERSION,
      N_("show version text"), NULL },
    { 'h', "help", 0, opt_special_handler, SPECIAL_SHOW_HELP,
      N_("show help text"), NULL },
    { 'p', "parser", 1, opt_parser_handler, 0,
      N_("select parser"), N_("parser") },
    { 'r', "preproc", 1, opt_preproc_handler, 0,
      N_("select preprocessor"), N_("preproc") },
    { 'f', "oformat", 1, opt_objfmt_handler, 0,
      N_("select object format"), N_("format") },
    { 'g', "dformat", 1, opt_dbgfmt_handler, 0,
      N_("select debugging format"), N_("debug") },
    { 'o', "objfile", 1, opt_objfile_handler, 0,
      N_("name of object-file output"), N_("filename") },
    { 'w', NULL, 0, opt_warning_handler, 1,
      N_("inhibits warning messages"), NULL },
    { 'W', NULL, 0, opt_warning_handler, 0,
      N_("enables/disables warning"), NULL },
    { 'e', "preproc-only", 0, preproc_only_handler, 0,
      N_("preprocess only (writes output to stdout by default)"), NULL },
    { 'I', NULL, 1, opt_preproc_include_path, 0,
      N_("add include path"), N_("path") },
    { 'P', NULL, 1, opt_preproc_include_file, 0,
      N_("pre-include file"), N_("filename") },
};

/* version message */
/*@observer@*/ static const char *version_msg[] = {
    PACKAGE " " VERSION "\n",
    N_("Copyright (c) 2001-2003 Peter Johnson and other"), " " PACKAGE " ",
    N_("developers.\n"),
    N_("This program is free software; you may redistribute it under the\n"),
    N_("terms of the GNU General Public License.  Portions of this program\n"),
    N_("are licensed under the GNU Lesser General Public License or the\n"),
    N_("3-clause BSD license; see individual file comments for details.\n"),
    N_("This program has absolutely no warranty; not even for\n"),
    N_("merchantibility or fitness for a particular purpose.\n"),
    N_("Compiled on"), " " __DATE__ ".\n",
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

/* parsed command line storage until appropriate modules have been loaded */
typedef STAILQ_HEAD(constcharparam_head, constcharparam) constcharparam_head;

typedef struct constcharparam {
    STAILQ_ENTRY(constcharparam) link;
    const char *param;
} constcharparam;

static constcharparam_head includepaths;
static constcharparam_head includefiles;

/* main function */
/*@-globstate -unrecog@*/
int
main(int argc, char *argv[])
{
    /*@null@*/ FILE *in = NULL, *obj = NULL;
    yasm_sectionhead *sections;
    size_t i;
#ifndef WIN32
    int errors;
#endif

#if defined(HAVE_SETLOCALE) && defined(HAVE_LC_MESSAGES)
    setlocale(LC_MESSAGES, "");
#endif
#if defined(LOCALEDIR)
    bindtextdomain(PACKAGE, LOCALEDIR);
#endif
    textdomain(PACKAGE);

    /* Initialize errwarn handling */
    yasm_internal_error_ = handle_yasm_int_error;
    yasm_fatal = handle_yasm_fatal;
    yasm_gettext_hook = handle_yasm_gettext;
    yasm_errwarn_initialize();

#ifndef WIN32
    /* Set libltdl malloc/free functions. */
#ifdef WITH_DMALLOC
    lt_dlmalloc = malloc;
    lt_dlfree = free;
#else
    lt_dlmalloc = yasm_xmalloc;
    lt_dlfree = yasm_xfree;
#endif

    /* Initialize preloaded symbol lookup table. */
    lt_dlpreload_default(lt_preloaded_symbols);

    /* Initialize libltdl. */
    errors = lt_dlinit();

    /* Set up extra module search directories. */
    if (errors == 0) {
	const char *path = getenv(YASM_MODULE_PATH_ENV);
	if (path)
	    errors = lt_dladdsearchdir(path);
    }
    if (errors != 0) {
	print_error(_("Module loader initialization failed"));
	return EXIT_FAILURE;
    }
#endif

    /* Initialize parameter storage */
    STAILQ_INIT(&includepaths);
    STAILQ_INIT(&includefiles);

    if (parse_cmdline(argc, argv, options, NELEMS(options), print_error))
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
	print_error(_("Could not initialize BitVector"));
	return EXIT_FAILURE;
    }

    if (in_filename && strcmp(in_filename, "-") != 0) {
	/* Open the input file (if not standard input) */
	in = fopen(in_filename, "rt");
	if (!in) {
	    print_error(_("could not open file `%s'"), in_filename);
	    yasm_xfree(in_filename);
	    if (obj_filename)
		yasm_xfree(obj_filename);
	    return EXIT_FAILURE;
	}
    } else {
	/* If no files were specified or filename was "-", read stdin */
	in = stdin;
	if (!in_filename)
	    in_filename = yasm__xstrdup("-");
    }

    /* Initialize line manager */
    yasm_std_linemgr.initialize();
    yasm_std_linemgr.set(in_filename, 1, 1);

    /* Initialize intnum and floatnum */
    yasm_intnum_initialize();
    yasm_floatnum_initialize();

    /* Initialize symbol table */
    yasm_symrec_initialize();

    /* handle preproc-only case here */
    if (preproc_only) {
	char *preproc_buf = yasm_xmalloc(PREPROC_BUF_SIZE);
	size_t got;

	/* Default output to stdout if not specified */
	if (!obj_filename)
	    obj = stdout;
	else {
	    /* Open output (object) file */
	    obj = open_obj("wt");
	    if (!obj) {
		yasm_xfree(preproc_buf);
		return EXIT_FAILURE;
	    }
	}

	/* If not already specified, default to nasm preproc. */
	if (!cur_preproc)
	    cur_preproc = load_preproc("nasm");

	if (!cur_preproc) {
	    print_error(_("Could not load default preprocessor"));
	    cleanup(NULL);
	    return EXIT_FAILURE;
	}

	apply_preproc_saved_options();

	/* Pre-process until done */
	cur_preproc->initialize(in, in_filename, &yasm_std_linemgr);
	while ((got = cur_preproc->input(preproc_buf, PREPROC_BUF_SIZE)) != 0)
	    fwrite(preproc_buf, got, 1, obj);

	if (in != stdin)
	    fclose(in);

	if (obj != stdout)
	    fclose(obj);

	if (yasm_get_num_errors(warning_error) > 0) {
	    yasm_errwarn_output_all(&yasm_std_linemgr, warning_error,
				    print_yasm_error, print_yasm_warning);
	    if (obj != stdout)
		remove(obj_filename);
	    yasm_xfree(preproc_buf);
	    cleanup(NULL);
	    return EXIT_FAILURE;
	}
	yasm_xfree(preproc_buf);
	cleanup(NULL);
	return EXIT_SUCCESS;
    }

    /* Set x86 as the architecture (TODO: user choice) */
    cur_arch = load_arch("x86");

    if (!cur_arch) {
	print_error(_("Could not load default architecture"));
	return EXIT_FAILURE;
    }

    cur_arch->initialize();

    /* Set basic as the optimizer (TODO: user choice) */
    cur_optimizer = load_optimizer("basic");

    if (!cur_optimizer) {
	print_error(_("Could not load default optimizer"));
	return EXIT_FAILURE;
    }

    yasm_arch_common_initialize(cur_arch);
    yasm_expr_initialize(cur_arch);
    yasm_bc_initialize(cur_arch);

    /* If not already specified, default to bin as the object format. */
    if (!cur_objfmt)
	cur_objfmt = load_objfmt("bin");

    if (!cur_objfmt) {
	print_error(_("Could not load default object format"));
	return EXIT_FAILURE;
    }

    /* If not already specified, default to null as the debug format. */
    if (!cur_dbgfmt)
	cur_dbgfmt = load_dbgfmt("null");
    else {
	int matched_dbgfmt = 0;
	/* Check to see if the requested debug format is in the allowed list
	 * for the active object format.
	 */
	for (i=0; cur_objfmt->dbgfmt_keywords[i]; i++)
	    if (yasm__strcasecmp(cur_objfmt->dbgfmt_keywords[i],
				 cur_dbgfmt->keyword) == 0)
		matched_dbgfmt = 1;
	if (!matched_dbgfmt) {
	    print_error(
		_("`%s' is not a valid debug format for object format `%s'"),
		cur_dbgfmt->keyword, cur_objfmt->keyword);
	    if (in != stdin)
		fclose(in);
	    /*cleanup(NULL);*/
	    return EXIT_FAILURE;
	}
    }

    if (!cur_dbgfmt) {
	print_error(_("Could not load default debug format"));
	return EXIT_FAILURE;
    }

    /* determine the object filename if not specified */
    if (!obj_filename) {
	if (in == stdin)
	    /* Default to yasm.out if no obj filename specified */
	    obj_filename = yasm__xstrdup("yasm.out");
	else
	    /* replace (or add) extension */
	    obj_filename = replace_extension(in_filename,
					     cur_objfmt->extension,
					     "yasm.out");
    }

    /* Initialize the object format */
    if (cur_objfmt->initialize)
	cur_objfmt->initialize(in_filename, obj_filename, cur_dbgfmt,
			       cur_arch);

    /* Set NASM as the parser */
    cur_parser = load_parser("nasm");
    if (!cur_parser) {
	print_error(_("unrecognized parser `%s'"), "nasm");
	cleanup(NULL);
	return EXIT_FAILURE;
    }

    /* If not already specified, default to the parser's default preproc. */
    if (!cur_preproc)
	cur_preproc = load_preproc(cur_parser->default_preproc_keyword);
    else {
	int matched_preproc = 0;
	/* Check to see if the requested preprocessor is in the allowed list
	 * for the active parser.
	 */
	for (i=0; cur_parser->preproc_keywords[i]; i++)
	    if (yasm__strcasecmp(cur_parser->preproc_keywords[i],
			   cur_preproc->keyword) == 0)
		matched_preproc = 1;
	if (!matched_preproc) {
	    print_error(_("`%s' is not a valid preprocessor for parser `%s'"),
			cur_preproc->keyword, cur_parser->keyword);
	    if (in != stdin)
		fclose(in);
	    cleanup(NULL);
	    return EXIT_FAILURE;
	}
    }
    apply_preproc_saved_options();

    /* Get initial x86 BITS setting from object format */
    if (strcmp(cur_arch->keyword, "x86") == 0) {
	unsigned char *x86_mode_bits;
	x86_mode_bits = (unsigned char *)get_module_data("x86", "mode_bits");
	if (x86_mode_bits)
	    *x86_mode_bits = cur_objfmt->default_x86_mode_bits;
    }

    /* Parse! */
    sections = cur_parser->do_parse(cur_preproc, cur_arch, cur_objfmt,
				    &yasm_std_linemgr, in, in_filename, 0);

    /* Close input file */
    if (in != stdin)
	fclose(in);

    if (yasm_get_num_errors(warning_error) > 0) {
	yasm_errwarn_output_all(&yasm_std_linemgr, warning_error,
				print_yasm_error, print_yasm_warning);
	cleanup(sections);
	return EXIT_FAILURE;
    }

    yasm_symrec_parser_finalize();
    cur_optimizer->optimize(sections);

    if (yasm_get_num_errors(warning_error) > 0) {
	yasm_errwarn_output_all(&yasm_std_linemgr, warning_error,
				print_yasm_error, print_yasm_warning);
	cleanup(sections);
	return EXIT_FAILURE;
    }

    /* open the object file for output (if not already opened by dbg objfmt) */
    if (!obj && strcmp(cur_objfmt->keyword, "dbg") != 0) {
	obj = open_obj("wb");
	if (!obj) {
	    cleanup(sections);
	    return EXIT_FAILURE;
	}
    }

    /* Write the object file */
    cur_objfmt->output(obj?obj:stderr, sections,
		       strcmp(cur_dbgfmt->keyword, "null"));

    /* Close object file */
    if (obj)
	fclose(obj);

    /* If we had an error at this point, we also need to delete the output
     * object file (to make sure it's not left newer than the source).
     */
    if (yasm_get_num_errors(warning_error) > 0) {
	yasm_errwarn_output_all(&yasm_std_linemgr, warning_error,
				print_yasm_error, print_yasm_warning);
	remove(obj_filename);
	cleanup(sections);
	return EXIT_FAILURE;
    }

    yasm_errwarn_output_all(&yasm_std_linemgr, warning_error,
			    print_yasm_error, print_yasm_warning);

    cleanup(sections);
    return EXIT_SUCCESS;
}
/*@=globstate =unrecog@*/

/* Open the object file.  Returns 0 on failure. */
static FILE *
open_obj(const char *mode)
{
    FILE *obj;

    assert(obj_filename != NULL);

    obj = fopen(obj_filename, mode);
    if (!obj)
	print_error(_("could not open file `%s'"), obj_filename);
    return obj;
}

/* Define DO_FREE to 1 to enable deallocation of all data structures.
 * Useful for detecting memory leaks, but slows down execution unnecessarily
 * (as the OS will free everything we miss here).
 */
#define DO_FREE		1

/* Cleans up all allocated structures. */
static void
cleanup(yasm_sectionhead *sections)
{
    if (DO_FREE) {
	if (cur_objfmt && cur_objfmt->cleanup)
	    cur_objfmt->cleanup();
	if (cur_dbgfmt && cur_dbgfmt->cleanup)
	    cur_dbgfmt->cleanup();
	if (cur_preproc)
	    cur_preproc->cleanup();
	if (sections)
	    yasm_sections_delete(sections);
	yasm_symrec_cleanup();
	if (cur_arch)
	    cur_arch->cleanup();

	yasm_floatnum_cleanup();
	yasm_intnum_cleanup();

	yasm_errwarn_cleanup();
	yasm_std_linemgr.cleanup();

	BitVector_Shutdown();
    }

    unload_modules();

#ifndef WIN32
    /* Finish with libltdl. */
    lt_dlexit();
#endif

    if (DO_FREE) {
	if (in_filename)
	    yasm_xfree(in_filename);
	if (obj_filename)
	    yasm_xfree(obj_filename);
    }
}

/*
 *  Command line options handlers
 */
int
not_an_option_handler(char *param)
{
    if (in_filename) {
	print_error(
	    _("warning: can open only one input file, only the last file will be processed"));
	yasm_xfree(in_filename);
    }

    in_filename = yasm__xstrdup(param);

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
    cur_parser = load_parser(param);
    if (!cur_parser) {
	print_error(_("unrecognized parser `%s'"), param);
	return 1;
    }
    return 0;
}

static int
opt_preproc_handler(/*@unused@*/ char *cmd, char *param, /*@unused@*/ int extra)
{
    assert(param != NULL);
    cur_preproc = load_preproc(param);
    if (!cur_preproc) {
	print_error(_("unrecognized preprocessor `%s'"), param);
	return 1;
    }
    return 0;
}

static int
opt_objfmt_handler(/*@unused@*/ char *cmd, char *param, /*@unused@*/ int extra)
{
    assert(param != NULL);
    cur_objfmt = load_objfmt(param);
    if (!cur_objfmt) {
	print_error(_("unrecognized object format `%s'"), param);
	return 1;
    }
    return 0;
}

static int
opt_dbgfmt_handler(/*@unused@*/ char *cmd, char *param, /*@unused@*/ int extra)
{
    assert(param != NULL);
    cur_dbgfmt = load_dbgfmt(param);
    if (!cur_dbgfmt) {
	print_error(_("unrecognized debugging format `%s'"), param);
	return 1;
    }
    return 0;
}

static int
opt_objfile_handler(/*@unused@*/ char *cmd, char *param,
		    /*@unused@*/ int extra)
{
    if (obj_filename) {
	print_error(
	    _("warning: can output to only one object file, last specified used"));
	yasm_xfree(obj_filename);
    }

    assert(param != NULL);
    obj_filename = yasm__xstrdup(param);

    return 0;
}

static int
opt_warning_handler(char *cmd, /*@unused@*/ char *param, int extra)
{
    int enable = 1;	/* is it disabling the warning instead of enabling? */

    if (extra == 1) {
	/* -w, disable warnings */
	yasm_warn_disable_all();
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
	if (enable)
	    yasm_warn_enable(YASM_WARN_UNREC_CHAR);
	else
	    yasm_warn_disable(YASM_WARN_UNREC_CHAR);
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

static int
opt_preproc_include_path(/*@unused@*/ char *cmd, char *param,
			 /*@unused@*/ int extra)
{
    constcharparam *cp;
    cp = yasm_xmalloc(sizeof(constcharparam));
    cp->param = param;
    STAILQ_INSERT_TAIL(&includepaths, cp, link);
    return 0;
}

static void
apply_preproc_saved_options()
{
    constcharparam *cp;
    constcharparam *cpnext;

    if (cur_preproc->add_include_path != NULL) {
	STAILQ_FOREACH(cp, &includepaths, link) {
	    cur_preproc->add_include_path(cp->param);
	}
    }

    cp = STAILQ_FIRST(&includepaths);
    while (cp != NULL) {
	cpnext = STAILQ_NEXT(cp, link);
	yasm_xfree(cp);
	cp = cpnext;
    }
    STAILQ_INIT(&includepaths);

    if (cur_preproc->add_include_file != NULL) {
	STAILQ_FOREACH(cp, &includefiles, link) {
	    cur_preproc->add_include_file(cp->param);
	}
    }

    cp = STAILQ_FIRST(&includepaths);
    while (cp != NULL) {
	cpnext = STAILQ_NEXT(cp, link);
	yasm_xfree(cp);
	cp = cpnext;
    }
    STAILQ_INIT(&includepaths);
}

static int
opt_preproc_include_file(/*@unused@*/ char *cmd, char *param,
			 /*@unused@*/ int extra)
{
    constcharparam *cp;
    cp = yasm_xmalloc(sizeof(constcharparam));
    cp->param = param;
    STAILQ_INSERT_TAIL(&includefiles, cp, link);
    return 0;
}

/* Replace extension on a filename (or append one if none is present).
 * If output filename would be identical to input (same extension out as in),
 * returns (copy of) def.
 * A NULL ext means the trailing '.' should NOT be included, whereas a "" ext
 * means the trailing '.' should be included.
 */
static char *
replace_extension(const char *orig, /*@null@*/ const char *ext,
		  const char *def)
{
    char *out, *outext;

    /* allocate enough space for full existing name + extension */
    out = yasm_xmalloc(strlen(orig)+(ext ? (strlen(ext)+2) : 1));
    strcpy(out, orig);
    outext = strrchr(out, '.');
    if (outext) {
	/* Existing extension: make sure it's not the same as the replacement
	 * (as we don't want to overwrite the source file).
	 */
	outext++;   /* advance past '.' */
	if (ext && strcmp(outext, ext) == 0) {
	    outext = NULL;  /* indicate default should be used */
	    print_error(
		_("file name already ends in `.%s': output will be in `%s'"),
		ext, def);
	}
    } else {
	/* No extension: make sure the output extension is not empty
	 * (again, we don't want to overwrite the source file).
	 */
	if (!ext)
	    print_error(
		_("file name already has no extension: output will be in `%s'"),
		def);
	else {
	    outext = strrchr(out, '\0');    /* point to end of the string */
	    *outext++ = '.';		    /* append '.' */
	}
    }

    /* replace extension or use default name */
    if (outext) {
	if (!ext) {
	    /* Back up and replace '.' with string terminator */
	    outext--;
	    *outext = '\0';
	} else
	    strcpy(outext, ext);
    } else
	strcpy(out, def);

    return out;
}

static void
print_error(const char *fmt, ...)
{
    va_list va;
    fprintf(stderr, "yasm: ");
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
    fputc('\n', stderr);
}

static /*@exits@*/ void
handle_yasm_int_error(const char *file, unsigned int line, const char *message)
{
    fprintf(stderr, _("INTERNAL ERROR at %s, line %u: %s\n"), file, line,
	    gettext(message));
#ifdef HAVE_ABORT
    abort();
#else
    exit(EXIT_FAILURE);
#endif
}

static /*@exits@*/ void
handle_yasm_fatal(const char *message)
{
    fprintf(stderr, _("FATAL: %s\n"), gettext(message));
#ifdef HAVE_ABORT
    abort();
#else
    exit(EXIT_FAILURE);
#endif
}

static const char *
handle_yasm_gettext(const char *msgid)
{
    return gettext(msgid);
}

static void
print_yasm_error(const char *filename, unsigned long line, const char *msg)
{
    fprintf(stderr, "%s:%lu: %s\n", filename, line, msg);
}

static void
print_yasm_warning(const char *filename, unsigned long line, const char *msg)
{
    fprintf(stderr, "%s:%lu: %s %s\n", filename, line, _("warning:"), msg);
}
